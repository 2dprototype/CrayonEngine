#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace crayon {

class MeshRenderer3D;

enum class MotionType {
    Static = 0,
    Kinematic = 1,
    Dynamic = 2
};

struct RaycastHit {
    bool hit = false;
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
    uint32_t body_id = 0;
};

struct PhysicsConfig {
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
    uint32_t max_bodies = 4096;
    uint32_t num_body_mutexes = 0;
    uint32_t max_body_pairs = 4096;
    uint32_t max_contact_constraints = 4096;
    int num_worker_threads = 2;
    size_t temp_allocator_size = 8 * 1024 * 1024;
};

struct PhysicsDebugDrawFlags {
    bool draw_shapes = true;
    bool draw_soft_bodies = true;
    bool draw_soft_body_constraints = true;
    bool draw_soft_body_rods = true;
    bool draw_constraints = true;
    bool draw_characters = true;
    bool draw_vehicles = true;
    bool draw_ragdolls = true;
    bool draw_bounding_boxes = false;
    bool draw_velocities = false;
};

class PhysicsSystem {
public:
    PhysicsSystem();
    ~PhysicsSystem();

    bool init(const PhysicsConfig& config = PhysicsConfig{});
    void shutdown();

    void update(float dt, int collision_steps = 1);

    // Gravity
    void set_gravity(const glm::vec3& gravity);
    glm::vec3 get_gravity() const;

    // Body creation
    uint32_t create_box(const glm::vec3& pos, const glm::vec3& half_extent, MotionType motion = MotionType::Dynamic, float friction = 0.5f, float restitution = 0.2f, float density = 1000.0f);
    uint32_t create_sphere(const glm::vec3& pos, float radius, MotionType motion = MotionType::Dynamic, float friction = 0.5f, float restitution = 0.5f, float density = 1000.0f);
    uint32_t create_capsule(const glm::vec3& pos, float half_height, float radius, MotionType motion = MotionType::Dynamic, float friction = 0.5f, float restitution = 0.2f, float density = 1000.0f);
    uint32_t create_cylinder(const glm::vec3& pos, float half_height, float radius, MotionType motion = MotionType::Dynamic, float friction = 0.5f, float restitution = 0.2f, float density = 1000.0f);
    uint32_t create_plane(const glm::vec3& pos, const glm::vec3& normal, float half_extent = 100.0f);
    uint32_t create_mesh_body(const glm::vec3& pos, const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, float friction = 0.5f, float restitution = 0.2f);

    // Body management
    bool destroy_body(uint32_t body_id);
    void destroy_all_bodies();
    bool is_body_valid(uint32_t body_id) const;
    bool is_body_active(uint32_t body_id) const;
    void activate_body(uint32_t body_id);
    void deactivate_body(uint32_t body_id);

    // Transforms
    glm::vec3 get_position(uint32_t body_id) const;
    void set_position(uint32_t body_id, const glm::vec3& pos, bool activate = true);

    glm::quat get_rotation(uint32_t body_id) const;
    void set_rotation(uint32_t body_id, const glm::quat& rot, bool activate = true);

    glm::vec3 get_euler_angles(uint32_t body_id) const;
    void set_euler_angles(uint32_t body_id, const glm::vec3& euler_deg, bool activate = true);

    void get_transform_matrix(uint32_t body_id, glm::mat4& out_matrix) const;

    // Dynamics & Forces
    glm::vec3 get_linear_velocity(uint32_t body_id) const;
    void set_linear_velocity(uint32_t body_id, const glm::vec3& vel);

    glm::vec3 get_angular_velocity(uint32_t body_id) const;
    void set_angular_velocity(uint32_t body_id, const glm::vec3& ang_vel);

    void add_force(uint32_t body_id, const glm::vec3& force);
    void add_impulse(uint32_t body_id, const glm::vec3& impulse);
    void add_torque(uint32_t body_id, const glm::vec3& torque);
    void add_angular_impulse(uint32_t body_id, const glm::vec3& impulse);

    void set_gravity_factor(uint32_t body_id, float factor);
    void set_friction(uint32_t body_id, float friction);
    void set_restitution(uint32_t body_id, float restitution);
    void set_motion_type(uint32_t body_id, MotionType motion);

    // Raycast
    bool raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance, RaycastHit& out_hit);

    // Constraints / Joints
    uint32_t create_point_constraint(uint32_t body1_id, uint32_t body2_id, const glm::vec3& pivot);
    uint32_t create_hinge_constraint(uint32_t body1_id, uint32_t body2_id, const glm::vec3& pivot, const glm::vec3& axis, float min_angle = -3.14159265f, float max_angle = 3.14159265f);
    uint32_t create_distance_constraint(uint32_t body1_id, uint32_t body2_id, const glm::vec3& p1, const glm::vec3& p2, float min_dist, float max_dist);
    uint32_t create_fixed_constraint(uint32_t body1_id, uint32_t body2_id);
    bool destroy_constraint(uint32_t constraint_id);

    // Sensor / Trigger
    void set_is_sensor(uint32_t body_id, bool is_sensor);
    bool is_sensor(uint32_t body_id) const;

    // Damping
    void set_damping(uint32_t body_id, float linear_damping, float angular_damping);

    // Overlap Queries
    std::vector<uint32_t> overlap_sphere(const glm::vec3& center, float radius);

    // Debug visualization
    using PhysicsDebugDrawFlags = crayon::PhysicsDebugDrawFlags;

    void draw_debug(MeshRenderer3D& renderer, const glm::vec4& active_color = glm::vec4(0.2f, 1.0f, 0.4f, 1.0f), const glm::vec4& sleeping_color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), const PhysicsDebugDrawFlags& flags = PhysicsDebugDrawFlags{});

    // Stats
    uint32_t get_num_bodies() const;
    uint32_t get_num_active_bodies() const;

    // ========================================================================
    // Game Character Simulation (Capsule: Rigid Body & Virtual Character)
    // ========================================================================
    enum class GroundState {
        OnGround = 0,
        OnSteepGround = 1,
        NotSupported = 2,
        InAir = 3
    };

    struct CharacterConfig {
        glm::vec3 pos{0.0f};
        float radius = 0.4f;
        float half_height = 0.6f;
        float mass = 80.0f;
        float friction = 0.2f;
        float gravity_factor = 1.0f;
        float max_slope_angle_deg = 45.0f;
        MotionType motion = MotionType::Dynamic;
    };

    struct CharacterVirtualConfig {
        glm::vec3 pos{0.0f};
        float radius = 0.4f;
        float half_height = 0.6f;
        float mass = 70.0f;
        float max_slope_angle_deg = 45.0f;
        float max_strength = 100.0f;
        float step_height = 0.4f;
        float predictive_contact_distance = 0.1f;
        bool inner_body = true;
    };

    // Rigid Body Character
    uint32_t create_character(const CharacterConfig& config);
    bool destroy_character(uint32_t id);
    void character_set_linear_velocity(uint32_t id, const glm::vec3& vel);
    glm::vec3 character_get_linear_velocity(uint32_t id) const;
    void character_set_position(uint32_t id, const glm::vec3& pos);
    glm::vec3 character_get_position(uint32_t id) const;
    void character_set_rotation(uint32_t id, const glm::quat& rot);
    glm::quat character_get_rotation(uint32_t id) const;
    bool character_is_supported(uint32_t id) const;
    GroundState character_get_ground_state(uint32_t id) const;
    glm::vec3 character_get_ground_normal(uint32_t id) const;
    glm::vec3 character_get_ground_velocity(uint32_t id) const;
    glm::vec3 character_get_ground_position(uint32_t id) const;
    uint32_t character_get_body_id(uint32_t id) const;

    // Virtual Character (simulated outside physics update)
    uint32_t create_character_virtual(const CharacterVirtualConfig& config);
    bool destroy_character_virtual(uint32_t id);
    void character_virtual_update(uint32_t id, float dt);
    void character_virtual_set_linear_velocity(uint32_t id, const glm::vec3& vel);
    glm::vec3 character_virtual_get_linear_velocity(uint32_t id) const;
    void character_virtual_set_position(uint32_t id, const glm::vec3& pos);
    glm::vec3 character_virtual_get_position(uint32_t id) const;
    void character_virtual_set_rotation(uint32_t id, const glm::quat& rot);
    glm::quat character_virtual_get_rotation(uint32_t id) const;
    bool character_virtual_is_supported(uint32_t id) const;
    GroundState character_virtual_get_ground_state(uint32_t id) const;
    glm::vec3 character_virtual_get_ground_normal(uint32_t id) const;
    glm::vec3 character_virtual_get_ground_velocity(uint32_t id) const;
    glm::vec3 character_virtual_get_ground_position(uint32_t id) const;

    // ========================================================================
    // Vehicles (Wheeled Vehicles, Tracked Vehicles, Motorcycles)
    // ========================================================================
    struct WheelConfig {
        glm::vec3 position{0.0f}; // Local attachment offset
        float radius = 0.3f;
        float width = 0.15f;
        float suspension_min_length = 0.15f;
        float suspension_max_length = 0.45f;
        float suspension_spring = 25000.0f;
        float suspension_damping = 2500.0f;
        float max_steer_angle_rad = 0.6f;
        float max_brake_torque = 1500.0f;
        float max_hand_brake_torque = 4000.0f;
        bool is_front = false;
        bool is_drive = true;
    };

    struct WheeledVehicleConfig {
        uint32_t chassis_body_id = 0;
        std::vector<WheelConfig> wheels;
        float max_pitch_roll_angle = 3.14159f;
        float engine_max_torque = 400.0f;
        float engine_min_rpm = 1000.0f;
        float engine_max_rpm = 7000.0f;
    };

    struct TrackedVehicleConfig {
        uint32_t chassis_body_id = 0;
        std::vector<WheelConfig> left_wheels;
        std::vector<WheelConfig> right_wheels;
        float engine_max_torque = 800.0f;
    };

    struct MotorcycleConfig {
        uint32_t chassis_body_id = 0;
        WheelConfig front_wheel;
        WheelConfig rear_wheel;
        float max_lean_angle_rad = 0.785f; // 45 deg
        float lean_spring_constant = 5000.0f;
        float lean_spring_damping = 1000.0f;
        float lean_smoothing_factor = 0.8f;
        float engine_max_torque = 250.0f;
    };

    uint32_t create_wheeled_vehicle(const WheeledVehicleConfig& config);
    uint32_t create_tracked_vehicle(const TrackedVehicleConfig& config);
    uint32_t create_motorcycle(const MotorcycleConfig& config);
    bool destroy_vehicle(uint32_t id);

    void vehicle_set_input_wheeled(uint32_t id, float forward, float steer, float brake, bool handbrake);
    void vehicle_set_input_tracked(uint32_t id, float left_ratio, float right_ratio, float brake);
    void vehicle_set_input_motorcycle(uint32_t id, float forward, float steer, float brake);

    void vehicle_enable_lean_controller(uint32_t id, bool enable);
    bool vehicle_is_lean_controller_enabled(uint32_t id) const;
    float vehicle_get_lean_angle(uint32_t id) const;

    float vehicle_get_speed_kmh(uint32_t id) const;
    float vehicle_get_engine_rpm(uint32_t id) const;
    int vehicle_get_transmission_gear(uint32_t id) const;
    int vehicle_get_wheel_count(uint32_t id) const;
    bool vehicle_get_wheel_transform(uint32_t id, int wheel_idx, glm::vec3& out_pos, glm::quat& out_rot) const;

    // ========================================================================
    // Animated Ragdolls & Skeleton Mapping
    // ========================================================================
    enum class RagdollPartShape { Box, Capsule, Sphere };

    struct RagdollPartConfig {
        std::string name;
        int parent_joint_index = -1; // -1 for root
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        RagdollPartShape shape_type = RagdollPartShape::Capsule;
        glm::vec3 half_extent{0.1f};
        float radius = 0.1f;
        float half_height = 0.2f;
        MotionType motion = MotionType::Dynamic;
        float mass = 5.0f;
        float friction = 0.5f;

        float swing_limit_y = 0.785f;
        float swing_limit_z = 0.785f;
        float twist_min = -0.785f;
        float twist_max = 0.785f;
        bool enable_motors = true;
        float motor_spring_k = 2000.0f;
        float motor_damping_c = 100.0f;
        float motor_max_torque = 500.0f;
    };

    struct RagdollConfig {
        std::vector<RagdollPartConfig> parts;
        bool disable_parent_child_collisions = true;
        bool stabilize = true;
    };

    // Skeleton
    uint32_t create_skeleton(const std::vector<std::pair<std::string, int>>& joints);
    bool destroy_skeleton(uint32_t id);

    // SkeletonPose
    uint32_t create_skeleton_pose(uint32_t skeleton_id);
    bool destroy_skeleton_pose(uint32_t id);
    void skeleton_pose_set_joint(uint32_t pose_id, int joint_idx, const glm::vec3& translation, const glm::quat& rotation);
    void skeleton_pose_calculate_matrices(uint32_t pose_id);
    glm::mat4 skeleton_pose_get_joint_matrix(uint32_t pose_id, int joint_idx) const;
    void skeleton_pose_set_root_offset(uint32_t pose_id, const glm::vec3& offset);
    glm::vec3 skeleton_pose_get_root_offset(uint32_t pose_id) const;
    int skeleton_pose_get_joint_count(uint32_t pose_id) const;

    // SkeletonMapper
    uint32_t create_skeleton_mapper(uint32_t skeleton_low_id, uint32_t skeleton_high_id, uint32_t neutral_pose_low_id, uint32_t neutral_pose_high_id);
    bool destroy_skeleton_mapper(uint32_t id);
    void skeleton_mapper_map(uint32_t mapper_id, uint32_t pose_low_id, uint32_t pose_high_local_id, uint32_t pose_high_out_model_id);
    void skeleton_mapper_map_reverse(uint32_t mapper_id, uint32_t pose_high_model_id, uint32_t pose_low_out_model_id);
    void skeleton_mapper_lock_all_translations(uint32_t mapper_id, uint32_t skeleton_high_id, uint32_t neutral_pose_high_id);

    // Ragdoll
    uint32_t create_ragdoll(const RagdollConfig& config);
    bool destroy_ragdoll(uint32_t id);
    void ragdoll_set_pose(uint32_t ragdoll_id, uint32_t pose_id);
    void ragdoll_drive_to_pose_kinematics(uint32_t ragdoll_id, uint32_t pose_id, float dt);
    void ragdoll_drive_to_pose_motors(uint32_t ragdoll_id, uint32_t pose_id);
    void ragdoll_drive_to_pose_motors_velocity(uint32_t ragdoll_id, uint32_t prev_pose_id, uint32_t pose_id, float dt);
    void ragdoll_get_pose(uint32_t ragdoll_id, uint32_t pose_id) const;
    void ragdoll_set_hard_keying(uint32_t ragdoll_id, bool hard_keying);
    void ragdoll_activate(uint32_t ragdoll_id);
    bool ragdoll_is_active(uint32_t ragdoll_id) const;
    uint32_t ragdoll_get_body_id(uint32_t ragdoll_id, int part_idx) const;
    int ragdoll_get_part_count(uint32_t ragdoll_id) const;
    uint32_t ragdoll_get_skeleton_id(uint32_t ragdoll_id) const;

    // ========================================================================
    // Soft Body Simulation (Cloth, Soft Balls, Volumetric Jellies, Cosserat Rods)
    // ========================================================================
    struct SoftBodyVertexConfig {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        float inv_mass = 1.0f; // 0.0f = kinematic / pinned
    };

    struct SoftBodyFaceConfig {
        uint32_t v[3] = {0, 0, 0};
        uint32_t material_index = 0;
    };

    struct SoftBodyEdgeConfig {
        uint32_t v[2] = {0, 0};
        float compliance = 0.0f; // 0 = rigid spring
    };

    struct SoftBodyDihedralBendConfig {
        uint32_t v[4] = {0, 0, 0, 0}; // v0, v1 shared edge; v2, v3 opposing vertices
        float compliance = 0.0f;
    };

    struct SoftBodyVolumeConfig {
        uint32_t v[4] = {0, 0, 0, 0}; // 4 vertices of tetrahedron
        float compliance = 0.0f;
    };

    struct SoftBodyLRAConfig {
        uint32_t kinematic_v = 0;
        uint32_t dynamic_v = 0;
        float max_distance = 0.0f; // 0.0 means calculate from rest pose
    };

    struct SoftBodySkinnedWeightConfig {
        uint32_t joint_index = 0;
        float weight = 0.0f;
    };

    struct SoftBodySkinnedConfig {
        uint32_t vertex = 0;
        float max_distance = 1e30f;      // FLT_MAX = disabled, 0 = hard skin
        float backstop_distance = 1e30f; // distance behind vertex normal
        float backstop_radius = 40.0f;
        std::vector<SoftBodySkinnedWeightConfig> weights;
    };

    struct SoftBodyRodStretchShearConfig {
        uint32_t v[2] = {0, 0};
        float compliance = 0.0f;
    };

    struct SoftBodyRodBendTwistConfig {
        uint32_t rod[2] = {0, 0};
        float compliance = 0.0f;
    };

    enum class SoftBodyBendType {
        None = 0,
        Distance = 1,
        Dihedral = 2
    };

    enum class SoftBodyLRAType {
        None = 0,
        EuclideanDistance = 1,
        GeodesicDistance = 2
    };

    struct SoftBodyConfig {
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        std::vector<SoftBodyVertexConfig> vertices;
        std::vector<SoftBodyFaceConfig> faces;
        std::vector<SoftBodyEdgeConfig> edge_constraints;
        std::vector<SoftBodyDihedralBendConfig> dihedral_bend_constraints;
        std::vector<SoftBodyVolumeConfig> volume_constraints;
        std::vector<SoftBodyLRAConfig> lra_constraints;
        std::vector<SoftBodySkinnedConfig> skinned_constraints;
        std::vector<SoftBodyRodStretchShearConfig> rod_stretch_shear_constraints;
        std::vector<SoftBodyRodBendTwistConfig> rod_bend_twist_constraints;

        // Dynamics & Material
        float pressure = 0.0f; // n * R * T ideal gas internal pressure
        float vertex_radius = 0.0f;
        float linear_damping = 0.1f;
        float max_linear_velocity = 500.0f;
        float friction = 0.2f;
        float restitution = 0.0f;
        float gravity_factor = 1.0f;
        uint32_t num_iterations = 5;
        bool update_position = true;
        bool make_rotation_identity = true;
        bool allow_sleeping = true;
        bool faces_double_sided = true;

        // Auto constraint generation options
        bool auto_generate_constraints = false;
        SoftBodyBendType auto_bend_type = SoftBodyBendType::Distance;
        SoftBodyLRAType auto_lra_type = SoftBodyLRAType::None;
        float auto_compliance = 0.0f;
        float auto_shear_compliance = 0.0f;
        float auto_bend_compliance = 0.01f;
        float auto_lra_multiplier = 1.0f;
    };

    // Soft Body Creation & Management
    uint32_t create_soft_body(const SoftBodyConfig& config);
    uint32_t create_soft_body_cloth(const glm::vec3& origin, float width, float height, int segments_x, int segments_y, float compliance = 0.0f, float bend_compliance = 0.01f, bool pin_top_corners = true, bool add_lra = true);
    uint32_t create_soft_body_cube(const glm::vec3& origin, float size, int grid_size = 3, float compliance = 0.0f, float pressure = 0.0f);
    uint32_t create_soft_body_sphere(const glm::vec3& origin, float radius, int rings = 8, int sectors = 12, float compliance = 0.0f, float pressure = 500.0f);
    uint32_t create_soft_body_rod(const std::vector<glm::vec3>& points, float stretch_compliance = 0.0f, float bend_twist_compliance = 0.001f, bool pin_root = true);
    bool destroy_soft_body(uint32_t id);
    bool is_soft_body(uint32_t id) const;

    // Soft Body State Queries & Modification
    uint32_t get_soft_body_vertex_count(uint32_t id) const;
    glm::vec3 get_soft_body_vertex_position(uint32_t id, uint32_t v_idx) const;
    void set_soft_body_vertex_position(uint32_t id, uint32_t v_idx, const glm::vec3& pos);
    glm::vec3 get_soft_body_vertex_velocity(uint32_t id, uint32_t v_idx) const;
    void set_soft_body_vertex_velocity(uint32_t id, uint32_t v_idx, const glm::vec3& vel);
    float get_soft_body_vertex_inv_mass(uint32_t id, uint32_t v_idx) const;
    void set_soft_body_vertex_inv_mass(uint32_t id, uint32_t v_idx, float inv_mass);

    void get_soft_body_vertices(uint32_t id, std::vector<glm::vec3>& out_positions) const;
    void get_soft_body_faces(uint32_t id, std::vector<uint32_t>& out_indices) const;

    void set_soft_body_pressure(uint32_t id, float pressure);
    float get_soft_body_pressure(uint32_t id) const;
    void set_soft_body_num_iterations(uint32_t id, uint32_t iters);
    uint32_t get_soft_body_num_iterations(uint32_t id) const;
    float get_soft_body_volume(uint32_t id) const;

    void add_soft_body_impulse_to_vertex(uint32_t id, uint32_t v_idx, const glm::vec3& impulse);
    void add_soft_body_force_to_vertex(uint32_t id, uint32_t v_idx, const glm::vec3& force);

    void skin_soft_body_vertices(uint32_t id, const std::vector<glm::mat4>& joint_matrices, bool hard_skin = false);
    void set_soft_body_skinned_max_distance_multiplier(uint32_t id, float multiplier);
    bool get_soft_body_rod_transform(uint32_t id, uint32_t rod_idx, glm::vec3& out_pos, glm::quat& out_rot) const;
    uint32_t soft_body_get_body_id(uint32_t id) const;
    void activate_soft_body(uint32_t id);
    bool is_soft_body_active(uint32_t id) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace crayon
