#include "mesh3d.hpp"
#include "model3d.hpp"
#include "default_shaders.hpp"
#include "mesh_optimize.hpp"
#include "../core/log.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#if defined(__GNUC__) && (__GNUC__ >= 12)
#pragma GCC diagnostic ignored "-Winvalid-constexpr"
#endif
#endif
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace crayon {

// Uploads the element buffer as 16-bit indices when the mesh has fewer than 65536
// vertices (almost every game asset) and as 32-bit otherwise. Half the index
// bandwidth and a better fit for the GPU's vertex-fetch hardware.
static void upload_index_buffer(GLuint& ebo, const std::vector<GLuint>& indices, size_t vertex_count,
                                GLenum& type, size_t& type_size) {
    type = GL_UNSIGNED_INT;
    type_size = sizeof(GLuint);
    if (indices.empty()) return;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    if (vertex_count <= 65535) {
        std::vector<uint16_t> small(indices.size());
        for (size_t i = 0; i < indices.size(); ++i) small[i] = static_cast<uint16_t>(indices[i]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, small.size() * sizeof(uint16_t), small.data(), GL_STATIC_DRAW);
        type = GL_UNSIGNED_SHORT;
        type_size = sizeof(uint16_t);
    } else {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    }
}

template <typename V>
static void compute_mesh_bounds(const std::vector<V>& verts, glm::vec3& mn, glm::vec3& mx, bool& valid) {
    valid = !verts.empty();
    if (!valid) { mn = mx = glm::vec3(0.0f); return; }
    mn = glm::vec3(std::numeric_limits<float>::max());
    mx = glm::vec3(std::numeric_limits<float>::lowest());
    for (const auto& v : verts) { mn = glm::min(mn, v.position); mx = glm::max(mx, v.position); }
}

// Flips any triangle whose winding disagrees with its vertex normals, so every generated mesh
// is CCW-outward. (Several generators below emitted inward-wound triangles; harmless with culling
// off, but they would disappear under backface culling.)
static void orient_to_normals(const std::vector<Vertex3D>& verts, std::vector<GLuint>& idx) {
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        const glm::vec3& a = verts[idx[t]].position;
        const glm::vec3& b = verts[idx[t + 1]].position;
        const glm::vec3& c = verts[idx[t + 2]].position;
        glm::vec3 fn = glm::cross(b - a, c - a);
        if (glm::dot(fn, fn) < 1e-16f) continue;                    // degenerate (pole) triangle
        glm::vec3 vn = verts[idx[t]].normal + verts[idx[t + 1]].normal + verts[idx[t + 2]].normal;
        if (glm::dot(fn, vn) < 0.0f) std::swap(idx[t + 1], idx[t + 2]);
    }
}

Mesh3D::Mesh3D() = default;

Mesh3D::~Mesh3D() {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ebo) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
}

void Mesh3D::create_from_data(const std::vector<Vertex3D>& vertices, const std::vector<GLuint>& indices) {
    compute_mesh_bounds(vertices, m_bounds_min, m_bounds_max, m_bounds_valid);
    if (m_retain_cpu && !soft_backend()) { m_cpu_vertices = vertices; m_cpu_indices = indices; }
    if (soft_backend()) {
        // CPU backend: no GL objects, the mesh simply lives in memory.
        m_cpu_vertices = vertices;
        m_cpu_indices = indices;
        m_cpu_skinned.clear();
        m_vertex_count = static_cast<GLsizei>(vertices.size());
        m_index_count = static_cast<GLsizei>(indices.size());
        m_is_skinned = false;
        m_submeshes.clear();
        return;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
        glDeleteBuffers(1, &m_ebo);
        m_vao = m_vbo = m_ebo = 0;
    }

    m_vertex_count = static_cast<GLsizei>(vertices.size());
    m_index_count = static_cast<GLsizei>(indices.size());
    m_is_skinned = false;
    m_submeshes.clear();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex3D), vertices.data(), GL_STATIC_DRAW);

    upload_index_buffer(m_ebo, indices, vertices.size(), m_index_type, m_index_size);

    // 0: Position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, position)));

    // 1: Normal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, normal)));

    // 2: UV (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, uv)));

    // 3: Color (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, color)));

    glBindVertexArray(0);
}

void Mesh3D::create_skinned_from_data(const std::vector<SkinnedVertex3D>& vertices, const std::vector<GLuint>& indices) {
    compute_mesh_bounds(vertices, m_bounds_min, m_bounds_max, m_bounds_valid);
    if (soft_backend()) {
        m_cpu_skinned = vertices;
        m_cpu_indices = indices;
        m_cpu_vertices.clear();
        m_vertex_count = static_cast<GLsizei>(vertices.size());
        m_index_count = static_cast<GLsizei>(indices.size());
        m_is_skinned = true;
        m_submeshes.clear();
        return;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
        glDeleteBuffers(1, &m_ebo);
        m_vao = m_vbo = m_ebo = 0;
    }

    m_vertex_count = static_cast<GLsizei>(vertices.size());
    m_index_count = static_cast<GLsizei>(indices.size());
    m_is_skinned = true;
    m_submeshes.clear();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SkinnedVertex3D), vertices.data(), GL_STATIC_DRAW);

    upload_index_buffer(m_ebo, indices, vertices.size(), m_index_type, m_index_size);

    // 0: Position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex3D), reinterpret_cast<void*>(offsetof(SkinnedVertex3D, position)));

    // 1: Normal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex3D), reinterpret_cast<void*>(offsetof(SkinnedVertex3D, normal)));

    // 2: UV (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex3D), reinterpret_cast<void*>(offsetof(SkinnedVertex3D, uv)));

    // 3: Color (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex3D), reinterpret_cast<void*>(offsetof(SkinnedVertex3D, color)));

    // 4: Joints (uvec4)
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 4, GL_UNSIGNED_INT, sizeof(SkinnedVertex3D), reinterpret_cast<void*>(offsetof(SkinnedVertex3D, joints)));

    // 5: Weights (vec4)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex3D), reinterpret_cast<void*>(offsetof(SkinnedVertex3D, weights)));

    glBindVertexArray(0);
}

bool Mesh3D::load_from_obj(const std::string& filepath) {
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
    if (!warn.empty()) {
        CRAYON_LOG_WARN("tinyobj warning: {}", warn);
    }
    if (!err.empty()) {
        CRAYON_LOG_ERROR("tinyobj error: {}", err);
    }
    if (!ret) {
        CRAYON_LOG_ERROR("Failed to load OBJ model from: {}", filepath);
        return false;
    }

    std::vector<Vertex3D> vertices;
    std::vector<GLuint> indices;

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = static_cast<size_t>(shape.mesh.num_face_vertices[f]);
            if (fv != 3) {
                // Not a triangle, skip or handle triangulation
                index_offset += fv;
                continue;
            }

            size_t tri_start = vertices.size();
            bool has_normals = true;

            for (size_t v = 0; v < 3; v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                Vertex3D vert{};

                // Position
                vert.position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                vert.position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                vert.position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                // Normal
                if (idx.normal_index >= 0) {
                    vert.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    vert.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    vert.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                } else {
                    has_normals = false;
                    vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                // UV
                if (idx.texcoord_index >= 0) {
                    vert.uv.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    vert.uv.y = 1.0f - attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                } else {
                    vert.uv = glm::vec2(0.0f, 0.0f);
                }

                // Color
                if (!attrib.colors.empty()) {
                    vert.color.r = attrib.colors[3 * size_t(idx.vertex_index) + 0];
                    vert.color.g = attrib.colors[3 * size_t(idx.vertex_index) + 1];
                    vert.color.b = attrib.colors[3 * size_t(idx.vertex_index) + 2];
                    vert.color.a = 1.0f;
                } else {
                    vert.color = glm::vec4(1.0f);
                }

                indices.push_back(static_cast<GLuint>(vertices.size()));
                vertices.push_back(vert);
            }

            if (!has_normals && vertices.size() >= tri_start + 3) {
                glm::vec3 p0 = vertices[tri_start + 0].position;
                glm::vec3 p1 = vertices[tri_start + 1].position;
                glm::vec3 p2 = vertices[tri_start + 2].position;
                glm::vec3 fn = glm::cross(p1 - p0, p2 - p0);
                float len = glm::length(fn);
                if (len > 0.0001f) fn /= len;
                else fn = glm::vec3(0.0f, 1.0f, 0.0f);
                vertices[tri_start + 0].normal = fn;
                vertices[tri_start + 1].normal = fn;
                vertices[tri_start + 2].normal = fn;
            }

            index_offset += 3;
        }
    }

    // The loop above emits 3 fresh vertices per triangle. Weld identical ones and reorder for
    // the GPU's post-transform cache (typically 4-6x fewer vertices to process).
    const size_t raw_vertex_count = vertices.size();
    mesh_opt::optimize(vertices, indices);
    create_from_data(vertices, indices);
    CRAYON_LOG_INFO("Loaded OBJ model {}: {} vertices (welded from {}), {} indices", filepath, m_vertex_count, raw_vertex_count, m_index_count);
    return true;
}

bool Mesh3D::load_from_gltf(const std::string& filepath) {
    Model3D model;
    if (!model.load_from_gltf(filepath)) {
        return false;
    }
    std::vector<Vertex3D> all_vertices;
    std::vector<GLuint> all_indices;
    std::vector<SubMesh> submeshes;
    submeshes.reserve(model.get_parts().size());

    for (const auto& part : model.get_parts()) {
        GLuint base_idx = static_cast<GLuint>(all_vertices.size());
        GLuint index_start = static_cast<GLuint>(all_indices.size());
        for (const auto& v : part.cpu_vertices) {
            Vertex3D transformed_v = v;
            transformed_v.position = glm::vec3(part.transform * glm::vec4(v.position, 1.0f));
            glm::mat3 norm_mat = glm::transpose(glm::inverse(glm::mat3(part.transform)));
            transformed_v.normal = glm::normalize(norm_mat * v.normal);
            transformed_v.color = v.color * part.color;
            all_vertices.push_back(transformed_v);
        }
        for (GLuint idx : part.cpu_indices) {
            all_indices.push_back(base_idx + idx);
        }

        // Record which texture this part's triangles should use. Previously
        // this information was discarded once everything was merged into one
        // vertex/index buffer, so any glTF file with more than one material
        // rendered every part with whatever single texture the caller bound -
        // e.g. a character's skin texture smeared across its clothes and
        // hair too. Keeping a per-part draw range fixes that.
        SubMesh sm;
        sm.index_offset = index_start;
        sm.index_count = static_cast<GLuint>(part.cpu_indices.size());
        sm.texture_id = part.texture_id;
        sm.color = part.color;
        if (sm.index_count > 0) {
            submeshes.push_back(sm);
        }
    }

    create_from_data(all_vertices, all_indices);

    // Only worth tracking submeshes if there's more than one, or the single
    // part actually carries a texture - otherwise behave exactly as before.
    bool multi_material = false;
    if (submeshes.size() > 1) {
        for (size_t i = 1; i < submeshes.size(); ++i) {
            if (submeshes[i].texture_id != submeshes[0].texture_id) {
                multi_material = true;
                break;
            }
        }
    }
    if (multi_material) {
        m_submeshes = std::move(submeshes);
        CRAYON_LOG_INFO("glTF model {} has {} materials - Mesh3D::draw() will bind each part's texture automatically", filepath, m_submeshes.size());
    }

    CRAYON_LOG_INFO("Loaded glTF model {}: {} vertices, {} indices", filepath, m_vertex_count, m_index_count);
    return true;
}

void Mesh3D::draw() const {
    if (m_vao == 0) return;
    glBindVertexArray(m_vao);

    if (!m_submeshes.empty()) {
        // Multi-material mesh: bind each part's own texture before drawing
        // its slice of the index buffer, instead of one texture for the
        // whole mesh. A submesh with no texture (texture_id == 0) leaves
        // whatever the caller already bound (e.g. the renderer's checker
        // fallback) untouched, matching the previous single-texture behavior.
        for (const auto& sm : m_submeshes) {
            if (sm.texture_id != 0) {
                glBindTexture(GL_TEXTURE_2D, sm.texture_id);
            }
            glDrawElements(GL_TRIANGLES, sm.index_count, m_index_type,
                           reinterpret_cast<void*>(static_cast<uintptr_t>(sm.index_offset * m_index_size)));
        }
    } else if (m_index_count > 0) {
        glDrawElements(GL_TRIANGLES, m_index_count, m_index_type, nullptr);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, m_vertex_count);
    }

    glBindVertexArray(0);
}

std::shared_ptr<Mesh3D> Mesh3D::create_cube(float size) {
    float h = size * 0.5f;

    std::vector<Vertex3D> vertices = {
        // Front face
        { {-h, -h,  h}, {0, 0, 1}, {0, 0}, {1, 1, 1, 1} },
        { { h, -h,  h}, {0, 0, 1}, {1, 0}, {1, 1, 1, 1} },
        { { h,  h,  h}, {0, 0, 1}, {1, 1}, {1, 1, 1, 1} },
        { {-h,  h,  h}, {0, 0, 1}, {0, 1}, {1, 1, 1, 1} },
        // Back face
        { { h, -h, -h}, {0, 0, -1}, {0, 0}, {1, 1, 1, 1} },
        { {-h, -h, -h}, {0, 0, -1}, {1, 0}, {1, 1, 1, 1} },
        { {-h,  h, -h}, {0, 0, -1}, {1, 1}, {1, 1, 1, 1} },
        { { h,  h, -h}, {0, 0, -1}, {0, 1}, {1, 1, 1, 1} },
        // Top face
        { {-h,  h,  h}, {0, 1, 0}, {0, 0}, {1, 1, 1, 1} },
        { { h,  h,  h}, {0, 1, 0}, {1, 0}, {1, 1, 1, 1} },
        { { h,  h, -h}, {0, 1, 0}, {1, 1}, {1, 1, 1, 1} },
        { {-h,  h, -h}, {0, 1, 0}, {0, 1}, {1, 1, 1, 1} },
        // Bottom face
        { {-h, -h, -h}, {0, -1, 0}, {0, 0}, {1, 1, 1, 1} },
        { { h, -h, -h}, {0, -1, 0}, {1, 0}, {1, 1, 1, 1} },
        { { h, -h,  h}, {0, -1, 0}, {1, 1}, {1, 1, 1, 1} },
        { {-h, -h,  h}, {0, -1, 0}, {0, 1}, {1, 1, 1, 1} },
        // Right face
        { { h, -h,  h}, {1, 0, 0}, {0, 0}, {1, 1, 1, 1} },
        { { h, -h, -h}, {1, 0, 0}, {1, 0}, {1, 1, 1, 1} },
        { { h,  h, -h}, {1, 0, 0}, {1, 1}, {1, 1, 1, 1} },
        { { h,  h,  h}, {1, 0, 0}, {0, 1}, {1, 1, 1, 1} },
        // Left face
        { {-h, -h, -h}, {-1, 0, 0}, {0, 0}, {1, 1, 1, 1} },
        { {-h, -h,  h}, {-1, 0, 0}, {1, 0}, {1, 1, 1, 1} },
        { {-h,  h,  h}, {-1, 0, 0}, {1, 1}, {1, 1, 1, 1} },
        { {-h,  h, -h}, {-1, 0, 0}, {0, 1}, {1, 1, 1, 1} }
    };

    std::vector<GLuint> indices = {
         0,  1,  2,  2,  3,  0,
         4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    auto mesh = std::make_shared<Mesh3D>();
    mesh->set_retain_cpu(true);
    mesh->create_from_data(vertices, indices);
    return mesh;
}

std::shared_ptr<Mesh3D> Mesh3D::create_plane(float width, float depth, int subdivisions) {
    std::vector<Vertex3D> vertices;
    std::vector<GLuint> indices;

    float dx = width / subdivisions;
    float dz = depth / subdivisions;
    float start_x = -width * 0.5f;
    float start_z = -depth * 0.5f;

    for (int z = 0; z <= subdivisions; ++z) {
        for (int x = 0; x <= subdivisions; ++x) {
            Vertex3D v{};
            v.position = glm::vec3(start_x + x * dx, 0.0f, start_z + z * dz);
            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            v.uv = glm::vec2(static_cast<float>(x), static_cast<float>(z));
            v.color = glm::vec4(1.0f);
            vertices.push_back(v);
        }
    }

    int stride = subdivisions + 1;
    for (int z = 0; z < subdivisions; ++z) {
        for (int x = 0; x < subdivisions; ++x) {
            GLuint i0 = z * stride + x;
            GLuint i1 = i0 + 1;
            GLuint i2 = (z + 1) * stride + x;
            GLuint i3 = i2 + 1;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    auto mesh = std::make_shared<Mesh3D>();
    mesh->set_retain_cpu(true);
    mesh->create_from_data(vertices, indices);
    return mesh;
}

std::shared_ptr<Mesh3D> Mesh3D::create_sphere(float radius, int rings, int sectors) {
    std::vector<Vertex3D> vertices;
    std::vector<GLuint> indices;

    float const R = 1.0f / static_cast<float>(rings - 1);
    float const S = 1.0f / static_cast<float>(sectors - 1);

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            float const y = std::sin(-3.14159265f / 2.0f + 3.14159265f * r * R);
            float const x = std::cos(2.0f * 3.14159265f * s * S) * std::sin(3.14159265f * r * R);
            float const z = std::sin(2.0f * 3.14159265f * s * S) * std::sin(3.14159265f * r * R);

            Vertex3D v{};
            v.position = glm::vec3(x, y, z) * radius;
            v.normal = glm::vec3(x, y, z);
            v.uv = glm::vec2(s * S, r * R);
            v.color = glm::vec4(1.0f);
            vertices.push_back(v);
        }
    }

    for (int r = 0; r < rings - 1; ++r) {
        for (int s = 0; s < sectors - 1; ++s) {
            GLuint i0 = r * sectors + s;
            GLuint i1 = r * sectors + (s + 1);
            GLuint i2 = (r + 1) * sectors + (s + 1);
            GLuint i3 = (r + 1) * sectors + s;

            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    auto mesh = std::make_shared<Mesh3D>();
    mesh->set_retain_cpu(true);
    orient_to_normals(vertices, indices);
    mesh->create_from_data(vertices, indices);
    return mesh;
}

std::shared_ptr<Mesh3D> Mesh3D::create_cylinder(float radius, float height, int sectors) {
    if (sectors < 3) sectors = 12;

    std::vector<Vertex3D> vertices;
    std::vector<GLuint> indices;

    float half_h = height * 0.5f;
    float step = (2.0f * 3.14159265f) / sectors;

    // ----------------------------------------------------------------
    // 1. Side wall (as before)
    // ----------------------------------------------------------------
    for (int i = 0; i <= sectors; ++i) {
        float angle = i * step;
        float x = std::cos(angle);
        float z = std::sin(angle);
        float u = static_cast<float>(i) / sectors;

        vertices.push_back({ {x * radius, half_h, z * radius}, {x, 0.0f, z}, {u, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f} });
        vertices.push_back({ {x * radius, -half_h, z * radius}, {x, 0.0f, z}, {u, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} });
    }

    for (int i = 0; i < sectors; ++i) {
        GLuint i0 = i * 2;
        GLuint i1 = i0 + 1;
        GLuint i2 = (i + 1) * 2;
        GLuint i3 = i2 + 1;

        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);

        indices.push_back(i2);
        indices.push_back(i1);
        indices.push_back(i3);
    }

    // ----------------------------------------------------------------
    // 2. Top cap (fan around a center vertex, +Y normal)
    // ----------------------------------------------------------------
    {
        glm::vec3 up(0.0f, 1.0f, 0.0f);
        GLuint center_idx = static_cast<GLuint>(vertices.size());
        vertices.push_back({ {0.0f, half_h, 0.0f}, up, {0.5f, 0.5f}, {1, 1, 1, 1} });

        for (int i = 0; i <= sectors; ++i) {
            float angle = i * step;
            float x = std::cos(angle) * radius;
            float z = std::sin(angle) * radius;
            // Map circle to a [0..1] UV square centered on the cap
            float u = 0.5f + std::cos(angle) * 0.5f;
            float v = 0.5f + std::sin(angle) * 0.5f;
            vertices.push_back({ {x, half_h, z}, up, {u, v}, {1, 1, 1, 1} });
        }

        for (int i = 0; i < sectors; ++i) {
            // +1 because center vertex was inserted first
            indices.push_back(center_idx);
            indices.push_back(center_idx + 1 + i);
            indices.push_back(center_idx + 1 + (i + 1));
        }
    }

    // ----------------------------------------------------------------
    // 3. Bottom cap (fan around a center vertex, -Y normal, reversed winding)
    // ----------------------------------------------------------------
    {
        glm::vec3 down(0.0f, -1.0f, 0.0f);
        GLuint center_idx = static_cast<GLuint>(vertices.size());
        vertices.push_back({ {0.0f, -half_h, 0.0f}, down, {0.5f, 0.5f}, {1, 1, 1, 1} });

        for (int i = 0; i <= sectors; ++i) {
            float angle = i * step;
            float x = std::cos(angle) * radius;
            float z = std::sin(angle) * radius;
            float u = 0.5f + std::cos(angle) * 0.5f;
            float v = 0.5f + std::sin(angle) * 0.5f;
            vertices.push_back({ {x, -half_h, z}, down, {u, v}, {1, 1, 1, 1} });
        }

        for (int i = 0; i < sectors; ++i) {
            // Reverse winding for correct front-facing orientation
            indices.push_back(center_idx);
            indices.push_back(center_idx + 1 + (i + 1));
            indices.push_back(center_idx + 1 + i);
        }
    }

    auto mesh = std::make_shared<Mesh3D>();
    mesh->set_retain_cpu(true);
    orient_to_normals(vertices, indices);
    mesh->create_from_data(vertices, indices);
    return mesh;
}

std::shared_ptr<Mesh3D> Mesh3D::create_cone(float radius, float height, int sectors) {
    if (sectors < 3) sectors = 12;
    std::vector<Vertex3D> vertices;
    std::vector<GLuint> indices;

    float half_h = height * 0.5f;
    float step = (2.0f * 3.14159265f) / sectors;
    glm::vec3 apex(0.0f, half_h, 0.0f);

    for (int i = 0; i < sectors; ++i) {
        float a0 = i * step;
        float a1 = (i + 1) * step;

        glm::vec3 b0(std::cos(a0) * radius, -half_h, std::sin(a0) * radius);
        glm::vec3 b1(std::cos(a1) * radius, -half_h, std::sin(a1) * radius);

        // outward-facing side normal (the previous cross-product order pointed inward)
        glm::vec3 n = -glm::normalize(glm::cross(b0 - apex, b1 - apex));

        GLuint base_idx = static_cast<GLuint>(vertices.size());
        vertices.push_back({ apex, n, {0.5f, 1.0f}, {1, 1, 1, 1} });
        vertices.push_back({ b0,   n, {static_cast<float>(i) / sectors, 0.0f}, {1, 1, 1, 1} });
        vertices.push_back({ b1,   n, {static_cast<float>(i + 1) / sectors, 0.0f}, {1, 1, 1, 1} });

        indices.push_back(base_idx);
        indices.push_back(base_idx + 1);
        indices.push_back(base_idx + 2);
    }

    // Bottom cap
    glm::vec3 bottom_center(0.0f, -half_h, 0.0f);
    glm::vec3 down(0.0f, -1.0f, 0.0f);
    for (int i = 0; i < sectors; ++i) {
        float a0 = i * step;
        float a1 = (i + 1) * step;

        glm::vec3 b0(std::cos(a0) * radius, -half_h, std::sin(a0) * radius);
        glm::vec3 b1(std::cos(a1) * radius, -half_h, std::sin(a1) * radius);

        GLuint base_idx = static_cast<GLuint>(vertices.size());
        vertices.push_back({ bottom_center, down, {0.5f, 0.5f}, {1, 1, 1, 1} });
        vertices.push_back({ b1, down, {0.5f + std::cos(a1) * 0.5f, 0.5f + std::sin(a1) * 0.5f}, {1, 1, 1, 1} });
        vertices.push_back({ b0, down, {0.5f + std::cos(a0) * 0.5f, 0.5f + std::sin(a0) * 0.5f}, {1, 1, 1, 1} });

        indices.push_back(base_idx);
        indices.push_back(base_idx + 1);
        indices.push_back(base_idx + 2);
    }

    auto mesh = std::make_shared<Mesh3D>();
    mesh->set_retain_cpu(true);
    orient_to_normals(vertices, indices);
    mesh->create_from_data(vertices, indices);
    return mesh;
}

std::shared_ptr<Mesh3D> Mesh3D::create_pyramid(float base_size, float height) {
    float h = base_size * 0.5f;
    float half_h = height * 0.5f;
    glm::vec3 apex(0.0f, half_h, 0.0f);

    glm::vec3 p0(-h, -half_h,  h);
    glm::vec3 p1( h, -half_h,  h);
    glm::vec3 p2( h, -half_h, -h);
    glm::vec3 p3(-h, -half_h, -h);

    std::vector<Vertex3D> vertices;
    std::vector<GLuint> indices;

    auto add_tri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        GLuint base = static_cast<GLuint>(vertices.size());
        vertices.push_back({ a, n, {0.5f, 1.0f}, {1, 1, 1, 1} });
        vertices.push_back({ b, n, {0.0f, 0.0f}, {1, 1, 1, 1} });
        vertices.push_back({ c, n, {1.0f, 0.0f}, {1, 1, 1, 1} });
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    };

    add_tri(apex, p0, p1);
    add_tri(apex, p1, p2);
    add_tri(apex, p2, p3);
    add_tri(apex, p3, p0);

    glm::vec3 down(0.0f, -1.0f, 0.0f);
    GLuint base = static_cast<GLuint>(vertices.size());
    vertices.push_back({ p0, down, {0, 1}, {1, 1, 1, 1} });
    vertices.push_back({ p2, down, {1, 0}, {1, 1, 1, 1} });
    vertices.push_back({ p1, down, {1, 1}, {1, 1, 1, 1} });
    vertices.push_back({ p3, down, {0, 0}, {1, 1, 1, 1} });

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);

    indices.push_back(base + 0);
    indices.push_back(base + 3);
    indices.push_back(base + 1);

    auto mesh = std::make_shared<Mesh3D>();
    mesh->set_retain_cpu(true);
    mesh->create_from_data(vertices, indices);
    return mesh;
}

std::shared_ptr<Mesh3D> Mesh3D::create_torus(float radius, float tube_radius, int radial_segments, int tubular_segments) {
    if (radial_segments < 3) radial_segments = 16;
    if (tubular_segments < 3) tubular_segments = 12;

    std::vector<Vertex3D> vertices;
    std::vector<GLuint> indices;

    for (int j = 0; j <= radial_segments; ++j) {
        float u = (static_cast<float>(j) / radial_segments) * 6.2831853f;
        float cu = std::cos(u);
        float su = std::sin(u);

        for (int i = 0; i <= tubular_segments; ++i) {
            float v = (static_cast<float>(i) / tubular_segments) * 6.2831853f;
            float cv = std::cos(v);
            float sv = std::sin(v);

            float x = (radius + tube_radius * cv) * cu;
            float y = tube_radius * sv;
            float z = (radius + tube_radius * cv) * su;

            glm::vec3 norm(cv * cu, sv, cv * su);
            glm::vec2 uv(static_cast<float>(j) / radial_segments, static_cast<float>(i) / tubular_segments);

            vertices.push_back({ {x, y, z}, norm, uv, {1, 1, 1, 1} });
        }
    }

    for (int j = 0; j < radial_segments; ++j) {
        for (int i = 0; i < tubular_segments; ++i) {
            GLuint a = j * (tubular_segments + 1) + i;
            GLuint b = (j + 1) * (tubular_segments + 1) + i;
            GLuint c = (j + 1) * (tubular_segments + 1) + (i + 1);
            GLuint d = j * (tubular_segments + 1) + (i + 1);

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(d);

            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    auto mesh = std::make_shared<Mesh3D>();
    mesh->set_retain_cpu(true);
    orient_to_normals(vertices, indices);
    mesh->create_from_data(vertices, indices);
    return mesh;
}

std::shared_ptr<Mesh3D> Mesh3D::create_capsule(float radius, float height, int rings, int sectors) {
    if (rings < 4) rings = 8;
    if (sectors < 4) sectors = 12;

    std::vector<Vertex3D> vertices;
    std::vector<GLuint> indices;

    float half_h = height * 0.5f;

    for (int r = 0; r <= rings; ++r) {
        float phi = -1.5707963f + (3.14159265f * static_cast<float>(r) / rings);
        float cp = std::cos(phi);
        float sp = std::sin(phi);

        float y = sp * radius;
        if (phi > 0.0f) y += half_h;
        else y -= half_h;

        for (int s = 0; s <= sectors; ++s) {
            float theta = (6.2831853f * static_cast<float>(s) / sectors);
            float ct = std::cos(theta);
            float st = std::sin(theta);

            float x = cp * ct * radius;
            float z = cp * st * radius;

            glm::vec3 norm(cp * ct, sp, cp * st);
            glm::vec2 uv(static_cast<float>(s) / sectors, static_cast<float>(r) / rings);

            vertices.push_back({ {x, y, z}, norm, uv, {1, 1, 1, 1} });
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            GLuint a = r * (sectors + 1) + s;
            GLuint b = (r + 1) * (sectors + 1) + s;
            GLuint c = (r + 1) * (sectors + 1) + (s + 1);
            GLuint d = r * (sectors + 1) + (s + 1);

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(d);

            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    auto mesh = std::make_shared<Mesh3D>();
    mesh->set_retain_cpu(true);
    orient_to_normals(vertices, indices);
    mesh->create_from_data(vertices, indices);
    return mesh;
}

std::shared_ptr<Mesh3D> Mesh3D::create_grid(float size, int divisions) {
    return create_plane(size, size, divisions);
}

// -------------------------------------------------------------
// MeshRenderer3D Implementation
// -------------------------------------------------------------

MeshRenderer3D::MeshRenderer3D() = default;

MeshRenderer3D::~MeshRenderer3D() {
    shutdown();
}

bool MeshRenderer3D::init() {
    if (soft_backend()) {
        m_checker_texture = Texture::create_checker();
        m_white_texture = Texture::create_white();
        m_prim_cube = Mesh3D::create_cube(1.0f);
        m_prim_plane = Mesh3D::create_plane(1.0f, 1.0f, 1);
        m_prim_sphere = Mesh3D::create_sphere(0.5f, 16, 16);
        m_prim_cylinder = Mesh3D::create_cylinder(0.5f, 1.0f, 16);
        m_prim_cone = Mesh3D::create_cone(0.5f, 1.0f, 16);
        m_prim_pyramid = Mesh3D::create_pyramid(1.0f, 1.0f);
        m_prim_torus = Mesh3D::create_torus(0.8f, 0.25f, 16, 12);
        m_prim_capsule = Mesh3D::create_capsule(0.4f, 0.8f, 8, 12);
        CRAYON_LOG_INFO("MeshRenderer3D initialized (software rasterizer)");
        return true;
    }
    m_shader = std::make_unique<Shader>();
    if (!m_shader->load_from_memory(SHADER_3D_VS, SHADER_3D_FS)) {
        CRAYON_LOG_ERROR("Failed to compile 3D Retro Mesh Shader");
        return false;
    }

    // Bone palette size: 128 where the GPU allows it, fewer on GLES parts with small
    // vertex-uniform budgets (GLES 3.0 only guarantees 256 vec4s).
    {
        GLint vectors = 0;
#if defined(CRAYON_GLES)
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &vectors);
#else
        GLint comps = 0;
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &comps);
        vectors = comps / 4;
#endif
        int avail = (vectors > 0) ? (vectors - 32) / 4 : 56;
        m_max_bones = std::clamp(avail, 32, 128);
    }
    if (!init_mesh_prog(m_prog_static, false) || !init_mesh_prog(m_prog_skinned, true)) {
        CRAYON_LOG_ERROR("Failed to compile optimized 3D mesh programs");
        return false;
    }

    m_checker_texture = Texture::create_checker();
    m_white_texture = Texture::create_white();

    m_prim_cube = Mesh3D::create_cube(1.0f);
    m_prim_plane = Mesh3D::create_plane(1.0f, 1.0f, 1);
    m_prim_sphere = Mesh3D::create_sphere(0.5f, 16, 16);
    m_prim_cylinder = Mesh3D::create_cylinder(0.5f, 1.0f, 16);
    m_prim_cone = Mesh3D::create_cone(0.5f, 1.0f, 16);
    m_prim_pyramid = Mesh3D::create_pyramid(1.0f, 1.0f);
    m_prim_torus = Mesh3D::create_torus(0.8f, 0.25f, 16, 12);
    m_prim_capsule = Mesh3D::create_capsule(0.4f, 0.8f, 8, 12);

    glGenVertexArrays(1, &m_dyn_vao);
    glGenBuffers(1, &m_dyn_vbo);
    glBindVertexArray(m_dyn_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_dyn_vbo);
    m_dyn_capacity = 4096;
    glBufferData(GL_ARRAY_BUFFER, m_dyn_capacity * sizeof(Vertex3D), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), reinterpret_cast<void*>(offsetof(Vertex3D, color)));

    glBindVertexArray(0);

    CRAYON_LOG_INFO("MeshRenderer3D initialized with procedural primitives");
    return true;
}

void MeshRenderer3D::shutdown() {
    if (m_grid_vao) {
        glDeleteVertexArrays(1, &m_grid_vao);
        m_grid_vao = 0;
    }
    if (m_grid_vbo) {
        glDeleteBuffers(1, &m_grid_vbo);
        m_grid_vbo = 0;
    }
    m_grid_size = -1.0f;
    m_grid_divs = -1;
    if (m_dyn_vao) {
        glDeleteVertexArrays(1, &m_dyn_vao);
        m_dyn_vao = 0;
    }
    if (m_dyn_vbo) {
        glDeleteBuffers(1, &m_dyn_vbo);
        m_dyn_vbo = 0;
    }
    m_prim_cube.reset();
    m_prim_plane.reset();
    m_prim_sphere.reset();
    m_prim_cylinder.reset();
    m_prim_cone.reset();
    m_prim_pyramid.reset();
    m_prim_torus.reset();
    m_prim_capsule.reset();
    m_checker_texture.reset();
    m_white_texture.reset();
    m_shader.reset();
    m_prog_static.shader.reset();
    m_prog_skinned.shader.reset();
    m_sky_shader.reset();
    if (m_sky_vbo) { glDeleteBuffers(1, &m_sky_vbo); m_sky_vbo = 0; }
    if (m_sky_vao) { glDeleteVertexArrays(1, &m_sky_vao); m_sky_vao = 0; }
}

void MeshRenderer3D::begin(const Camera& camera, float aspect) {
    if (!soft_backend()) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_BLEND);
    }

    m_view = camera.get_view_matrix();
    m_proj = camera.get_projection_matrix(aspect);
    m_viewproj = m_proj * m_view;
    m_cam_position = camera.get_position();
    ++m_frame_version;
    m_culled_draws = 0;
    m_transparent.clear();
    if (soft_backend()) soft::framebuffer().begin(m_view, m_proj, m_cam_position);

    m_current_matrix = glm::mat4(1.0f);
    while (!m_matrix_stack.empty()) {
        m_matrix_stack.pop();
    }
}

void MeshRenderer3D::end() {
    flush_transparent();
    if (soft_backend()) return;
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0); // draw_mesh() leaves its program bound between draws
}

void MeshRenderer3D::push_matrix() {
    m_matrix_stack.push(m_current_matrix);
}

void MeshRenderer3D::pop_matrix() {
    if (!m_matrix_stack.empty()) {
        m_current_matrix = m_matrix_stack.top();
        m_matrix_stack.pop();
    } else {
        m_current_matrix = glm::mat4(1.0f);
    }
}

void MeshRenderer3D::load_identity() {
    m_current_matrix = glm::mat4(1.0f);
}

void MeshRenderer3D::translate(const glm::vec3& translation) {
    m_current_matrix = glm::translate(m_current_matrix, translation);
}

void MeshRenderer3D::rotate(float angle_rad, const glm::vec3& axis) {
    m_current_matrix = glm::rotate(m_current_matrix, angle_rad, axis);
}

void MeshRenderer3D::scale(const glm::vec3& factors) {
    m_current_matrix = glm::scale(m_current_matrix, factors);
}

const glm::mat4& MeshRenderer3D::get_current_transform() const {
    return m_current_matrix;
}

void MeshRenderer3D::set_directional_light(const glm::vec3& dir, const glm::vec3& color, const glm::vec3& ambient) {
    m_light_dir = glm::normalize(dir);
    m_light_color = color;
    m_ambient_color = ambient;
    ++m_frame_version;
}

void MeshRenderer3D::set_point_light(int index, const glm::vec3& pos, const glm::vec3& color, float radius, float intensity) {
    if (index >= 0 && index < 4) {
        m_point_lights[index].pos = pos;
        m_point_lights[index].color = color;
        m_point_lights[index].radius = radius;
        m_point_lights[index].intensity = intensity;
        m_point_lights[index].enabled = true;
        ++m_frame_version;
    }
}

void MeshRenderer3D::set_point_light_enabled(int index, bool enabled) {
    if (index >= 0 && index < 4) {
        m_point_lights[index].enabled = enabled;
        ++m_frame_version;
    }
}

void MeshRenderer3D::set_spot_light(int index, const glm::vec3& pos, const glm::vec3& dir, const glm::vec3& color, float radius, float intensity, float inner_angle_deg, float outer_angle_deg) {
    if (index >= 0 && index < 2) {
        m_spot_lights[index].pos = pos;
        m_spot_lights[index].dir = glm::normalize(dir);
        m_spot_lights[index].color = color;
        m_spot_lights[index].radius = radius;
        m_spot_lights[index].intensity = intensity;
        m_spot_lights[index].inner_cutoff_cos = std::cos(glm::radians(inner_angle_deg));
        m_spot_lights[index].outer_cutoff_cos = std::cos(glm::radians(outer_angle_deg));
        m_spot_lights[index].enabled = true;
        ++m_frame_version;
    }
}

void MeshRenderer3D::set_spot_light_enabled(int index, bool enabled) {
    if (index >= 0 && index < 2) {
        m_spot_lights[index].enabled = enabled;
        ++m_frame_version;
    }
}

// Uniform names for the light arrays, built once instead of via string concatenation per draw.
static const char* const kPointLightNames[4][4] = {
    {"u_point_lights[0].pos","u_point_lights[0].color","u_point_lights[0].radius","u_point_lights[0].intensity"},
    {"u_point_lights[1].pos","u_point_lights[1].color","u_point_lights[1].radius","u_point_lights[1].intensity"},
    {"u_point_lights[2].pos","u_point_lights[2].color","u_point_lights[2].radius","u_point_lights[2].intensity"},
    {"u_point_lights[3].pos","u_point_lights[3].color","u_point_lights[3].radius","u_point_lights[3].intensity"},
};
static const char* const kSpotLightNames[2][7] = {
    {"u_spot_lights[0].pos","u_spot_lights[0].dir","u_spot_lights[0].color","u_spot_lights[0].radius","u_spot_lights[0].intensity","u_spot_lights[0].inner_cutoff","u_spot_lights[0].outer_cutoff"},
    {"u_spot_lights[1].pos","u_spot_lights[1].dir","u_spot_lights[1].color","u_spot_lights[1].radius","u_spot_lights[1].intensity","u_spot_lights[1].inner_cutoff","u_spot_lights[1].outer_cutoff"},
};

// ---------------------------------------------------------------------------
// Optimized mesh path
// ---------------------------------------------------------------------------
// Prepends #defines right after the #version line.
static std::string with_defines(const char* src, const std::string& defines) {
    std::string s(src);
    size_t nl = s.find('\n');
    if (nl == std::string::npos) return s;
    s.insert(nl + 1, defines);
    return s;
}

bool MeshRenderer3D::init_mesh_prog(MeshProgState& st, bool skinned) {
    std::string defs = "#define NORMALMAT 1\n";
    if (skinned) {
        defs += "#define SKINNED 1\n#define MAX_BONES " + std::to_string(m_max_bones) + "\n";
    }
    st.shader = std::make_unique<Shader>();
    if (!st.shader->load_from_memory(with_defines(SHADER_3D_VS, defs), SHADER_3D_FS)) {
        st.shader.reset();
        return false;
    }
    st.loc_model = st.shader->get_uniform_location("u_model");
    st.loc_normal = st.shader->get_uniform_location("u_normal_mat");
    st.loc_color = st.shader->get_uniform_location("u_color");
    st.loc_bones = st.shader->get_uniform_location("u_bone_matrices");
    st.uploaded_version = ~0ull;
    return true;
}

static bool retro_uniforms_equal(const RetroEffects& a, const RetroEffects& b) {
    return a.jitter_enabled == b.jitter_enabled && a.jitter_resolution == b.jitter_resolution &&
           a.affine_blend == b.affine_blend && a.fog_enabled == b.fog_enabled &&
           a.fog_start == b.fog_start && a.fog_end == b.fog_end && a.fog_color == b.fog_color;
}

// Uploads everything that is constant for a whole frame (camera, lights, fog, retro). The
// old code re-sent all of this on every single draw call.
void MeshRenderer3D::upload_frame_uniforms(MeshProgState& st) {
    if (st.uploaded_version == m_frame_version && st.uploaded_shading == m_shading_mode &&
        retro_uniforms_equal(st.uploaded_retro, m_retro)) {
        return;
    }
    Shader& sh = *st.shader;
    sh.set_mat4("u_view", m_view);
    sh.set_mat4("u_proj", m_proj);
    sh.set_int("u_texture", 0);

    sh.set_int("u_jitter_enabled", m_retro.jitter_enabled ? 1 : 0);
    sh.set_vec2("u_jitter_res", m_retro.jitter_resolution);
    sh.set_float("u_affine_blend", m_retro.affine_blend);

    sh.set_vec3("u_light_dir", m_light_dir);
    sh.set_vec3("u_light_color", m_light_color);
    sh.set_vec3("u_ambient_color", m_ambient_color);
    sh.set_int("u_shading_mode", static_cast<int>(m_shading_mode));

    int num_lights = 0;
    for (int i = 0; i < 4; ++i) {
        if (!m_point_lights[i].enabled) continue;
        const auto& nm = kPointLightNames[num_lights];
        sh.set_vec3(nm[0], m_point_lights[i].pos);
        sh.set_vec3(nm[1], m_point_lights[i].color);
        sh.set_float(nm[2], m_point_lights[i].radius);
        sh.set_float(nm[3], m_point_lights[i].intensity);
        num_lights++;
    }
    sh.set_int("u_num_point_lights", num_lights);

    int num_spot_lights = 0;
    for (int i = 0; i < 2; ++i) {
        if (!m_spot_lights[i].enabled) continue;
        const auto& nm = kSpotLightNames[num_spot_lights];
        sh.set_vec3(nm[0], m_spot_lights[i].pos);
        sh.set_vec3(nm[1], m_spot_lights[i].dir);
        sh.set_vec3(nm[2], m_spot_lights[i].color);
        sh.set_float(nm[3], m_spot_lights[i].radius);
        sh.set_float(nm[4], m_spot_lights[i].intensity);
        sh.set_float(nm[5], m_spot_lights[i].inner_cutoff_cos);
        sh.set_float(nm[6], m_spot_lights[i].outer_cutoff_cos);
        num_spot_lights++;
    }
    sh.set_int("u_num_spot_lights", num_spot_lights);

    sh.set_int("u_fog_enabled", m_retro.fog_enabled ? 1 : 0);
    sh.set_float("u_fog_start", m_retro.fog_start);
    sh.set_float("u_fog_end", m_retro.fog_end);
    sh.set_vec3("u_fog_color", m_retro.fog_color);

    st.uploaded_version = m_frame_version;
    st.uploaded_shading = m_shading_mode;
    st.uploaded_retro = m_retro;
}

bool MeshRenderer3D::sphere_visible(const glm::vec3& c, float r) const {
    // Gribb-Hartmann planes from the view-projection matrix (columns of m_viewproj).
    const glm::mat4& m = m_viewproj;
    for (int i = 0; i < 6; ++i) {
        const int axis = i >> 1;
        const float sign = (i & 1) ? -1.0f : 1.0f;
        glm::vec4 plane(m[0][3] + sign * m[0][axis], m[1][3] + sign * m[1][axis],
                        m[2][3] + sign * m[2][axis], m[3][3] + sign * m[3][axis]);
        float len = glm::length(glm::vec3(plane));
        if (len <= 1e-8f) continue;
        if (glm::dot(glm::vec3(plane), c) + plane.w < -r * len) return false;
    }
    return true;
}

bool MeshRenderer3D::aabb_visible(const glm::vec3& mn, const glm::vec3& mx, const glm::mat4& model) const {
    const glm::mat4 mvp = m_viewproj * model;
    unsigned all = 0x3F;
    for (int i = 0; i < 8; ++i) {
        glm::vec4 p(i & 1 ? mx.x : mn.x, i & 2 ? mx.y : mn.y, i & 4 ? mx.z : mn.z, 1.0f);
        glm::vec4 c = mvp * p;
        unsigned oc = (c.x < -c.w) | ((c.x > c.w) << 1) | ((c.y < -c.w) << 2) |
                      ((c.y > c.w) << 3) | ((c.z < -c.w) << 4) | ((c.z > c.w) << 5);
        all &= oc;
        if (!all) return true; // some corner pair straddles every plane -> may be visible
    }
    return all == 0;
}

void MeshRenderer3D::draw_mesh(const Mesh3D& mesh, const glm::mat4& model, GLuint texture_id, const glm::vec4& tint,
                               const glm::vec3* bounds_min, const glm::vec3* bounds_max, bool single_sided,
                               bool alpha_blend) {
    glm::mat4 final_model = m_current_matrix * model;

    // ---- auto culling: frustum + max distance (bounds come from the part, else from the mesh) ----
    if (m_auto_cull) {
        const glm::vec3* lo = bounds_min;
        const glm::vec3* hi = bounds_max;
        if (!(lo && hi) && mesh.has_bounds()) { lo = &mesh.bounds_min(); hi = &mesh.bounds_max(); }
        if (lo && hi) {
            glm::vec3 lo_v = *lo, hi_v = *hi;
            if (mesh.is_skinned()) {   // animation can leave the bind-pose box: grow it
                glm::vec3 c = (lo_v + hi_v) * 0.5f, e = (hi_v - lo_v) * 0.5f * (1.0f + m_skinned_cull_margin);
                lo_v = c - e; hi_v = c + e;
            }
            if (!aabb_visible(lo_v, hi_v, final_model)) { ++m_culled_draws; return; }
            if (m_max_draw_distance > 0.0f) {
                glm::vec3 wc = glm::vec3(final_model * glm::vec4((lo_v + hi_v) * 0.5f, 1.0f));
                float scale = std::max({glm::length(glm::vec3(final_model[0])), glm::length(glm::vec3(final_model[1])),
                                        glm::length(glm::vec3(final_model[2]))});
                float radius = glm::length((hi_v - lo_v) * 0.5f) * scale;
                if (glm::length(wc - m_cam_position) - radius > m_max_draw_distance) { ++m_culled_draws; return; }
            }
        }
    }

    // ---- transparent draws are deferred and sorted (see flush_transparent) ----
    if ((alpha_blend || tint.a < 0.999f) && !m_in_transparent_flush) {
        TransparentDraw d;
        d.mesh = &mesh;
        d.model = final_model;
        d.texture = texture_id;
        d.tint = tint;
        d.single_sided = single_sided;
        d.has_bounds = false;
        const glm::vec3* lo = bounds_min;
        const glm::vec3* hi = bounds_max;
        if (!(lo && hi) && mesh.has_bounds()) { lo = &mesh.bounds_min(); hi = &mesh.bounds_max(); }
        glm::vec3 local_center(0.0f);
        if (lo && hi) { d.has_bounds = true; d.lo = *lo; d.hi = *hi; local_center = (*lo + *hi) * 0.5f; }
        d.distance = glm::length(glm::vec3(final_model * glm::vec4(local_center, 1.0f)) - m_cam_position);
        m_transparent.push_back(d);
        return;
    }

    // Single-sided geometry gets backface culling even when the global cull state is off.
    const bool auto_back = m_auto_cull && single_sided && m_cull_mode == 0;

    if (soft_backend()) {
        m_force_cull_back = auto_back;
        soft_draw_mesh(mesh, final_model, texture_id != 0 ? texture_id : m_checker_texture->get_id(), tint);
        m_force_cull_back = false;
        return;
    }

    MeshProgState& st = m_prog_static;
    st.shader->bind();
    upload_frame_uniforms(st);

    glUniformMatrix4fv(st.loc_model, 1, GL_FALSE, glm::value_ptr(final_model));
    const glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(final_model))); // once per draw, not per vertex
    glUniformMatrix3fv(st.loc_normal, 1, GL_FALSE, glm::value_ptr(normal_mat));
    glUniform4fv(st.loc_color, 1, glm::value_ptr(tint));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id != 0 ? texture_id : m_checker_texture->get_id());

    if (auto_back) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
    mesh.draw();
    if (auto_back) glDisable(GL_CULL_FACE);
}

// ---------------------------------------------------------------------------
// Skybox
// ---------------------------------------------------------------------------
static const char* kSkyVS = R"(#version 330 core
layout(location = 0) in vec3 a_pos;
uniform mat4 u_view_rot;
uniform mat4 u_proj;
out vec3 v_dir;
void main() {
    v_dir = a_pos;
    vec4 p = u_proj * u_view_rot * vec4(a_pos, 1.0);
    gl_Position = p.xyww;   // depth = 1.0: only fills pixels nothing else has covered
}
)";

static const char* kSkyFS = R"(#version 330 core
in vec3 v_dir;
uniform samplerCube u_sky;
uniform vec3 u_tint;
out vec4 frag_color;
void main() {
    frag_color = vec4(texture(u_sky, v_dir).rgb * u_tint, 1.0);
}
)";

void MeshRenderer3D::draw_skybox(Skybox& sky, const glm::vec3& tint) {
    if (!sky.valid()) return;
    if (soft_backend()) {
        soft::framebuffer().draw_skybox(m_view, m_proj, tint,
                                        [&sky](const glm::vec3& d) { return sky.sample(d); });
        return;
    }
    if (!m_sky_shader) {
        m_sky_shader = std::make_unique<Shader>();
        if (!m_sky_shader->load_from_memory(kSkyVS, kSkyFS)) {
            CRAYON_LOG_ERROR("Skybox shader failed to compile");
            m_sky_shader.reset();
            return;
        }
        static const float cube[] = {
            -1, 1,-1, -1,-1,-1,  1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
            -1,-1, 1, -1,-1,-1, -1, 1,-1, -1, 1,-1, -1, 1, 1, -1,-1, 1,
             1,-1,-1,  1,-1, 1,  1, 1, 1,  1, 1, 1,  1, 1,-1,  1,-1,-1,
            -1,-1, 1, -1, 1, 1,  1, 1, 1,  1, 1, 1,  1,-1, 1, -1,-1, 1,
            -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1,
            -1,-1,-1, -1,-1, 1,  1,-1,-1,  1,-1,-1, -1,-1, 1,  1,-1, 1};
        glGenVertexArrays(1, &m_sky_vao);
        glGenBuffers(1, &m_sky_vbo);
        glBindVertexArray(m_sky_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_sky_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glBindVertexArray(0);
    }
    GLuint cube_tex = sky.gl_texture();
    if (cube_tex == 0) return;

    glm::mat4 rot_view = m_view;
    rot_view[3] = glm::vec4(0, 0, 0, 1);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    const bool cull_was_on = m_cull_mode != 0;
    if (cull_was_on) glDisable(GL_CULL_FACE);

    m_sky_shader->bind();
    m_sky_shader->set_mat4("u_view_rot", rot_view);
    m_sky_shader->set_mat4("u_proj", m_proj);
    m_sky_shader->set_vec3("u_tint", tint);
    m_sky_shader->set_int("u_sky", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube_tex);
    glBindVertexArray(m_sky_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glUseProgram(0);

    if (cull_was_on) glEnable(GL_CULL_FACE);
    glDepthMask(m_depth_write ? GL_TRUE : GL_FALSE);
}

void MeshRenderer3D::flush_transparent() {
    if (m_transparent.empty() || m_in_transparent_flush) return;
    std::vector<TransparentDraw> queue;
    queue.swap(m_transparent);
    std::stable_sort(queue.begin(), queue.end(),
                     [](const TransparentDraw& a, const TransparentDraw& b) { return a.distance > b.distance; });

    m_in_transparent_flush = true;
    m_blend_pass = true;
    const glm::mat4 saved_stack = m_current_matrix;
    m_current_matrix = glm::mat4(1.0f);          // queued matrices already include the stack
    if (!soft_backend()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    for (const TransparentDraw& d : queue) {
        draw_mesh(*d.mesh, d.model, d.texture, d.tint, d.has_bounds ? &d.lo : nullptr, d.has_bounds ? &d.hi : nullptr,
                  d.single_sided, true);
    }
    if (!soft_backend()) {
        glDepthMask(m_depth_write ? GL_TRUE : GL_FALSE);
        glDisable(GL_BLEND);
    }
    m_current_matrix = saved_stack;
    m_blend_pass = false;
    m_in_transparent_flush = false;
}

void MeshRenderer3D::draw_mesh_skinned(const Mesh3D& mesh, const glm::mat4& model, const glm::mat4* bone_matrices, size_t bone_count, GLuint texture_id, const glm::vec4& tint) {
    glm::mat4 final_model = m_current_matrix * model;

    if (m_auto_cull && mesh.has_bounds()) {
        glm::vec3 c = (mesh.bounds_min() + mesh.bounds_max()) * 0.5f;
        glm::vec3 e = (mesh.bounds_max() - mesh.bounds_min()) * 0.5f * (1.0f + m_skinned_cull_margin);
        bool vis = aabb_visible(c - e, c + e, final_model);
        if (vis && m_max_draw_distance > 0.0f) {
            glm::vec3 wc = glm::vec3(final_model * glm::vec4(c, 1.0f));
            float scale = std::max({glm::length(glm::vec3(final_model[0])), glm::length(glm::vec3(final_model[1])),
                                    glm::length(glm::vec3(final_model[2]))});
            vis = glm::length(wc - m_cam_position) - glm::length(e) * scale <= m_max_draw_distance;
        }
        if (!vis) { ++m_culled_draws; return; }
    }

    if (soft_backend()) {
        soft_draw_mesh_skinned(mesh, final_model, bone_matrices, bone_count,
                               texture_id != 0 ? texture_id : m_checker_texture->get_id(), tint);
        return;
    }

    MeshProgState& st = m_prog_skinned;
    st.shader->bind();
    upload_frame_uniforms(st);

    glUniformMatrix4fv(st.loc_model, 1, GL_FALSE, glm::value_ptr(final_model));
    const glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(final_model)));
    glUniformMatrix3fv(st.loc_normal, 1, GL_FALSE, glm::value_ptr(normal_mat));
    glUniform4fv(st.loc_color, 1, glm::value_ptr(tint));
    if (bone_matrices && bone_count > 0 && st.loc_bones != -1) {
        glUniformMatrix4fv(st.loc_bones, static_cast<GLsizei>(std::min(bone_count, static_cast<size_t>(m_max_bones))),
                           GL_FALSE, glm::value_ptr(bone_matrices[0]));
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id != 0 ? texture_id : m_checker_texture->get_id());

    mesh.draw();
}

void MeshRenderer3D::draw_model(const Model3D& model, const glm::mat4& transform, GLuint override_texture) {
    model.draw(*this, transform, override_texture);
}

void MeshRenderer3D::draw_model_skinned(const Model3D& model, const glm::mat4& transform, const glm::mat4* bone_matrices, size_t bone_count, GLuint override_texture) {
    model.draw_skinned(*this, transform, bone_matrices, bone_count, override_texture);
}

static glm::mat4 make_transform(const glm::vec3& pos, const glm::vec3& scale, const glm::vec3& rot) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, pos);
    if (rot.z != 0.0f) m = glm::rotate(m, rot.z, glm::vec3(0, 0, 1));
    if (rot.y != 0.0f) m = glm::rotate(m, rot.y, glm::vec3(0, 1, 0));
    if (rot.x != 0.0f) m = glm::rotate(m, rot.x, glm::vec3(1, 0, 0));
    m = glm::scale(m, scale);
    return m;
}

void MeshRenderer3D::draw_cube(const glm::vec3& pos, const glm::vec3& size, GLuint texture_id, const glm::vec3& rot) {
    if (m_prim_cube) draw_mesh(*m_prim_cube, make_transform(pos, size, rot), texture_id, m_active_color_3d, nullptr, nullptr, true);
}

void MeshRenderer3D::draw_plane(const glm::vec3& pos, float width, float depth, GLuint texture_id, const glm::vec3& rot) {
    if (m_prim_plane) draw_mesh(*m_prim_plane, make_transform(pos, glm::vec3(width, 1.0f, depth), rot), texture_id, m_active_color_3d);
}

void MeshRenderer3D::draw_sphere(const glm::vec3& pos, float radius, GLuint texture_id, const glm::vec3& rot) {
    if (m_prim_sphere) draw_mesh(*m_prim_sphere, make_transform(pos, glm::vec3(radius * 2.0f), rot), texture_id, m_active_color_3d, nullptr, nullptr, true);
}

void MeshRenderer3D::draw_cylinder(const glm::vec3& pos, float radius, float height, GLuint texture_id, const glm::vec3& rot) {
    if (m_prim_cylinder) draw_mesh(*m_prim_cylinder, make_transform(pos, glm::vec3(radius * 2.0f, height, radius * 2.0f), rot), texture_id, m_active_color_3d, nullptr, nullptr, true);
}

void MeshRenderer3D::draw_cone(const glm::vec3& pos, float radius, float height, GLuint texture_id, const glm::vec3& rot) {
    if (m_prim_cone) draw_mesh(*m_prim_cone, make_transform(pos, glm::vec3(radius * 2.0f, height, radius * 2.0f), rot), texture_id, m_active_color_3d, nullptr, nullptr, true);
}

void MeshRenderer3D::draw_pyramid(const glm::vec3& pos, float base_size, float height, GLuint texture_id, const glm::vec3& rot) {
    if (m_prim_pyramid) draw_mesh(*m_prim_pyramid, make_transform(pos, glm::vec3(base_size, height, base_size), rot), texture_id, m_active_color_3d, nullptr, nullptr, true);
}

void MeshRenderer3D::draw_torus(const glm::vec3& pos, float radius, float tube_radius, GLuint texture_id, const glm::vec3& rot) {
    (void)radius; (void)tube_radius;
    if (m_prim_torus) draw_mesh(*m_prim_torus, make_transform(pos, glm::vec3(1.0f), rot), texture_id, m_active_color_3d, nullptr, nullptr, true);
}

void MeshRenderer3D::draw_capsule(const glm::vec3& pos, float radius, float height, GLuint texture_id, const glm::vec3& rot) {
    (void)radius; (void)height;
    if (m_prim_capsule) draw_mesh(*m_prim_capsule, make_transform(pos, glm::vec3(1.0f), rot), texture_id, m_active_color_3d, nullptr, nullptr, true);
}

void MeshRenderer3D::draw_billboard(GLuint texture_id, const glm::vec3& position, const glm::vec2& size,
                                    BillboardMode mode, const glm::vec4& color,
                                    float u0, float v0, float u1, float v1) {
    if (m_auto_cull && !sphere_visible(position, 0.5f * std::max(size.x, size.y) * 1.4143f)) { ++m_culled_draws; return; }
    glm::vec3 right, up;
    if (mode == BillboardMode::Spherical) {
        right = glm::vec3(m_view[0][0], m_view[1][0], m_view[2][0]);
        up    = glm::vec3(m_view[0][1], m_view[1][1], m_view[2][1]);
    } else {
        glm::vec3 look = m_cam_position - position;
        look.y = 0.0f;
        if (glm::length(look) > 0.0001f) look = glm::normalize(look);
        else look = glm::vec3(0, 0, 1);
        up = glm::vec3(0.0f, 1.0f, 0.0f);
        right = glm::normalize(glm::cross(up, look));
    }

    glm::vec3 p0 = position - right * (size.x * 0.5f) - up * (size.y * 0.5f);
    glm::vec3 p1 = position + right * (size.x * 0.5f) - up * (size.y * 0.5f);
    glm::vec3 p2 = position + right * (size.x * 0.5f) + up * (size.y * 0.5f);
    glm::vec3 p3 = position - right * (size.x * 0.5f) + up * (size.y * 0.5f);

    glm::vec3 norm = glm::normalize(glm::cross(right, up));

    Vertex3D quad[6] = {
        { p0, norm, {u0, v1}, color },
        { p1, norm, {u1, v1}, color },
        { p2, norm, {u1, v0}, color },

        { p0, norm, {u0, v1}, color },
        { p2, norm, {u1, v0}, color },
        { p3, norm, {u0, v0}, color }
    };

    if (soft_backend()) { soft_draw_billboard(texture_id, quad); return; }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader->bind();
    m_shader->set_vec4("u_color", glm::vec4(1.0f));
    m_shader->set_mat4("u_model", glm::mat4(1.0f));
    m_shader->set_mat4("u_view", m_view);
    m_shader->set_mat4("u_proj", m_proj);
    m_shader->set_int("u_jitter_enabled", 0);
    m_shader->set_float("u_affine_blend", 0.0f);
    m_shader->set_int("u_shading_mode", 2); // Unlit
    m_shader->set_int("u_fog_enabled", m_retro.fog_enabled ? 1 : 0);
    m_shader->set_float("u_fog_start", m_retro.fog_start);
    m_shader->set_float("u_fog_end", m_retro.fog_end);
    m_shader->set_vec3("u_fog_color", m_retro.fog_color);

    m_shader->set_int("u_texture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id != 0 ? texture_id : m_white_texture->get_id());

    glBindVertexArray(m_dyn_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_dyn_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_shader->unbind();
    glDisable(GL_BLEND);
}

void MeshRenderer3D::draw_line_3d(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color) {
    Vertex3D line[2] = {
        { p1, {0, 1, 0}, {0, 0}, color },
        { p2, {0, 1, 0}, {1, 1}, color }
    };

    if (soft_backend()) { soft_draw_lines(line, 2, m_current_matrix); return; }

    m_shader->bind();
    m_shader->set_vec4("u_color", glm::vec4(1.0f));
    m_shader->set_mat4("u_model", m_current_matrix);
    m_shader->set_mat4("u_view", m_view);
    m_shader->set_mat4("u_proj", m_proj);
    m_shader->set_int("u_jitter_enabled", m_retro.jitter_enabled ? 1 : 0);
    m_shader->set_vec2("u_jitter_res", m_retro.jitter_resolution);
    m_shader->set_float("u_affine_blend", 0.0f);
    m_shader->set_int("u_shading_mode", 2);
    m_shader->set_int("u_fog_enabled", m_retro.fog_enabled ? 1 : 0);
    m_shader->set_float("u_fog_start", m_retro.fog_start);
    m_shader->set_float("u_fog_end", m_retro.fog_end);
    m_shader->set_vec3("u_fog_color", m_retro.fog_color);

    m_shader->set_int("u_texture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_white_texture->get_id());

    glBindVertexArray(m_dyn_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_dyn_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(line), line);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);

    m_shader->unbind();
}

void MeshRenderer3D::draw_lines_3d_batched(const Vertex3D* vertices, size_t count) {
    if (!vertices || count < 2) return;
    if (soft_backend()) { soft_draw_lines(vertices, count, glm::mat4(1.0f)); return; }

    m_shader->bind();
    m_shader->set_vec4("u_color", glm::vec4(1.0f));
    m_shader->set_mat4("u_model", glm::mat4(1.0f));
    m_shader->set_mat4("u_view", m_view);
    m_shader->set_mat4("u_proj", m_proj);
    m_shader->set_int("u_jitter_enabled", m_retro.jitter_enabled ? 1 : 0);
    m_shader->set_vec2("u_jitter_res", m_retro.jitter_resolution);
    m_shader->set_float("u_affine_blend", 0.0f);
    m_shader->set_int("u_shading_mode", 2);
    m_shader->set_int("u_fog_enabled", m_retro.fog_enabled ? 1 : 0);
    m_shader->set_float("u_fog_start", m_retro.fog_start);
    m_shader->set_float("u_fog_end", m_retro.fog_end);
    m_shader->set_vec3("u_fog_color", m_retro.fog_color);

    m_shader->set_int("u_texture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_white_texture->get_id());

    glBindVertexArray(m_dyn_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_dyn_vbo);

    size_t offset = 0;
    while (offset < count) {
        size_t chunk = std::min(count - offset, size_t(65536));
        if (chunk > m_dyn_capacity) {
            // Grow geometrically; the VBO name (and the VAO's attribute bindings) stay valid.
            size_t new_cap = m_dyn_capacity;
            while (new_cap < chunk) new_cap *= 2;
            m_dyn_capacity = new_cap;
            glBufferData(GL_ARRAY_BUFFER, m_dyn_capacity * sizeof(Vertex3D), nullptr, GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, chunk * sizeof(Vertex3D), vertices + offset);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(chunk));
        offset += chunk;
    }

    glBindVertexArray(0);
    m_shader->unbind();
}

void MeshRenderer3D::begin_line_batch() {
    m_line_batch.clear();
}

void MeshRenderer3D::add_line_to_batch(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color) {
    m_line_batch.push_back({ p1, {0, 1, 0}, {0, 0}, color });
    m_line_batch.push_back({ p2, {0, 1, 0}, {1, 1}, color });
}

void MeshRenderer3D::batch_wire_box(const glm::vec3& center, const glm::vec3& half_extent, const glm::quat& rot, const glm::vec4& color) {
    glm::vec3 corners[8] = {
        rot * glm::vec3(-half_extent.x, -half_extent.y, -half_extent.z) + center,
        rot * glm::vec3( half_extent.x, -half_extent.y, -half_extent.z) + center,
        rot * glm::vec3( half_extent.x,  half_extent.y, -half_extent.z) + center,
        rot * glm::vec3(-half_extent.x,  half_extent.y, -half_extent.z) + center,
        rot * glm::vec3(-half_extent.x, -half_extent.y,  half_extent.z) + center,
        rot * glm::vec3( half_extent.x, -half_extent.y,  half_extent.z) + center,
        rot * glm::vec3( half_extent.x,  half_extent.y,  half_extent.z) + center,
        rot * glm::vec3(-half_extent.x,  half_extent.y,  half_extent.z) + center
    };

    // Bottom
    add_line_to_batch(corners[0], corners[1], color);
    add_line_to_batch(corners[1], corners[2], color);
    add_line_to_batch(corners[2], corners[3], color);
    add_line_to_batch(corners[3], corners[0], color);
    // Top
    add_line_to_batch(corners[4], corners[5], color);
    add_line_to_batch(corners[5], corners[6], color);
    add_line_to_batch(corners[6], corners[7], color);
    add_line_to_batch(corners[7], corners[4], color);
    // Pillars
    add_line_to_batch(corners[0], corners[4], color);
    add_line_to_batch(corners[1], corners[5], color);
    add_line_to_batch(corners[2], corners[6], color);
    add_line_to_batch(corners[3], corners[7], color);
}

void MeshRenderer3D::batch_wire_sphere(const glm::vec3& center, float radius, const glm::vec4& color, int rings, int sectors) {
    if (rings < 4) rings = 8;
    if (sectors < 4) sectors = 12;

    float step = 6.2831853f / sectors;
    // 3 orthogonal main circles
    for (int i = 0; i < sectors; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;

        // XY circle
        add_line_to_batch(center + glm::vec3(std::cos(a1) * radius, std::sin(a1) * radius, 0.0f),
                          center + glm::vec3(std::cos(a2) * radius, std::sin(a2) * radius, 0.0f), color);
        // XZ circle
        add_line_to_batch(center + glm::vec3(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius),
                          center + glm::vec3(std::cos(a2) * radius, 0.0f, std::sin(a2) * radius), color);
        // YZ circle
        add_line_to_batch(center + glm::vec3(0.0f, std::cos(a1) * radius, std::sin(a1) * radius),
                          center + glm::vec3(0.0f, std::cos(a2) * radius, std::sin(a2) * radius), color);
    }
}

void MeshRenderer3D::batch_wire_capsule(const glm::vec3& center, float radius, float half_height, const glm::quat& rot, const glm::vec4& color, int segments) {
    if (segments < 6) segments = 12;
    float step = 6.2831853f / segments;

    glm::vec3 top_cap = rot * glm::vec3(0.0f, half_height, 0.0f) + center;
    glm::vec3 bot_cap = rot * glm::vec3(0.0f, -half_height, 0.0f) + center;
    (void)top_cap;
    (void)bot_cap;

    // Rings around cylinder ends
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;

        glm::vec3 p1 = rot * glm::vec3(std::cos(a1) * radius, half_height, std::sin(a1) * radius) + center;
        glm::vec3 p2 = rot * glm::vec3(std::cos(a2) * radius, half_height, std::sin(a2) * radius) + center;
        add_line_to_batch(p1, p2, color);

        glm::vec3 b1 = rot * glm::vec3(std::cos(a1) * radius, -half_height, std::sin(a1) * radius) + center;
        glm::vec3 b2 = rot * glm::vec3(std::cos(a2) * radius, -half_height, std::sin(a2) * radius) + center;
        add_line_to_batch(b1, b2, color);
    }

    // 4 vertical side lines
    glm::vec3 sides[4] = { {radius, 0, 0}, {-radius, 0, 0}, {0, 0, radius}, {0, 0, -radius} };
    for (int i = 0; i < 4; ++i) {
        glm::vec3 s_top = rot * (sides[i] + glm::vec3(0, half_height, 0)) + center;
        glm::vec3 s_bot = rot * (sides[i] - glm::vec3(0, half_height, 0)) + center;
        add_line_to_batch(s_top, s_bot, color);
    }

    // Hemisphere arcs
    int half_segs = segments / 2;
    float half_step = 3.14159265f / half_segs;
    for (int i = 0; i < half_segs; ++i) {
        float a1 = i * half_step;
        float a2 = (i + 1) * half_step;

        // Top cap arcs (XY and ZY)
        glm::vec3 t1_xy = rot * glm::vec3(std::cos(a1) * radius, half_height + std::sin(a1) * radius, 0.0f) + center;
        glm::vec3 t2_xy = rot * glm::vec3(std::cos(a2) * radius, half_height + std::sin(a2) * radius, 0.0f) + center;
        add_line_to_batch(t1_xy, t2_xy, color);

        glm::vec3 t1_zy = rot * glm::vec3(0.0f, half_height + std::sin(a1) * radius, std::cos(a1) * radius) + center;
        glm::vec3 t2_zy = rot * glm::vec3(0.0f, half_height + std::sin(a2) * radius, std::cos(a2) * radius) + center;
        add_line_to_batch(t1_zy, t2_zy, color);

        // Bottom cap arcs (XY and ZY)
        glm::vec3 b1_xy = rot * glm::vec3(std::cos(a1) * radius, -half_height - std::sin(a1) * radius, 0.0f) + center;
        glm::vec3 b2_xy = rot * glm::vec3(std::cos(a2) * radius, -half_height - std::sin(a2) * radius, 0.0f) + center;
        add_line_to_batch(b1_xy, b2_xy, color);

        glm::vec3 b1_zy = rot * glm::vec3(0.0f, -half_height - std::sin(a1) * radius, std::cos(a1) * radius) + center;
        glm::vec3 b2_zy = rot * glm::vec3(0.0f, -half_height - std::sin(a2) * radius, std::cos(a2) * radius) + center;
        add_line_to_batch(b1_zy, b2_zy, color);
    }
}

void MeshRenderer3D::batch_wire_cylinder(const glm::vec3& center, float radius, float half_height, const glm::quat& rot, const glm::vec4& color, int segments) {
    if (segments < 6) segments = 12;
    float step = 6.2831853f / segments;

    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;

        glm::vec3 p1 = rot * glm::vec3(std::cos(a1) * radius, half_height, std::sin(a1) * radius) + center;
        glm::vec3 p2 = rot * glm::vec3(std::cos(a2) * radius, half_height, std::sin(a2) * radius) + center;
        add_line_to_batch(p1, p2, color);

        glm::vec3 b1 = rot * glm::vec3(std::cos(a1) * radius, -half_height, std::sin(a1) * radius) + center;
        glm::vec3 b2 = rot * glm::vec3(std::cos(a2) * radius, -half_height, std::sin(a2) * radius) + center;
        add_line_to_batch(b1, b2, color);
    }

    // 4 vertical side lines
    glm::vec3 sides[4] = { {radius, 0, 0}, {-radius, 0, 0}, {0, 0, radius}, {0, 0, -radius} };
    for (int i = 0; i < 4; ++i) {
        glm::vec3 s_top = rot * (sides[i] + glm::vec3(0, half_height, 0)) + center;
        glm::vec3 s_bot = rot * (sides[i] - glm::vec3(0, half_height, 0)) + center;
        add_line_to_batch(s_top, s_bot, color);
    }
}

void MeshRenderer3D::end_line_batch() {
    if (!m_line_batch.empty()) {
        draw_lines_3d_batched(m_line_batch.data(), m_line_batch.size());
        m_line_batch.clear();
    }
}

void MeshRenderer3D::draw_lines_3d(const std::vector<glm::vec3>& points, const glm::vec4& color) {
    if (points.size() < 2) return;
    std::vector<Vertex3D> verts;
    verts.reserve((points.size() - 1) * 2);
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        verts.push_back({ points[i], {0, 1, 0}, {0, 0}, color });
        verts.push_back({ points[i + 1], {0, 1, 0}, {1, 1}, color });
    }
    draw_lines_3d_batched(verts.data(), verts.size());
}

void MeshRenderer3D::draw_grid_3d(float size, int divisions, float y_level, const glm::vec4& color) {
    if (divisions < 1) divisions = 10;
    if (soft_backend()) { soft_draw_grid(size, divisions, y_level, color); return; }
    
    // Per-instance cache (was function-static: shared by every renderer, leaked on
    // shutdown, and ignored the colour so changing it never changed the grid).
    if (m_grid_vao == 0) {
        glGenVertexArrays(1, &m_grid_vao);
        glGenBuffers(1, &m_grid_vbo);
    }

    if (m_grid_size != size || m_grid_divs != divisions || m_grid_y != y_level || m_grid_color != color) {
        std::vector<Vertex3D> lines;
        lines.reserve((divisions + 1) * 4);

        float half_size = size * 0.5f;
        float step = size / divisions;

        for (int i = 0; i <= divisions; ++i) {
            float c = -half_size + i * step;
            lines.push_back({ {c, y_level, -half_size}, {0, 1, 0}, {0, 0}, color });
            lines.push_back({ {c, y_level,  half_size}, {0, 1, 0}, {0, 0}, color });
            lines.push_back({ {-half_size, y_level, c}, {0, 1, 0}, {0, 0}, color });
            lines.push_back({ { half_size, y_level, c}, {0, 1, 0}, {0, 0}, color });
        }

        glBindVertexArray(m_grid_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_grid_vbo);
        glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(Vertex3D), lines.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, uv));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, color));

        glBindVertexArray(0);

        m_grid_count = lines.size();
        m_grid_size = size;
        m_grid_divs = divisions;
        m_grid_y = y_level;
        m_grid_color = color;
    }

    m_shader->bind();
    m_shader->set_vec4("u_color", glm::vec4(1.0f));
    m_shader->set_mat4("u_model", m_current_matrix);
    m_shader->set_mat4("u_view", m_view);
    m_shader->set_mat4("u_proj", m_proj);
    m_shader->set_int("u_jitter_enabled", m_retro.jitter_enabled ? 1 : 0);
    m_shader->set_vec2("u_jitter_res", m_retro.jitter_resolution);
    m_shader->set_float("u_affine_blend", 0.0f);
    m_shader->set_int("u_shading_mode", 2);
    m_shader->set_int("u_fog_enabled", m_retro.fog_enabled ? 1 : 0);
    m_shader->set_float("u_fog_start", m_retro.fog_start);
    m_shader->set_float("u_fog_end", m_retro.fog_end);
    m_shader->set_vec3("u_fog_color", m_retro.fog_color);

    m_shader->set_int("u_texture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_white_texture->get_id());

    glBindVertexArray(m_grid_vao);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_grid_count));
    glBindVertexArray(0);

    m_shader->unbind();
}

void MeshRenderer3D::draw_triangle_3d(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
                                      const glm::vec4& color, GLuint texture_id,
                                      const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3) {
    glm::vec3 fn = glm::cross(p2 - p1, p3 - p1);
    if (glm::length(fn) > 0.0001f) fn = glm::normalize(fn);
    else fn = glm::vec3(0, 1, 0);

    Vertex3D tri[3] = {
        { p1, fn, uv1, color },
        { p2, fn, uv2, color },
        { p3, fn, uv3, color }
    };

    if (soft_backend()) { soft_draw_triangle(tri, texture_id); return; }

    m_shader->bind();
    m_shader->set_vec4("u_color", glm::vec4(1.0f));
    m_shader->set_mat4("u_model", m_current_matrix);
    m_shader->set_mat4("u_view", m_view);
    m_shader->set_mat4("u_proj", m_proj);
    m_shader->set_int("u_jitter_enabled", m_retro.jitter_enabled ? 1 : 0);
    m_shader->set_vec2("u_jitter_res", m_retro.jitter_resolution);
    m_shader->set_float("u_affine_blend", m_retro.affine_blend);
    m_shader->set_int("u_shading_mode", static_cast<int>(m_shading_mode));
    m_shader->set_vec3("u_light_dir", m_light_dir);
    m_shader->set_vec3("u_light_color", m_light_color);
    m_shader->set_vec3("u_ambient_color", m_ambient_color);

    m_shader->set_int("u_texture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id != 0 ? texture_id : m_white_texture->get_id());

    glBindVertexArray(m_dyn_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_dyn_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(tri), tri);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    m_shader->unbind();
}

void MeshRenderer3D::draw_quad_3d(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, const glm::vec3& p4,
                                  const glm::vec4& color, GLuint texture_id,
                                  const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3, const glm::vec2& uv4) {
    draw_triangle_3d(p1, p2, p3, color, texture_id, uv1, uv2, uv3);
    draw_triangle_3d(p1, p3, p4, color, texture_id, uv1, uv3, uv4);
}

void MeshRenderer3D::draw_billboard_rot(GLuint texture_id, const glm::vec3& position, const glm::vec2& size,
                                        float angle_rad, BillboardMode mode, const glm::vec4& color,
                                        float u0, float v0, float u1, float v1) {
    glm::vec3 right, up;
    if (mode == BillboardMode::Spherical) {
        right = glm::vec3(m_view[0][0], m_view[1][0], m_view[2][0]);
        up    = glm::vec3(m_view[0][1], m_view[1][1], m_view[2][1]);
    } else {
        glm::vec3 look = m_cam_position - position;
        look.y = 0.0f;
        if (glm::length(look) > 0.0001f) look = glm::normalize(look);
        else look = glm::vec3(0, 0, 1);
        up = glm::vec3(0.0f, 1.0f, 0.0f);
        right = glm::normalize(glm::cross(up, look));
    }

    if (angle_rad != 0.0f) {
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);
        glm::vec3 new_right = right * c + up * s;
        glm::vec3 new_up    = -right * s + up * c;
        right = new_right;
        up    = new_up;
    }

    glm::vec3 p0 = position - right * (size.x * 0.5f) - up * (size.y * 0.5f);
    glm::vec3 p1 = position + right * (size.x * 0.5f) - up * (size.y * 0.5f);
    glm::vec3 p2 = position + right * (size.x * 0.5f) + up * (size.y * 0.5f);
    glm::vec3 p3 = position - right * (size.x * 0.5f) + up * (size.y * 0.5f);

    glm::vec3 norm = glm::normalize(glm::cross(right, up));

    Vertex3D quad[6] = {
        { p0, norm, {u0, v1}, color },
        { p1, norm, {u1, v1}, color },
        { p2, norm, {u1, v0}, color },

        { p0, norm, {u0, v1}, color },
        { p2, norm, {u1, v0}, color },
        { p3, norm, {u0, v0}, color }
    };

    if (soft_backend()) { soft_draw_billboard(texture_id, quad); return; }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader->bind();
    m_shader->set_vec4("u_color", glm::vec4(1.0f));
    m_shader->set_mat4("u_model", glm::mat4(1.0f));
    m_shader->set_mat4("u_view", m_view);
    m_shader->set_mat4("u_proj", m_proj);
    m_shader->set_int("u_jitter_enabled", 0);
    m_shader->set_float("u_affine_blend", 0.0f);
    m_shader->set_int("u_shading_mode", 2); // Unlit
    m_shader->set_int("u_fog_enabled", m_retro.fog_enabled ? 1 : 0);
    m_shader->set_float("u_fog_start", m_retro.fog_start);
    m_shader->set_float("u_fog_end", m_retro.fog_end);
    m_shader->set_vec3("u_fog_color", m_retro.fog_color);

    m_shader->set_int("u_texture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id != 0 ? texture_id : m_white_texture->get_id());

    glBindVertexArray(m_dyn_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_dyn_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_shader->unbind();
    glDisable(GL_BLEND);
}

void MeshRenderer3D::draw_axes_3d(const glm::vec3& pos, float size) {
    draw_line_3d(pos, pos + glm::vec3(size, 0.0f, 0.0f), glm::vec4(1.0f, 0.15f, 0.15f, 1.0f));
    draw_line_3d(pos, pos + glm::vec3(0.0f, size, 0.0f), glm::vec4(0.15f, 1.0f, 0.15f, 1.0f));
    draw_line_3d(pos, pos + glm::vec3(0.0f, 0.0f, size), glm::vec4(0.2f, 0.4f, 1.0f, 1.0f));
}

void MeshRenderer3D::draw_cube_wires(const glm::vec3& pos, const glm::vec3& size, const glm::vec4& color, const glm::vec3& rot) {
    push_matrix();
    translate(pos);
    if (rot.z != 0.0f) rotate(rot.z, glm::vec3(0, 0, 1));
    if (rot.y != 0.0f) rotate(rot.y, glm::vec3(0, 1, 0));
    if (rot.x != 0.0f) rotate(rot.x, glm::vec3(1, 0, 0));
    scale(size);

    glm::vec3 c[8] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
    };

    // Bottom square
    draw_line_3d(c[0], c[1], color);
    draw_line_3d(c[1], c[2], color);
    draw_line_3d(c[2], c[3], color);
    draw_line_3d(c[3], c[0], color);

    // Top square
    draw_line_3d(c[4], c[5], color);
    draw_line_3d(c[5], c[6], color);
    draw_line_3d(c[6], c[7], color);
    draw_line_3d(c[7], c[4], color);

    // Vertical pillars
    draw_line_3d(c[0], c[4], color);
    draw_line_3d(c[1], c[5], color);
    draw_line_3d(c[2], c[6], color);
    draw_line_3d(c[3], c[7], color);

    pop_matrix();
}

void MeshRenderer3D::draw_capsule_wires(const glm::vec3& pos, float radius, float half_height, const glm::vec4& color, const glm::vec3& rot) {
    push_matrix();
    translate(pos);
    if (rot.z != 0.0f) rotate(rot.z, glm::vec3(0, 0, 1));
    if (rot.y != 0.0f) rotate(rot.y, glm::vec3(0, 1, 0));
    if (rot.x != 0.0f) rotate(rot.x, glm::vec3(1, 0, 0));

    const int segments = 16;
    const float step = 6.2831853f / segments;

    // Top and bottom cap centers
    float top_y = half_height;
    float bot_y = -half_height;

    // 1. Top and bottom horizontal circles
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        float x1 = radius * std::cos(a1), z1 = radius * std::sin(a1);
        float x2 = radius * std::cos(a2), z2 = radius * std::sin(a2);

        draw_line_3d(glm::vec3(x1, top_y, z1), glm::vec3(x2, top_y, z2), color);
        draw_line_3d(glm::vec3(x1, bot_y, z1), glm::vec3(x2, bot_y, z2), color);
    }

    // 2. 4 Vertical cylinder lines connecting top and bottom circles
    draw_line_3d(glm::vec3( radius, top_y, 0.0f), glm::vec3( radius, bot_y, 0.0f), color);
    draw_line_3d(glm::vec3(-radius, top_y, 0.0f), glm::vec3(-radius, bot_y, 0.0f), color);
    draw_line_3d(glm::vec3(0.0f, top_y,  radius), glm::vec3(0.0f, bot_y,  radius), color);
    draw_line_3d(glm::vec3(0.0f, top_y, -radius), glm::vec3(0.0f, bot_y, -radius), color);

    // 3. Top and bottom dome semicircles (in XY and ZY planes)
    const int half_segs = segments / 2;
    const float half_step = 3.14159265f / half_segs;
    for (int i = 0; i < half_segs; ++i) {
        float a1 = i * half_step;
        float a2 = (i + 1) * half_step;

        // Top dome in XY plane (0 to pi)
        float tx1 = radius * std::cos(a1), ty1 = top_y + radius * std::sin(a1);
        float tx2 = radius * std::cos(a2), ty2 = top_y + radius * std::sin(a2);
        draw_line_3d(glm::vec3(tx1, ty1, 0.0f), glm::vec3(tx2, ty2, 0.0f), color);

        // Top dome in ZY plane (0 to pi)
        float tz1 = radius * std::cos(a1);
        float tz2 = radius * std::cos(a2);
        draw_line_3d(glm::vec3(0.0f, ty1, tz1), glm::vec3(0.0f, ty2, tz2), color);

        // Bottom dome in XY plane (pi to 2pi -> -sin)
        float bx1 = radius * std::cos(a1), by1 = bot_y - radius * std::sin(a1);
        float bx2 = radius * std::cos(a2), by2 = bot_y - radius * std::sin(a2);
        draw_line_3d(glm::vec3(bx1, by1, 0.0f), glm::vec3(bx2, by2, 0.0f), color);

        // Bottom dome in ZY plane
        float bz1 = radius * std::cos(a1);
        float bz2 = radius * std::cos(a2);
        draw_line_3d(glm::vec3(0.0f, by1, bz1), glm::vec3(0.0f, by2, bz2), color);
    }

    pop_matrix();
}

void MeshRenderer3D::draw_cylinder_wires(const glm::vec3& pos, float radius, float half_height, const glm::vec4& color, const glm::vec3& rot) {
    push_matrix();
    translate(pos);
    if (rot.z != 0.0f) rotate(rot.z, glm::vec3(0, 0, 1));
    if (rot.y != 0.0f) rotate(rot.y, glm::vec3(0, 1, 0));
    if (rot.x != 0.0f) rotate(rot.x, glm::vec3(1, 0, 0));

    const int segments = 16;
    const float step = 6.2831853f / segments;
    float top_y = half_height;
    float bot_y = -half_height;

    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;
        float x1 = radius * std::cos(a1), z1 = radius * std::sin(a1);
        float x2 = radius * std::cos(a2), z2 = radius * std::sin(a2);

        draw_line_3d(glm::vec3(x1, top_y, z1), glm::vec3(x2, top_y, z2), color);
        draw_line_3d(glm::vec3(x1, bot_y, z1), glm::vec3(x2, bot_y, z2), color);
    }

    draw_line_3d(glm::vec3( radius, top_y, 0.0f), glm::vec3( radius, bot_y, 0.0f), color);
    draw_line_3d(glm::vec3(-radius, top_y, 0.0f), glm::vec3(-radius, bot_y, 0.0f), color);
    draw_line_3d(glm::vec3(0.0f, top_y,  radius), glm::vec3(0.0f, bot_y,  radius), color);
    draw_line_3d(glm::vec3(0.0f, top_y, -radius), glm::vec3(0.0f, bot_y, -radius), color);

    pop_matrix();
}

void MeshRenderer3D::draw_ray_3d(const glm::vec3& start, const glm::vec3& dir, float length, const glm::vec4& color) {
    glm::vec3 d = dir;
    if (glm::length(d) > 0.0001f) d = glm::normalize(d);
    else d = glm::vec3(0, 1, 0);

    glm::vec3 end = start + d * length;
    draw_line_3d(start, end, color);

    // Endpoint small cross
    float s = length * 0.04f;
    if (s > 0.1f) s = 0.1f;
    draw_line_3d(end - glm::vec3(s, 0, 0), end + glm::vec3(s, 0, 0), color);
    draw_line_3d(end - glm::vec3(0, s, 0), end + glm::vec3(0, s, 0), color);
    draw_line_3d(end - glm::vec3(0, 0, s), end + glm::vec3(0, 0, s), color);
}

void MeshRenderer3D::draw_skeleton_3d(const std::vector<glm::vec3>& joint_positions, const std::vector<std::pair<int, int>>& connections, const glm::vec4& color) {
    for (const auto& conn : connections) {
        if (conn.first >= 0 && conn.first < (int)joint_positions.size() &&
            conn.second >= 0 && conn.second < (int)joint_positions.size()) {
            draw_line_3d(joint_positions[conn.first], joint_positions[conn.second], color);
        }
    }
    // Small joint dots/axes
    for (const auto& pos : joint_positions) {
        draw_axes_3d(pos, 0.08f);
    }
}

void MeshRenderer3D::draw_segmented_mesh(const std::vector<std::shared_ptr<Mesh3D>>& meshes, const std::vector<glm::mat4>& transforms, const std::vector<GLuint>& textures) {
    size_t count = std::min(meshes.size(), transforms.size());
    for (size_t i = 0; i < count; ++i) {
        if (!meshes[i]) continue;
        GLuint tex = (i < textures.size()) ? textures[i] : 0;
        draw_mesh(*meshes[i], transforms[i], tex);
    }
}

bool MeshRenderer3D::project(const glm::vec3& world_pos, float view_w, float view_h, glm::vec2& out_screen_pos) const {
    glm::vec4 clip = m_proj * m_view * glm::vec4(world_pos, 1.0f);
    if (clip.w <= 0.0001f) {
        return false;
    }
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    out_screen_pos.x = (ndc.x * 0.5f + 0.5f) * view_w;
    out_screen_pos.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * view_h;
    return (ndc.z >= -1.0f && ndc.z <= 1.0f && ndc.x >= -1.2f && ndc.x <= 1.2f && ndc.y >= -1.2f && ndc.y <= 1.2f);
}

void MeshRenderer3D::unproject(const glm::vec2& screen_pos, float view_w, float view_h, glm::vec3& out_ray_orig, glm::vec3& out_ray_dir) const {
    float ndc_x = (screen_pos.x / view_w) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (screen_pos.y / view_h) * 2.0f;

    glm::mat4 inv_pv = glm::inverse(m_proj * m_view);
    glm::vec4 near_pt = inv_pv * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
    glm::vec4 far_pt  = inv_pv * glm::vec4(ndc_x, ndc_y,  1.0f, 1.0f);

    if (std::abs(near_pt.w) > 0.00001f) near_pt /= near_pt.w;
    if (std::abs(far_pt.w) > 0.00001f)  far_pt  /= far_pt.w;

    out_ray_orig = glm::vec3(near_pt);
    glm::vec3 diff = glm::vec3(far_pt - near_pt);
    float len = glm::length(diff);
    out_ray_dir = (len > 0.00001f) ? (diff / len) : glm::vec3(0, 0, 1);
}

GLuint MeshRenderer3D::get_fallback_texture_id() const {
    return m_checker_texture ? m_checker_texture->get_id() : 0;
}

} // namespace crayon
