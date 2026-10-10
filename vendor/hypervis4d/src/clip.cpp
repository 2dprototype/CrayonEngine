// Adapted from https://www.geometrictools.com/Documentation/ClipMesh.pdf
#include <algorithm>
#include <unordered_map>

#include "hv4d/mesh.hpp"

namespace hv4d {

ClipMesh ClipMesh::from_cell(const Mesh& mesh, int cell_idx) {
    ClipMesh out;
    const Cell& cell = mesh.cells[cell_idx];

    std::unordered_map<int, int> vertex_map, edge_map, face_map;

    auto push_vertex = [&](int vertex_idx) -> int {
        auto it = vertex_map.find(vertex_idx);
        if (it != vertex_map.end()) return it->second;
        const int fresh = static_cast<int>(out.vertices_.size());
        CVertex v;
        v.point = mesh.vertices[vertex_idx];
        out.vertices_.push_back(v);
        vertex_map.emplace(vertex_idx, fresh);
        return fresh;
    };

    auto push_edge = [&](int edge_idx) -> int {
        auto it = edge_map.find(edge_idx);
        if (it != edge_map.end()) return it->second;
        const Edge& edge = mesh.edges[edge_idx];
        const int fresh = static_cast<int>(out.edges_.size());
        CEdge e;
        e.hd_vertex = push_vertex(edge.hd_vertex);
        e.tl_vertex = push_vertex(edge.tl_vertex);
        out.edges_.push_back(e);
        edge_map.emplace(edge_idx, fresh);
        return fresh;
    };

    auto push_face = [&](int face_idx) -> int {
        auto it = face_map.find(face_idx);
        if (it != face_map.end()) return it->second;
        const Face& face = mesh.faces[face_idx];
        const int fresh = static_cast<int>(out.faces_.size());
        CFace cf;
        for (int edge_idx : face.edges) {
            const int ce = push_edge(edge_idx);
            out.edges_[ce].faces.push_back(fresh);
            cf.edges.push_back(ce);
        }
        out.faces_.push_back(cf);
        face_map.emplace(face_idx, fresh);
        return fresh;
    };

    for (int face_idx : cell.faces) push_face(face_idx);
    return out;
}

void ClipMesh::clip_by(const Vec4& clip_normal, float clip_distance) {
    switch (process_vertices(clip_normal, clip_distance)) {
        case ProcessResult::NoneClipped:
            return;
        case ProcessResult::AllClipped:
            for (auto& e : edges_) e.visible = false;
            for (auto& f : faces_) f.visible = false;
            return;
        case ProcessResult::PartiallyClipped:
            break;
    }
    process_edges();
    process_faces();
}

std::vector<Vec4> ClipMesh::to_vertices() const {
    std::vector<Vec4> out;
    for (const auto& v : vertices_)
        if (v.visible) out.push_back(v.point);
    return out;
}

ClipMesh::ProcessResult ClipMesh::process_vertices(const Vec4& clip_normal, float clip_distance) {
    int positive = 0, negative = 0;
    for (auto& vertex : vertices_) {
        if (!vertex.visible) continue;
        vertex.distance = clip_normal.dot(vertex.point) - clip_distance;
        if (vertex.distance >= EPSILON) {
            ++positive;
        } else if (vertex.distance < -EPSILON) {
            ++negative;
            vertex.visible = false;
        } else {
            vertex.distance = 0.0f;
        }
    }
    if (negative == 0) return ProcessResult::NoneClipped;
    if (positive == 0) return ProcessResult::AllClipped;
    return ProcessResult::PartiallyClipped;
}

void ClipMesh::process_edges() {
    for (size_t edge_idx = 0; edge_idx < edges_.size(); ++edge_idx) {
        CEdge& edge = edges_[edge_idx];
        if (!edge.visible) continue;

        const float d0 = vertices_[edge.hd_vertex].distance;
        const float d1 = vertices_[edge.tl_vertex].distance;

        if (d0 <= 0.0f && d1 <= 0.0f) {
            // edge is culled, remove edge from faces sharing it
            for (auto& face : faces_) {
                for (size_t i = 0; i < face.edges.size(); ++i) {
                    if (face.edges[i] == static_cast<int>(edge_idx)) {
                        face.edges.erase(face.edges.begin() + i);
                        break;
                    }
                }
                if (face.edges.empty()) face.visible = false;
            }
            edge.visible = false;
        } else if (d0 >= 0.0f && d1 >= 0.0f) {
            // edge is on the nonnegative side; faces retain the edge
        } else {
            // edge is split by the plane.  New edge is <v0, I> when d0 > 0, or <I, v1> when d1 > 0.
            // d0 and d1 are at least 2 EPSILONs apart here, so the division is safe.
            const float t = d0 / (d0 - d1);
            CVertex fresh;
            fresh.point = vertices_[edge.hd_vertex].point * (1.0f - t) + vertices_[edge.tl_vertex].point * t;
            const int fresh_idx = static_cast<int>(vertices_.size());
            vertices_.push_back(fresh);
            // NB: `edge` reference stays valid (edges_ is not resized here).
            if (d0 > 0.0f)
                edge.tl_vertex = fresh_idx;
            else
                edge.hd_vertex = fresh_idx;
        }
    }
}

void ClipMesh::process_faces() {
    // The mesh straddles the plane, so a new convex polygonal face is generated.
    // Add it now and insert edges when they are visited.
    const int close_face_idx = static_cast<int>(faces_.size());
    faces_.push_back(CFace{});

    for (int face_idx = 0; face_idx < static_cast<int>(faces_.size()); ++face_idx) {
        if (!faces_[face_idx].visible) continue;

        int start, end;
        if (get_open_polyline(face_idx, start, end)) {
            CEdge close_edge;
            close_edge.hd_vertex = start;
            close_edge.tl_vertex = end;
            close_edge.faces = {face_idx, close_face_idx};
            const int fresh_edge_idx = static_cast<int>(edges_.size());
            edges_.push_back(close_edge);
            faces_[face_idx].edges.push_back(fresh_edge_idx);
            faces_[close_face_idx].edges.push_back(fresh_edge_idx);
        }
    }
}

bool ClipMesh::get_open_polyline(int face_idx, int& start_out, int& end_out) {
    const CFace& face = faces_[face_idx];

    for (int e : face.edges) {
        vertices_[edges_[e].hd_vertex].occurs = 0;
        vertices_[edges_[e].tl_vertex].occurs = 0;
    }
    for (int e : face.edges) {
        vertices_[edges_[e].hd_vertex].occurs += 1;
        vertices_[edges_[e].tl_vertex].occurs += 1;
    }

    // Each occurs value on this face's vertices must now be 1 or 2.  A vertex
    // with 1 is one end of the open polyline.
    int start = -1, end = -1;
    for (int e : face.edges) {
        const CEdge& edge = edges_[e];
        if (vertices_[edge.hd_vertex].occurs == 1) {
            if (start < 0) start = edge.hd_vertex;
            else if (end < 0) end = edge.hd_vertex;
        }
        if (vertices_[edge.tl_vertex].occurs == 1) {
            if (start < 0) start = edge.tl_vertex;
            else if (end < 0) end = edge.tl_vertex;
        }
    }
    if (start >= 0 && end >= 0) {
        start_out = start;
        end_out = end;
        return true;
    }
    return false;
}

}  // namespace hv4d
