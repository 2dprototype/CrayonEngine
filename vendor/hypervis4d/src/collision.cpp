#include <algorithm>
#include <cmath>
#include <limits>

#include "hv4d/physics.hpp"

namespace hv4d {

using detail::ContactData;
using detail::EdgeFaceContact;
using detail::VertexCellContact;

namespace {

constexpr float INF = std::numeric_limits<float>::infinity();
constexpr size_t SAT_CACHE_MAX = 1000;

// -----------------------------------------------------------------------------
// Axis bookkeeping
// -----------------------------------------------------------------------------
struct ContactAxis {
    bool vertex_cell;  // else edge-face
    bool side;
    int cell_idx;
    int edge_idx;
    int face_idx;
};

struct AxisResult {
    enum Kind { Intersection, NotValidAxis, LargerPenetration, NoIntersection } kind = NotValidAxis;
    float penetration = 0.0f;
    ContactData contact;
    Vec4 normal;  // for NoIntersection
};

using EdgeCellsCache = std::optional<std::vector<Vec4>>;

// Is the great-circle arc of the Minkowski-sum check hit? (Gauss-map test that
// edge `e` of A and face `f` of B form a face of the Minkowski difference.)
bool minkowski_edge_face_check(const std::vector<Vec4>& edge_cells, const Vec4& face_u, const Vec4& face_v) {
    // normal of the great sphere the edge lies in
    if (edge_cells.size() < 3) return false;
    const Vec4 normal = triple_cross_product(edge_cells[0], edge_cells[1], edge_cells[2]);

    // intersect the plane defined by the face with the hyperplane, giving a line
    const Vec4& u = face_u;
    const Vec4& v = face_v;
    const float factor = -v.dot(normal) / u.dot(normal);
    if (!std::isfinite(factor)) return false;
    const Vec4 t = u * factor + v;
    // intersect t with the great sphere, giving two points
    const Vec4 s0 = t.normalized();
    if (!s0.is_finite()) return false;
    const Vec4 s1 = -s0;

    auto acosf_clamped = [](float x) { return std::acos(std::min(1.0f, std::max(-1.0f, x))); };

    // check that either s0 or s1 are inside the great arc
    Vec4 s;
    {
        const float target_angle = acosf_clamped(u.dot(v));
        const float s0_u = acosf_clamped(s0.dot(u));
        const float s0_v = acosf_clamped(s0.dot(v));
        const float s1_u = acosf_clamped(s1.dot(u));
        const float s1_v = acosf_clamped(s1.dot(v));
        // acos() near 0/pi has poor float conditioning: use a looser tolerance than EPSILON
        const float tol = 1e-4f;
        if (std::fabs(target_angle - s0_u - s0_v) < tol) s = s0;
        else if (std::fabs(target_angle - s1_u - s1_v) < tol) s = s1;
        else return false;  // neither s0 nor s1 are in the arc
    }

    // now check if s is actually in the spherical polygon corresponding to the edge
    const size_t n = edge_cells.size();
    for (size_t i = 0; i < n; ++i) {
        const Vec4& cu = edge_cells[i];
        const Vec4& cv = edge_cells[(i + 1) % n];
        const Vec4& cw = edge_cells[(i + 2) % n];
        const Vec4 nn = triple_cross_product(cu, cv, normal);
        // s must lie on the same side of the sphere as another point in the polygon, w
        if (s.dot(nn) * cw.dot(nn) < 0.0f) return false;
    }
    return true;  // s is indeed an intersection!
}

// -----------------------------------------------------------------------------
// Axis checks
// -----------------------------------------------------------------------------
AxisResult check_vertex_cell(MeshRef a, MeshRef b, int cell_idx, bool side, float min_penetration) {
    AxisResult out;
    const Cell& cell = a.mesh->cells[cell_idx];

    // grab a representative vertex on the cell to get the distance
    const Vec4 v0 = a.mesh->cell_representative_vertex(cell);
    const float dist_a = v0.dot(cell.normal);
    float min_dist_b = dist_a;
    int min_vertex_idx = 0;

    for (size_t vi = 0; vi < b.mesh->vertices.size(); ++vi) {
        const float dist_b = a.body->world_pos_to_body(b.body->body_pos_to_world(b.mesh->vertices[vi])).dot(cell.normal);
        if (dist_b < min_dist_b) {
            min_dist_b = dist_b;
            min_vertex_idx = static_cast<int>(vi);
        }
    }

    if (min_dist_b < dist_a) {
        // intersection along this axis
        if (dist_a - min_dist_b < min_penetration) {
            out.kind = AxisResult::Intersection;
            out.penetration = dist_a - min_dist_b;
            out.contact.kind = ContactData::VertexCell;
            out.contact.vc = VertexCellContact{side, min_vertex_idx, cell_idx, a.body->body_vec_to_world(cell.normal)};
        } else {
            out.kind = AxisResult::LargerPenetration;
        }
    } else {
        out.kind = AxisResult::NoIntersection;  // found a separating axis!
        out.normal = a.body->body_vec_to_world(cell.normal);
    }
    return out;
}

AxisResult check_edge_face(MeshRef a, MeshRef b, int edge_idx, int face_idx, bool side, float min_penetration,
                           EdgeCellsCache& edge_cells_ref) {
    AxisResult out;
    const Edge& edge = a.mesh->edges[edge_idx];
    const Face& face = b.mesh->faces[face_idx];

    if (!edge_cells_ref) {
        std::vector<int> cells;
        if (!edge.faces.empty()) {
            const Face& f0 = a.mesh->faces[edge.faces[0]];
            cells.push_back(f0.hd_cell);
            cells.push_back(f0.tl_cell);
            for (size_t k = 1; k < edge.faces.size(); ++k) {
                const Face& f = a.mesh->faces[edge.faces[k]];
                if (std::find(cells.begin(), cells.end(), f.hd_cell) == cells.end()) cells.push_back(f.hd_cell);
                else if (std::find(cells.begin(), cells.end(), f.tl_cell) == cells.end()) cells.push_back(f.tl_cell);
            }
        }
        std::vector<Vec4> normals;
        normals.reserve(cells.size());
        for (int c : cells) normals.push_back(a.mesh->cells[c].normal);
        edge_cells_ref = std::move(normals);
    }
    const std::vector<Vec4>& edge_cells = *edge_cells_ref;

    // grab a representative vertex on the edge, and the edge vector
    const Vec4 v0 = a.mesh->vertices[edge.hd_vertex];
    const Vec4 u = a.mesh->vertices[edge.tl_vertex] - v0;

    const Vec4 c0 = a.body->world_vec_to_body(b.body->body_vec_to_world(b.mesh->cells[face.hd_cell].normal));
    const Vec4 c1 = a.body->world_vec_to_body(b.body->body_vec_to_world(b.mesh->cells[face.tl_cell].normal));

    if (!minkowski_edge_face_check(edge_cells, -c0, -c1)) {
        out.kind = AxisResult::NotValidAxis;
        return out;
    }

    // grab two edges on the face (non-parallel by construction) as vectors in a's frame
    const Edge& e0 = b.mesh->edges[face.edges[0]];
    const Edge& e1 = b.mesh->edges[face.edges[1]];
    const Vec4 v = a.body->world_vec_to_body(
        b.body->body_vec_to_world(b.mesh->vertices[e0.tl_vertex] - b.mesh->vertices[e0.hd_vertex]));
    const Vec4 w = a.body->world_vec_to_body(
        b.body->body_vec_to_world(b.mesh->vertices[e1.tl_vertex] - b.mesh->vertices[e1.hd_vertex]));

    // a point on the face, in a's frame
    const Vec4 v1 = a.body->world_pos_to_body(b.body->body_pos_to_world(b.mesh->vertices[e0.hd_vertex]));

    // the normal adjacent to all three directions
    Vec4 n = triple_cross_product(u, v, w).normalized();
    if (!n.is_finite()) {
        out.kind = AxisResult::NotValidAxis;
        return out;
    }
    // ensure that n points from a to b
    float dist_a = n.dot(v0);
    if (dist_a < 0.0f) {
        n = -n;
        dist_a = -dist_a;
    }
    const float dist_b = n.dot(v1);

    if (dist_b < dist_a) {
        if (dist_a - dist_b < min_penetration) {
            out.kind = AxisResult::Intersection;
            out.penetration = dist_a - dist_b;
            out.contact.kind = ContactData::EdgeFace;
            EdgeFaceContact& c = out.contact.ef;
            c.side = side;
            c.k = a.body->body_pos_to_world(v0);
            c.t = a.body->body_vec_to_world(u);
            c.s = b.body->body_pos_to_world(b.mesh->vertices[e0.hd_vertex]);
            c.u = a.body->body_vec_to_world(v);
            c.v = a.body->body_vec_to_world(w);
            c.normal = a.body->body_vec_to_world(n);
        } else {
            out.kind = AxisResult::LargerPenetration;
        }
    } else {
        out.kind = AxisResult::NoIntersection;
        out.normal = a.body->body_vec_to_world(n);
    }
    return out;
}

AxisResult check_axis(MeshRef a, MeshRef b, const ContactAxis& axis, float min_penetration,
                      EdgeCellsCache& edge_cells_ref) {
    if (axis.vertex_cell) {
        if (!axis.side) std::swap(a, b);
        return check_vertex_cell(a, b, axis.cell_idx, axis.side, min_penetration);
    }
    if (axis.side) std::swap(a, b);
    return check_edge_face(a, b, axis.edge_idx, axis.face_idx, axis.side, min_penetration, edge_cells_ref);
}

// Projection span of a mesh body onto a world-space axis.
std::pair<float, float> axis_span(MeshRef a, const Vec4& normal) {
    float mn = INF, mx = -INF;
    for (const Vec4& v : a.mesh->vertices) {
        const float d = a.body->body_pos_to_world(v).dot(normal);
        mn = std::min(mn, d);
        mx = std::max(mx, d);
    }
    return {mn, mx};
}

// true if the spans still overlap on this axis
bool fast_check_axis(MeshRef a, MeshRef b, const Vec4& normal) {
    const auto ar = axis_span(a, normal);
    const auto br = axis_span(b, normal);
    return ar.first <= br.second && br.first <= ar.second;
}

// -----------------------------------------------------------------------------
// Contact generation
// -----------------------------------------------------------------------------
CollisionManifold resolve_vertex_cell_contact(MeshRef a, MeshRef b, const VertexCellContact& contact) {
    if (!contact.side) {
        // just swap the meshes around in the call
        VertexCellContact swapped = contact;
        swapped.side = true;
        CollisionManifold result = resolve_vertex_cell_contact(b, a, swapped);
        // flip the normal as the collision resolution code expects a -> b
        result.normal = -result.normal;
        return result;
    }

    const Cell& reference_cell = a.mesh->cells[contact.cell_idx];

    // Determine the incident cell: the cell at the vertex with the least dot
    // product with the reference normal.
    float min_dot = 1.0f;
    int incident_cell_idx = b.mesh->vertex_data[contact.vertex_idx].cells.empty()
                                ? 0
                                : b.mesh->vertex_data[contact.vertex_idx].cells[0];
    for (int ci : b.mesh->vertex_data[contact.vertex_idx].cells) {
        const float d = b.body->body_vec_to_world(b.mesh->cells[ci].normal).dot(contact.normal);
        if (d < min_dot) {
            min_dot = d;
            incident_cell_idx = ci;
        }
    }

    // clip the incident cell against the adjacent cells of the reference cell
    ClipMesh clipper = ClipMesh::from_cell(*b.mesh, incident_cell_idx);
    Vec4 v0 = Vec4::zero();
    for (int face_idx : reference_cell.faces) {
        const Face& face = a.mesh->faces[face_idx];
        v0 = a.mesh->vertices[a.mesh->edges[face.edges[0]].hd_vertex];

        const int cell_idx = face.hd_cell == contact.cell_idx ? face.tl_cell : face.hd_cell;
        const Vec4 clip_normal = b.body->world_vec_to_body(a.body->body_vec_to_world(-a.mesh->cells[cell_idx].normal));
        const float clip_distance = clip_normal.dot(b.body->world_pos_to_body(a.body->body_pos_to_world(v0)));
        clipper.clip_by(clip_normal, clip_distance);
    }
    const float reference_dist = v0.dot(reference_cell.normal);

    // keep points that are below the reference plane
    CollisionManifold m;
    float max_depth = 0.0f;
    for (const Vec4& b_vec : clipper.to_vertices()) {
        const Vec4 world_vec = b.body->body_pos_to_world(b_vec);
        const Vec4 a_vec = a.body->world_pos_to_body(world_vec);
        const float dist = a_vec.dot(reference_cell.normal);
        if (dist < reference_dist) {
            max_depth = std::max(max_depth, reference_dist - dist);
            m.contacts.push_back(world_vec);
        }
    }
    m.normal = a.body->body_vec_to_world(reference_cell.normal);
    m.depth = max_depth;
    return m;
}

// Solve the 3x3 system M x = y by Cramer's rule.  Returns false if singular.
bool solve3(const float M[3][3], const float y[3], float x[3]) {
    const float det = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
                      M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
                      M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
    if (std::fabs(det) < 1e-12f) return false;
    const float inv = 1.0f / det;
    x[0] = (y[0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[0][1] * (y[1] * M[2][2] - M[1][2] * y[2]) +
            M[0][2] * (y[1] * M[2][1] - M[1][1] * y[2])) * inv;
    x[1] = (M[0][0] * (y[1] * M[2][2] - M[1][2] * y[2]) - y[0] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
            M[0][2] * (M[1][0] * y[2] - y[1] * M[2][0])) * inv;
    x[2] = (M[0][0] * (M[1][1] * y[2] - y[1] * M[2][1]) - M[0][1] * (M[1][0] * y[2] - y[1] * M[2][0]) +
            y[0] * (M[1][0] * M[2][1] - M[1][1] * M[2][0])) * inv;
    return true;
}

CollisionManifold resolve_edge_face_contact(const EdgeFaceContact& c) {
    // k + x0 t  and  s + x1 u + x2 v  are the closest points between the edge's
    // line and the face's plane: a three-variable linear system.
    CollisionManifold m;
    m.normal = c.side ? -c.normal : c.normal;  // c.normal points edge-owner -> face-owner

    const Vec4 &k = c.k, &t = c.t, &s = c.s, &u = c.u, &v = c.v;
    const float M[3][3] = {{t.dot(t), -t.dot(u), -t.dot(v)},
                           {-t.dot(u), u.dot(u), u.dot(v)},
                           {-t.dot(v), u.dot(v), v.dot(v)}};
    const Vec4 ks = k - s;
    const float y[3] = {-ks.dot(t), ks.dot(u), ks.dot(v)};
    float x[3];
    if (!solve3(M, y, x)) {
        // shouldn't really happen; failsafe: empty contact
        m.depth = 0.0f;
        return m;
    }
    const Vec4 p1 = k + t * x[0];
    const Vec4 p2 = s + u * x[1] + v * x[2];
    m.depth = (p1 - p2).length();
    m.contacts.push_back((p1 + p2) * 0.5f);
    return m;
}

}  // namespace

// -----------------------------------------------------------------------------
// mesh_sat
// -----------------------------------------------------------------------------
std::optional<ContactData> CollisionDetection::mesh_sat(uint64_t key_a, uint64_t key_b, MeshRef a, MeshRef b) {
    // Bounding hypersphere check
    const float rr = a.mesh->radius + b.mesh->radius;
    if ((a.body->pos - b.body->pos).length2() > rr * rr) return std::nullopt;

    const uint64_t key = key_a < key_b ? (key_a << 32) | key_b : (key_b << 32) | key_a;

    float min_penetration = INF;
    std::optional<ContactData> curr_contact;
    EdgeCellsCache edge_cells_cache;

    // Cached separating axis from last frame?
    auto cached = sat_cache_.find(key);
    if (cached != sat_cache_.end()) {
        const Vec4 axis = cached->second;
        if (!fast_check_axis(a, b, axis)) return std::nullopt;
        // the cache entry is no longer useful
        sat_cache_.erase(cached);
    }

    // returns true if a separating axis was found
    auto axis_check = [&](const ContactAxis& axis) -> bool {
        AxisResult r = check_axis(a, b, axis, min_penetration, edge_cells_cache);
        switch (r.kind) {
            case AxisResult::Intersection:
                min_penetration = r.penetration;
                curr_contact = r.contact;
                return false;
            case AxisResult::NoIntersection:
                if (sat_cache_.size() >= SAT_CACHE_MAX) sat_cache_.clear();
                sat_cache_[key] = r.normal;
                return true;
            default:
                return false;
        }
    };

    for (int ci = 0; ci < static_cast<int>(a.mesh->cells.size()); ++ci)
        if (axis_check({true, true, ci, 0, 0})) return std::nullopt;

    for (int ci = 0; ci < static_cast<int>(b.mesh->cells.size()); ++ci)
        if (axis_check({true, false, ci, 0, 0})) return std::nullopt;

    for (int ei = 0; ei < static_cast<int>(a.mesh->edges.size()); ++ei) {
        edge_cells_cache.reset();
        for (int fi = 0; fi < static_cast<int>(b.mesh->faces.size()); ++fi)
            if (axis_check({false, false, 0, ei, fi})) return std::nullopt;
    }

    for (int ei = 0; ei < static_cast<int>(b.mesh->edges.size()); ++ei) {
        edge_cells_cache.reset();
        for (int fi = 0; fi < static_cast<int>(a.mesh->faces.size()); ++fi)
            if (axis_check({false, true, 0, ei, fi})) return std::nullopt;
    }

    return curr_contact;
}

// -----------------------------------------------------------------------------
// detect_collisions
// -----------------------------------------------------------------------------
std::optional<CollisionManifold> CollisionDetection::detect_collisions(uint64_t key_a, uint64_t key_b,
                                                                       const Body& a, const Body& b) {
    const ColliderType ta = a.collider.type, tb = b.collider.type;

    auto swapped = [&]() -> std::optional<CollisionManifold> {
        auto m = detect_collisions(key_b, key_a, b, a);
        if (m) m->normal = -m->normal;
        return m;
    };

    if (ta == ColliderType::HalfSpace && tb == ColliderType::Mesh) {
        const Vec4& normal = a.collider.normal;
        const float plane_distance = a.pos.dot(normal);
        float max_depth = 0.0f;
        CollisionManifold m;
        for (const Vec4& local : b.collider.mesh->vertices) {
            const Vec4 pos = b.body_pos_to_world(local);
            const float depth = plane_distance - pos.dot(normal);
            if (depth > 0.0f) {
                max_depth = std::max(max_depth, depth);
                m.contacts.push_back(pos);
            }
        }
        if (m.contacts.empty()) return std::nullopt;
        m.normal = normal;
        m.depth = max_depth;
        return m;
    }

    if (ta == ColliderType::HalfSpace && tb == ColliderType::Sphere) {
        const Vec4& normal = a.collider.normal;
        const float center_distance = b.pos.dot(normal) - a.pos.dot(normal);
        const float r = b.collider.radius;
        if (center_distance < r) {
            CollisionManifold m;
            m.normal = normal;
            m.depth = r - center_distance;
            m.contacts.push_back(b.pos - normal * r);
            return m;
        }
        return std::nullopt;
    }

    if ((ta == ColliderType::Mesh && tb == ColliderType::HalfSpace) ||
        (ta == ColliderType::Sphere && tb == ColliderType::HalfSpace) ||
        (ta == ColliderType::Sphere && tb == ColliderType::Mesh)) {
        return swapped();
    }

    if (ta == ColliderType::Mesh && tb == ColliderType::Mesh) {
        MeshRef ra{&a, a.collider.mesh.get()};
        MeshRef rb{&b, b.collider.mesh.get()};
        auto contact = mesh_sat(key_a, key_b, ra, rb);
        if (!contact) return std::nullopt;
        if (contact->kind == ContactData::VertexCell) return resolve_vertex_cell_contact(ra, rb, contact->vc);
        return resolve_edge_face_contact(contact->ef);
    }

    if (ta == ColliderType::Mesh && tb == ColliderType::Sphere) {
        const Mesh& mesh = *a.collider.mesh;
        const float r = b.collider.radius;
        if ((a.pos - b.pos).length() > mesh.radius + r) return std::nullopt;

        const Vec4 local = a.world_pos_to_body(b.pos);
        if (mesh.contains(local)) {
            // Sphere centre is inside the polytope: push out through the nearest cell.
            float best = INF;
            int best_cell = 0;
            for (size_t c = 0; c < mesh.cells.size(); ++c) {
                const float gap = mesh.cell_representative_vertex(mesh.cells[c]).dot(mesh.cells[c].normal) -
                                  local.dot(mesh.cells[c].normal);
                if (gap < best) {
                    best = gap;
                    best_cell = static_cast<int>(c);
                }
            }
            CollisionManifold m;
            m.normal = a.body_vec_to_world(mesh.cells[best_cell].normal);
            m.depth = r + best;
            m.contacts.push_back(b.pos);
            return m;
        }

        const Vec4 closest_point = a.body_pos_to_world(mesh.closest_point_to(local));
        const Vec4 displacement = closest_point - b.pos;
        const float dist = displacement.length();
        if (dist < EPSILON) return std::nullopt;
        const float depth = r - dist;
        if (depth > 0.0f) {
            CollisionManifold m;
            m.depth = depth;
            m.normal = -displacement / dist;
            m.contacts.push_back(closest_point);
            return m;
        }
        return std::nullopt;
    }

    if (ta == ColliderType::Sphere && tb == ColliderType::Sphere) {
        const Vec4 displacement = b.pos - a.pos;
        const float dist = displacement.length();
        const float ra = a.collider.radius, rb = b.collider.radius;
        const float depth = ra + rb - dist;
        if (depth > 0.0f) {
            CollisionManifold m;
            m.normal = dist > EPSILON ? displacement / dist : Vec4::unit_y();
            m.depth = depth;
            // midpoint of the overlap region
            m.contacts.push_back(a.pos + m.normal * (ra - depth * 0.5f));
            return m;
        }
        return std::nullopt;
    }

    return std::nullopt;
}

}  // namespace hv4d
