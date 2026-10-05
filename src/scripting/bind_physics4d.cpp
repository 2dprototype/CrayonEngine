// Lua bindings for the hv4d 4D physics engine:  crayon.physics4D.*  and the Physics4D.Body userdata.
//
// Conventions
//   * Positions / vectors are 4D: x, y, z, w   (y is up; w is the 4th spatial dimension).
//   * Rotations are bivector "angles" in the six rotation planes, in the order
//         xy, xz, xw, yz, yw, zw        (radians; xy/xz/yz are the familiar 3D rotations,
//                                        xw/yw/zw are the rotations that "turn into" the 4th axis)
//   * Angular velocity is a bivector in the same order, expressed in the body's own frame.
//   * Body ids are plain integers (as used by the collision callbacks); body handles are userdata.

#include "lua_runtime.hpp"
#include "../core/engine.hpp"
#include "../physics/physics4d_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace crayon {

namespace {

struct LuaPhysics4DBody {
    uint32_t id;
    Physics4DSystem* physics;
    bool valid;
};

constexpr const char* BODY_MT = "Physics4D.Body";

inline Physics4DSystem& P4() { return Engine::get().get_physics4d(); }
inline hv4d::World& W() { return Engine::get().get_physics4d().world(); }

// ---------------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------------
float num(lua_State* L, int idx) { return static_cast<float>(luaL_checknumber(L, idx)); }
float opt_num(lua_State* L, int idx, float def) { return static_cast<float>(luaL_optnumber(L, idx, def)); }

hv4d::Vec4 check_vec4(lua_State* L, int idx) {
    return hv4d::Vec4(num(L, idx), num(L, idx + 1), num(L, idx + 2), num(L, idx + 3));
}

hv4d::Bivec4 check_bivec(lua_State* L, int idx) {
    return hv4d::Bivec4(num(L, idx), num(L, idx + 1), num(L, idx + 2), num(L, idx + 3), num(L, idx + 4),
                        num(L, idx + 5));
}

int push_vec4(lua_State* L, const hv4d::Vec4& v) {
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    lua_pushnumber(L, v.w);
    return 4;
}

int push_bivec(lua_State* L, const hv4d::Bivec4& b) {
    lua_pushnumber(L, b.xy);
    lua_pushnumber(L, b.xz);
    lua_pushnumber(L, b.xw);
    lua_pushnumber(L, b.yz);
    lua_pushnumber(L, b.yw);
    lua_pushnumber(L, b.zw);
    return 6;
}

bool field_number(lua_State* L, int tbl, const char* key, float& out) {
    lua_getfield(L, tbl, key);
    const bool ok = lua_isnumber(L, -1);
    if (ok) out = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return ok;
}

bool field_bool(lua_State* L, int tbl, const char* key, bool& out) {
    lua_getfield(L, tbl, key);
    const bool ok = lua_isboolean(L, -1);
    if (ok) out = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return ok;
}

// reads {a, b, c, ...} (n numbers) from table field `key`; returns false if absent/malformed
bool field_array(lua_State* L, int tbl, const char* key, float* out, int n) {
    lua_getfield(L, tbl, key);
    bool ok = lua_istable(L, -1);
    if (ok) {
        for (int i = 0; i < n; ++i) {
            lua_rawgeti(L, -1, i + 1);
            if (!lua_isnumber(L, -1)) ok = false;
            else out[i] = static_cast<float>(lua_tonumber(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return ok;
}

bool field_color(lua_State* L, int tbl, const char* key, glm::vec4& out) {
    float c[4] = {out.r, out.g, out.b, out.a};
    lua_getfield(L, tbl, key);
    bool ok = lua_istable(L, -1);
    if (ok) {
        const int n = static_cast<int>(lua_objlen(L, -1));
        for (int i = 0; i < 4 && i < n; ++i) {
            lua_rawgeti(L, -1, i + 1);
            if (lua_isnumber(L, -1)) c[i] = static_cast<float>(lua_tonumber(L, -1));
            lua_pop(L, 1);
        }
        out = glm::vec4(c[0], c[1], c[2], c[3]);
    }
    lua_pop(L, 1);
    return ok;
}

std::string field_string(lua_State* L, int tbl, const char* key) {
    lua_getfield(L, tbl, key);
    std::string s;
    if (lua_isstring(L, -1)) s = lua_tostring(L, -1);
    lua_pop(L, 1);
    return s;
}

void push_body_userdata(lua_State* L, uint32_t id) {
    auto* u = static_cast<LuaPhysics4DBody*>(lua_newuserdata(L, sizeof(LuaPhysics4DBody)));
    u->id = id;
    u->physics = &P4();
    u->valid = true;
    luaL_getmetatable(L, BODY_MT);
    lua_setmetatable(L, -2);
}

LuaPhysics4DBody* check_handle(lua_State* L, int idx) {
    auto* b = static_cast<LuaPhysics4DBody*>(luaL_checkudata(L, idx, BODY_MT));
    if (!b || !b->valid || b->id == 0 || !b->physics || !b->physics->world().valid(b->id)) {
        luaL_error(L, "attempt to use invalid or destroyed Physics4D.Body");
        return nullptr;
    }
    return b;
}

hv4d::Body* check_body(lua_State* L, int idx) {
    auto* h = check_handle(L, idx);
    return h->physics->world().get(h->id);
}

// Accept either a body handle (userdata) or an integer id.
uint32_t body_id_arg(lua_State* L, int idx) {
    if (lua_isuserdata(L, idx)) {
        auto* b = static_cast<LuaPhysics4DBody*>(luaL_checkudata(L, idx, BODY_MT));
        return b ? b->id : 0;
    }
    return static_cast<uint32_t>(luaL_checkinteger(L, idx));
}

// Common options for body creation:  { mass, restitution, friction, sensor, type, linearDamping,
// angularDamping, gravityScale, rotation = {6}, velocity = {4}, angularVelocity = {6} }
void apply_body_options(lua_State* L, int tbl, hv4d::Body& b) {
    if (tbl == 0 || !lua_istable(L, tbl)) return;

    float f = 0.0f;
    bool flag = false;
    if (field_number(L, tbl, "restitution", f)) b.material.restitution = f;
    if (field_number(L, tbl, "friction", f)) b.material.friction = f;
    if (field_number(L, tbl, "linearDamping", f)) b.linear_damping = f;
    if (field_number(L, tbl, "angularDamping", f)) b.angular_damping = f;
    if (field_number(L, tbl, "gravityScale", f)) b.gravity_scale = f;
    if (field_bool(L, tbl, "sensor", flag)) b.sensor = flag;

    const std::string type = field_string(L, tbl, "type");
    if (type == "static") b.stationary = true;
    else if (type == "dynamic") b.stationary = false;
    else if (type == "sensor") { b.stationary = true; b.sensor = true; }

    if (field_number(L, tbl, "mass", f)) b.dynamic_mass = f;
    b.recompute_mass_properties();

    float a[6];
    if (field_array(L, tbl, "rotation", a, 6)) b.rotation = hv4d::Rotor4::from_angles(hv4d::Bivec4(a[0], a[1], a[2], a[3], a[4], a[5]));
    if (field_array(L, tbl, "velocity", a, 4) && !b.stationary) b.vel.linear = hv4d::Vec4(a[0], a[1], a[2], a[3]);
    if (field_array(L, tbl, "angularVelocity", a, 6) && !b.stationary)
        b.vel.angular = hv4d::Bivec4(a[0], a[1], a[2], a[3], a[4], a[5]);
}

int pushed_body(lua_State* L, hv4d::BodyId id) {
    push_body_userdata(L, id);
    return 1;
}

} // namespace

// =================================================================================
// Body methods
// =================================================================================
static int l_body_get_id(lua_State* L) {
    lua_pushinteger(L, check_handle(L, 1)->id);
    return 1;
}

static int l_body_is_valid(lua_State* L) {
    auto* b = static_cast<LuaPhysics4DBody*>(luaL_checkudata(L, 1, BODY_MT));
    lua_pushboolean(L, b && b->valid && b->id != 0 && b->physics && b->physics->world().valid(b->id));
    return 1;
}

static int l_body_destroy(lua_State* L) {
    auto* b = static_cast<LuaPhysics4DBody*>(luaL_checkudata(L, 1, BODY_MT));
    if (b && b->valid && b->physics) {
        b->physics->world().remove_body(b->id);
        b->valid = false;
    }
    return 0;
}

static int l_body_get_position(lua_State* L) { return push_vec4(L, check_body(L, 1)->pos); }

static int l_body_set_position(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    b->pos = check_vec4(L, 2);
    return 0;
}

static int l_body_get_linear_velocity(lua_State* L) { return push_vec4(L, check_body(L, 1)->vel.linear); }

static int l_body_set_linear_velocity(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    if (!b->stationary) b->vel.linear = check_vec4(L, 2);
    return 0;
}

static int l_body_get_angular_velocity(lua_State* L) { return push_bivec(L, check_body(L, 1)->vel.angular); }

static int l_body_set_angular_velocity(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    if (!b->stationary) b->vel.angular = check_bivec(L, 2);
    return 0;
}

// getRotor() -> s, xy, xz, xw, yz, yw, zw, xyzw
static int l_body_get_rotor(lua_State* L) {
    const hv4d::Rotor4& r = check_body(L, 1)->rotation;
    lua_pushnumber(L, r.s);
    push_bivec(L, r.b);
    lua_pushnumber(L, r.q.xyzw);
    return 8;
}

// setRotation(xy, xz, xw, yz, yw, zw): orientation = rotation by those plane angles from identity
static int l_body_set_rotation(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    b->rotation = hv4d::Rotor4::from_angles(check_bivec(L, 2));
    return 0;
}

// rotateBy(xy, xz, xw, yz, yw, zw): apply an additional rotation in the body frame
static int l_body_rotate_by(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    b->rotation.update(check_bivec(L, 2));
    return 0;
}

static int l_body_reset_rotation(lua_State* L) {
    check_body(L, 1)->rotation = hv4d::Rotor4::identity();
    return 0;
}

static int l_body_local_to_world(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    return push_vec4(L, b->body_pos_to_world(check_vec4(L, 2)));
}

static int l_body_world_to_local(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    return push_vec4(L, b->world_pos_to_body(check_vec4(L, 2)));
}

static int l_body_local_dir_to_world(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    return push_vec4(L, b->body_vec_to_world(check_vec4(L, 2)));
}

// applyImpulse(ix, iy, iz, iw [, px, py, pz, pw])  (point defaults to the centre of mass)
static int l_body_apply_impulse(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    const hv4d::Vec4 imp = check_vec4(L, 2);
    if (lua_gettop(L) >= 9) b->apply_impulse(imp, check_vec4(L, 6));
    else b->apply_central_impulse(imp);
    return 0;
}

static int l_body_apply_force(lua_State* L) {
    check_body(L, 1)->apply_force(check_vec4(L, 2));
    return 0;
}

static int l_body_apply_torque(lua_State* L) {
    check_body(L, 1)->apply_torque(check_bivec(L, 2));
    return 0;
}

static int l_body_get_mass(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    lua_pushnumber(L, b->stationary ? 0.0f : b->mass);
    return 1;
}

static int l_body_set_mass(lua_State* L) {
    check_body(L, 1)->set_dynamic_mass(num(L, 2));
    return 0;
}

static int l_body_is_static(lua_State* L) {
    lua_pushboolean(L, check_body(L, 1)->stationary);
    return 1;
}

static int l_body_set_static(lua_State* L) {
    check_body(L, 1)->set_stationary(lua_toboolean(L, 2) != 0);
    return 0;
}

static int l_body_is_sensor(lua_State* L) {
    lua_pushboolean(L, check_body(L, 1)->sensor);
    return 1;
}

static int l_body_set_sensor(lua_State* L) {
    check_body(L, 1)->sensor = lua_toboolean(L, 2) != 0;
    return 0;
}

static int l_body_get_restitution(lua_State* L) {
    lua_pushnumber(L, check_body(L, 1)->material.restitution);
    return 1;
}
static int l_body_set_restitution(lua_State* L) {
    check_body(L, 1)->material.restitution = num(L, 2);
    return 0;
}
static int l_body_get_friction(lua_State* L) {
    lua_pushnumber(L, check_body(L, 1)->material.friction);
    return 1;
}
static int l_body_set_friction(lua_State* L) {
    check_body(L, 1)->material.friction = num(L, 2);
    return 0;
}
static int l_body_get_gravity_scale(lua_State* L) {
    lua_pushnumber(L, check_body(L, 1)->gravity_scale);
    return 1;
}
static int l_body_set_gravity_scale(lua_State* L) {
    check_body(L, 1)->gravity_scale = num(L, 2);
    return 0;
}
static int l_body_set_damping(lua_State* L) {
    hv4d::Body* b = check_body(L, 1);
    b->linear_damping = num(L, 2);
    b->angular_damping = opt_num(L, 3, b->angular_damping);
    return 0;
}

// getShape() -> "8cell" | "5cell" | ... | "hypersphere" | "halfspace"
static int l_body_get_shape(lua_State* L) {
    const hv4d::Body* b = check_body(L, 1);
    switch (b->collider.type) {
        case hv4d::ColliderType::Mesh:      lua_pushstring(L, Physics4DSystem::solid_name(b->collider.solid)); break;
        case hv4d::ColliderType::Sphere:    lua_pushstring(L, "hypersphere"); break;
        case hv4d::ColliderType::HalfSpace: lua_pushstring(L, "halfspace"); break;
    }
    return 1;
}

// getRadius() -> bounding hypersphere radius (nil for half-spaces)
static int l_body_get_radius(lua_State* L) {
    const hv4d::Body* b = check_body(L, 1);
    if (b->collider.type == hv4d::ColliderType::HalfSpace) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, b->collider.bounding_radius());
    }
    return 1;
}

// getHalfSpaceNormal() -> nx, ny, nz, nw (half-space bodies only)
static int l_body_get_half_space_normal(lua_State* L) {
    const hv4d::Body* b = check_body(L, 1);
    if (b->collider.type != hv4d::ColliderType::HalfSpace) {
        lua_pushnil(L);
        return 1;
    }
    return push_vec4(L, b->collider.normal);
}

// getVertices() -> flat array {x,y,z,w, x,y,z,w, ...} of the polytope's world-space vertices
static int l_body_get_vertices(lua_State* L) {
    const hv4d::Body* b = check_body(L, 1);
    lua_newtable(L);
    if (b->collider.type != hv4d::ColliderType::Mesh) return 1;
    const hv4d::Mat4 rot = b->rotation.to_matrix();
    int k = 1;
    for (const hv4d::Vec4& v : b->collider.mesh->vertices) {
        const hv4d::Vec4 p = rot * v + b->pos;
        for (int c = 0; c < 4; ++c) {
            lua_pushnumber(L, p[c]);
            lua_rawseti(L, -2, k++);
        }
    }
    return 1;
}

// getSliceSegments() -> flat array {ax,ay,az, bx,by,bz, ...}: wireframe of this body's cross-section
static int l_body_get_slice_segments(lua_State* L) {
    const hv4d::Body* b = check_body(L, 1);
    std::vector<hv4d::SliceSegment> segs;
    hv4d::slice_body_wireframe(*b, P4().slice(), segs);
    lua_newtable(L);
    int k = 1;
    for (const auto& s : segs) {
        const float v[6] = {s.a.x, s.a.y, s.a.z, s.b.x, s.b.y, s.b.z};
        for (float f : v) {
            lua_pushnumber(L, f);
            lua_rawseti(L, -2, k++);
        }
    }
    return 1;
}

// getSliceTriangles() -> flat array of 9 numbers per triangle (surface of the cross-section)
static int l_body_get_slice_triangles(lua_State* L) {
    const hv4d::Body* b = check_body(L, 1);
    std::vector<hv4d::SliceTriangle> tris;
    hv4d::slice_body_triangles(*b, P4().slice(), tris);
    lua_newtable(L);
    int k = 1;
    for (const auto& t : tris) {
        for (int i = 0; i < 3; ++i) {
            lua_pushnumber(L, t.p[i].x); lua_rawseti(L, -2, k++);
            lua_pushnumber(L, t.p[i].y); lua_rawseti(L, -2, k++);
            lua_pushnumber(L, t.p[i].z); lua_rawseti(L, -2, k++);
        }
    }
    return 1;
}

// getSliceSphere() -> cx, cy, cz, radius   (hyperspheres whose cross-section is non-empty; nil otherwise)
static int l_body_get_slice_sphere(lua_State* L) {
    const hv4d::Body* b = check_body(L, 1);
    hv4d::Vec3 c;
    float r = 0.0f;
    if (!hv4d::slice_sphere(*b, P4().slice(), c, r)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, c.x);
    lua_pushnumber(L, c.y);
    lua_pushnumber(L, c.z);
    lua_pushnumber(L, r);
    return 4;
}

// distanceToSlice() -> signed distance of the body's centre from the slice hyperplane
static int l_body_distance_to_slice(lua_State* L) {
    lua_pushnumber(L, P4().slice().signed_distance(check_body(L, 1)->pos));
    return 1;
}

static int l_body_tostring(lua_State* L) {
    auto* b = static_cast<LuaPhysics4DBody*>(luaL_checkudata(L, 1, BODY_MT));
    lua_pushfstring(L, "Physics4D.Body(%d%s)", b ? static_cast<int>(b->id) : 0, (b && b->valid) ? "" : ", destroyed");
    return 1;
}

static void register_body_metatable(lua_State* L) {
    luaL_newmetatable(L, BODY_MT);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    struct Entry { const char* name; lua_CFunction fn; };
    static const Entry entries[] = {
        {"getId", l_body_get_id},
        {"isValid", l_body_is_valid},
        {"destroy", l_body_destroy},
        {"getPosition", l_body_get_position},
        {"setPosition", l_body_set_position},
        {"getLinearVelocity", l_body_get_linear_velocity},
        {"setLinearVelocity", l_body_set_linear_velocity},
        {"getVelocity", l_body_get_linear_velocity},
        {"setVelocity", l_body_set_linear_velocity},
        {"getAngularVelocity", l_body_get_angular_velocity},
        {"setAngularVelocity", l_body_set_angular_velocity},
        {"getRotor", l_body_get_rotor},
        {"setRotation", l_body_set_rotation},
        {"rotateBy", l_body_rotate_by},
        {"resetRotation", l_body_reset_rotation},
        {"localToWorld", l_body_local_to_world},
        {"worldToLocal", l_body_world_to_local},
        {"localDirectionToWorld", l_body_local_dir_to_world},
        {"applyImpulse", l_body_apply_impulse},
        {"applyForce", l_body_apply_force},
        {"applyTorque", l_body_apply_torque},
        {"getMass", l_body_get_mass},
        {"setMass", l_body_set_mass},
        {"isStatic", l_body_is_static},
        {"setStatic", l_body_set_static},
        {"isSensor", l_body_is_sensor},
        {"setSensor", l_body_set_sensor},
        {"getRestitution", l_body_get_restitution},
        {"setRestitution", l_body_set_restitution},
        {"getFriction", l_body_get_friction},
        {"setFriction", l_body_set_friction},
        {"getGravityScale", l_body_get_gravity_scale},
        {"setGravityScale", l_body_set_gravity_scale},
        {"setDamping", l_body_set_damping},
        {"getShape", l_body_get_shape},
        {"getRadius", l_body_get_radius},
        {"getHalfSpaceNormal", l_body_get_half_space_normal},
        {"getVertices", l_body_get_vertices},
        {"getSliceSegments", l_body_get_slice_segments},
        {"getSliceTriangles", l_body_get_slice_triangles},
        {"getSliceSphere", l_body_get_slice_sphere},
        {"distanceToSlice", l_body_distance_to_slice},
        {"__tostring", l_body_tostring},
    };
    for (const Entry& e : entries) {
        lua_pushcfunction(L, e.fn);
        lua_setfield(L, -2, e.name);
    }
    lua_pop(L, 1);
}

// =================================================================================
// Module functions
// =================================================================================

// parses [scale] [opts] starting at `first`; both optional, opts may come first position
static void scale_and_opts(lua_State* L, int first, float default_scale, float& scale, int& opts_idx) {
    scale = default_scale;
    opts_idx = 0;
    int top = lua_gettop(L);
    for (int i = first; i <= top; ++i) {
        if (lua_istable(L, i)) opts_idx = i;
        else if (lua_isnumber(L, i) && i == first) scale = static_cast<float>(lua_tonumber(L, i));
    }
    if (opts_idx) {
        float s;
        if (field_number(L, opts_idx, "scale", s)) scale = s;
    }
}

// createPolytope(shape, x, y, z, w [, scale] [, opts])
static int l_create_polytope(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    hv4d::RegularSolid solid;
    if (!Physics4DSystem::parse_solid(name, solid)) {
        return luaL_error(L, "unknown 4D shape '%s' (use 5cell, 8cell/tesseract, 16cell, 24cell, 120cell, 600cell)", name);
    }
    const hv4d::Vec4 pos = check_vec4(L, 2);
    float scale; int opts;
    scale_and_opts(L, 6, 1.0f, scale, opts);
    if (!(scale > 0.0f)) return luaL_error(L, "scale must be > 0");

    auto& w = W();
    hv4d::BodyId id = w.create_polytope(solid, pos, scale, 1.0f, false);
    apply_body_options(L, opts, *w.get(id));
    return pushed_body(L, id);
}

// createTesseract(x, y, z, w [, edgeLength] [, opts])
static int l_create_tesseract(lua_State* L) {
    const hv4d::Vec4 pos = check_vec4(L, 1);
    float scale; int opts;
    scale_and_opts(L, 5, 1.0f, scale, opts);
    if (!(scale > 0.0f)) return luaL_error(L, "size must be > 0");
    auto& w = W();
    hv4d::BodyId id = w.create_polytope(hv4d::RegularSolid::EightCell, pos, scale, 1.0f, false);
    apply_body_options(L, opts, *w.get(id));
    return pushed_body(L, id);
}

// createHypersphere(x, y, z, w [, radius] [, opts])
static int l_create_hypersphere(lua_State* L) {
    const hv4d::Vec4 pos = check_vec4(L, 1);
    float radius; int opts;
    scale_and_opts(L, 5, 0.5f, radius, opts);
    if (opts) {
        float r;
        if (field_number(L, opts, "radius", r)) radius = r;
    }
    if (!(radius > 0.0f)) return luaL_error(L, "radius must be > 0");
    auto& w = W();
    hv4d::BodyId id = w.create_hypersphere(pos, radius, 1.0f, false);
    apply_body_options(L, opts, *w.get(id));
    return pushed_body(L, id);
}

// createHalfSpace(px, py, pz, pw, nx, ny, nz, nw [, opts])   normal points to the free (empty) side
static int l_create_half_space(lua_State* L) {
    const hv4d::Vec4 p = check_vec4(L, 1);
    const hv4d::Vec4 n = check_vec4(L, 5);
    if (!(n.length() > 1e-6f)) return luaL_error(L, "half-space normal must be non-zero");
    auto& w = W();
    hv4d::BodyId id = w.create_half_space(p, n);
    hv4d::Body* b = w.get(id);
    if (lua_istable(L, 9)) {
        float f;
        if (field_number(L, 9, "restitution", f)) b->material.restitution = f;
        if (field_number(L, 9, "friction", f)) b->material.friction = f;
    }
    return pushed_body(L, id);
}

// createArena([halfSize = 10] [, friction = 0.4] [, restitution = 0.4])
// A floor at y = 0 plus walls at +-halfSize on x, z and w.  Returns nothing.
static int l_create_arena(lua_State* L) {
    const float half = opt_num(L, 1, 10.0f);
    hv4d::Material m;
    m.friction = opt_num(L, 2, 0.4f);
    m.restitution = opt_num(L, 3, 0.4f);
    W().create_arena(half, m);
    return 0;
}

static int l_destroy_body(lua_State* L) {
    const uint32_t id = body_id_arg(L, 1);
    const bool ok = W().remove_body(id);
    if (lua_isuserdata(L, 1)) {
        auto* b = static_cast<LuaPhysics4DBody*>(lua_touserdata(L, 1));
        if (b) b->valid = false;
    }
    lua_pushboolean(L, ok);
    return 1;
}

static int l_destroy_all(lua_State* /*L*/) {
    W().clear();
    return 0;
}

static int l_get_body(lua_State* L) {
    const uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    if (!W().valid(id)) {
        lua_pushnil(L);
        return 1;
    }
    return pushed_body(L, id);
}

static int l_get_body_count(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(W().body_count()));
    return 1;
}

static int l_set_gravity(lua_State* L) {
    W().gravity = check_vec4(L, 1);
    return 0;
}

static int l_get_gravity(lua_State* L) { return push_vec4(L, W().gravity); }

static int l_set_solver_iterations(lua_State* L) {
    W().solver_iterations = std::max(1, static_cast<int>(luaL_checkinteger(L, 1)));
    return 0;
}

// step(dt): manual stepping (the engine already steps at a fixed 60 Hz)
static int l_step(lua_State* L) {
    P4().update(opt_num(L, 1, 1.0f / 60.0f));
    return 0;
}

// raycast(ox,oy,oz,ow, dx,dy,dz,dw [, maxDistance])
//   -> hit(bool), px,py,pz,pw, nx,ny,nz,nw, distance, bodyId     (hit = false -> only `false`)
static int l_raycast(lua_State* L) {
    const hv4d::Vec4 o = check_vec4(L, 1);
    const hv4d::Vec4 d = check_vec4(L, 5);
    const float max_t = opt_num(L, 9, 1e30f);
    const hv4d::RayHit h = W().raycast(o, d, max_t);
    if (!h.hit) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    push_vec4(L, h.point);
    push_vec4(L, h.normal);
    lua_pushnumber(L, h.t);
    lua_pushinteger(L, h.body);
    return 11;
}

// overlapSphere(x,y,z,w, radius) -> { bodyId, ... }
static int l_overlap_sphere(lua_State* L) {
    const hv4d::Vec4 c = check_vec4(L, 1);
    const float r = num(L, 5);
    const auto ids = W().overlap_sphere(c, r);
    lua_newtable(L);
    int k = 1;
    for (hv4d::BodyId id : ids) {
        lua_pushinteger(L, id);
        lua_rawseti(L, -2, k++);
    }
    return 1;
}

// testOverlap(bodyA, bodyB) -- exact convex test with GJK + EPA
//   overlapping : true,  depth,    nx,ny,nz,nw   (normal points from A towards B)
//   separated   : false, distance, dx,dy,dz,dw   (direction from A towards B)
static int l_test_overlap(lua_State* L) {
    const hv4d::Body* a = W().get(body_id_arg(L, 1));
    const hv4d::Body* b = W().get(body_id_arg(L, 2));
    if (!a || !b) return luaL_error(L, "testOverlap: invalid body");
    if (a->collider.type == hv4d::ColliderType::HalfSpace || b->collider.type == hv4d::ColliderType::HalfSpace) {
        return luaL_error(L, "testOverlap: half-spaces are not supported (use polytopes / hyperspheres)");
    }
    const hv4d::GjkResult r = hv4d::gjk_epa(*a, *b);
    lua_pushboolean(L, r.intersecting);
    if (r.intersecting) {
        lua_pushnumber(L, r.epa_ok ? r.penetration_depth : 0.0f);
        push_vec4(L, r.epa_ok ? r.penetration_normal : hv4d::Vec4::zero());
    } else {
        lua_pushnumber(L, r.distance);
        push_vec4(L, r.separating_direction);
    }
    return 6;
}

// ---- slice control -------------------------------------------------------------
static int l_set_slice(lua_State* L) {
    P4().set_slice_w(num(L, 1));
    return 0;
}

static int l_get_slice(lua_State* L) {
    lua_pushnumber(L, P4().get_slice_w());
    return 1;
}

// setSliceHyperplane(nx,ny,nz,nw [, bx,by,bz,bw])  -- an arbitrary slicing hyperplane (normal + a point on it)
static int l_set_slice_hyperplane(lua_State* L) {
    const hv4d::Vec4 n = check_vec4(L, 1);
    hv4d::Vec4 base = hv4d::Vec4::zero();
    if (lua_gettop(L) >= 8) base = check_vec4(L, 5);
    if (!(n.length() > 1e-6f)) return luaL_error(L, "slice normal must be non-zero");
    P4().set_slice_hyperplane(glm::vec4(n.x, n.y, n.z, n.w), glm::vec4(base.x, base.y, base.z, base.w));
    return 0;
}

static int l_get_slice_normal(lua_State* L) { return push_vec4(L, P4().slice().normal); }

// sliceToWorld(x, y, z) -> x, y, z, w : a point of the 3D slice space as a 4D world point
static int l_slice_to_world(lua_State* L) {
    return push_vec4(L, P4().slice().unproject(hv4d::Vec3(num(L, 1), num(L, 2), num(L, 3))));
}

// sliceDirectionToWorld(x, y, z) -> x, y, z, w : a direction in slice space as a 4D direction
static int l_slice_dir_to_world(lua_State* L) {
    const hv4d::SlicePlane& s = P4().slice();
    const hv4d::Vec4 d = s.e[0] * num(L, 1) + s.e[1] * num(L, 2) + s.e[2] * num(L, 3);
    return push_vec4(L, d);
}

// worldToSlice(x, y, z, w) -> sx, sy, sz, distance : project a 4D point into slice space (+ signed distance to the slice)
static int l_world_to_slice(lua_State* L) {
    const hv4d::Vec4 p = check_vec4(L, 1);
    const hv4d::SlicePlane& s = P4().slice();
    const hv4d::Vec3 q = s.project(p);
    lua_pushnumber(L, q.x);
    lua_pushnumber(L, q.y);
    lua_pushnumber(L, q.z);
    lua_pushnumber(L, s.signed_distance(p));
    return 4;
}

// ---- events -----------------------------------------------------------------------
// Nothing to poll: use crayon.onCollision4DEnter / Exit and crayon.onTrigger4DEnter / Exit.

// ---- debug drawing -------------------------------------------------------------
// drawDebug([{ shapes, fill, projection, contacts, bounds, velocities, planes,
//              color={r,g,b,a}, staticColor, sensorColor, contactColor,
//              projectionRange, planeExtent, planeDivisions }])
static int l_draw_debug(lua_State* L) {
    Physics4DDebugFlags f;
    if (lua_gettop(L) >= 1 && lua_istable(L, 1)) {
        bool b;
        if (field_bool(L, 1, "shapes", b)) f.shapes = b;
        if (field_bool(L, 1, "fill", b)) f.fill = b;
        if (field_bool(L, 1, "projection", b)) f.projection = b;
        if (field_bool(L, 1, "contacts", b)) f.contacts = b;
        if (field_bool(L, 1, "bounds", b)) f.bounds = b;
        if (field_bool(L, 1, "velocities", b)) f.velocities = b;
        if (field_bool(L, 1, "planes", b)) f.planes = b;
        field_color(L, 1, "color", f.color);
        field_color(L, 1, "staticColor", f.static_color);
        field_color(L, 1, "sensorColor", f.sensor_color);
        field_color(L, 1, "contactColor", f.contact_color);
        float v;
        if (field_number(L, 1, "projectionRange", v)) f.projection_range = v;
        if (field_number(L, 1, "planeExtent", v)) f.plane_extent = v;
        if (field_number(L, 1, "planeDivisions", v)) f.plane_divisions = static_cast<int>(v);
    }
    P4().draw_debug(Engine::get().get_mesh_renderer(), f);
    return 0;
}

// getShapes() -> list of valid shape names for createPolytope
static int l_get_shapes(lua_State* L) {
    static const char* names[] = {"5cell", "8cell", "16cell", "24cell", "120cell", "600cell"};
    lua_newtable(L);
    for (int i = 0; i < 6; ++i) {
        lua_pushstring(L, names[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

void register_physics4d_bindings(lua_State* L) {
    register_body_metatable(L);

    lua_getglobal(L, "crayon");
    lua_newtable(L);

    struct Entry { const char* name; lua_CFunction fn; };
    static const Entry entries[] = {
        {"createPolytope", l_create_polytope},
        {"createTesseract", l_create_tesseract},
        {"createHypersphere", l_create_hypersphere},
        {"createHalfSpace", l_create_half_space},
        {"createArena", l_create_arena},
        {"destroyBody", l_destroy_body},
        {"destroyAll", l_destroy_all},
        {"getBody", l_get_body},
        {"getBodyCount", l_get_body_count},
        {"setGravity", l_set_gravity},
        {"getGravity", l_get_gravity},
        {"setSolverIterations", l_set_solver_iterations},
        {"step", l_step},
        {"raycast", l_raycast},
        {"overlapSphere", l_overlap_sphere},
        {"testOverlap", l_test_overlap},
        {"setSlice", l_set_slice},
        {"getSlice", l_get_slice},
        {"setSliceHyperplane", l_set_slice_hyperplane},
        {"getSliceNormal", l_get_slice_normal},
        {"sliceToWorld", l_slice_to_world},
        {"sliceDirectionToWorld", l_slice_dir_to_world},
        {"worldToSlice", l_world_to_slice},
        {"drawDebug", l_draw_debug},
        {"getShapes", l_get_shapes},
    };
    for (const Entry& e : entries) {
        lua_pushcfunction(L, e.fn);
        lua_setfield(L, -2, e.name);
    }

    lua_setfield(L, -2, "physics4D");
    lua_pop(L, 1);
}

} // namespace crayon
