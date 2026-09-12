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
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/Character.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Skeleton/Skeleton.h>
#include <Jolt/Skeleton/SkeletonPose.h>
#include <Jolt/Skeleton/SkeletonMapper.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/Vehicle/TrackedVehicleController.h>
#include <Jolt/Physics/Vehicle/MotorcycleController.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>

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

static inline JPH::Vec3 to_jolt_vec3(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
static inline JPH::RVec3 to_jolt_rvec3(const glm::vec3& v) { return JPH::RVec3(v.x, v.y, v.z); }
static inline glm::vec3 to_glm_vec3(const JPH::Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }

static inline JPH::Quat to_jolt_quat(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
static inline glm::quat to_glm_quat(const JPH::Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

static inline glm::mat4 to_glm_mat4(const JPH::Mat44& m) {
    glm::mat4 r;
    for (int col = 0; col < 4; ++col) {
        JPH::Vec4 c = m.GetColumn4(col);
        r[col][0] = c.GetX();
        r[col][1] = c.GetY();
        r[col][2] = c.GetZ();
        r[col][3] = c.GetW();
    }
    return r;
}

static inline JPH::Mat44 to_jolt_mat4(const glm::mat4& m) {
    JPH::Mat44 r;
    for (int col = 0; col < 4; ++col) {
        r.SetColumn4(col, JPH::Vec4(m[col][0], m[col][1], m[col][2], m[col][3]));
    }
    return r;
}

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

    // Character simulation
    std::unordered_map<uint32_t, JPH::Ref<JPH::Character>> characters;
    std::unordered_map<uint32_t, float> character_half_heights;
    std::unordered_map<uint32_t, float> character_radii;
    uint32_t next_character_id = 1;

    // Virtual Characters
    std::unordered_map<uint32_t, JPH::Ref<JPH::CharacterVirtual>> virtual_characters;
    std::unordered_map<uint32_t, float> virtual_char_half_heights;
    std::unordered_map<uint32_t, float> virtual_char_radii;
    std::unordered_map<uint32_t, float> virtual_char_step_heights;
    uint32_t next_vchar_id = 1;

    // Vehicles
    enum class VehicleType { Wheeled, Tracked, Motorcycle };
    struct VehicleRecord {
        VehicleType type;
        JPH::Ref<JPH::VehicleConstraint> constraint;
        uint32_t chassis_body_id = 0;
        std::vector<float> wheel_radii;
        std::vector<float> wheel_widths;
    };
    std::unordered_map<uint32_t, VehicleRecord> vehicles;
    uint32_t next_vehicle_id = 1;

    // Skeletons, Poses, Mappers, Ragdolls
    std::unordered_map<uint32_t, JPH::Ref<JPH::Skeleton>> skeletons;
    uint32_t next_skeleton_id = 1;

    std::unordered_map<uint32_t, std::unique_ptr<JPH::SkeletonPose>> skeleton_poses;
    uint32_t next_pose_id = 1;

    std::unordered_map<uint32_t, JPH::Ref<JPH::SkeletonMapper>> skeleton_mappers;
    uint32_t next_mapper_id = 1;

    struct RagdollRecord {
        JPH::Ref<JPH::Ragdoll> ragdoll;
        JPH::Ref<JPH::RagdollSettings> settings;
        uint32_t skeleton_id = 0;
        bool is_hard_keyed = false;
        std::vector<RagdollPartConfig> parts;
    };
    std::unordered_map<uint32_t, RagdollRecord> ragdolls;
    uint32_t next_ragdoll_id = 1;

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

    for (auto& [id, ch] : m_impl->characters) {
        if (ch) ch->PostSimulation(0.05f);
    }
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
    if (!m_impl || !m_impl->initialized) return;

    // Remove vehicles
    for (auto& [id, v] : m_impl->vehicles) {
        if (v.constraint) {
            m_impl->physics_system.RemoveStepListener(v.constraint.GetPtr());
            m_impl->physics_system.RemoveConstraint(v.constraint.GetPtr());
        }
    }
    m_impl->vehicles.clear();

    // Remove characters
    for (auto& [id, ch] : m_impl->characters) {
        if (ch) ch->RemoveFromPhysicsSystem();
    }
    m_impl->characters.clear();
    m_impl->character_half_heights.clear();
    m_impl->character_radii.clear();

    // Remove virtual characters
    m_impl->virtual_characters.clear();
    m_impl->virtual_char_half_heights.clear();
    m_impl->virtual_char_radii.clear();
    m_impl->virtual_char_step_heights.clear();

    // Remove ragdolls
    for (auto& [id, r] : m_impl->ragdolls) {
        if (r.ragdoll) r.ragdoll->RemoveFromPhysicsSystem();
    }
    m_impl->ragdolls.clear();

    // Remove all constraints
    for (auto& pair : m_impl->constraints) {
        m_impl->physics_system.RemoveConstraint(pair.second.GetPtr());
    }
    m_impl->constraints.clear();

    // Remove all rigid bodies
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

    // 2. Draw Characters
    for (const auto& [id, ch] : m_impl->characters) {
        if (!ch) continue;
        JPH::RVec3 cpos;
        JPH::Quat crot;
        ch->GetPositionAndRotation(cpos, crot);
        glm::vec3 pos = to_glm_vec3(cpos);
        glm::quat q = to_glm_quat(crot);
        glm::vec3 euler = glm::eulerAngles(q);
        auto it_h = m_impl->character_half_heights.find(id);
        auto it_r = m_impl->character_radii.find(id);
        float rh = (it_h != m_impl->character_half_heights.end()) ? it_h->second : 0.6f;
        float rr = (it_r != m_impl->character_radii.end()) ? it_r->second : 0.4f;
        renderer.draw_capsule_wires(pos, rr, rh, glm::vec4(0.3f, 0.8f, 1.0f, 1.0f), euler);
        if (ch->IsSupported()) {
            renderer.draw_ray_3d(to_glm_vec3(ch->GetGroundPosition()), to_glm_vec3(ch->GetGroundNormal()), 0.5f, glm::vec4(1.0f, 1.0f, 0.2f, 1.0f));
        }
    }

    // 3. Draw Virtual Characters
    for (const auto& [id, vch] : m_impl->virtual_characters) {
        if (!vch) continue;
        JPH::RVec3 cpos = vch->GetPosition();
        JPH::Quat crot = vch->GetRotation();
        glm::vec3 pos = to_glm_vec3(cpos);
        glm::quat q = to_glm_quat(crot);
        glm::vec3 euler = glm::eulerAngles(q);
        auto it_h = m_impl->virtual_char_half_heights.find(id);
        auto it_r = m_impl->virtual_char_radii.find(id);
        float rh = (it_h != m_impl->virtual_char_half_heights.end()) ? it_h->second : 0.6f;
        float rr = (it_r != m_impl->virtual_char_radii.end()) ? it_r->second : 0.4f;
        renderer.draw_capsule_wires(pos, rr, rh, glm::vec4(0.9f, 0.4f, 1.0f, 1.0f), euler);
        if (vch->IsSupported()) {
            renderer.draw_ray_3d(to_glm_vec3(vch->GetGroundPosition()), to_glm_vec3(vch->GetGroundNormal()), 0.5f, glm::vec4(1.0f, 1.0f, 0.2f, 1.0f));
        }
    }

    // 4. Draw Vehicles
    for (const auto& [id, v] : m_impl->vehicles) {
        if (!v.constraint) continue;
        size_t num_wheels = v.constraint->GetWheels().size();
        for (size_t w = 0; w < num_wheels; ++w) {
            JPH::RMat44 wt = v.constraint->GetWheelWorldTransform(static_cast<JPH::uint>(w), JPH::Vec3::sAxisX(), JPH::Vec3::sAxisY());
            glm::mat4 gwt = to_glm_mat4(wt);
            glm::vec3 wpos = glm::vec3(gwt[3]);
            glm::quat wrot = glm::quat_cast(gwt);
            glm::vec3 weuler = glm::eulerAngles(wrot);
            float wr = (w < v.wheel_radii.size()) ? v.wheel_radii[w] : 0.3f;
            float ww = (w < v.wheel_widths.size()) ? v.wheel_widths[w] : 0.15f;
            renderer.draw_cylinder_wires(wpos, wr, ww * 0.5f, glm::vec4(1.0f, 0.6f, 0.1f, 1.0f), weuler);
        }
    }

    // 5. Draw Ragdolls
    for (const auto& [id, r] : m_impl->ragdolls) {
        if (!r.ragdoll) continue;
        std::vector<glm::vec3> joint_positions;
        std::vector<std::pair<int, int>> connections;
        size_t body_count = r.ragdoll->GetBodyCount();
        joint_positions.resize(body_count);
        for (size_t b = 0; b < body_count; ++b) {
            JPH::BodyID bid = r.ragdoll->GetBodyID(static_cast<int>(b));
            joint_positions[b] = to_glm_vec3(bi.GetPosition(bid));
            if (b < r.parts.size() && r.parts[b].parent_joint_index >= 0 && r.parts[b].parent_joint_index < (int)body_count) {
                connections.emplace_back(r.parts[b].parent_joint_index, static_cast<int>(b));
            }
        }
        renderer.draw_skeleton_3d(joint_positions, connections, glm::vec4(0.2f, 1.0f, 0.9f, 1.0f));
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

// ============================================================================
// Game Character Simulation (Rigid Body Character)
// ============================================================================

uint32_t PhysicsSystem::create_character(const CharacterConfig& config) {
    if (!m_impl->initialized) return 0;

    JPH::CharacterSettings settings;
    settings.mShape = new JPH::CapsuleShape(config.half_height, config.radius);
    settings.mLayer = to_jolt_layer(config.motion);
    settings.mMass = config.mass;
    settings.mFriction = config.friction;
    settings.mGravityFactor = config.gravity_factor;
    settings.mMaxSlopeAngle = glm::radians(config.max_slope_angle_deg);

    auto* character = new JPH::Character(
        &settings,
        to_jolt_rvec3(config.pos),
        JPH::Quat::sIdentity(),
        0,
        &m_impl->physics_system
    );
    character->AddToPhysicsSystem((config.motion == MotionType::Static) ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);

    uint32_t cid = m_impl->next_character_id++;
    m_impl->characters[cid] = character;
    m_impl->character_half_heights[cid] = config.half_height;
    m_impl->character_radii[cid] = config.radius;
    return cid;
}

bool PhysicsSystem::destroy_character(uint32_t id) {
    if (!m_impl->initialized) return false;
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end()) {
        if (it->second) it->second->RemoveFromPhysicsSystem();
        m_impl->characters.erase(it);
        m_impl->character_half_heights.erase(id);
        m_impl->character_radii.erase(id);
        return true;
    }
    return false;
}

void PhysicsSystem::character_set_linear_velocity(uint32_t id, const glm::vec3& vel) {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        it->second->SetLinearVelocity(to_jolt_vec3(vel));
    }
}

glm::vec3 PhysicsSystem::character_get_linear_velocity(uint32_t id) const {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        return to_glm_vec3(it->second->GetLinearVelocity());
    }
    return glm::vec3(0.0f);
}

void PhysicsSystem::character_set_position(uint32_t id, const glm::vec3& pos) {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        it->second->SetPosition(to_jolt_rvec3(pos));
    }
}

glm::vec3 PhysicsSystem::character_get_position(uint32_t id) const {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        return to_glm_vec3(it->second->GetPosition());
    }
    return glm::vec3(0.0f);
}

void PhysicsSystem::character_set_rotation(uint32_t id, const glm::quat& rot) {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        it->second->SetRotation(to_jolt_quat(rot));
    }
}

glm::quat PhysicsSystem::character_get_rotation(uint32_t id) const {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        return to_glm_quat(it->second->GetRotation());
    }
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

bool PhysicsSystem::character_is_supported(uint32_t id) const {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        return it->second->IsSupported();
    }
    return false;
}

PhysicsSystem::GroundState PhysicsSystem::character_get_ground_state(uint32_t id) const {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        switch (it->second->GetGroundState()) {
            case JPH::CharacterBase::EGroundState::OnGround: return GroundState::OnGround;
            case JPH::CharacterBase::EGroundState::OnSteepGround: return GroundState::OnSteepGround;
            case JPH::CharacterBase::EGroundState::NotSupported: return GroundState::NotSupported;
            case JPH::CharacterBase::EGroundState::InAir: return GroundState::InAir;
        }
    }
    return GroundState::InAir;
}

glm::vec3 PhysicsSystem::character_get_ground_normal(uint32_t id) const {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        return to_glm_vec3(it->second->GetGroundNormal());
    }
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 PhysicsSystem::character_get_ground_velocity(uint32_t id) const {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        return to_glm_vec3(it->second->GetGroundVelocity());
    }
    return glm::vec3(0.0f);
}

glm::vec3 PhysicsSystem::character_get_ground_position(uint32_t id) const {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        return to_glm_vec3(it->second->GetGroundPosition());
    }
    return glm::vec3(0.0f);
}

uint32_t PhysicsSystem::character_get_body_id(uint32_t id) const {
    auto it = m_impl->characters.find(id);
    if (it != m_impl->characters.end() && it->second) {
        return it->second->GetBodyID().GetIndexAndSequenceNumber();
    }
    return 0;
}

// ============================================================================
// Virtual Character Simulation (Outside physics loop)
// ============================================================================

uint32_t PhysicsSystem::create_character_virtual(const CharacterVirtualConfig& config) {
    if (!m_impl->initialized) return 0;

    JPH::CharacterVirtualSettings settings;
    settings.mShape = new JPH::CapsuleShape(config.half_height, config.radius);
    settings.mMass = config.mass;
    settings.mMaxSlopeAngle = glm::radians(config.max_slope_angle_deg);
    settings.mMaxStrength = config.max_strength;
    settings.mPredictiveContactDistance = config.predictive_contact_distance;
    settings.mCharacterPadding = 0.02f;
    settings.mPenetrationRecoverySpeed = 1.0f;
    if (config.inner_body) {
        settings.mInnerBodyShape = settings.mShape;
        settings.mInnerBodyLayer = Layers::MOVING;
    }

    auto* cv = new JPH::CharacterVirtual(
        &settings,
        to_jolt_rvec3(config.pos),
        JPH::Quat::sIdentity(),
        &m_impl->physics_system
    );

    uint32_t cid = m_impl->next_vchar_id++;
    m_impl->virtual_characters[cid] = cv;
    m_impl->virtual_char_half_heights[cid] = config.half_height;
    m_impl->virtual_char_radii[cid] = config.radius;
    m_impl->virtual_char_step_heights[cid] = config.step_height;
    return cid;
}

bool PhysicsSystem::destroy_character_virtual(uint32_t id) {
    if (!m_impl->initialized) return false;
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end()) {
        m_impl->virtual_characters.erase(it);
        m_impl->virtual_char_half_heights.erase(id);
        m_impl->virtual_char_radii.erase(id);
        m_impl->virtual_char_step_heights.erase(id);
        return true;
    }
    return false;
}

void PhysicsSystem::character_virtual_update(uint32_t id, float dt) {
    if (!m_impl->initialized) return;
    auto it = m_impl->virtual_characters.find(id);
    if (it == m_impl->virtual_characters.end() || !it->second) return;

    float step_h = m_impl->virtual_char_step_heights[id];
    JPH::CharacterVirtual::ExtendedUpdateSettings ext;
    ext.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0);
    ext.mWalkStairsStepUp = JPH::Vec3(0, step_h, 0);

    it->second->ExtendedUpdate(
        dt,
        m_impl->physics_system.GetGravity(),
        ext,
        m_impl->physics_system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        m_impl->physics_system.GetDefaultLayerFilter(Layers::MOVING),
        { },
        { },
        *m_impl->temp_allocator
    );
}

void PhysicsSystem::character_virtual_set_linear_velocity(uint32_t id, const glm::vec3& vel) {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        it->second->SetLinearVelocity(to_jolt_vec3(vel));
    }
}

glm::vec3 PhysicsSystem::character_virtual_get_linear_velocity(uint32_t id) const {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        return to_glm_vec3(it->second->GetLinearVelocity());
    }
    return glm::vec3(0.0f);
}

void PhysicsSystem::character_virtual_set_position(uint32_t id, const glm::vec3& pos) {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        it->second->SetPosition(to_jolt_rvec3(pos));
    }
}

glm::vec3 PhysicsSystem::character_virtual_get_position(uint32_t id) const {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        return to_glm_vec3(it->second->GetPosition());
    }
    return glm::vec3(0.0f);
}

void PhysicsSystem::character_virtual_set_rotation(uint32_t id, const glm::quat& rot) {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        it->second->SetRotation(to_jolt_quat(rot));
    }
}

glm::quat PhysicsSystem::character_virtual_get_rotation(uint32_t id) const {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        return to_glm_quat(it->second->GetRotation());
    }
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

bool PhysicsSystem::character_virtual_is_supported(uint32_t id) const {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        return it->second->IsSupported();
    }
    return false;
}

PhysicsSystem::GroundState PhysicsSystem::character_virtual_get_ground_state(uint32_t id) const {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        switch (it->second->GetGroundState()) {
            case JPH::CharacterBase::EGroundState::OnGround: return GroundState::OnGround;
            case JPH::CharacterBase::EGroundState::OnSteepGround: return GroundState::OnSteepGround;
            case JPH::CharacterBase::EGroundState::NotSupported: return GroundState::NotSupported;
            case JPH::CharacterBase::EGroundState::InAir: return GroundState::InAir;
        }
    }
    return GroundState::InAir;
}

glm::vec3 PhysicsSystem::character_virtual_get_ground_normal(uint32_t id) const {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        return to_glm_vec3(it->second->GetGroundNormal());
    }
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 PhysicsSystem::character_virtual_get_ground_velocity(uint32_t id) const {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        return to_glm_vec3(it->second->GetGroundVelocity());
    }
    return glm::vec3(0.0f);
}

glm::vec3 PhysicsSystem::character_virtual_get_ground_position(uint32_t id) const {
    auto it = m_impl->virtual_characters.find(id);
    if (it != m_impl->virtual_characters.end() && it->second) {
        return to_glm_vec3(it->second->GetGroundPosition());
    }
    return glm::vec3(0.0f);
}

// ============================================================================
// Vehicles (Wheeled, Tracked, Motorcycle)
// ============================================================================

uint32_t PhysicsSystem::create_wheeled_vehicle(const WheeledVehicleConfig& config) {
    if (!m_impl->initialized) return 0;

    JPH::BodyID chassis_id(config.chassis_body_id);
    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), chassis_id);
    if (!lock.Succeeded()) return 0;
    JPH::Body& chassis_body = lock.GetBody();

    JPH::VehicleConstraintSettings vcs;
    vcs.mDrawConstraintSize = 0.1f;
    vcs.mMaxPitchRollAngle = config.max_pitch_roll_angle;

    auto* controller = new JPH::WheeledVehicleControllerSettings();
    controller->mEngine.mMaxTorque = config.engine_max_torque;
    controller->mEngine.mMinRPM = config.engine_min_rpm;
    controller->mEngine.mMaxRPM = config.engine_max_rpm;
    vcs.mController = controller;

    std::vector<float> radii, widths;
    for (const auto& w : config.wheels) {
        auto* wheel = new JPH::WheelSettingsWV();
        wheel->mPosition = to_jolt_vec3(w.position);
        wheel->mRadius = w.radius;
        wheel->mWidth = w.width;
        wheel->mSuspensionMinLength = w.suspension_min_length;
        wheel->mSuspensionMaxLength = w.suspension_max_length;
        wheel->mSuspensionSpring.mFrequency = std::max(0.5f, std::sqrt(w.suspension_spring / 250.0f) / 6.28f);
        wheel->mMaxSteerAngle = w.max_steer_angle_rad;
        wheel->mMaxBrakeTorque = w.max_brake_torque;
        wheel->mMaxHandBrakeTorque = w.max_hand_brake_torque;
        vcs.mWheels.push_back(wheel);
        radii.push_back(w.radius);
        widths.push_back(w.width);
    }

    auto* vehicle = new JPH::VehicleConstraint(chassis_body, vcs);
    vehicle->SetVehicleCollisionTester(new JPH::VehicleCollisionTesterRay(Layers::NON_MOVING));
    m_impl->physics_system.AddConstraint(vehicle);
    m_impl->physics_system.AddStepListener(vehicle);

    uint32_t vid = m_impl->next_vehicle_id++;
    m_impl->vehicles[vid] = { Impl::VehicleType::Wheeled, vehicle, config.chassis_body_id, radii, widths };
    return vid;
}

uint32_t PhysicsSystem::create_tracked_vehicle(const TrackedVehicleConfig& config) {
    if (!m_impl->initialized) return 0;

    JPH::BodyID chassis_id(config.chassis_body_id);
    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), chassis_id);
    if (!lock.Succeeded()) return 0;
    JPH::Body& chassis_body = lock.GetBody();

    JPH::VehicleConstraintSettings vcs;
    vcs.mDrawConstraintSize = 0.1f;

    auto* controller = new JPH::TrackedVehicleControllerSettings();
    controller->mEngine.mMaxTorque = config.engine_max_torque;
    vcs.mController = controller;

    std::vector<float> radii, widths;
    JPH::uint wheel_idx = 0;
    JPH::VehicleTrackSettings& left_track = controller->mTracks[(int)JPH::ETrackSide::Left];
    left_track.mDrivenWheel = 0;
    for (const auto& w : config.left_wheels) {
        auto* wheel = new JPH::WheelSettingsTV();
        wheel->mPosition = to_jolt_vec3(w.position);
        wheel->mRadius = w.radius;
        wheel->mWidth = w.width;
        wheel->mSuspensionMinLength = w.suspension_min_length;
        wheel->mSuspensionMaxLength = w.suspension_max_length;
        wheel->mSuspensionSpring.mFrequency = std::max(0.5f, std::sqrt(w.suspension_spring / 250.0f) / 6.28f);
        vcs.mWheels.push_back(wheel);
        left_track.mWheels.push_back(wheel_idx++);
        radii.push_back(w.radius);
        widths.push_back(w.width);
    }

    JPH::VehicleTrackSettings& right_track = controller->mTracks[(int)JPH::ETrackSide::Right];
    right_track.mDrivenWheel = wheel_idx;
    for (const auto& w : config.right_wheels) {
        auto* wheel = new JPH::WheelSettingsTV();
        wheel->mPosition = to_jolt_vec3(w.position);
        wheel->mRadius = w.radius;
        wheel->mWidth = w.width;
        wheel->mSuspensionMinLength = w.suspension_min_length;
        wheel->mSuspensionMaxLength = w.suspension_max_length;
        wheel->mSuspensionSpring.mFrequency = std::max(0.5f, std::sqrt(w.suspension_spring / 250.0f) / 6.28f);
        vcs.mWheels.push_back(wheel);
        right_track.mWheels.push_back(wheel_idx++);
        radii.push_back(w.radius);
        widths.push_back(w.width);
    }

    auto* vehicle = new JPH::VehicleConstraint(chassis_body, vcs);
    vehicle->SetVehicleCollisionTester(new JPH::VehicleCollisionTesterRay(Layers::NON_MOVING));
    m_impl->physics_system.AddConstraint(vehicle);
    m_impl->physics_system.AddStepListener(vehicle);

    uint32_t vid = m_impl->next_vehicle_id++;
    m_impl->vehicles[vid] = { Impl::VehicleType::Tracked, vehicle, config.chassis_body_id, radii, widths };
    return vid;
}

uint32_t PhysicsSystem::create_motorcycle(const MotorcycleConfig& config) {
    if (!m_impl->initialized) return 0;

    JPH::BodyID chassis_id(config.chassis_body_id);
    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), chassis_id);
    if (!lock.Succeeded()) return 0;
    JPH::Body& chassis_body = lock.GetBody();

    JPH::VehicleConstraintSettings vcs;
    vcs.mDrawConstraintSize = 0.1f;
    vcs.mMaxPitchRollAngle = config.max_lean_angle_rad;

    auto* controller = new JPH::MotorcycleControllerSettings();
    controller->mMaxLeanAngle = config.max_lean_angle_rad;
    controller->mLeanSpringConstant = config.lean_spring_constant;
    controller->mLeanSpringDamping = config.lean_spring_damping;
    controller->mLeanSmoothingFactor = config.lean_smoothing_factor;
    controller->mEngine.mMaxTorque = config.engine_max_torque;
    vcs.mController = controller;

    std::vector<float> radii, widths;
    // Front wheel
    {
        auto* wheel = new JPH::WheelSettingsWV();
        wheel->mPosition = to_jolt_vec3(config.front_wheel.position);
        wheel->mRadius = config.front_wheel.radius;
        wheel->mWidth = config.front_wheel.width;
        wheel->mSuspensionMinLength = config.front_wheel.suspension_min_length;
        wheel->mSuspensionMaxLength = config.front_wheel.suspension_max_length;
        wheel->mSuspensionSpring.mFrequency = std::max(0.5f, std::sqrt(config.front_wheel.suspension_spring / 250.0f) / 6.28f);
        wheel->mMaxSteerAngle = config.front_wheel.max_steer_angle_rad;
        wheel->mMaxBrakeTorque = config.front_wheel.max_brake_torque;
        wheel->mMaxHandBrakeTorque = 0.0f;
        vcs.mWheels.push_back(wheel);
        radii.push_back(config.front_wheel.radius);
        widths.push_back(config.front_wheel.width);
    }
    // Rear wheel
    {
        auto* wheel = new JPH::WheelSettingsWV();
        wheel->mPosition = to_jolt_vec3(config.rear_wheel.position);
        wheel->mRadius = config.rear_wheel.radius;
        wheel->mWidth = config.rear_wheel.width;
        wheel->mSuspensionMinLength = config.rear_wheel.suspension_min_length;
        wheel->mSuspensionMaxLength = config.rear_wheel.suspension_max_length;
        wheel->mSuspensionSpring.mFrequency = std::max(0.5f, std::sqrt(config.rear_wheel.suspension_spring / 250.0f) / 6.28f);
        wheel->mMaxSteerAngle = 0.0f;
        wheel->mMaxBrakeTorque = config.rear_wheel.max_brake_torque;
        wheel->mMaxHandBrakeTorque = config.rear_wheel.max_hand_brake_torque;
        vcs.mWheels.push_back(wheel);
        radii.push_back(config.rear_wheel.radius);
        widths.push_back(config.rear_wheel.width);
    }

    auto* vehicle = new JPH::VehicleConstraint(chassis_body, vcs);
    vehicle->SetVehicleCollisionTester(new JPH::VehicleCollisionTesterRay(Layers::NON_MOVING));
    m_impl->physics_system.AddConstraint(vehicle);
    m_impl->physics_system.AddStepListener(vehicle);

    uint32_t vid = m_impl->next_vehicle_id++;
    m_impl->vehicles[vid] = { Impl::VehicleType::Motorcycle, vehicle, config.chassis_body_id, radii, widths };
    return vid;
}

bool PhysicsSystem::destroy_vehicle(uint32_t id) {
    if (!m_impl->initialized) return false;
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end()) {
        if (it->second.constraint) {
            m_impl->physics_system.RemoveStepListener(it->second.constraint.GetPtr());
            m_impl->physics_system.RemoveConstraint(it->second.constraint.GetPtr());
        }
        m_impl->vehicles.erase(it);
        return true;
    }
    return false;
}

void PhysicsSystem::vehicle_set_input_wheeled(uint32_t id, float forward, float steer, float brake, bool handbrake) {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* controller = dynamic_cast<JPH::WheeledVehicleController*>(it->second.constraint->GetController());
        if (controller) {
            controller->SetDriverInput(forward, steer, brake, handbrake ? 1.0f : 0.0f);
        }
    }
}

void PhysicsSystem::vehicle_set_input_tracked(uint32_t id, float left_ratio, float right_ratio, float brake) {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* controller = dynamic_cast<JPH::TrackedVehicleController*>(it->second.constraint->GetController());
        if (controller) {
            controller->SetDriverInput(1.0f, left_ratio, right_ratio, brake);
        }
    }
}

void PhysicsSystem::vehicle_set_input_motorcycle(uint32_t id, float forward, float steer, float brake) {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* controller = dynamic_cast<JPH::MotorcycleController*>(it->second.constraint->GetController());
        if (controller) {
            controller->SetDriverInput(forward, steer, brake, 0.0f);
        }
    }
}

void PhysicsSystem::vehicle_enable_lean_controller(uint32_t id, bool enable) {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* controller = dynamic_cast<JPH::MotorcycleController*>(it->second.constraint->GetController());
        if (controller) {
            controller->EnableLeanController(enable);
        }
    }
}

bool PhysicsSystem::vehicle_is_lean_controller_enabled(uint32_t id) const {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* controller = dynamic_cast<const JPH::MotorcycleController*>(it->second.constraint->GetController());
        if (controller) {
            return controller->IsLeanControllerEnabled();
        }
    }
    return false;
}

float PhysicsSystem::vehicle_get_lean_angle(uint32_t id) const {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        glm::quat q = to_glm_quat(it->second.constraint->GetVehicleBody()->GetRotation());
        glm::vec3 euler = glm::eulerAngles(q);
        return euler.z;
    }
    return 0.0f;
}

float PhysicsSystem::vehicle_get_speed_kmh(uint32_t id) const {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        return it->second.constraint->GetVehicleBody()->GetLinearVelocity().Length() * 3.6f;
    }
    return 0.0f;
}

float PhysicsSystem::vehicle_get_engine_rpm(uint32_t id) const {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* wvc = dynamic_cast<const JPH::WheeledVehicleController*>(it->second.constraint->GetController());
        if (wvc) return wvc->GetEngine().GetCurrentRPM();
        auto* tvc = dynamic_cast<const JPH::TrackedVehicleController*>(it->second.constraint->GetController());
        if (tvc) return tvc->GetEngine().GetCurrentRPM();
    }
    return 0.0f;
}

int PhysicsSystem::vehicle_get_transmission_gear(uint32_t id) const {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* wvc = dynamic_cast<const JPH::WheeledVehicleController*>(it->second.constraint->GetController());
        if (wvc) return wvc->GetTransmission().GetCurrentGear();
        auto* tvc = dynamic_cast<const JPH::TrackedVehicleController*>(it->second.constraint->GetController());
        if (tvc) return tvc->GetTransmission().GetCurrentGear();
    }
    return 0;
}

int PhysicsSystem::vehicle_get_wheel_count(uint32_t id) const {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        return static_cast<int>(it->second.constraint->GetWheels().size());
    }
    return 0;
}

bool PhysicsSystem::vehicle_get_wheel_transform(uint32_t id, int wheel_idx, glm::vec3& out_pos, glm::quat& out_rot) const {
    auto it = m_impl->vehicles.find(id);
    if (it == m_impl->vehicles.end() || !it->second.constraint) return false;
    if (wheel_idx < 0 || wheel_idx >= (int)it->second.constraint->GetWheels().size()) return false;

    JPH::RMat44 wt = it->second.constraint->GetWheelWorldTransform(static_cast<JPH::uint>(wheel_idx), JPH::Vec3::sAxisX(), JPH::Vec3::sAxisY());
    glm::mat4 gwt = to_glm_mat4(wt);
    out_pos = glm::vec3(gwt[3]);
    out_rot = glm::quat_cast(gwt);
    return true;
}

// ============================================================================
// Animated Ragdolls & Skeleton Mapping
// ============================================================================

uint32_t PhysicsSystem::create_skeleton(const std::vector<std::pair<std::string, int>>& joints) {
    if (!m_impl->initialized) return 0;
    auto skel = JPH::Ref<JPH::Skeleton>(new JPH::Skeleton());
    for (const auto& j : joints) {
        skel->AddJoint(j.first, j.second);
    }
    skel->CalculateParentJointIndices();
    uint32_t sid = m_impl->next_skeleton_id++;
    m_impl->skeletons[sid] = skel;
    return sid;
}

bool PhysicsSystem::destroy_skeleton(uint32_t id) {
    return m_impl->skeletons.erase(id) > 0;
}

uint32_t PhysicsSystem::create_skeleton_pose(uint32_t skeleton_id) {
    if (!m_impl->initialized) return 0;
    auto it = m_impl->skeletons.find(skeleton_id);
    if (it == m_impl->skeletons.end() || !it->second) return 0;

    auto pose = std::make_unique<JPH::SkeletonPose>();
    pose->SetSkeleton(it->second);
    pose->CalculateJointMatrices();

    uint32_t pid = m_impl->next_pose_id++;
    m_impl->skeleton_poses[pid] = std::move(pose);
    return pid;
}

bool PhysicsSystem::destroy_skeleton_pose(uint32_t id) {
    return m_impl->skeleton_poses.erase(id) > 0;
}

void PhysicsSystem::skeleton_pose_set_joint(uint32_t pose_id, int joint_idx, const glm::vec3& translation, const glm::quat& rotation) {
    auto it = m_impl->skeleton_poses.find(pose_id);
    if (it != m_impl->skeleton_poses.end() && it->second) {
        if (joint_idx >= 0 && joint_idx < (int)it->second->GetJointCount()) {
            auto& j = it->second->GetJoint(joint_idx);
            j.mTranslation = to_jolt_vec3(translation);
            j.mRotation = to_jolt_quat(rotation);
        }
    }
}

void PhysicsSystem::skeleton_pose_calculate_matrices(uint32_t pose_id) {
    auto it = m_impl->skeleton_poses.find(pose_id);
    if (it != m_impl->skeleton_poses.end() && it->second) {
        it->second->CalculateJointMatrices();
    }
}

glm::mat4 PhysicsSystem::skeleton_pose_get_joint_matrix(uint32_t pose_id, int joint_idx) const {
    auto it = m_impl->skeleton_poses.find(pose_id);
    if (it != m_impl->skeleton_poses.end() && it->second) {
        if (joint_idx >= 0 && joint_idx < (int)it->second->GetJointCount()) {
            return to_glm_mat4(it->second->GetJointMatrix(joint_idx));
        }
    }
    return glm::mat4(1.0f);
}

void PhysicsSystem::skeleton_pose_set_root_offset(uint32_t pose_id, const glm::vec3& offset) {
    auto it = m_impl->skeleton_poses.find(pose_id);
    if (it != m_impl->skeleton_poses.end() && it->second) {
        it->second->SetRootOffset(to_jolt_rvec3(offset));
    }
}

glm::vec3 PhysicsSystem::skeleton_pose_get_root_offset(uint32_t pose_id) const {
    auto it = m_impl->skeleton_poses.find(pose_id);
    if (it != m_impl->skeleton_poses.end() && it->second) {
        return to_glm_vec3(it->second->GetRootOffset());
    }
    return glm::vec3(0.0f);
}

int PhysicsSystem::skeleton_pose_get_joint_count(uint32_t pose_id) const {
    auto it = m_impl->skeleton_poses.find(pose_id);
    if (it != m_impl->skeleton_poses.end() && it->second) {
        return static_cast<int>(it->second->GetJointCount());
    }
    return 0;
}

uint32_t PhysicsSystem::create_skeleton_mapper(uint32_t skeleton_low_id, uint32_t skeleton_high_id, uint32_t neutral_pose_low_id, uint32_t neutral_pose_high_id) {
    if (!m_impl->initialized) return 0;
    auto it_slow = m_impl->skeletons.find(skeleton_low_id);
    auto it_shigh = m_impl->skeletons.find(skeleton_high_id);
    auto it_plow = m_impl->skeleton_poses.find(neutral_pose_low_id);
    auto it_phigh = m_impl->skeleton_poses.find(neutral_pose_high_id);
    if (it_slow == m_impl->skeletons.end() || it_shigh == m_impl->skeletons.end() ||
        it_plow == m_impl->skeleton_poses.end() || it_phigh == m_impl->skeleton_poses.end()) return 0;

    auto mapper = JPH::Ref<JPH::SkeletonMapper>(new JPH::SkeletonMapper());
    mapper->Initialize(
        it_slow->second,
        it_plow->second->GetJointMatrices().data(),
        it_shigh->second,
        it_phigh->second->GetJointMatrices().data()
    );

    uint32_t mid = m_impl->next_mapper_id++;
    m_impl->skeleton_mappers[mid] = mapper;
    return mid;
}

bool PhysicsSystem::destroy_skeleton_mapper(uint32_t id) {
    return m_impl->skeleton_mappers.erase(id) > 0;
}

void PhysicsSystem::skeleton_mapper_map(uint32_t mapper_id, uint32_t pose_low_id, uint32_t pose_high_local_id, uint32_t pose_high_out_model_id) {
    auto it_m = m_impl->skeleton_mappers.find(mapper_id);
    auto it_plow = m_impl->skeleton_poses.find(pose_low_id);
    auto it_phigh_local = m_impl->skeleton_poses.find(pose_high_local_id);
    auto it_phigh_out = m_impl->skeleton_poses.find(pose_high_out_model_id);
    if (it_m == m_impl->skeleton_mappers.end() || it_plow == m_impl->skeleton_poses.end() ||
        it_phigh_local == m_impl->skeleton_poses.end() || it_phigh_out == m_impl->skeleton_poses.end()) return;

    it_m->second->Map(
        it_plow->second->GetJointMatrices().data(),
        it_phigh_local->second->GetJointMatrices().data(),
        it_phigh_out->second->GetJointMatrices().data()
    );
}

void PhysicsSystem::skeleton_mapper_map_reverse(uint32_t mapper_id, uint32_t pose_high_model_id, uint32_t pose_low_out_model_id) {
    auto it_m = m_impl->skeleton_mappers.find(mapper_id);
    auto it_phigh = m_impl->skeleton_poses.find(pose_high_model_id);
    auto it_plow_out = m_impl->skeleton_poses.find(pose_low_out_model_id);
    if (it_m == m_impl->skeleton_mappers.end() || it_phigh == m_impl->skeleton_poses.end() || it_plow_out == m_impl->skeleton_poses.end()) return;

    it_m->second->MapReverse(
        it_phigh->second->GetJointMatrices().data(),
        it_plow_out->second->GetJointMatrices().data()
    );
}

void PhysicsSystem::skeleton_mapper_lock_all_translations(uint32_t mapper_id, uint32_t skeleton_high_id, uint32_t neutral_pose_high_id) {
    auto it_m = m_impl->skeleton_mappers.find(mapper_id);
    auto it_shigh = m_impl->skeletons.find(skeleton_high_id);
    auto it_phigh = m_impl->skeleton_poses.find(neutral_pose_high_id);
    if (it_m == m_impl->skeleton_mappers.end() || it_shigh == m_impl->skeletons.end() || it_phigh == m_impl->skeleton_poses.end()) return;

    it_m->second->LockAllTranslations(it_shigh->second, it_phigh->second->GetJointMatrices().data());
}

uint32_t PhysicsSystem::create_ragdoll(const RagdollConfig& config) {
    if (!m_impl->initialized || config.parts.empty()) return 0;

    auto settings = JPH::Ref<JPH::RagdollSettings>(new JPH::RagdollSettings());
    auto skel = JPH::Ref<JPH::Skeleton>(new JPH::Skeleton());

    for (int i = 0; i < (int)config.parts.size(); ++i) {
        const auto& part = config.parts[i];
        skel->AddJoint(part.name, part.parent_joint_index);
    }
    skel->CalculateParentJointIndices();
    settings->mSkeleton = skel;

    settings->mParts.resize(config.parts.size());
    for (int i = 0; i < (int)config.parts.size(); ++i) {
        const auto& pcfg = config.parts[i];
        auto& part = settings->mParts[i];

        JPH::RefConst<JPH::Shape> shape;
        if (pcfg.shape_type == RagdollPartShape::Box) {
            shape = new JPH::BoxShape(to_jolt_vec3(pcfg.half_extent));
        } else if (pcfg.shape_type == RagdollPartShape::Sphere) {
            shape = new JPH::SphereShape(pcfg.radius);
        } else {
            shape = new JPH::CapsuleShape(pcfg.half_height, pcfg.radius);
        }

        part.SetShape(shape);
        part.mPosition = to_jolt_rvec3(pcfg.position);
        part.mRotation = to_jolt_quat(pcfg.rotation);
        part.mMotionType = to_jolt_motion(pcfg.motion);
        part.mObjectLayer = to_jolt_layer(pcfg.motion);
        part.mMassPropertiesOverride.mMass = pcfg.mass;
        part.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        part.mFriction = pcfg.friction;

        if (pcfg.parent_joint_index >= 0 && pcfg.parent_joint_index < (int)config.parts.size()) {
            const auto& parent_cfg = config.parts[pcfg.parent_joint_index];
            auto* st = new JPH::SwingTwistConstraintSettings();
            st->mSpace = JPH::EConstraintSpace::WorldSpace;
            st->mPosition1 = st->mPosition2 = to_jolt_rvec3(parent_cfg.position);
            st->mTwistAxis1 = st->mTwistAxis2 = JPH::Vec3::sAxisY();
            st->mPlaneAxis1 = st->mPlaneAxis2 = JPH::Vec3::sAxisZ();
            st->mNormalHalfConeAngle = pcfg.swing_limit_y;
            st->mPlaneHalfConeAngle = pcfg.swing_limit_z;
            st->mTwistMinAngle = pcfg.twist_min;
            st->mTwistMaxAngle = pcfg.twist_max;

            if (pcfg.enable_motors) {
                st->mSwingType = JPH::ESwingType::Cone;
                st->mSwingMotorSettings.mSpringSettings.mFrequency = std::max(0.5f, std::sqrt(pcfg.motor_spring_k / 50.0f) / 6.28f);
                st->mTwistMotorSettings.mSpringSettings.mFrequency = std::max(0.5f, std::sqrt(pcfg.motor_spring_k / 50.0f) / 6.28f);
                st->mSwingMotorSettings.mSpringSettings.mDamping = pcfg.motor_damping_c;
                st->mTwistMotorSettings.mSpringSettings.mDamping = pcfg.motor_damping_c;
                st->mSwingMotorSettings.mMaxTorqueLimit = pcfg.motor_max_torque;
                st->mTwistMotorSettings.mMaxTorqueLimit = pcfg.motor_max_torque;
            }

            part.mToParent = st;
        }
    }

    if (config.disable_parent_child_collisions) {
        settings->DisableParentChildCollisions();
    }
    if (config.stabilize) {
        settings->Stabilize();
    }
    settings->CalculateConstraintPriorities();
    settings->CalculateBodyIndexToConstraintIndex();
    settings->CalculateConstraintIndexToBodyIdxPair();

    JPH::CollisionGroup::GroupID group_id = static_cast<JPH::CollisionGroup::GroupID>(m_impl->next_ragdoll_id);
    JPH::Ragdoll* ragdoll = settings->CreateRagdoll(group_id, 0, &m_impl->physics_system);
    if (!ragdoll) return 0;

    ragdoll->AddToPhysicsSystem(JPH::EActivation::Activate);

    uint32_t rid = m_impl->next_ragdoll_id++;
    uint32_t sid = m_impl->next_skeleton_id++;
    m_impl->skeletons[sid] = skel;
    m_impl->ragdolls[rid] = { ragdoll, settings, sid, false, config.parts };
    return rid;
}

bool PhysicsSystem::destroy_ragdoll(uint32_t id) {
    if (!m_impl->initialized) return false;
    auto it = m_impl->ragdolls.find(id);
    if (it != m_impl->ragdolls.end()) {
        if (it->second.ragdoll) it->second.ragdoll->RemoveFromPhysicsSystem();
        m_impl->ragdolls.erase(it);
        return true;
    }
    return false;
}

void PhysicsSystem::ragdoll_set_pose(uint32_t ragdoll_id, uint32_t pose_id) {
    auto it_r = m_impl->ragdolls.find(ragdoll_id);
    auto it_p = m_impl->skeleton_poses.find(pose_id);
    if (it_r != m_impl->ragdolls.end() && it_r->second.ragdoll && it_p != m_impl->skeleton_poses.end() && it_p->second) {
        it_r->second.ragdoll->SetPose(*it_p->second);
    }
}

void PhysicsSystem::ragdoll_drive_to_pose_kinematics(uint32_t ragdoll_id, uint32_t pose_id, float dt) {
    auto it_r = m_impl->ragdolls.find(ragdoll_id);
    auto it_p = m_impl->skeleton_poses.find(pose_id);
    if (it_r != m_impl->ragdolls.end() && it_r->second.ragdoll && it_p != m_impl->skeleton_poses.end() && it_p->second) {
        it_r->second.ragdoll->DriveToPoseUsingKinematics(*it_p->second, dt);
    }
}

void PhysicsSystem::ragdoll_drive_to_pose_motors(uint32_t ragdoll_id, uint32_t pose_id) {
    auto it_r = m_impl->ragdolls.find(ragdoll_id);
    auto it_p = m_impl->skeleton_poses.find(pose_id);
    if (it_r != m_impl->ragdolls.end() && it_r->second.ragdoll && it_p != m_impl->skeleton_poses.end() && it_p->second) {
        it_r->second.ragdoll->DriveToPoseUsingMotors(*it_p->second);
    }
}

void PhysicsSystem::ragdoll_drive_to_pose_motors_velocity(uint32_t ragdoll_id, uint32_t prev_pose_id, uint32_t pose_id, float dt) {
    auto it_r = m_impl->ragdolls.find(ragdoll_id);
    auto it_prev = m_impl->skeleton_poses.find(prev_pose_id);
    auto it_cur = m_impl->skeleton_poses.find(pose_id);
    if (it_r != m_impl->ragdolls.end() && it_r->second.ragdoll &&
        it_prev != m_impl->skeleton_poses.end() && it_prev->second &&
        it_cur != m_impl->skeleton_poses.end() && it_cur->second) {
        it_r->second.ragdoll->DriveToPoseUsingMotors(*it_prev->second, *it_cur->second, dt);
    }
}

void PhysicsSystem::ragdoll_get_pose(uint32_t ragdoll_id, uint32_t pose_id) const {
    auto it_r = m_impl->ragdolls.find(ragdoll_id);
    auto it_p = m_impl->skeleton_poses.find(pose_id);
    if (it_r != m_impl->ragdolls.end() && it_r->second.ragdoll && it_p != m_impl->skeleton_poses.end() && it_p->second) {
        it_r->second.ragdoll->GetPose(*it_p->second);
    }
}

void PhysicsSystem::ragdoll_set_hard_keying(uint32_t ragdoll_id, bool hard_keying) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it == m_impl->ragdolls.end() || !it->second.ragdoll) return;
    it->second.is_hard_keyed = hard_keying;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    size_t count = it->second.ragdoll->GetBodyCount();
    for (size_t i = 0; i < count; ++i) {
        JPH::BodyID bid = it->second.ragdoll->GetBodyID(static_cast<int>(i));
        bi.SetMotionType(bid, hard_keying ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
    }
}

void PhysicsSystem::ragdoll_activate(uint32_t ragdoll_id) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        it->second.ragdoll->Activate();
    }
}

bool PhysicsSystem::ragdoll_is_active(uint32_t ragdoll_id) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        return it->second.ragdoll->IsActive();
    }
    return false;
}

uint32_t PhysicsSystem::ragdoll_get_body_id(uint32_t ragdoll_id, int part_idx) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        if (part_idx >= 0 && part_idx < (int)it->second.ragdoll->GetBodyCount()) {
            return it->second.ragdoll->GetBodyID(part_idx).GetIndexAndSequenceNumber();
        }
    }
    return 0;
}

int PhysicsSystem::ragdoll_get_part_count(uint32_t ragdoll_id) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        return static_cast<int>(it->second.ragdoll->GetBodyCount());
    }
    return 0;
}

uint32_t PhysicsSystem::ragdoll_get_skeleton_id(uint32_t ragdoll_id) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end()) {
        return it->second.skeleton_id;
    }
    return 0;
}

} // namespace crayon
