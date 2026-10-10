#include "hv4d/mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>

#include "hv4d/todd_coxeter.hpp"

namespace hv4d {

namespace {

void get_mirror_normals(const int symbol[3], Vec4 mn[4]) {
    for (int i = 0; i < 4; ++i) mn[i] = Vec4::zero();

    mn[0] = Vec4::unit_x();

    // dot(N_0, N_1) = cos(pi / symbol[0])
    mn[1].x = std::cos(PI / static_cast<float>(symbol[0]));
    mn[1].y = std::sqrt(1.0f - mn[1].x * mn[1].x);

    // dot(N_0, N_2) = cos(pi / 2) = 0 ;  dot(N_1, N_2) = cos(pi / symbol[1])
    mn[2].y = std::cos(PI / static_cast<float>(symbol[1])) / mn[1].y;
    mn[2].z = std::sqrt(1.0f - mn[2].y * mn[2].y);

    // dot(N_0, N_3) = 0 ; dot(N_1, N_3) = 0 ; dot(N_2, N_3) = cos(pi / symbol[2])
    mn[3].z = std::cos(PI / static_cast<float>(symbol[2])) / mn[2].z;
    mn[3].w = std::sqrt(1.0f - mn[3].z * mn[3].z);
}

std::vector<int> repeat2(int a, int b, int n) {
    std::vector<int> v;
    v.reserve(2 * n);
    for (int i = 0; i < n; ++i) {
        v.push_back(a);
        v.push_back(b);
    }
    return v;
}

}  // namespace

void Mesh::schlafli_of(RegularSolid solid, int out[3]) {
    switch (solid) {
        case RegularSolid::FiveCell:       out[0] = 3; out[1] = 3; out[2] = 3; break;
        case RegularSolid::EightCell:      out[0] = 4; out[1] = 3; out[2] = 3; break;
        case RegularSolid::SixteenCell:    out[0] = 3; out[1] = 3; out[2] = 4; break;
        case RegularSolid::TwentyFourCell: out[0] = 3; out[1] = 4; out[2] = 3; break;
        case RegularSolid::OneTwentyCell:  out[0] = 5; out[1] = 3; out[2] = 3; break;
        case RegularSolid::SixHundredCell: out[0] = 3; out[1] = 3; out[2] = 5; break;
    }
}

Mesh Mesh::from_regular_solid(RegularSolid solid) {
    int s[3] = {3, 3, 3};
    schlafli_of(solid, s);
    return from_schlafli_symbol(s[0], s[1], s[2]);
}

Mesh Mesh::from_schlafli_symbol(int p, int q, int r) {
    namespace tc = todd_coxeter;
    const int symbol[3] = {p, q, r};
    Vec4 mirror_normals[4];
    get_mirror_normals(symbol, mirror_normals);

    // setup for todd-coxeter
    const int num_gens = 4;
    const std::vector<tc::Relation> relations = {
        {0, 0}, {1, 1}, {2, 2}, {3, 3},
        repeat2(0, 1, symbol[0]),
        repeat2(1, 2, symbol[1]),
        repeat2(2, 3, symbol[2]),
        repeat2(0, 2, 2),
        repeat2(0, 3, 2),
        repeat2(1, 3, 2),
    };

    const tc::Table vertex_table = tc::coset_table(num_gens, relations, {1, 2, 3});
    const tc::Table edge_table = tc::coset_table(num_gens, relations, {0, 2, 3});
    const tc::Table face_table = tc::coset_table(num_gens, relations, {0, 1, 3});
    const tc::Table cell_table = tc::coset_table(num_gens, relations, {0, 1, 2});

    // pick a v0 so that it's on planes 1, 2, and 3, but not on 0
    Vec4 v0 = Vec4::unit_x();
    v0.y = -mirror_normals[1].x * v0.x / mirror_normals[1].y;
    v0.z = -mirror_normals[2].y * v0.y / mirror_normals[2].z;
    v0.w = -mirror_normals[3].z * v0.z / mirror_normals[3].w;
    v0 = v0.normalized();

    Mesh mesh;
    mesh.radius = 1.0f;
    mesh.vertices = tc::table_bfs_fold(vertex_table, 0, v0, [&](const Vec4& v, int mirror) {
        return reflect(v, mirror_normals[mirror]);
    });

    // Edges ------------------------------------------------------------------
    Edge e0;
    e0.hd_vertex = 0;
    e0.tl_vertex = 1;
    {
        int curr_face = 0;
        for (;;) {
            e0.faces.push_back(curr_face);
            curr_face = face_table[face_table[curr_face][2]][3];
            if (curr_face == 0) break;
        }
    }
    mesh.edges = tc::table_bfs_fold(edge_table, 0, e0, [&](const Edge& e, int mirror) {
        Edge out;
        out.hd_vertex = vertex_table[e.hd_vertex][mirror];
        out.tl_vertex = vertex_table[e.tl_vertex][mirror];
        out.faces.reserve(e.faces.size());
        for (int f : e.faces) out.faces.push_back(face_table[f][mirror]);
        return out;
    });

    // Faces: the initial face is all the edges invariant under the rotation (0, 1)
    std::vector<int> f0;
    {
        int curr_edge = 0;
        for (;;) {
            f0.push_back(curr_edge);
            curr_edge = edge_table[edge_table[curr_edge][0]][1];
            if (curr_edge == 0) break;
        }
    }
    const std::vector<std::vector<int>> face_tmp =
        tc::table_bfs_fold(face_table, 0, f0, [&](const std::vector<int>& f, int mirror) {
            std::vector<int> out;
            out.reserve(f.size());
            for (int e : f) out.push_back(edge_table[e][mirror]);
            return out;
        });

    // Cells: the initial cell is invariant under mirrors 0, 1 and 2, so applying
    // those mirrors repeatedly to the initial face yields all faces of the cell.
    Cell c0;
    c0.normal = -Vec4::unit_w();  // follows from the choice of mirror normals
    c0.faces.push_back(0);
    for (size_t i = 0; i < c0.faces.size(); ++i) {
        const int f = c0.faces[i];
        for (int j = 0; j < 3; ++j) {
            const int nf = face_table[f][j];
            if (std::find(c0.faces.begin(), c0.faces.end(), nf) == c0.faces.end()) c0.faces.push_back(nf);
        }
    }
    mesh.cells = tc::table_bfs_fold(cell_table, 0, c0, [&](const Cell& cell, int mirror) {
        Cell out;
        out.normal = reflect(cell.normal, mirror_normals[mirror]);
        out.faces.reserve(cell.faces.size());
        for (int f : cell.faces) out.faces.push_back(face_table[f][mirror]);
        return out;
    });

    mesh.faces.resize(face_tmp.size());
    for (size_t i = 0; i < face_tmp.size(); ++i) mesh.faces[i].edges = face_tmp[i];

    // populate cells for each face
    for (size_t i = 0; i < mesh.cells.size(); ++i) {
        for (int j : mesh.cells[i].faces) {
            Face& f = mesh.faces[j];
            if (f.hd_cell < 0)
                f.hd_cell = static_cast<int>(i);
            else
                f.tl_cell = static_cast<int>(i);
        }
    }

    // populate cells for each vertex
    mesh.vertex_data.assign(mesh.vertices.size(), VertexData{});
    for (size_t cell_idx = 0; cell_idx < mesh.cells.size(); ++cell_idx) {
        for (int face_idx : mesh.cells[cell_idx].faces) {
            for (int edge_idx : mesh.faces[face_idx].edges) {
                const Edge& edge = mesh.edges[edge_idx];
                for (int vi : {edge.hd_vertex, edge.tl_vertex}) {
                    auto& cs = mesh.vertex_data[vi].cells;
                    if (std::find(cs.begin(), cs.end(), static_cast<int>(cell_idx)) == cs.end())
                        cs.push_back(static_cast<int>(cell_idx));
                }
            }
        }
    }

    mesh.finalize();
    return mesh;
}

Mesh Mesh::scaled(float factor) const {
    Mesh m = *this;
    for (auto& v : m.vertices) v *= factor;
    m.radius *= factor;
    m.finalize();
    return m;
}

// ---------------------------------------------------------------------------
// Derived data
// ---------------------------------------------------------------------------
void Mesh::finalize() {
    // Face vertex loops: chain the face's edges through shared vertices so the
    // result is a proper cycle regardless of per-edge orientation.
    face_loops.assign(faces.size(), {});
    for (size_t fi = 0; fi < faces.size(); ++fi) {
        const auto& fe = faces[fi].edges;
        std::vector<int>& loop = face_loops[fi];
        if (fe.empty()) continue;

        std::vector<char> used(fe.size(), 0);
        const Edge& first = edges[fe[0]];
        loop.push_back(first.hd_vertex);
        loop.push_back(first.tl_vertex);
        used[0] = 1;
        for (size_t step = 1; step < fe.size(); ++step) {
            const int last = loop.back();
            bool found = false;
            for (size_t k = 1; k < fe.size(); ++k) {
                if (used[k]) continue;
                const Edge& e = edges[fe[k]];
                int next = -1;
                if (e.hd_vertex == last) next = e.tl_vertex;
                else if (e.tl_vertex == last) next = e.hd_vertex;
                if (next < 0) continue;
                used[k] = 1;
                loop.push_back(next);
                found = true;
                break;
            }
            if (!found) break;
        }
        // The cycle closes on its first vertex; drop the duplicate.
        if (loop.size() > 1 && loop.back() == loop.front()) loop.pop_back();
    }

    // Tetrahedral decomposition: for each cell pick an apex vertex and fan-
    // triangulate every face that does not contain the apex.
    tetrahedra.clear();
    for (size_t ci = 0; ci < cells.size(); ++ci) {
        const Cell& cell = cells[ci];
        if (cell.faces.empty()) continue;
        const int apex = edges[faces[cell.faces[0]].edges[0]].hd_vertex;
        for (int fi : cell.faces) {
            const auto& loop = face_loops[fi];
            if (loop.size() < 3) continue;
            if (std::find(loop.begin(), loop.end(), apex) != loop.end()) continue;
            for (size_t i = 1; i + 1 < loop.size(); ++i) {
                Tetrahedron t;
                t.v[0] = apex;
                t.v[1] = loop[0];
                t.v[2] = loop[i];
                t.v[3] = loop[i + 1];
                t.cell = static_cast<int>(ci);
                tetrahedra.push_back(t);
            }
        }
    }

    // Volume and second moment about the origin (polytope assumed to contain it).
    //   simplex(0, a, b, c, d):  V = |det[a b c d]| / 24
    //   integral(x x^T)       = V/30 * (sum v v^T + s s^T),  s = sum v
    double total_volume = 0.0;
    double trace_S = 0.0;
    for (const Tetrahedron& t : tetrahedra) {
        const Vec4& a = vertices[t.v[0]];
        const Vec4& b = vertices[t.v[1]];
        const Vec4& c = vertices[t.v[2]];
        const Vec4& d = vertices[t.v[3]];
        const double det = std::fabs(static_cast<double>(triple_cross_product(a, b, c).dot(d)));
        const double V = det / 24.0;
        const Vec4 s = a + b + c + d;
        const double tr = a.length2() + b.length2() + c.length2() + d.length2() + s.length2();
        total_volume += V;
        trace_S += V / 30.0 * tr;
    }
    volume = static_cast<float>(total_volume);
    if (total_volume > 0.0) {
        // Average over the six coordinate planes: mean(S_ii + S_jj) = tr(S) / 2.
        inertia_per_mass = static_cast<float>(trace_S / (2.0 * total_volume));
    }
}

// ---------------------------------------------------------------------------
// Closest point queries
// ---------------------------------------------------------------------------
Vec4 Mesh::edge_representative_vertex(const Edge& edge) const { return vertices[edge.hd_vertex]; }
Vec4 Mesh::face_representative_vertex(const Face& face) const {
    return edge_representative_vertex(edges[face.edges[0]]);
}
Vec4 Mesh::cell_representative_vertex(const Cell& cell) const {
    return face_representative_vertex(faces[cell.faces[0]]);
}
Vec4 Mesh::edge_vector(int edge_idx) const {
    const Edge& e = edges[edge_idx];
    return vertices[e.tl_vertex] - vertices[e.hd_vertex];
}

bool Mesh::contains(const Vec4& point) const {
    for (const Cell& cell : cells) {
        const Vec4 v0 = cell_representative_vertex(cell);
        if (v0.dot(cell.normal) < point.dot(cell.normal)) return false;
    }
    return true;
}

Vec4 Mesh::closest_point_to(const Vec4& point) const {
    if (contains(point)) return point;

    Vec4 best = point;
    float best_d = std::numeric_limits<float>::infinity();
    for (const Cell& cell : cells) {
        const Vec4 c = closest_on_cell(cell, point);
        const float d = (c - point).length2();
        if (d < best_d) {
            best_d = d;
            best = c;
        }
    }
    return best;
}

Vec4 Mesh::closest_on_cell(const Cell& cell, const Vec4& point_in) const {
    // project the point onto the cell hyperplane
    const Vec4 v0 = cell_representative_vertex(cell);
    const float k = (point_in.dot(cell.normal) - v0.dot(cell.normal)) / cell.normal.length2();
    const Vec4 point = point_in - cell.normal * k;

    // This is the same algorithm one dimension down: is the point within all faces?
    bool inside = true;
    for (int face_idx : cell.faces) {
        const Face& face = faces[face_idx];
        const Vec4 fv0 = face_representative_vertex(face);
        const Vec4 e0 = edge_vector(face.edges[0]);
        const Vec4 e1 = edge_vector(face.edges[1]);
        Vec4 normal = triple_cross_product(e0, e1, cell.normal).normalized();
        if (fv0.dot(normal) < 0.0f) normal = -normal;
        if (fv0.dot(normal) < point.dot(normal)) {
            inside = false;
            break;
        }
    }
    if (inside) return point;

    Vec4 best = point;
    float best_d = std::numeric_limits<float>::infinity();
    for (int face_idx : cell.faces) {
        const Vec4 c = closest_on_face(faces[face_idx], cell.normal, point);
        const float d = (c - point).length2();
        if (d < best_d) {
            best_d = d;
            best = c;
        }
    }
    return best;
}

Vec4 Mesh::closest_on_face(const Face& face, const Vec4& cell_normal, const Vec4& point_in) const {
    // Project the point onto the face
    const Vec4 v0 = face_representative_vertex(face);
    const Vec4 e0 = edge_vector(face.edges[0]);
    const Vec4 e1 = edge_vector(face.edges[1]);
    Vec4 normal = triple_cross_product(e0, e1, cell_normal);
    if (v0.dot(normal) < 0.0f) normal = -normal;

    const float k = (point_in.dot(normal) - v0.dot(normal)) / normal.length2();
    const Vec4 point = point_in - normal * k;

    // Is the point inside all the edges?
    bool inside = true;
    for (int edge_idx : face.edges) {
        const Edge& edge = edges[edge_idx];
        const Vec4 ev0 = edge_representative_vertex(edge);
        const Vec4 ee = edge_vector(edge_idx);
        Vec4 edge_normal = triple_cross_product(ee, normal, cell_normal);
        if (edge_normal.dot(ev0) < 0.0f) edge_normal = -edge_normal;
        if (ev0.dot(edge_normal) < point.dot(edge_normal)) {
            inside = false;
            break;
        }
    }
    if (inside) return point;

    Vec4 best = point;
    float best_d = std::numeric_limits<float>::infinity();
    for (int edge_idx : face.edges) {
        const Vec4 c = closest_on_edge(edges[edge_idx], point);
        const float d = (c - point).length2();
        if (d < best_d) {
            best_d = d;
            best = c;
        }
    }
    return best;
}

Vec4 Mesh::closest_on_edge(const Edge& edge, const Vec4& point) const {
    const Vec4 a = vertices[edge.hd_vertex];
    const Vec4 b = vertices[edge.tl_vertex];
    const Vec4 ab = b - a;
    // NOTE: upstream hypervis computes (a - point).ab here (inverted sign), which
    // snaps to the wrong endpoint for edge-region queries.  Fixed.
    float lambda = (point - a).dot(ab) / ab.length2();
    lambda = std::min(1.0f, std::max(0.0f, lambda));
    return a + ab * lambda;
}

// ---------------------------------------------------------------------------
// Tetra soup / geodesic subdivision
// ---------------------------------------------------------------------------
TetraSoup tetra_soup_from_mesh(const Mesh& mesh) {
    TetraSoup soup;
    soup.positions = mesh.vertices;
    for (const Tetrahedron& t : mesh.tetrahedra) {
        for (int k = 0; k < 4; ++k) soup.indices.push_back(t.v[k]);
        soup.cell_of_tet.push_back(t.cell);
    }
    return soup;
}

TetraSoup make_geodesic(const TetraSoup& soup, int frequency, float radius) {
    TetraSoup out;
    if (frequency < 1) frequency = 1;

    for (size_t ti = 0; ti + 3 < soup.indices.size(); ti += 4) {
        const Vec4& a = soup.positions[soup.indices[ti + 0]];
        const Vec4& b = soup.positions[soup.indices[ti + 1]];
        const Vec4& c = soup.positions[soup.indices[ti + 2]];
        const Vec4& d = soup.positions[soup.indices[ti + 3]];
        const int cell = ti / 4 < soup.cell_of_tet.size() ? soup.cell_of_tet[ti / 4] : -1;

        using Idx = std::tuple<int, int, int>;
        std::vector<Idx> mapped;
        const int F = frequency;
        for (int n = 0; n < F; ++n) {
            for (int i = 0; i < n + 1; ++i) {
                for (int j = 0; j < n - i + 1; ++j) {
                    const int k = n - i - j;
                    // tetrahedron based at this vertex: x, xi, xj, xk
                    mapped.insert(mapped.end(), {Idx{i, j, k}, Idx{i + 1, j, k}, Idx{i, j + 1, k}, Idx{i, j, k + 1}});

                    if (n < F - 1) {
                        // octahedron here as well, as four tetrahedra
                        mapped.insert(mapped.end(), {
                            Idx{i + 1, j, k}, Idx{i, j + 1, k}, Idx{i, j, k + 1}, Idx{i + 1, j, k + 1},
                            Idx{i + 1, j, k}, Idx{i, j + 1, k}, Idx{i + 1, j + 1, k}, Idx{i + 1, j, k + 1},
                            Idx{i, j + 1, k}, Idx{i, j, k + 1}, Idx{i + 1, j, k + 1}, Idx{i, j + 1, k + 1},
                            Idx{i, j + 1, k}, Idx{i + 1, j + 1, k}, Idx{i + 1, j, k + 1}, Idx{i, j + 1, k + 1}});
                    }
                    if (n < F - 2) {
                        mapped.insert(mapped.end(), {Idx{i + 1, j + 1, k}, Idx{i + 1, j, k + 1},
                                                     Idx{i, j + 1, k + 1}, Idx{i + 1, j + 1, k + 1}});
                    }
                }
            }
        }

        std::map<Idx, int> vertex_map;
        for (const Idx& coords : mapped) {
            auto it = vertex_map.find(coords);
            int index;
            if (it == vertex_map.end()) {
                index = static_cast<int>(out.positions.size());
                const float s = std::get<0>(coords) / static_cast<float>(F);
                const float t = std::get<1>(coords) / static_cast<float>(F);
                const float u = std::get<2>(coords) / static_cast<float>(F);
                const float r = 1.0f - s - t - u;
                out.positions.push_back((a * r + b * s + c * t + d * u).normalized_or_zero() * radius);
                vertex_map.emplace(coords, index);
            } else {
                index = it->second;
            }
            out.indices.push_back(index);
        }
        const size_t tets_added = mapped.size() / 4;
        for (size_t k = 0; k < tets_added; ++k) out.cell_of_tet.push_back(cell);
    }
    return out;
}

}  // namespace hv4d
