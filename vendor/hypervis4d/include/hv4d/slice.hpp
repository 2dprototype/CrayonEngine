// hv4d/slice.hpp
// CPU hyperplane slicing: intersect bodies with a 3D slice through 4D space and
// produce renderable 3D geometry.  This is the CPU equivalent of hypervis'
// slice.comp compute shader, plus an exact wireframe of the slice polyhedron.
#pragma once

#include <vector>

#include "hv4d/math.hpp"
#include "hv4d/physics.hpp"

namespace hv4d {

// A hyperplane { x : dot(normal, x - base) = 0 } together with an orthonormal
// basis (e[0], e[1], e[2]) spanning it.  project() gives 3D coordinates in
// that basis, so the engine renders the slice as ordinary 3D geometry.
struct SlicePlane {
    Vec4 normal = Vec4::unit_w();
    Vec4 base = Vec4::zero();
    Vec4 e[3] = {Vec4::unit_x(), Vec4::unit_y(), Vec4::unit_z()};

    // The classic slice: constant w.
    static SlicePlane at_w(float w);
    // Arbitrary unit normal and a point on the plane.
    static SlicePlane from_normal(const Vec4& normal, const Vec4& base);

    float signed_distance(const Vec4& p) const { return normal.dot(p - base); }
    Vec3 project(const Vec4& p) const {
        const Vec4 d = p - base;
        return {d.dot(e[0]), d.dot(e[1]), d.dot(e[2])};
    }
    Vec4 unproject(const Vec3& p) const { return base + e[0] * p.x + e[1] * p.y + e[2] * p.z; }
};

struct SliceTriangle {
    Vec3 p[3];
    Vec3 normal;
    int cell = -1;  // source cell of the polytope (for colouring)
};

struct SliceSegment {
    Vec3 a, b;
};

// Filled slice of a convex polytope body (triangles from its tetrahedra).
void slice_body_triangles(const Body& body, const SlicePlane& plane, std::vector<SliceTriangle>& out);

// Exact wireframe of the slice polyhedron (one segment per 2-face that straddles the plane).
void slice_body_wireframe(const Body& body, const SlicePlane& plane, std::vector<SliceSegment>& out);

// Hypersphere slice -> 3D sphere.  Returns false if the plane misses the sphere.
bool slice_sphere(const Body& body, const SlicePlane& plane, Vec3& center, float& radius);

// Half-space slice -> 3D plane  dot(n3, p) = offset.  Returns false if parallel.
bool slice_half_space(const Body& body, const SlicePlane& plane, Vec3& n3, float& offset);

// Slice a single tetrahedron (4 world-space vertices).  Appends 0..2 triangles.
void slice_tetrahedron(const Vec4 v[4], const SlicePlane& plane, int cell, std::vector<SliceTriangle>& out);

}  // namespace hv4d
