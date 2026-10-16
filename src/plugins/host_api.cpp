#include "host_api.hpp"

#include "../core/engine.hpp"
#include "../core/log.hpp"
#include "../graphics/mesh3d.hpp"
#include "../scripting/lua_runtime.hpp"

#include <glm/glm.hpp>

namespace crayon {

namespace {

// 3D debug drawing is only valid while the mesh renderer exists (t.modules.mesh3D) -> else no-op.
MeshRenderer3D* renderer() {
    Engine& e = Engine::get();
    return e.get_config().modules.mesh3d ? &e.get_mesh_renderer() : nullptr;
}

inline glm::vec3 v3(const float p[3]) { return glm::vec3(p[0], p[1], p[2]); }
inline glm::vec4 v4(const float c[4]) { return glm::vec4(c[0], c[1], c[2], c[3]); }

void host_log(int level, const char* message) {
    const char* m = message ? message : "";
    switch (level) {
        case CRAYON_LOG_LEVEL_DEBUG: CRAYON_LOG_DEBUG("{}", m); break;
        case CRAYON_LOG_LEVEL_WARN:  CRAYON_LOG_WARN("{}", m);  break;
        case CRAYON_LOG_LEVEL_ERROR: CRAYON_LOG_ERROR("{}", m); break;
        default:                     CRAYON_LOG_INFO("{}", m);  break;
    }
}

lua_State* host_get_lua_state() {
    Engine& e = Engine::get();
    return e.get_lua_runtime().get_state();
}

void host_get_model_color(float out[4]) {
    if (MeshRenderer3D* r = renderer()) {
        const glm::vec4& c = r->get_model_color();
        out[0] = c.r; out[1] = c.g; out[2] = c.b; out[3] = c.a;
    } else {
        out[0] = out[1] = out[2] = out[3] = 1.0f;
    }
}

void host_set_model_color(const float rgba[4]) {
    if (MeshRenderer3D* r = renderer()) r->set_model_color(v4(rgba));
}

void host_draw_triangle_3d(const float p0[3], const float p1[3], const float p2[3], const float rgba[4]) {
    if (MeshRenderer3D* r = renderer()) r->draw_triangle_3d(v3(p0), v3(p1), v3(p2), v4(rgba));
}

void host_draw_sphere_3d(const float c[3], float radius) {
    if (MeshRenderer3D* r = renderer()) r->draw_sphere(v3(c), radius);
}

void host_draw_line_3d(const float a[3], const float b[3], const float rgba[4]) {
    if (MeshRenderer3D* r = renderer()) r->draw_line_3d(v3(a), v3(b), v4(rgba));
}

void host_line_batch_begin() { if (MeshRenderer3D* r = renderer()) r->begin_line_batch(); }

void host_line_batch_add(const float a[3], const float b[3], const float rgba[4]) {
    if (MeshRenderer3D* r = renderer()) r->add_line_to_batch(v3(a), v3(b), v4(rgba));
}

void host_line_batch_wire_sphere(const float c[3], float radius, const float rgba[4], int rings, int sectors) {
    if (MeshRenderer3D* r = renderer()) r->batch_wire_sphere(v3(c), radius, v4(rgba), rings, sectors);
}

void host_line_batch_end() { if (MeshRenderer3D* r = renderer()) r->end_line_batch(); }

} // namespace

const CrayonHostAPI* plugin_host_api() {
    static CrayonHostAPI api = [] {
        CrayonHostAPI a{};
        a.struct_size   = sizeof(CrayonHostAPI);
        a.api_version   = CRAYON_PLUGIN_API_VERSION;
        a.engine_version = "0.0.2";
        a.log           = host_log;
        a.get_lua_state = host_get_lua_state;
        a.get_model_color = host_get_model_color;
        a.set_model_color = host_set_model_color;
        a.draw_triangle_3d = host_draw_triangle_3d;
        a.draw_sphere_3d = host_draw_sphere_3d;
        a.draw_line_3d  = host_draw_line_3d;
        a.line_batch_begin = host_line_batch_begin;
        a.line_batch_add = host_line_batch_add;
        a.line_batch_wire_sphere = host_line_batch_wire_sphere;
        a.line_batch_end = host_line_batch_end;
        return a;
    }();
    return &api;
}

} // namespace crayon
