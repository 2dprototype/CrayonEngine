#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include "../physics/physics_system.hpp"
#include <cstring>
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>

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

struct LuaPhysics3DCharacter {
    uint32_t id;
    PhysicsSystem* physics;
    bool valid;
};

struct LuaPhysics3DCharacterVirtual {
    uint32_t id;
    PhysicsSystem* physics;
    bool valid;
};

struct LuaPhysics3DVehicle {
    uint32_t id;
    PhysicsSystem* physics;
    bool valid;
};

struct LuaPhysics3DSkeleton {
    uint32_t id;
    PhysicsSystem* physics;
    bool valid;
};

struct LuaPhysics3DSkeletonPose {
    uint32_t id;
    PhysicsSystem* physics;
    bool valid;
};

struct LuaPhysics3DSkeletonMapper {
    uint32_t id;
    PhysicsSystem* physics;
    bool valid;
};

struct LuaPhysics3DRagdoll {
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

static LuaPhysics3DCharacter* check_character(lua_State* L, int idx) {
    auto* c = static_cast<LuaPhysics3DCharacter*>(luaL_checkudata(L, idx, "Physics3D.Character"));
    if (!c || !c->valid || !c->physics) {
        luaL_error(L, "attempt to use destroyed Physics3D.Character");
        return nullptr;
    }
    return c;
}

static void push_character_userdata(lua_State* L, uint32_t id, PhysicsSystem* physics) {
    auto* udata = static_cast<LuaPhysics3DCharacter*>(lua_newuserdata(L, sizeof(LuaPhysics3DCharacter)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics3D.Character");
    lua_setmetatable(L, -2);
}

static LuaPhysics3DCharacterVirtual* check_character_virtual(lua_State* L, int idx) {
    auto* c = static_cast<LuaPhysics3DCharacterVirtual*>(luaL_checkudata(L, idx, "Physics3D.CharacterVirtual"));
    if (!c || !c->valid || !c->physics) {
        luaL_error(L, "attempt to use destroyed Physics3D.CharacterVirtual");
        return nullptr;
    }
    return c;
}

static void push_character_virtual_userdata(lua_State* L, uint32_t id, PhysicsSystem* physics) {
    auto* udata = static_cast<LuaPhysics3DCharacterVirtual*>(lua_newuserdata(L, sizeof(LuaPhysics3DCharacterVirtual)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics3D.CharacterVirtual");
    lua_setmetatable(L, -2);
}

static LuaPhysics3DVehicle* check_vehicle(lua_State* L, int idx) {
    auto* v = static_cast<LuaPhysics3DVehicle*>(luaL_checkudata(L, idx, "Physics3D.Vehicle"));
    if (!v || !v->valid || !v->physics) {
        luaL_error(L, "attempt to use destroyed Physics3D.Vehicle");
        return nullptr;
    }
    return v;
}

static void push_vehicle_userdata(lua_State* L, uint32_t id, PhysicsSystem* physics) {
    auto* udata = static_cast<LuaPhysics3DVehicle*>(lua_newuserdata(L, sizeof(LuaPhysics3DVehicle)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics3D.Vehicle");
    lua_setmetatable(L, -2);
}

static LuaPhysics3DSkeleton* check_skeleton(lua_State* L, int idx) {
    auto* s = static_cast<LuaPhysics3DSkeleton*>(luaL_checkudata(L, idx, "Physics3D.Skeleton"));
    if (!s || !s->valid || !s->physics) {
        luaL_error(L, "attempt to use destroyed Physics3D.Skeleton");
        return nullptr;
    }
    return s;
}

static void push_skeleton_userdata(lua_State* L, uint32_t id, PhysicsSystem* physics) {
    auto* udata = static_cast<LuaPhysics3DSkeleton*>(lua_newuserdata(L, sizeof(LuaPhysics3DSkeleton)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics3D.Skeleton");
    lua_setmetatable(L, -2);
}

static LuaPhysics3DSkeletonPose* check_skeleton_pose(lua_State* L, int idx) {
    auto* p = static_cast<LuaPhysics3DSkeletonPose*>(luaL_checkudata(L, idx, "Physics3D.SkeletonPose"));
    if (!p || !p->valid || !p->physics) {
        luaL_error(L, "attempt to use destroyed Physics3D.SkeletonPose");
        return nullptr;
    }
    return p;
}

static void push_skeleton_pose_userdata(lua_State* L, uint32_t id, PhysicsSystem* physics) {
    auto* udata = static_cast<LuaPhysics3DSkeletonPose*>(lua_newuserdata(L, sizeof(LuaPhysics3DSkeletonPose)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics3D.SkeletonPose");
    lua_setmetatable(L, -2);
}

static LuaPhysics3DSkeletonMapper* check_skeleton_mapper(lua_State* L, int idx) {
    auto* m = static_cast<LuaPhysics3DSkeletonMapper*>(luaL_checkudata(L, idx, "Physics3D.SkeletonMapper"));
    if (!m || !m->valid || !m->physics) {
        luaL_error(L, "attempt to use destroyed Physics3D.SkeletonMapper");
        return nullptr;
    }
    return m;
}

static void push_skeleton_mapper_userdata(lua_State* L, uint32_t id, PhysicsSystem* physics) {
    auto* udata = static_cast<LuaPhysics3DSkeletonMapper*>(lua_newuserdata(L, sizeof(LuaPhysics3DSkeletonMapper)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics3D.SkeletonMapper");
    lua_setmetatable(L, -2);
}

static LuaPhysics3DRagdoll* check_ragdoll(lua_State* L, int idx) {
    auto* r = static_cast<LuaPhysics3DRagdoll*>(luaL_checkudata(L, idx, "Physics3D.Ragdoll"));
    if (!r || !r->valid || !r->physics) {
        luaL_error(L, "attempt to use destroyed Physics3D.Ragdoll");
        return nullptr;
    }
    return r;
}

static void push_ragdoll_userdata(lua_State* L, uint32_t id, PhysicsSystem* physics) {
    auto* udata = static_cast<LuaPhysics3DRagdoll*>(lua_newuserdata(L, sizeof(LuaPhysics3DRagdoll)));
    udata->id = id;
    udata->physics = physics;
    udata->valid = true;
    luaL_getmetatable(L, "Physics3D.Ragdoll");
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
// Physics3D.Character Userdata Methods
// ============================================================================

static int l_character_destroy(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DCharacter*>(luaL_checkudata(L, 1, "Physics3D.Character"));
    if (!c || !c->valid) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool res = false;
    if (c->physics) {
        res = c->physics->destroy_character(c->id);
    }
    c->valid = false;
    lua_pushboolean(L, res);
    return 1;
}

static int l_character_is_valid(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DCharacter*>(luaL_checkudata(L, 1, "Physics3D.Character"));
    lua_pushboolean(L, c && c->valid);
    return 1;
}

static int l_character_get_id(lua_State* L) {
    auto* c = check_character(L, 1);
    lua_pushinteger(L, c->id);
    return 1;
}

static int l_character_get_body_id(lua_State* L) {
    auto* c = check_character(L, 1);
    lua_pushinteger(L, c->physics->character_get_body_id(c->id));
    return 1;
}

static int l_character_get_position(lua_State* L) {
    auto* c = check_character(L, 1);
    glm::vec3 pos = c->physics->character_get_position(c->id);
    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    lua_pushnumber(L, pos.z);
    return 3;
}

static int l_character_set_position(lua_State* L) {
    auto* c = check_character(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    c->physics->character_set_position(c->id, glm::vec3(x, y, z));
    return 0;
}

static int l_character_get_rotation(lua_State* L) {
    auto* c = check_character(L, 1);
    glm::quat rot = c->physics->character_get_rotation(c->id);
    lua_pushnumber(L, rot.x);
    lua_pushnumber(L, rot.y);
    lua_pushnumber(L, rot.z);
    lua_pushnumber(L, rot.w);
    return 4;
}

static int l_character_set_rotation(lua_State* L) {
    auto* c = check_character(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float w = static_cast<float>(luaL_checknumber(L, 5));
    c->physics->character_set_rotation(c->id, glm::quat(w, x, y, z));
    return 0;
}

static int l_character_get_linear_velocity(lua_State* L) {
    auto* c = check_character(L, 1);
    glm::vec3 v = c->physics->character_get_linear_velocity(c->id);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    return 3;
}

static int l_character_set_linear_velocity(lua_State* L) {
    auto* c = check_character(L, 1);
    float vx = static_cast<float>(luaL_checknumber(L, 2));
    float vy = static_cast<float>(luaL_checknumber(L, 3));
    float vz = static_cast<float>(luaL_checknumber(L, 4));
    c->physics->character_set_linear_velocity(c->id, glm::vec3(vx, vy, vz));
    return 0;
}

static int l_character_is_supported(lua_State* L) {
    auto* c = check_character(L, 1);
    lua_pushboolean(L, c->physics->character_is_supported(c->id));
    return 1;
}

static int l_character_get_ground_state(lua_State* L) {
    auto* c = check_character(L, 1);
    auto state = c->physics->character_get_ground_state(c->id);
    switch (state) {
        case PhysicsSystem::GroundState::OnGround: lua_pushstring(L, "on_ground"); break;
        case PhysicsSystem::GroundState::OnSteepGround: lua_pushstring(L, "on_steep_ground"); break;
        case PhysicsSystem::GroundState::NotSupported: lua_pushstring(L, "not_supported"); break;
        case PhysicsSystem::GroundState::InAir: default: lua_pushstring(L, "in_air"); break;
    }
    return 1;
}

static int l_character_get_ground_normal(lua_State* L) {
    auto* c = check_character(L, 1);
    glm::vec3 n = c->physics->character_get_ground_normal(c->id);
    lua_pushnumber(L, n.x);
    lua_pushnumber(L, n.y);
    lua_pushnumber(L, n.z);
    return 3;
}

static int l_character_get_ground_velocity(lua_State* L) {
    auto* c = check_character(L, 1);
    glm::vec3 v = c->physics->character_get_ground_velocity(c->id);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    return 3;
}

static int l_character_get_ground_position(lua_State* L) {
    auto* c = check_character(L, 1);
    glm::vec3 p = c->physics->character_get_ground_position(c->id);
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    lua_pushnumber(L, p.z);
    return 3;
}

static int l_character_tostring(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DCharacter*>(luaL_checkudata(L, 1, "Physics3D.Character"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics3D.Character(ID: %u, valid: %s)", c ? c->id : 0, (c && c->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_character_gc(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DCharacter*>(luaL_checkudata(L, 1, "Physics3D.Character"));
    if (c) {
        c->valid = false;
        c->physics = nullptr;
    }
    return 0;
}

// ============================================================================
// Physics3D.CharacterVirtual Userdata Methods
// ============================================================================

static int l_character_virtual_destroy(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DCharacterVirtual*>(luaL_checkudata(L, 1, "Physics3D.CharacterVirtual"));
    if (!c || !c->valid) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool res = false;
    if (c->physics) {
        res = c->physics->destroy_character_virtual(c->id);
    }
    c->valid = false;
    lua_pushboolean(L, res);
    return 1;
}

static int l_character_virtual_is_valid(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DCharacterVirtual*>(luaL_checkudata(L, 1, "Physics3D.CharacterVirtual"));
    lua_pushboolean(L, c && c->valid);
    return 1;
}

static int l_character_virtual_get_id(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    lua_pushinteger(L, c->id);
    return 1;
}

static int l_character_virtual_update(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    float dt = static_cast<float>(luaL_checknumber(L, 2));
    c->physics->character_virtual_update(c->id, dt);
    return 0;
}

static int l_character_virtual_get_position(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    glm::vec3 pos = c->physics->character_virtual_get_position(c->id);
    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    lua_pushnumber(L, pos.z);
    return 3;
}

static int l_character_virtual_set_position(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    c->physics->character_virtual_set_position(c->id, glm::vec3(x, y, z));
    return 0;
}

static int l_character_virtual_get_rotation(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    glm::quat rot = c->physics->character_virtual_get_rotation(c->id);
    lua_pushnumber(L, rot.x);
    lua_pushnumber(L, rot.y);
    lua_pushnumber(L, rot.z);
    lua_pushnumber(L, rot.w);
    return 4;
}

static int l_character_virtual_set_rotation(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float w = static_cast<float>(luaL_checknumber(L, 5));
    c->physics->character_virtual_set_rotation(c->id, glm::quat(w, x, y, z));
    return 0;
}

static int l_character_virtual_get_linear_velocity(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    glm::vec3 v = c->physics->character_virtual_get_linear_velocity(c->id);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    return 3;
}

static int l_character_virtual_set_linear_velocity(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    float vx = static_cast<float>(luaL_checknumber(L, 2));
    float vy = static_cast<float>(luaL_checknumber(L, 3));
    float vz = static_cast<float>(luaL_checknumber(L, 4));
    c->physics->character_virtual_set_linear_velocity(c->id, glm::vec3(vx, vy, vz));
    return 0;
}

static int l_character_virtual_is_supported(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    lua_pushboolean(L, c->physics->character_virtual_is_supported(c->id));
    return 1;
}

static int l_character_virtual_get_ground_state(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    auto state = c->physics->character_virtual_get_ground_state(c->id);
    switch (state) {
        case PhysicsSystem::GroundState::OnGround: lua_pushstring(L, "on_ground"); break;
        case PhysicsSystem::GroundState::OnSteepGround: lua_pushstring(L, "on_steep_ground"); break;
        case PhysicsSystem::GroundState::NotSupported: lua_pushstring(L, "not_supported"); break;
        case PhysicsSystem::GroundState::InAir: default: lua_pushstring(L, "in_air"); break;
    }
    return 1;
}

static int l_character_virtual_get_ground_normal(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    glm::vec3 n = c->physics->character_virtual_get_ground_normal(c->id);
    lua_pushnumber(L, n.x);
    lua_pushnumber(L, n.y);
    lua_pushnumber(L, n.z);
    return 3;
}

static int l_character_virtual_get_ground_velocity(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    glm::vec3 v = c->physics->character_virtual_get_ground_velocity(c->id);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    return 3;
}

static int l_character_virtual_get_ground_position(lua_State* L) {
    auto* c = check_character_virtual(L, 1);
    glm::vec3 p = c->physics->character_virtual_get_ground_position(c->id);
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    lua_pushnumber(L, p.z);
    return 3;
}

static int l_character_virtual_tostring(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DCharacterVirtual*>(luaL_checkudata(L, 1, "Physics3D.CharacterVirtual"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics3D.CharacterVirtual(ID: %u, valid: %s)", c ? c->id : 0, (c && c->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_character_virtual_gc(lua_State* L) {
    auto* c = static_cast<LuaPhysics3DCharacterVirtual*>(luaL_checkudata(L, 1, "Physics3D.CharacterVirtual"));
    if (c) {
        c->valid = false;
        c->physics = nullptr;
    }
    return 0;
}

// ============================================================================
// Physics3D.Vehicle Userdata Methods
// ============================================================================

static int l_vehicle_destroy(lua_State* L) {
    auto* v = static_cast<LuaPhysics3DVehicle*>(luaL_checkudata(L, 1, "Physics3D.Vehicle"));
    if (!v || !v->valid) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool res = false;
    if (v->physics) {
        res = v->physics->destroy_vehicle(v->id);
    }
    v->valid = false;
    lua_pushboolean(L, res);
    return 1;
}

static int l_vehicle_is_valid(lua_State* L) {
    auto* v = static_cast<LuaPhysics3DVehicle*>(luaL_checkudata(L, 1, "Physics3D.Vehicle"));
    lua_pushboolean(L, v && v->valid);
    return 1;
}

static int l_vehicle_get_id(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    lua_pushinteger(L, v->id);
    return 1;
}

static int l_vehicle_set_input_wheeled(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    float forward = static_cast<float>(luaL_checknumber(L, 2));
    float steer = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float brake = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    bool handbrake = lua_toboolean(L, 5);
    v->physics->vehicle_set_input_wheeled(v->id, forward, steer, brake, handbrake);
    return 0;
}

static int l_vehicle_set_input_tracked(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    float left_ratio = static_cast<float>(luaL_checknumber(L, 2));
    float right_ratio = static_cast<float>(luaL_checknumber(L, 3));
    float brake = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    v->physics->vehicle_set_input_tracked(v->id, left_ratio, right_ratio, brake);
    return 0;
}

static int l_vehicle_set_input_motorcycle(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    float forward = static_cast<float>(luaL_checknumber(L, 2));
    float steer = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float brake = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    v->physics->vehicle_set_input_motorcycle(v->id, forward, steer, brake);
    return 0;
}

static int l_vehicle_enable_lean_controller(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    bool enable = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true;
    v->physics->vehicle_enable_lean_controller(v->id, enable);
    return 0;
}

static int l_vehicle_is_lean_controller_enabled(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    lua_pushboolean(L, v->physics->vehicle_is_lean_controller_enabled(v->id));
    return 1;
}

static int l_vehicle_get_lean_angle(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    lua_pushnumber(L, v->physics->vehicle_get_lean_angle(v->id));
    return 1;
}

static int l_vehicle_get_speed_kmh(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    lua_pushnumber(L, v->physics->vehicle_get_speed_kmh(v->id));
    return 1;
}

static int l_vehicle_get_engine_rpm(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    lua_pushnumber(L, v->physics->vehicle_get_engine_rpm(v->id));
    return 1;
}

static int l_vehicle_get_transmission_gear(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    lua_pushinteger(L, v->physics->vehicle_get_transmission_gear(v->id));
    return 1;
}

static int l_vehicle_get_wheel_count(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    lua_pushinteger(L, v->physics->vehicle_get_wheel_count(v->id));
    return 1;
}

static int l_vehicle_get_wheel_transform(lua_State* L) {
    auto* v = check_vehicle(L, 1);
    int wheel_idx = static_cast<int>(luaL_checkinteger(L, 2));
    glm::vec3 pos(0.0f);
    glm::quat rot(1.0f, 0.0f, 0.0f, 0.0f);
    bool ok = v->physics->vehicle_get_wheel_transform(v->id, wheel_idx, pos, rot);
    if (!ok) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 2);
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, pos.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, pos.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, pos.z); lua_setfield(L, -2, "z");
    lua_setfield(L, -2, "position");

    lua_createtable(L, 0, 4);
    lua_pushnumber(L, rot.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, rot.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, rot.z); lua_setfield(L, -2, "z");
    lua_pushnumber(L, rot.w); lua_setfield(L, -2, "w");
    lua_setfield(L, -2, "rotation");
    return 1;
}

static int l_vehicle_tostring(lua_State* L) {
    auto* v = static_cast<LuaPhysics3DVehicle*>(luaL_checkudata(L, 1, "Physics3D.Vehicle"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics3D.Vehicle(ID: %u, valid: %s)", v ? v->id : 0, (v && v->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_vehicle_gc(lua_State* L) {
    auto* v = static_cast<LuaPhysics3DVehicle*>(luaL_checkudata(L, 1, "Physics3D.Vehicle"));
    if (v) {
        v->valid = false;
        v->physics = nullptr;
    }
    return 0;
}

// ============================================================================
// Physics3D.Skeleton Userdata Methods
// ============================================================================

static int l_skeleton_destroy(lua_State* L) {
    auto* s = static_cast<LuaPhysics3DSkeleton*>(luaL_checkudata(L, 1, "Physics3D.Skeleton"));
    if (!s || !s->valid) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool res = false;
    if (s->physics) {
        res = s->physics->destroy_skeleton(s->id);
    }
    s->valid = false;
    lua_pushboolean(L, res);
    return 1;
}

static int l_skeleton_is_valid(lua_State* L) {
    auto* s = static_cast<LuaPhysics3DSkeleton*>(luaL_checkudata(L, 1, "Physics3D.Skeleton"));
    lua_pushboolean(L, s && s->valid);
    return 1;
}

static int l_skeleton_get_id(lua_State* L) {
    auto* s = check_skeleton(L, 1);
    lua_pushinteger(L, s->id);
    return 1;
}

static int l_skeleton_tostring(lua_State* L) {
    auto* s = static_cast<LuaPhysics3DSkeleton*>(luaL_checkudata(L, 1, "Physics3D.Skeleton"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics3D.Skeleton(ID: %u, valid: %s)", s ? s->id : 0, (s && s->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_skeleton_gc(lua_State* L) {
    auto* s = static_cast<LuaPhysics3DSkeleton*>(luaL_checkudata(L, 1, "Physics3D.Skeleton"));
    if (s) {
        s->valid = false;
        s->physics = nullptr;
    }
    return 0;
}

// ============================================================================
// Physics3D.SkeletonPose Userdata Methods
// ============================================================================

static int l_skeleton_pose_destroy(lua_State* L) {
    auto* p = static_cast<LuaPhysics3DSkeletonPose*>(luaL_checkudata(L, 1, "Physics3D.SkeletonPose"));
    if (!p || !p->valid) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool res = false;
    if (p->physics) {
        res = p->physics->destroy_skeleton_pose(p->id);
    }
    p->valid = false;
    lua_pushboolean(L, res);
    return 1;
}

static int l_skeleton_pose_is_valid(lua_State* L) {
    auto* p = static_cast<LuaPhysics3DSkeletonPose*>(luaL_checkudata(L, 1, "Physics3D.SkeletonPose"));
    lua_pushboolean(L, p && p->valid);
    return 1;
}

static int l_skeleton_pose_get_id(lua_State* L) {
    auto* p = check_skeleton_pose(L, 1);
    lua_pushinteger(L, p->id);
    return 1;
}

static int l_skeleton_pose_set_joint(lua_State* L) {
    auto* p = check_skeleton_pose(L, 1);
    int joint_idx = static_cast<int>(luaL_checkinteger(L, 2));
    float tx = static_cast<float>(luaL_checknumber(L, 3));
    float ty = static_cast<float>(luaL_checknumber(L, 4));
    float tz = static_cast<float>(luaL_checknumber(L, 5));
    float rx = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float ry = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float rz = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float rw = static_cast<float>(luaL_optnumber(L, 9, 1.0));
    p->physics->skeleton_pose_set_joint(p->id, joint_idx, glm::vec3(tx, ty, tz), glm::quat(rw, rx, ry, rz));
    return 0;
}

static int l_skeleton_pose_calculate_matrices(lua_State* L) {
    auto* p = check_skeleton_pose(L, 1);
    p->physics->skeleton_pose_calculate_matrices(p->id);
    return 0;
}

static int l_skeleton_pose_get_joint_matrix(lua_State* L) {
    auto* p = check_skeleton_pose(L, 1);
    int joint_idx = static_cast<int>(luaL_checkinteger(L, 2));
    glm::mat4 m = p->physics->skeleton_pose_get_joint_matrix(p->id, joint_idx);
    lua_createtable(L, 16, 0);
    const float* f = glm::value_ptr(m);
    for (int i = 0; i < 16; ++i) {
        lua_pushnumber(L, f[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int l_skeleton_pose_set_root_offset(lua_State* L) {
    auto* p = check_skeleton_pose(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    p->physics->skeleton_pose_set_root_offset(p->id, glm::vec3(x, y, z));
    return 0;
}

static int l_skeleton_pose_get_root_offset(lua_State* L) {
    auto* p = check_skeleton_pose(L, 1);
    glm::vec3 off = p->physics->skeleton_pose_get_root_offset(p->id);
    lua_pushnumber(L, off.x);
    lua_pushnumber(L, off.y);
    lua_pushnumber(L, off.z);
    return 3;
}

static int l_skeleton_pose_get_joint_count(lua_State* L) {
    auto* p = check_skeleton_pose(L, 1);
    lua_pushinteger(L, p->physics->skeleton_pose_get_joint_count(p->id));
    return 1;
}

static int l_skeleton_pose_tostring(lua_State* L) {
    auto* p = static_cast<LuaPhysics3DSkeletonPose*>(luaL_checkudata(L, 1, "Physics3D.SkeletonPose"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics3D.SkeletonPose(ID: %u, valid: %s)", p ? p->id : 0, (p && p->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_skeleton_pose_gc(lua_State* L) {
    auto* p = static_cast<LuaPhysics3DSkeletonPose*>(luaL_checkudata(L, 1, "Physics3D.SkeletonPose"));
    if (p) {
        p->valid = false;
        p->physics = nullptr;
    }
    return 0;
}

// ============================================================================
// Physics3D.SkeletonMapper Userdata Methods
// ============================================================================

static int l_skeleton_mapper_destroy(lua_State* L) {
    auto* m = static_cast<LuaPhysics3DSkeletonMapper*>(luaL_checkudata(L, 1, "Physics3D.SkeletonMapper"));
    if (!m || !m->valid) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool res = false;
    if (m->physics) {
        res = m->physics->destroy_skeleton_mapper(m->id);
    }
    m->valid = false;
    lua_pushboolean(L, res);
    return 1;
}

static int l_skeleton_mapper_is_valid(lua_State* L) {
    auto* m = static_cast<LuaPhysics3DSkeletonMapper*>(luaL_checkudata(L, 1, "Physics3D.SkeletonMapper"));
    lua_pushboolean(L, m && m->valid);
    return 1;
}

static int l_skeleton_mapper_get_id(lua_State* L) {
    auto* m = check_skeleton_mapper(L, 1);
    lua_pushinteger(L, m->id);
    return 1;
}

static int l_skeleton_mapper_map(lua_State* L) {
    auto* m = check_skeleton_mapper(L, 1);
    uint32_t pose_low_id = 0;
    if (lua_isuserdata(L, 2)) {
        pose_low_id = check_skeleton_pose(L, 2)->id;
    } else {
        pose_low_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    }

    uint32_t pose_high_local_id = 0;
    if (lua_isuserdata(L, 3)) {
        pose_high_local_id = check_skeleton_pose(L, 3)->id;
    } else {
        pose_high_local_id = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    }

    uint32_t pose_high_out_model_id = 0;
    if (lua_isuserdata(L, 4)) {
        pose_high_out_model_id = check_skeleton_pose(L, 4)->id;
    } else {
        pose_high_out_model_id = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    }

    m->physics->skeleton_mapper_map(m->id, pose_low_id, pose_high_local_id, pose_high_out_model_id);
    return 0;
}

static int l_skeleton_mapper_map_reverse(lua_State* L) {
    auto* m = check_skeleton_mapper(L, 1);
    uint32_t pose_high_model_id = 0;
    if (lua_isuserdata(L, 2)) {
        pose_high_model_id = check_skeleton_pose(L, 2)->id;
    } else {
        pose_high_model_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    }

    uint32_t pose_low_out_model_id = 0;
    if (lua_isuserdata(L, 3)) {
        pose_low_out_model_id = check_skeleton_pose(L, 3)->id;
    } else {
        pose_low_out_model_id = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    }

    m->physics->skeleton_mapper_map_reverse(m->id, pose_high_model_id, pose_low_out_model_id);
    return 0;
}

static int l_skeleton_mapper_lock_all_translations(lua_State* L) {
    auto* m = check_skeleton_mapper(L, 1);
    uint32_t skel_high_id = 0;
    if (lua_isuserdata(L, 2)) {
        skel_high_id = check_skeleton(L, 2)->id;
    } else {
        skel_high_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    }

    uint32_t neutral_pose_id = 0;
    if (lua_isuserdata(L, 3)) {
        neutral_pose_id = check_skeleton_pose(L, 3)->id;
    } else {
        neutral_pose_id = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    }

    m->physics->skeleton_mapper_lock_all_translations(m->id, skel_high_id, neutral_pose_id);
    return 0;
}

static int l_skeleton_mapper_tostring(lua_State* L) {
    auto* m = static_cast<LuaPhysics3DSkeletonMapper*>(luaL_checkudata(L, 1, "Physics3D.SkeletonMapper"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics3D.SkeletonMapper(ID: %u, valid: %s)", m ? m->id : 0, (m && m->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_skeleton_mapper_gc(lua_State* L) {
    auto* m = static_cast<LuaPhysics3DSkeletonMapper*>(luaL_checkudata(L, 1, "Physics3D.SkeletonMapper"));
    if (m) {
        m->valid = false;
        m->physics = nullptr;
    }
    return 0;
}

// ============================================================================
// Physics3D.Ragdoll Userdata Methods
// ============================================================================

static int l_ragdoll_destroy(lua_State* L) {
    auto* r = static_cast<LuaPhysics3DRagdoll*>(luaL_checkudata(L, 1, "Physics3D.Ragdoll"));
    if (!r || !r->valid) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool res = false;
    if (r->physics) {
        res = r->physics->destroy_ragdoll(r->id);
    }
    r->valid = false;
    lua_pushboolean(L, res);
    return 1;
}

static int l_ragdoll_is_valid(lua_State* L) {
    auto* r = static_cast<LuaPhysics3DRagdoll*>(luaL_checkudata(L, 1, "Physics3D.Ragdoll"));
    lua_pushboolean(L, r && r->valid);
    return 1;
}

static int l_ragdoll_get_id(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    lua_pushinteger(L, r->id);
    return 1;
}

static int l_ragdoll_set_pose(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    uint32_t pose_id = 0;
    if (lua_isuserdata(L, 2)) {
        pose_id = check_skeleton_pose(L, 2)->id;
    } else {
        pose_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    }
    r->physics->ragdoll_set_pose(r->id, pose_id);
    return 0;
}

static int l_ragdoll_drive_to_pose_kinematics(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    uint32_t pose_id = 0;
    if (lua_isuserdata(L, 2)) {
        pose_id = check_skeleton_pose(L, 2)->id;
    } else {
        pose_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    }
    float dt = static_cast<float>(luaL_checknumber(L, 3));
    r->physics->ragdoll_drive_to_pose_kinematics(r->id, pose_id, dt);
    return 0;
}

static int l_ragdoll_drive_to_pose_motors(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    uint32_t pose_id = 0;
    if (lua_isuserdata(L, 2)) {
        pose_id = check_skeleton_pose(L, 2)->id;
    } else {
        pose_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    }
    r->physics->ragdoll_drive_to_pose_motors(r->id, pose_id);
    return 0;
}

static int l_ragdoll_drive_to_pose_motors_velocity(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    uint32_t prev_pose_id = 0;
    if (lua_isuserdata(L, 2)) {
        prev_pose_id = check_skeleton_pose(L, 2)->id;
    } else {
        prev_pose_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    }
    uint32_t pose_id = 0;
    if (lua_isuserdata(L, 3)) {
        pose_id = check_skeleton_pose(L, 3)->id;
    } else {
        pose_id = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    }
    float dt = static_cast<float>(luaL_checknumber(L, 4));
    r->physics->ragdoll_drive_to_pose_motors_velocity(r->id, prev_pose_id, pose_id, dt);
    return 0;
}

static int l_ragdoll_get_pose(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    uint32_t pose_id = 0;
    if (lua_isuserdata(L, 2)) {
        pose_id = check_skeleton_pose(L, 2)->id;
    } else {
        pose_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    }
    r->physics->ragdoll_get_pose(r->id, pose_id);
    return 0;
}

static int l_ragdoll_set_hard_keying(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    bool hard = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true;
    r->physics->ragdoll_set_hard_keying(r->id, hard);
    return 0;
}

static int l_ragdoll_activate(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    r->physics->ragdoll_activate(r->id);
    return 0;
}

static int l_ragdoll_is_active(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    lua_pushboolean(L, r->physics->ragdoll_is_active(r->id));
    return 1;
}

static int l_ragdoll_get_body_id(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    int part_idx = static_cast<int>(luaL_checkinteger(L, 2));
    uint32_t bid = r->physics->ragdoll_get_body_id(r->id, part_idx);
    lua_pushinteger(L, bid);
    return 1;
}

static int l_ragdoll_get_part_count(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    lua_pushinteger(L, r->physics->ragdoll_get_part_count(r->id));
    return 1;
}

static int l_ragdoll_get_skeleton_id(lua_State* L) {
    auto* r = check_ragdoll(L, 1);
    lua_pushinteger(L, r->physics->ragdoll_get_skeleton_id(r->id));
    return 1;
}

static int l_ragdoll_tostring(lua_State* L) {
    auto* r = static_cast<LuaPhysics3DRagdoll*>(luaL_checkudata(L, 1, "Physics3D.Ragdoll"));
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Physics3D.Ragdoll(ID: %u, valid: %s)", r ? r->id : 0, (r && r->valid) ? "true" : "false");
    lua_pushstring(L, buf);
    return 1;
}

static int l_ragdoll_gc(lua_State* L) {
    auto* r = static_cast<LuaPhysics3DRagdoll*>(luaL_checkudata(L, 1, "Physics3D.Ragdoll"));
    if (r) {
        r->valid = false;
        r->physics = nullptr;
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

// ----------------------------------------------------------------------------
// Character & Virtual Character Factories
// ----------------------------------------------------------------------------

static int l_physics_create_character(lua_State* L) {
    PhysicsSystem::CharacterConfig cfg;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "pos");
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1); cfg.pos.x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 2); cfg.pos.y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 3); cfg.pos.z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "radius");
        if (lua_isnumber(L, -1)) cfg.radius = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "half_height");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "halfHeight");
        }
        if (lua_isnumber(L, -1)) cfg.half_height = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "mass");
        if (lua_isnumber(L, -1)) cfg.mass = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "friction");
        if (lua_isnumber(L, -1)) cfg.friction = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "gravity_factor");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "gravityFactor");
        }
        if (lua_isnumber(L, -1)) cfg.gravity_factor = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "max_slope_angle_deg");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "maxSlopeAngleDeg");
        }
        if (lua_isnumber(L, -1)) cfg.max_slope_angle_deg = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "motion");
        if (!lua_isnil(L, -1)) cfg.motion = parse_motion_type(L, -1);
        lua_pop(L, 1);
    } else {
        cfg.pos.x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
        cfg.pos.y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
        cfg.pos.z = static_cast<float>(luaL_optnumber(L, 3, 0.0));
        cfg.radius = static_cast<float>(luaL_optnumber(L, 4, 0.4));
        cfg.half_height = static_cast<float>(luaL_optnumber(L, 5, 0.6));
        cfg.mass = static_cast<float>(luaL_optnumber(L, 6, 80.0));
    }

    auto& ps = Engine::get().get_physics();
    uint32_t cid = ps.create_character(cfg);
    push_character_userdata(L, cid, &ps);
    return 1;
}

static int l_physics_create_character_virtual(lua_State* L) {
    PhysicsSystem::CharacterVirtualConfig cfg;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "pos");
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1); cfg.pos.x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 2); cfg.pos.y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
            lua_rawgeti(L, -1, 3); cfg.pos.z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 1, "radius");
        if (lua_isnumber(L, -1)) cfg.radius = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "half_height");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "halfHeight");
        }
        if (lua_isnumber(L, -1)) cfg.half_height = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "mass");
        if (lua_isnumber(L, -1)) cfg.mass = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "max_slope_angle_deg");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "maxSlopeAngleDeg");
        }
        if (lua_isnumber(L, -1)) cfg.max_slope_angle_deg = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "max_strength");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "maxStrength");
        }
        if (lua_isnumber(L, -1)) cfg.max_strength = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "step_height");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "stepHeight");
        }
        if (lua_isnumber(L, -1)) cfg.step_height = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "predictive_contact_distance");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "predictiveContactDistance");
        }
        if (lua_isnumber(L, -1)) cfg.predictive_contact_distance = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 1, "inner_body");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "innerBody");
        }
        if (lua_isboolean(L, -1)) cfg.inner_body = lua_toboolean(L, -1);
        lua_pop(L, 1);
    } else {
        cfg.pos.x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
        cfg.pos.y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
        cfg.pos.z = static_cast<float>(luaL_optnumber(L, 3, 0.0));
        cfg.radius = static_cast<float>(luaL_optnumber(L, 4, 0.4));
        cfg.half_height = static_cast<float>(luaL_optnumber(L, 5, 0.6));
        cfg.mass = static_cast<float>(luaL_optnumber(L, 6, 70.0));
    }

    auto& ps = Engine::get().get_physics();
    uint32_t cid = ps.create_character_virtual(cfg);
    push_character_virtual_userdata(L, cid, &ps);
    return 1;
}

// ----------------------------------------------------------------------------
// Vehicles Factories & Wheel Parsing Helper
// ----------------------------------------------------------------------------

static PhysicsSystem::WheelConfig parse_wheel_config(lua_State* L, int idx) {
    PhysicsSystem::WheelConfig w;
    if (!lua_istable(L, idx)) return w;

    lua_getfield(L, idx, "pos");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "position");
    }
    if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1); w.position.x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, -1, 2); w.position.y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
        lua_rawgeti(L, -1, 3); w.position.z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "radius");
    if (lua_isnumber(L, -1)) w.radius = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "width");
    if (lua_isnumber(L, -1)) w.width = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "suspension_min_length");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "suspensionMinLength");
    }
    if (lua_isnumber(L, -1)) w.suspension_min_length = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "suspension_max_length");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "suspensionMaxLength");
    }
    if (lua_isnumber(L, -1)) w.suspension_max_length = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "suspension_spring");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "suspensionSpring");
    }
    if (lua_isnumber(L, -1)) w.suspension_spring = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "suspension_damping");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "suspensionDamping");
    }
    if (lua_isnumber(L, -1)) w.suspension_damping = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "max_steer_angle_rad");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "maxSteerAngleRad");
    }
    if (lua_isnumber(L, -1)) w.max_steer_angle_rad = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "max_brake_torque");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "maxBrakeTorque");
    }
    if (lua_isnumber(L, -1)) w.max_brake_torque = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "max_hand_brake_torque");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "maxHandBrakeTorque");
    }
    if (lua_isnumber(L, -1)) w.max_hand_brake_torque = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, idx, "is_front");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "isFront");
    }
    if (lua_isboolean(L, -1)) w.is_front = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "is_drive");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, idx, "isDrive");
    }
    if (lua_isboolean(L, -1)) w.is_drive = lua_toboolean(L, -1);
    lua_pop(L, 1);

    return w;
}

static int l_physics_create_wheeled_vehicle(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    PhysicsSystem::WheeledVehicleConfig cfg;

    lua_getfield(L, 1, "chassis_body_id");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "chassisBodyId");
    }
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "chassis");
    }
    if (!lua_isnil(L, -1)) cfg.chassis_body_id = check_body_id(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "wheels");
    if (lua_istable(L, -1)) {
        int n = static_cast<int>(lua_objlen(L, -1));
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            cfg.wheels.push_back(parse_wheel_config(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "max_pitch_roll_angle");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "maxPitchRollAngle");
    }
    if (lua_isnumber(L, -1)) cfg.max_pitch_roll_angle = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "engine_max_torque");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "engineMaxTorque");
    }
    if (lua_isnumber(L, -1)) cfg.engine_max_torque = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "engine_min_rpm");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "engineMinRpm");
    }
    if (lua_isnumber(L, -1)) cfg.engine_min_rpm = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "engine_max_rpm");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "engineMaxRpm");
    }
    if (lua_isnumber(L, -1)) cfg.engine_max_rpm = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    auto& ps = Engine::get().get_physics();
    uint32_t vid = ps.create_wheeled_vehicle(cfg);
    push_vehicle_userdata(L, vid, &ps);
    return 1;
}

static int l_physics_create_tracked_vehicle(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    PhysicsSystem::TrackedVehicleConfig cfg;

    lua_getfield(L, 1, "chassis_body_id");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "chassisBodyId");
    }
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "chassis");
    }
    if (!lua_isnil(L, -1)) cfg.chassis_body_id = check_body_id(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "left_wheels");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "leftWheels");
    }
    if (lua_istable(L, -1)) {
        int n = static_cast<int>(lua_objlen(L, -1));
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            cfg.left_wheels.push_back(parse_wheel_config(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "right_wheels");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "rightWheels");
    }
    if (lua_istable(L, -1)) {
        int n = static_cast<int>(lua_objlen(L, -1));
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            cfg.right_wheels.push_back(parse_wheel_config(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "engine_max_torque");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "engineMaxTorque");
    }
    if (lua_isnumber(L, -1)) cfg.engine_max_torque = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    auto& ps = Engine::get().get_physics();
    uint32_t vid = ps.create_tracked_vehicle(cfg);
    push_vehicle_userdata(L, vid, &ps);
    return 1;
}

static int l_physics_create_motorcycle(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    PhysicsSystem::MotorcycleConfig cfg;

    lua_getfield(L, 1, "chassis_body_id");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "chassisBodyId");
    }
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "chassis");
    }
    if (!lua_isnil(L, -1)) cfg.chassis_body_id = check_body_id(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "front_wheel");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "frontWheel");
    }
    if (lua_istable(L, -1)) cfg.front_wheel = parse_wheel_config(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "rear_wheel");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "rearWheel");
    }
    if (lua_istable(L, -1)) cfg.rear_wheel = parse_wheel_config(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "max_lean_angle_rad");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "maxLeanAngleRad");
    }
    if (lua_isnumber(L, -1)) cfg.max_lean_angle_rad = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "lean_spring_constant");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "leanSpringConstant");
    }
    if (lua_isnumber(L, -1)) cfg.lean_spring_constant = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "lean_spring_damping");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "leanSpringDamping");
    }
    if (lua_isnumber(L, -1)) cfg.lean_spring_damping = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "lean_smoothing_factor");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "leanSmoothingFactor");
    }
    if (lua_isnumber(L, -1)) cfg.lean_smoothing_factor = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, 1, "engine_max_torque");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "engineMaxTorque");
    }
    if (lua_isnumber(L, -1)) cfg.engine_max_torque = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);

    auto& ps = Engine::get().get_physics();
    uint32_t vid = ps.create_motorcycle(cfg);
    push_vehicle_userdata(L, vid, &ps);
    return 1;
}

// ----------------------------------------------------------------------------
// Skeleton & SkeletonPose & SkeletonMapper Factories
// ----------------------------------------------------------------------------

static int l_physics_create_skeleton(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<std::pair<std::string, int>> joints;
    int n = static_cast<int>(lua_objlen(L, 1));
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 1, i);
        if (lua_istable(L, -1)) {
            std::string name = "Joint";
            int parent_idx = -1;

            lua_getfield(L, -1, "name");
            if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "parent");
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_getfield(L, -1, "parent_idx");
            }
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                lua_getfield(L, -1, "parentIndex");
            }
            if (lua_isnumber(L, -1)) parent_idx = static_cast<int>(lua_tointeger(L, -1));
            lua_pop(L, 1);

            joints.emplace_back(name, parent_idx);
        }
        lua_pop(L, 1);
    }

    auto& ps = Engine::get().get_physics();
    uint32_t sid = ps.create_skeleton(joints);
    push_skeleton_userdata(L, sid, &ps);
    return 1;
}

static int l_physics_create_skeleton_pose(lua_State* L) {
    uint32_t skeleton_id = 0;
    if (lua_isuserdata(L, 1)) {
        skeleton_id = check_skeleton(L, 1)->id;
    } else {
        skeleton_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    }

    auto& ps = Engine::get().get_physics();
    uint32_t pid = ps.create_skeleton_pose(skeleton_id);
    push_skeleton_pose_userdata(L, pid, &ps);
    return 1;
}

static int l_physics_create_skeleton_mapper(lua_State* L) {
    uint32_t skel_low = 0;
    if (lua_isuserdata(L, 1)) skel_low = check_skeleton(L, 1)->id;
    else skel_low = static_cast<uint32_t>(luaL_checkinteger(L, 1));

    uint32_t skel_high = 0;
    if (lua_isuserdata(L, 2)) skel_high = check_skeleton(L, 2)->id;
    else skel_high = static_cast<uint32_t>(luaL_checkinteger(L, 2));

    uint32_t pose_low = 0;
    if (lua_isuserdata(L, 3)) pose_low = check_skeleton_pose(L, 3)->id;
    else pose_low = static_cast<uint32_t>(luaL_checkinteger(L, 3));

    uint32_t pose_high = 0;
    if (lua_isuserdata(L, 4)) pose_high = check_skeleton_pose(L, 4)->id;
    else pose_high = static_cast<uint32_t>(luaL_checkinteger(L, 4));

    auto& ps = Engine::get().get_physics();
    uint32_t mid = ps.create_skeleton_mapper(skel_low, skel_high, pose_low, pose_high);
    push_skeleton_mapper_userdata(L, mid, &ps);
    return 1;
}

// ----------------------------------------------------------------------------
// Ragdoll Factory
// ----------------------------------------------------------------------------

static int l_physics_create_ragdoll(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    PhysicsSystem::RagdollConfig cfg;

    lua_getfield(L, 1, "disable_parent_child_collisions");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, 1, "disableParentChildCollisions");
    }
    if (lua_isboolean(L, -1)) cfg.disable_parent_child_collisions = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "stabilize");
    if (lua_isboolean(L, -1)) cfg.stabilize = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "parts");
    if (lua_istable(L, -1)) {
        int n = static_cast<int>(lua_objlen(L, -1));
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            if (lua_istable(L, -1)) {
                PhysicsSystem::RagdollPartConfig part;
                lua_getfield(L, -1, "name");
                if (lua_isstring(L, -1)) part.name = lua_tostring(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "parent_joint_index");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "parentJointIndex");
                }
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "parent");
                }
                if (lua_isnumber(L, -1)) part.parent_joint_index = static_cast<int>(lua_tointeger(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "pos");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "position");
                }
                if (lua_istable(L, -1)) {
                    lua_rawgeti(L, -1, 1); part.position.x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 2); part.position.y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 3); part.position.z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "rot");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "rotation");
                }
                if (lua_istable(L, -1)) {
                    lua_rawgeti(L, -1, 1); part.rotation.x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 2); part.rotation.y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 3); part.rotation.z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 4); part.rotation.w = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "shape_type");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "shapeType");
                }
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "shape");
                }
                if (lua_isstring(L, -1)) {
                    const char* s = lua_tostring(L, -1);
                    if (std::strcmp(s, "box") == 0) part.shape_type = PhysicsSystem::RagdollPartShape::Box;
                    else if (std::strcmp(s, "sphere") == 0) part.shape_type = PhysicsSystem::RagdollPartShape::Sphere;
                    else part.shape_type = PhysicsSystem::RagdollPartShape::Capsule;
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "half_extent");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "halfExtent");
                }
                if (lua_istable(L, -1)) {
                    lua_rawgeti(L, -1, 1); part.half_extent.x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 2); part.half_extent.y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                    lua_rawgeti(L, -1, 3); part.half_extent.z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "radius");
                if (lua_isnumber(L, -1)) part.radius = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "half_height");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "halfHeight");
                }
                if (lua_isnumber(L, -1)) part.half_height = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "motion");
                if (!lua_isnil(L, -1)) part.motion = parse_motion_type(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "mass");
                if (lua_isnumber(L, -1)) part.mass = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "friction");
                if (lua_isnumber(L, -1)) part.friction = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "swing_limit_y");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "swingLimitY");
                }
                if (lua_isnumber(L, -1)) part.swing_limit_y = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "swing_limit_z");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "swingLimitZ");
                }
                if (lua_isnumber(L, -1)) part.swing_limit_z = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "twist_min");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "twistMin");
                }
                if (lua_isnumber(L, -1)) part.twist_min = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "twist_max");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "twistMax");
                }
                if (lua_isnumber(L, -1)) part.twist_max = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "enable_motors");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "enableMotors");
                }
                if (lua_isboolean(L, -1)) part.enable_motors = lua_toboolean(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "motor_spring_k");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "motorSpringK");
                }
                if (lua_isnumber(L, -1)) part.motor_spring_k = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "motor_damping_c");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "motorDampingC");
                }
                if (lua_isnumber(L, -1)) part.motor_damping_c = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_getfield(L, -1, "motor_max_torque");
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "motorMaxTorque");
                }
                if (lua_isnumber(L, -1)) part.motor_max_torque = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                cfg.parts.push_back(part);
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    auto& ps = Engine::get().get_physics();
    uint32_t rid = ps.create_ragdoll(cfg);
    push_ragdoll_userdata(L, rid, &ps);
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

    // Metamethods
    lua_pushcfunction(L, l_constraint_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_constraint_gc);
    lua_setfield(L, -2, "__gc");

    lua_pop(L, 1);
}

static void register_character_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics3D.Character");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_character_destroy); lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_character_is_valid); lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_character_get_id); lua_setfield(L, -2, "getId");
    lua_pushcfunction(L, l_character_get_body_id); lua_setfield(L, -2, "getBodyId");
    lua_pushcfunction(L, l_character_get_position); lua_setfield(L, -2, "getPosition");
    lua_pushcfunction(L, l_character_set_position); lua_setfield(L, -2, "setPosition");
    lua_pushcfunction(L, l_character_get_rotation); lua_setfield(L, -2, "getRotation");
    lua_pushcfunction(L, l_character_set_rotation); lua_setfield(L, -2, "setRotation");
    lua_pushcfunction(L, l_character_get_linear_velocity); lua_setfield(L, -2, "getLinearVelocity");
    lua_pushcfunction(L, l_character_set_linear_velocity); lua_setfield(L, -2, "setLinearVelocity");
    lua_pushcfunction(L, l_character_is_supported); lua_setfield(L, -2, "isSupported");
    lua_pushcfunction(L, l_character_get_ground_state); lua_setfield(L, -2, "getGroundState");
    lua_pushcfunction(L, l_character_get_ground_normal); lua_setfield(L, -2, "getGroundNormal");
    lua_pushcfunction(L, l_character_get_ground_velocity); lua_setfield(L, -2, "getGroundVelocity");
    lua_pushcfunction(L, l_character_get_ground_position); lua_setfield(L, -2, "getGroundPosition");

    lua_pushcfunction(L, l_character_tostring); lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_character_gc); lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

static void register_character_virtual_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics3D.CharacterVirtual");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_character_virtual_destroy); lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_character_virtual_is_valid); lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_character_virtual_get_id); lua_setfield(L, -2, "getId");
    lua_pushcfunction(L, l_character_virtual_update); lua_setfield(L, -2, "update");
    lua_pushcfunction(L, l_character_virtual_get_position); lua_setfield(L, -2, "getPosition");
    lua_pushcfunction(L, l_character_virtual_set_position); lua_setfield(L, -2, "setPosition");
    lua_pushcfunction(L, l_character_virtual_get_rotation); lua_setfield(L, -2, "getRotation");
    lua_pushcfunction(L, l_character_virtual_set_rotation); lua_setfield(L, -2, "setRotation");
    lua_pushcfunction(L, l_character_virtual_get_linear_velocity); lua_setfield(L, -2, "getLinearVelocity");
    lua_pushcfunction(L, l_character_virtual_set_linear_velocity); lua_setfield(L, -2, "setLinearVelocity");
    lua_pushcfunction(L, l_character_virtual_is_supported); lua_setfield(L, -2, "isSupported");
    lua_pushcfunction(L, l_character_virtual_get_ground_state); lua_setfield(L, -2, "getGroundState");
    lua_pushcfunction(L, l_character_virtual_get_ground_normal); lua_setfield(L, -2, "getGroundNormal");
    lua_pushcfunction(L, l_character_virtual_get_ground_velocity); lua_setfield(L, -2, "getGroundVelocity");
    lua_pushcfunction(L, l_character_virtual_get_ground_position); lua_setfield(L, -2, "getGroundPosition");

    lua_pushcfunction(L, l_character_virtual_tostring); lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_character_virtual_gc); lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

static void register_vehicle_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics3D.Vehicle");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_vehicle_destroy); lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_vehicle_is_valid); lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_vehicle_get_id); lua_setfield(L, -2, "getId");
    lua_pushcfunction(L, l_vehicle_set_input_wheeled); lua_setfield(L, -2, "setInputWheeled");
    lua_pushcfunction(L, l_vehicle_set_input_tracked); lua_setfield(L, -2, "setInputTracked");
    lua_pushcfunction(L, l_vehicle_set_input_motorcycle); lua_setfield(L, -2, "setInputMotorcycle");
    lua_pushcfunction(L, l_vehicle_enable_lean_controller); lua_setfield(L, -2, "enableLeanController");
    lua_pushcfunction(L, l_vehicle_is_lean_controller_enabled); lua_setfield(L, -2, "isLeanControllerEnabled");
    lua_pushcfunction(L, l_vehicle_get_lean_angle); lua_setfield(L, -2, "getLeanAngle");
    lua_pushcfunction(L, l_vehicle_get_speed_kmh); lua_setfield(L, -2, "getSpeedKmh");
    lua_pushcfunction(L, l_vehicle_get_engine_rpm); lua_setfield(L, -2, "getEngineRpm");
    lua_pushcfunction(L, l_vehicle_get_transmission_gear); lua_setfield(L, -2, "getTransmissionGear");
    lua_pushcfunction(L, l_vehicle_get_wheel_count); lua_setfield(L, -2, "getWheelCount");
    lua_pushcfunction(L, l_vehicle_get_wheel_transform); lua_setfield(L, -2, "getWheelTransform");

    lua_pushcfunction(L, l_vehicle_tostring); lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_vehicle_gc); lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

static void register_skeleton_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics3D.Skeleton");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_skeleton_destroy); lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_skeleton_is_valid); lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_skeleton_get_id); lua_setfield(L, -2, "getId");

    lua_pushcfunction(L, l_skeleton_tostring); lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_skeleton_gc); lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

static void register_skeleton_pose_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics3D.SkeletonPose");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_skeleton_pose_destroy); lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_skeleton_pose_is_valid); lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_skeleton_pose_get_id); lua_setfield(L, -2, "getId");
    lua_pushcfunction(L, l_skeleton_pose_set_joint); lua_setfield(L, -2, "setJoint");
    lua_pushcfunction(L, l_skeleton_pose_calculate_matrices); lua_setfield(L, -2, "calculateMatrices");
    lua_pushcfunction(L, l_skeleton_pose_get_joint_matrix); lua_setfield(L, -2, "getJointMatrix");
    lua_pushcfunction(L, l_skeleton_pose_set_root_offset); lua_setfield(L, -2, "setRootOffset");
    lua_pushcfunction(L, l_skeleton_pose_get_root_offset); lua_setfield(L, -2, "getRootOffset");
    lua_pushcfunction(L, l_skeleton_pose_get_joint_count); lua_setfield(L, -2, "getJointCount");

    lua_pushcfunction(L, l_skeleton_pose_tostring); lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_skeleton_pose_gc); lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

static void register_skeleton_mapper_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics3D.SkeletonMapper");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_skeleton_mapper_destroy); lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_skeleton_mapper_is_valid); lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_skeleton_mapper_get_id); lua_setfield(L, -2, "getId");
    lua_pushcfunction(L, l_skeleton_mapper_map); lua_setfield(L, -2, "map");
    lua_pushcfunction(L, l_skeleton_mapper_map_reverse); lua_setfield(L, -2, "mapReverse");
    lua_pushcfunction(L, l_skeleton_mapper_lock_all_translations); lua_setfield(L, -2, "lockAllTranslations");

    lua_pushcfunction(L, l_skeleton_mapper_tostring); lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_skeleton_mapper_gc); lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

static void register_ragdoll_metatable(lua_State* L) {
    luaL_newmetatable(L, "Physics3D.Ragdoll");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, l_ragdoll_destroy); lua_setfield(L, -2, "destroy");
    lua_pushcfunction(L, l_ragdoll_is_valid); lua_setfield(L, -2, "isValid");
    lua_pushcfunction(L, l_ragdoll_get_id); lua_setfield(L, -2, "getId");
    lua_pushcfunction(L, l_ragdoll_set_pose); lua_setfield(L, -2, "setPose");
    lua_pushcfunction(L, l_ragdoll_drive_to_pose_kinematics); lua_setfield(L, -2, "driveToPoseKinematics");
    lua_pushcfunction(L, l_ragdoll_drive_to_pose_motors); lua_setfield(L, -2, "driveToPoseMotors");
    lua_pushcfunction(L, l_ragdoll_drive_to_pose_motors_velocity); lua_setfield(L, -2, "driveToPoseMotorsVelocity");
    lua_pushcfunction(L, l_ragdoll_get_pose); lua_setfield(L, -2, "getPose");
    lua_pushcfunction(L, l_ragdoll_set_hard_keying); lua_setfield(L, -2, "setHardKeying");
    lua_pushcfunction(L, l_ragdoll_activate); lua_setfield(L, -2, "activate");
    lua_pushcfunction(L, l_ragdoll_is_active); lua_setfield(L, -2, "isActive");
    lua_pushcfunction(L, l_ragdoll_get_body_id); lua_setfield(L, -2, "getBodyId");
    lua_pushcfunction(L, l_ragdoll_get_part_count); lua_setfield(L, -2, "getPartCount");
    lua_pushcfunction(L, l_ragdoll_get_skeleton_id); lua_setfield(L, -2, "getSkeletonId");

    lua_pushcfunction(L, l_ragdoll_tostring); lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, l_ragdoll_gc); lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);
}

void register_physics3d_bindings(lua_State* L) {
    register_body_metatable(L);
    register_constraint_metatable(L);
    register_character_metatable(L);
    register_character_virtual_metatable(L);
    register_vehicle_metatable(L);
    register_skeleton_metatable(L);
    register_skeleton_pose_metatable(L);
    register_skeleton_mapper_metatable(L);
    register_ragdoll_metatable(L);

    lua_getglobal(L, "crayon");
    lua_newtable(L);

    // Factories (camelCase)
    lua_pushcfunction(L, l_physics_create_box);
    lua_setfield(L, -2, "createBox");

    lua_pushcfunction(L, l_physics_create_sphere);
    lua_setfield(L, -2, "createSphere");

    lua_pushcfunction(L, l_physics_create_capsule);
    lua_setfield(L, -2, "createCapsule");

    lua_pushcfunction(L, l_physics_create_cylinder);
    lua_setfield(L, -2, "createCylinder");

    lua_pushcfunction(L, l_physics_create_plane);
    lua_setfield(L, -2, "createPlane");

    // Characters (camelCase)
    lua_pushcfunction(L, l_physics_create_character);
    lua_setfield(L, -2, "createCharacter");

    lua_pushcfunction(L, l_physics_create_character_virtual);
    lua_setfield(L, -2, "createCharacterVirtual");

    // Vehicles (camelCase)
    lua_pushcfunction(L, l_physics_create_wheeled_vehicle);
    lua_setfield(L, -2, "createWheeledVehicle");

    lua_pushcfunction(L, l_physics_create_tracked_vehicle);
    lua_setfield(L, -2, "createTrackedVehicle");

    lua_pushcfunction(L, l_physics_create_motorcycle);
    lua_setfield(L, -2, "createMotorcycle");

    // Skeleton & Ragdoll (camelCase)
    lua_pushcfunction(L, l_physics_create_skeleton);
    lua_setfield(L, -2, "createSkeleton");

    lua_pushcfunction(L, l_physics_create_skeleton_pose);
    lua_setfield(L, -2, "createSkeletonPose");

    lua_pushcfunction(L, l_physics_create_skeleton_mapper);
    lua_setfield(L, -2, "createSkeletonMapper");

    lua_pushcfunction(L, l_physics_create_ragdoll);
    lua_setfield(L, -2, "createRagdoll");

    // World operations (camelCase)
    lua_pushcfunction(L, l_physics_destroy_all);
    lua_setfield(L, -2, "destroyAll");

    lua_pushcfunction(L, l_physics_set_gravity);
    lua_setfield(L, -2, "setGravity");

    lua_pushcfunction(L, l_physics_get_gravity);
    lua_setfield(L, -2, "getGravity");

    lua_pushcfunction(L, l_physics_raycast);
    lua_setfield(L, -2, "raycast");

    lua_pushcfunction(L, l_physics_draw_debug);
    lua_setfield(L, -2, "drawDebug");

    lua_pushcfunction(L, l_physics_get_body_count);
    lua_setfield(L, -2, "getBodyCount");

    lua_pushcfunction(L, l_physics_overlap_sphere);
    lua_setfield(L, -2, "overlapSphere");

    // Constraints (camelCase)
    lua_pushcfunction(L, l_physics_create_point_constraint);
    lua_setfield(L, -2, "createPointConstraint");

    lua_pushcfunction(L, l_physics_create_hinge_constraint);
    lua_setfield(L, -2, "createHingeConstraint");

    lua_pushcfunction(L, l_physics_create_distance_constraint);
    lua_setfield(L, -2, "createDistanceConstraint");

    lua_pushcfunction(L, l_physics_create_fixed_constraint);
    lua_setfield(L, -2, "createFixedConstraint");

    lua_pushcfunction(L, l_physics_destroy_constraint);
    lua_setfield(L, -2, "destroyConstraint");

    lua_setfield(L, -2, "physics3d");
    lua_pop(L, 1);
}

} // namespace crayon
