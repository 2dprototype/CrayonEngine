#pragma once
// Static batching + spatial culling for large scenes (forests, cities, tile-based levels).
//
// Many placed instances of models are merged at build() time into a handful of big meshes,
// grouped by (spatial chunk, texture, material flags). At draw time each chunk mesh is frustum /
// distance culled individually (using its real bounds), so a 10,000-tree forest costs a few
// draw calls and only the chunks you can see are submitted. Works identically on the GPU and CPU
// backends (no GPU instancing required). Instances are baked: move/animate = rebuild.
#include "mesh3d.hpp"
#include "model3d.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace crayon {

class StaticBatch3D {
public:
    struct Stats {
        size_t instances = 0;
        size_t groups = 0;        // merged meshes (upper bound of draw calls per frame)
        size_t vertices = 0;
        size_t triangles = 0;
    };

    explicit StaticBatch3D(float chunk_size = 32.0f) : m_chunk(chunk_size > 0.001f ? chunk_size : 32.0f) {}

    // Queue an instance. Skinned parts are skipped (they cannot be baked). `tex_override` replaces
    // every part's texture when non-zero (same meaning as drawModel's texture argument).
    // Returns the number of parts queued.
    size_t add(const Model3D& model, const glm::mat4& transform, GLuint tex_override = 0);

    // Merges everything queued so far (idempotent). Called automatically by draw().
    void build();
    void draw(MeshRenderer3D& renderer);
    void clear();

    const Stats& stats() const { return m_stats; }
    bool is_built() const { return !m_dirty; }

private:
    struct Group {
        std::vector<Vertex3D> vertices;
        std::vector<GLuint> indices;
        GLuint texture = 0;
        bool double_sided = true;
        std::shared_ptr<Mesh3D> mesh;   // filled by build()
    };
    struct Key {
        int cx, cy, cz;
        GLuint tex;
        bool double_sided;
        int part;                         // overflow split index
        bool operator==(const Key& o) const {
            return cx == o.cx && cy == o.cy && cz == o.cz && tex == o.tex && double_sided == o.double_sided && part == o.part;
        }
    };
    struct KeyHash {
        size_t operator()(const Key& k) const {
            size_t h = size_t(k.cx) * 73856093u ^ size_t(k.cy) * 19349663u ^ size_t(k.cz) * 83492791u;
            return h ^ (size_t(k.tex) * 2654435761u) ^ (size_t(k.double_sided) << 3) ^ (size_t(k.part) << 17);
        }
    };

    float m_chunk;
    bool m_dirty = false;
    Stats m_stats;
    std::unordered_map<Key, size_t, KeyHash> m_index;   // key -> groups[] slot
    std::vector<Group> m_groups;
};

} // namespace crayon
