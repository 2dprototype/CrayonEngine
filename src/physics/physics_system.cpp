#include "physics_system.hpp"
#include "../core/log.hpp"
#include "../graphics/mesh3d.hpp"

// Jolt configuration defines matching libJolt.a
#ifndef JPH_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER
#endif
#ifndef JPH_PROFILE_ENABLED
#define JPH_PROFILE_ENABLED
#endif
#ifndef JPH_OBJECT_STREAM
#define JPH_OBJECT_STREAM
#endif

// Jolt includes
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>

#include <glm/gtc/matrix_transform.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <thread>

namespace crayon {

namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
}

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

struct PhysicsSystem::Impl {
    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator;
    std::unique_ptr<JPH::JobSystemThreadPool> job_system;
    BPLayerInterfaceImpl bp_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl obj_vs_bp_filter;
    ObjectLayerPairFilterImpl obj_pair_filter;
    JPH::PhysicsSystem physics_system;
    std::unordered_set<uint32_t> alive_bodies;
    std::unordered_map<uint32_t, JPH::Ref<JPH::TwoBodyConstraint>> constraints;
    uint32_t next_constraint_id = 1;

    bool initialized = false;
};

PhysicsSystem::PhysicsSystem() : m_impl(std::make_unique<Impl>()) {}
PhysicsSystem::~PhysicsSystem() { shutdown(); }

bool PhysicsSystem::init(const PhysicsConfig& config) {
    if (m_impl->initialized) return true;

    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_impl->temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(static_cast<JPH::uint>(config.temp_allocator_size));
    
    int threads = config.num_worker_threads;
    if (threads <= 0) {
        threads = static_cast<int>(std::thread::hardware_concurrency());
        if (threads > 1) threads -= 1;
        if (threads <= 0) threads = 1;
    }
    m_impl->job_system = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threads);

    m_impl->physics_system.Init(
        config.max_bodies,
        config.num_body_mutexes,
        config.max_body_pairs,
        config.max_contact_constraints,
        m_impl->bp_layer_interface,
        m_impl->obj_vs_bp_filter,
        m_impl->obj_pair_filter
    );

    m_impl->physics_system.SetGravity(JPH::Vec3(config.gravity.x, config.gravity.y, config.gravity.z));
    m_impl->initialized = true;

    CRAYON_LOG_INFO("Jolt Physics 3D System initialized (threads: {})", threads);
    return true;
}

void PhysicsSystem::shutdown() {
    if (!m_impl || !m_impl->initialized) return;

    destroy_all_bodies();

    m_impl->job_system.reset();
    m_impl->temp_allocator.reset();

    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    m_impl->initialized = false;
    CRAYON_LOG_INFO("Jolt Physics 3D System shutdown");
}

void PhysicsSystem::update(float dt, int collision_steps) {
    if (!m_impl->initialized) return;
    m_impl->physics_system.Update(dt, collision_steps, m_impl->temp_allocator.get(), m_impl->job_system.get());
}

void PhysicsSystem::set_gravity(const glm::vec3& gravity) {
    if (!m_impl->initialized) return;
    m_impl->physics_system.SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
}

glm::vec3 PhysicsSystem::get_gravity() const {
    if (!m_impl->initialized) return glm::vec3(0.0f);
    JPH::Vec3 g = m_impl->physics_system.GetGravity();
    return glm::vec3(g.GetX(), g.GetY(), g.GetZ());
}

static JPH::EMotionType to_jolt_motion(MotionType motion) {
    switch (motion) {
        case MotionType::Static: return JPH::EMotionType::Static;
        case MotionType::Kinematic: return JPH::EMotionType::Kinematic;
        case MotionType::Dynamic: return JPH::EMotionType::Dynamic;
    }
    return JPH::EMotionType::Dynamic;
}

static JPH::ObjectLayer to_jolt_layer(MotionType motion) {
    return (motion == MotionType::Static) ? Layers::NON_MOVING : Layers::MOVING;
}

uint32_t PhysicsSystem::create_box(const glm::vec3& pos, const glm::vec3& half_extent, MotionType motion, float friction, float restitution, float density) {
    if (!m_impl->initialized) return 0;

    JPH::BoxShapeSettings shape_settings(JPH::Vec3(half_extent.x, half_extent.y, half_extent.z));
    shape_settings.mDensity = density;
    JPH::ShapeSettings::ShapeResult result = shape_settings.Create();
    if (result.HasError()) {
        CRAYON_LOG_ERROR("Jolt create_box error: {}", result.GetError().c_str());
        return 0;
    }

    JPH::BodyCreationSettings settings(
        result.Get(),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        to_jolt_motion(motion),
        to_jolt_layer(motion)
    );
    settings.mFriction = friction;
    settings.mRestitution = restitution;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (!body) return 0;

    JPH::BodyID id = body->GetID();
    bi.AddBody(id, (motion == MotionType::Static) ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    uint32_t raw_id = id.GetIndexAndSequenceNumber();
    m_impl->alive_bodies.insert(raw_id);
    return raw_id;
}

uint32_t PhysicsSystem::create_sphere(const glm::vec3& pos, float radius, MotionType motion, float friction, float restitution, float density) {
    if (!m_impl->initialized) return 0;

    JPH::SphereShapeSettings shape_settings(radius);
    shape_settings.mDensity = density;
    JPH::ShapeSettings::ShapeResult result = shape_settings.Create();
    if (result.HasError()) {
        CRAYON_LOG_ERROR("Jolt create_sphere error: {}", result.GetError().c_str());
        return 0;
    }

    JPH::BodyCreationSettings settings(
        result.Get(),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        to_jolt_motion(motion),
        to_jolt_layer(motion)
    );
    settings.mFriction = friction;
    settings.mRestitution = restitution;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (!body) return 0;

    JPH::BodyID id = body->GetID();
    bi.AddBody(id, (motion == MotionType::Static) ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    uint32_t raw_id = id.GetIndexAndSequenceNumber();
    m_impl->alive_bodies.insert(raw_id);
    return raw_id;
}

uint32_t PhysicsSystem::create_capsule(const glm::vec3& pos, float half_height, float radius, MotionType motion, float friction, float restitution, float density) {
    if (!m_impl->initialized) return 0;

    JPH::CapsuleShapeSettings shape_settings(half_height, radius);
    shape_settings.mDensity = density;
    JPH::ShapeSettings::ShapeResult result = shape_settings.Create();
    if (result.HasError()) {
        CRAYON_LOG_ERROR("Jolt create_capsule error: {}", result.GetError().c_str());
        return 0;
    }

    JPH::BodyCreationSettings settings(
        result.Get(),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        to_jolt_motion(motion),
        to_jolt_layer(motion)
    );
    settings.mFriction = friction;
    settings.mRestitution = restitution;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (!body) return 0;

    JPH::BodyID id = body->GetID();
    bi.AddBody(id, (motion == MotionType::Static) ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    uint32_t raw_id = id.GetIndexAndSequenceNumber();
    m_impl->alive_bodies.insert(raw_id);
    return raw_id;
}

uint32_t PhysicsSystem::create_cylinder(const glm::vec3& pos, float half_height, float radius, MotionType motion, float friction, float restitution, float density) {
    if (!m_impl->initialized) return 0;

    JPH::CylinderShapeSettings shape_settings(half_height, radius);
    shape_settings.mDensity = density;
    JPH::ShapeSettings::ShapeResult result = shape_settings.Create();
    if (result.HasError()) {
        CRAYON_LOG_ERROR("Jolt create_cylinder error: {}", result.GetError().c_str());
        return 0;
    }

    JPH::BodyCreationSettings settings(
        result.Get(),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        to_jolt_motion(motion),
        to_jolt_layer(motion)
    );
    settings.mFriction = friction;
    settings.mRestitution = restitution;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (!body) return 0;

    JPH::BodyID id = body->GetID();
    bi.AddBody(id, (motion == MotionType::Static) ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    uint32_t raw_id = id.GetIndexAndSequenceNumber();
    m_impl->alive_bodies.insert(raw_id);
    return raw_id;
}

uint32_t PhysicsSystem::create_plane(const glm::vec3& pos, const glm::vec3& normal, float half_extent) {
    if (!m_impl->initialized) return 0;

    // A static thin box serves as a robust ground plane
    JPH::BoxShapeSettings shape_settings(JPH::Vec3(half_extent, 0.5f, half_extent));
    JPH::ShapeSettings::ShapeResult result = shape_settings.Create();
    if (result.HasError()) return 0;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::quat rot = glm::rotation(up, glm::normalize(normal));

    JPH::BodyCreationSettings settings(
        result.Get(),
        JPH::RVec3(pos.x, pos.y - 0.5f, pos.z),
        JPH::Quat(rot.x, rot.y, rot.z, rot.w),
        JPH::EMotionType::Static,
        Layers::NON_MOVING
    );
    settings.mFriction = 0.8f;
    settings.mRestitution = 0.2f;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (!body) return 0;

    JPH::BodyID id = body->GetID();
    bi.AddBody(id, JPH::EActivation::DontActivate);
    uint32_t raw_id = id.GetIndexAndSequenceNumber();
    m_impl->alive_bodies.insert(raw_id);
    return raw_id;
}

bool PhysicsSystem::destroy_body(uint32_t body_id) {
    if (!m_impl->initialized) return false;
    JPH::BodyID id(body_id);
    auto& bi = m_impl->physics_system.GetBodyInterface();
    if (bi.IsAdded(id)) {
        // Remove any constraints attached to this body
        std::vector<uint32_t> to_remove;
        for (auto& pair : m_impl->constraints) {
            JPH::Body* b1 = pair.second->GetBody1();
            JPH::Body* b2 = pair.second->GetBody2();
            if ((b1 && b1->GetID() == id) || (b2 && b2->GetID() == id)) {
                m_impl->physics_system.RemoveConstraint(pair.second.GetPtr());
                to_remove.push_back(pair.first);
            }
        }
        for (uint32_t cid : to_remove) {
            m_impl->constraints.erase(cid);
        }

        bi.RemoveBody(id);
        bi.DestroyBody(id);
        m_impl->alive_bodies.erase(body_id);
        return true;
    }
    return false;
}

void PhysicsSystem::destroy_all_bodies() {
    if (!m_impl->initialized) return;

    // Remove all constraints first
    for (auto& pair : m_impl->constraints) {
        m_impl->physics_system.RemoveConstraint(pair.second.GetPtr());
    }
    m_impl->constraints.clear();

    auto& bi = m_impl->physics_system.GetBodyInterface();
    for (uint32_t raw_id : m_impl->alive_bodies) {
        JPH::BodyID id(raw_id);
        if (bi.IsAdded(id)) {
            bi.RemoveBody(id);
            bi.DestroyBody(id);
        }
    }
    m_impl->alive_bodies.clear();
}

bool PhysicsSystem::is_body_valid(uint32_t body_id) const {
    if (!m_impl->initialized) return false;
    return m_impl->alive_bodies.count(body_id) > 0;
}

bool PhysicsSystem::is_body_active(uint32_t body_id) const {
    if (!m_impl->initialized) return false;
    JPH::BodyID id(body_id);
    return m_impl->physics_system.GetBodyInterface().IsActive(id);
}

void PhysicsSystem::activate_body(uint32_t body_id) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().ActivateBody(id);
}

void PhysicsSystem::deactivate_body(uint32_t body_id) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().DeactivateBody(id);
}

glm::vec3 PhysicsSystem::get_position(uint32_t body_id) const {
    if (!m_impl->initialized) return glm::vec3(0.0f);
    JPH::BodyID id(body_id);
    JPH::RVec3 p = m_impl->physics_system.GetBodyInterface().GetCenterOfMassPosition(id);
    return glm::vec3(p.GetX(), p.GetY(), p.GetZ());
}

void PhysicsSystem::set_position(uint32_t body_id, const glm::vec3& pos, bool activate) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().SetPosition(id, JPH::RVec3(pos.x, pos.y, pos.z), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

glm::quat PhysicsSystem::get_rotation(uint32_t body_id) const {
    if (!m_impl->initialized) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    JPH::BodyID id(body_id);
    JPH::Quat q = m_impl->physics_system.GetBodyInterface().GetRotation(id);
    return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

void PhysicsSystem::set_rotation(uint32_t body_id, const glm::quat& rot, bool activate) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().SetRotation(id, JPH::Quat(rot.x, rot.y, rot.z, rot.w), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

glm::vec3 PhysicsSystem::get_euler_angles(uint32_t body_id) const {
    glm::quat q = get_rotation(body_id);
    return glm::degrees(glm::eulerAngles(q));
}

void PhysicsSystem::set_euler_angles(uint32_t body_id, const glm::vec3& euler_deg, bool activate) {
    glm::quat q = glm::quat(glm::radians(euler_deg));
    set_rotation(body_id, q, activate);
}

void PhysicsSystem::get_transform_matrix(uint32_t body_id, glm::mat4& out_matrix) const {
    glm::vec3 pos = get_position(body_id);
    glm::quat rot = get_rotation(body_id);
    out_matrix = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(rot);
}

glm::vec3 PhysicsSystem::get_linear_velocity(uint32_t body_id) const {
    if (!m_impl->initialized) return glm::vec3(0.0f);
    JPH::BodyID id(body_id);
    JPH::Vec3 v = m_impl->physics_system.GetBodyInterface().GetLinearVelocity(id);
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

void PhysicsSystem::set_linear_velocity(uint32_t body_id, const glm::vec3& vel) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().SetLinearVelocity(id, JPH::Vec3(vel.x, vel.y, vel.z));
}

glm::vec3 PhysicsSystem::get_angular_velocity(uint32_t body_id) const {
    if (!m_impl->initialized) return glm::vec3(0.0f);
    JPH::BodyID id(body_id);
    JPH::Vec3 w = m_impl->physics_system.GetBodyInterface().GetAngularVelocity(id);
    return glm::vec3(w.GetX(), w.GetY(), w.GetZ());
}

void PhysicsSystem::set_angular_velocity(uint32_t body_id, const glm::vec3& ang_vel) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().SetAngularVelocity(id, JPH::Vec3(ang_vel.x, ang_vel.y, ang_vel.z));
}

void PhysicsSystem::add_force(uint32_t body_id, const glm::vec3& force) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().AddForce(id, JPH::Vec3(force.x, force.y, force.z));
}

void PhysicsSystem::add_impulse(uint32_t body_id, const glm::vec3& impulse) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().AddImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
}

void PhysicsSystem::add_torque(uint32_t body_id, const glm::vec3& torque) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().AddTorque(id, JPH::Vec3(torque.x, torque.y, torque.z));
}

void PhysicsSystem::add_angular_impulse(uint32_t body_id, const glm::vec3& impulse) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().AddAngularImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
}

void PhysicsSystem::set_gravity_factor(uint32_t body_id, float factor) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().SetGravityFactor(id, factor);
}

void PhysicsSystem::set_friction(uint32_t body_id, float friction) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().SetFriction(id, friction);
}

void PhysicsSystem::set_restitution(uint32_t body_id, float restitution) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().SetRestitution(id, restitution);
}

void PhysicsSystem::set_motion_type(uint32_t body_id, MotionType motion) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().SetMotionType(id, to_jolt_motion(motion), JPH::EActivation::Activate);
}

bool PhysicsSystem::raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance, RaycastHit& out_hit) {
    if (!m_impl->initialized) return false;

    glm::vec3 norm_dir = glm::normalize(direction);
    glm::vec3 ray_vec = norm_dir * max_distance;

    JPH::RRayCast ray{ JPH::RVec3(origin.x, origin.y, origin.z), JPH::Vec3(ray_vec.x, ray_vec.y, ray_vec.z) };
    JPH::RayCastResult result;
    bool has_hit = m_impl->physics_system.GetNarrowPhaseQuery().CastRay(ray, result);
    if (has_hit) {
        out_hit.hit = true;
        out_hit.distance = result.mFraction * max_distance;
        JPH::RVec3 hit_pos = ray.GetPointOnRay(result.mFraction);
        out_hit.position = glm::vec3(hit_pos.GetX(), hit_pos.GetY(), hit_pos.GetZ());
        out_hit.body_id = result.mBodyID.GetIndexAndSequenceNumber();

        JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction));
            out_hit.normal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        }
        return true;
    }

    out_hit.hit = false;
    return false;
}

void PhysicsSystem::draw_debug(MeshRenderer3D& renderer, const glm::vec4& active_color, const glm::vec4& sleeping_color) {
    if (!m_impl->initialized) return;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    std::vector<glm::vec3> line_points;
    line_points.reserve(m_impl->alive_bodies.size() * 24);

    for (uint32_t raw_id : m_impl->alive_bodies) {
        JPH::BodyID id(raw_id);
        if (!bi.IsAdded(id)) continue;

        bool active = bi.IsActive(id);
        glm::vec4 color = active ? active_color : sleeping_color;

        JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), id);
        if (!lock.Succeeded()) continue;
        const JPH::Body& body = lock.GetBody();
        JPH::AABox aabb = body.GetWorldSpaceBounds();
        glm::vec3 min(aabb.mMin.GetX(), aabb.mMin.GetY(), aabb.mMin.GetZ());
        glm::vec3 max(aabb.mMax.GetX(), aabb.mMax.GetY(), aabb.mMax.GetZ());

        // 8 corners
        glm::vec3 p0(min.x, min.y, min.z);
        glm::vec3 p1(max.x, min.y, min.z);
        glm::vec3 p2(max.x, max.y, min.z);
        glm::vec3 p3(min.x, max.y, min.z);
        glm::vec3 p4(min.x, min.y, max.z);
        glm::vec3 p5(max.x, min.y, max.z);
        glm::vec3 p6(max.x, max.y, max.z);
        glm::vec3 p7(min.x, max.y, max.z);

        // 12 edges
        auto add_line = [&](const glm::vec3& a, const glm::vec3& b) {
            line_points.push_back(a);
            line_points.push_back(b);
        };

        add_line(p0, p1); add_line(p1, p2); add_line(p2, p3); add_line(p3, p0); // bottom
        add_line(p4, p5); add_line(p5, p6); add_line(p6, p7); add_line(p7, p4); // top
        add_line(p0, p4); add_line(p1, p5); add_line(p2, p6); add_line(p3, p7); // pillars

        if (!line_points.empty()) {
            renderer.draw_lines_3d(line_points, color);
            line_points.clear();
        }
    }
}

uint32_t PhysicsSystem::get_num_bodies() const {
    if (!m_impl->initialized) return 0;
    return static_cast<uint32_t>(m_impl->alive_bodies.size());
}

uint32_t PhysicsSystem::get_num_active_bodies() const {
    if (!m_impl->initialized) return 0;
    return m_impl->physics_system.GetNumActiveBodies(JPH::EBodyType::RigidBody);
}

uint32_t PhysicsSystem::create_point_constraint(uint32_t body1_id, uint32_t body2_id, const glm::vec3& pivot) {
    if (!m_impl->initialized) return 0;
    JPH::BodyID b1(body1_id);
    JPH::BodyID b2(body2_id);
    JPH::BodyLockWrite lock1(m_impl->physics_system.GetBodyLockInterface(), b1);
    JPH::BodyLockWrite lock2(m_impl->physics_system.GetBodyLockInterface(), b2);
    if (!lock1.Succeeded() || !lock2.Succeeded()) return 0;

    JPH::PointConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = settings.mPoint2 = JPH::RVec3(pivot.x, pivot.y, pivot.z);

    JPH::TwoBodyConstraint* constraint = settings.Create(lock1.GetBody(), lock2.GetBody());
    if (!constraint) return 0;

    m_impl->physics_system.AddConstraint(constraint);
    uint32_t cid = m_impl->next_constraint_id++;
    m_impl->constraints[cid] = constraint;
    return cid;
}

uint32_t PhysicsSystem::create_hinge_constraint(uint32_t body1_id, uint32_t body2_id, const glm::vec3& pivot, const glm::vec3& axis, float min_angle, float max_angle) {
    if (!m_impl->initialized) return 0;
    JPH::BodyID b1(body1_id);
    JPH::BodyID b2(body2_id);
    JPH::BodyLockWrite lock1(m_impl->physics_system.GetBodyLockInterface(), b1);
    JPH::BodyLockWrite lock2(m_impl->physics_system.GetBodyLockInterface(), b2);
    if (!lock1.Succeeded() || !lock2.Succeeded()) return 0;

    JPH::HingeConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = settings.mPoint2 = JPH::RVec3(pivot.x, pivot.y, pivot.z);

    glm::vec3 norm_axis = glm::normalize(axis);
    settings.mHingeAxis1 = settings.mHingeAxis2 = JPH::Vec3(norm_axis.x, norm_axis.y, norm_axis.z);

    glm::vec3 up = std::abs(norm_axis.y) > 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 normal = glm::normalize(glm::cross(norm_axis, up));
    settings.mNormalAxis1 = settings.mNormalAxis2 = JPH::Vec3(normal.x, normal.y, normal.z);

    settings.mLimitsMin = min_angle;
    settings.mLimitsMax = max_angle;

    JPH::TwoBodyConstraint* constraint = settings.Create(lock1.GetBody(), lock2.GetBody());
    if (!constraint) return 0;

    m_impl->physics_system.AddConstraint(constraint);
    uint32_t cid = m_impl->next_constraint_id++;
    m_impl->constraints[cid] = constraint;
    return cid;
}

uint32_t PhysicsSystem::create_distance_constraint(uint32_t body1_id, uint32_t body2_id, const glm::vec3& p1, const glm::vec3& p2, float min_dist, float max_dist) {
    if (!m_impl->initialized) return 0;
    JPH::BodyID b1(body1_id);
    JPH::BodyID b2(body2_id);
    JPH::BodyLockWrite lock1(m_impl->physics_system.GetBodyLockInterface(), b1);
    JPH::BodyLockWrite lock2(m_impl->physics_system.GetBodyLockInterface(), b2);
    if (!lock1.Succeeded() || !lock2.Succeeded()) return 0;

    JPH::DistanceConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = JPH::RVec3(p1.x, p1.y, p1.z);
    settings.mPoint2 = JPH::RVec3(p2.x, p2.y, p2.z);
    settings.mMinDistance = min_dist;
    settings.mMaxDistance = max_dist;

    JPH::TwoBodyConstraint* constraint = settings.Create(lock1.GetBody(), lock2.GetBody());
    if (!constraint) return 0;

    m_impl->physics_system.AddConstraint(constraint);
    uint32_t cid = m_impl->next_constraint_id++;
    m_impl->constraints[cid] = constraint;
    return cid;
}

uint32_t PhysicsSystem::create_fixed_constraint(uint32_t body1_id, uint32_t body2_id) {
    if (!m_impl->initialized) return 0;
    JPH::BodyID b1(body1_id);
    JPH::BodyID b2(body2_id);
    JPH::BodyLockWrite lock1(m_impl->physics_system.GetBodyLockInterface(), b1);
    JPH::BodyLockWrite lock2(m_impl->physics_system.GetBodyLockInterface(), b2);
    if (!lock1.Succeeded() || !lock2.Succeeded()) return 0;

    JPH::FixedConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mAutoDetectPoint = true;

    JPH::TwoBodyConstraint* constraint = settings.Create(lock1.GetBody(), lock2.GetBody());
    if (!constraint) return 0;

    m_impl->physics_system.AddConstraint(constraint);
    uint32_t cid = m_impl->next_constraint_id++;
    m_impl->constraints[cid] = constraint;
    return cid;
}

bool PhysicsSystem::destroy_constraint(uint32_t constraint_id) {
    if (!m_impl->initialized) return false;
    auto it = m_impl->constraints.find(constraint_id);
    if (it != m_impl->constraints.end()) {
        m_impl->physics_system.RemoveConstraint(it->second.GetPtr());
        m_impl->constraints.erase(it);
        return true;
    }
    return false;
}

void PhysicsSystem::set_is_sensor(uint32_t body_id, bool is_sensor) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    m_impl->physics_system.GetBodyInterface().SetIsSensor(id, is_sensor);
}

bool PhysicsSystem::is_sensor(uint32_t body_id) const {
    if (!m_impl->initialized) return false;
    JPH::BodyID id(body_id);
    return m_impl->physics_system.GetBodyInterface().IsSensor(id);
}

void PhysicsSystem::set_damping(uint32_t body_id, float linear_damping, float angular_damping) {
    if (!m_impl->initialized) return;
    JPH::BodyID id(body_id);
    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), id);
    if (lock.Succeeded()) {
        JPH::MotionProperties* mp = lock.GetBody().GetMotionProperties();
        if (mp) {
            mp->SetLinearDamping(linear_damping);
            mp->SetAngularDamping(angular_damping);
        }
    }
}

std::vector<uint32_t> PhysicsSystem::overlap_sphere(const glm::vec3& center, float radius) {
    std::vector<uint32_t> result;
    if (!m_impl->initialized) return result;

    JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
    m_impl->physics_system.GetBroadPhaseQuery().CollideSphere(
        JPH::Vec3(center.x, center.y, center.z),
        radius,
        collector
    );

    result.reserve(collector.mHits.size());
    for (const JPH::BodyID& id : collector.mHits) {
        result.push_back(id.GetIndexAndSequenceNumber());
    }
    return result;
}

} // namespace crayon
