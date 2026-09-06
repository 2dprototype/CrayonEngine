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

    // Debug visualization
    void draw_debug(MeshRenderer3D& renderer, const glm::vec4& active_color = glm::vec4(0.2f, 1.0f, 0.4f, 1.0f), const glm::vec4& sleeping_color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));

    // Stats
    uint32_t get_num_bodies() const;
    uint32_t get_num_active_bodies() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace crayon
