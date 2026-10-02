#pragma once

#include <glm/glm.hpp>
#include <string>
#include <random>

namespace crayon {

enum class CameraEase {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    InOutQuad,
    OutCubic,
    InOutCubic
};

class Camera2D {
public:
    Camera2D(float x = 0.0f, float y = 0.0f);

    // Target tracking
    void setTarget(float x, float y);
    void clearTarget();
    bool hasTarget() const { return m_hasTarget; }
    glm::vec2 getTarget() const { return m_target; }

    // Bounding room
    void setBounds(float x, float y, float w, float h);
    void clearBounds();
    bool hasBounds() const { return m_hasBounds; }
    glm::vec4 getBounds() const { return m_bounds; }

    // Deadzone
    void setDeadzone(float w, float h);
    glm::vec2 getDeadzone() const { return m_deadzone; }

    // Lookahead
    void setLookahead(float dx, float dy);
    glm::vec2 getLookahead() const { return m_lookahead; }

    // Smoothing lerp (speed in units/sec or exponential smoothing factor)
    void setFollowLerp(float speed);
    float getFollowLerp() const { return m_followLerp; }

    // Screen Shake
    void shake(float intensity, float duration, float frequency = 30.0f);
    bool isShaking() const { return m_shakeTimer > 0.0f; }

    // Room transition / MoveTo
    void moveTo(float x, float y, float duration, CameraEase ease = CameraEase::EaseInOut);
    bool isTransitioning() const { return m_transitioning; }
    bool isMoving() const { return m_transitioning; }

    // Update & Apply
    void update(float dt);
    void apply();

    // Viewport query
    void getVisibleRect(float& outX, float& outY, float& outW, float& outH) const;

    // Transform properties
    void setPosition(float x, float y);
    glm::vec2 getPosition() const { return m_position; }

    void setZoom(float zoom);
    float getZoom() const { return m_zoom; }

    void setRotation(float angleRad);
    float getRotation() const { return m_rotation; }

    void setOffset(float ox, float oy);
    glm::vec2 getOffset() const { return m_offset; }
    bool hasCustomOffset() const { return m_hasCustomOffset; }

    // Coordinate conversions
    glm::vec2 screenToWorld(float sx, float sy) const;
    glm::vec2 worldToScreen(float wx, float wy) const;

private:
    float evaluateEase(float t, CameraEase ease) const;

    glm::vec2 m_position{0.0f, 0.0f};
    glm::vec2 m_target{0.0f, 0.0f};
    bool m_hasTarget = false;

    glm::vec4 m_bounds{0.0f, 0.0f, 0.0f, 0.0f}; // x, y, w, h
    bool m_hasBounds = false;

    glm::vec2 m_deadzone{0.0f, 0.0f};
    glm::vec2 m_lookahead{0.0f, 0.0f};
    float m_followLerp = 10.0f; // Fast, smooth responsive follow by default

    // Shake state
    float m_shakeIntensity = 0.0f;
    float m_shakeDuration = 0.0f;
    float m_shakeTimer = 0.0f;
    glm::vec2 m_shakeOffset{0.0f, 0.0f};

    // Transition state
    bool m_transitioning = false;
    glm::vec2 m_transitionStart{0.0f, 0.0f};
    glm::vec2 m_transitionEnd{0.0f, 0.0f};
    float m_transitionDuration = 0.0f;
    float m_transitionTimer = 0.0f;
    CameraEase m_transitionEase = CameraEase::InOutQuad;

    // Basic transform
    float m_zoom = 1.0f;
    float m_rotation = 0.0f;
    glm::vec2 m_offset{0.0f, 0.0f};
    bool m_hasCustomOffset = false;
};

} // namespace crayon
