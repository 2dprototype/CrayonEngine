// hv4d/physics.hpp
// 4D rigid-body physics: bodies, colliders (half-space / hypersphere / convex
// 4-polytope), SAT + clipping contact generation, GJK/EPA, a sequential-impulse
// solver and a World that ties it all together.
// Re-creation of hypervis' `physics` and `world` modules.
#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "hv4d/math.hpp"
#include "hv4d/mesh.hpp"

namespace hv4d {

using BodyId = uint32_t;
constexpr BodyId INVALID_BODY = 0;

// ---------------------------------------------------------------------------
// Material
// ---------------------------------------------------------------------------
struct Material {
    float restitution = 0.2f;
    float friction = 0.4f;  // combined between bodies as sqrt(fa * fb)
};

// ---------------------------------------------------------------------------
// Collider
// ---------------------------------------------------------------------------
enum class ColliderType { HalfSpace, Mesh, Sphere };

struct Collider {
    ColliderType type = ColliderType::Sphere;
    Vec4 normal{0, 1, 0, 0};              // HalfSpace: outward (free-side) unit normal
    std::shared_ptr<const Mesh> mesh;     // Mesh
    RegularSolid solid = RegularSolid::EightCell;  // Mesh: which regular polytope it was made from (informational)
    float radius = 0.5f;                  // Sphere

    static Collider half_space(const Vec4& n) {
        Collider c;
        c.type = ColliderType::HalfSpace;
        c.normal = n.normalized();
        return c;
    }
    static Collider sphere(float r) {
        Collider c;
        c.type = ColliderType::Sphere;
        c.radius = r;
        return c;
    }
    static Collider polytope(std::shared_ptr<const Mesh> m, RegularSolid solid = RegularSolid::EightCell) {
        Collider c;
        c.type = ColliderType::Mesh;
        c.mesh = std::move(m);
        c.solid = solid;
        return c;
    }
    // Radius of a bounding hypersphere around the body origin (inf for half-space).
    float bounding_radius() const;
};

// ---------------------------------------------------------------------------
// Body
// ---------------------------------------------------------------------------
struct Velocity {
    Vec4 linear;
    Bivec4 angular;
};

struct Body {
    BodyId id = INVALID_BODY;
    float mass = 1.0f;           // effective mass (0 for stationary bodies)
    float dynamic_mass = 1.0f;   // mass the body has when it is NOT stationary
    // For tesseracts a scalar is sufficient; really it should be a tensor
    // Bivec4 -> Bivec4.  Computed from the shape by the World helpers.
    float moment_inertia_scalar = 1.0f / 6.0f;
    Material material;
    bool stationary = false;
    bool sensor = false;      // reports overlaps, never produces a collision response
    float gravity_scale = 1.0f;
    float linear_damping = 0.0f;
    float angular_damping = 0.0f;

    Vec4 pos;
    Rotor4 rotation;
    Velocity vel;
    Collider collider;

    // Accumulated by apply_force / apply_torque, consumed by step().
    Vec4 force_accum;
    Bivec4 torque_accum;

    uint64_t user_data = 0;

    // Recompute `mass` / `moment_inertia_scalar` from `dynamic_mass`, the collider shape and `stationary`.
    // Call after changing any of those (the World create_* helpers and setters below do it for you).
    void recompute_mass_properties();
    void set_dynamic_mass(float m) { dynamic_mass = m; recompute_mass_properties(); }
    void set_stationary(bool s);

    void resolve_impulse(const Vec4& impulse, const Vec4& world_contact);
    void step(float dt, const Vec4& gravity);
    Bivec4 inverse_moment_of_inertia(const Bivec4& body_bivec) const;
    Vec4 vel_at(const Vec4& world_pos) const;

    // Returns the ray parameter of the first hit (>= 0), if any.
    std::optional<float> ray_intersect(const Vec4& start, const Vec4& dir) const;

    Vec4 body_vec_to_world(const Vec4& v) const { return rotation.rotate(v); }
    Vec4 world_vec_to_body(const Vec4& v) const { return rotation.reverse().rotate(v); }
    Vec4 body_pos_to_world(const Vec4& v) const { return rotation.rotate(v) + pos; }
    Vec4 world_pos_to_body(const Vec4& v) const { return rotation.reverse().rotate(v - pos); }

    void apply_impulse(const Vec4& impulse, const Vec4& world_point) { resolve_impulse(impulse, world_point); }
    void apply_central_impulse(const Vec4& impulse) {
        if (!stationary && mass > 0.0f) vel.linear += impulse / mass;
    }
    void apply_force(const Vec4& f) { force_accum += f; }
    void apply_torque(const Bivec4& t) { torque_accum += t; }
};

// ---------------------------------------------------------------------------
// Collision data
// ---------------------------------------------------------------------------
struct CollisionManifold {
    Vec4 normal;                 // unit, pointing from body a towards body b
    float depth = 0.0f;
    std::vector<Vec4> contacts;  // world-space contact points
};

struct MeshRef {
    const Body* body;
    const Mesh* mesh;
};

namespace detail {

struct VertexCellContact {
    bool side;  // true => vertex is on body b but the cell is on body a
    int vertex_idx;
    int cell_idx;
    Vec4 normal;
};

struct EdgeFaceContact {
    bool side;  // true => edge is on body b but the face is on body a
    Vec4 k, t, s, u, v, normal;
};

struct ContactData {
    enum Kind { VertexCell, EdgeFace } kind = VertexCell;
    VertexCellContact vc{};
    EdgeFaceContact ef{};
};

}  // namespace detail

// Collision detection with a small cache of last separating axes per pair.
class CollisionDetection {
  public:
    std::optional<CollisionManifold> detect_collisions(uint64_t key_a, uint64_t key_b, const Body& a, const Body& b);
    void clear_cache() { sat_cache_.clear(); }
    size_t cache_size() const { return sat_cache_.size(); }

  private:
    std::optional<detail::ContactData> mesh_sat(uint64_t key_a, uint64_t key_b, MeshRef a, MeshRef b);

    static uint64_t pair_key(uint64_t a, uint64_t b) { return (a << 32) ^ b; }
    std::unordered_map<uint64_t, Vec4> sat_cache_;
};

// ---------------------------------------------------------------------------
// Contact solver
// ---------------------------------------------------------------------------
struct ContactState {
    Vec4 contact;
    float bias = 0.0f;
    float normal_mass = 0.0f;
    float normal_impulse = 0.0f;
    float tangent_mass[3] = {0, 0, 0};
    float tangent_impulse[3] = {0, 0, 0};
};

class CollisionConstraint {
  public:
    CollisionConstraint(const CollisionManifold& manifold, const Body& a, float mass_adjustment_a,
                        const Body& b, float mass_adjustment_b);
    void solve(Body& a, Body& b);

    Vec4 normal;
    Vec4 tangents[3];
    std::vector<ContactState> contacts;
    float mu = 0.4f;
};

// ---------------------------------------------------------------------------
// GJK / EPA on convex support-mapped shapes.
// ---------------------------------------------------------------------------
struct GjkResult {
    bool intersecting = false;
    Vec4 separating_direction;  // valid when !intersecting (points from A towards B)
    float distance = 0.0f;      // valid when !intersecting
    Vec4 penetration_normal;    // valid when intersecting && epa_ok (unit, A -> B)
    float penetration_depth = 0.0f;
    bool epa_ok = false;
};

// Support mapping of a body collider in world space (half-space unsupported).
Vec4 support_point(const Body& body, const Vec4& direction);

// Runs GJK on the Minkowski difference A - B, then EPA if they overlap.
GjkResult gjk_epa(const Body& a, const Body& b);

// ---------------------------------------------------------------------------
// World
// ---------------------------------------------------------------------------
enum class WorldEventType { CollisionEnter, CollisionExit, TriggerEnter, TriggerExit };

struct WorldEvent {
    WorldEventType type;
    BodyId a = 0, b = 0;
    Vec4 normal;
    float impulse = 0.0f;
};

struct RayHit {
    bool hit = false;
    BodyId body = INVALID_BODY;
    float t = 0.0f;
    Vec4 point;
    Vec4 normal;
};

struct ContactDebugInfo {
    BodyId a, b;
    Vec4 normal;
    float depth;
    std::vector<Vec4> points;
};

class World {
  public:
    World();

    Vec4 gravity{0.0f, -9.8f, 0.0f, 0.0f};
    int solver_iterations = 20;
    float slop = 0.01f;
    float baumgarte = 0.2f;

    // ---- body management ----
    BodyId add_body(Body body);  // assigns and returns a fresh id
    bool remove_body(BodyId id);
    Body* get(BodyId id);
    const Body* get(BodyId id) const;
    bool valid(BodyId id) const { return index_.count(id) != 0; }
    size_t body_count() const { return bodies_.size(); }
    const std::vector<Body>& bodies() const { return bodies_; }
    std::vector<Body>& bodies_mut() { return bodies_; }
    void clear();

    // ---- convenience constructors (mirror hypervis' shapes.rs) ----
    BodyId create_polytope(RegularSolid solid, const Vec4& pos, float scale = 1.0f, float mass = 1.0f,
                           bool stationary = false);
    BodyId create_hypersphere(const Vec4& pos, float radius = 0.5f, float mass = 1.0f, bool stationary = false);
    BodyId create_half_space(const Vec4& point, const Vec4& normal);
    // The standard arena: floor (+y) plus 4 side walls (x, z, w both ways).
    void create_arena(float half_size, const Material& material = Material{0.4f, 0.4f});

    // Shared (cached) mesh for a regular solid at a given uniform scale.
    std::shared_ptr<const Mesh> get_mesh(RegularSolid solid, float scale = 1.0f);

    // ---- simulation ----
    void update(float dt);
    std::vector<WorldEvent> take_events();
    const std::vector<ContactDebugInfo>& last_contacts() const { return last_contacts_; }

    // ---- queries ----
    RayHit raycast(const Vec4& origin, const Vec4& dir, float max_t = 1e30f) const;
    std::vector<BodyId> overlap_sphere(const Vec4& center, float radius) const;

    CollisionDetection& collision() { return collision_; }

  private:
    void rebuild_index();

    std::vector<Body> bodies_;
    std::unordered_map<BodyId, size_t> index_;
    BodyId next_id_ = 1;
    CollisionDetection collision_;

    std::unordered_map<uint64_t, std::shared_ptr<const Mesh>> mesh_cache_;

    std::unordered_set<uint64_t> prev_collisions_;
    std::unordered_set<uint64_t> prev_triggers_;
    std::vector<WorldEvent> events_;
    std::vector<ContactDebugInfo> last_contacts_;
};

}  // namespace hv4d
