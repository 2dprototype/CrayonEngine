#pragma once
// Heightmap terrain: built from a grid of heights, split into chunk meshes (each frustum / distance
// culled on its own), with height / normal queries for gameplay (ground following, placement).
//
//   world x runs along columns, world z along rows; the terrain occupies
//   [origin.x, origin.x + size.x] x [origin.z, origin.z + size.z], y = height.
#include "image_data.hpp"
#include "mesh3d.hpp"
#include <memory>
#include <vector>

namespace crayon {

class Terrain {
public:
    struct Options {
        glm::vec3 size{100.0f, 10.0f, 100.0f};   // world width (x), height range (y), depth (z)
        glm::vec3 origin{0.0f};                   // world position of the (0,0) corner
        float uv_repeat_x = 1.0f, uv_repeat_z = 1.0f;   // texture tiles across the whole terrain
        int chunk_cells = 32;                     // cells per chunk side
        bool smooth_normals = true;
        // optional vertex colour ramp by normalised height (low -> high); alpha 0 disables
        glm::vec4 color_low{1, 1, 1, 1}, color_high{1, 1, 1, 1};
    };

    // heights: columns * rows values; normalised 0..1 (multiplied by size.y), row-major (row 0 = z 0).
    bool build(const std::vector<float>& heights, int columns, int rows, const Options& opts);
    // From an image: brightness (red channel) = height.
    bool build_from_image(const ImageData& img, const Options& opts);

    // World-space queries (return false / 0 outside the terrain).
    bool contains(float x, float z) const;
    float height_at(float x, float z) const;              // bilinear on the same triangulation as the mesh
    glm::vec3 normal_at(float x, float z) const;
    // Ray vs terrain (coarse step + bisection). Returns false when it misses.
    bool raycast(const glm::vec3& origin, const glm::vec3& dir, float max_dist, glm::vec3& hit) const;

    void draw(MeshRenderer3D& renderer, GLuint texture = 0, const glm::vec4& tint = glm::vec4(1.0f)) const;

    int columns() const { return m_cols; }
    int rows() const { return m_rows; }
    size_t chunk_count() const { return m_chunks.size(); }
    size_t triangle_count() const { return m_triangles; }
    const Options& options() const { return m_opt; }
    // Normalised heights (0..1), columns*rows, row-major: what the physics heightfield is built from.
    const std::vector<float>& heights() const { return m_h; }

private:
    float cell_height(int cx, int cz) const { return m_h[size_t(cz) * m_cols + cx] * m_opt.size.y + m_opt.origin.y; }
    glm::vec3 vertex_normal(int cx, int cz) const;

    int m_cols = 0, m_rows = 0;
    std::vector<float> m_h;
    Options m_opt;
    float m_dx = 1, m_dz = 1;
    size_t m_triangles = 0;
    std::vector<std::shared_ptr<Mesh3D>> m_chunks;
};

} // namespace crayon
