#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace crayon {

class Camera {
public:
    Camera();

    void set_position(const glm::vec3& pos) { m_position = pos; }
    const glm::vec3& get_position() const { return m_position; }

    void set_target(const glm::vec3& target) { m_target = target; }
    const glm::vec3& get_target() const { return m_target; }

    void set_up(const glm::vec3& up) { m_up = up; }
    const glm::vec3& get_up() const { return m_up; }

    void set_fov(float fov) { m_fov = fov; }
    float get_fov() const { return m_fov; }

    void set_near(float near_plane) { m_near = near_plane; }
    float get_near() const { return m_near; }

    void set_far(float far_plane) { m_far = far_plane; }
    float get_far() const { return m_far; }

    void set_orthographic(bool ortho) { m_ortho = ortho; }
    bool is_orthographic() const { return m_ortho; }

    void set_ortho_size(float size) { m_ortho_size = size; }
    float get_ortho_size() const { return m_ortho_size; }

    glm::mat4 get_view_matrix() const;
    glm::mat4 get_projection_matrix(float aspect) const;

    void get_ray(float norm_screen_x, float norm_screen_y, float aspect, glm::vec3& ray_origin, glm::vec3& ray_dir) const;

private:
    glm::vec3 m_position{0.0f, 2.0f, 5.0f};
    glm::vec3 m_target{0.0f, 0.0f, 0.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    float m_fov = 60.0f;
    float m_near = 0.1f;
    float m_far = 1000.0f;
    bool m_ortho = false;
    float m_ortho_size = 10.0f;
};

} // namespace crayon
