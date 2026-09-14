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
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
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
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/SoftBody/SoftBodyShape.h>
#include <Jolt/Physics/SoftBody/SoftBodyContactListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyManifold.h>

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

    // Soft Bodies
    struct SoftBodyRecord {
        JPH::BodyID body_id;
        JPH::Ref<JPH::SoftBodySharedSettings> shared_settings;
        uint32_t vertex_count = 0;
        uint32_t face_count = 0;
        uint32_t rod_count = 0;
        std::vector<glm::mat4> inv_bind_matrices;
    };
    std::unordered_map<uint32_t, SoftBodyRecord> soft_bodies;
    std::unordered_map<uint32_t, uint32_t> body_id_to_soft_body_id;
    uint32_t next_soft_body_id = 1;

    class SoftBodyContactListenerImpl : public JPH::SoftBodyContactListener {
    public:
        virtual JPH::SoftBodyValidateResult OnSoftBodyContactValidate(const JPH::Body&, const JPH::Body&, JPH::SoftBodyContactSettings&) override {
            return JPH::SoftBodyValidateResult::AcceptContact;
        }
        virtual void OnSoftBodyContactAdded(const JPH::Body&, const JPH::SoftBodyManifold&) override {
        }
    };
    SoftBodyContactListenerImpl soft_body_listener;

    bool initialized = false;
    float last_dt = 0.0f;
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
    m_impl->physics_system.SetSoftBodyContactListener(&m_impl->soft_body_listener);
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
    m_impl->last_dt = dt;
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

    glm::vec3 n = glm::normalize(normal);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    float d = glm::dot(up, n);
    glm::quat rot;
    if (d > 0.9999f) {
        rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    } else if (d < -0.9999f) {
        // 180 degrees around X — safe for antiparallel case
        rot = glm::quat(glm::vec3(glm::pi<float>(), 0.0f, 0.0f));
    } else {
        rot = glm::rotation(up, n);
    }

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

uint32_t PhysicsSystem::create_mesh_body(const glm::vec3& pos, const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, float friction, float restitution) {
    if (!m_impl->initialized || vertices.empty() || indices.size() < 3) return 0;

    JPH::VertexList jolt_vertices;
    jolt_vertices.reserve(vertices.size());
    for (const auto& v : vertices) {
        jolt_vertices.push_back(JPH::Float3(v.x, v.y, v.z));
    }

    JPH::IndexedTriangleList jolt_triangles;
    jolt_triangles.reserve(indices.size() / 3);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        jolt_triangles.push_back(JPH::IndexedTriangle(indices[i], indices[i+1], indices[i+2], 0));
    }

    JPH::MeshShapeSettings shape_settings(jolt_vertices, jolt_triangles);
    JPH::ShapeSettings::ShapeResult result = shape_settings.Create();
    if (result.HasError()) {
        CRAYON_LOG_ERROR("Jolt create_mesh_body error: {}", result.GetError().c_str());
        return 0;
    }

    JPH::BodyCreationSettings settings(
        result.Get(),
        JPH::RVec3(pos.x, pos.y, pos.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING
    );
    settings.mFriction = friction;
    settings.mRestitution = restitution;

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

        if (m_impl->body_id_to_soft_body_id.count(body_id)) {
            uint32_t sbid = m_impl->body_id_to_soft_body_id[body_id];
            m_impl->soft_bodies.erase(sbid);
            m_impl->body_id_to_soft_body_id.erase(body_id);
        }
        return true;
    }
    return false;
}

void PhysicsSystem::destroy_all_bodies() {
    if (!m_impl || !m_impl->initialized) return;

    // Remove soft bodies tracking
    m_impl->soft_bodies.clear();
    m_impl->body_id_to_soft_body_id.clear();

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

    // Remove all bodies (including soft bodies)
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

void PhysicsSystem::draw_debug(MeshRenderer3D& renderer, const glm::vec4& active_color, const glm::vec4& sleeping_color, const PhysicsDebugDrawFlags& flags) {
    if (!m_impl->initialized) return;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    renderer.begin_line_batch();

    // 1. Draw Rigid Bodies
    if (flags.draw_shapes || flags.draw_bounding_boxes) {
        for (uint32_t raw_id : m_impl->alive_bodies) {
            // If this is a soft body, skip here; we draw soft bodies in their dedicated pass below
            if (m_impl->body_id_to_soft_body_id.count(raw_id)) continue;

            JPH::BodyID id(raw_id);
            if (!bi.IsAdded(id)) continue;

            bool active = bi.IsActive(id);
            glm::vec4 color = active ? active_color : sleeping_color;

            JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), id);
            if (!lock.Succeeded()) continue;
            const JPH::Body& body = lock.GetBody();

            if (flags.draw_shapes && body.GetShape()) {
                glm::vec3 pos = to_glm_vec3(body.GetPosition());
                glm::quat rot = to_glm_quat(body.GetRotation());

                switch (body.GetShape()->GetSubType()) {
                    case JPH::EShapeSubType::Box: {
                        const auto* box = static_cast<const JPH::BoxShape*>(body.GetShape());
                        renderer.batch_wire_box(pos, to_glm_vec3(box->GetHalfExtent()), rot, color);
                        break;
                    }
                    case JPH::EShapeSubType::Sphere: {
                        const auto* sp = static_cast<const JPH::SphereShape*>(body.GetShape());
                        renderer.batch_wire_sphere(pos, sp->GetRadius(), color);
                        break;
                    }
                    case JPH::EShapeSubType::Capsule: {
                        const auto* cap = static_cast<const JPH::CapsuleShape*>(body.GetShape());
                        renderer.batch_wire_capsule(pos, cap->GetRadius(), cap->GetHalfHeightOfCylinder(), rot, color);
                        break;
                    }
                    case JPH::EShapeSubType::Cylinder: {
                        const auto* cyl = static_cast<const JPH::CylinderShape*>(body.GetShape());
                        renderer.batch_wire_cylinder(pos, cyl->GetRadius(), cyl->GetHalfHeight(), rot, color);
                        break;
                    }
                    default: {
                        // Fallback to world bounding box
                        JPH::AABox aabb = body.GetWorldSpaceBounds();
                        glm::vec3 c = to_glm_vec3(aabb.GetCenter());
                        glm::vec3 ext = to_glm_vec3(aabb.GetExtent()) * 0.5f;
                        renderer.batch_wire_box(c, ext, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), color);
                        break;
                    }
                }
            } else if (flags.draw_bounding_boxes) {
                JPH::AABox aabb = body.GetWorldSpaceBounds();
                glm::vec3 c = to_glm_vec3(aabb.GetCenter());
                glm::vec3 ext = to_glm_vec3(aabb.GetExtent()) * 0.5f;
                renderer.batch_wire_box(c, ext, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), color);
            }

            if (flags.draw_velocities && active) {
                glm::vec3 pos = to_glm_vec3(body.GetPosition());
                glm::vec3 vel = to_glm_vec3(body.GetLinearVelocity());
                renderer.add_line_to_batch(pos, pos + vel * 0.25f, glm::vec4(1.0f, 1.0f, 0.1f, 1.0f));
            }
        }
    }

    // 2. Draw Soft Bodies (Cloth, Soft Balls, Volumetric Jellies, Cosserat Rods)
    if (flags.draw_soft_bodies) {
        glm::vec4 edge_color(0.2f, 0.85f, 1.0f, 0.9f);
        glm::vec4 bend_color(1.0f, 0.35f, 0.85f, 0.7f);
        glm::vec4 vol_color(0.3f, 1.0f, 0.6f, 0.5f);
        glm::vec4 lra_color(1.0f, 0.85f, 0.2f, 0.6f);
        glm::vec4 rod_color(0.1f, 1.0f, 0.3f, 1.0f);
        glm::vec4 frame_x(1.0f, 0.2f, 0.2f, 0.9f);
        glm::vec4 frame_y(0.2f, 0.4f, 1.0f, 0.9f);

        for (const auto& [sb_id, rec] : m_impl->soft_bodies) {
            JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), rec.body_id);
            if (!lock.Succeeded()) continue;
            const JPH::Body& body = lock.GetBody();
            const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
            if (!mp) continue;

            const auto& verts = mp->GetVertices();
            const auto* settings = mp->GetSettings();
            if (!settings) continue;

            // Fetch the body's world transform and rotation
            JPH::RMat44 com_transform = body.GetCenterOfMassTransform();
            JPH::Quat body_rot = body.GetRotation();

            // Helper lambda to transform local Jolt vertices to world-space GLM vectors
            auto get_world_pos = [&](uint32_t idx) {
                return to_glm_vec3(com_transform * verts[idx].mPosition);
            };

            // Draw Edge Constraints
            for (const auto& edge : settings->mEdgeConstraints) {
                if (edge.mVertex[0] < verts.size() && edge.mVertex[1] < verts.size()) {
                    renderer.add_line_to_batch(
                        get_world_pos(edge.mVertex[0]),
                        get_world_pos(edge.mVertex[1]),
                        edge_color
                    );
                }
            }

            // Draw Dihedral Bend Constraints
            if (flags.draw_soft_body_constraints) {
                for (const auto& bend : settings->mDihedralBendConstraints) {
                    if (bend.mVertex[2] < verts.size() && bend.mVertex[3] < verts.size()) {
                        renderer.add_line_to_batch(
                            get_world_pos(bend.mVertex[2]),
                            get_world_pos(bend.mVertex[3]),
                            bend_color
                        );
                    }
                }

                // Draw Tetrahedron Volume Constraints
                for (const auto& vol : settings->mVolumeConstraints) {
                    if (vol.mVertex[0] < verts.size() && vol.mVertex[1] < verts.size() &&
                        vol.mVertex[2] < verts.size() && vol.mVertex[3] < verts.size()) {
                        glm::vec3 p0 = get_world_pos(vol.mVertex[0]);
                        glm::vec3 p1 = get_world_pos(vol.mVertex[1]);
                        glm::vec3 p2 = get_world_pos(vol.mVertex[2]);
                        glm::vec3 p3 = get_world_pos(vol.mVertex[3]);
                        renderer.add_line_to_batch(p0, p1, vol_color);
                        renderer.add_line_to_batch(p0, p2, vol_color);
                        renderer.add_line_to_batch(p0, p3, vol_color);
                        renderer.add_line_to_batch(p1, p2, vol_color);
                        renderer.add_line_to_batch(p2, p3, vol_color);
                        renderer.add_line_to_batch(p3, p1, vol_color);
                    }
                }

                // Draw Long Range Attachment (LRA) Tethers
                for (const auto& lra : settings->mLRAConstraints) {
                    if (lra.mVertex[0] < verts.size() && lra.mVertex[1] < verts.size()) {
                        renderer.add_line_to_batch(
                            get_world_pos(lra.mVertex[0]),
                            get_world_pos(lra.mVertex[1]),
                            lra_color
                        );
                    }
                }
            }

            // Draw Cosserat Rods (Stretch-Shear edges and Bishop frame orientation frames)
            if (flags.draw_soft_body_rods) {
                for (size_t r = 0; r < settings->mRodStretchShearConstraints.size(); ++r) {
                    const auto& rod = settings->mRodStretchShearConstraints[r];
                    if (rod.mVertex[0] < verts.size() && rod.mVertex[1] < verts.size()) {
                        glm::vec3 p0 = get_world_pos(rod.mVertex[0]);
                        glm::vec3 p1 = get_world_pos(rod.mVertex[1]);
                        renderer.add_line_to_batch(p0, p1, rod_color);

                        glm::vec3 mid = (p0 + p1) * 0.5f;
                        // Transform local rod rotation to world space
                        glm::quat q = to_glm_quat(body_rot * mp->GetRodRotation(static_cast<JPH::uint>(r)));
                        float tick = std::max(0.08f, glm::length(p1 - p0) * 0.35f);
                        renderer.add_line_to_batch(mid, mid + q * glm::vec3(tick, 0.0f, 0.0f), frame_x);
                        renderer.add_line_to_batch(mid, mid + q * glm::vec3(0.0f, tick, 0.0f), frame_y);
                    }
                }
            }
        }
    }

    // 3. Draw Rigid Body Characters
    if (flags.draw_characters) {
        for (const auto& [id, ch] : m_impl->characters) {
            if (!ch) continue;
            JPH::RVec3 cpos;
            JPH::Quat crot;
            ch->GetPositionAndRotation(cpos, crot);
            glm::vec3 pos = to_glm_vec3(cpos);
            glm::quat q = to_glm_quat(crot);
            auto it_h = m_impl->character_half_heights.find(id);
            auto it_r = m_impl->character_radii.find(id);
            float rh = (it_h != m_impl->character_half_heights.end()) ? it_h->second : 0.6f;
            float rr = (it_r != m_impl->character_radii.end()) ? it_r->second : 0.4f;
            renderer.batch_wire_capsule(pos, rr, rh, q, glm::vec4(0.3f, 0.8f, 1.0f, 1.0f));
            if (ch->IsSupported()) {
                renderer.add_line_to_batch(to_glm_vec3(ch->GetGroundPosition()),
                                           to_glm_vec3(ch->GetGroundPosition()) + to_glm_vec3(ch->GetGroundNormal()) * 0.5f,
                                           glm::vec4(1.0f, 1.0f, 0.2f, 1.0f));
            }
        }
    }

    // 4. Draw Virtual Characters
    if (flags.draw_characters) {
        for (const auto& [id, vch] : m_impl->virtual_characters) {
            if (!vch) continue;
            glm::vec3 pos = to_glm_vec3(vch->GetPosition());
            glm::quat q = to_glm_quat(vch->GetRotation());
            auto it_h = m_impl->virtual_char_half_heights.find(id);
            auto it_r = m_impl->virtual_char_radii.find(id);
            float rh = (it_h != m_impl->virtual_char_half_heights.end()) ? it_h->second : 0.6f;
            float rr = (it_r != m_impl->virtual_char_radii.end()) ? it_r->second : 0.4f;
            renderer.batch_wire_capsule(pos, rr, rh, q, glm::vec4(0.9f, 0.4f, 1.0f, 1.0f));
            if (vch->IsSupported()) {
                renderer.add_line_to_batch(to_glm_vec3(vch->GetGroundPosition()),
                                           to_glm_vec3(vch->GetGroundPosition()) + to_glm_vec3(vch->GetGroundNormal()) * 0.5f,
                                           glm::vec4(1.0f, 1.0f, 0.2f, 1.0f));
            }
        }
    }

    // 5. Draw Vehicles
    if (flags.draw_vehicles) {
        for (const auto& [id, v] : m_impl->vehicles) {
            if (!v.constraint) continue;
            size_t num_wheels = v.constraint->GetWheels().size();
            for (size_t w = 0; w < num_wheels; ++w) {
                JPH::RMat44 wt = v.constraint->GetWheelWorldTransform(static_cast<JPH::uint>(w), JPH::Vec3::sAxisX(), JPH::Vec3::sAxisY());
                glm::mat4 gwt = to_glm_mat4(wt);
                glm::vec3 wpos = glm::vec3(gwt[3]);
                glm::quat wrot = glm::quat_cast(gwt);
                float wr = (w < v.wheel_radii.size()) ? v.wheel_radii[w] : 0.3f;
                float ww = (w < v.wheel_widths.size()) ? v.wheel_widths[w] : 0.15f;
                renderer.batch_wire_cylinder(wpos, wr, ww * 0.5f, wrot, glm::vec4(1.0f, 0.6f, 0.1f, 1.0f));
            }
        }
    }

    // 6. Draw Ragdolls
    if (flags.draw_ragdolls) {
        for (const auto& [id, r] : m_impl->ragdolls) {
            if (!r.ragdoll) continue;
            size_t body_count = r.ragdoll->GetBodyCount();
            std::vector<glm::vec3> joint_positions(body_count);
            for (size_t b = 0; b < body_count; ++b) {
                JPH::BodyID bid = r.ragdoll->GetBodyID(static_cast<int>(b));
                joint_positions[b] = to_glm_vec3(bi.GetPosition(bid));
                if (b < r.parts.size() && r.parts[b].parent_joint_index >= 0 && r.parts[b].parent_joint_index < (int)body_count) {
                    renderer.add_line_to_batch(joint_positions[r.parts[b].parent_joint_index], joint_positions[b], glm::vec4(0.2f, 1.0f, 0.9f, 1.0f));
                }
            }
        }
    }

    renderer.end_line_batch();
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
    settings.mLayer = Layers::MOVING;   // Characters always live in MOVING
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
    std::vector<int> drive_wheels;
    int wheel_index = 0;
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

        if (w.is_drive) {
            drive_wheels.push_back(wheel_index);
        }
        wheel_index++;
    }

    // Connect drive wheels to transmission via differentials
    if (drive_wheels.empty()) {
        for (int i = 0; i < wheel_index; ++i) drive_wheels.push_back(i);
    }
    for (size_t i = 0; i < drive_wheels.size(); i += 2) {
        JPH::VehicleDifferentialSettings diff;
        diff.mLeftWheel = drive_wheels[i];
        diff.mRightWheel = (i + 1 < drive_wheels.size()) ? drive_wheels[i + 1] : -1;
        controller->mDifferentials.push_back(diff);
    }
    if (!controller->mDifferentials.empty()) {
        float ratio = 1.0f / static_cast<float>(controller->mDifferentials.size());
        for (auto& d : controller->mDifferentials) {
            d.mEngineTorqueRatio = ratio;
        }
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

    // Motorcycle drives rear wheel (index 1)
    controller->mDifferentials.resize(1);
    controller->mDifferentials[0].mLeftWheel = -1;
    controller->mDifferentials[0].mRightWheel = 1;
    controller->mDifferentials[0].mEngineTorqueRatio = 1.0f;

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
    if (it != m_impl->vehicles.end() && it->second.constraint && it->second.type == Impl::VehicleType::Wheeled) {
        auto* controller = static_cast<JPH::WheeledVehicleController*>(it->second.constraint->GetController());
        if (controller) {
            controller->SetDriverInput(forward, steer, brake, handbrake ? 1.0f : 0.0f);
        }
    }
}

void PhysicsSystem::vehicle_set_input_tracked(uint32_t id, float left_ratio, float right_ratio, float brake) {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* controller = static_cast<JPH::TrackedVehicleController*>(it->second.constraint->GetController());
        if (controller) {
            controller->SetDriverInput(1.0f, left_ratio, right_ratio, brake);
        }
    }
}

void PhysicsSystem::vehicle_set_input_motorcycle(uint32_t id, float forward, float steer, float brake) {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* controller = static_cast<JPH::MotorcycleController*>(it->second.constraint->GetController());
        if (controller) {
            controller->SetDriverInput(forward, steer, brake, 0.0f);
        }
    }
}

void PhysicsSystem::vehicle_enable_lean_controller(uint32_t id, bool enable) {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* controller = static_cast<JPH::MotorcycleController*>(it->second.constraint->GetController());
        if (controller) {
            controller->EnableLeanController(enable);
        }
    }
}

bool PhysicsSystem::vehicle_is_lean_controller_enabled(uint32_t id) const {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        auto* controller = static_cast<const JPH::MotorcycleController*>(it->second.constraint->GetController());
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
        if (it->second.type == Impl::VehicleType::Wheeled) {
            auto* wvc = static_cast<const JPH::WheeledVehicleController*>(it->second.constraint->GetController());
            return wvc->GetEngine().GetCurrentRPM();
        } else if (it->second.type == Impl::VehicleType::Tracked) {
            auto* tvc = static_cast<const JPH::TrackedVehicleController*>(it->second.constraint->GetController());
            return tvc->GetEngine().GetCurrentRPM();
        }
    }
    return 0.0f;
}

int PhysicsSystem::vehicle_get_transmission_gear(uint32_t id) const {
    auto it = m_impl->vehicles.find(id);
    if (it != m_impl->vehicles.end() && it->second.constraint) {
        if (it->second.type == Impl::VehicleType::Wheeled) {
            auto* wvc = static_cast<const JPH::WheeledVehicleController*>(it->second.constraint->GetController());
            if (wvc) return wvc->GetTransmission().GetCurrentGear();
        } else if (it->second.type == Impl::VehicleType::Tracked) {
            auto* tvc = static_cast<const JPH::TrackedVehicleController*>(it->second.constraint->GetController());
            if (tvc) return tvc->GetTransmission().GetCurrentGear();
        }
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

void PhysicsSystem::skeleton_pose_calculate_joint_states(uint32_t pose_id) {
    auto it = m_impl->skeleton_poses.find(pose_id);
    if (it != m_impl->skeleton_poses.end() && it->second) {
        it->second->CalculateJointStates();
    }
}

bool PhysicsSystem::skeleton_pose_get_joint(uint32_t pose_id, int joint_idx, glm::vec3& out_translation, glm::quat& out_rotation) const {
    auto it = m_impl->skeleton_poses.find(pose_id);
    if (it != m_impl->skeleton_poses.end() && it->second) {
        if (joint_idx >= 0 && joint_idx < (int)it->second->GetJointCount()) {
            const auto& j = it->second->GetJoint(joint_idx);
            out_translation = to_glm_vec3(j.mTranslation);
            out_rotation = to_glm_quat(j.mRotation);
            return true;
        }
    }
    return false;
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
        part.mAllowDynamicOrKinematic = true;

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

void PhysicsSystem::ragdoll_set_linear_velocity(uint32_t ragdoll_id, const glm::vec3& vel) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        it->second.ragdoll->SetLinearVelocity(to_jolt_vec3(vel));
    }
}

void PhysicsSystem::ragdoll_set_linear_and_angular_velocity(uint32_t ragdoll_id, const glm::vec3& linear, const glm::vec3& angular) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        it->second.ragdoll->SetLinearAndAngularVelocity(to_jolt_vec3(linear), to_jolt_vec3(angular));
    }
}

void PhysicsSystem::ragdoll_add_linear_velocity(uint32_t ragdoll_id, const glm::vec3& vel) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        it->second.ragdoll->AddLinearVelocity(to_jolt_vec3(vel));
    }
}

void PhysicsSystem::ragdoll_add_impulse(uint32_t ragdoll_id, const glm::vec3& impulse) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        it->second.ragdoll->AddImpulse(to_jolt_vec3(impulse));
    }
}

void PhysicsSystem::ragdoll_add_impulse_to_part(uint32_t ragdoll_id, int part_idx, const glm::vec3& impulse) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        if (part_idx >= 0 && part_idx < (int)it->second.ragdoll->GetBodyCount()) {
            JPH::BodyID bid = it->second.ragdoll->GetBodyID(part_idx);
            m_impl->physics_system.GetBodyInterface().AddImpulse(bid, to_jolt_vec3(impulse));
        }
    }
}

void PhysicsSystem::ragdoll_add_impulse_to_part_at_pos(uint32_t ragdoll_id, int part_idx, const glm::vec3& impulse, const glm::vec3& pos) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        if (part_idx >= 0 && part_idx < (int)it->second.ragdoll->GetBodyCount()) {
            JPH::BodyID bid = it->second.ragdoll->GetBodyID(part_idx);
            m_impl->physics_system.GetBodyInterface().AddImpulse(bid, to_jolt_vec3(impulse), to_jolt_rvec3(pos));
        }
    }
}

void PhysicsSystem::ragdoll_reset_warm_start(uint32_t ragdoll_id) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        it->second.ragdoll->ResetWarmStart();
    }
}

bool PhysicsSystem::ragdoll_get_root_transform(uint32_t ragdoll_id, glm::vec3& out_pos, glm::quat& out_rot) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        JPH::RVec3 p;
        JPH::Quat q;
        it->second.ragdoll->GetRootTransform(p, q);
        out_pos = to_glm_vec3(p);
        out_rot = to_glm_quat(q);
        return true;
    }
    return false;
}

int PhysicsSystem::ragdoll_get_ground_orientation(uint32_t ragdoll_id) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it == m_impl->ragdolls.end() || !it->second.ragdoll) return 0;
    auto* ragdoll = it->second.ragdoll.GetPtr();
    if (ragdoll->GetBodyCount() == 0) return 0;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    int check_idx = 0;
    if (ragdoll->GetBodyCount() > 2) {
        check_idx = 1; // Spine or Chest usually
    }
    JPH::BodyID bid = ragdoll->GetBodyID(check_idx);
    JPH::Quat rot = bi.GetRotation(bid);
    JPH::Vec3 fwd = rot.RotateAxisZ();
    if (std::abs(fwd.GetY()) < 0.25f) {
        JPH::Vec3 up = rot.RotateAxisY();
        if (up.GetY() > 0.35f) return 1;
        if (up.GetY() < -0.35f) return -1;
    }
    if (fwd.GetY() > 0.3f) return 1;
    if (fwd.GetY() < -0.3f) return -1;
    return 0;
}

void PhysicsSystem::ragdoll_get_bounds(uint32_t ragdoll_id, glm::vec3& out_min, glm::vec3& out_max) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        JPH::AABox box = it->second.ragdoll->GetWorldSpaceBounds();
        out_min = to_glm_vec3(box.mMin);
        out_max = to_glm_vec3(box.mMax);
    } else {
        out_min = glm::vec3(0.0f);
        out_max = glm::vec3(0.0f);
    }
}

glm::vec3 PhysicsSystem::ragdoll_get_part_position(uint32_t ragdoll_id, int part_idx) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        if (part_idx >= 0 && part_idx < (int)it->second.ragdoll->GetBodyCount()) {
            JPH::BodyID bid = it->second.ragdoll->GetBodyID(part_idx);
            return to_glm_vec3(m_impl->physics_system.GetBodyInterface().GetPosition(bid));
        }
    }
    return glm::vec3(0.0f);
}

glm::quat PhysicsSystem::ragdoll_get_part_rotation(uint32_t ragdoll_id, int part_idx) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        if (part_idx >= 0 && part_idx < (int)it->second.ragdoll->GetBodyCount()) {
            JPH::BodyID bid = it->second.ragdoll->GetBodyID(part_idx);
            return to_glm_quat(m_impl->physics_system.GetBodyInterface().GetRotation(bid));
        }
    }
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

glm::vec3 PhysicsSystem::ragdoll_get_part_linear_velocity(uint32_t ragdoll_id, int part_idx) const {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        if (part_idx >= 0 && part_idx < (int)it->second.ragdoll->GetBodyCount()) {
            JPH::BodyID bid = it->second.ragdoll->GetBodyID(part_idx);
            return to_glm_vec3(m_impl->physics_system.GetBodyInterface().GetLinearVelocity(bid));
        }
    }
    return glm::vec3(0.0f);
}

glm::vec3 PhysicsSystem::ragdoll_get_linear_velocity(uint32_t ragdoll_id) const {
    return ragdoll_get_part_linear_velocity(ragdoll_id, 0);
}

void PhysicsSystem::ragdoll_set_motors_stiffness(uint32_t ragdoll_id, float spring_k, float damping_c, float max_torque) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it == m_impl->ragdolls.end() || !it->second.ragdoll) return;
    auto* ragdoll = it->second.ragdoll.GetPtr();

    size_t ccount = ragdoll->GetConstraintCount();
    for (size_t i = 0; i < ccount; ++i) {
        auto* c = ragdoll->GetConstraint(static_cast<int>(i));
        if (c && c->GetSubType() == JPH::EConstraintSubType::SwingTwist) {
            auto* st = static_cast<JPH::SwingTwistConstraint*>(c);
            if (spring_k <= 0.001f) {
                st->SetSwingMotorState(JPH::EMotorState::Off);
                st->SetTwistMotorState(JPH::EMotorState::Off);
            } else {
                st->SetSwingMotorState(JPH::EMotorState::Position);
                st->SetTwistMotorState(JPH::EMotorState::Position);
                float freq = std::max(0.5f, std::sqrt(spring_k / 50.0f) / 6.28f);
                st->GetSwingMotorSettings().mSpringSettings.mFrequency = freq;
                st->GetTwistMotorSettings().mSpringSettings.mFrequency = freq;
                st->GetSwingMotorSettings().mSpringSettings.mDamping = damping_c;
                st->GetTwistMotorSettings().mSpringSettings.mDamping = damping_c;
                st->GetSwingMotorSettings().mMaxTorqueLimit = max_torque;
                st->GetTwistMotorSettings().mMaxTorqueLimit = max_torque;
            }
        }
    }
}

void PhysicsSystem::ragdoll_set_part_motor(uint32_t ragdoll_id, int part_idx, float spring_k, float damping_c, float max_torque) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it == m_impl->ragdolls.end() || !it->second.ragdoll || !it->second.settings) return;
    auto* ragdoll = it->second.ragdoll.GetPtr();

    int c_idx = it->second.settings->GetConstraintIndexForBodyIndex(part_idx);
    if (c_idx >= 0 && c_idx < (int)ragdoll->GetConstraintCount()) {
        auto* c = ragdoll->GetConstraint(c_idx);
        if (c && c->GetSubType() == JPH::EConstraintSubType::SwingTwist) {
            auto* st = static_cast<JPH::SwingTwistConstraint*>(c);
            if (spring_k <= 0.001f) {
                st->SetSwingMotorState(JPH::EMotorState::Off);
                st->SetTwistMotorState(JPH::EMotorState::Off);
            } else {
                st->SetSwingMotorState(JPH::EMotorState::Position);
                st->SetTwistMotorState(JPH::EMotorState::Position);
                float freq = std::max(0.5f, std::sqrt(spring_k / 50.0f) / 6.28f);
                st->GetSwingMotorSettings().mSpringSettings.mFrequency = freq;
                st->GetTwistMotorSettings().mSpringSettings.mFrequency = freq;
                st->GetSwingMotorSettings().mSpringSettings.mDamping = damping_c;
                st->GetTwistMotorSettings().mSpringSettings.mDamping = damping_c;
                st->GetSwingMotorSettings().mMaxTorqueLimit = max_torque;
                st->GetTwistMotorSettings().mMaxTorqueLimit = max_torque;
            }
        }
    }
}

void PhysicsSystem::ragdoll_set_part_motion_type(uint32_t ragdoll_id, int part_idx, MotionType motion) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        if (part_idx >= 0 && part_idx < (int)it->second.ragdoll->GetBodyCount()) {
            JPH::BodyID bid = it->second.ragdoll->GetBodyID(part_idx);
            m_impl->physics_system.GetBodyInterface().SetMotionType(bid, to_jolt_motion(motion), JPH::EActivation::Activate);
        }
    }
}

void PhysicsSystem::ragdoll_set_part_friction(uint32_t ragdoll_id, int part_idx, float friction) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        if (part_idx >= 0 && part_idx < (int)it->second.ragdoll->GetBodyCount()) {
            JPH::BodyID bid = it->second.ragdoll->GetBodyID(part_idx);
            m_impl->physics_system.GetBodyInterface().SetFriction(bid, friction);
        }
    }
}

void PhysicsSystem::ragdoll_set_part_restitution(uint32_t ragdoll_id, int part_idx, float restitution) {
    auto it = m_impl->ragdolls.find(ragdoll_id);
    if (it != m_impl->ragdolls.end() && it->second.ragdoll) {
        if (part_idx >= 0 && part_idx < (int)it->second.ragdoll->GetBodyCount()) {
            JPH::BodyID bid = it->second.ragdoll->GetBodyID(part_idx);
            m_impl->physics_system.GetBodyInterface().SetRestitution(bid, restitution);
        }
    }
}

// ============================================================================
// Soft Body Implementation
// ============================================================================

uint32_t PhysicsSystem::create_soft_body(const SoftBodyConfig& config) {
    if (!m_impl || !m_impl->initialized) return 0;

    JPH::Ref<JPH::SoftBodySharedSettings> shared_settings = new JPH::SoftBodySharedSettings();

    // 1. Add Vertices
    shared_settings->mVertices.reserve(config.vertices.size());
    for (const auto& v : config.vertices) {
        shared_settings->mVertices.push_back(JPH::SoftBodySharedSettings::Vertex(
            JPH::Float3(v.position.x, v.position.y, v.position.z),
            JPH::Float3(v.velocity.x, v.velocity.y, v.velocity.z),
            v.inv_mass
        ));
    }

    // 2. Add Faces
    for (const auto& f : config.faces) {
        if (f.v[0] < shared_settings->mVertices.size() &&
            f.v[1] < shared_settings->mVertices.size() &&
            f.v[2] < shared_settings->mVertices.size()) {
            shared_settings->AddFace(JPH::SoftBodySharedSettings::Face(f.v[0], f.v[1], f.v[2], f.material_index));
        }
    }

    // 3. Auto-generate constraints if requested
    if (config.auto_generate_constraints && !shared_settings->mFaces.empty()) {
        JPH::SoftBodySharedSettings::VertexAttributes attr;
        attr.mCompliance = config.auto_compliance;
        attr.mShearCompliance = config.auto_shear_compliance;
        attr.mBendCompliance = config.auto_bend_compliance;
        attr.mLRAType = (config.auto_lra_type == SoftBodyLRAType::EuclideanDistance) ? JPH::SoftBodySharedSettings::ELRAType::EuclideanDistance :
                        (config.auto_lra_type == SoftBodyLRAType::GeodesicDistance)  ? JPH::SoftBodySharedSettings::ELRAType::GeodesicDistance :
                        JPH::SoftBodySharedSettings::ELRAType::None;
        attr.mLRAMaxDistanceMultiplier = config.auto_lra_multiplier;

        JPH::SoftBodySharedSettings::EBendType bend_type =
            (config.auto_bend_type == SoftBodyBendType::Dihedral) ? JPH::SoftBodySharedSettings::EBendType::Dihedral :
            (config.auto_bend_type == SoftBodyBendType::Distance) ? JPH::SoftBodySharedSettings::EBendType::Distance :
            JPH::SoftBodySharedSettings::EBendType::None;

        shared_settings->CreateConstraints(&attr, 1, bend_type);
    }

    // 4. Add Explicit Edge Constraints
    for (const auto& e : config.edge_constraints) {
        shared_settings->mEdgeConstraints.push_back(JPH::SoftBodySharedSettings::Edge(e.v[0], e.v[1], e.compliance));
    }

    // 5. Add Dihedral Bend Constraints
    for (const auto& b : config.dihedral_bend_constraints) {
        shared_settings->mDihedralBendConstraints.push_back(JPH::SoftBodySharedSettings::DihedralBend(b.v[0], b.v[1], b.v[2], b.v[3], b.compliance));
    }

    // 6. Add Tetrahedron Volume Constraints
    for (const auto& vol : config.volume_constraints) {
        shared_settings->mVolumeConstraints.push_back(JPH::SoftBodySharedSettings::Volume(vol.v[0], vol.v[1], vol.v[2], vol.v[3], vol.compliance));
    }

    // 7. Add Long Range Attachment (LRA) Constraints (Tethers)
    for (const auto& lra : config.lra_constraints) {
        shared_settings->mLRAConstraints.push_back(JPH::SoftBodySharedSettings::LRA(lra.kinematic_v, lra.dynamic_v, lra.max_distance));
    }

    // 8. Add Skinned Vertex Constraints
    for (const auto& sk : config.skinned_constraints) {
        JPH::SoftBodySharedSettings::Skinned skinned(sk.vertex, sk.max_distance, sk.backstop_distance, sk.backstop_radius);
        for (size_t w = 0; w < sk.weights.size() && w < JPH::SoftBodySharedSettings::Skinned::cMaxSkinWeights; ++w) {
            skinned.mWeights[w] = JPH::SoftBodySharedSettings::SkinWeight(sk.weights[w].joint_index, sk.weights[w].weight);
        }
        skinned.NormalizeWeights();
        shared_settings->mSkinnedConstraints.push_back(skinned);
    }

    // 9. Add Cosserat Rods (Stretch-Shear and Bend-Twist)
    for (const auto& r : config.rod_stretch_shear_constraints) {
        shared_settings->mRodStretchShearConstraints.push_back(JPH::SoftBodySharedSettings::RodStretchShear(r.v[0], r.v[1], r.compliance));
    }
    for (const auto& r : config.rod_bend_twist_constraints) {
        shared_settings->mRodBendTwistConstraints.push_back(JPH::SoftBodySharedSettings::RodBendTwist(r.rod[0], r.rod[1], r.compliance));
    }

    // 10. Calculate properties for any manually added constraints
    if (!config.auto_generate_constraints) {
        if (!shared_settings->mEdgeConstraints.empty()) shared_settings->CalculateEdgeLengths();
        if (!shared_settings->mRodStretchShearConstraints.empty()) shared_settings->CalculateRodProperties();
        if (!shared_settings->mLRAConstraints.empty()) shared_settings->CalculateLRALengths(config.auto_lra_multiplier);
        if (!shared_settings->mDihedralBendConstraints.empty()) shared_settings->CalculateBendConstraintConstants();
        if (!shared_settings->mVolumeConstraints.empty()) shared_settings->CalculateVolumeConstraintVolumes();
        if (!shared_settings->mSkinnedConstraints.empty()) shared_settings->CalculateSkinnedConstraintNormals();
    }

    // Optimize constraints for parallel XPBD solving
    shared_settings->Optimize();

    // 11. Create Jolt Soft Body
    JPH::SoftBodyCreationSettings sb_settings(
        shared_settings,
        to_jolt_rvec3(config.position),
        to_jolt_quat(config.rotation),
        Layers::MOVING
    );
    sb_settings.mPressure = config.pressure;
    sb_settings.mVertexRadius = config.vertex_radius;
    sb_settings.mLinearDamping = config.linear_damping;
    sb_settings.mMaxLinearVelocity = config.max_linear_velocity;
    sb_settings.mFriction = config.friction;
    sb_settings.mRestitution = config.restitution;
    sb_settings.mGravityFactor = config.gravity_factor;
    sb_settings.mNumIterations = config.num_iterations;
    sb_settings.mUpdatePosition = config.update_position;
    sb_settings.mMakeRotationIdentity = config.make_rotation_identity;
    sb_settings.mAllowSleeping = config.allow_sleeping;
    sb_settings.mFacesDoubleSided = config.faces_double_sided;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    JPH::Body* body = bi.CreateSoftBody(sb_settings);
    if (!body) {
        CRAYON_LOG_ERROR("Failed to create SoftBody in Jolt");
        return 0;
    }

    JPH::BodyID body_id = body->GetID();
    bi.AddBody(body_id, JPH::EActivation::Activate);

    uint32_t raw_id = body_id.GetIndexAndSequenceNumber();
    m_impl->alive_bodies.insert(raw_id);

    uint32_t sb_id = m_impl->next_soft_body_id++;
    Impl::SoftBodyRecord rec;
    rec.body_id = body_id;
    rec.shared_settings = shared_settings;
    rec.vertex_count = static_cast<uint32_t>(shared_settings->mVertices.size());
    rec.face_count = static_cast<uint32_t>(shared_settings->mFaces.size());
    rec.rod_count = static_cast<uint32_t>(shared_settings->mRodStretchShearConstraints.size());
    m_impl->soft_bodies[sb_id] = rec;
    m_impl->body_id_to_soft_body_id[raw_id] = sb_id;

    return sb_id;
}

uint32_t PhysicsSystem::create_soft_body_cloth(const glm::vec3& origin, float width, float height, int segments_x, int segments_y, float compliance, float bend_compliance, bool pin_top_corners, bool add_lra) {
    if (segments_x < 2) segments_x = 2;
    if (segments_y < 2) segments_y = 2;

    SoftBodyConfig config;
    config.position = origin;
    config.auto_generate_constraints = true;
    config.auto_bend_type = (bend_compliance >= 0.0f) ? SoftBodyBendType::Dihedral : SoftBodyBendType::None;
    config.auto_compliance = compliance;
    config.auto_bend_compliance = (bend_compliance >= 0.0f) ? bend_compliance : 1e30f;
    config.auto_shear_compliance = compliance;
    config.auto_lra_type = add_lra ? SoftBodyLRAType::EuclideanDistance : SoftBodyLRAType::None;
    config.faces_double_sided = true;

    float dx = width / (segments_x - 1);
    float dy = height / (segments_y - 1);

    config.vertices.resize(segments_x * segments_y);
    for (int y = 0; y < segments_y; ++y) {
        for (int x = 0; x < segments_x; ++x) {
            int idx = y * segments_x + x;
            config.vertices[idx].position = glm::vec3(x * dx - width * 0.5f, -y * dy, 0.0f);
            config.vertices[idx].velocity = glm::vec3(0.0f);
            if (y == 0 && (pin_top_corners ? (x == 0 || x == segments_x - 1) : true)) {
                config.vertices[idx].inv_mass = 0.0f; // Kinematic anchor
            } else {
                config.vertices[idx].inv_mass = 1.0f;
            }
        }
    }

    for (int y = 0; y < segments_y - 1; ++y) {
        for (int x = 0; x < segments_x - 1; ++x) {
            uint32_t v0 = y * segments_x + x;
            uint32_t v1 = y * segments_x + (x + 1);
            uint32_t v2 = (y + 1) * segments_x + x;
            uint32_t v3 = (y + 1) * segments_x + (x + 1);

            config.faces.push_back({ {v0, v2, v1}, 0 });
            config.faces.push_back({ {v1, v2, v3}, 0 });
        }
    }

    return create_soft_body(config);
}

uint32_t PhysicsSystem::create_soft_body_cube(const glm::vec3& origin, float size, int grid_size, float compliance, float pressure) {
    if (grid_size < 2) grid_size = 2;
    float spacing = size / (grid_size - 1);
    JPH::Ref<JPH::SoftBodySharedSettings> shared_settings = JPH::SoftBodySharedSettings::sCreateCube(static_cast<JPH::uint>(grid_size), spacing);

    if (compliance > 0.0f) {
        for (auto& e : shared_settings->mEdgeConstraints) e.mCompliance = compliance;
        for (auto& v : shared_settings->mVolumeConstraints) v.mCompliance = compliance;
    }
    shared_settings->Optimize();

    JPH::SoftBodyCreationSettings sb_settings(
        shared_settings,
        to_jolt_rvec3(origin),
        JPH::Quat::sIdentity(),
        Layers::MOVING
    );
    sb_settings.mPressure = pressure;
    sb_settings.mFriction = 0.5f;
    sb_settings.mRestitution = 0.3f;
    sb_settings.mNumIterations = 6;

    auto& bi = m_impl->physics_system.GetBodyInterface();
    JPH::Body* body = bi.CreateSoftBody(sb_settings);
    if (!body) return 0;
    JPH::BodyID body_id = body->GetID();
    bi.AddBody(body_id, JPH::EActivation::Activate);

    uint32_t raw_id = body_id.GetIndexAndSequenceNumber();
    m_impl->alive_bodies.insert(raw_id);

    uint32_t sb_id = m_impl->next_soft_body_id++;
    Impl::SoftBodyRecord rec;
    rec.body_id = body_id;
    rec.shared_settings = shared_settings;
    rec.vertex_count = static_cast<uint32_t>(shared_settings->mVertices.size());
    rec.face_count = static_cast<uint32_t>(shared_settings->mFaces.size());
    rec.rod_count = 0;
    m_impl->soft_bodies[sb_id] = rec;
    m_impl->body_id_to_soft_body_id[raw_id] = sb_id;
    return sb_id;
}

uint32_t PhysicsSystem::create_soft_body_sphere(const glm::vec3& origin, float radius, int rings, int sectors, float compliance, float pressure) {
    if (rings < 4) rings = 8;
    if (sectors < 4) sectors = 12;

    SoftBodyConfig config;
    config.position = origin;
    config.pressure = pressure;
    config.auto_generate_constraints = true;
    config.auto_bend_type = SoftBodyBendType::Dihedral;
    config.auto_compliance = compliance;
    config.auto_shear_compliance = compliance;
    config.auto_bend_compliance = (compliance > 0.0f) ? compliance : 0.001f;
    config.faces_double_sided = true;

    for (int r = 0; r <= rings; ++r) {
        float phi = 3.14159265f * float(r) / float(rings);
        float y = radius * std::cos(phi);
        float r_sin = radius * std::sin(phi);

        for (int s = 0; s <= sectors; ++s) {
            float theta = 2.0f * 3.14159265f * float(s) / float(sectors);
            float x = r_sin * std::cos(theta);
            float z = r_sin * std::sin(theta);
            config.vertices.push_back({ glm::vec3(x, y, z), glm::vec3(0.0f), 1.0f });
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            uint32_t cur  = r * (sectors + 1) + s;
            uint32_t next = cur + sectors + 1;

            // Skip north-pole fan (all vertices coincide at r == 0)
            if (r > 0) {
                config.faces.push_back({ {cur, next, cur + 1}, 0 });
            }
            // Skip south-pole fan (all vertices coincide at r == rings-1)
            if (r < rings - 1) {
                config.faces.push_back({ {cur + 1, next, next + 1}, 0 });
            }
        }
    }

    return create_soft_body(config);
}

uint32_t PhysicsSystem::create_soft_body_rod(const std::vector<glm::vec3>& points, float stretch_compliance, float bend_twist_compliance, bool pin_root) {
    if (points.size() < 2) return 0;

    SoftBodyConfig config;
    config.position = glm::vec3(0.0f);
    config.vertices.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        config.vertices[i].position = points[i];
        config.vertices[i].velocity = glm::vec3(0.0f);
        config.vertices[i].inv_mass = (pin_root && i == 0) ? 0.0f : 1.0f;
    }

    size_t num_rods = points.size() - 1;
    for (size_t i = 0; i < num_rods; ++i) {
        config.rod_stretch_shear_constraints.push_back({ {static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1)}, stretch_compliance });
    }
    for (size_t i = 0; i + 1 < num_rods; ++i) {
        config.rod_bend_twist_constraints.push_back({ {static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1)}, bend_twist_compliance });
    }

    return create_soft_body(config);
}

bool PhysicsSystem::destroy_soft_body(uint32_t id) {
    if (!m_impl->initialized) return false;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return false;

    JPH::BodyID body_id = it->second.body_id;
    uint32_t raw_id = body_id.GetIndexAndSequenceNumber();
    auto& bi = m_impl->physics_system.GetBodyInterface();
    if (bi.IsAdded(body_id)) {
        bi.RemoveBody(body_id);
        bi.DestroyBody(body_id);
    }
    m_impl->alive_bodies.erase(raw_id);
    m_impl->body_id_to_soft_body_id.erase(raw_id);
    m_impl->soft_bodies.erase(it);
    return true;
}

bool PhysicsSystem::is_soft_body(uint32_t id) const {
    if (!m_impl->initialized) return false;
    return m_impl->soft_bodies.count(id) > 0;
}

uint32_t PhysicsSystem::get_soft_body_vertex_count(uint32_t id) const {
    if (!m_impl->initialized) return 0;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return 0;
    return it->second.vertex_count;
}

glm::vec3 PhysicsSystem::get_soft_body_vertex_position(uint32_t id, uint32_t v_idx) const {
    if (!m_impl->initialized) return glm::vec3(0.0f);
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return glm::vec3(0.0f);

    JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return glm::vec3(0.0f);
    const JPH::Body& body = lock.GetBody();
    const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp || v_idx >= mp->GetVertices().size()) return glm::vec3(0.0f);

    JPH::RMat44 com = body.GetCenterOfMassTransform();
    JPH::RVec3 world_pos = com * mp->GetVertex(v_idx).mPosition;
    return to_glm_vec3(world_pos);
}

void PhysicsSystem::set_soft_body_vertex_position(uint32_t id, uint32_t v_idx, const glm::vec3& pos) {
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;

    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return;
    JPH::Body& body = lock.GetBody();
    auto* mp = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp || v_idx >= mp->GetVertices().size()) return;

    JPH::RVec3 local_pos = body.GetInverseCenterOfMassTransform() * to_jolt_rvec3(pos);
    mp->GetVertex(v_idx).mPosition = JPH::Vec3(local_pos);
    mp->GetVertex(v_idx).mPreviousPosition = mp->GetVertex(v_idx).mPosition;
}

glm::vec3 PhysicsSystem::get_soft_body_vertex_velocity(uint32_t id, uint32_t v_idx) const {
    if (!m_impl->initialized) return glm::vec3(0.0f);
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return glm::vec3(0.0f);

    JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return glm::vec3(0.0f);
    const JPH::Body& body = lock.GetBody();
    const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp || v_idx >= mp->GetVertices().size()) return glm::vec3(0.0f);

    JPH::RMat44 com = body.GetCenterOfMassTransform();
    return to_glm_vec3(com.Multiply3x3(mp->GetVertex(v_idx).mVelocity));
}

void PhysicsSystem::set_soft_body_vertex_velocity(uint32_t id, uint32_t v_idx, const glm::vec3& vel) {
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;

    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return;
    JPH::Body& body = lock.GetBody();
    auto* mp = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp || v_idx >= mp->GetVertices().size()) return;

    mp->GetVertex(v_idx).mVelocity = body.GetInverseCenterOfMassTransform().Multiply3x3(to_jolt_vec3(vel));
}

float PhysicsSystem::get_soft_body_vertex_inv_mass(uint32_t id, uint32_t v_idx) const {
    if (!m_impl->initialized) return 0.0f;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return 0.0f;

    JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return 0.0f;
    const JPH::Body& body = lock.GetBody();
    const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp || v_idx >= mp->GetVertices().size()) return 0.0f;
    return mp->GetVertex(v_idx).mInvMass;
}

void PhysicsSystem::set_soft_body_vertex_inv_mass(uint32_t id, uint32_t v_idx, float inv_mass) {
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;

    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return;
    JPH::Body& body = lock.GetBody();
    auto* mp = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp || v_idx >= mp->GetVertices().size()) return;
    mp->GetVertex(v_idx).mInvMass = inv_mass;
}

void PhysicsSystem::get_soft_body_vertices(uint32_t id, std::vector<glm::vec3>& out_positions) const {
    out_positions.clear();
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;

    JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return;
    const JPH::Body& body = lock.GetBody();
    const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp) return;

    const auto& verts = mp->GetVertices();
    out_positions.resize(verts.size());
    JPH::RMat44 com = body.GetCenterOfMassTransform();
    for (size_t i = 0; i < verts.size(); ++i) {
        out_positions[i] = to_glm_vec3(com * verts[i].mPosition);
    }
}

void PhysicsSystem::get_soft_body_faces(uint32_t id, std::vector<uint32_t>& out_indices) const {
    out_indices.clear();
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end() || !it->second.shared_settings) return;

    const auto& faces = it->second.shared_settings->mFaces;
    out_indices.reserve(faces.size() * 3);
    for (const auto& f : faces) {
        out_indices.push_back(f.mVertex[0]);
        out_indices.push_back(f.mVertex[1]);
        out_indices.push_back(f.mVertex[2]);
    }
}

void PhysicsSystem::set_soft_body_pressure(uint32_t id, float pressure) {
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;

    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return;
    JPH::Body& body = lock.GetBody();
    auto* mp = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (mp) mp->SetPressure(pressure);
}

float PhysicsSystem::get_soft_body_pressure(uint32_t id) const {
    if (!m_impl->initialized) return 0.0f;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return 0.0f;

    JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return 0.0f;
    const JPH::Body& body = lock.GetBody();
    const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    return mp ? mp->GetPressure() : 0.0f;
}

void PhysicsSystem::set_soft_body_num_iterations(uint32_t id, uint32_t iters) {
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;

    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return;
    JPH::Body& body = lock.GetBody();
    auto* mp = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (mp) mp->SetNumIterations(iters);
}

uint32_t PhysicsSystem::get_soft_body_num_iterations(uint32_t id) const {
    if (!m_impl->initialized) return 0;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return 0;

    JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return 0;
    const JPH::Body& body = lock.GetBody();
    const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    return mp ? mp->GetNumIterations() : 0;
}

float PhysicsSystem::get_soft_body_volume(uint32_t id) const {
    if (!m_impl->initialized) return 0.0f;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return 0.0f;

    JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return 0.0f;
    const JPH::Body& body = lock.GetBody();
    const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    return mp ? mp->GetVolume() : 0.0f;
}

void PhysicsSystem::add_soft_body_impulse_to_vertex(uint32_t id, uint32_t v_idx, const glm::vec3& impulse) {
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;

    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return;
    JPH::Body& body = lock.GetBody();
    auto* mp = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp || v_idx >= mp->GetVertices().size()) return;

    auto& vert = mp->GetVertex(v_idx);
    if (vert.mInvMass > 0.0f) {
        JPH::Vec3 local_impulse = body.GetInverseCenterOfMassTransform().Multiply3x3(to_jolt_vec3(impulse));
        vert.mVelocity += local_impulse * vert.mInvMass;
    }
}

void PhysicsSystem::add_soft_body_force_to_vertex(uint32_t id, uint32_t v_idx, const glm::vec3& force) {
    // Treat force as an impulse integrated over the current physics step.
    // Default to 1/60 only if update() has never run. If you call this from
    // a context that knows the real dt, pass dt via add_soft_body_impulse_to_vertex
    // directly instead.
    float dt = m_impl->last_dt > 0.0f ? m_impl->last_dt : (1.0f / 60.0f);
    add_soft_body_impulse_to_vertex(id, v_idx, force * dt);
}

void PhysicsSystem::skin_soft_body_vertices(uint32_t id, const std::vector<glm::mat4>& joint_matrices, bool hard_skin) {
    if (!m_impl->initialized || joint_matrices.empty()) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;

    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return;
    JPH::Body& body = lock.GetBody();
    auto* mp = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp) return;

    std::vector<JPH::Mat44> jph_mats(joint_matrices.size());
    for (size_t i = 0; i < joint_matrices.size(); ++i) {
        jph_mats[i] = to_jolt_mat4(joint_matrices[i]);
    }
    mp->SkinVertices(body.GetCenterOfMassTransform(), jph_mats.data(), static_cast<JPH::uint>(jph_mats.size()), hard_skin, *m_impl->temp_allocator);
}

void PhysicsSystem::set_soft_body_skinned_max_distance_multiplier(uint32_t id, float multiplier) {
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;

    JPH::BodyLockWrite lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return;
    JPH::Body& body = lock.GetBody();
    auto* mp = static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (mp) mp->SetSkinnedMaxDistanceMultiplier(multiplier);
}

bool PhysicsSystem::get_soft_body_rod_transform(uint32_t id, uint32_t rod_idx, glm::vec3& out_pos, glm::quat& out_rot) const {
    if (!m_impl->initialized) return false;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end() || !it->second.shared_settings) return false;
    if (rod_idx >= it->second.rod_count) return false;

    JPH::BodyLockRead lock(m_impl->physics_system.GetBodyLockInterface(), it->second.body_id);
    if (!lock.Succeeded()) return false;
    const JPH::Body& body = lock.GetBody();
    const auto* mp = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!mp) return false;

    const auto& verts = mp->GetVertices();
    const auto& rod = it->second.shared_settings->mRodStretchShearConstraints[rod_idx];
    if (rod.mVertex[0] >= verts.size() || rod.mVertex[1] >= verts.size()) return false;

    JPH::RMat44 com = body.GetCenterOfMassTransform();
    glm::vec3 p0 = to_glm_vec3(com * verts[rod.mVertex[0]].mPosition);
    glm::vec3 p1 = to_glm_vec3(com * verts[rod.mVertex[1]].mPosition);
    out_pos = (p0 + p1) * 0.5f;
    out_rot = to_glm_quat(mp->GetRodRotation(static_cast<JPH::uint>(rod_idx)));
    return true;
}

uint32_t PhysicsSystem::soft_body_get_body_id(uint32_t id) const {
    if (!m_impl->initialized) return 0;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return 0;
    return it->second.body_id.GetIndexAndSequenceNumber();
}

void PhysicsSystem::activate_soft_body(uint32_t id) {
    if (!m_impl->initialized) return;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return;
    m_impl->physics_system.GetBodyInterface().ActivateBody(it->second.body_id);
}

bool PhysicsSystem::is_soft_body_active(uint32_t id) const {
    if (!m_impl->initialized) return false;
    auto it = m_impl->soft_bodies.find(id);
    if (it == m_impl->soft_bodies.end()) return false;
    return m_impl->physics_system.GetBodyInterface().IsActive(it->second.body_id);
}

} // namespace crayon
