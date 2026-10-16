#pragma once
// Turns a Model3D into physics shapes. Authored collision nodes (UBX_/USP_/UCP_/UCX_ prefixes, or
// *_col / col_* names, or a "collision" custom property -- see Model3D::classify_collision_name) are used
// when present; otherwise the visible geometry is used according to `shape`.
#include "physics_system.hpp"
#include "../graphics/model3d.hpp"
#include <string>
#include <vector>

namespace crayon {

struct ModelColliderOptions {
    // Used only when the model has no authored collision parts:
    //   "auto" = triangle mesh for static bodies, convex hull for moving ones
    //   "mesh" | "convex" | "box" | "sphere" | "capsule"
    std::string shape = "auto";
    glm::vec3 scale{1.0f};     // same scale you pass to drawModel
    bool dynamic = false;      // true for dynamic/kinematic bodies: triangle meshes are replaced by convex hulls
    bool use_authored = true;  // false = ignore collision nodes and use the visible geometry
    int max_hull_points = 256; // hulls are simplified (farthest-point sampling) beyond this many points
};

// Fills `parts` (model-local space, scale applied). Returns false and sets `error` when no shape can be built.
bool build_model_collider_parts(const Model3D& model, const ModelColliderOptions& opt,
                                std::vector<ShapePartDesc>& parts, std::string& error);

// Removes duplicate points and simplifies to at most `max_points` (farthest-point sampling).
std::vector<glm::vec3> simplify_point_cloud(const std::vector<glm::vec3>& points, int max_points);

} // namespace crayon
