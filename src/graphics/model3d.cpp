#include "model3d.hpp"
#include "mesh3d.hpp"
#include "texture.hpp"
#include "default_shaders.hpp"
#include "../core/log.hpp"

#include <tiny_gltf_v3.h>
#include <tiny_obj_loader.h>
#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace crayon {

static bool tg3_str_equals(const tg3_str& s, const char* name) {
    if (!s.data) return false;
    size_t n = strlen(name);
    return s.len == n && memcmp(s.data, name, n) == 0;
}

static std::string tg3_str_to_string(const tg3_str& s, const std::string& fallback = "") {
    if (!s.data || s.len == 0) return fallback;
    return std::string(s.data, s.len);
}

// Simple base64 decoding helper for data: URIs
static std::vector<uint8_t> decode_base64(const char* input, size_t length) {
    static const int b64_table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    std::vector<uint8_t> out;
    out.reserve(length * 3 / 4);

    uint32_t val = 0;
    int valb = -8;
    for (size_t i = 0; i < length; ++i) {
        uint8_t c = (uint8_t)input[i];
        if (c == '=' || c == '\r' || c == '\n' || c == ' ') continue;
        int d = b64_table[c];
        if (d < 0) break;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

struct AccessorReader {
    const uint8_t* base_ptr = nullptr;
    size_t stride = 0;
    uint64_t count = 0;
    int component_type = 0;
    int type = 0;
    bool valid = false;

    bool init(const tg3_model& model, int acc_idx) {
        if (acc_idx < 0 || acc_idx >= (int)model.accessors_count) return false;
        const tg3_accessor* acc = &model.accessors[acc_idx];
        if (acc->buffer_view < 0 || acc->buffer_view >= (int)model.buffer_views_count) return false;
        const tg3_buffer_view* bv = &model.buffer_views[acc->buffer_view];
        if (bv->buffer < 0 || bv->buffer >= (int)model.buffers_count) return false;
        const tg3_buffer* buf = &model.buffers[bv->buffer];
        if (!buf->data.data) return false;

        component_type = acc->component_type;
        type = acc->type;
        count = acc->count;

        int comp_size = tg3_component_size(component_type);
        int num_comps = tg3_num_components(type);
        size_t elem_size = (size_t)comp_size * num_comps;

        stride = bv->byte_stride != 0 ? bv->byte_stride : elem_size;
        size_t total_offset = (size_t)(bv->byte_offset + acc->byte_offset);
        if (total_offset + (count > 0 ? (count - 1) * stride + elem_size : 0) > buf->data.count) {
            return false;
        }

        base_ptr = buf->data.data + total_offset;
        valid = true;
        return true;
    }

    const uint8_t* get_elem(uint64_t i) const {
        return base_ptr + i * stride;
    }
};

Model3D::Model3D() = default;
Model3D::~Model3D() = default;

bool Model3D::load_from_file(const std::string& filepath) {
    m_filepath = filepath;
    std::string lower = filepath;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.ends_with(".gltf") || lower.ends_with(".glb")) {
        return load_from_gltf(filepath);
    } else if (lower.ends_with(".obj")) {
        return load_from_obj(filepath);
    }

    if (load_from_gltf(filepath)) return true;
    return load_from_obj(filepath);
}

bool Model3D::load_from_gltf(const std::string& filepath) {
    m_parts.clear();
    m_nodes.clear();
    m_owned_textures.clear();
    m_skins.clear();
    m_animations.clear();
    m_valid = false;
    m_filepath = filepath;

    tg3_parse_options opts;
    tg3_parse_options_init(&opts);
    opts.images_as_is = 1;

    tg3_error_stack errors;
    tg3_error_stack_init(&errors);

    tg3_model model;
    tg3_error_code err = tg3_parse_file(&model, &errors, filepath.c_str(), (uint32_t)filepath.length(), &opts);
    if (err != TG3_OK) {
        for (uint32_t i = 0; i < errors.count; ++i) {
            CRAYON_LOG_ERROR("glTF error [{}]: {}", (int)errors.entries[i].code,
                             errors.entries[i].message ? errors.entries[i].message : "unknown");
        }
        tg3_error_stack_free(&errors);
        return false;
    }

    std::string base_dir = "";
    size_t last_slash = filepath.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        base_dir = filepath.substr(0, last_slash + 1);
    }

    // 1. Load images into OpenGL Textures
    std::vector<std::shared_ptr<Texture>> loaded_textures(model.images_count, nullptr);
    for (uint32_t i = 0; i < model.images_count; ++i) {
        const tg3_image& img = model.images[i];
        std::shared_ptr<Texture> tex = nullptr;

        if (img.buffer_view >= 0 && img.buffer_view < (int)model.buffer_views_count) {
            const tg3_buffer_view* bv = &model.buffer_views[img.buffer_view];
            if (bv->buffer >= 0 && bv->buffer < (int)model.buffers_count) {
                const tg3_buffer* buf = &model.buffers[bv->buffer];
                if (buf->data.data && bv->byte_offset + bv->byte_length <= buf->data.count) {
                    const unsigned char* raw_bytes = buf->data.data + bv->byte_offset;
                    int w = 0, h = 0, comp = 0;
                    unsigned char* pixels = stbi_load_from_memory(raw_bytes, (int)bv->byte_length, &w, &h, &comp, 4);
                    if (pixels) {
                        tex = std::make_shared<Texture>();
                        tex->load_from_memory(pixels, w, h, 4, false);
                        stbi_image_free(pixels);
                    }
                }
            }
        } else if (img.uri.len > 0) {
            std::string uri_str(img.uri.data, img.uri.len);
            if (uri_str.rfind("data:", 0) == 0) {
                size_t comma = uri_str.find(',');
                if (comma != std::string::npos) {
                    std::vector<uint8_t> decoded = decode_base64(uri_str.c_str() + comma + 1, uri_str.length() - comma - 1);
                    if (!decoded.empty()) {
                        int w = 0, h = 0, comp = 0;
                        unsigned char* pixels = stbi_load_from_memory(decoded.data(), (int)decoded.size(), &w, &h, &comp, 4);
                        if (pixels) {
                            tex = std::make_shared<Texture>();
                            tex->load_from_memory(pixels, w, h, 4, false);
                            stbi_image_free(pixels);
                        }
                    }
                }
            } else {
                std::string full_img_path = base_dir + uri_str;
                tex = std::make_shared<Texture>();
                if (!tex->load_from_file(full_img_path, false)) {
                    tex = nullptr;
                }
            }
        }

        loaded_textures[i] = tex;
        if (tex) {
            m_owned_textures.push_back(tex);
        }
    }

    // 2. Map Materials to Texture IDs and Base Colors
    std::vector<GLuint> mat_textures(model.materials_count, 0);
    std::vector<glm::vec4> mat_colors(model.materials_count, glm::vec4(1.0f));

    for (uint32_t i = 0; i < model.materials_count; ++i) {
        const tg3_material& mat = model.materials[i];
        mat_colors[i] = glm::vec4(
            (float)mat.pbr_metallic_roughness.base_color_factor[0],
            (float)mat.pbr_metallic_roughness.base_color_factor[1],
            (float)mat.pbr_metallic_roughness.base_color_factor[2],
            (float)mat.pbr_metallic_roughness.base_color_factor[3]
        );

        int tex_idx = mat.pbr_metallic_roughness.base_color_texture.index;
        if (tex_idx >= 0 && tex_idx < (int)model.textures_count) {
            int img_idx = model.textures[tex_idx].source;
            if (img_idx >= 0 && img_idx < (int)loaded_textures.size() && loaded_textures[img_idx]) {
                mat_textures[i] = loaded_textures[img_idx]->get_id();
            }
        }
    }

    // 3. Build Nodes Hierarchy
    m_nodes.resize(model.nodes_count);
    for (uint32_t i = 0; i < model.nodes_count; ++i) {
        const tg3_node& gn = model.nodes[i];
        ModelNode& mn = m_nodes[i];
        mn.name = gn.name.len > 0 ? tg3_str_to_string(gn.name) : ("node_" + std::to_string(i));
        mn.parent_index = -1;

        if (gn.has_matrix) {
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                    mn.local_matrix[col][row] = (float)gn.matrix[col * 4 + row];
                }
            }
            mn.translation = glm::vec3(mn.local_matrix[3]);
            mn.scale = glm::vec3(
                glm::length(glm::vec3(mn.local_matrix[0])),
                glm::length(glm::vec3(mn.local_matrix[1])),
                glm::length(glm::vec3(mn.local_matrix[2]))
            );
            mn.rotation = glm::quat_cast(mn.local_matrix);
        } else {
            mn.translation = glm::vec3((float)gn.translation[0], (float)gn.translation[1], (float)gn.translation[2]);
            mn.rotation = glm::quat((float)gn.rotation[3], (float)gn.rotation[0], (float)gn.rotation[1], (float)gn.rotation[2]);
            mn.scale = glm::vec3((float)gn.scale[0], (float)gn.scale[1], (float)gn.scale[2]);

            mn.local_matrix = glm::translate(glm::mat4(1.0f), mn.translation) *
                              glm::mat4_cast(mn.rotation) *
                              glm::scale(glm::mat4(1.0f), mn.scale);
        }

        mn.children.resize(gn.children_count);
        for (uint32_t c = 0; c < gn.children_count; ++c) {
            mn.children[c] = gn.children[c];
        }
    }

    // Connect parents
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        for (int ch : m_nodes[i].children) {
            if (ch >= 0 && ch < (int)m_nodes.size()) {
                m_nodes[ch].parent_index = (int)i;
            }
        }
    }

    // Compute node world matrices
    update_node_world_matrices();

    // 3b. Extract Skins
    m_skins.resize(model.skins_count);
    for (uint32_t s = 0; s < model.skins_count; ++s) {
        const tg3_skin& gs = model.skins[s];
        ModelSkin& skin = m_skins[s];
        skin.name = gs.name.len > 0 ? tg3_str_to_string(gs.name) : ("skin_" + std::to_string(s));
        skin.skeleton_root_node = gs.skeleton;

        AccessorReader ibm_reader;
        bool has_ibm = (gs.inverse_bind_matrices >= 0) && ibm_reader.init(model, gs.inverse_bind_matrices);

        skin.joints.resize(gs.joints_count);
        for (uint32_t j = 0; j < gs.joints_count; ++j) {
            ModelJoint& mj = skin.joints[j];
            mj.node_index = gs.joints[j];
            if (mj.node_index >= 0 && mj.node_index < (int)m_nodes.size()) {
                mj.name = m_nodes[mj.node_index].name;
            } else {
                mj.name = "joint_" + std::to_string(j);
            }
            skin.joint_name_to_index[mj.name] = static_cast<int>(j);

            if (has_ibm && j < ibm_reader.count) {
                const float* mat_ptr = (const float*)ibm_reader.get_elem(j);
                for (int col = 0; col < 4; ++col) {
                    for (int row = 0; row < 4; ++row) {
                        mj.inverse_bind_matrix[col][row] = mat_ptr[col * 4 + row];
                    }
                }
            } else {
                if (mj.node_index >= 0 && mj.node_index < (int)m_nodes.size()) {
                    mj.inverse_bind_matrix = glm::inverse(m_nodes[mj.node_index].world_matrix);
                }
            }
        }

        // Connect parent joint indices
        for (uint32_t j = 0; j < gs.joints_count; ++j) {
            int node_idx = skin.joints[j].node_index;
            if (node_idx >= 0 && node_idx < (int)m_nodes.size()) {
                int parent_node = m_nodes[node_idx].parent_index;
                while (parent_node >= 0) {
                    auto it = skin.joint_name_to_index.find(m_nodes[parent_node].name);
                    if (it != skin.joint_name_to_index.end()) {
                        skin.joints[j].parent_joint_index = it->second;
                        break;
                    }
                    parent_node = m_nodes[parent_node].parent_index;
                }
            }
        }
    }

    // 3c. Extract Animations
    m_animations.resize(model.animations_count);
    for (uint32_t a = 0; a < model.animations_count; ++a) {
        const tg3_animation& ga = model.animations[a];
        AnimationClip& clip = m_animations[a];
        clip.name = ga.name.len > 0 ? tg3_str_to_string(ga.name) : ("anim_" + std::to_string(a));
        clip.duration = 0.0f;

        clip.channels.reserve(ga.channels_count);
        for (uint32_t c = 0; c < ga.channels_count; ++c) {
            const tg3_animation_channel& gc = ga.channels[c];
            if (gc.sampler < 0 || gc.sampler >= (int)ga.samplers_count) continue;
            const tg3_animation_sampler& gs = ga.samplers[gc.sampler];

            AccessorReader time_reader;
            AccessorReader val_reader;
            if (!time_reader.init(model, gs.input) || !val_reader.init(model, gs.output)) continue;
            if (time_reader.count == 0 || val_reader.count == 0) continue;

            AnimationChannel ch;
            ch.target_node = gc.target.node;

            std::string path_str = tg3_str_to_string(gc.target.path);
            if (path_str == "translation") ch.path = AnimationPath::Translation;
            else if (path_str == "rotation") ch.path = AnimationPath::Rotation;
            else if (path_str == "scale") ch.path = AnimationPath::Scale;
            else continue;

            std::string interp_str = tg3_str_to_string(gs.interpolation);
            if (interp_str == "STEP") ch.interpolation = AnimationInterpolation::Step;
            else if (interp_str == "CUBICSPLINE") ch.interpolation = AnimationInterpolation::CubicSpline;
            else ch.interpolation = AnimationInterpolation::Linear;

            ch.timestamps.resize(time_reader.count);
            for (uint64_t k = 0; k < time_reader.count; ++k) {
                const float* tf = (const float*)time_reader.get_elem(k);
                ch.timestamps[k] = *tf;
                if (*tf > clip.duration) clip.duration = *tf;
            }

            if (ch.path == AnimationPath::Rotation) {
                ch.quat_values.resize(val_reader.count);
                for (uint64_t k = 0; k < val_reader.count; ++k) {
                    const float* qf = (const float*)val_reader.get_elem(k);
                    ch.quat_values[k] = glm::quat(qf[3], qf[0], qf[1], qf[2]);
                }
            } else {
                ch.vec3_values.resize(val_reader.count);
                for (uint64_t k = 0; k < val_reader.count; ++k) {
                    const float* vf = (const float*)val_reader.get_elem(k);
                    ch.vec3_values[k] = glm::vec3(vf[0], vf[1], vf[2]);
                }
            }

            clip.channels.push_back(std::move(ch));
        }
    }

    // 4. Extract Primitives for each Node
    for (uint32_t node_idx = 0; node_idx < model.nodes_count; ++node_idx) {
        const tg3_node& gn = model.nodes[node_idx];
        if (gn.mesh < 0 || gn.mesh >= (int)model.meshes_count) continue;

        const tg3_mesh& gm = model.meshes[gn.mesh];
        const glm::mat4& node_world = m_nodes[node_idx].world_matrix;

        for (uint32_t p = 0; p < gm.primitives_count; ++p) {
            const tg3_primitive& prim = gm.primitives[p];

            // Attributes
            int pos_idx = -1;
            int norm_idx = -1;
            int uv_idx = -1;
            int col_idx = -1;
            int joints_idx = -1;
            int weights_idx = -1;

            for (uint32_t a = 0; a < prim.attributes_count; ++a) {
                const tg3_str_int_pair& attr = prim.attributes[a];
                if (tg3_str_equals(attr.key, "POSITION")) pos_idx = attr.value;
                else if (tg3_str_equals(attr.key, "NORMAL")) norm_idx = attr.value;
                else if (tg3_str_equals(attr.key, "TEXCOORD_0")) uv_idx = attr.value;
                else if (tg3_str_equals(attr.key, "COLOR_0")) col_idx = attr.value;
                else if (tg3_str_equals(attr.key, "JOINTS_0")) joints_idx = attr.value;
                else if (tg3_str_equals(attr.key, "WEIGHTS_0")) weights_idx = attr.value;
            }

            AccessorReader pos_reader;
            if (!pos_reader.init(model, pos_idx) || pos_reader.type != TG3_TYPE_VEC3 || pos_reader.count == 0) {
                continue;
            }

            std::vector<Vertex3D> vertices(pos_reader.count);
            glm::vec3 part_min( 1e30f);
            glm::vec3 part_max(-1e30f);

            for (uint64_t v = 0; v < pos_reader.count; ++v) {
                const float* f = (const float*)pos_reader.get_elem(v);
                vertices[v].position = glm::vec3(f[0], f[1], f[2]);
                vertices[v].normal = glm::vec3(0, 1, 0);
                vertices[v].uv = glm::vec2(0, 0);
                vertices[v].color = glm::vec4(1.0f);

                part_min = glm::min(part_min, vertices[v].position);
                part_max = glm::max(part_max, vertices[v].position);
            }

            bool has_normals = false;
            AccessorReader norm_reader;
            if (norm_reader.init(model, norm_idx) && norm_reader.type == TG3_TYPE_VEC3 && norm_reader.component_type == TG3_COMPONENT_TYPE_FLOAT) {
                has_normals = true;
                uint64_t cnt = std::min(pos_reader.count, norm_reader.count);
                for (uint64_t v = 0; v < cnt; ++v) {
                    const float* f = (const float*)norm_reader.get_elem(v);
                    vertices[v].normal = glm::vec3(f[0], f[1], f[2]);
                }
            }

            AccessorReader uv_reader;
            if (uv_reader.init(model, uv_idx) && uv_reader.type == TG3_TYPE_VEC2) {
                uint64_t cnt = std::min(pos_reader.count, uv_reader.count);
                for (uint64_t v = 0; v < cnt; ++v) {
                    const uint8_t* ptr = uv_reader.get_elem(v);
                    if (uv_reader.component_type == TG3_COMPONENT_TYPE_FLOAT) {
                        const float* f = (const float*)ptr;
                        vertices[v].uv = glm::vec2(f[0], f[1]);
                    } else if (uv_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* u = (const uint16_t*)ptr;
                        vertices[v].uv = glm::vec2(u[0] / 65535.0f, u[1] / 65535.0f);
                    } else if (uv_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        vertices[v].uv = glm::vec2(ptr[0] / 255.0f, ptr[1] / 255.0f);
                    }
                }
            }

            AccessorReader col_reader;
            if (col_reader.init(model, col_idx)) {
                uint64_t cnt = std::min(pos_reader.count, col_reader.count);
                for (uint64_t v = 0; v < cnt; ++v) {
                    const uint8_t* ptr = col_reader.get_elem(v);
                    if (col_reader.component_type == TG3_COMPONENT_TYPE_FLOAT) {
                        const float* f = (const float*)ptr;
                        if (col_reader.type == TG3_TYPE_VEC4) {
                            vertices[v].color = glm::vec4(f[0], f[1], f[2], f[3]);
                        } else if (col_reader.type == TG3_TYPE_VEC3) {
                            vertices[v].color = glm::vec4(f[0], f[1], f[2], 1.0f);
                        }
                    } else if (col_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        if (col_reader.type == TG3_TYPE_VEC4) {
                            vertices[v].color = glm::vec4(ptr[0]/255.f, ptr[1]/255.f, ptr[2]/255.f, ptr[3]/255.f);
                        } else if (col_reader.type == TG3_TYPE_VEC3) {
                            vertices[v].color = glm::vec4(ptr[0]/255.f, ptr[1]/255.f, ptr[2]/255.f, 1.0f);
                        }
                    }
                }
            }

            // Indices
            std::vector<GLuint> indices;
            AccessorReader idx_reader;
            if (prim.indices >= 0 && idx_reader.init(model, prim.indices)) {
                indices.reserve((size_t)idx_reader.count);
                for (uint64_t i = 0; i < idx_reader.count; ++i) {
                    const uint8_t* ptr = idx_reader.get_elem(i);
                    if (idx_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        indices.push_back(*(const uint16_t*)ptr);
                    } else if (idx_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_INT) {
                        indices.push_back(*(const uint32_t*)ptr);
                    } else if (idx_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        indices.push_back(*ptr);
                    }
                }
            } else {
                indices.resize(vertices.size());
                for (size_t i = 0; i < vertices.size(); ++i) indices[i] = (GLuint)i;
            }

            // Calculate normals if missing
            if (!has_normals) {
                for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                    GLuint i0 = indices[i];
                    GLuint i1 = indices[i+1];
                    GLuint i2 = indices[i+2];
                    if (i0 < vertices.size() && i1 < vertices.size() && i2 < vertices.size()) {
                        glm::vec3 e1 = vertices[i1].position - vertices[i0].position;
                        glm::vec3 e2 = vertices[i2].position - vertices[i0].position;
                        glm::vec3 n = glm::cross(e1, e2);
                        float l = glm::length(n);
                        if (l > 0.00001f) n /= l;
                        else n = glm::vec3(0, 1, 0);

                        vertices[i0].normal += n;
                        vertices[i1].normal += n;
                        vertices[i2].normal += n;
                    }
                }
                for (auto& v : vertices) {
                    float l = glm::length(v.normal);
                    if (l > 0.00001f) v.normal /= l;
                    else v.normal = glm::vec3(0, 1, 0);
                }
            }

            // Check for JOINTS_0 and WEIGHTS_0
            AccessorReader joints_reader;
            AccessorReader weights_reader;
            bool is_skinned_prim = (gn.skin >= 0 && joints_idx >= 0 && weights_idx >= 0 &&
                                    joints_reader.init(model, joints_idx) &&
                                    weights_reader.init(model, weights_idx));

            auto gpu_mesh = std::make_shared<Mesh3D>();
            if (is_skinned_prim) {
                std::vector<SkinnedVertex3D> skinned_verts(vertices.size());
                for (size_t v = 0; v < vertices.size(); ++v) {
                    skinned_verts[v].position = vertices[v].position;
                    skinned_verts[v].normal = vertices[v].normal;
                    skinned_verts[v].uv = vertices[v].uv;
                    skinned_verts[v].color = vertices[v].color;

                    if (v < joints_reader.count) {
                        const uint8_t* ptr = joints_reader.get_elem(v);
                        if (joints_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            skinned_verts[v].joints = glm::uvec4(ptr[0], ptr[1], ptr[2], ptr[3]);
                        } else if (joints_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
                            const uint16_t* u = (const uint16_t*)ptr;
                            skinned_verts[v].joints = glm::uvec4(u[0], u[1], u[2], u[3]);
                        } else if (joints_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_INT) {
                            const uint32_t* u = (const uint32_t*)ptr;
                            skinned_verts[v].joints = glm::uvec4(u[0], u[1], u[2], u[3]);
                        }
                    }

                    if (v < weights_reader.count) {
                        const uint8_t* ptr = weights_reader.get_elem(v);
                        if (weights_reader.component_type == TG3_COMPONENT_TYPE_FLOAT) {
                            const float* f = (const float*)ptr;
                            skinned_verts[v].weights = glm::vec4(f[0], f[1], f[2], f[3]);
                        } else if (weights_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE) {
                            skinned_verts[v].weights = glm::vec4(ptr[0]/255.f, ptr[1]/255.f, ptr[2]/255.f, ptr[3]/255.f);
                        } else if (weights_reader.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) {
                            const uint16_t* u = (const uint16_t*)ptr;
                            skinned_verts[v].weights = glm::vec4(u[0]/65535.f, u[1]/65535.f, u[2]/65535.f, u[3]/65535.f);
                        }
                    }

                    float sum = skinned_verts[v].weights.x + skinned_verts[v].weights.y + skinned_verts[v].weights.z + skinned_verts[v].weights.w;
                    if (sum > 0.00001f) {
                        skinned_verts[v].weights /= sum;
                    } else {
                        skinned_verts[v].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                    }
                }
                gpu_mesh->create_skinned_from_data(skinned_verts, indices);
            } else {
                gpu_mesh->create_from_data(vertices, indices);
            }

            ModelPart part;
            part.mesh = gpu_mesh;
            part.transform = node_world;
            part.node_index = (int)node_idx;
            part.is_skinned = is_skinned_prim;
            part.skin_index = gn.skin;
            part.name = m_nodes[node_idx].name + "_part" + std::to_string(p);
            part.min_bounds = part_min;
            part.max_bounds = part_max;
            part.cpu_vertices = std::move(vertices);
            part.cpu_indices = std::move(indices);

            if (prim.material >= 0 && prim.material < (int)model.materials_count) {
                part.material_name = tg3_str_to_string(model.materials[prim.material].name, "mat_" + std::to_string(prim.material));
                part.color = mat_colors[prim.material];
                part.texture_id = mat_textures[prim.material];
            } else {
                part.material_name = "default";
                part.color = glm::vec4(1.0f);
                part.texture_id = 0;
            }

            int part_idx = (int)m_parts.size();
            m_parts.push_back(std::move(part));
            m_nodes[node_idx].part_indices.push_back(part_idx);
        }
    }

    tg3_model_free(&model);
    tg3_error_stack_free(&errors);

    compute_bounds();
    m_valid = !m_parts.empty();
    return m_valid;
}

bool Model3D::load_from_obj(const std::string& filepath) {
    m_parts.clear();
    m_nodes.clear();
    m_owned_textures.clear();
    m_skins.clear();
    m_animations.clear();
    m_valid = false;
    m_filepath = filepath;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string base_dir = "";
    size_t last_slash = filepath.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        base_dir = filepath.substr(0, last_slash + 1);
    }

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str(), base_dir.c_str());
    if (!ret) {
        CRAYON_LOG_ERROR("Failed to load OBJ model: {}", filepath);
        return false;
    }

    // Load textures from materials
    std::vector<GLuint> mat_textures(materials.size(), 0);
    for (size_t i = 0; i < materials.size(); ++i) {
        if (!materials[i].diffuse_texname.empty()) {
            std::string tex_path = base_dir + materials[i].diffuse_texname;
            auto tex = std::make_shared<Texture>();
            if (tex->load_from_file(tex_path, false)) {
                mat_textures[i] = tex->get_id();
                m_owned_textures.push_back(tex);
            }
        }
    }

    // Process shapes as parts
    for (size_t s = 0; s < shapes.size(); ++s) {
        const auto& shape = shapes[s];
        std::vector<Vertex3D> vertices;
        std::vector<GLuint> indices;
        glm::vec3 part_min( 1e30f);
        glm::vec3 part_max(-1e30f);

        size_t index_offset = 0;
        int shape_mat_id = -1;

        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            size_t fv = (size_t)shape.mesh.num_face_vertices[f];
            if (fv != 3) {
                index_offset += fv;
                continue;
            }

            if (shape_mat_id < 0 && f < shape.mesh.material_ids.size()) {
                shape_mat_id = shape.mesh.material_ids[f];
            }

            for (size_t v = 0; v < 3; ++v) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                Vertex3D vert{};

                vert.position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                vert.position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                vert.position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                part_min = glm::min(part_min, vert.position);
                part_max = glm::max(part_max, vert.position);

                if (idx.normal_index >= 0) {
                    vert.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    vert.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    vert.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                } else {
                    vert.normal = glm::vec3(0, 1, 0);
                }

                if (idx.texcoord_index >= 0) {
                    vert.uv.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    // OBJ texture coordinates use a bottom-left origin, but our
                    // textures are uploaded top-row-first (unflipped stb_image
                    // load), so V must be inverted to line up. Without this the
                    // texture is mirrored top-to-bottom on every OBJ model.
                    // (Mesh3D::load_from_obj already does this correctly - this
                    // path was inconsistent with it.)
                    vert.uv.y = 1.0f - attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                } else {
                    vert.uv = glm::vec2(0, 0);
                }

                vert.color = glm::vec4(1.0f);
                indices.push_back((GLuint)vertices.size());
                vertices.push_back(vert);
            }
            index_offset += fv;
        }

        if (vertices.empty()) continue;

        auto gpu_mesh = std::make_shared<Mesh3D>();
        gpu_mesh->create_from_data(vertices, indices);

        ModelPart part;
        part.mesh = gpu_mesh;
        part.transform = glm::mat4(1.0f);
        part.node_index = (int)m_nodes.size();
        part.name = shape.name.empty() ? ("shape_" + std::to_string(s)) : shape.name;
        part.min_bounds = part_min;
        part.max_bounds = part_max;
        part.cpu_vertices = std::move(vertices);
        part.cpu_indices = std::move(indices);

        if (shape_mat_id >= 0 && shape_mat_id < (int)materials.size()) {
            part.material_name = materials[shape_mat_id].name;
            part.texture_id = mat_textures[shape_mat_id];
            part.color = glm::vec4(materials[shape_mat_id].diffuse[0], materials[shape_mat_id].diffuse[1], materials[shape_mat_id].diffuse[2], 1.0f);
        } else {
            part.material_name = "default";
            part.texture_id = 0;
            part.color = glm::vec4(1.0f);
        }

        ModelNode node;
        node.name = part.name;
        node.local_matrix = glm::mat4(1.0f);
        node.world_matrix = glm::mat4(1.0f);
        node.part_indices.push_back((int)m_parts.size());

        m_parts.push_back(std::move(part));
        m_nodes.push_back(std::move(node));
    }

    compute_bounds();
    m_valid = !m_parts.empty();
    return m_valid;
}

std::shared_ptr<Model3D> Model3D::create_from_mesh(std::shared_ptr<Mesh3D> mesh, const std::string& name) {
    auto model = std::make_shared<Model3D>();
    if (!mesh) return model;

    ModelPart part;
    part.mesh = mesh;
    part.transform = glm::mat4(1.0f);
    part.node_index = 0;
    part.name = name;
    part.material_name = "default";
    part.color = glm::vec4(1.0f);
    part.texture_id = 0;
    part.min_bounds = glm::vec3(-0.5f);
    part.max_bounds = glm::vec3( 0.5f);

    ModelNode node;
    node.name = name;
    node.local_matrix = glm::mat4(1.0f);
    node.world_matrix = glm::mat4(1.0f);
    node.part_indices.push_back(0);

    model->m_parts.push_back(std::move(part));
    model->m_nodes.push_back(std::move(node));
    model->compute_bounds();
    model->m_valid = true;
    return model;
}

void Model3D::update_node_world_matrices() {
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].parent_index < 0) {
            m_nodes[i].world_matrix = m_nodes[i].local_matrix;
        } else {
            int p = m_nodes[i].parent_index;
            m_nodes[i].world_matrix = m_nodes[p].world_matrix * m_nodes[i].local_matrix;
        }
    }

    // Update part transforms
    for (auto& part : m_parts) {
        if (part.node_index >= 0 && part.node_index < (int)m_nodes.size()) {
            part.transform = m_nodes[part.node_index].world_matrix;
        }
    }
}

void Model3D::compute_bounds() {
    if (m_parts.empty()) {
        m_min_bounds = glm::vec3(0.0f);
        m_max_bounds = glm::vec3(0.0f);
        return;
    }

    m_min_bounds = glm::vec3( 1e30f);
    m_max_bounds = glm::vec3(-1e30f);

    for (const auto& part : m_parts) {
        glm::vec3 corners[8] = {
            part.min_bounds,
            {part.max_bounds.x, part.min_bounds.y, part.min_bounds.z},
            {part.min_bounds.x, part.max_bounds.y, part.min_bounds.z},
            {part.max_bounds.x, part.max_bounds.y, part.min_bounds.z},
            {part.min_bounds.x, part.min_bounds.y, part.max_bounds.z},
            {part.max_bounds.x, part.min_bounds.y, part.max_bounds.z},
            {part.min_bounds.x, part.max_bounds.y, part.max_bounds.z},
            part.max_bounds
        };

        for (int c = 0; c < 8; ++c) {
            glm::vec4 tc = part.transform * glm::vec4(corners[c], 1.0f);
            m_min_bounds = glm::min(m_min_bounds, glm::vec3(tc));
            m_max_bounds = glm::max(m_max_bounds, glm::vec3(tc));
        }
    }
}

void Model3D::draw(MeshRenderer3D& renderer, const glm::mat4& world_transform, GLuint override_texture) const {
    for (const auto& part : m_parts) {
        if (!part.mesh) continue;
        glm::mat4 final_tf = world_transform * part.transform;
        GLuint tex = (override_texture != 0) ? override_texture : part.texture_id;
        renderer.draw_mesh(*part.mesh, final_tf, tex);
    }
}

void Model3D::draw_node(MeshRenderer3D& renderer, int node_index, const glm::mat4& world_transform, GLuint override_texture) const {
    if (node_index < 0 || node_index >= (int)m_nodes.size()) return;

    for (int part_idx : m_nodes[node_index].part_indices) {
        if (part_idx >= 0 && part_idx < (int)m_parts.size()) {
            draw_part(renderer, part_idx, world_transform, override_texture);
        }
    }

    for (int child_idx : m_nodes[node_index].children) {
        draw_node(renderer, child_idx, world_transform, override_texture);
    }
}

void Model3D::draw_node(MeshRenderer3D& renderer, const std::string& node_name, const glm::mat4& world_transform, GLuint override_texture) const {
    int idx = find_node_index(node_name);
    if (idx >= 0) {
        draw_node(renderer, idx, world_transform, override_texture);
    }
}

void Model3D::draw_part(MeshRenderer3D& renderer, int part_index, const glm::mat4& world_transform, GLuint override_texture) const {
    if (part_index < 0 || part_index >= (int)m_parts.size()) return;
    const auto& part = m_parts[part_index];
    if (!part.mesh) return;

    glm::mat4 final_tf = world_transform * part.transform;
    GLuint tex = (override_texture != 0) ? override_texture : part.texture_id;
    renderer.draw_mesh(*part.mesh, final_tf, tex);
}

const ModelNode* Model3D::get_node(size_t index) const {
    if (index < m_nodes.size()) return &m_nodes[index];
    return nullptr;
}

int Model3D::find_node_index(const std::string& name) const {
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].name == name) return (int)i;
    }
    return -1;
}

const ModelNode* Model3D::find_node(const std::string& name) const {
    int idx = find_node_index(name);
    return idx >= 0 ? &m_nodes[idx] : nullptr;
}

const ModelPart* Model3D::get_part(size_t index) const {
    if (index < m_parts.size()) return &m_parts[index];
    return nullptr;
}

ModelPart* Model3D::get_part(size_t index) {
    if (index < m_parts.size()) return &m_parts[index];
    return nullptr;
}

void Model3D::set_part_texture(size_t index, GLuint texture_id) {
    if (index < m_parts.size()) {
        m_parts[index].texture_id = texture_id;
    }
}

void Model3D::set_part_color(size_t index, const glm::vec4& color) {
    if (index < m_parts.size()) {
        m_parts[index].color = color;
    }
}

void Model3D::get_bounds(glm::vec3& out_min, glm::vec3& out_max) const {
    out_min = m_min_bounds;
    out_max = m_max_bounds;
}

glm::vec3 Model3D::get_center() const {
    return (m_min_bounds + m_max_bounds) * 0.5f;
}

glm::vec3 Model3D::get_size() const {
    return m_max_bounds - m_min_bounds;
}

void Model3D::get_collision_data(std::vector<glm::vec3>& out_vertices, std::vector<uint32_t>& out_indices, bool apply_transforms) const {
    out_vertices.clear();
    out_indices.clear();

    for (const auto& part : m_parts) {
        uint32_t base_idx = (uint32_t)out_vertices.size();
        for (const auto& v : part.cpu_vertices) {
            glm::vec3 p = v.position;
            if (apply_transforms) {
                p = glm::vec3(part.transform * glm::vec4(p, 1.0f));
            }
            out_vertices.push_back(p);
        }

        for (GLuint idx : part.cpu_indices) {
            out_indices.push_back(base_idx + idx);
        }
    }
}

void Model3D::draw_skinned(MeshRenderer3D& renderer, const glm::mat4& world_transform, const glm::mat4* bone_matrices, size_t bone_count, GLuint override_texture) const {
    for (const auto& part : m_parts) {
        if (!part.mesh) continue;
        GLuint tex = (override_texture != 0) ? override_texture : part.texture_id;
        if (part.is_skinned && bone_matrices && bone_count > 0) {
            renderer.draw_mesh_skinned(*part.mesh, world_transform, bone_matrices, bone_count, tex);
        } else {
            glm::mat4 final_tf = world_transform * part.transform;
            renderer.draw_mesh(*part.mesh, final_tf, tex);
        }
    }
}

const ModelSkin* Model3D::get_skin(size_t index) const {
    if (index < m_skins.size()) return &m_skins[index];
    return nullptr;
}

const AnimationClip* Model3D::get_animation(size_t index) const {
    if (index < m_animations.size()) return &m_animations[index];
    return nullptr;
}

const AnimationClip* Model3D::find_animation(const std::string& name) const {
    for (const auto& a : m_animations) {
        if (a.name == name) return &a;
    }
    return nullptr;
}

int Model3D::find_animation_index(const std::string& name) const {
    for (size_t i = 0; i < m_animations.size(); ++i) {
        if (m_animations[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

std::vector<std::pair<std::string, int>> Model3D::get_physics_skeleton_joints(size_t skin_index) const {
    std::vector<std::pair<std::string, int>> joints;
    if (skin_index < m_skins.size()) {
        const auto& s = m_skins[skin_index];
        joints.reserve(s.joints.size());
        for (const auto& j : s.joints) {
            joints.push_back({ j.name, j.parent_joint_index });
        }
    }
    return joints;
}

} // namespace crayon
