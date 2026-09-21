#include "lua_runtime.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <vector>

namespace crayon {

// --- Vector / Math Helper Functions ---

static float lerp_f(float a, float b, float t) {
    return a + (b - a) * t;
}

static float clamp_f(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static float smoothstep_f(float edge0, float edge1, float x) {
    float t = clamp_f((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static float remap_f(float val, float in_min, float in_max, float out_min, float out_max) {
    return out_min + (val - in_min) * (out_max - out_min) / (in_max - in_min);
}

// Simple Perlin-style gradient noise implementation
static float hash_noise(int x, int y) {
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static float smooth_noise(float x, float y) {
    int i = static_cast<int>(std::floor(x));
    int j = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(i);
    float fy = y - static_cast<float>(j);

    float sx = fx * fx * (3.0f - 2.0f * fx);
    float sy = fy * fy * (3.0f - 2.0f * fy);

    float n00 = hash_noise(i, j);
    float n10 = hash_noise(i + 1, j);
    float n01 = hash_noise(i, j + 1);
    float n11 = hash_noise(i + 1, j + 1);

    float ix0 = lerp_f(n00, n10, sx);
    float ix1 = lerp_f(n01, n11, sx);
    return lerp_f(ix0, ix1, sy);
}

// --- Lua Bindings ---

static int l_math_lerp(lua_State* L) {
    float a = static_cast<float>(luaL_checknumber(L, 1));
    float b = static_cast<float>(luaL_checknumber(L, 2));
    float t = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, lerp_f(a, b, t));
    return 1;
}

static int l_math_clamp(lua_State* L) {
    float v = static_cast<float>(luaL_checknumber(L, 1));
    float min_v = static_cast<float>(luaL_checknumber(L, 2));
    float max_v = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, clamp_f(v, min_v, max_v));
    return 1;
}

static int l_math_smoothstep(lua_State* L) {
    float e0 = static_cast<float>(luaL_checknumber(L, 1));
    float e1 = static_cast<float>(luaL_checknumber(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, smoothstep_f(e0, e1, x));
    return 1;
}

static int l_math_remap(lua_State* L) {
    float val = static_cast<float>(luaL_checknumber(L, 1));
    float in_min = static_cast<float>(luaL_checknumber(L, 2));
    float in_max = static_cast<float>(luaL_checknumber(L, 3));
    float out_min = static_cast<float>(luaL_checknumber(L, 4));
    float out_max = static_cast<float>(luaL_checknumber(L, 5));
    lua_pushnumber(L, remap_f(val, in_min, in_max, out_min, out_max));
    return 1;
}

static int l_math_damp(lua_State* L) {
    float a = static_cast<float>(luaL_checknumber(L, 1));
    float b = static_cast<float>(luaL_checknumber(L, 2));
    float lambda = static_cast<float>(luaL_checknumber(L, 3));
    float dt = static_cast<float>(luaL_checknumber(L, 4));
    float res = lerp_f(a, b, 1.0f - std::exp(-lambda * dt));
    lua_pushnumber(L, res);
    return 1;
}

static int l_math_noise(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    lua_pushnumber(L, smooth_noise(x, y));
    return 1;
}

static int l_math_vec2_len(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    lua_pushnumber(L, std::sqrt(x * x + y * y));
    return 1;
}

static int l_math_vec2_norm(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float len = std::sqrt(x * x + y * y);
    if (len > 0.00001f) {
        lua_pushnumber(L, x / len);
        lua_pushnumber(L, y / len);
    } else {
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
    }
    return 2;
}

static int l_math_vec2_dot(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    lua_pushnumber(L, x1 * x2 + y1 * y2);
    return 1;
}

static int l_math_vec2_dist(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float dx = x2 - x1;
    float dy = y2 - y1;
    lua_pushnumber(L, std::sqrt(dx * dx + dy * dy));
    return 1;
}

static int l_math_vec3_len(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushnumber(L, std::sqrt(x * x + y * y + z * z));
    return 1;
}

static int l_math_vec3_norm(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float len = std::sqrt(x * x + y * y + z * z);
    if (len > 0.00001f) {
        lua_pushnumber(L, x / len);
        lua_pushnumber(L, y / len);
        lua_pushnumber(L, z / len);
    } else {
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
        lua_pushnumber(L, 0.0f);
    }
    return 3;
}

static int l_math_vec3_cross(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float z1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float z2 = static_cast<float>(luaL_checknumber(L, 6));

    glm::vec3 c = glm::cross(glm::vec3(x1, y1, z1), glm::vec3(x2, y2, z2));
    lua_pushnumber(L, c.x);
    lua_pushnumber(L, c.y);
    lua_pushnumber(L, c.z);
    return 3;
}

static int l_math_vec3_dot(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float z1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float z2 = static_cast<float>(luaL_checknumber(L, 6));
    lua_pushnumber(L, x1 * x2 + y1 * y2 + z1 * z2);
    return 1;
}

static int l_math_vec3_dist(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float z1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float z2 = static_cast<float>(luaL_checknumber(L, 6));
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    lua_pushnumber(L, std::sqrt(dx * dx + dy * dy + dz * dz));
    return 1;
}

static int l_math_quat_from_euler(lua_State* L) {
    float pitch = static_cast<float>(luaL_checknumber(L, 1));
    float yaw = static_cast<float>(luaL_checknumber(L, 2));
    float roll = static_cast<float>(luaL_checknumber(L, 3));
    glm::quat q = glm::quat(glm::vec3(pitch, yaw, roll));
    lua_pushnumber(L, q.x);
    lua_pushnumber(L, q.y);
    lua_pushnumber(L, q.z);
    lua_pushnumber(L, q.w);
    return 4;
}

static int l_math_quat_slerp(lua_State* L) {
    glm::quat q1(
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_checknumber(L, 1)),
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3))
    );
    glm::quat q2(
        static_cast<float>(luaL_checknumber(L, 8)),
        static_cast<float>(luaL_checknumber(L, 5)),
        static_cast<float>(luaL_checknumber(L, 6)),
        static_cast<float>(luaL_checknumber(L, 7))
    );
    float t = static_cast<float>(luaL_checknumber(L, 9));
    glm::quat res = glm::slerp(q1, q2, t);
    lua_pushnumber(L, res.x);
    lua_pushnumber(L, res.y);
    lua_pushnumber(L, res.z);
    lua_pushnumber(L, res.w);
    return 4;
}

void register_math_bindings(lua_State* L) {
    lua_getglobal(L, "crayon");
    lua_newtable(L);

    // Scalars & interpolation
    lua_pushcfunction(L, l_math_lerp);
    lua_setfield(L, -2, "lerp");
    lua_pushcfunction(L, l_math_clamp);
    lua_setfield(L, -2, "clamp");
    lua_pushcfunction(L, l_math_smoothstep);
    lua_setfield(L, -2, "smoothstep");
    lua_pushcfunction(L, l_math_remap);
    lua_setfield(L, -2, "remap");
    lua_pushcfunction(L, l_math_damp);
    lua_setfield(L, -2, "damp");
    lua_pushcfunction(L, l_math_noise);
    lua_setfield(L, -2, "noise");

    // 2D Vectors
    lua_pushcfunction(L, l_math_vec2_len);
    lua_setfield(L, -2, "vec2Length");
    lua_pushcfunction(L, l_math_vec2_norm);
    lua_setfield(L, -2, "vec2Normalize");
    lua_pushcfunction(L, l_math_vec2_dot);
    lua_setfield(L, -2, "vec2Dot");
    lua_pushcfunction(L, l_math_vec2_dist);
    lua_setfield(L, -2, "vec2Distance");

    // 3D Vectors
    lua_pushcfunction(L, l_math_vec3_len);
    lua_setfield(L, -2, "vec3Length");
    lua_pushcfunction(L, l_math_vec3_norm);
    lua_setfield(L, -2, "vec3Normalize");
    lua_pushcfunction(L, l_math_vec3_cross);
    lua_setfield(L, -2, "vec3Cross");
    lua_pushcfunction(L, l_math_vec3_dot);
    lua_setfield(L, -2, "vec3Dot");
    lua_pushcfunction(L, l_math_vec3_dist);
    lua_setfield(L, -2, "vec3Distance");

    // Quaternions
    lua_pushcfunction(L, l_math_quat_from_euler);
    lua_setfield(L, -2, "quatFromEuler");
    lua_pushcfunction(L, l_math_quat_slerp);
    lua_setfield(L, -2, "quatSlerp");

    lua_setfield(L, -2, "math");
    lua_pop(L, 1);
}

} // namespace crayon
