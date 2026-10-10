#include "hv4d/physics.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace hv4d {

namespace {
constexpr float INF = std::numeric_limits<float>::infinity();

inline uint64_t unordered_pair(uint64_t a, uint64_t b) {
    return a < b ? (a << 32) | b : (b << 32) | a;
}
}  // namespace

// ---------------------------------------------------------------------------
// Collider
// ---------------------------------------------------------------------------
float Collider::bounding_radius() const {
    switch (type) {
        case ColliderType::HalfSpace: return INF;
        case ColliderType::Mesh:      return mesh ? mesh->radius : 0.0f;
        case ColliderType::Sphere:    return radius;
    }
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Body
// ---------------------------------------------------------------------------
void Body::recompute_mass_properties() {
    if (stationary || collider.type == ColliderType::HalfSpace || dynamic_mass <= 0.0f) {
        mass = 0.0f;
        moment_inertia_scalar = 0.0f;
        return;
    }
    mass = dynamic_mass;
    switch (collider.type) {
        case ColliderType::Mesh:
            moment_inertia_scalar = collider.mesh ? dynamic_mass * collider.mesh->inertia_per_mass : 0.0f;
            break;
        case ColliderType::Sphere:
            // uniform 4-ball: E[x_i^2] = R^2 / 6, so a rotation plane has I = m R^2 / 3
            moment_inertia_scalar = dynamic_mass * collider.radius * collider.radius / 3.0f;
            break;
        case ColliderType::HalfSpace: break;
    }
}

void Body::set_stationary(bool s) {
    stationary = s;
    if (s) {
        vel = Velocity{};
        force_accum = Vec4::zero();
        torque_accum = Bivec4::zero();
    }
    recompute_mass_properties();
}

Bivec4 Body::inverse_moment_of_inertia(const Bivec4& body_bivec) const {
    if (moment_inertia_scalar <= 0.0f) return Bivec4::zero();
    return (1.0f / moment_inertia_scalar) * body_bivec;
}

void Body::resolve_impulse(const Vec4& impulse, const Vec4& world_contact) {
    if (stationary || mass <= 0.0f) return;
    const Vec4 body_contact = world_pos_to_body(world_contact);
    const Bivec4 delta_angular =
        inverse_moment_of_inertia(body_contact.wedge_v(rotation.reverse().rotate(impulse)));
    vel.linear += impulse / mass;
    vel.angular = vel.angular + delta_angular;
}

void Body::step(float dt, const Vec4& gravity) {
    if (stationary) {
        force_accum = Vec4::zero();
        torque_accum = Bivec4::zero();
        return;
    }
    // gravity + accumulated forces
    vel.linear += gravity * (gravity_scale * dt);
    if (mass > 0.0f) vel.linear += force_accum * (dt / mass);
    vel.angular = vel.angular + inverse_moment_of_inertia(torque_accum) * dt;
    force_accum = Vec4::zero();
    torque_accum = Bivec4::zero();

    if (linear_damping > 0.0f) vel.linear *= 1.0f / (1.0f + linear_damping * dt);
    if (angular_damping > 0.0f) vel.angular = vel.angular * (1.0f / (1.0f + angular_damping * dt));

    pos += vel.linear * dt;
    rotation.update(vel.angular * dt);
}

Vec4 Body::vel_at(const Vec4& world_pos) const {
    const Vec4 body_pos = world_pos_to_body(world_pos);
    const Vec4 rot_vel = body_vec_to_world(body_pos.left_contract_bv(vel.angular));
    return vel.linear + rot_vel;
}

std::optional<float> Body::ray_intersect(const Vec4& start_w, const Vec4& dir_w) const {
    switch (collider.type) {
        case ColliderType::Mesh: {
            const Mesh& mesh = *collider.mesh;
            const Vec4 start = world_pos_to_body(start_w);
            const Vec4 dir = world_vec_to_body(dir_w);

            float t0 = -INF, t1 = INF;
            for (const Cell& cell : mesh.cells) {
                const Vec4 v0 = mesh.cell_representative_vertex(cell);
                const float denom = dir.dot(cell.normal);
                const float num = (v0 - start).dot(cell.normal);
                if (std::fabs(denom) < 1e-9f) {
                    if (num < 0.0f) return std::nullopt;  // parallel and outside
                    continue;
                }
                const float lambda = num / denom;
                if (denom < 0.0f) t0 = std::max(t0, lambda);
                else t1 = std::min(t1, lambda);
                if (t1 < t0) return std::nullopt;
            }
            if (t1 < 0.0f) return std::nullopt;
            return std::max(t0, 0.0f);
        }
        case ColliderType::Sphere: {
            // Solve a quadratic equation!
            const Vec4 s = start_w - pos;
            const float a = dir_w.length2();
            const float b = 2.0f * s.dot(dir_w);
            const float c = s.length2() - collider.radius * collider.radius;
            const float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f || a <= 0.0f) return std::nullopt;
            const float sq = std::sqrt(disc);
            const float t0 = (-b - sq) / (2.0f * a);
            const float t1 = (-b + sq) / (2.0f * a);
            if (t1 < 0.0f) return std::nullopt;
            return std::max(t0, 0.0f);
        }
        case ColliderType::HalfSpace: {
            const float denom = dir_w.dot(collider.normal);
            if (denom >= -1e-9f) return std::nullopt;
            const float t = (pos - start_w).dot(collider.normal) / denom;
            if (t < 0.0f) return std::nullopt;
            return t;
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// CollisionConstraint  (sequential impulses with Coulomb friction in the 3D
// tangent space of the contact normal)
// ---------------------------------------------------------------------------
CollisionConstraint::CollisionConstraint(const CollisionManifold& manifold, const Body& a,
                                         float mass_adjustment_a, const Body& b, float mass_adjustment_b) {
    normal = manifold.normal;
    const float depth = manifold.depth;

    const float e = std::min(a.material.restitution, b.material.restitution);
    mu = std::sqrt(std::max(0.0f, a.material.friction) * std::max(0.0f, b.material.friction));

    orthonormal_basis(normal, tangents);

    const float inv_a_mass = (!a.stationary && a.mass > 0.0f) ? mass_adjustment_a / a.mass : 0.0f;
    const float inv_b_mass = (!b.stationary && b.mass > 0.0f) ? mass_adjustment_b / b.mass : 0.0f;

    // n . (R x . I_b^-1(x /\ n') ~R)
    auto inverse_mass_term = [](const Body& body, const Vec4& n, const Vec4& contact) {
        if (body.stationary) return 0.0f;
        const Vec4 body_normal = body.world_vec_to_body(n);
        const Vec4 body_contact = body.world_pos_to_body(contact);
        return n.dot(body.body_vec_to_world(
            body_contact.left_contract_bv(body.inverse_moment_of_inertia(body_contact.wedge_v(body_normal)))));
    };

    const float inv_dt = 60.0f;
    for (const Vec4& contact : manifold.contacts) {
        const Vec4 rel_vel = b.vel_at(contact) - a.vel_at(contact);
        const float rel_vel_normal = rel_vel.dot(normal);

        ContactState cs;
        cs.contact = contact;
        const float slop = 0.01f, baumgarte = 0.2f;
        cs.bias = -baumgarte * inv_dt * std::min(slop - depth, 0.0f) +
                  (rel_vel_normal < -1.0f ? -e * rel_vel_normal : 0.0f);

        const float inv_l_a = mass_adjustment_a * inverse_mass_term(a, normal, contact);
        const float inv_l_b = mass_adjustment_b * inverse_mass_term(b, normal, contact);
        const float inv_n = inv_a_mass + inv_b_mass + inv_l_a + inv_l_b;
        cs.normal_mass = inv_n > 0.0f ? 1.0f / inv_n : 0.0f;

        for (int i = 0; i < 3; ++i) {
            const float t_a = mass_adjustment_a * inverse_mass_term(a, tangents[i], contact);
            const float t_b = mass_adjustment_b * inverse_mass_term(b, tangents[i], contact);
            const float inv_t = inv_a_mass + inv_b_mass + t_a + t_b;
            cs.tangent_mass[i] = inv_t > 0.0f ? 1.0f / inv_t : 0.0f;
        }
        contacts.push_back(cs);
    }
}

void CollisionConstraint::solve(Body& a, Body& b) {
    for (ContactState& cs : contacts) {
        // ---- friction ----
        {
            const Vec4 rel_vel = b.vel_at(cs.contact) - a.vel_at(cs.contact);
            float new_impulses[3];
            for (int i = 0; i < 3; ++i) {
                const float lambda = -rel_vel.dot(tangents[i]) * cs.tangent_mass[i];
                new_impulses[i] = cs.tangent_impulse[i] + lambda;
            }
            // clamp the total magnitude (friction cone)
            const float max_impulse = std::fabs(mu * cs.normal_impulse);
            const float mag = std::sqrt(new_impulses[0] * new_impulses[0] + new_impulses[1] * new_impulses[1] +
                                        new_impulses[2] * new_impulses[2]);
            if (mag > max_impulse) {
                const float factor = mag > 0.0f ? max_impulse / mag : 0.0f;
                for (float& v : new_impulses) v *= factor;
            }
            for (int i = 0; i < 3; ++i) {
                const Vec4 impulse = tangents[i] * (new_impulses[i] - cs.tangent_impulse[i]);
                cs.tangent_impulse[i] = new_impulses[i];
                a.resolve_impulse(-impulse, cs.contact);
                b.resolve_impulse(impulse, cs.contact);
            }
        }
        // ---- normal ----
        {
            const Vec4 rel_vel = b.vel_at(cs.contact) - a.vel_at(cs.contact);
            const float rel_vel_normal = rel_vel.dot(normal);
            const float lambda = cs.normal_mass * (-rel_vel_normal + cs.bias);
            const float prev = cs.normal_impulse;
            cs.normal_impulse = std::max(prev + lambda, 0.0f);
            const Vec4 impulse = normal * (cs.normal_impulse - prev);
            a.resolve_impulse(-impulse, cs.contact);
            b.resolve_impulse(impulse, cs.contact);
        }
    }
}

// ---------------------------------------------------------------------------
// World
// ---------------------------------------------------------------------------
World::World() = default;

void World::rebuild_index() {
    index_.clear();
    for (size_t i = 0; i < bodies_.size(); ++i) index_[bodies_[i].id] = i;
}

BodyId World::add_body(Body body) {
    body.id = next_id_++;
    index_[body.id] = bodies_.size();
    bodies_.push_back(std::move(body));
    return bodies_.back().id;
}

bool World::remove_body(BodyId id) {
    auto it = index_.find(id);
    if (it == index_.end()) return false;
    bodies_.erase(bodies_.begin() + static_cast<std::ptrdiff_t>(it->second));
    rebuild_index();
    // forget pairs involving this body (emit exits next update through the normal path)
    return true;
}

Body* World::get(BodyId id) {
    auto it = index_.find(id);
    return it == index_.end() ? nullptr : &bodies_[it->second];
}
const Body* World::get(BodyId id) const {
    auto it = index_.find(id);
    return it == index_.end() ? nullptr : &bodies_[it->second];
}

void World::clear() {
    bodies_.clear();
    index_.clear();
    collision_.clear_cache();
    prev_collisions_.clear();
    prev_triggers_.clear();
    events_.clear();
    last_contacts_.clear();
}

std::shared_ptr<const Mesh> World::get_mesh(RegularSolid solid, float scale) {
    uint32_t sbits;
    std::memcpy(&sbits, &scale, sizeof(sbits));
    const uint64_t key = (static_cast<uint64_t>(solid) << 32) | sbits;
    auto it = mesh_cache_.find(key);
    if (it != mesh_cache_.end()) return it->second;

    std::shared_ptr<const Mesh> mesh;
    if (scale == 1.0f) {
        mesh = std::make_shared<const Mesh>(Mesh::from_regular_solid(solid));
    } else {
        // reuse the unit mesh topology
        auto unit = get_mesh(solid, 1.0f);
        mesh = std::make_shared<const Mesh>(unit->scaled(scale));
    }
    mesh_cache_[key] = mesh;
    return mesh;
}

BodyId World::create_polytope(RegularSolid solid, const Vec4& pos, float scale, float mass, bool stationary) {
    Body b;
    b.collider = Collider::polytope(get_mesh(solid, scale), solid);
    b.pos = pos;
    b.dynamic_mass = mass;
    b.stationary = stationary;
    b.recompute_mass_properties();
    return add_body(std::move(b));
}

BodyId World::create_hypersphere(const Vec4& pos, float radius, float mass, bool stationary) {
    Body b;
    b.collider = Collider::sphere(radius);
    b.pos = pos;
    b.dynamic_mass = mass;
    b.stationary = stationary;
    b.recompute_mass_properties();
    return add_body(std::move(b));
}

BodyId World::create_half_space(const Vec4& point, const Vec4& normal) {
    Body b;
    b.collider = Collider::half_space(normal);
    b.pos = point;
    b.stationary = true;
    b.mass = 0.0f;
    b.moment_inertia_scalar = 0.0f;
    return add_body(std::move(b));
}

void World::create_arena(float h, const Material& m) {
    auto wall = [&](const Vec4& p, const Vec4& n) {
        BodyId id = create_half_space(p, n);
        get(id)->material = m;
    };
    wall(Vec4(0, 0, 0, 0), Vec4(0, 1, 0, 0));  // floor
    wall(Vec4(-h, 0, 0, 0), Vec4(1, 0, 0, 0));
    wall(Vec4(h, 0, 0, 0), Vec4(-1, 0, 0, 0));
    wall(Vec4(0, 0, -h, 0), Vec4(0, 0, 1, 0));
    wall(Vec4(0, 0, h, 0), Vec4(0, 0, -1, 0));
    wall(Vec4(0, 0, 0, -h), Vec4(0, 0, 0, 1));
    wall(Vec4(0, 0, 0, h), Vec4(0, 0, 0, -1));
}

void World::update(float dt) {
    struct Pair {
        size_t ia, ib;
        CollisionManifold manifold;
    };
    std::vector<Pair> solid_pairs;
    std::vector<Pair> sensor_pairs;
    std::unordered_map<size_t, int> mass_adjust;

    const size_t n = bodies_.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const Body& a = bodies_[i];
            const Body& b = bodies_[j];
            if (a.stationary && b.stationary) continue;

            auto manifold = collision_.detect_collisions(a.id, b.id, a, b);
            if (!manifold || manifold->contacts.empty()) continue;

            if (a.sensor || b.sensor) {
                sensor_pairs.push_back({i, j, std::move(*manifold)});
            } else {
                mass_adjust[i] += 1;
                mass_adjust[j] += 1;
                solid_pairs.push_back({i, j, std::move(*manifold)});
            }
        }
    }

    // build constraints
    std::vector<CollisionConstraint> constraints;
    constraints.reserve(solid_pairs.size());
    for (const Pair& p : solid_pairs) {
        constraints.emplace_back(p.manifold, bodies_[p.ia], static_cast<float>(mass_adjust[p.ia]),
                                 bodies_[p.ib], static_cast<float>(mass_adjust[p.ib]));
    }

    for (int it = 0; it < solver_iterations; ++it) {
        for (size_t k = 0; k < constraints.size(); ++k) {
            constraints[k].solve(bodies_[solid_pairs[k].ia], bodies_[solid_pairs[k].ib]);
        }
    }

    // ---- events + debug info ----
    last_contacts_.clear();
    std::unordered_set<uint64_t> cur_collisions, cur_triggers;
    for (size_t k = 0; k < solid_pairs.size(); ++k) {
        const Pair& p = solid_pairs[k];
        const BodyId ida = bodies_[p.ia].id, idb = bodies_[p.ib].id;
        const uint64_t key = unordered_pair(ida, idb);
        cur_collisions.insert(key);
        float impulse = 0.0f;
        for (const ContactState& cs : constraints[k].contacts) impulse += cs.normal_impulse;
        if (!prev_collisions_.count(key))
            events_.push_back({WorldEventType::CollisionEnter, ida, idb, p.manifold.normal, impulse});
        last_contacts_.push_back({ida, idb, p.manifold.normal, p.manifold.depth, p.manifold.contacts});
    }
    for (const Pair& p : sensor_pairs) {
        const BodyId ida = bodies_[p.ia].id, idb = bodies_[p.ib].id;
        const uint64_t key = unordered_pair(ida, idb);
        cur_triggers.insert(key);
        if (!prev_triggers_.count(key)) {
            // report the sensor first
            if (bodies_[p.ia].sensor)
                events_.push_back({WorldEventType::TriggerEnter, ida, idb, p.manifold.normal, 0.0f});
            else
                events_.push_back({WorldEventType::TriggerEnter, idb, ida, -p.manifold.normal, 0.0f});
        }
        last_contacts_.push_back({ida, idb, p.manifold.normal, p.manifold.depth, p.manifold.contacts});
    }
    for (uint64_t key : prev_collisions_) {
        if (!cur_collisions.count(key))
            events_.push_back({WorldEventType::CollisionExit, static_cast<BodyId>(key >> 32),
                               static_cast<BodyId>(key & 0xffffffffu), Vec4::zero(), 0.0f});
    }
    for (uint64_t key : prev_triggers_) {
        if (!cur_triggers.count(key)) {
            const BodyId x = static_cast<BodyId>(key >> 32), y = static_cast<BodyId>(key & 0xffffffffu);
            const Body* bx = get(x);
            const bool x_is_sensor = bx && bx->sensor;
            events_.push_back({WorldEventType::TriggerExit, x_is_sensor ? x : y, x_is_sensor ? y : x,
                               Vec4::zero(), 0.0f});
        }
    }
    prev_collisions_ = std::move(cur_collisions);
    prev_triggers_ = std::move(cur_triggers);

    for (Body& b : bodies_) b.step(dt, gravity);
}

std::vector<WorldEvent> World::take_events() {
    std::vector<WorldEvent> out;
    out.swap(events_);
    return out;
}

RayHit World::raycast(const Vec4& origin, const Vec4& dir_in, float max_t) const {
    RayHit best;
    best.t = max_t;
    const Vec4 dir = dir_in.normalized_or_zero();
    if (dir.length2() == 0.0f) return best;

    for (const Body& b : bodies_) {
        if (b.sensor) continue;
        auto t = b.ray_intersect(origin, dir);
        if (!t || *t > best.t) continue;
        best.hit = true;
        best.body = b.id;
        best.t = *t;
        best.point = origin + dir * *t;
        switch (b.collider.type) {
            case ColliderType::HalfSpace: best.normal = b.collider.normal; break;
            case ColliderType::Sphere: best.normal = (best.point - b.pos).normalized_or_zero(); break;
            case ColliderType::Mesh: {
                const Mesh& m = *b.collider.mesh;
                const Vec4 p = b.world_pos_to_body(best.point);
                float best_gap = INF;
                int best_cell = 0;
                for (size_t c = 0; c < m.cells.size(); ++c) {
                    const float gap = std::fabs(m.cell_representative_vertex(m.cells[c]).dot(m.cells[c].normal) -
                                                p.dot(m.cells[c].normal));
                    if (gap < best_gap) { best_gap = gap; best_cell = static_cast<int>(c); }
                }
                best.normal = b.body_vec_to_world(m.cells[best_cell].normal);
                break;
            }
        }
    }
    return best;
}

std::vector<BodyId> World::overlap_sphere(const Vec4& center, float radius) const {
    std::vector<BodyId> out;
    for (const Body& b : bodies_) {
        bool hit = false;
        switch (b.collider.type) {
            case ColliderType::HalfSpace: hit = (center - b.pos).dot(b.collider.normal) < radius; break;
            case ColliderType::Sphere: hit = (center - b.pos).length() < radius + b.collider.radius; break;
            case ColliderType::Mesh: {
                if ((center - b.pos).length() > radius + b.collider.mesh->radius) break;
                const Vec4 local = b.world_pos_to_body(center);
                const Vec4 closest = b.collider.mesh->closest_point_to(local);
                hit = (closest - local).length() < radius;
                break;
            }
        }
        if (hit) out.push_back(b.id);
    }
    return out;
}

}  // namespace hv4d
