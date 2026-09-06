#include "camera.hpp"

namespace crayon {

Camera::Camera() = default;

glm::mat4 Camera::get_view_matrix() const {
    return glm::lookAt(m_position, m_target, m_up);
}

glm::mat4 Camera::get_projection_matrix(float aspect) const {
    if (m_ortho) {
        float half_h = m_ortho_size * 0.5f;
        float half_w = half_h * aspect;
        return glm::ortho(-half_w, half_w, -half_h, half_h, m_near, m_far);
    }
    return glm::perspective(glm::radians(m_fov), aspect, m_near, m_far);
}

void Camera::get_ray(float norm_screen_x, float norm_screen_y, float aspect, glm::vec3& ray_origin, glm::vec3& ray_dir) const {
    glm::mat4 inv_vp = glm::inverse(get_projection_matrix(aspect) * get_view_matrix());
    glm::vec4 near_pt = inv_vp * glm::vec4(norm_screen_x, norm_screen_y, -1.0f, 1.0f);
    glm::vec4 far_pt  = inv_vp * glm::vec4(norm_screen_x, norm_screen_y,  1.0f, 1.0f);
    if (near_pt.w != 0.0f) near_pt /= near_pt.w;
    if (far_pt.w != 0.0f) far_pt /= far_pt.w;
    ray_origin = glm::vec3(near_pt);
    ray_dir = glm::normalize(glm::vec3(far_pt - near_pt));
}

} // namespace crayon
