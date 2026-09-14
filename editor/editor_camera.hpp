#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include "../../src/graphics/camera.hpp"

namespace crayon::editor {

class EditorCamera {
public:
    EditorCamera() {
        reset();
    }

    void reset() {
        m_position = glm::vec3(0.0f, 6.0f, 12.0f);
        m_target = glm::vec3(0.0f, 0.0f, 0.0f);
        m_yaw = -90.0f;
        m_pitch = -25.0f;
        update_vectors();
    }

    void process_mouse_movement(float xoffset, float yoffset, bool constrain_pitch = true) {
        float sensitivity = 0.15f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        m_yaw += xoffset;
        m_pitch -= yoffset; // Inverted Y for intuitive camera look

        if (constrain_pitch) {
            if (m_pitch > 89.0f) m_pitch = 89.0f;
            if (m_pitch < -89.0f) m_pitch = -89.0f;
        }

        update_vectors();
    }

    void process_mouse_pan(float xoffset, float yoffset) {
        float pan_speed = 0.02f * (glm::length(m_position - m_target) * 0.1f + 0.5f);
        m_position -= m_right * (xoffset * pan_speed);
        m_position += m_up * (yoffset * pan_speed);
        m_target -= m_right * (xoffset * pan_speed);
        m_target += m_up * (yoffset * pan_speed);
    }

    void process_mouse_scroll(float yoffset) {
        float zoom_speed = 1.0f;
        glm::vec3 dir = m_front;
        m_position += dir * (yoffset * zoom_speed);
        // Don't overshoot target too far
    }

    void process_keyboard(int direction, float dt) {
        // 0=Forward, 1=Back, 2=Left, 3=Right, 4=Up, 5=Down
        float velocity = m_move_speed * dt;
        if (direction == 0) m_position += m_front * velocity;
        if (direction == 1) m_position -= m_front * velocity;
        if (direction == 2) m_position -= m_right * velocity;
        if (direction == 3) m_position += m_right * velocity;
        if (direction == 4) m_position += glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
        if (direction == 5) m_position -= glm::vec3(0.0f, 1.0f, 0.0f) * velocity;

        m_target = m_position + m_front * 5.0f;
    }

    crayon::Camera to_crayon_camera() const {
        crayon::Camera cam;
        cam.set_position(m_position);
        cam.set_target(m_position + m_front);
        cam.set_up(m_up);
        cam.set_fov(60.0f);
        cam.set_near(0.1f);
        cam.set_far(1000.0f);
        return cam;
    }

    float& get_move_speed() { return m_move_speed; }
    const glm::vec3& get_position() const { return m_position; }

private:
    void update_vectors() {
        glm::vec3 front;
        float yaw_rad = glm::radians(m_yaw);
        float pitch_rad = glm::radians(m_pitch);
        front.x = cos(yaw_rad) * cos(pitch_rad);
        front.y = sin(pitch_rad);
        front.z = sin(yaw_rad) * cos(pitch_rad);
        m_front = glm::normalize(front);
        m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
        m_up = glm::normalize(glm::cross(m_right, m_front));
    }

    glm::vec3 m_position{0.0f, 6.0f, 12.0f};
    glm::vec3 m_target{0.0f, 0.0f, 0.0f};
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};

    float m_yaw = -90.0f;
    float m_pitch = -25.0f;
    float m_move_speed = 8.0f;
};

} // namespace crayon::editor
