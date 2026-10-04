#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include "../physics/physics2d_system.hpp"
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

namespace crayon {

struct LuaPhysics2DBody {
    uint32_t id;
    Physics2DSystem* physics;
    bool valid;
};

struct LuaPhysics2DJoint {
    uint32_t id;
    Physics2DSystem* physics;
    bool valid;
};

static LuaPhysics2DBody* check_body2d(lua_State* L, int idx) {
    auto* b = static_cast<LuaPhysics2DBody*>(luaL_checkudata(L, idx, "Physics2D.Body"));
    if (!b || !b->valid || b->id == 0 || !b->physics || !b->physics->isValidBody(b->id)) {
        luaL_error(L, "attempt to use invalid or destroyed Physics2D.Body");
        return nullptr;
    }
    return b;
}

static void push_body2d_userdata(lua_State* L, uint32_t id, Physics2DSystem* physics) {
    auto* udata = static_cast<LuaPhysics2DBody*>(lua_newuserdata(L, sizeof(LuaPhysics2DBody)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics2D.Body");
    lua_setmetatable(L, -2);
}

static LuaPhysics2DJoint* check_joint2d(lua_State* L, int idx) {
    auto* j = static_cast<LuaPhysics2DJoint*>(luaL_checkudata(L, idx, "Physics2D.Joint"));
    if (!j || !j->valid || j->id == 0 || !j->physics) {
        luaL_error(L, "attempt to use invalid or destroyed Physics2D.Joint");
        return nullptr;
    }
    return j;
}

static void push_joint2d_userdata(lua_State* L, uint32_t id, Physics2DSystem* physics) {
    auto* udata = static_cast<LuaPhysics2DJoint*>(lua_newuserdata(L, sizeof(LuaPhysics2DJoint)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics2D.Joint");
    lua_setmetatable(L, -2);
}

// ---------------- Body Methods ----------------

static int l_body2d_add_box(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float w = static_cast<float>(luaL_checknumber(L, 2));
    float h = static_cast<float>(luaL_checknumber(L, 3));
    float ox = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float oy = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float angle = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float density = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    float friction = static_cast<float>(luaL_optnumber(L, 8, 0.2));
    float restitution = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    bool isSensor = lua_toboolean(L, 10);

    bool ok = b->physics->addBoxFixture(b->id, w, h, ox, oy, angle, density, friction, restitution, isSensor);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_body2d_add_circle(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float ox = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float oy = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float density = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float friction = static_cast<float>(luaL_optnumber(L, 6, 0.2));
    float restitution = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    bool isSensor = lua_toboolean(L, 8);

    bool ok = b->physics->addCircleFixture(b->id, r, ox, oy, density, friction, restitution, isSensor);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_body2d_add_polygon(lua_State* L) {
    auto* b = check_body2d(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    int count = static_cast<int>(lua_objlen(L, 2));
    std::vector<glm::vec2> verts;
    verts.reserve(count / 2);

    for (int i = 1; i <= count; i += 2) {
        lua_rawgeti(L, 2, i);
        float x = static_cast<float>(lua_tonumber(L, -1));
        lua_rawgeti(L, 2, i + 1);
        float y = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 2);
        verts.emplace_back(x, y);
    }

    float density = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    float friction = static_cast<float>(luaL_optnumber(L, 4, 0.2));
    float restitution = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    bool isSensor = lua_toboolean(L, 6);

    bool ok = b->physics->addPolygonFixture(b->id, verts, density, friction, restitution, isSensor);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_body2d_add_edge(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float x1 = static_cast<float>(luaL_checknumber(L, 2));
    float y1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float friction = static_cast<float>(luaL_optnumber(L, 6, 0.2));
    float restitution = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    bool isSensor = lua_toboolean(L, 8);

    bool ok = b->physics->addEdgeFixture(b->id, x1, y1, x2, y2, friction, restitution, isSensor);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_body2d_add_chain(lua_State* L) {
    auto* b = check_body2d(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    bool loop = lua_toboolean(L, 3);

    int count = static_cast<int>(lua_objlen(L, 2));
    std::vector<glm::vec2> verts;
    verts.reserve(count / 2);

    for (int i = 1; i <= count; i += 2) {
        lua_rawgeti(L, 2, i);
        float x = static_cast<float>(lua_tonumber(L, -1));
        lua_rawgeti(L, 2, i + 1);
        float y = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 2);
        verts.emplace_back(x, y);
    }

    float friction = static_cast<float>(luaL_optnumber(L, 4, 0.2));
    float restitution = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    bool isSensor = lua_toboolean(L, 6);

    bool ok = b->physics->addChainFixture(b->id, verts, loop, friction, restitution, isSensor);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_body2d_get_position(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b2Body* body = b->physics->getBody(b->id);
    glm::vec2 p = b->physics->toPixels(body->GetPosition());
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    return 2;
}

static int l_body2d_set_position(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    b2Body* body = b->physics->getBody(b->id);
    body->SetTransform(b->physics->toMeters(x, y), body->GetAngle());
    return 0;
}

static int l_body2d_get_angle(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b2Body* body = b->physics->getBody(b->id);
    lua_pushnumber(L, body->GetAngle());
    return 1;
}

static int l_body2d_set_angle(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float angle = static_cast<float>(luaL_checknumber(L, 2));
    b2Body* body = b->physics->getBody(b->id);
    body->SetTransform(body->GetPosition(), angle);
    return 0;
}

static int l_body2d_get_linear_velocity(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b2Body* body = b->physics->getBody(b->id);
    glm::vec2 v = b->physics->toPixels(body->GetLinearVelocity());
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    return 2;
}

static int l_body2d_set_linear_velocity(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float vx = static_cast<float>(luaL_checknumber(L, 2));
    float vy = static_cast<float>(luaL_checknumber(L, 3));
    b2Body* body = b->physics->getBody(b->id);
    body->SetLinearVelocity(b->physics->toMeters(vx, vy));
    return 0;
}

static int l_body2d_get_angular_velocity(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b2Body* body = b->physics->getBody(b->id);
    lua_pushnumber(L, body->GetAngularVelocity());
    return 1;
}

static int l_body2d_set_angular_velocity(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float w = static_cast<float>(luaL_checknumber(L, 2));
    b2Body* body = b->physics->getBody(b->id);
    body->SetAngularVelocity(w);
    return 0;
}

static int l_body2d_apply_force(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float fx = static_cast<float>(luaL_checknumber(L, 2));
    float fy = static_cast<float>(luaL_checknumber(L, 3));
    b2Body* body = b->physics->getBody(b->id);

    b2Vec2 force(fx, fy);
    if (lua_gettop(L) >= 5) {
        float px = static_cast<float>(luaL_checknumber(L, 4));
        float py = static_cast<float>(luaL_checknumber(L, 5));
        body->ApplyForce(force, b->physics->toMeters(px, py), true);
    } else {
        body->ApplyForceToCenter(force, true);
    }
    return 0;
}

static int l_body2d_apply_impulse(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float ix = static_cast<float>(luaL_checknumber(L, 2));
    float iy = static_cast<float>(luaL_checknumber(L, 3));
    b2Body* body = b->physics->getBody(b->id);

    b2Vec2 impulse(ix, iy);
    if (lua_gettop(L) >= 5) {
        float px = static_cast<float>(luaL_checknumber(L, 4));
        float py = static_cast<float>(luaL_checknumber(L, 5));
        body->ApplyLinearImpulse(impulse, b->physics->toMeters(px, py), true);
    } else {
        body->ApplyLinearImpulseToCenter(impulse, true);
    }
    return 0;
}

static int l_body2d_apply_torque(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float torque = static_cast<float>(luaL_checknumber(L, 2));
    b2Body* body = b->physics->getBody(b->id);
    body->ApplyTorque(torque, true);
    return 0;
}

static int l_body2d_apply_angular_impulse(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float imp = static_cast<float>(luaL_checknumber(L, 2));
    b2Body* body = b->physics->getBody(b->id);
    body->ApplyAngularImpulse(imp, true);
    return 0;
}

static int l_body2d_set_gravity_scale(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float s = static_cast<float>(luaL_checknumber(L, 2));
    b2Body* body = b->physics->getBody(b->id);
    body->SetGravityScale(s);
    return 0;
}

static int l_body2d_get_gravity_scale(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b2Body* body = b->physics->getBody(b->id);
    lua_pushnumber(L, body->GetGravityScale());
    return 1;
}

static int l_body2d_set_fixed_rotation(lua_State* L) {
    auto* b = check_body2d(L, 1);
    bool flag = lua_toboolean(L, 2);
    b2Body* body = b->physics->getBody(b->id);
    body->SetFixedRotation(flag);
    return 0;
}

static int l_body2d_is_fixed_rotation(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b2Body* body = b->physics->getBody(b->id);
    lua_pushboolean(L, body->IsFixedRotation());
    return 1;
}

static int l_body2d_set_bullet(lua_State* L) {
    auto* b = check_body2d(L, 1);
    bool flag = lua_toboolean(L, 2);
    b2Body* body = b->physics->getBody(b->id);
    body->SetBullet(flag);
    return 0;
}

static int l_body2d_is_bullet(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b2Body* body = b->physics->getBody(b->id);
    lua_pushboolean(L, body->IsBullet());
    return 1;
}

static int l_body2d_set_enabled(lua_State* L) {
    auto* b = check_body2d(L, 1);
    bool flag = lua_toboolean(L, 2);
    b2Body* body = b->physics->getBody(b->id);
    body->SetEnabled(flag);
    return 0;
}

static int l_body2d_is_enabled(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b2Body* body = b->physics->getBody(b->id);
    lua_pushboolean(L, body->IsEnabled());
    return 1;
}

static int l_body2d_set_awake(lua_State* L) {
    auto* b = check_body2d(L, 1);
    bool flag = lua_toboolean(L, 2);
    b2Body* body = b->physics->getBody(b->id);
    body->SetAwake(flag);
    return 0;
}

static int l_body2d_is_awake(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b2Body* body = b->physics->getBody(b->id);
    lua_pushboolean(L, body->IsAwake());
    return 1;
}

static int l_body2d_get_id(lua_State* L) {
    auto* b = check_body2d(L, 1);
    lua_pushinteger(L, b->id);
    return 1;
}

static int l_body2d_is_valid(lua_State* L) {
    auto* b = static_cast<LuaPhysics2DBody*>(luaL_checkudata(L, 1, "Physics2D.Body"));
    bool valid = b && b->valid && b->id != 0 && b->physics && b->physics->isValidBody(b->id);
    lua_pushboolean(L, valid);
    return 1;
}

static int l_body2d_destroy(lua_State* L) {
    auto* b = check_body2d(L, 1);
    bool ok = b->physics->destroyBody(b->id);
    b->valid = false;
    lua_pushboolean(L, ok);
    return 1;
}

static int l_body2d_tostring(lua_State* L) {
    auto* b = static_cast<LuaPhysics2DBody*>(luaL_checkudata(L, 1, "Physics2D.Body"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics2D.Body(ID: %u, Valid: %s)",
                  b ? b->id : 0, (b && b->valid && b->physics && b->physics->isValidBody(b->id)) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static void register_body2d_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics2D.Body");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_body2d_add_box);
    lua_setfield(L, -2, "addBox");
    lua_pushcfunction(L, l_body2d_add_circle);
    lua_setfield(L, -2, "addCircle");
    lua_pushcfunction(L, l_body2d_add_polygon);
    lua_setfield(L, -2, "addPolygon");
    lua_pushcfunction(L, l_body2d_add_edge);
    lua_setfield(L, -2, "addEdge");
    lua_pushcfunction(L, l_body2d_add_chain);
    lua_setfield(L, -2, "addChain");

    lua_pushcfunction(L, l_body2d_get_position);
    lua_setfield(L, -2, "getPosition");
    lua_pushcfunction(L, l_body2d_set_position);
    lua_setfield(L, -2, "setPosition");
    lua_pushcfunction(L, l_body2d_get_angle);
    lua_setfield(L, -2, "getAngle");
    lua_pushcfunction(L, l_body2d_set_angle);
    lua_setfield(L, -2, "setAngle");
    lua_pushcfunction(L, l_body2d_get_linear_velocity);
    lua_setfield(L, -2, "getLinearVelocity");
    lua_pushcfunction(L, l_body2d_set_linear_velocity);
    lua_setfield(L, -2, "setLinearVelocity");
    lua_pushcfunction(L, l_body2d_get_angular_velocity);
    lua_setfield(L, -2, "getAngularVelocity");
    lua_pushcfunction(L, l_body2d_set_angular_velocity);
    lua_setfield(L, -2, "setAngularVelocity");

    lua_pushcfunction(L, l_body2d_apply_force);
    lua_setfield(L, -2, "applyForce");
    lua_pushcfunction(L, l_body2d_apply_impulse);
    lua_setfield(L, -2, "applyImpulse");
    lua_pushcfunction(L, l_body2d_apply_torque);
    lua_setfield(L, -2, "applyTorque");
    lua_pushcfunction(L, l_body2d_apply_angular_impulse);
    lua_setfield(L, -2, "applyAngularImpulse");

    lua_pushcfunction(L, l_body2d_set_gravity_scale);
    lua_setfield(L, -2, "setGravityScale");
    lua_pushcfunction(L, l_body2d_get_gravity_scale);
    lua_setfield(L, -2, "getGravityScale");
    lua_pushcfunction(L, l_body2d_set_fixed_rotation);
    lua_setfield(L, -2, "setFixedRotation");
    lua_pushcfunction(L, l_body2d_is_fixed_rotation);
    lua_setfield(L, -2, "isFixedRotation");
    lua_pushcfunction(L, l_body2d_set_bullet);
    lua_setfield(L, -2, "setBullet");
    lua_pushcfunction(L, l_body2d_is_bullet);
    lua_setfield(L, -2, "isBullet");
    lua_pushcfunction(L, l_body2d_set_enabled);
    lua_setfield(L, -2, "setEnabled");
    lua_pushcfunction(L, l_body2d_is_enabled);
    lua_setfield(L, -2, "isEnabled");
    lua_pushcfunction(L, l_body2d_set_awake);
    lua_setfield(L, -2, "setAwake");
    lua_pushcfunction(L, l_body2d_is_awake);
    lua_setfield(L, -2, "isAwake");

    lua_pushcfunction(L, l_body2d_get_id);
    lua_setfield(L, -2, "getId");
    lua_pushcfunction(L, l_body2d_is_valid);
    lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_body2d_destroy);
    lua_setfield(L, -2, "destroy");

    lua_pushcfunction(L, l_body2d_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pop(L, 1);
}

// ---------------- Joint Methods ----------------

static int l_joint2d_destroy(lua_State* L) {
    auto* j = check_joint2d(L, 1);
    bool ok = j->physics->destroyJoint(j->id);
    j->valid = false;
    lua_pushboolean(L, ok);
    return 1;
}

static int l_joint2d_is_valid(lua_State* L) {
    auto* j = static_cast<LuaPhysics2DJoint*>(luaL_checkudata(L, 1, "Physics2D.Joint"));
    lua_pushboolean(L, j && j->valid && j->id != 0);
    return 1;
}

static int l_joint2d_get_id(lua_State* L) {
    auto* j = check_joint2d(L, 1);
    lua_pushinteger(L, j->id);
    return 1;
}

static int l_joint2d_tostring(lua_State* L) {
    auto* j = static_cast<LuaPhysics2DJoint*>(luaL_checkudata(L, 1, "Physics2D.Joint"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics2D.Joint(ID: %u, Valid: %s)", j ? j->id : 0, (j && j->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static void register_joint2d_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics2D.Joint");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_joint2d_destroy);
    lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_joint2d_is_valid);
    lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_joint2d_get_id);
    lua_setfield(L, -2, "getId");
    lua_pushcfunction(L, l_joint2d_tostring);
    lua_setfield(L, -2, "__tostring");

    lua_pop(L, 1);
}

// ---------------- Module Functions (crayon.physics2d) ----------------

static int l_physics2d_create_body(lua_State* L) {
    auto& phys = Engine::get().get_physics2d();

    std::string typeStr = luaL_checkstring(L, 1);
    b2BodyType type = b2_dynamicBody;
    if (typeStr == "static") type = b2_staticBody;
    else if (typeStr == "kinematic") type = b2_kinematicBody;
    else if (typeStr == "dynamic") type = b2_dynamicBody;

    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));

    Body2DOptions opts;
    if (lua_istable(L, 4)) {
        lua_getfield(L, 4, "fixedRotation");
        if (!lua_isnil(L, -1)) opts.fixedRotation = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 4, "linearDamping");
        if (!lua_isnil(L, -1)) opts.linearDamping = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 4, "angularDamping");
        if (!lua_isnil(L, -1)) opts.angularDamping = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 4, "gravityScale");
        if (!lua_isnil(L, -1)) opts.gravityScale = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 4, "bullet");
        if (!lua_isnil(L, -1)) opts.bullet = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 4, "allowSleep");
        if (!lua_isnil(L, -1)) opts.allowSleep = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    uint32_t id = phys.createBody(type, x, y, opts);
    if (id == 0) {
        lua_pushnil(L);
        return 1;
    }

    push_body2d_userdata(L, id, &phys);
    return 1;
}

static int l_physics2d_destroy_body(lua_State* L) {
    auto& phys = Engine::get().get_physics2d();
    if (lua_isuserdata(L, 1)) {
        auto* b = check_body2d(L, 1);
        bool ok = phys.destroyBody(b->id);
        b->valid = false;
        lua_pushboolean(L, ok);
        return 1;
    } else {
        uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
        bool ok = phys.destroyBody(id);
        lua_pushboolean(L, ok);
        return 1;
    }
}

static int l_physics2d_set_gravity(lua_State* L) {
    float gx = static_cast<float>(luaL_checknumber(L, 1));
    float gy = static_cast<float>(luaL_checknumber(L, 2));
    Engine::get().get_physics2d().setGravity(gx, gy);
    return 0;
}

static int l_physics2d_get_gravity(lua_State* L) {
    glm::vec2 g = Engine::get().get_physics2d().getGravity();
    lua_pushnumber(L, g.x);
    lua_pushnumber(L, g.y);
    return 2;
}

static int l_physics2d_set_meter_scale(lua_State* L) {
    float s = static_cast<float>(luaL_checknumber(L, 1));
    Engine::get().get_physics2d().setMeterScale(s);
    return 0;
}

static int l_physics2d_get_meter_scale(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_physics2d().getMeterScale());
    return 1;
}

static int l_physics2d_raycast(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));

    auto hit = Engine::get().get_physics2d().raycast(x1, y1, x2, y2);
    lua_pushboolean(L, hit.hit);
    if (hit.hit) {
        lua_pushnumber(L, hit.point.x);
        lua_pushnumber(L, hit.point.y);
        lua_pushnumber(L, hit.normal.x);
        lua_pushnumber(L, hit.normal.y);
        lua_pushnumber(L, hit.fraction);
        lua_pushinteger(L, hit.bodyId);
        return 7;
    }
    return 1;
}

static int l_physics2d_draw_debug(lua_State* /*L*/) {
    Engine::get().get_physics2d().drawDebug(Engine::get().get_batch2d());
    return 0;
}

static int l_physics2d_create_distance_joint(lua_State* L) {
    auto& phys = Engine::get().get_physics2d();
    uint32_t bA = check_body2d(L, 1)->id;
    uint32_t bB = check_body2d(L, 2)->id;
    float ax1 = static_cast<float>(luaL_checknumber(L, 3));
    float ay1 = static_cast<float>(luaL_checknumber(L, 4));
    float ax2 = static_cast<float>(luaL_checknumber(L, 5));
    float ay2 = static_cast<float>(luaL_checknumber(L, 6));
    float length = static_cast<float>(luaL_optnumber(L, 7, -1.0));
    float freq = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float damping = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    bool collide = lua_toboolean(L, 10);

    uint32_t jId = phys.createDistanceJoint(bA, bB, ax1, ay1, ax2, ay2, length, freq, damping, collide);
    if (jId == 0) {
        lua_pushnil(L);
        return 1;
    }
    push_joint2d_userdata(L, jId, &phys);
    return 1;
}

static int l_physics2d_create_revolute_joint(lua_State* L) {
    auto& phys = Engine::get().get_physics2d();
    uint32_t bA = check_body2d(L, 1)->id;
    uint32_t bB = check_body2d(L, 2)->id;
    float ax = static_cast<float>(luaL_checknumber(L, 3));
    float ay = static_cast<float>(luaL_checknumber(L, 4));
    bool enableLimit = lua_toboolean(L, 5);
    float lower = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float upper = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    bool enableMotor = lua_toboolean(L, 8);
    float speed = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    float maxTorque = static_cast<float>(luaL_optnumber(L, 10, 0.0));
    bool collide = lua_toboolean(L, 11);

    uint32_t jId = phys.createRevoluteJoint(bA, bB, ax, ay, enableLimit, lower, upper, enableMotor, speed, maxTorque, collide);
    if (jId == 0) {
        lua_pushnil(L);
        return 1;
    }
    push_joint2d_userdata(L, jId, &phys);
    return 1;
}

static int l_physics2d_create_prismatic_joint(lua_State* L) {
    auto& phys = Engine::get().get_physics2d();
    uint32_t bA = check_body2d(L, 1)->id;
    uint32_t bB = check_body2d(L, 2)->id;
    float ax = static_cast<float>(luaL_checknumber(L, 3));
    float ay = static_cast<float>(luaL_checknumber(L, 4));
    float axisX = static_cast<float>(luaL_checknumber(L, 5));
    float axisY = static_cast<float>(luaL_checknumber(L, 6));
    bool enableLimit = lua_toboolean(L, 7);
    float lower = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float upper = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    bool enableMotor = lua_toboolean(L, 10);
    float speed = static_cast<float>(luaL_optnumber(L, 11, 0.0));
    float maxForce = static_cast<float>(luaL_optnumber(L, 12, 0.0));
    bool collide = lua_toboolean(L, 13);

    uint32_t jId = phys.createPrismaticJoint(bA, bB, ax, ay, axisX, axisY, enableLimit, lower, upper, enableMotor, speed, maxForce, collide);
    if (jId == 0) {
        lua_pushnil(L);
        return 1;
    }
    push_joint2d_userdata(L, jId, &phys);
    return 1;
}

static int l_physics2d_create_weld_joint(lua_State* L) {
    auto& phys = Engine::get().get_physics2d();
    uint32_t bA = check_body2d(L, 1)->id;
    uint32_t bB = check_body2d(L, 2)->id;
    float ax = static_cast<float>(luaL_checknumber(L, 3));
    float ay = static_cast<float>(luaL_checknumber(L, 4));
    float freq = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float damping = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    bool collide = lua_toboolean(L, 7);

    uint32_t jId = phys.createWeldJoint(bA, bB, ax, ay, freq, damping, collide);
    if (jId == 0) {
        lua_pushnil(L);
        return 1;
    }
    push_joint2d_userdata(L, jId, &phys);
    return 1;
}

static int l_physics2d_create_wheel_joint(lua_State* L) {
    auto& phys = Engine::get().get_physics2d();
    uint32_t bA = check_body2d(L, 1)->id;
    uint32_t bB = check_body2d(L, 2)->id;
    float ax = static_cast<float>(luaL_checknumber(L, 3));
    float ay = static_cast<float>(luaL_checknumber(L, 4));
    float axisX = static_cast<float>(luaL_checknumber(L, 5));
    float axisY = static_cast<float>(luaL_checknumber(L, 6));
    float freq = static_cast<float>(luaL_optnumber(L, 7, 2.0));
    float damping = static_cast<float>(luaL_optnumber(L, 8, 0.7));
    bool collide = lua_toboolean(L, 9);

    uint32_t jId = phys.createWheelJoint(bA, bB, ax, ay, axisX, axisY, freq, damping, collide);
    if (jId == 0) {
        lua_pushnil(L);
        return 1;
    }
    push_joint2d_userdata(L, jId, &phys);
    return 1;
}

static int l_physics2d_destroy_joint(lua_State* L) {
    auto& phys = Engine::get().get_physics2d();
    if (lua_isuserdata(L, 1)) {
        auto* j = check_joint2d(L, 1);
        bool ok = phys.destroyJoint(j->id);
        j->valid = false;
        lua_pushboolean(L, ok);
        return 1;
    } else {
        uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
        bool ok = phys.destroyJoint(id);
        lua_pushboolean(L, ok);
        return 1;
    }
}

void register_physics2d_bindings(lua_State* L) {
    register_body2d_metatable(L);
    register_joint2d_metatable(L);

    lua_getglobal(L, "crayon");
    lua_newtable(L);

    lua_pushcfunction(L, l_physics2d_create_body);
    lua_setfield(L, -2, "createBody");

    lua_pushcfunction(L, l_physics2d_destroy_body);
    lua_setfield(L, -2, "destroyBody");

    lua_pushcfunction(L, l_physics2d_set_gravity);
    lua_setfield(L, -2, "setGravity");

    lua_pushcfunction(L, l_physics2d_get_gravity);
    lua_setfield(L, -2, "getGravity");

    lua_pushcfunction(L, l_physics2d_set_meter_scale);
    lua_setfield(L, -2, "setMeterScale");

    lua_pushcfunction(L, l_physics2d_get_meter_scale);
    lua_setfield(L, -2, "getMeterScale");

    lua_pushcfunction(L, l_physics2d_raycast);
    lua_setfield(L, -2, "raycast");

    lua_pushcfunction(L, l_physics2d_draw_debug);
    lua_setfield(L, -2, "drawDebug");

    lua_pushcfunction(L, l_physics2d_create_distance_joint);
    lua_setfield(L, -2, "createDistanceJoint");

    lua_pushcfunction(L, l_physics2d_create_revolute_joint);
    lua_setfield(L, -2, "createRevoluteJoint");

    lua_pushcfunction(L, l_physics2d_create_prismatic_joint);
    lua_setfield(L, -2, "createPrismaticJoint");

    lua_pushcfunction(L, l_physics2d_create_weld_joint);
    lua_setfield(L, -2, "createWeldJoint");

    lua_pushcfunction(L, l_physics2d_create_wheel_joint);
    lua_setfield(L, -2, "createWheelJoint");

    lua_pushcfunction(L, l_physics2d_destroy_joint);
    lua_setfield(L, -2, "destroyJoint");

    lua_setfield(L, -2, "physics2D");
    lua_pop(L, 1);
}

} // namespace crayon
