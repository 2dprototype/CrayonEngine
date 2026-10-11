// Software (CPU) backend for MeshRenderer3D. Everything here only runs when
// crayon::soft_backend() is true; the GL path never touches it.
#include "mesh3d.hpp"
#include "../core/log.hpp"

#include <algorithm>
#include <cmath>

namespace crayon {

static_assert(sizeof(Vertex3D) == sizeof(soft::Vertex), "soft::Vertex must match Vertex3D");
static_assert(offsetof(Vertex3D, uv) == offsetof(soft::Vertex, uv), "soft::Vertex layout drift");
static_assert(offsetof(Vertex3D, color) == offsetof(soft::Vertex, color), "soft::Vertex layout drift");

// Copies camera-independent state (retro look, lights, fog, fixed-function flags) into the
// rasterizer. Cheap (a few dozen floats), so it simply runs before every draw instead of
// tracking dirtiness.
void MeshRenderer3D::soft_sync() {
    soft::Settings& S = soft::framebuffer().settings;
    S.jitter = m_retro.jitter_enabled;
    S.jitter_res = m_retro.jitter_resolution;
    S.affine = m_retro.affine_blend >= 0.5f;
    S.fog = m_retro.fog_enabled;
    S.fog_start = m_retro.fog_start;
    S.fog_end = m_retro.fog_end;
    S.fog_color = m_retro.fog_color;
    S.dither = false; // dither / CRT / vignette run as one full-screen pass at present time
    S.shading = static_cast<soft::Shading>(static_cast<int>(m_shading_mode));
    S.cull = static_cast<soft::Cull>(m_cull_mode);
    S.depth_test = m_depth_test;
    S.depth_write = m_depth_write;
    S.blend = false;
    S.light_dir = m_light_dir;
    S.light_color = m_light_color;
    S.ambient = m_ambient_color;
    for (int i = 0; i < 4; ++i) {
        S.points[i].pos = m_point_lights[i].pos;
        S.points[i].color = m_point_lights[i].color;
        S.points[i].radius = m_point_lights[i].radius;
        S.points[i].intensity = m_point_lights[i].intensity;
        S.points[i].enabled = m_point_lights[i].enabled;
    }
    for (int i = 0; i < 2; ++i) {
        S.spots[i].pos = m_spot_lights[i].pos;
        S.spots[i].dir = m_spot_lights[i].dir;
        S.spots[i].color = m_spot_lights[i].color;
        S.spots[i].radius = m_spot_lights[i].radius;
        S.spots[i].intensity = m_spot_lights[i].intensity;
        S.spots[i].inner_cos = m_spot_lights[i].inner_cutoff_cos;
        S.spots[i].outer_cos = m_spot_lights[i].outer_cutoff_cos;
        S.spots[i].enabled = m_spot_lights[i].enabled;
    }
}

void MeshRenderer3D::soft_draw_geometry(const Vertex3D* v, size_t nv, const GLuint* idx, size_t ni,
                                        const glm::mat4& model, GLuint tex, const glm::vec4& tint) {
    const soft::Texture* t = soft::TextureRegistry::get(tex != 0 ? tex : (m_checker_texture ? m_checker_texture->get_id() : 0));
    soft::framebuffer().draw(reinterpret_cast<const soft::Vertex*>(v), nv, idx, ni, model, t, tint);
}

void MeshRenderer3D::soft_draw_mesh(const Mesh3D& mesh, const glm::mat4& model, GLuint texture_id, const glm::vec4& tint) {
    soft_sync();
    const auto& V = mesh.cpu_vertices();
    const auto& I = mesh.cpu_indices();
    if (V.empty()) return;

    if (mesh.has_submeshes()) {
        for (const SubMesh& sm : mesh.get_submeshes()) {
            if (size_t(sm.index_offset) + sm.index_count > I.size()) continue;
            GLuint t = sm.texture_id != 0 ? sm.texture_id : texture_id; // same rule as Mesh3D::draw()
            soft_draw_geometry(V.data(), V.size(), I.data() + sm.index_offset, sm.index_count, model, t, tint);
        }
    } else if (!I.empty()) {
        soft_draw_geometry(V.data(), V.size(), I.data(), I.size(), model, texture_id, tint);
    } else {
        m_soft_index_scratch.resize(V.size());
        for (size_t i = 0; i < V.size(); ++i) m_soft_index_scratch[i] = static_cast<GLuint>(i);
        soft_draw_geometry(V.data(), V.size(), m_soft_index_scratch.data(), V.size(), model, texture_id, tint);
    }
}

// CPU skinning: the GL path does this in the vertex shader.
void MeshRenderer3D::soft_draw_mesh_skinned(const Mesh3D& mesh, const glm::mat4& model, const glm::mat4* bones,
                                            size_t bone_count, GLuint texture_id, const glm::vec4& tint) {
    soft_sync();
    const auto& SV = mesh.cpu_skinned_vertices();
    const auto& I = mesh.cpu_indices();
    if (SV.empty()) {
        soft_draw_mesh(mesh, model, texture_id, tint);
        return;
    }
    m_soft_scratch.resize(SV.size());
    const bool skin = bones && bone_count > 0;
    for (size_t i = 0; i < SV.size(); ++i) {
        const SkinnedVertex3D& s = SV[i];
        Vertex3D& o = m_soft_scratch[i];
        o.uv = s.uv;
        o.color = s.color;
        if (!skin) { o.position = s.position; o.normal = s.normal; continue; }
        glm::mat4 m(0.0f);
        float wsum = 0.0f;
        for (int k = 0; k < 4; ++k) {
            float w = s.weights[k];
            if (w <= 0.0f || s.joints[k] >= bone_count) continue;
            m += bones[s.joints[k]] * w;
            wsum += w;
        }
        if (wsum <= 0.0f) { o.position = s.position; o.normal = s.normal; continue; }
        o.position = glm::vec3(m * glm::vec4(s.position, 1.0f));
        o.normal = glm::mat3(m) * s.normal;
    }
    if (mesh.has_submeshes()) {
        for (const SubMesh& sm : mesh.get_submeshes()) {
            if (size_t(sm.index_offset) + sm.index_count > I.size()) continue;
            GLuint t = sm.texture_id != 0 ? sm.texture_id : texture_id;
            soft_draw_geometry(m_soft_scratch.data(), m_soft_scratch.size(), I.data() + sm.index_offset, sm.index_count, model, t, tint);
        }
    } else {
        soft_draw_geometry(m_soft_scratch.data(), m_soft_scratch.size(), I.data(), I.size(), model, texture_id, tint);
    }
}

// Billboards: unlit, alpha-blended, no vertex wobble / warp (matches the GL billboard path).
void MeshRenderer3D::soft_draw_billboard(GLuint tex, const Vertex3D quad[6]) {
    soft_sync();
    soft::Settings& S = soft::framebuffer().settings;
    const soft::Settings saved = S;
    S.shading = soft::Shading::Unlit;
    S.jitter = false;
    S.affine = false;
    S.blend = true;
    S.cull = soft::Cull::None;
    static const GLuint idx[6] = {0, 1, 2, 3, 4, 5};
    const soft::Texture* t = soft::TextureRegistry::get(tex != 0 ? tex : (m_white_texture ? m_white_texture->get_id() : 0));
    soft::framebuffer().draw(reinterpret_cast<const soft::Vertex*>(quad), 6, idx, 6, glm::mat4(1.0f), t, glm::vec4(1.0f));
    S = saved;
}

void MeshRenderer3D::soft_draw_lines(const Vertex3D* v, size_t count, const glm::mat4& model) {
    soft_sync();
    auto& fb = soft::framebuffer();
    for (size_t i = 0; i + 1 < count; i += 2) {
        glm::vec3 a = glm::vec3(model * glm::vec4(v[i].position, 1.0f));
        glm::vec3 b = glm::vec3(model * glm::vec4(v[i + 1].position, 1.0f));
        fb.draw_line(a, b, v[i].color);
    }
}

void MeshRenderer3D::soft_draw_triangle(const Vertex3D tri[3], GLuint tex) {
    soft_sync();
    static const GLuint idx[3] = {0, 1, 2};
    const soft::Texture* t = soft::TextureRegistry::get(tex != 0 ? tex : (m_white_texture ? m_white_texture->get_id() : 0));
    soft::framebuffer().draw(reinterpret_cast<const soft::Vertex*>(tri), 3, idx, 3, m_current_matrix, t, glm::vec4(1.0f));
}

void MeshRenderer3D::soft_draw_grid(float size, int divisions, float y, const glm::vec4& color) {
    soft_sync();
    auto& fb = soft::framebuffer();
    float half = size * 0.5f, step = size / divisions;
    for (int i = 0; i <= divisions; ++i) {
        float c = -half + i * step;
        glm::vec3 a0 = glm::vec3(m_current_matrix * glm::vec4(c, y, -half, 1.0f));
        glm::vec3 a1 = glm::vec3(m_current_matrix * glm::vec4(c, y, half, 1.0f));
        glm::vec3 b0 = glm::vec3(m_current_matrix * glm::vec4(-half, y, c, 1.0f));
        glm::vec3 b1 = glm::vec3(m_current_matrix * glm::vec4(half, y, c, 1.0f));
        fb.draw_line(a0, a1, color);
        fb.draw_line(b0, b1, color);
    }
}

} // namespace crayon
