#include "terrain.hpp"
#include <algorithm>
#include <cmath>

namespace crayon {

bool Terrain::build_from_image(const ImageData& img, const Options& opts) {
    std::vector<float> h(size_t(img.width()) * img.height());
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) h[size_t(y) * img.width() + x] = img.data()[(size_t(y) * img.width() + x) * 4] / 255.0f;
    return build(h, img.width(), img.height(), opts);
}

glm::vec3 Terrain::vertex_normal(int cx, int cz) const {
    const int x0 = std::max(cx - 1, 0), x1 = std::min(cx + 1, m_cols - 1);
    const int z0 = std::max(cz - 1, 0), z1 = std::min(cz + 1, m_rows - 1);
    const float dhdx = (cell_height(x1, cz) - cell_height(x0, cz)) / (float(x1 - x0) * m_dx);
    const float dhdz = (cell_height(cx, z1) - cell_height(cx, z0)) / (float(z1 - z0) * m_dz);
    return glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
}

bool Terrain::build(const std::vector<float>& heights, int columns, int rows, const Options& opts) {
    if (columns < 2 || rows < 2 || heights.size() != size_t(columns) * rows) return false;
    m_cols = columns; m_rows = rows; m_h = heights; m_opt = opts;
    m_opt.chunk_cells = std::max(2, opts.chunk_cells);
    m_dx = opts.size.x / float(columns - 1);
    m_dz = opts.size.z / float(rows - 1);
    m_chunks.clear();
    m_triangles = 0;

    const int cells_x = columns - 1, cells_z = rows - 1;
    const bool ramp = opts.color_low.a > 0.0f && opts.color_high.a > 0.0f;
    for (int cz0 = 0; cz0 < cells_z; cz0 += m_opt.chunk_cells) {
        for (int cx0 = 0; cx0 < cells_x; cx0 += m_opt.chunk_cells) {
            const int cx1 = std::min(cx0 + m_opt.chunk_cells, cells_x), cz1 = std::min(cz0 + m_opt.chunk_cells, cells_z);
            const int nx = cx1 - cx0 + 1, nz = cz1 - cz0 + 1;
            std::vector<Vertex3D> verts;
            std::vector<GLuint> idx;
            verts.reserve(size_t(nx) * nz);
            for (int cz = cz0; cz <= cz1; ++cz) {
                for (int cx = cx0; cx <= cx1; ++cx) {
                    Vertex3D v{};
                    const float hn = m_h[size_t(cz) * m_cols + cx];
                    v.position = {m_opt.origin.x + cx * m_dx, cell_height(cx, cz), m_opt.origin.z + cz * m_dz};
                    v.normal = vertex_normal(cx, cz);
                    v.uv = {float(cx) / cells_x * opts.uv_repeat_x, float(cz) / cells_z * opts.uv_repeat_z};
                    v.color = ramp ? glm::mix(opts.color_low, opts.color_high, std::clamp(hn, 0.0f, 1.0f)) : glm::vec4(1.0f);
                    verts.push_back(v);
                }
            }
            for (int z = 0; z < nz - 1; ++z) {
                for (int x = 0; x < nx - 1; ++x) {
                    const GLuint a = GLuint(z * nx + x), b = a + 1, c = a + nx, d = c + 1;
                    // CCW seen from above (+Y), diagonal a-d (matches height_at's triangulation)
                    idx.insert(idx.end(), {a, c, d, a, d, b});
                }
            }
            auto mesh = std::make_shared<Mesh3D>();
            mesh->create_from_data(verts, idx);
            m_triangles += idx.size() / 3;
            m_chunks.push_back(std::move(mesh));
        }
    }
    return true;
}

bool Terrain::contains(float x, float z) const {
    return m_cols > 0 && x >= m_opt.origin.x && z >= m_opt.origin.z &&
           x <= m_opt.origin.x + m_opt.size.x && z <= m_opt.origin.z + m_opt.size.z;
}

float Terrain::height_at(float x, float z) const {
    if (!contains(x, z)) return m_opt.origin.y;
    const float fx = (x - m_opt.origin.x) / m_dx, fz = (z - m_opt.origin.z) / m_dz;
    int cx = std::min(int(fx), m_cols - 2), cz = std::min(int(fz), m_rows - 2);
    const float u = fx - cx, v = fz - cz;
    const float ha = cell_height(cx, cz), hb = cell_height(cx + 1, cz);
    const float hc = cell_height(cx, cz + 1), hd = cell_height(cx + 1, cz + 1);
    // triangles (a,c,d) and (a,d,b) share the a-d diagonal (u == v)
    if (u <= v) return ha + (hd - hc) * u + (hc - ha) * v;       // lower-left triangle a,c,d
    return ha + (hb - ha) * u + (hd - hb) * v;                    // upper-right triangle a,d,b
}

glm::vec3 Terrain::normal_at(float x, float z) const {
    const float e = std::min(m_dx, m_dz) * 0.5f;
    const float dhdx = (height_at(x + e, z) - height_at(x - e, z)) / (2 * e);
    const float dhdz = (height_at(x, z + e) - height_at(x, z - e)) / (2 * e);
    return glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
}

bool Terrain::raycast(const glm::vec3& o, const glm::vec3& dir, float max_dist, glm::vec3& hit) const {
    if (m_cols == 0) return false;
    glm::vec3 d = glm::normalize(dir);
    const float step = std::min(m_dx, m_dz) * 0.5f;
    float prev_t = 0.0f;
    bool prev_above = o.y >= height_at(o.x, o.z) || !contains(o.x, o.z);
    for (float t = step; t <= max_dist; t += step) {
        glm::vec3 p = o + d * t;
        if (!contains(p.x, p.z)) { prev_t = t; prev_above = true; continue; }
        const bool above = p.y >= height_at(p.x, p.z);
        if (prev_above && !above) {
            float lo = prev_t, hi = t;
            for (int i = 0; i < 24; ++i) {
                float mid = 0.5f * (lo + hi);
                glm::vec3 q = o + d * mid;
                if (contains(q.x, q.z) && q.y < height_at(q.x, q.z)) hi = mid; else lo = mid;
            }
            hit = o + d * (0.5f * (lo + hi));
            hit.y = height_at(hit.x, hit.z);
            return true;
        }
        prev_t = t; prev_above = above;
    }
    return false;
}

void Terrain::draw(MeshRenderer3D& renderer, GLuint texture, const glm::vec4& tint) const {
    const glm::mat4 identity(1.0f);
    for (const auto& c : m_chunks) renderer.draw_mesh(*c, identity, texture, tint);   // per-chunk culling via mesh bounds
}

} // namespace crayon
