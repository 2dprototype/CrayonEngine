// 4D GJK (distance / intersection) and EPA (penetration depth + normal) on
// support-mapped convex shapes.
//
// Upstream hypervis ships a prototype of this that is not wired into its world.
// This version reduces the simplex exactly (by enumerating the affine subsets
// of the <= 5 points and choosing the nearest valid one) which is simple and
// robust in 4D, and keeps EPA facets normalised.
#include <algorithm>
#include <cmath>
#include <limits>

#include "hv4d/physics.hpp"

namespace hv4d {

namespace {

constexpr float INF = std::numeric_limits<float>::infinity();

inline int popcount(unsigned v) {
    int c = 0;
    for (; v; v &= v - 1) ++c;
    return c;
}

struct Simplex {
    Vec4 p[5];
    int n = 0;
};

// Solve A x = b for an n x n system (n <= 4) with partial pivoting.
bool solve_small(int n, double A[4][5]) {
    for (int c = 0; c < n; ++c) {
        int piv = c;
        for (int r = c + 1; r < n; ++r)
            if (std::fabs(A[r][c]) > std::fabs(A[piv][c])) piv = r;
        if (std::fabs(A[piv][c]) < 1e-12) return false;
        if (piv != c)
            for (int k = 0; k <= n; ++k) std::swap(A[piv][k], A[c][k]);
        for (int r = 0; r < n; ++r) {
            if (r == c) continue;
            const double f = A[r][c] / A[c][c];
            for (int k = c; k <= n; ++k) A[r][k] -= f * A[c][k];
        }
    }
    for (int r = 0; r < n; ++r) A[r][n] /= A[r][r];
    return true;
}

// Closest point to the origin on the convex hull of s.p[0..n), reducing s to the
// supporting sub-simplex.  Returns the closest point.
Vec4 closest_on_simplex(Simplex& s) {
    const int n = s.n;
    double best_d2 = INFINITY;
    Vec4 best_pt = s.p[0];
    unsigned best_mask = 1;

    for (unsigned mask = 1; mask < (1u << n); ++mask) {
        int idx[5], k = 0;
        for (int i = 0; i < n; ++i)
            if (mask & (1u << i)) idx[k++] = i;

        double lam[5];
        if (k == 1) {
            lam[0] = 1.0;
        } else {
            // minimise |p0 + sum_i mu_i (p_i - p0)|^2   =>  Gram mu = -(p0 . q_i)
            const Vec4 p0 = s.p[idx[0]];
            Vec4 q[4];
            for (int i = 1; i < k; ++i) q[i - 1] = s.p[idx[i]] - p0;
            const int m = k - 1;
            double A[4][5];
            for (int r = 0; r < m; ++r) {
                for (int c = 0; c < m; ++c) A[r][c] = q[r].dot(q[c]);
                A[r][m] = -static_cast<double>(p0.dot(q[r]));
            }
            if (!solve_small(m, A)) continue;
            double sum = 0.0;
            for (int i = 0; i < m; ++i) {
                lam[i + 1] = A[i][m];
                sum += lam[i + 1];
            }
            lam[0] = 1.0 - sum;
        }

        bool ok = true;
        for (int i = 0; i < k; ++i)
            if (lam[i] < -1e-7) { ok = false; break; }
        if (!ok) continue;

        Vec4 pt = Vec4::zero();
        for (int i = 0; i < k; ++i) pt += s.p[idx[i]] * static_cast<float>(lam[i]);
        const double d2 = pt.length2();
        // prefer the smaller sub-simplex on ties
        if (d2 < best_d2 - 1e-12 || (d2 <= best_d2 + 1e-12 && k < popcount(best_mask))) {
            best_d2 = d2;
            best_pt = pt;
            best_mask = mask;
        }
    }

    // reduce
    Vec4 kept[5];
    int kn = 0;
    for (int i = 0; i < n; ++i)
        if (best_mask & (1u << i)) kept[kn++] = s.p[i];
    for (int i = 0; i < kn; ++i) s.p[i] = kept[i];
    s.n = kn;
    return best_pt;
}

// Support of the Minkowski difference A - B
Vec4 cso_support(const Body& a, const Body& b, const Vec4& dir) {
    return support_point(a, dir) - support_point(b, -dir);
}

// 5 affinely independent points?
bool full_simplex(const Vec4 p[5]) {
    const Vec4 a = p[1] - p[0], b = p[2] - p[0], c = p[3] - p[0], d = p[4] - p[0];
    return std::fabs(triple_cross_product(a, b, c).dot(d)) > 1e-7f;
}

// Try to grow a (possibly degenerate) simplex to 5 affinely independent points.
bool complete_simplex(const Body& a, const Body& b, Simplex& s) {
    static const Vec4 dirs[8] = {Vec4(1, 0, 0, 0),  Vec4(-1, 0, 0, 0), Vec4(0, 1, 0, 0), Vec4(0, -1, 0, 0),
                                 Vec4(0, 0, 1, 0),  Vec4(0, 0, -1, 0), Vec4(0, 0, 0, 1), Vec4(0, 0, 0, -1)};
    // dimension of the affine hull via incremental Gram-Schmidt
    auto rank_of = [](const Simplex& sx) {
        Vec4 basis[4];
        int r = 0;
        for (int i = 1; i < sx.n; ++i) {
            Vec4 v = sx.p[i] - sx.p[0];
            for (int j = 0; j < r; ++j) v -= basis[j] * v.dot(basis[j]);
            const float l = v.length();
            if (l > 1e-5f) basis[r++] = v / l;
        }
        return r;
    };
    int guard = 0;
    while (s.n < 5 && guard++ < 32) {
        const int before = rank_of(s);
        bool grew = false;
        for (const Vec4& d : dirs) {
            Simplex t = s;
            t.p[t.n++] = cso_support(a, b, d);
            if (rank_of(t) > before) {
                s = t;
                grew = true;
                break;
            }
        }
        if (!grew) return false;
    }
    return s.n == 5 && full_simplex(s.p);
}

struct Facet {
    int v[4];
    Vec4 normal;  // unit, outward
    float dist;   // >= 0
};

bool make_facet(const std::vector<Vec4>& verts, const Vec4& interior, int i0, int i1, int i2, int i3, Facet& f) {
    f.v[0] = i0; f.v[1] = i1; f.v[2] = i2; f.v[3] = i3;
    const Vec4& a = verts[i0];
    Vec4 n = triple_cross_product(verts[i1] - a, verts[i2] - a, verts[i3] - a);
    const float l = n.length();
    if (!(l > 1e-9f)) return false;
    n = n / l;
    if (n.dot(a - interior) < 0.0f) n = -n;
    f.normal = n;
    f.dist = n.dot(a);
    return true;
}

}  // namespace

Vec4 support_point(const Body& body, const Vec4& direction) {
    switch (body.collider.type) {
        case ColliderType::Sphere: return body.pos + direction.normalized_or_zero() * body.collider.radius;
        case ColliderType::Mesh: {
            const Vec4 d = body.world_vec_to_body(direction);
            float best = -INF;
            const Vec4* bv = nullptr;
            for (const Vec4& v : body.collider.mesh->vertices) {
                const float s = v.dot(d);
                if (s > best) { best = s; bv = &v; }
            }
            return body.body_pos_to_world(*bv);
        }
        case ColliderType::HalfSpace: return body.pos;  // unbounded; unsupported
    }
    return body.pos;
}

GjkResult gjk_epa(const Body& a, const Body& b) {
    GjkResult res;
    if (a.collider.type == ColliderType::HalfSpace || b.collider.type == ColliderType::HalfSpace) return res;

    Vec4 dir = b.pos - a.pos;
    if (dir.length2() < 1e-12f) dir = Vec4::unit_x();

    Simplex s;
    s.p[0] = cso_support(a, b, dir);
    s.n = 1;
    Vec4 closest = s.p[0];

    bool intersect = false;
    for (int iter = 0; iter < 64; ++iter) {
        if (closest.length2() < 1e-10f) { intersect = true; break; }
        const Vec4 d = -closest;                       // towards the origin
        const Vec4 w = cso_support(a, b, d);
        const float gap = (w - closest).dot(d.normalized());
        if (gap < 1e-5f) break;                         // no progress: separated
        s.p[s.n++] = w;
        closest = closest_on_simplex(s);
    }

    if (!intersect) {
        res.intersecting = false;
        res.distance = closest.length();
        res.separating_direction = (-closest).normalized_or_zero();
        return res;
    }
    res.intersecting = true;

    // ---------------- EPA ----------------
    if (!complete_simplex(a, b, s)) return res;

    std::vector<Vec4> verts(s.p, s.p + 5);
    Vec4 interior = Vec4::zero();
    for (int i = 0; i < 5; ++i) interior += verts[i];
    interior = interior / 5.0f;

    std::vector<Facet> facets;
    for (int skip = 0; skip < 5; ++skip) {
        int idx[4], k = 0;
        for (int i = 0; i < 5; ++i)
            if (i != skip) idx[k++] = i;
        Facet f;
        if (make_facet(verts, interior, idx[0], idx[1], idx[2], idx[3], f)) facets.push_back(f);
    }
    if (facets.empty()) return res;

    for (int iter = 0; iter < 128; ++iter) {
        size_t mi = 0;
        for (size_t i = 1; i < facets.size(); ++i)
            if (facets[i].dist < facets[mi].dist) mi = i;
        const Facet nearest = facets[mi];

        const Vec4 p = cso_support(a, b, nearest.normal);
        if (p.dot(nearest.normal) - nearest.dist < 1e-5f) {
            res.epa_ok = true;
            res.penetration_normal = nearest.normal;
            res.penetration_depth = std::max(0.0f, nearest.dist);
            return res;
        }

        // remove visible facets, gather horizon triangles
        struct Tri { int v[3]; };
        std::vector<Tri> horizon;
        std::vector<Facet> keep;
        for (const Facet& f : facets) {
            if (f.normal.dot(p - verts[f.v[0]]) > 1e-7f) {
                for (int skip = 0; skip < 4; ++skip) {
                    Tri t;
                    int k = 0;
                    for (int i = 0; i < 4; ++i)
                        if (i != skip) t.v[k++] = f.v[i];
                    std::sort(t.v, t.v + 3);
                    auto it = std::find_if(horizon.begin(), horizon.end(), [&](const Tri& o) {
                        return o.v[0] == t.v[0] && o.v[1] == t.v[1] && o.v[2] == t.v[2];
                    });
                    if (it != horizon.end()) horizon.erase(it);
                    else horizon.push_back(t);
                }
            } else {
                keep.push_back(f);
            }
        }
        if (horizon.empty()) {
            // numerical stall: accept the current best
            res.epa_ok = true;
            res.penetration_normal = nearest.normal;
            res.penetration_depth = std::max(0.0f, nearest.dist);
            return res;
        }
        const int pi = static_cast<int>(verts.size());
        verts.push_back(p);
        facets = std::move(keep);
        for (const Tri& t : horizon) {
            Facet f;
            if (make_facet(verts, interior, t.v[0], t.v[1], t.v[2], pi, f)) facets.push_back(f);
        }
        if (facets.empty()) return res;
    }

    // iteration cap: return the best facet found so far
    size_t mi = 0;
    for (size_t i = 1; i < facets.size(); ++i)
        if (facets[i].dist < facets[mi].dist) mi = i;
    res.epa_ok = true;
    res.penetration_normal = facets[mi].normal;
    res.penetration_depth = std::max(0.0f, facets[mi].dist);
    return res;
}

}  // namespace hv4d
