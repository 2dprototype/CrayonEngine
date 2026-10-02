#pragma once

#include <box2d/box2d.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>

namespace crayon {

class Batch2D;

enum class Physics2DEventType {
    CollisionEnter,
    CollisionExit,
    TriggerEnter,
    TriggerExit
};

struct Physics2DEvent {
    Physics2DEventType type = Physics2DEventType::CollisionEnter;
    uint32_t bodyA = 0;
    uint32_t bodyB = 0;
    glm::vec2 normal{0.0f, 0.0f};
    float impulse = 0.0f;
};

struct Raycast2DHit {
    bool hit = false;
    glm::vec2 point{0.0f, 0.0f};
    glm::vec2 normal{0.0f, 0.0f};
    float fraction = 0.0f;
    uint32_t bodyId = 0;
};

struct Body2DOptions {
    bool fixedRotation = false;
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    float gravityScale = 1.0f;
    bool bullet = false;
    bool allowSleep = true;
};

class Physics2DSystem : public b2ContactListener {
public:
    Physics2DSystem();
    ~Physics2DSystem() override;

    bool init(float gravityX = 0.0f, float gravityY = 9.8f, float meterScale = 32.0f);
    void shutdown();

    void update(float dt, int velocityIterations = 8, int positionIterations = 3);

    // World configuration
    void setGravity(float gx, float gy);
    glm::vec2 getGravity() const;
    void setMeterScale(float scale);
    float getMeterScale() const { return m_meterScale; }

    // Coordinate conversions (pixels <-> meters)
    float toMeters(float pixels) const { return pixels / m_meterScale; }
    float toPixels(float meters) const { return meters * m_meterScale; }
    b2Vec2 toMeters(float px, float py) const { return b2Vec2(px / m_meterScale, py / m_meterScale); }
    glm::vec2 toPixels(const b2Vec2& m) const { return glm::vec2(m.x * m_meterScale, m.y * m_meterScale); }

    // Body Management
    uint32_t createBody(b2BodyType type, float pixelX, float pixelY, const Body2DOptions& options = {});
    bool destroyBody(uint32_t bodyId);
    b2Body* getBody(uint32_t bodyId);
    bool isValidBody(uint32_t bodyId) const;

    // Fixture Creation (all dimensions in PIXELS)
    bool addBoxFixture(uint32_t bodyId, float w, float h, float ox = 0.0f, float oy = 0.0f, float angleRad = 0.0f,
                       float density = 1.0f, float friction = 0.2f, float restitution = 0.0f, bool isSensor = false);
    bool addCircleFixture(uint32_t bodyId, float radius, float ox = 0.0f, float oy = 0.0f,
                          float density = 1.0f, float friction = 0.2f, float restitution = 0.0f, bool isSensor = false);
    bool addPolygonFixture(uint32_t bodyId, const std::vector<glm::vec2>& vertices,
                           float density = 1.0f, float friction = 0.2f, float restitution = 0.0f, bool isSensor = false);
    bool addEdgeFixture(uint32_t bodyId, float x1, float y1, float x2, float y2,
                        float friction = 0.2f, float restitution = 0.0f, bool isSensor = false);
    bool addChainFixture(uint32_t bodyId, const std::vector<glm::vec2>& vertices, bool loop,
                         float friction = 0.2f, float restitution = 0.0f, bool isSensor = false);

    // Joint Management
    uint32_t createDistanceJoint(uint32_t bodyA, uint32_t bodyB, float ax1, float ay1, float ax2, float ay2,
                                 float length = -1.0f, float frequencyHz = 0.0f, float dampingRatio = 0.0f, bool collideConnected = false);
    uint32_t createRevoluteJoint(uint32_t bodyA, uint32_t bodyB, float anchorX, float anchorY,
                                 bool enableLimit = false, float lowerAngle = 0.0f, float upperAngle = 0.0f,
                                 bool enableMotor = false, float motorSpeed = 0.0f, float maxTorque = 0.0f, bool collideConnected = false);
    uint32_t createPrismaticJoint(uint32_t bodyA, uint32_t bodyB, float anchorX, float anchorY, float axisX, float axisY,
                                  bool enableLimit = false, float lowerTranslation = 0.0f, float upperTranslation = 0.0f,
                                  bool enableMotor = false, float motorSpeed = 0.0f, float maxForce = 0.0f, bool collideConnected = false);
    uint32_t createWeldJoint(uint32_t bodyA, uint32_t bodyB, float anchorX, float anchorY,
                             float frequencyHz = 0.0f, float dampingRatio = 0.0f, bool collideConnected = false);
    uint32_t createWheelJoint(uint32_t bodyA, uint32_t bodyB, float anchorX, float anchorY, float axisX, float axisY,
                              float frequencyHz = 2.0f, float dampingRatio = 0.7f, bool collideConnected = false);
    bool destroyJoint(uint32_t jointId);

    // Raycast Query
    Raycast2DHit raycast(float x1, float y1, float x2, float y2);

    // Debug Drawing into Batch2D
    void drawDebug(Batch2D& batch);

    // Contact Events
    std::vector<Physics2DEvent> getAndClearEvents();

    // b2ContactListener callbacks
    void BeginContact(b2Contact* contact) override;
    void EndContact(b2Contact* contact) override;
    void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override;

    b2World* getWorld() { return m_world.get(); }

private:
    std::unique_ptr<b2World> m_world;
    float m_meterScale = 32.0f; // 32 pixels = 1 meter

    uint32_t m_nextBodyId = 1;
    std::unordered_map<uint32_t, b2Body*> m_bodies;
    std::unordered_map<b2Body*, uint32_t> m_bodyToId;

    uint32_t m_nextJointId = 1;
    std::unordered_map<uint32_t, b2Joint*> m_joints;
    std::unordered_map<b2Joint*, uint32_t> m_jointToId;

    std::vector<Physics2DEvent> m_events;
};

} // namespace crayon
