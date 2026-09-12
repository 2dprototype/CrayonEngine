#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include <cstdio>
#include <cstring>

namespace crayon {

struct LuaPhysics2DBody {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float angle = 0.0f;
    float angular_velocity = 0.0f;
    float mass = 1.0f;
    bool valid = true;
};

static LuaPhysics2DBody* check_body2d(lua_State* L, int idx) {
    auto* body = static_cast<LuaPhysics2DBody*>(luaL_checkudata(L, idx, "Physics2D.Body"));
    if (!body || !body->valid) {
        luaL_error(L, "attempt to use destroyed Physics2D.Body");
        return nullptr;
    }
    return body;
}

static int l_body2d_get_position(lua_State* L) {
    auto* b = check_body2d(L, 1);
    lua_pushnumber(L, b->x);
    lua_pushnumber(L, b->y);
    return 2;
}

static int l_body2d_set_position(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b->x = static_cast<float>(luaL_checknumber(L, 2));
    b->y = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

static int l_body2d_get_velocity(lua_State* L) {
    auto* b = check_body2d(L, 1);
    lua_pushnumber(L, b->vx);
    lua_pushnumber(L, b->vy);
    return 2;
}

static int l_body2d_set_velocity(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b->vx = static_cast<float>(luaL_checknumber(L, 2));
    b->vy = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

static int l_body2d_get_angle(lua_State* L) {
    auto* b = check_body2d(L, 1);
    lua_pushnumber(L, b->angle);
    return 1;
}

static int l_body2d_set_angle(lua_State* L) {
    auto* b = check_body2d(L, 1);
    b->angle = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

static int l_body2d_apply_force(lua_State* L) {
    auto* b = check_body2d(L, 1);
    float fx = static_cast<float>(luaL_checknumber(L, 2));
    float fy = static_cast<float>(luaL_checknumber(L, 3));
    if (b->mass > 0.0f) {
        b->vx += fx / b->mass;
        b->vy += fy / b->mass;
    }
    return 0;
}

static int l_body2d_is_valid(lua_State* L) {
    auto* b = static_cast<LuaPhysics2DBody*>(luaL_checkudata(L, 1, "Physics2D.Body"));
    lua_pushboolean(L, b && b->valid);
    return 1;
}

static int l_body2d_destroy(lua_State* L) {
    auto* b = static_cast<LuaPhysics2DBody*>(luaL_checkudata(L, 1, "Physics2D.Body"));
    if (b) {
        b->valid = false;
    }
    return 0;
}

static int l_body2d_gc(lua_State* L) {
    auto* b = static_cast<LuaPhysics2DBody*>(luaL_checkudata(L, 1, "Physics2D.Body"));
    if (b) {
        b->valid = false;
    }
    return 0;
}

static int l_body2d_tostring(lua_State* L) {
    auto* b = static_cast<LuaPhysics2DBody*>(luaL_checkudata(L, 1, "Physics2D.Body"));
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Physics2D.Body(%p, valid: %s)", b, (b && b->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_physics2d_create_body(lua_State* L) {
    float x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float mass = static_cast<float>(luaL_optnumber(L, 3, 1.0));

    auto* udata = static_cast<LuaPhysics2DBody*>(lua_newuserdata(L, sizeof(LuaPhysics2DBody)));
    udata->x = x;
    udata->y = y;
    udata->vx = 0.0f;
    udata->vy = 0.0f;
    udata->angle = 0.0f;
    udata->angular_velocity = 0.0f;
    udata->mass = mass;
    udata->valid = true;

    luaL_getmetatable(L, "Physics2D.Body");
    lua_setmetatable(L, -2);
    return 1;
}

static void register_body2d_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics2D.Body");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // camelCase methods
    lua_pushcfunction(L, l_body2d_get_position);
    lua_setfield(L, -2, "getPosition");
    lua_pushcfunction(L, l_body2d_set_position);
    lua_setfield(L, -2, "setPosition");
    lua_pushcfunction(L, l_body2d_get_velocity);
    lua_setfield(L, -2, "getVelocity");
    lua_pushcfunction(L, l_body2d_set_velocity);
    lua_setfield(L, -2, "setVelocity");
    lua_pushcfunction(L, l_body2d_get_angle);
    lua_setfield(L, -2, "getAngle");
    lua_pushcfunction(L, l_body2d_set_angle);
    lua_setfield(L, -2, "setAngle");
    lua_pushcfunction(L, l_body2d_apply_force);
    lua_setfield(L, -2, "applyForce");
    lua_pushcfunction(L, l_body2d_is_valid);
    lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_body2d_destroy);
    lua_setfield(L, -2, "destroy");

    lua_pushcfunction(L, l_body2d_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_body2d_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

void register_physics2d_bindings(lua_State* L) {
    register_body2d_metatable(L);

    lua_getglobal(L, "crayon");
    lua_newtable(L);

    lua_pushcfunction(L, l_physics2d_create_body);
    lua_setfield(L, -2, "createBody");

    lua_setfield(L, -2, "physics2d");
    lua_pop(L, 1);
}

} // namespace crayon
