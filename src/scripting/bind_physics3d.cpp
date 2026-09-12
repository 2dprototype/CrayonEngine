#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include "../physics/physics_system.hpp"
#include <cstring>
#include <cstdio>

namespace crayon {

struct LuaPhysics3DBody {
    uint32_t id;
    PhysicsSystem* physics;
    bool valid;
};

struct LuaPhysics3DConstraint {
    uint32_t id;
    PhysicsSystem* physics;
    bool valid;
};

static MotionType parse_motion_type(lua_State* L, int index) {
    if (lua_isstring(L, index)) {
        const char* str = lua_tostring(L, index);
        if (std::strcmp(str, "static") == 0) return MotionType::Static;
        if (std::strcmp(str, "kinematic") == 0) return MotionType::Kinematic;
        return MotionType::Dynamic;
    }
    if (lua_isnumber(L, index)) {
        int val = static_cast<int>(lua_tointeger(L, index));
        if (val == 0) return MotionType::Static;
        if (val == 1) return MotionType::Kinematic;
        return MotionType::Dynamic;
    }
    return MotionType::Dynamic;
}

static LuaPhysics3DBody* check_body(lua_State* L, int idx) {
    auto* body = static_cast<LuaPhysics3DBody*>(luaL_checkudata(L, idx, "Physics3D.Body"));
    if (!body || !body->valid || !body->physics || !body->physics->is_body_valid(body->id)) {
        luaL_error(L, "attempt to use destroyed Physics3D.Body");
        return nullptr;
    }
    return body;
}

static LuaPhysics3DConstraint* check_constraint(lua_State* L, int idx) {
    auto* c = static_cast<LuaPhysics3DConstraint*>(luaL_checkudata(L, idx, "Physics3D.Constraint"));
    if (!c || !c->valid || !c->physics) {
        luaL_error(L, "attempt to use destroyed Physics3D.Constraint");
        return nullptr;
    }
    return c;
}

static uint32_t check_body_id(lua_State* L, int idx) {
    if (lua_isuserdata(L, idx)) {
        auto* b = static_cast<LuaPhysics3DBody*>(luaL_checkudata(L, idx, "Physics3D.Body"));
        if (!b || !b->valid || !b->physics || !b->physics->is_body_valid(b->id)) {
            luaL_error(L, "attempt to use destroyed Physics3D.Body");
            return 0;
        }
        return b->id;
    }
    return static_cast<uint32_t>(luaL_checkinteger(L, idx));
}

static void push_body_userdata(lua_State* L, uint32_t id, PhysicsSystem* physics) {
    auto* udata = static_cast<LuaPhysics3DBody*>(lua_newuserdata(L, sizeof(LuaPhysics3DBody)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics3D.Body");
    lua_setmetatable(L, -2);
}

static void push_constraint_userdata(lua_State* L, uint32_t id, PhysicsSystem* physics) {
    auto* udata = static_cast<LuaPhysics3DConstraint*>(lua_newuserdata(L, sizeof(LuaPhysics3DConstraint)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics3D.Constraint");
    lua_setmetatable(L, -2);
}

// ============================================================================
// Physics3D.Body Userdata Methods
// ============================================================================

static int l_body_get_position(lua_State* L) {
    auto* b = check_body(L, 1);
    glm::vec3 pos = b->physics->get_position(b->id);
    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    lua_pushnumber(L, pos.z);
    return 3;
}

static int l_body_set_position(lua_State* L) {
    auto* b = check_body(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    bool activate = lua_isboolean(L, 5) ? lua_toboolean(L, 5) : true;
    b->physics->set_position(b->id, glm::vec3(x, y, z), activate);
    return 0;
}

static int l_body_get_rotation(lua_State* L) {
    auto* b = check_body(L, 1);
    glm::vec3 euler = b->physics->get_euler_angles(b->id);
    lua_pushnumber(L, euler.x);
    lua_pushnumber(L, euler.y);
    lua_pushnumber(L, euler.z);
    return 3;
}

static int l_body_set_rotation(lua_State* L) {
    auto* b = check_body(L, 1);
    float rx = static_cast<float>(luaL_checknumber(L, 2));
    float ry = static_cast<float>(luaL_checknumber(L, 3));
    float rz = static_cast<float>(luaL_checknumber(L, 4));
    bool activate = lua_isboolean(L, 5) ? lua_toboolean(L, 5) : true;
    b->physics->set_euler_angles(b->id, glm::vec3(rx, ry, rz), activate);
    return 0;
}

static int l_body_get_velocity(lua_State* L) {
    auto* b = check_body(L, 1);
    glm::vec3 v = b->physics->get_linear_velocity(b->id);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    return 3;
}

static int l_body_set_velocity(lua_State* L) {
    auto* b = check_body(L, 1);
    float vx = static_cast<float>(luaL_checknumber(L, 2));
    float vy = static_cast<float>(luaL_checknumber(L, 3));
    float vz = static_cast<float>(luaL_checknumber(L, 4));
    b->physics->set_linear_velocity(b->id, glm::vec3(vx, vy, vz));
    return 0;
}

static int l_body_get_angular_velocity(lua_State* L) {
    auto* b = check_body(L, 1);
    glm::vec3 w = b->physics->get_angular_velocity(b->id);
    lua_pushnumber(L, w.x);
    lua_pushnumber(L, w.y);
    lua_pushnumber(L, w.z);
    return 3;
}

static int l_body_set_angular_velocity(lua_State* L) {
    auto* b = check_body(L, 1);
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wy = static_cast<float>(luaL_checknumber(L, 3));
    float wz = static_cast<float>(luaL_checknumber(L, 4));
    b->physics->set_angular_velocity(b->id, glm::vec3(wx, wy, wz));
    return 0;
}

static int l_body_apply_force(lua_State* L) {
    auto* b = check_body(L, 1);
    float fx = static_cast<float>(luaL_checknumber(L, 2));
    float fy = static_cast<float>(luaL_checknumber(L, 3));
    float fz = static_cast<float>(luaL_checknumber(L, 4));
    b->physics->add_force(b->id, glm::vec3(fx, fy, fz));
    return 0;
}

static int l_body_apply_impulse(lua_State* L) {
    auto* b = check_body(L, 1);
    float ix = static_cast<float>(luaL_checknumber(L, 2));
    float iy = static_cast<float>(luaL_checknumber(L, 3));
    float iz = static_cast<float>(luaL_checknumber(L, 4));
    b->physics->add_impulse(b->id, glm::vec3(ix, iy, iz));
    return 0;
}

static int l_body_apply_torque(lua_State* L) {
    auto* b = check_body(L, 1);
    float tx = static_cast<float>(luaL_checknumber(L, 2));
    float ty = static_cast<float>(luaL_checknumber(L, 3));
    float tz = static_cast<float>(luaL_checknumber(L, 4));
    b->physics->add_torque(b->id, glm::vec3(tx, ty, tz));
    return 0;
}

static int l_body_set_gravity_factor(lua_State* L) {
    auto* b = check_body(L, 1);
    float factor = static_cast<float>(luaL_checknumber(L, 2));
    b->physics->set_gravity_factor(b->id, factor);
    return 0;
}

static int l_body_set_friction(lua_State* L) {
    auto* b = check_body(L, 1);
    float friction = static_cast<float>(luaL_checknumber(L, 2));
    b->physics->set_friction(b->id, friction);
    return 0;
}

static int l_body_set_restitution(lua_State* L) {
    auto* b = check_body(L, 1);
    float restitution = static_cast<float>(luaL_checknumber(L, 2));
    b->physics->set_restitution(b->id, restitution);
    return 0;
}

static int l_body_set_motion_type(lua_State* L) {
    auto* b = check_body(L, 1);
    MotionType motion = parse_motion_type(L, 2);
    b->physics->set_motion_type(b->id, motion);
    return 0;
}

static int l_body_set_sensor(lua_State* L) {
    auto* b = check_body(L, 1);
    bool is_sensor = lua_toboolean(L, 2);
    b->physics->set_is_sensor(b->id, is_sensor);
    return 0;
}

static int l_body_is_sensor(lua_State* L) {
    auto* b = check_body(L, 1);
    lua_pushboolean(L, b->physics->is_sensor(b->id));
    return 1;
}

static int l_body_set_damping(lua_State* L) {
    auto* b = check_body(L, 1);
    float lin_d = static_cast<float>(luaL_checknumber(L, 2));
    float ang_d = static_cast<float>(luaL_optnumber(L, 3, lin_d));
    b->physics->set_damping(b->id, lin_d, ang_d);
    return 0;
}

static int l_body_is_valid(lua_State* L) {
    auto* b = static_cast<LuaPhysics3DBody*>(luaL_checkudata(L, 1, "Physics3D.Body"));
    bool valid = (b && b->valid && b->physics && b->physics->is_body_valid(b->id));
    lua_pushboolean(L, valid);
    return 1;
}

static int l_body_is_active(lua_State* L) {
    auto* b = check_body(L, 1);
    lua_pushboolean(L, b->physics->is_body_active(b->id));
    return 1;
}

static int l_body_set_active(lua_State* L) {
    auto* b = check_body(L, 1);
    bool active = lua_toboolean(L, 2);
    if (active) {
        b->physics->activate_body(b->id);
    } else {
        b->physics->deactivate_body(b->id);
    }
    return 0;
}

static int l_body_destroy(lua_State* L) {
    auto* b = static_cast<LuaPhysics3DBody*>(luaL_checkudata(L, 1, "Physics3D.Body"));
    if (!b || !b->valid) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool res = false;
    if (b->physics && b->physics->is_body_valid(b->id)) {
        res = b->physics->destroy_body(b->id);
    }
    b->valid = false;
    lua_pushboolean(L, res);
    return 1;
}

static int l_body_get_id(lua_State* L) {
    auto* b = check_body(L, 1);
    lua_pushinteger(L, b->id);
    return 1;
}

static int l_body_tostring(lua_State* L) {
    auto* b = static_cast<LuaPhysics3DBody*>(luaL_checkudata(L, 1, "Physics3D.Body"));
    char buf[80];
    bool valid = (b && b->valid && b->physics && b->physics->is_body_valid(b->id));
    std::snprintf(buf, sizeof(buf), "Physics3D.Body(ID: %u, valid: %s)", b ? b->id : 0, valid ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_body_gc(lua_State* L) {
    auto* b = static_cast<LuaPhysics3DBody*>(luaL_checkudata(L, 1, "Physics3D.Body"));
    if (b) {
        b->valid = false;
        b->physics = nullptr;
    }
    return 0;
}

// ============================================================================
// Physics3D.Constraint Userdata Methods
// ============================================================================

static int l_constraint_destroy(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DConstraint*>(luaL_checkudata(L, 1, "Physics3D.Constraint"));
    if (!c || !c->valid) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool res = false;
    if (c->physics) {
        res = c->physics->destroy_constraint(c->id);
    }
    c->valid = false;
    lua_pushboolean(L, res);
    return 1;
}

static int l_constraint_is_valid(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DConstraint*>(luaL_checkudata(L, 1, "Physics3D.Constraint"));
    lua_pushboolean(L, c && c->valid);
    return 1;
}

static int l_constraint_get_id(lua_State* L) {
    auto* c = check_constraint(L, 1);
    lua_pushinteger(L, c->id);
    return 1;
}

static int l_constraint_tostring(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DConstraint*>(luaL_checkudata(L, 1, "Physics3D.Constraint"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics3D.Constraint(ID: %u, valid: %s)", c ? c->id : 0, (c && c->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_constraint_gc(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DConstraint*>(luaL_checkudata(L, 1, "Physics3D.Constraint"));
    if (c) {
        c->valid = false;
        c->physics = nullptr;
    }
    return 0;
}

// ============================================================================
// World-Level Functions in crayon.physics3d
// ============================================================================

static int l_physics_create_box(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float hx = static_cast<float>(luaL_checknumber(L, 4));
    float hy = static_cast<float>(luaL_checknumber(L, 5));
    float hz = static_cast<float>(luaL_checknumber(L, 6));

    MotionType motion = MotionType::Dynamic;
    if (lua_gettop(L) >= 7) motion = parse_motion_type(L, 7);

    float friction = static_cast<float>(luaL_optnumber(L, 8, 0.5));
    float restitution = static_cast<float>(luaL_optnumber(L, 9, 0.2));
    float density = static_cast<float>(luaL_optnumber(L, 10, 1000.0));

    auto& ps = Engine::get().get_physics();
    uint32_t id = ps.create_box(
        glm::vec3(x, y, z),
        glm::vec3(hx, hy, hz),
        motion, friction, restitution, density
    );
    push_body_userdata(L, id, &ps);
    return 1;
}

static int l_physics_create_sphere(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));

    MotionType motion = MotionType::Dynamic;
    if (lua_gettop(L) >= 5) motion = parse_motion_type(L, 5);

    float friction = static_cast<float>(luaL_optnumber(L, 6, 0.5));
    float restitution = static_cast<float>(luaL_optnumber(L, 7, 0.5));
    float density = static_cast<float>(luaL_optnumber(L, 8, 1000.0));

    auto& ps = Engine::get().get_physics();
    uint32_t id = ps.create_sphere(
        glm::vec3(x, y, z),
        radius, motion, friction, restitution, density
    );
    push_body_userdata(L, id, &ps);
    return 1;
}

static int l_physics_create_capsule(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float half_h = static_cast<float>(luaL_checknumber(L, 4));
    float radius = static_cast<float>(luaL_checknumber(L, 5));

    MotionType motion = MotionType::Dynamic;
    if (lua_gettop(L) >= 6) motion = parse_motion_type(L, 6);

    float friction = static_cast<float>(luaL_optnumber(L, 7, 0.5));
    float restitution = static_cast<float>(luaL_optnumber(L, 8, 0.2));
    float density = static_cast<float>(luaL_optnumber(L, 9, 1000.0));

    auto& ps = Engine::get().get_physics();
    uint32_t id = ps.create_capsule(
        glm::vec3(x, y, z),
        half_h, radius, motion, friction, restitution, density
    );
    push_body_userdata(L, id, &ps);
    return 1;
}

static int l_physics_create_cylinder(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float half_h = static_cast<float>(luaL_checknumber(L, 4));
    float radius = static_cast<float>(luaL_checknumber(L, 5));

    MotionType motion = MotionType::Dynamic;
    if (lua_gettop(L) >= 6) motion = parse_motion_type(L, 6);

    float friction = static_cast<float>(luaL_optnumber(L, 7, 0.5));
    float restitution = static_cast<float>(luaL_optnumber(L, 8, 0.2));
    float density = static_cast<float>(luaL_optnumber(L, 9, 1000.0));

    auto& ps = Engine::get().get_physics();
    uint32_t id = ps.create_cylinder(
        glm::vec3(x, y, z),
        half_h, radius, motion, friction, restitution, density
    );
    push_body_userdata(L, id, &ps);
    return 1;
}

static int l_physics_create_plane(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float nx = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float ny = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float nz = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float half_extent = static_cast<float>(luaL_optnumber(L, 7, 100.0));

    auto& ps = Engine::get().get_physics();
    uint32_t id = ps.create_plane(
        glm::vec3(x, y, z),
        glm::vec3(nx, ny, nz),
        half_extent
    );
    push_body_userdata(L, id, &ps);
    return 1;
}

static int l_physics_destroy_all(lua_State* /*L*/) {
    Engine::get().get_physics().destroy_all_bodies();
    return 0;
}

static int l_physics_set_gravity(lua_State* L) {
    float gx = static_cast<float>(luaL_checknumber(L, 1));
    float gy = static_cast<float>(luaL_checknumber(L, 2));
    float gz = static_cast<float>(luaL_checknumber(L, 3));
    Engine::get().get_physics().set_gravity(glm::vec3(gx, gy, gz));
    return 0;
}

static int l_physics_get_gravity(lua_State* L) {
    glm::vec3 g = Engine::get().get_physics().get_gravity();
    lua_pushnumber(L, g.x);
    lua_pushnumber(L, g.y);
    lua_pushnumber(L, g.z);
    return 3;
}

static int l_physics_raycast(lua_State* L) {
    float ox = static_cast<float>(luaL_checknumber(L, 1));
    float oy = static_cast<float>(luaL_checknumber(L, 2));
    float oz = static_cast<float>(luaL_checknumber(L, 3));
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    float max_dist = static_cast<float>(luaL_optnumber(L, 7, 1000.0));

    RaycastHit hit;
    auto& ps = Engine::get().get_physics();
    bool has_hit = ps.raycast(
        glm::vec3(ox, oy, oz),
        glm::vec3(dx, dy, dz),
        max_dist, hit
    );

    if (has_hit) {
        lua_pushboolean(L, true);
        lua_pushnumber(L, hit.position.x);
        lua_pushnumber(L, hit.position.y);
        lua_pushnumber(L, hit.position.z);
        lua_pushnumber(L, hit.normal.x);
        lua_pushnumber(L, hit.normal.y);
        lua_pushnumber(L, hit.normal.z);
        lua_pushnumber(L, hit.distance);
        push_body_userdata(L, hit.body_id, &ps);
        return 9;
    }

    lua_pushboolean(L, false);
    return 1;
}

static int l_physics_draw_debug(lua_State* L) {
    glm::vec4 active_col(0.2f, 1.0f, 0.4f, 1.0f);
    glm::vec4 sleep_col(0.5f, 0.5f, 0.5f, 1.0f);

    if (lua_gettop(L) >= 4) {
        active_col.r = static_cast<float>(luaL_checknumber(L, 1));
        active_col.g = static_cast<float>(luaL_checknumber(L, 2));
        active_col.b = static_cast<float>(luaL_checknumber(L, 3));
        active_col.a = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    }
    if (lua_gettop(L) >= 8) {
        sleep_col.r = static_cast<float>(luaL_checknumber(L, 5));
        sleep_col.g = static_cast<float>(luaL_checknumber(L, 6));
        sleep_col.b = static_cast<float>(luaL_checknumber(L, 7));
        sleep_col.a = static_cast<float>(luaL_optnumber(L, 8, 1.0));
    }

    Engine::get().get_physics().draw_debug(Engine::get().get_mesh_renderer(), active_col, sleep_col);
    return 0;
}

static int l_physics_get_body_count(lua_State* L) {
    lua_pushinteger(L, Engine::get().get_physics().get_num_bodies());
    lua_pushinteger(L, Engine::get().get_physics().get_num_active_bodies());
    return 2;
}

static int l_physics_overlap_sphere(lua_State* L) {
    float cx = static_cast<float>(luaL_checknumber(L, 1));
    float cy = static_cast<float>(luaL_checknumber(L, 2));
    float cz = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));

    auto& ps = Engine::get().get_physics();
    std::vector<uint32_t> hits = ps.overlap_sphere(glm::vec3(cx, cy, cz), radius);
    lua_createtable(L, static_cast<int>(hits.size()), 0);
    for (size_t i = 0; i < hits.size(); ++i) {
        push_body_userdata(L, hits[i], &ps);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

static int l_physics_create_point_constraint(lua_State* L) {
    uint32_t b1 = check_body_id(L, 1);
    uint32_t b2 = check_body_id(L, 2);
    float px = static_cast<float>(luaL_checknumber(L, 3));
    float py = static_cast<float>(luaL_checknumber(L, 4));
    float pz = static_cast<float>(luaL_checknumber(L, 5));

    auto& ps = Engine::get().get_physics();
    uint32_t cid = ps.create_point_constraint(b1, b2, glm::vec3(px, py, pz));
    push_constraint_userdata(L, cid, &ps);
    return 1;
}

static int l_physics_create_hinge_constraint(lua_State* L) {
    uint32_t b1 = check_body_id(L, 1);
    uint32_t b2 = check_body_id(L, 2);
    float px = static_cast<float>(luaL_checknumber(L, 3));
    float py = static_cast<float>(luaL_checknumber(L, 4));
    float pz = static_cast<float>(luaL_checknumber(L, 5));
    float ax = static_cast<float>(luaL_checknumber(L, 6));
    float ay = static_cast<float>(luaL_checknumber(L, 7));
    float az = static_cast<float>(luaL_checknumber(L, 8));
    float min_angle = static_cast<float>(luaL_optnumber(L, 9, -3.14159265));
    float max_angle = static_cast<float>(luaL_optnumber(L, 10, 3.14159265));

    auto& ps = Engine::get().get_physics();
    uint32_t cid = ps.create_hinge_constraint(
        b1, b2, glm::vec3(px, py, pz), glm::vec3(ax, ay, az), min_angle, max_angle
    );
    push_constraint_userdata(L, cid, &ps);
    return 1;
}

static int l_physics_create_distance_constraint(lua_State* L) {
    uint32_t b1 = check_body_id(L, 1);
    uint32_t b2 = check_body_id(L, 2);
    float p1x = static_cast<float>(luaL_checknumber(L, 3));
    float p1y = static_cast<float>(luaL_checknumber(L, 4));
    float p1z = static_cast<float>(luaL_checknumber(L, 5));
    float p2x = static_cast<float>(luaL_checknumber(L, 6));
    float p2y = static_cast<float>(luaL_checknumber(L, 7));
    float p2z = static_cast<float>(luaL_checknumber(L, 8));
    float min_d = static_cast<float>(luaL_optnumber(L, 9, -1.0));
    float max_d = static_cast<float>(luaL_optnumber(L, 10, -1.0));

    auto& ps = Engine::get().get_physics();
    uint32_t cid = ps.create_distance_constraint(
        b1, b2, glm::vec3(p1x, p1y, p1z), glm::vec3(p2x, p2y, p2z), min_d, max_d
    );
    push_constraint_userdata(L, cid, &ps);
    return 1;
}

static int l_physics_create_fixed_constraint(lua_State* L) {
    uint32_t b1 = check_body_id(L, 1);
    uint32_t b2 = check_body_id(L, 2);

    auto& ps = Engine::get().get_physics();
    uint32_t cid = ps.create_fixed_constraint(b1, b2);
    push_constraint_userdata(L, cid, &ps);
    return 1;
}

static int l_physics_destroy_constraint(lua_State* L) {
    if (lua_isuserdata(L, 1)) {
        return l_constraint_destroy(L);
    }
    uint32_t cid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool ok = Engine::get().get_physics().destroy_constraint(cid);
    lua_pushboolean(L, ok);
    return 1;
}

// ============================================================================
// Registration
// ============================================================================

static void register_body_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics3D.Body");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // Primary camelCase methods
    lua_pushcfunction(L, l_body_get_position);
    lua_setfield(L, -2, "getPosition");
    lua_pushcfunction(L, l_body_set_position);
    lua_setfield(L, -2, "setPosition");
    lua_pushcfunction(L, l_body_get_rotation);
    lua_setfield(L, -2, "getRotation");
    lua_pushcfunction(L, l_body_set_rotation);
    lua_setfield(L, -2, "setRotation");
    lua_pushcfunction(L, l_body_get_velocity);
    lua_setfield(L, -2, "getVelocity");
    lua_pushcfunction(L, l_body_set_velocity);
    lua_setfield(L, -2, "setVelocity");
    lua_pushcfunction(L, l_body_get_angular_velocity);
    lua_setfield(L, -2, "getAngularVelocity");
    lua_pushcfunction(L, l_body_set_angular_velocity);
    lua_setfield(L, -2, "setAngularVelocity");
    lua_pushcfunction(L, l_body_apply_force);
    lua_setfield(L, -2, "applyForce");
    lua_pushcfunction(L, l_body_apply_impulse);
    lua_setfield(L, -2, "applyImpulse");
    lua_pushcfunction(L, l_body_apply_torque);
    lua_setfield(L, -2, "applyTorque");
    lua_pushcfunction(L, l_body_set_gravity_factor);
    lua_setfield(L, -2, "setGravityFactor");
    lua_pushcfunction(L, l_body_set_friction);
    lua_setfield(L, -2, "setFriction");
    lua_pushcfunction(L, l_body_set_restitution);
    lua_setfield(L, -2, "setRestitution");
    lua_pushcfunction(L, l_body_set_motion_type);
    lua_setfield(L, -2, "setMotionType");
    lua_pushcfunction(L, l_body_set_sensor);
    lua_setfield(L, -2, "setSensor");
    lua_pushcfunction(L, l_body_is_sensor);
    lua_setfield(L, -2, "isSensor");
    lua_pushcfunction(L, l_body_set_damping);
    lua_setfield(L, -2, "setDamping");
    lua_pushcfunction(L, l_body_is_valid);
    lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_body_is_active);
    lua_setfield(L, -2, "isActive");
    lua_pushcfunction(L, l_body_set_active);
    lua_setfield(L, -2, "setActive");
    lua_pushcfunction(L, l_body_destroy);
    lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_body_get_id);
    lua_setfield(L, -2, "getId");

    // Backward-compatibility snake_case aliases
    lua_pushcfunction(L, l_body_get_position);
    lua_setfield(L, -2, "get_position");
    lua_pushcfunction(L, l_body_set_position);
    lua_setfield(L, -2, "set_position");
    lua_pushcfunction(L, l_body_get_rotation);
    lua_setfield(L, -2, "get_rotation");
    lua_pushcfunction(L, l_body_set_rotation);
    lua_setfield(L, -2, "set_rotation");
    lua_pushcfunction(L, l_body_get_velocity);
    lua_setfield(L, -2, "get_velocity");
    lua_pushcfunction(L, l_body_set_velocity);
    lua_setfield(L, -2, "set_velocity");
    lua_pushcfunction(L, l_body_get_angular_velocity);
    lua_setfield(L, -2, "get_angular_velocity");
    lua_pushcfunction(L, l_body_set_angular_velocity);
    lua_setfield(L, -2, "set_angular_velocity");
    lua_pushcfunction(L, l_body_apply_force);
    lua_setfield(L, -2, "apply_force");
    lua_pushcfunction(L, l_body_apply_impulse);
    lua_setfield(L, -2, "apply_impulse");
    lua_pushcfunction(L, l_body_apply_torque);
    lua_setfield(L, -2, "apply_torque");
    lua_pushcfunction(L, l_body_set_gravity_factor);
    lua_setfield(L, -2, "set_gravity_factor");
    lua_pushcfunction(L, l_body_set_friction);
    lua_setfield(L, -2, "set_friction");
    lua_pushcfunction(L, l_body_set_restitution);
    lua_setfield(L, -2, "set_restitution");
    lua_pushcfunction(L, l_body_set_motion_type);
    lua_setfield(L, -2, "set_motion_type");
    lua_pushcfunction(L, l_body_set_sensor);
    lua_setfield(L, -2, "set_sensor");
    lua_pushcfunction(L, l_body_is_sensor);
    lua_setfield(L, -2, "is_sensor");
    lua_pushcfunction(L, l_body_set_damping);
    lua_setfield(L, -2, "set_damping");
    lua_pushcfunction(L, l_body_is_valid);
    lua_setfield(L, -2, "is_valid");
    lua_pushcfunction(L, l_body_is_active);
    lua_setfield(L, -2, "is_active");
    lua_pushcfunction(L, l_body_set_active);
    lua_setfield(L, -2, "set_active");
    lua_pushcfunction(L, l_body_get_id);
    lua_setfield(L, -2, "get_id");

    // Metamethods
    lua_pushcfunction(L, l_body_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_body_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

static void register_constraint_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics3D.Constraint");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    // Primary camelCase methods
    lua_pushcfunction(L, l_constraint_destroy);
    lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_constraint_is_valid);
    lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_constraint_get_id);
    lua_setfield(L, -2, "getId");

    // Backward-compatibility snake_case aliases
    lua_pushcfunction(L, l_constraint_is_valid);
    lua_setfield(L, -2, "is_valid");
    lua_pushcfunction(L, l_constraint_get_id);
    lua_setfield(L, -2, "get_id");

    // Metamethods
    lua_pushcfunction(L, l_constraint_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_constraint_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

void register_physics3d_bindings(lua_State* L) {
    register_body_metatable(L);
    register_constraint_metatable(L);

    lua_getglobal(L, "crayon");
    lua_newtable(L);

    // Factories (camelCase + snake_case)
    lua_pushcfunction(L, l_physics_create_box);
    lua_setfield(L, -2, "createBox");
    lua_pushcfunction(L, l_physics_create_box);
    lua_setfield(L, -2, "create_box");

    lua_pushcfunction(L, l_physics_create_sphere);
    lua_setfield(L, -2, "createSphere");
    lua_pushcfunction(L, l_physics_create_sphere);
    lua_setfield(L, -2, "create_sphere");

    lua_pushcfunction(L, l_physics_create_capsule);
    lua_setfield(L, -2, "createCapsule");
    lua_pushcfunction(L, l_physics_create_capsule);
    lua_setfield(L, -2, "create_capsule");

    lua_pushcfunction(L, l_physics_create_cylinder);
    lua_setfield(L, -2, "createCylinder");
    lua_pushcfunction(L, l_physics_create_cylinder);
    lua_setfield(L, -2, "create_cylinder");

    lua_pushcfunction(L, l_physics_create_plane);
    lua_setfield(L, -2, "createPlane");
    lua_pushcfunction(L, l_physics_create_plane);
    lua_setfield(L, -2, "create_plane");

    // World operations (camelCase + snake_case)
    lua_pushcfunction(L, l_physics_destroy_all);
    lua_setfield(L, -2, "destroyAll");
    lua_pushcfunction(L, l_physics_destroy_all);
    lua_setfield(L, -2, "destroy_all");

    lua_pushcfunction(L, l_physics_set_gravity);
    lua_setfield(L, -2, "setGravity");
    lua_pushcfunction(L, l_physics_set_gravity);
    lua_setfield(L, -2, "set_gravity");

    lua_pushcfunction(L, l_physics_get_gravity);
    lua_setfield(L, -2, "getGravity");
    lua_pushcfunction(L, l_physics_get_gravity);
    lua_setfield(L, -2, "get_gravity");

    lua_pushcfunction(L, l_physics_raycast);
    lua_setfield(L, -2, "raycast");

    lua_pushcfunction(L, l_physics_draw_debug);
    lua_setfield(L, -2, "drawDebug");
    lua_pushcfunction(L, l_physics_draw_debug);
    lua_setfield(L, -2, "draw_debug");

    lua_pushcfunction(L, l_physics_get_body_count);
    lua_setfield(L, -2, "getBodyCount");
    lua_pushcfunction(L, l_physics_get_body_count);
    lua_setfield(L, -2, "get_body_count");

    lua_pushcfunction(L, l_physics_overlap_sphere);
    lua_setfield(L, -2, "overlapSphere");
    lua_pushcfunction(L, l_physics_overlap_sphere);
    lua_setfield(L, -2, "overlap_sphere");

    // Constraints (camelCase + snake_case)
    lua_pushcfunction(L, l_physics_create_point_constraint);
    lua_setfield(L, -2, "createPointConstraint");
    lua_pushcfunction(L, l_physics_create_point_constraint);
    lua_setfield(L, -2, "create_point_constraint");

    lua_pushcfunction(L, l_physics_create_hinge_constraint);
    lua_setfield(L, -2, "createHingeConstraint");
    lua_pushcfunction(L, l_physics_create_hinge_constraint);
    lua_setfield(L, -2, "create_hinge_constraint");

    lua_pushcfunction(L, l_physics_create_distance_constraint);
    lua_setfield(L, -2, "createDistanceConstraint");
    lua_pushcfunction(L, l_physics_create_distance_constraint);
    lua_setfield(L, -2, "create_distance_constraint");

    lua_pushcfunction(L, l_physics_create_fixed_constraint);
    lua_setfield(L, -2, "createFixedConstraint");
    lua_pushcfunction(L, l_physics_create_fixed_constraint);
    lua_setfield(L, -2, "create_fixed_constraint");

    lua_pushcfunction(L, l_physics_destroy_constraint);
    lua_setfield(L, -2, "destroyConstraint");
    lua_pushcfunction(L, l_physics_destroy_constraint);
    lua_setfield(L, -2, "destroy_constraint");

    lua_setfield(L, -2, "physics3d");
    lua_pop(L, 1);
}

} // namespace crayon
