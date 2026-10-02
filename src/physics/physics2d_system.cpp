#include "physics2d_system.hpp"
#include "../graphics/batch2d.hpp"
#include "../core/log.hpp"
#include <cmath>
#include <algorithm>

namespace crayon {

class Raycast2DCallback : public b2RayCastCallback {
public:
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        hit.hit = true;
        hit.point = glm::vec2(point.x, point.y);
        hit.normal = glm::vec2(normal.x, normal.y);
        hit.fraction = fraction;
        hit.bodyId = static_cast<uint32_t>(fixture->GetBody()->GetUserData().pointer);
        return fraction; // Clip ray to closest hit
    }
    Raycast2DHit hit;
};

Physics2DSystem::Physics2DSystem() = default;

Physics2DSystem::~Physics2DSystem() {
    shutdown();
}

bool Physics2DSystem::init(float gravityX, float gravityY, float meterScale) {
    m_meterScale = (meterScale > 0.001f) ? meterScale : 32.0f;
    b2Vec2 gravity(gravityX, gravityY);
    m_world = std::make_unique<b2World>(gravity);
    m_world->SetContactListener(this);

    m_bodies.clear();
    m_bodyToId.clear();
    m_joints.clear();
    m_jointToId.clear();
    m_events.clear();
    m_nextBodyId = 1;
    m_nextJointId = 1;

    CRAYON_LOG_INFO("Physics2D System (Box2D) initialized (Gravity: {:.2f}, {:.2f}, Scale: {:.1f} px/m)",
                    gravityX, gravityY, m_meterScale);
    return true;
}

void Physics2DSystem::shutdown() {
    if (m_world) {
        m_bodies.clear();
        m_bodyToId.clear();
        m_joints.clear();
        m_jointToId.clear();
        m_events.clear();
        m_world.reset();
        CRAYON_LOG_INFO("Physics2D System (Box2D) shutdown");
    }
}

void Physics2DSystem::update(float dt, int velocityIterations, int positionIterations) {
    if (!m_world || dt <= 0.0f) return;
    m_world->Step(dt, velocityIterations, positionIterations);
}

void Physics2DSystem::setGravity(float gx, float gy) {
    if (m_world) {
        m_world->SetGravity(b2Vec2(gx, gy));
    }
}

glm::vec2 Physics2DSystem::getGravity() const {
    if (m_world) {
        b2Vec2 g = m_world->GetGravity();
        return glm::vec2(g.x, g.y);
    }
    return glm::vec2(0.0f, 9.8f);
}

void Physics2DSystem::setMeterScale(float scale) {
    if (scale > 0.001f) {
        m_meterScale = scale;
    }
}

uint32_t Physics2DSystem::createBody(b2BodyType type, float pixelX, float pixelY, const Body2DOptions& options) {
    if (!m_world) return 0;

    b2BodyDef def;
    def.type = type;
    def.position = toMeters(pixelX, pixelY);
    def.fixedRotation = options.fixedRotation;
    def.linearDamping = options.linearDamping;
    def.angularDamping = options.angularDamping;
    def.gravityScale = options.gravityScale;
    def.bullet = options.bullet;
    def.allowSleep = options.allowSleep;

    uint32_t id = m_nextBodyId++;
    def.userData.pointer = static_cast<uintptr_t>(id);

    b2Body* body = m_world->CreateBody(&def);
    if (!body) return 0;

    m_bodies[id] = body;
    m_bodyToId[body] = id;
    return id;
}

bool Physics2DSystem::destroyBody(uint32_t bodyId) {
    auto it = m_bodies.find(bodyId);
    if (it == m_bodies.end() || !m_world) return false;

    b2Body* body = it->second;

    // Clean up associated joints in our map
    b2JointEdge* edge = body->GetJointList();
    while (edge) {
        b2Joint* joint = edge->joint;
        auto jit = m_jointToId.find(joint);
        if (jit != m_jointToId.end()) {
            m_joints.erase(jit->second);
            m_jointToId.erase(jit);
        }
        edge = edge->next;
    }

    m_bodyToId.erase(body);
    m_bodies.erase(it);

    m_world->DestroyBody(body);
    return true;
}

b2Body* Physics2DSystem::getBody(uint32_t bodyId) {
    auto it = m_bodies.find(bodyId);
    return (it != m_bodies.end()) ? it->second : nullptr;
}

bool Physics2DSystem::isValidBody(uint32_t bodyId) const {
    return m_bodies.find(bodyId) != m_bodies.end();
}

bool Physics2DSystem::addBoxFixture(uint32_t bodyId, float w, float h, float ox, float oy, float angleRad,
                                   float density, float friction, float restitution, bool isSensor) {
    b2Body* body = getBody(bodyId);
    if (!body) return false;

    b2PolygonShape shape;
    b2Vec2 center = toMeters(ox, oy);
    shape.SetAsBox(toMeters(w * 0.5f), toMeters(h * 0.5f), center, angleRad);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = density;
    fixtureDef.friction = friction;
    fixtureDef.restitution = restitution;
    fixtureDef.isSensor = isSensor;

    body->CreateFixture(&fixtureDef);
    return true;
}

bool Physics2DSystem::addCircleFixture(uint32_t bodyId, float radius, float ox, float oy,
                                      float density, float friction, float restitution, bool isSensor) {
    b2Body* body = getBody(bodyId);
    if (!body) return false;

    b2CircleShape shape;
    shape.m_p = toMeters(ox, oy);
    shape.m_radius = toMeters(radius);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = density;
    fixtureDef.friction = friction;
    fixtureDef.restitution = restitution;
    fixtureDef.isSensor = isSensor;

    body->CreateFixture(&fixtureDef);
    return true;
}

bool Physics2DSystem::addPolygonFixture(uint32_t bodyId, const std::vector<glm::vec2>& vertices,
                                       float density, float friction, float restitution, bool isSensor) {
    b2Body* body = getBody(bodyId);
    if (!body || vertices.size() < 3 || vertices.size() > b2_maxPolygonVertices) return false;

    std::vector<b2Vec2> b2verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        b2verts[i] = toMeters(vertices[i].x, vertices[i].y);
    }

    b2PolygonShape shape;
    shape.Set(b2verts.data(), static_cast<int32>(b2verts.size()));

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = density;
    fixtureDef.friction = friction;
    fixtureDef.restitution = restitution;
    fixtureDef.isSensor = isSensor;

    body->CreateFixture(&fixtureDef);
    return true;
}

bool Physics2DSystem::addEdgeFixture(uint32_t bodyId, float x1, float y1, float x2, float y2,
                                    float friction, float restitution, bool isSensor) {
    b2Body* body = getBody(bodyId);
    if (!body) return false;

    b2EdgeShape shape;
    shape.SetTwoSided(toMeters(x1, y1), toMeters(x2, y2));

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.friction = friction;
    fixtureDef.restitution = restitution;
    fixtureDef.isSensor = isSensor;

    body->CreateFixture(&fixtureDef);
    return true;
}

bool Physics2DSystem::addChainFixture(uint32_t bodyId, const std::vector<glm::vec2>& vertices, bool loop,
                                     float friction, float restitution, bool isSensor) {
    b2Body* body = getBody(bodyId);
    if (!body || vertices.size() < 2) return false;

    std::vector<b2Vec2> b2verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        b2verts[i] = toMeters(vertices[i].x, vertices[i].y);
    }

    b2ChainShape shape;
    if (loop) {
        shape.CreateLoop(b2verts.data(), static_cast<int32>(b2verts.size()));
    } else {
        shape.CreateChain(b2verts.data(), static_cast<int32>(b2verts.size()), b2verts.front(), b2verts.back());
    }

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.friction = friction;
    fixtureDef.restitution = restitution;
    fixtureDef.isSensor = isSensor;

    body->CreateFixture(&fixtureDef);
    return true;
}

uint32_t Physics2DSystem::createDistanceJoint(uint32_t bodyA, uint32_t bodyB, float ax1, float ay1, float ax2, float ay2,
                                             float length, float frequencyHz, float dampingRatio, bool collideConnected) {
    b2Body* bA = getBody(bodyA);
    b2Body* bB = getBody(bodyB);
    if (!bA || !bB || !m_world) return 0;

    b2DistanceJointDef def;
    def.Initialize(bA, bB, toMeters(ax1, ay1), toMeters(ax2, ay2));
    if (length > 0.0f) {
        def.length = toMeters(length);
    }
    if (frequencyHz > 0.0f) {
        b2LinearStiffness(def.stiffness, def.damping, frequencyHz, dampingRatio, def.bodyA, def.bodyB);
    }
    def.collideConnected = collideConnected;

    b2Joint* joint = m_world->CreateJoint(&def);
    if (!joint) return 0;

    uint32_t id = m_nextJointId++;
    m_joints[id] = joint;
    m_jointToId[joint] = id;
    return id;
}

uint32_t Physics2DSystem::createRevoluteJoint(uint32_t bodyA, uint32_t bodyB, float anchorX, float anchorY,
                                             bool enableLimit, float lowerAngle, float upperAngle,
                                             bool enableMotor, float motorSpeed, float maxTorque, bool collideConnected) {
    b2Body* bA = getBody(bodyA);
    b2Body* bB = getBody(bodyB);
    if (!bA || !bB || !m_world) return 0;

    b2RevoluteJointDef def;
    def.Initialize(bA, bB, toMeters(anchorX, anchorY));
    def.enableLimit = enableLimit;
    def.lowerAngle = lowerAngle;
    def.upperAngle = upperAngle;
    def.enableMotor = enableMotor;
    def.motorSpeed = motorSpeed;
    def.maxMotorTorque = maxTorque;
    def.collideConnected = collideConnected;

    b2Joint* joint = m_world->CreateJoint(&def);
    if (!joint) return 0;

    uint32_t id = m_nextJointId++;
    m_joints[id] = joint;
    m_jointToId[joint] = id;
    return id;
}

uint32_t Physics2DSystem::createPrismaticJoint(uint32_t bodyA, uint32_t bodyB, float anchorX, float anchorY, float axisX, float axisY,
                                              bool enableLimit, float lowerTranslation, float upperTranslation,
                                              bool enableMotor, float motorSpeed, float maxForce, bool collideConnected) {
    b2Body* bA = getBody(bodyA);
    b2Body* bB = getBody(bodyB);
    if (!bA || !bB || !m_world) return 0;

    b2PrismaticJointDef def;
    def.Initialize(bA, bB, toMeters(anchorX, anchorY), b2Vec2(axisX, axisY));
    def.enableLimit = enableLimit;
    def.lowerTranslation = toMeters(lowerTranslation);
    def.upperTranslation = toMeters(upperTranslation);
    def.enableMotor = enableMotor;
    def.motorSpeed = motorSpeed;
    def.maxMotorForce = maxForce;
    def.collideConnected = collideConnected;

    b2Joint* joint = m_world->CreateJoint(&def);
    if (!joint) return 0;

    uint32_t id = m_nextJointId++;
    m_joints[id] = joint;
    m_jointToId[joint] = id;
    return id;
}

uint32_t Physics2DSystem::createWeldJoint(uint32_t bodyA, uint32_t bodyB, float anchorX, float anchorY,
                                         float frequencyHz, float dampingRatio, bool collideConnected) {
    b2Body* bA = getBody(bodyA);
    b2Body* bB = getBody(bodyB);
    if (!bA || !bB || !m_world) return 0;

    b2WeldJointDef def;
    def.Initialize(bA, bB, toMeters(anchorX, anchorY));
    if (frequencyHz > 0.0f) {
        b2AngularStiffness(def.stiffness, def.damping, frequencyHz, dampingRatio, def.bodyA, def.bodyB);
    }
    def.collideConnected = collideConnected;

    b2Joint* joint = m_world->CreateJoint(&def);
    if (!joint) return 0;

    uint32_t id = m_nextJointId++;
    m_joints[id] = joint;
    m_jointToId[joint] = id;
    return id;
}

uint32_t Physics2DSystem::createWheelJoint(uint32_t bodyA, uint32_t bodyB, float anchorX, float anchorY, float axisX, float axisY,
                                          float frequencyHz, float dampingRatio, bool collideConnected) {
    b2Body* bA = getBody(bodyA);
    b2Body* bB = getBody(bodyB);
    if (!bA || !bB || !m_world) return 0;

    b2WheelJointDef def;
    def.Initialize(bA, bB, toMeters(anchorX, anchorY), b2Vec2(axisX, axisY));
    if (frequencyHz > 0.0f) {
        b2LinearStiffness(def.stiffness, def.damping, frequencyHz, dampingRatio, def.bodyA, def.bodyB);
    }
    def.collideConnected = collideConnected;

    b2Joint* joint = m_world->CreateJoint(&def);
    if (!joint) return 0;

    uint32_t id = m_nextJointId++;
    m_joints[id] = joint;
    m_jointToId[joint] = id;
    return id;
}

bool Physics2DSystem::destroyJoint(uint32_t jointId) {
    auto it = m_joints.find(jointId);
    if (it == m_joints.end() || !m_world) return false;

    b2Joint* joint = it->second;
    m_jointToId.erase(joint);
    m_joints.erase(it);

    m_world->DestroyJoint(joint);
    return true;
}

Raycast2DHit Physics2DSystem::raycast(float x1, float y1, float x2, float y2) {
    Raycast2DHit res;
    if (!m_world) return res;

    b2Vec2 p1 = toMeters(x1, y1);
    b2Vec2 p2 = toMeters(x2, y2);

    Raycast2DCallback cb;
    m_world->RayCast(&cb, p1, p2);

    if (cb.hit.hit) {
        res = cb.hit;
        res.point = glm::vec2(toPixels(res.point.x), toPixels(res.point.y));
    }
    return res;
}

std::vector<Physics2DEvent> Physics2DSystem::getAndClearEvents() {
    std::vector<Physics2DEvent> out = std::move(m_events);
    m_events.clear();
    return out;
}

void Physics2DSystem::BeginContact(b2Contact* contact) {
    b2Fixture* fA = contact->GetFixtureA();
    b2Fixture* fB = contact->GetFixtureB();
    if (!fA || !fB) return;

    uint32_t idA = static_cast<uint32_t>(fA->GetBody()->GetUserData().pointer);
    uint32_t idB = static_cast<uint32_t>(fB->GetBody()->GetUserData().pointer);
    if (idA == 0 || idB == 0) return;

    bool isSensor = fA->IsSensor() || fB->IsSensor();

    Physics2DEvent evt;
    evt.type = isSensor ? Physics2DEventType::TriggerEnter : Physics2DEventType::CollisionEnter;
    evt.bodyA = idA;
    evt.bodyB = idB;

    b2WorldManifold worldManifold;
    contact->GetWorldManifold(&worldManifold);
    evt.normal = glm::vec2(worldManifold.normal.x, worldManifold.normal.y);
    evt.impulse = 0.0f;

    m_events.push_back(evt);
}

void Physics2DSystem::EndContact(b2Contact* contact) {
    b2Fixture* fA = contact->GetFixtureA();
    b2Fixture* fB = contact->GetFixtureB();
    if (!fA || !fB) return;

    uint32_t idA = static_cast<uint32_t>(fA->GetBody()->GetUserData().pointer);
    uint32_t idB = static_cast<uint32_t>(fB->GetBody()->GetUserData().pointer);
    if (idA == 0 || idB == 0) return;

    bool isSensor = fA->IsSensor() || fB->IsSensor();

    Physics2DEvent evt;
    evt.type = isSensor ? Physics2DEventType::TriggerExit : Physics2DEventType::CollisionExit;
    evt.bodyA = idA;
    evt.bodyB = idB;
    m_events.push_back(evt);
}

void Physics2DSystem::PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) {
    if (!contact || !impulse || impulse->count == 0) return;

    b2Fixture* fA = contact->GetFixtureA();
    b2Fixture* fB = contact->GetFixtureB();
    if (!fA || !fB) return;

    uint32_t idA = static_cast<uint32_t>(fA->GetBody()->GetUserData().pointer);
    uint32_t idB = static_cast<uint32_t>(fB->GetBody()->GetUserData().pointer);

    float maxImpulse = 0.0f;
    for (int i = 0; i < impulse->count; ++i) {
        maxImpulse = std::max(maxImpulse, impulse->normalImpulses[i]);
    }

    // Update the last matching CollisionEnter event's impulse
    for (auto it = m_events.rbegin(); it != m_events.rend(); ++it) {
        if (it->type == Physics2DEventType::CollisionEnter &&
            ((it->bodyA == idA && it->bodyB == idB) || (it->bodyA == idB && it->bodyB == idA))) {
            it->impulse = maxImpulse;
            break;
        }
    }
}

void Physics2DSystem::drawDebug(Batch2D& batch) {
    if (!m_world) return;

    for (b2Body* b = m_world->GetBodyList(); b; b = b->GetNext()) {
        b2Transform xf = b->GetTransform();

        glm::vec4 color(0.2f, 0.9f, 0.3f, 0.9f); // Dynamic green
        if (b->GetType() == b2_staticBody) {
            color = glm::vec4(0.4f, 0.6f, 0.95f, 0.9f); // Static blue
        } else if (b->GetType() == b2_kinematicBody) {
            color = glm::vec4(0.95f, 0.6f, 0.2f, 0.9f); // Kinematic orange
        } else if (!b->IsAwake()) {
            color = glm::vec4(0.15f, 0.5f, 0.25f, 0.6f); // Asleep dark green
        }

        for (b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext()) {
            glm::vec4 fixtureColor = f->IsSensor() ? glm::vec4(0.95f, 0.95f, 0.2f, 0.7f) : color;

            switch (f->GetType()) {
                case b2Shape::e_circle: {
                    auto* circle = static_cast<b2CircleShape*>(f->GetShape());
                    b2Vec2 center = b2Mul(xf, circle->m_p);
                    float cx = toPixels(center.x);
                    float cy = toPixels(center.y);
                    float r = toPixels(circle->m_radius);

                    batch.draw_circle(cx, cy, r, fixtureColor, false, 20);

                    // Radius line showing rotation
                    b2Vec2 axis = b2Mul(xf.q, b2Vec2(circle->m_radius, 0.0f));
                    batch.draw_line(cx, cy, cx + toPixels(axis.x), cy + toPixels(axis.y), fixtureColor, 1.0f);
                    break;
                }
                case b2Shape::e_polygon: {
                    auto* poly = static_cast<b2PolygonShape*>(f->GetShape());
                    int32 count = poly->m_count;
                    for (int32 i = 0; i < count; ++i) {
                        b2Vec2 v1 = b2Mul(xf, poly->m_vertices[i]);
                        b2Vec2 v2 = b2Mul(xf, poly->m_vertices[(i + 1) % count]);
                        batch.draw_line(toPixels(v1.x), toPixels(v1.y), toPixels(v2.x), toPixels(v2.y), fixtureColor, 1.0f);
                    }
                    break;
                }
                case b2Shape::e_edge: {
                    auto* edge = static_cast<b2EdgeShape*>(f->GetShape());
                    b2Vec2 v1 = b2Mul(xf, edge->m_vertex1);
                    b2Vec2 v2 = b2Mul(xf, edge->m_vertex2);
                    batch.draw_line(toPixels(v1.x), toPixels(v1.y), toPixels(v2.x), toPixels(v2.y), fixtureColor, 1.0f);
                    break;
                }
                case b2Shape::e_chain: {
                    auto* chain = static_cast<b2ChainShape*>(f->GetShape());
                    int32 count = chain->m_count;
                    for (int32 i = 0; i < count - 1; ++i) {
                        b2Vec2 v1 = b2Mul(xf, chain->m_vertices[i]);
                        b2Vec2 v2 = b2Mul(xf, chain->m_vertices[i + 1]);
                        batch.draw_line(toPixels(v1.x), toPixels(v1.y), toPixels(v2.x), toPixels(v2.y), fixtureColor, 1.0f);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    // Joints debug
    for (b2Joint* j = m_world->GetJointList(); j; j = j->GetNext()) {
        b2Vec2 p1 = j->GetAnchorA();
        b2Vec2 p2 = j->GetAnchorB();
        b2Vec2 b1 = j->GetBodyA()->GetPosition();
        b2Vec2 b2 = j->GetBodyB()->GetPosition();

        batch.draw_line(toPixels(b1.x), toPixels(b1.y), toPixels(p1.x), toPixels(p1.y), glm::vec4(0.8f, 0.4f, 0.4f, 0.6f), 1.0f);
        batch.draw_line(toPixels(p1.x), toPixels(p1.y), toPixels(p2.x), toPixels(p2.y), glm::vec4(1.0f, 0.2f, 0.2f, 0.9f), 1.5f);
        batch.draw_line(toPixels(p2.x), toPixels(p2.y), toPixels(b2.x), toPixels(b2.y), glm::vec4(0.8f, 0.4f, 0.4f, 0.6f), 1.0f);
    }
}

} // namespace crayon
