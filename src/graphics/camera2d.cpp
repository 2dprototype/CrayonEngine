#include "camera2d.hpp"
#include "../core/engine.hpp"
#include <cmath>
#include <algorithm>

namespace crayon {

Camera2D::Camera2D(float x, float y)
    : m_position(x, y), m_target(x, y) {
}

void Camera2D::setTarget(float x, float y) {
    m_target = glm::vec2(x, y);
    m_hasTarget = true;
}

void Camera2D::clearTarget() {
    m_hasTarget = false;
}

void Camera2D::setBounds(float x, float y, float w, float h) {
    m_bounds = glm::vec4(x, y, w, h);
    m_hasBounds = true;
}

void Camera2D::clearBounds() {
    m_hasBounds = false;
}

void Camera2D::setDeadzone(float w, float h) {
    m_deadzone = glm::vec2(std::max(0.0f, w), std::max(0.0f, h));
}

void Camera2D::setLookahead(float dx, float dy) {
    m_lookahead = glm::vec2(dx, dy);
}

void Camera2D::setFollowLerp(float speed) {
    m_followLerp = std::max(0.0f, speed);
}

void Camera2D::shake(float intensity, float duration, float /*frequency*/) {
    m_shakeIntensity = std::max(m_shakeIntensity, intensity);
    m_shakeDuration = std::max(m_shakeDuration, duration);
    m_shakeTimer = m_shakeDuration;
}

void Camera2D::moveTo(float x, float y, float duration, CameraEase ease) {
    if (duration <= 0.0001f) {
        m_position = glm::vec2(x, y);
        m_transitioning = false;
        return;
    }
    m_transitionStart = m_position;
    m_transitionEnd = glm::vec2(x, y);
    m_transitionDuration = duration;
    m_transitionTimer = 0.0f;
    m_transitionEase = ease;
    m_transitioning = true;
}

float Camera2D::evaluateEase(float t, CameraEase ease) const {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (ease) {
        case CameraEase::Linear:
            return t;
        case CameraEase::EaseIn:
            return t * t;
        case CameraEase::EaseOut:
            return t * (2.0f - t);
        case CameraEase::EaseInOut:
        case CameraEase::InOutQuad:
            return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
        case CameraEase::OutCubic: {
            float f = t - 1.0f;
            return f * f * f + 1.0f;
        }
        case CameraEase::InOutCubic:
            return (t < 0.5f) ? (4.0f * t * t * t) : ((t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f);
        default:
            return t;
    }
}

void Camera2D::update(float dt) {
    if (dt <= 0.0f) return;

    // 1. Room Transition (MoveTo)
    if (m_transitioning) {
        m_transitionTimer += dt;
        float progress = m_transitionTimer / m_transitionDuration;
        if (progress >= 1.0f) {
            m_position = m_transitionEnd;
            m_transitioning = false;
        } else {
            float eased = evaluateEase(progress, m_transitionEase);
            m_position = glm::mix(m_transitionStart, m_transitionEnd, eased);
        }
    }
    // 2. Follow target with deadzone & lookahead
    else if (m_hasTarget) {
        glm::vec2 desired = m_target + m_lookahead;

        // Apply deadzone
        if (m_deadzone.x > 0.0f) {
            float diffX = desired.x - m_position.x;
            float halfDw = m_deadzone.x * 0.5f;
            if (std::abs(diffX) <= halfDw) {
                desired.x = m_position.x;
            } else if (diffX > halfDw) {
                desired.x -= halfDw;
            } else {
                desired.x += halfDw;
            }
        }

        if (m_deadzone.y > 0.0f) {
            float diffY = desired.y - m_position.y;
            float halfDh = m_deadzone.y * 0.5f;
            if (std::abs(diffY) <= halfDh) {
                desired.y = m_position.y;
            } else if (diffY > halfDh) {
                desired.y -= halfDh;
            } else {
                desired.y += halfDh;
            }
        }

        // Smooth follow lerp (frame-rate independent)
        float factor = 1.0f - std::exp(-m_followLerp * dt);
        m_position = glm::mix(m_position, desired, factor);
    }

    // 3. Clamp to bounds if active
    if (m_hasBounds) {
        float vx, vy, vw, vh;
        getVisibleRect(vx, vy, vw, vh);

        float halfW = vw * 0.5f;
        float halfH = vh * 0.5f;

        if (m_bounds.z > vw) {
            m_position.x = std::clamp(m_position.x, m_bounds.x + halfW, m_bounds.x + m_bounds.z - halfW);
        } else {
            m_position.x = m_bounds.x + m_bounds.z * 0.5f;
        }

        if (m_bounds.w > vh) {
            m_position.y = std::clamp(m_position.y, m_bounds.y + halfH, m_bounds.y + m_bounds.w - halfH);
        } else {
            m_position.y = m_bounds.y + m_bounds.w * 0.5f;
        }
    }

    // 4. Update screen shake
    if (m_shakeTimer > 0.0f) {
        m_shakeTimer -= dt;
        float currentIntensity = m_shakeIntensity * std::max(0.0f, m_shakeTimer / m_shakeDuration);

        // Deterministic high-frequency pseudo-random noise using sine harmonics
        float timeVal = m_shakeDuration - m_shakeTimer;
        float rx = std::sin(timeVal * 78.233f) * std::cos(timeVal * 43.123f);
        float ry = std::cos(timeVal * 67.543f) * std::sin(timeVal * 89.912f);

        m_shakeOffset = glm::vec2(rx * currentIntensity, ry * currentIntensity);

        if (m_shakeTimer <= 0.0f) {
            m_shakeOffset = glm::vec2(0.0f);
            m_shakeIntensity = 0.0f;
            m_shakeDuration = 0.0f;
        }
    } else {
        m_shakeOffset = glm::vec2(0.0f);
    }
}

void Camera2D::apply() {
    int virtW = Engine::get().get_window().get_virtual_width();
    int virtH = Engine::get().get_window().get_virtual_height();

    glm::vec2 effectiveOffset = m_hasCustomOffset
        ? m_offset
        : glm::vec2(static_cast<float>(virtW) * 0.5f, static_cast<float>(virtH) * 0.5f);

    glm::vec2 finalPos = m_position + m_shakeOffset;

    Engine::get().get_batch2d().set_camera2d(
        finalPos.x,
        finalPos.y,
        m_zoom,
        m_rotation,
        effectiveOffset.x,
        effectiveOffset.y
    );
}

void Camera2D::getVisibleRect(float& outX, float& outY, float& outW, float& outH) const {
    int virtW = Engine::get().get_window().get_virtual_width();
    int virtH = Engine::get().get_window().get_virtual_height();

    float z = (m_zoom > 0.0001f) ? m_zoom : 1.0f;
    outW = static_cast<float>(virtW) / z;
    outH = static_cast<float>(virtH) / z;

    glm::vec2 effectiveOffset = m_hasCustomOffset
        ? m_offset
        : glm::vec2(static_cast<float>(virtW) * 0.5f, static_cast<float>(virtH) * 0.5f);

    glm::vec2 renderPos = m_position + m_shakeOffset;
    outX = renderPos.x - (effectiveOffset.x / z);
    outY = renderPos.y - (effectiveOffset.y / z);
}

void Camera2D::setPosition(float x, float y) {
    m_position = glm::vec2(x, y);
}

void Camera2D::setZoom(float zoom) {
    m_zoom = std::max(0.001f, zoom);
}

void Camera2D::setRotation(float angleRad) {
    m_rotation = angleRad;
}

void Camera2D::setOffset(float ox, float oy) {
    m_offset = glm::vec2(ox, oy);
    m_hasCustomOffset = true;
}

glm::vec2 Camera2D::screenToWorld(float sx, float sy) const {
    int virtW = Engine::get().get_window().get_virtual_width();
    int virtH = Engine::get().get_window().get_virtual_height();
    glm::vec2 offset = m_hasCustomOffset ? m_offset : glm::vec2(virtW * 0.5f, virtH * 0.5f);

    float dx = sx - offset.x;
    float dy = sy - offset.y;

    if (m_rotation != 0.0f) {
        float cosA = std::cos(-m_rotation);
        float sinA = std::sin(-m_rotation);
        float rx = dx * cosA - dy * sinA;
        float ry = dx * sinA + dy * cosA;
        dx = rx;
        dy = ry;
    }

    float z = (m_zoom > 0.0001f) ? m_zoom : 1.0f;
    return (m_position + m_shakeOffset) + glm::vec2(dx / z, dy / z);
}

glm::vec2 Camera2D::worldToScreen(float wx, float wy) const {
    int virtW = Engine::get().get_window().get_virtual_width();
    int virtH = Engine::get().get_window().get_virtual_height();
    glm::vec2 offset = m_hasCustomOffset ? m_offset : glm::vec2(virtW * 0.5f, virtH * 0.5f);

    glm::vec2 diff = glm::vec2(wx, wy) - (m_position + m_shakeOffset);
    float z = (m_zoom > 0.0001f) ? m_zoom : 1.0f;
    float dx = diff.x * z;
    float dy = diff.y * z;

    if (m_rotation != 0.0f) {
        float cosA = std::cos(m_rotation);
        float sinA = std::sin(m_rotation);
        float rx = dx * cosA - dy * sinA;
        float ry = dx * sinA + dy * cosA;
        dx = rx;
        dy = ry;
    }

    return offset + glm::vec2(dx, dy);
}

} // namespace crayon
