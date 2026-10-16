#include "model_colliders.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace crayon {

std::vector<glm::vec3> simplify_point_cloud(const std::vector<glm::vec3>& in, int max_points) {
    // 1. dedupe on a 1e-4 grid
    struct Key { int64_t x, y, z; bool operator==(const Key& o) const { return x == o.x && y == o.y && z == o.z; } };
    struct KeyHash { size_t operator()(const Key& k) const { return size_t(k.x * 73856093 ^ k.y * 19349663 ^ k.z * 83492791); } };
    std::unordered_set<Key, KeyHash> seen;
    std::vector<glm::vec3> pts;
    pts.reserve(in.size());
    for (const auto& p : in) {
        Key k{int64_t(std::llround(p.x * 1e4)), int64_t(std::llround(p.y * 1e4)), int64_t(std::llround(p.z * 1e4))};
        if (seen.insert(k).second) pts.push_back(p);
    }
    if (max_points < 8) max_points = 8;
    if (int(pts.size()) <= max_points) return pts;

    // 2. farthest-point sampling keeps the silhouette of the cloud (extremes first)
    std::vector<glm::vec3> out;
    out.reserve(max_points);
    std::vector<float> dist(pts.size(), std::numeric_limits<float>::max());
    size_t first = 0;                                           // start from the point farthest from the centroid
    glm::vec3 c(0.0f);
    for (const auto& p : pts) c += p;
    c /= float(pts.size());
    float best = -1.0f;
    for (size_t i = 0; i < pts.size(); ++i) { float d = glm::dot(pts[i] - c, pts[i] - c); if (d > best) { best = d; first = i; } }
    size_t cur = first;
    for (int k = 0; k < max_points; ++k) {
        out.push_back(pts[cur]);
        size_t next = cur;
        float far = -1.0f;
        for (size_t i = 0; i < pts.size(); ++i) {
            const glm::vec3 d = pts[i] - pts[cur];
            dist[i] = std::min(dist[i], glm::dot(d, d));
            if (dist[i] > far) { far = dist[i]; next = i; }
        }
        if (far <= 0.0f) break;
        cur = next;
    }
    return out;
}

namespace {

struct PartData {
    const ModelPart* part;
    glm::mat4 m;                    // node/world matrix of the part inside the model
};

glm::vec3 apply(const glm::mat4& m, const glm::vec3& s, const glm::vec3& v) {
    return s * glm::vec3(m * glm::vec4(v, 1.0f));
}

// Oriented fit: returns the box (in the part's own rotated frame) of the scaled vertices.
struct FitBox { glm::vec3 center_world; glm::quat rot; glm::vec3 half; };
FitBox fit_box(const ModelPart& part, const glm::vec3& user_scale) {
    const glm::mat4& M = part.transform;
    const glm::vec3 sn(glm::length(glm::vec3(M[0])), glm::length(glm::vec3(M[1])), glm::length(glm::vec3(M[2])));
    glm::mat3 R(glm::vec3(M[0]) / std::max(sn.x, 1e-8f), glm::vec3(M[1]) / std::max(sn.y, 1e-8f), glm::vec3(M[2]) / std::max(sn.z, 1e-8f));
    glm::vec3 lo(1e30f), hi(-1e30f);
    for (const auto& v : part.cpu_vertices) {
        glm::vec3 p = sn * v.position;
        lo = glm::min(lo, p); hi = glm::max(hi, p);
    }
    const glm::vec3 c = (lo + hi) * 0.5f, h = (hi - lo) * 0.5f;
    FitBox f;
    f.center_world = user_scale * (glm::vec3(M[3]) + R * c);
    f.rot = glm::quat_cast(R);
    f.half = h * glm::abs(user_scale);
    return f;
}

void add_primitive(CollisionKind kind, const ModelPart& part, const glm::vec3& scale, std::vector<ShapePartDesc>& out) {
    FitBox f = fit_box(part, scale);
    ShapePartDesc d;
    d.position = f.center_world;
    d.rotation = f.rot;
    if (kind == CollisionKind::Box) {
        d.kind = ShapePartDesc::Kind::Box;
        d.half_extent = f.half;
    } else if (kind == CollisionKind::Sphere) {
        d.kind = ShapePartDesc::Kind::Sphere;
        d.radius = std::max({f.half.x, f.half.y, f.half.z});
    } else {   // capsule along the longest axis (Jolt capsules run along Y)
        d.kind = ShapePartDesc::Kind::Capsule;
        int axis = 1;
        if (f.half.x > f.half[axis]) axis = 0;
        if (f.half.z > f.half[axis]) axis = 2;
        const float len = f.half[axis];
        const float r = std::max(0.001f, (f.half[(axis + 1) % 3] + f.half[(axis + 2) % 3]) * 0.5f);
        d.radius = std::min(r, len);
        d.half_height = std::max(len - d.radius, 0.001f);
        if (axis == 0) d.rotation = f.rot * glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 0, 1));
        else if (axis == 2) d.rotation = f.rot * glm::angleAxis(glm::half_pi<float>(), glm::vec3(1, 0, 0));
    }
    out.push_back(std::move(d));
}

void append_world_points(const ModelPart& part, const glm::vec3& scale, std::vector<glm::vec3>& pts) {
    pts.reserve(pts.size() + part.cpu_vertices.size());
    for (const auto& v : part.cpu_vertices) pts.push_back(apply(part.transform, scale, v.position));
}

void append_world_mesh(const ModelPart& part, const glm::vec3& scale, std::vector<glm::vec3>& verts, std::vector<uint32_t>& idx) {
    const uint32_t base = static_cast<uint32_t>(verts.size());
    for (const auto& v : part.cpu_vertices) verts.push_back(apply(part.transform, scale, v.position));
    for (GLuint i : part.cpu_indices) idx.push_back(base + i);
}

} // namespace

bool build_model_collider_parts(const Model3D& model, const ModelColliderOptions& opt,
                                std::vector<ShapePartDesc>& parts, std::string& error) {
    parts.clear();
    error.clear();
    auto usable = [](const ModelPart& p) { return !p.is_skinned && !p.cpu_vertices.empty() && !p.cpu_indices.empty(); };

    if (opt.use_authored && model.has_collision_parts()) {
        ShapePartDesc mesh;                 // all trimesh parts are merged into one mesh shape
        mesh.kind = ShapePartDesc::Kind::Mesh;
        for (const ModelPart& part : model.get_parts()) {
            if (part.collision == CollisionKind::None || !usable(part)) continue;
            switch (part.collision) {
                case CollisionKind::Box:
                case CollisionKind::Sphere:
                case CollisionKind::Capsule:
                    add_primitive(part.collision, part, opt.scale, parts);
                    break;
                case CollisionKind::Convex: {
                    ShapePartDesc d; d.kind = ShapePartDesc::Kind::Convex;
                    std::vector<glm::vec3> pts; append_world_points(part, opt.scale, pts);
                    d.points = simplify_point_cloud(pts, opt.max_hull_points);
                    parts.push_back(std::move(d));
                    break;
                }
                case CollisionKind::Mesh:
                    if (opt.dynamic) {     // moving bodies cannot use triangle meshes: use the part's convex hull
                        ShapePartDesc d; d.kind = ShapePartDesc::Kind::Convex;
                        std::vector<glm::vec3> pts; append_world_points(part, opt.scale, pts);
                        d.points = simplify_point_cloud(pts, opt.max_hull_points);
                        parts.push_back(std::move(d));
                    } else {
                        append_world_mesh(part, opt.scale, mesh.vertices, mesh.indices);
                    }
                    break;
                default: break;
            }
        }
        if (!mesh.indices.empty()) parts.push_back(std::move(mesh));
        if (parts.empty()) error = "the model's collision parts have no usable geometry";
        return !parts.empty();
    }

    // No authored collision: derive it from the visible geometry.
    std::vector<const ModelPart*> visible;
    for (const ModelPart& p : model.get_parts()) if (usable(p) && (p.collision == CollisionKind::None || !opt.use_authored)) visible.push_back(&p);
    if (visible.empty()) { error = "the model has no geometry to build a collider from"; return false; }

    std::string shape = opt.shape;
    if (shape == "auto") shape = opt.dynamic ? "convex" : "mesh";

    if (shape == "mesh") {
        if (opt.dynamic) { error = "shape 'mesh' is static-only; use 'convex' (or box/sphere/capsule) for moving bodies"; return false; }
        ShapePartDesc d; d.kind = ShapePartDesc::Kind::Mesh;
        for (const ModelPart* p : visible) append_world_mesh(*p, opt.scale, d.vertices, d.indices);
        parts.push_back(std::move(d));
        return true;
    }
    if (shape == "convex") {
        ShapePartDesc d; d.kind = ShapePartDesc::Kind::Convex;
        std::vector<glm::vec3> pts;
        for (const ModelPart* p : visible) append_world_points(*p, opt.scale, pts);
        d.points = simplify_point_cloud(pts, opt.max_hull_points);
        if (d.points.size() < 4) { error = "not enough distinct points for a convex hull"; return false; }
        parts.push_back(std::move(d));
        return true;
    }
    if (shape == "box" || shape == "sphere" || shape == "capsule") {
        glm::vec3 lo(1e30f), hi(-1e30f);
        for (const ModelPart* p : visible)
            for (const auto& v : p->cpu_vertices) { glm::vec3 w = apply(p->transform, opt.scale, v.position); lo = glm::min(lo, w); hi = glm::max(hi, w); }
        const glm::vec3 c = (lo + hi) * 0.5f, h = (hi - lo) * 0.5f;
        ShapePartDesc d;
        d.position = c;
        if (shape == "box") { d.kind = ShapePartDesc::Kind::Box; d.half_extent = h; }
        else if (shape == "sphere") { d.kind = ShapePartDesc::Kind::Sphere; d.radius = std::max({h.x, h.y, h.z}); }
        else {
            d.kind = ShapePartDesc::Kind::Capsule;
            d.radius = std::max(0.001f, std::min(std::max(h.x, h.z), h.y));
            d.half_height = std::max(h.y - d.radius, 0.001f);
        }
        parts.push_back(std::move(d));
        return true;
    }
    error = "unknown collider shape '" + shape + "' (use auto, mesh, convex, box, sphere or capsule)";
    return false;
}

} // namespace crayon
