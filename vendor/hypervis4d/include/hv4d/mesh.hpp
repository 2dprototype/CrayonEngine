// hv4d/mesh.hpp
// Convex 4-polytope meshes with full vertex/edge/face/cell topology, generated
// from Schlafli symbols via Todd-Coxeter coset enumeration, plus a
// tetrahedral decomposition (for slicing/rendering) and a ClipMesh used by the
// contact generator.   Re-creation of hypervis' `mesh` module.
#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <unordered_map>
#include <vector>

#include "hv4d/math.hpp"

namespace hv4d {

struct VertexData {
    std::vector<int> cells;  // cells incident to this vertex
};

struct Edge {
    int hd_vertex = -1;
    int tl_vertex = -1;
    std::vector<int> faces;  // faces around this edge
};

struct Face {
    int hd_cell = -1;
    int tl_cell = -1;  // the two cells sharing this face
    std::vector<int> edges;
};

struct Cell {
    Vec4 normal;  // outward unit normal of the cell's hyperplane
    std::vector<int> faces;
};

// One tetrahedron of the boundary's tetrahedral decomposition.
struct Tetrahedron {
    int v[4];
    int cell;
};

// The six regular convex 4-polytopes.
enum class RegularSolid {
    FiveCell,        // {3,3,3}
    EightCell,       // {4,3,3}  (tesseract)
    SixteenCell,     // {3,3,4}
    TwentyFourCell,  // {3,4,3}
    OneTwentyCell,   // {5,3,3}
    SixHundredCell,  // {3,3,5}
};

struct Mesh {
    float radius = 1.0f;  // bounding hypersphere radius (circumradius)
    std::vector<Vec4> vertices;
    std::vector<VertexData> vertex_data;
    std::vector<Edge> edges;
    std::vector<Face> faces;
    std::vector<Cell> cells;

    // ---- derived data (built by finalize()) ----
    std::vector<std::vector<int>> face_loops;  // ordered vertex cycle of each face
    std::vector<Tetrahedron> tetrahedra;       // boundary decomposed into tetrahedra
    float volume = 0.0f;                       // 4-volume (hypervolume)
    // Rotational inertia (per unit mass) about a coordinate plane, assuming the
    // body is centred at the origin and isotropic enough for a scalar.
    float inertia_per_mass = 1.0f / 6.0f;

    // Generate the regular polytope with Schlafli symbol {p, q, r}.
    // Circumradius is 1 (a tesseract has edge length 1).
    static Mesh from_schlafli_symbol(int p, int q, int r);
    static Mesh from_regular_solid(RegularSolid solid);
    static void schlafli_of(RegularSolid solid, int out[3]);

    // Uniformly scaled copy (vertices and radius scaled, topology shared).
    Mesh scaled(float factor) const;

    // Closest point on (or in) the polytope to `point` (all in mesh-local space).
    Vec4 closest_point_to(const Vec4& point) const;

    // Is `point` inside (or on) the polytope?  (local space)
    bool contains(const Vec4& point) const;

    // Build face_loops / tetrahedra / volume / inertia.  Called by the
    // generators; call again if you edit vertices manually.
    void finalize();

    // helpers
    Vec4 cell_representative_vertex(const Cell& cell) const;
    Vec4 face_representative_vertex(const Face& face) const;
    Vec4 edge_representative_vertex(const Edge& edge) const;
    Vec4 edge_vector(int edge_idx) const;

  private:
    Vec4 closest_on_cell(const Cell& cell, const Vec4& point) const;
    Vec4 closest_on_face(const Face& face, const Vec4& cell_normal, const Vec4& point) const;
    Vec4 closest_on_edge(const Edge& edge, const Vec4& point) const;
};

// Subdivide each tetrahedron `frequency` times and project onto a hypersphere
// (used to make a render mesh for a hypersphere).  Returns tetra vertex lists.
struct TetraSoup {
    std::vector<Vec4> positions;
    std::vector<int> cell_of_tet;  // cell id per tetrahedron (or -1)
    std::vector<int> indices;      // 4 per tetrahedron
};
TetraSoup tetra_soup_from_mesh(const Mesh& mesh);
TetraSoup make_geodesic(const TetraSoup& soup, int frequency, float radius);

// ---------------------------------------------------------------------------
// ClipMesh: a polyhedron (one 3D cell of a polytope) that can be clipped by
// half-spaces while maintaining its vertex/edge/face structure.
// Adapted from https://www.geometrictools.com/Documentation/ClipMesh.pdf
// ---------------------------------------------------------------------------
class ClipMesh {
  public:
    static ClipMesh from_cell(const Mesh& mesh, int cell_idx);

    // Keep the part of the mesh with  dot(clip_normal, p) - clip_distance >= 0.
    void clip_by(const Vec4& clip_normal, float clip_distance);

    // Vertices that survived clipping.
    std::vector<Vec4> to_vertices() const;

  private:
    struct CVertex {
        Vec4 point;
        float distance = 0.0f;
        int occurs = 0;
        bool visible = true;
    };
    struct CEdge {
        int hd_vertex = -1, tl_vertex = -1;
        std::vector<int> faces;
        bool visible = true;
    };
    struct CFace {
        std::vector<int> edges;
        bool visible = true;
    };

    enum class ProcessResult { NoneClipped, AllClipped, PartiallyClipped };

    ProcessResult process_vertices(const Vec4& clip_normal, float clip_distance);
    void process_edges();
    void process_faces();
    bool get_open_polyline(int face_idx, int& start, int& end);

    std::vector<CVertex> vertices_;
    std::vector<CEdge> edges_;
    std::vector<CFace> faces_;
};

}  // namespace hv4d
