#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include "../physics/physics_system.hpp"
#include <cstring>

namespace crayon {

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

    uint32_t id = Engine::get().get_physics().create_box(
        glm::vec3(x, y, z),
        glm::vec3(hx, hy, hz),
        motion, friction, restitution, density
    );
    lua_pushinteger(L, id);
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

    uint32_t id = Engine::get().get_physics().create_sphere(
        glm::vec3(x, y, z),
        radius, motion, friction, restitution, density
    );
    lua_pushinteger(L, id);
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

    uint32_t id = Engine::get().get_physics().create_capsule(
        glm::vec3(x, y, z),
        half_h, radius, motion, friction, restitution, density
    );
    lua_pushinteger(L, id);
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

    uint32_t id = Engine::get().get_physics().create_cylinder(
        glm::vec3(x, y, z),
        half_h, radius, motion, friction, restitution, density
    );
    lua_pushinteger(L, id);
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

    uint32_t id = Engine::get().get_physics().create_plane(
        glm::vec3(x, y, z),
        glm::vec3(nx, ny, nz),
        half_extent
    );
    lua_pushinteger(L, id);
    return 1;
}

static int l_physics_destroy_body(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool res = Engine::get().get_physics().destroy_body(id);
    lua_pushboolean(L, res);
    return 1;
}

static int l_physics_destroy_all(lua_State* /*L*/) {
    Engine::get().get_physics().destroy_all_bodies();
    return 0;
}

static int l_physics_is_valid(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, Engine::get().get_physics().is_body_valid(id));
    return 1;
}

static int l_physics_is_active(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, Engine::get().get_physics().is_body_active(id));
    return 1;
}

static int l_physics_set_active(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool active = lua_toboolean(L, 2);
    if (active) {
        Engine::get().get_physics().activate_body(id);
    } else {
        Engine::get().get_physics().deactivate_body(id);
    }
    return 0;
}

static int l_physics_get_position(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    glm::vec3 pos = Engine::get().get_physics().get_position(id);
    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    lua_pushnumber(L, pos.z);
    return 3;
}

static int l_physics_set_position(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    bool activate = lua_isboolean(L, 5) ? lua_toboolean(L, 5) : true;
    Engine::get().get_physics().set_position(id, glm::vec3(x, y, z), activate);
    return 0;
}

static int l_physics_get_rotation(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    glm::vec3 euler = Engine::get().get_physics().get_euler_angles(id);
    lua_pushnumber(L, euler.x);
    lua_pushnumber(L, euler.y);
    lua_pushnumber(L, euler.z);
    return 3;
}

static int l_physics_set_rotation(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float rx = static_cast<float>(luaL_checknumber(L, 2));
    float ry = static_cast<float>(luaL_checknumber(L, 3));
    float rz = static_cast<float>(luaL_checknumber(L, 4));
    bool activate = lua_isboolean(L, 5) ? lua_toboolean(L, 5) : true;
    Engine::get().get_physics().set_euler_angles(id, glm::vec3(rx, ry, rz), activate);
    return 0;
}

static int l_physics_get_velocity(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    glm::vec3 v = Engine::get().get_physics().get_linear_velocity(id);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    return 3;
}

static int l_physics_set_velocity(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float vx = static_cast<float>(luaL_checknumber(L, 2));
    float vy = static_cast<float>(luaL_checknumber(L, 3));
    float vz = static_cast<float>(luaL_checknumber(L, 4));
    Engine::get().get_physics().set_linear_velocity(id, glm::vec3(vx, vy, vz));
    return 0;
}

static int l_physics_get_angular_velocity(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    glm::vec3 w = Engine::get().get_physics().get_angular_velocity(id);
    lua_pushnumber(L, w.x);
    lua_pushnumber(L, w.y);
    lua_pushnumber(L, w.z);
    return 3;
}

static int l_physics_set_angular_velocity(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wy = static_cast<float>(luaL_checknumber(L, 3));
    float wz = static_cast<float>(luaL_checknumber(L, 4));
    Engine::get().get_physics().set_angular_velocity(id, glm::vec3(wx, wy, wz));
    return 0;
}

static int l_physics_apply_force(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float fx = static_cast<float>(luaL_checknumber(L, 2));
    float fy = static_cast<float>(luaL_checknumber(L, 3));
    float fz = static_cast<float>(luaL_checknumber(L, 4));
    Engine::get().get_physics().add_force(id, glm::vec3(fx, fy, fz));
    return 0;
}

static int l_physics_apply_impulse(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float ix = static_cast<float>(luaL_checknumber(L, 2));
    float iy = static_cast<float>(luaL_checknumber(L, 3));
    float iz = static_cast<float>(luaL_checknumber(L, 4));
    Engine::get().get_physics().add_impulse(id, glm::vec3(ix, iy, iz));
    return 0;
}

static int l_physics_apply_torque(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float tx = static_cast<float>(luaL_checknumber(L, 2));
    float ty = static_cast<float>(luaL_checknumber(L, 3));
    float tz = static_cast<float>(luaL_checknumber(L, 4));
    Engine::get().get_physics().add_torque(id, glm::vec3(tx, ty, tz));
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

static int l_physics_set_gravity_factor(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float factor = static_cast<float>(luaL_checknumber(L, 2));
    Engine::get().get_physics().set_gravity_factor(id, factor);
    return 0;
}

static int l_physics_set_friction(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float friction = static_cast<float>(luaL_checknumber(L, 2));
    Engine::get().get_physics().set_friction(id, friction);
    return 0;
}

static int l_physics_set_restitution(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float restitution = static_cast<float>(luaL_checknumber(L, 2));
    Engine::get().get_physics().set_restitution(id, restitution);
    return 0;
}

static int l_physics_set_motion_type(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    MotionType motion = parse_motion_type(L, 2);
    Engine::get().get_physics().set_motion_type(id, motion);
    return 0;
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
    bool has_hit = Engine::get().get_physics().raycast(
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
        lua_pushinteger(L, hit.body_id);
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

static int l_physics_create_point_constraint(lua_State* L) {
    uint32_t b1 = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t b2 = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    float px = static_cast<float>(luaL_checknumber(L, 3));
    float py = static_cast<float>(luaL_checknumber(L, 4));
    float pz = static_cast<float>(luaL_checknumber(L, 5));

    uint32_t cid = Engine::get().get_physics().create_point_constraint(b1, b2, glm::vec3(px, py, pz));
    lua_pushinteger(L, cid);
    return 1;
}

static int l_physics_create_hinge_constraint(lua_State* L) {
    uint32_t b1 = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t b2 = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    float px = static_cast<float>(luaL_checknumber(L, 3));
    float py = static_cast<float>(luaL_checknumber(L, 4));
    float pz = static_cast<float>(luaL_checknumber(L, 5));
    float ax = static_cast<float>(luaL_checknumber(L, 6));
    float ay = static_cast<float>(luaL_checknumber(L, 7));
    float az = static_cast<float>(luaL_checknumber(L, 8));
    float min_angle = static_cast<float>(luaL_optnumber(L, 9, -3.14159265));
    float max_angle = static_cast<float>(luaL_optnumber(L, 10, 3.14159265));

    uint32_t cid = Engine::get().get_physics().create_hinge_constraint(
        b1, b2, glm::vec3(px, py, pz), glm::vec3(ax, ay, az), min_angle, max_angle
    );
    lua_pushinteger(L, cid);
    return 1;
}

static int l_physics_create_distance_constraint(lua_State* L) {
    uint32_t b1 = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t b2 = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    float p1x = static_cast<float>(luaL_checknumber(L, 3));
    float p1y = static_cast<float>(luaL_checknumber(L, 4));
    float p1z = static_cast<float>(luaL_checknumber(L, 5));
    float p2x = static_cast<float>(luaL_checknumber(L, 6));
    float p2y = static_cast<float>(luaL_checknumber(L, 7));
    float p2z = static_cast<float>(luaL_checknumber(L, 8));
    float min_d = static_cast<float>(luaL_optnumber(L, 9, -1.0));
    float max_d = static_cast<float>(luaL_optnumber(L, 10, -1.0));

    uint32_t cid = Engine::get().get_physics().create_distance_constraint(
        b1, b2, glm::vec3(p1x, p1y, p1z), glm::vec3(p2x, p2y, p2z), min_d, max_d
    );
    lua_pushinteger(L, cid);
    return 1;
}

static int l_physics_create_fixed_constraint(lua_State* L) {
    uint32_t b1 = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t b2 = static_cast<uint32_t>(luaL_checkinteger(L, 2));

    uint32_t cid = Engine::get().get_physics().create_fixed_constraint(b1, b2);
    lua_pushinteger(L, cid);
    return 1;
}

static int l_physics_destroy_constraint(lua_State* L) {
    uint32_t cid = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool ok = Engine::get().get_physics().destroy_constraint(cid);
    lua_pushboolean(L, ok);
    return 1;
}

static int l_physics_set_sensor(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    bool is_sensor = lua_toboolean(L, 2);
    Engine::get().get_physics().set_is_sensor(id, is_sensor);
    return 0;
}

static int l_physics_is_sensor(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, Engine::get().get_physics().is_sensor(id));
    return 1;
}

static int l_physics_set_damping(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float lin_d = static_cast<float>(luaL_checknumber(L, 2));
    float ang_d = static_cast<float>(luaL_optnumber(L, 3, lin_d));
    Engine::get().get_physics().set_damping(id, lin_d, ang_d);
    return 0;
}

static int l_physics_overlap_sphere(lua_State* L) {
    float cx = static_cast<float>(luaL_checknumber(L, 1));
    float cy = static_cast<float>(luaL_checknumber(L, 2));
    float cz = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));

    std::vector<uint32_t> hits = Engine::get().get_physics().overlap_sphere(glm::vec3(cx, cy, cz), radius);
    lua_createtable(L, static_cast<int>(hits.size()), 0);
    for (size_t i = 0; i < hits.size(); ++i) {
        lua_pushinteger(L, hits[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
    return 1;
}

void register_physics_bindings(lua_State* L) {
    lua_getglobal(L, "crayon");
    lua_newtable(L);

    lua_pushcfunction(L, l_physics_create_box);
    lua_setfield(L, -2, "create_box");

    lua_pushcfunction(L, l_physics_create_sphere);
    lua_setfield(L, -2, "create_sphere");

    lua_pushcfunction(L, l_physics_create_capsule);
    lua_setfield(L, -2, "create_capsule");

    lua_pushcfunction(L, l_physics_create_cylinder);
    lua_setfield(L, -2, "create_cylinder");

    lua_pushcfunction(L, l_physics_create_plane);
    lua_setfield(L, -2, "create_plane");

    lua_pushcfunction(L, l_physics_destroy_body);
    lua_setfield(L, -2, "destroy_body");

    lua_pushcfunction(L, l_physics_destroy_all);
    lua_setfield(L, -2, "destroy_all");

    lua_pushcfunction(L, l_physics_is_valid);
    lua_setfield(L, -2, "is_valid");

    lua_pushcfunction(L, l_physics_is_active);
    lua_setfield(L, -2, "is_active");

    lua_pushcfunction(L, l_physics_set_active);
    lua_setfield(L, -2, "set_active");

    lua_pushcfunction(L, l_physics_get_position);
    lua_setfield(L, -2, "get_position");

    lua_pushcfunction(L, l_physics_set_position);
    lua_setfield(L, -2, "set_position");

    lua_pushcfunction(L, l_physics_get_rotation);
    lua_setfield(L, -2, "get_rotation");

    lua_pushcfunction(L, l_physics_set_rotation);
    lua_setfield(L, -2, "set_rotation");

    lua_pushcfunction(L, l_physics_get_velocity);
    lua_setfield(L, -2, "get_velocity");

    lua_pushcfunction(L, l_physics_set_velocity);
    lua_setfield(L, -2, "set_velocity");

    lua_pushcfunction(L, l_physics_get_angular_velocity);
    lua_setfield(L, -2, "get_angular_velocity");

    lua_pushcfunction(L, l_physics_set_angular_velocity);
    lua_setfield(L, -2, "set_angular_velocity");

    lua_pushcfunction(L, l_physics_apply_force);
    lua_setfield(L, -2, "apply_force");

    lua_pushcfunction(L, l_physics_apply_impulse);
    lua_setfield(L, -2, "apply_impulse");

    lua_pushcfunction(L, l_physics_apply_torque);
    lua_setfield(L, -2, "apply_torque");

    lua_pushcfunction(L, l_physics_set_gravity);
    lua_setfield(L, -2, "set_gravity");

    lua_pushcfunction(L, l_physics_get_gravity);
    lua_setfield(L, -2, "get_gravity");

    lua_pushcfunction(L, l_physics_set_gravity_factor);
    lua_setfield(L, -2, "set_gravity_factor");

    lua_pushcfunction(L, l_physics_set_friction);
    lua_setfield(L, -2, "set_friction");

    lua_pushcfunction(L, l_physics_set_restitution);
    lua_setfield(L, -2, "set_restitution");

    lua_pushcfunction(L, l_physics_set_motion_type);
    lua_setfield(L, -2, "set_motion_type");

    lua_pushcfunction(L, l_physics_raycast);
    lua_setfield(L, -2, "raycast");

    lua_pushcfunction(L, l_physics_draw_debug);
    lua_setfield(L, -2, "draw_debug");

    lua_pushcfunction(L, l_physics_get_body_count);
    lua_setfield(L, -2, "get_body_count");

    // Constraints & Advanced features
    lua_pushcfunction(L, l_physics_create_point_constraint);
    lua_setfield(L, -2, "create_point_constraint");

    lua_pushcfunction(L, l_physics_create_hinge_constraint);
    lua_setfield(L, -2, "create_hinge_constraint");

    lua_pushcfunction(L, l_physics_create_distance_constraint);
    lua_setfield(L, -2, "create_distance_constraint");

    lua_pushcfunction(L, l_physics_create_fixed_constraint);
    lua_setfield(L, -2, "create_fixed_constraint");

    lua_pushcfunction(L, l_physics_destroy_constraint);
    lua_setfield(L, -2, "destroy_constraint");

    lua_pushcfunction(L, l_physics_set_sensor);
    lua_setfield(L, -2, "set_sensor");

    lua_pushcfunction(L, l_physics_is_sensor);
    lua_setfield(L, -2, "is_sensor");

    lua_pushcfunction(L, l_physics_set_damping);
    lua_setfield(L, -2, "set_damping");

    lua_pushcfunction(L, l_physics_overlap_sphere);
    lua_setfield(L, -2, "overlap_sphere");

    lua_setfield(L, -2, "physics");
    lua_pop(L, 1);
}

} // namespace crayon
