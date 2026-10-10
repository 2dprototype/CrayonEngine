#include "hv4d/slice.hpp"

#include <algorithm>
#include <cmath>

namespace hv4d {

SlicePlane SlicePlane::at_w(float w) {
    SlicePlane p;
    p.normal = Vec4::unit_w();
    p.base = Vec4(0, 0, 0, w);
    return p;
}

SlicePlane SlicePlane::from_normal(const Vec4& n_in, const Vec4& base) {
    SlicePlane p;
    p.normal = n_in.normalized();
    p.base = base;
    orthonormal_basis(p.normal, p.e);
    return p;
}

namespace {

constexpr float SLICE_EPS = 1e-5f;

Vec3 tri_normal(const Vec3& a, const Vec3& b, const Vec3& c) { return normalize(cross(b - a, c - a)); }

// Order 4 coplanar points around their centroid (convex quad).
void order_quad(Vec3 q[4]) {
    Vec3 c = (q[0] + q[1] + q[2] + q[3]) * 0.25f;
    Vec3 n = cross(q[1] - q[0], q[2] - q[0]);
    if (length(n) < 1e-12f) n = cross(q[1] - q[0], q[3] - q[0]);
    n = normalize(n);
    const Vec3 first = normalize(q[0] - c);
    float ang[4] = {0, 0, 0, 0};
    for (int i = 1; i < 4; ++i) {
        const Vec3 e = normalize(q[i] - c);
        float a = std::acos(std::min(1.0f, std::max(-1.0f, dot(first, e))));
        if (dot(n, cross(first, e)) < 0.0f) a = -a;
        ang[i] = a;
    }
    int idx[4] = {0, 1, 2, 3};
    std::sort(idx, idx + 4, [&](int x, int y) { return ang[x] < ang[y]; });
    Vec3 r[4] = {q[idx[0]], q[idx[1]], q[idx[2]], q[idx[3]]};
    for (int i = 0; i < 4; ++i) q[i] = r[i];
}

}  // namespace

void slice_tetrahedron(const Vec4 v[4], const SlicePlane& plane, int cell, std::vector<SliceTriangle>& out) {
    float d[4];
    for (int i = 0; i < 4; ++i) d[i] = plane.signed_distance(v[i]);

    auto emit = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
        SliceTriangle t;
        t.p[0] = a; t.p[1] = b; t.p[2] = c;
        t.normal = tri_normal(a, b, c);
        t.cell = cell;
        out.push_back(t);
    };

    // tetrahedron lying in the cut plane: emit its four faces
    if (std::fabs(d[0]) < SLICE_EPS && std::fabs(d[1]) < SLICE_EPS && std::fabs(d[2]) < SLICE_EPS &&
        std::fabs(d[3]) < SLICE_EPS) {
        static const int F[4][3] = {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}};
        for (const auto& f : F) emit(plane.project(v[f[0]]), plane.project(v[f[1]]), plane.project(v[f[2]]));
        return;
    }

    Vec3 pts[6];
    int n = 0;
    for (int i = 0; i < 4; ++i)
        if (std::fabs(d[i]) < SLICE_EPS) pts[n++] = plane.project(v[i]);

    static const int E[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
    for (const auto& e : E) {
        const float d0 = d[e[0]], d1 = d[e[1]];
        if (std::fabs(d0) < SLICE_EPS || std::fabs(d1) < SLICE_EPS) continue;  // vertex handled above
        if ((d0 > 0.0f) == (d1 > 0.0f)) continue;                               // same side
        const float t = d0 / (d0 - d1);
        pts[n++] = plane.project(v[e[0]] + (v[e[1]] - v[e[0]]) * t);
    }

    if (n == 3) {
        emit(pts[0], pts[1], pts[2]);
    } else if (n == 4) {
        order_quad(pts);
        emit(pts[0], pts[1], pts[2]);
        emit(pts[0], pts[2], pts[3]);
    }
    // n < 3: touches at a point/edge only -> nothing to draw
}

void slice_body_triangles(const Body& body, const SlicePlane& plane, std::vector<SliceTriangle>& out) {
    if (body.collider.type != ColliderType::Mesh) return;
    const Mesh& mesh = *body.collider.mesh;

    // quick reject with the bounding hypersphere
    if (std::fabs(plane.signed_distance(body.pos)) > mesh.radius) return;

    const Mat4 rot = body.rotation.to_matrix();
    std::vector<Vec4> world(mesh.vertices.size());
    for (size_t i = 0; i < world.size(); ++i) world[i] = rot * mesh.vertices[i] + body.pos;

    for (const Tetrahedron& t : mesh.tetrahedra) {
        const Vec4 v[4] = {world[t.v[0]], world[t.v[1]], world[t.v[2]], world[t.v[3]]};
        slice_tetrahedron(v, plane, t.cell, out);
    }
}

void slice_body_wireframe(const Body& body, const SlicePlane& plane, std::vector<SliceSegment>& out) {
    if (body.collider.type != ColliderType::Mesh) return;
    const Mesh& mesh = *body.collider.mesh;
    if (std::fabs(plane.signed_distance(body.pos)) > mesh.radius) return;

    const Mat4 rot = body.rotation.to_matrix();
    std::vector<Vec4> world(mesh.vertices.size());
    std::vector<float> dist(mesh.vertices.size());
    for (size_t i = 0; i < world.size(); ++i) {
        world[i] = rot * mesh.vertices[i] + body.pos;
        dist[i] = plane.signed_distance(world[i]);
    }

    for (const auto& loop : mesh.face_loops) {
        const size_t n = loop.size();
        if (n < 3) continue;

        // Face lying in the slice plane: draw its outline.
        bool all_on = true;
        for (int vi : loop)
            if (std::fabs(dist[vi]) >= SLICE_EPS) { all_on = false; break; }
        if (all_on) {
            for (size_t i = 0; i < n; ++i)
                out.push_back({plane.project(world[loop[i]]), plane.project(world[loop[(i + 1) % n]])});
            continue;
        }

        // A convex polygon crosses the hyperplane in at most 2 points.
        Vec3 pts[4];
        int cnt = 0;
        for (size_t i = 0; i < n && cnt < 4; ++i) {
            const int a = loop[i], b = loop[(i + 1) % n];
            const float da = dist[a], db = dist[b];
            if (std::fabs(da) < SLICE_EPS) {
                pts[cnt++] = plane.project(world[a]);
            } else if (std::fabs(db) >= SLICE_EPS && (da > 0.0f) != (db > 0.0f)) {
                const float t = da / (da - db);
                pts[cnt++] = plane.project(world[a] + (world[b] - world[a]) * t);
            }
        }
        if (cnt >= 2) {
            // pick the two farthest apart (robust when a vertex-on-plane is hit twice)
            int bi = 0, bj = 1;
            float best = -1.0f;
            for (int i = 0; i < cnt; ++i)
                for (int j = i + 1; j < cnt; ++j) {
                    const float l = length(pts[i] - pts[j]);
                    if (l > best) { best = l; bi = i; bj = j; }
                }
            if (best > 1e-7f) out.push_back({pts[bi], pts[bj]});
        }
    }
}

bool slice_sphere(const Body& body, const SlicePlane& plane, Vec3& center, float& radius) {
    if (body.collider.type != ColliderType::Sphere) return false;
    const float h = plane.signed_distance(body.pos);
    const float r2 = body.collider.radius * body.collider.radius - h * h;
    if (r2 <= 0.0f) return false;
    radius = std::sqrt(r2);
    center = plane.project(body.pos - plane.normal * h);
    return true;
}

bool slice_half_space(const Body& body, const SlicePlane& plane, Vec3& n3, float& offset) {
    if (body.collider.type != ColliderType::HalfSpace) return false;
    const Vec4& n = body.collider.normal;
    n3 = {n.dot(plane.e[0]), n.dot(plane.e[1]), n.dot(plane.e[2])};
    if (length(n3) < 1e-4f) return false;  // plane parallel to the slice
    offset = n.dot(body.pos - plane.base);
    return true;
}

}  // namespace hv4d
