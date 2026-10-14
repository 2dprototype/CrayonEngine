#include "static_batch3d.hpp"
#include <algorithm>
#include <cmath>

namespace crayon {

static constexpr size_t kMaxVerticesPerGroup = 200000;

size_t StaticBatch3D::add(const Model3D& model, const glm::mat4& transform, GLuint tex_override) {
    size_t queued = 0;
    for (const ModelPart& part : model.get_parts()) {
        if (part.is_skinned || part.cpu_vertices.empty() || part.cpu_indices.empty()) continue;

        const glm::mat4 m = transform * part.transform;
        const glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(m)));
        const GLuint tex = tex_override != 0 ? tex_override : part.texture_id;
        const glm::vec3 origin = glm::vec3(m[3]);
        Key key{int(std::floor(origin.x / m_chunk)), int(std::floor(origin.y / m_chunk)),
                int(std::floor(origin.z / m_chunk)), tex, part.double_sided, 0};

        // find the group for this key, rolling over to a new one when it gets too big
        size_t slot;
        for (;;) {
            auto it = m_index.find(key);
            if (it == m_index.end()) {
                slot = m_groups.size();
                m_groups.emplace_back();
                m_groups.back().texture = tex;
                m_groups.back().double_sided = part.double_sided;
                m_index[key] = slot;
                break;
            }
            slot = it->second;
            if (m_groups[slot].vertices.size() + part.cpu_vertices.size() <= kMaxVerticesPerGroup) break;
            ++key.part;
        }

        Group& g = m_groups[slot];
        const GLuint base = static_cast<GLuint>(g.vertices.size());
        g.vertices.reserve(g.vertices.size() + part.cpu_vertices.size());
        for (const Vertex3D& v : part.cpu_vertices) {
            Vertex3D o = v;
            o.position = glm::vec3(m * glm::vec4(v.position, 1.0f));
            glm::vec3 n = nm * v.normal;
            const float len = glm::length(n);
            o.normal = len > 1e-8f ? n / len : v.normal;
            o.color = v.color * part.color;   // part tint baked into vertex colour
            g.vertices.push_back(o);
        }
        g.indices.reserve(g.indices.size() + part.cpu_indices.size());
        for (GLuint i : part.cpu_indices) g.indices.push_back(base + i);
        ++queued;
    }
    if (queued) { ++m_stats.instances; m_dirty = true; }
    return queued;
}

void StaticBatch3D::build() {
    if (!m_dirty) return;
    m_stats.groups = m_groups.size();
    m_stats.vertices = m_stats.triangles = 0;
    for (Group& g : m_groups) {
        g.mesh = std::make_shared<Mesh3D>();
        g.mesh->create_from_data(g.vertices, g.indices);
        m_stats.vertices += g.vertices.size();
        m_stats.triangles += g.indices.size() / 3;
    }
    m_dirty = false;
}

void StaticBatch3D::draw(MeshRenderer3D& renderer) {
    build();
    const glm::mat4 identity(1.0f);
    for (const Group& g : m_groups) {
        if (!g.mesh) continue;
        // bounds come from the merged mesh itself -> per-chunk frustum + distance culling
        renderer.draw_mesh(*g.mesh, identity, g.texture, glm::vec4(1.0f), nullptr, nullptr, !g.double_sided);
    }
}

void StaticBatch3D::clear() {
    m_groups.clear();
    m_index.clear();
    m_stats = Stats{};
    m_dirty = false;
}

} // namespace crayon
