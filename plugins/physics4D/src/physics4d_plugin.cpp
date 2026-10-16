// physics4D - Crayon Engine plugin: 4D rigid-body physics (hv4d) + the `crayon.physics4D` Lua API.
//
//   function crayon.config(t) t.plugins.physics4D = true end
//
// Everything the engine offers is reached through CrayonHostAPI (plugins/include/crayon_plugin.h).

#include <lua.hpp>

#include <exception>
#include <memory>
#include <string>

#include "crayon_plugin.h"
#include "physics4d_system.hpp"
#include "plugin_state.hpp"

namespace crayon {
Physics4DSystem*     g_physics4d = nullptr;
const CrayonHostAPI* g_host      = nullptr;
}

namespace {

using namespace crayon;

std::unique_ptr<Physics4DSystem> s_system;
float s_accumulator = 0.0f;

void host_log(int level, const std::string& msg) {
    if (CRAYON_HOST_HAS(g_host, log)) g_host->log(level, ("[physics4D] " + msg).c_str());
}

int traceback_handler(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    if (!msg) msg = "Unknown Lua error";
    lua_getglobal(L, "debug");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "traceback");
        if (lua_isfunction(L, -1)) {
            lua_pushstring(L, msg);
            lua_pushinteger(L, 2);
            lua_call(L, 2, 1);
            return 1;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    lua_pushstring(L, msg);
    return 1;
}

// Calls crayon.<name>(...) if the game defined it.  `push` pushes the nargs arguments.
template <class PushArgs>
void call_crayon_callback(lua_State* L, const char* name, int nargs, PushArgs push) {
    lua_pushcfunction(L, traceback_handler);
    const int handler = lua_gettop(L);
    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) { lua_settop(L, handler - 1); return; }
    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1)) { lua_settop(L, handler - 1); return; }
    lua_remove(L, -2);   // drop the crayon table, keep the function
    push();
    if (lua_pcall(L, nargs, 0, handler) != 0) {
        const char* err = lua_tostring(L, -1);
        host_log(CRAYON_LOG_LEVEL_ERROR, std::string("Error in crayon.") + name + "():\n" + (err ? err : "unknown error"));
    }
    lua_settop(L, handler - 1);
}

// ---- plugin callbacks ------------------------------------------------------------------

int p_on_load(const CrayonHostAPI* host) {
    g_host = host;
    return 0;
}

int p_on_register(lua_State* L) {
    // Called for every (re)created Lua state: start from an empty world.
    s_system = std::make_unique<Physics4DSystem>();
    if (!s_system->init()) {
        host_log(CRAYON_LOG_LEVEL_ERROR, "Physics4DSystem failed to initialize");
        s_system.reset();
        g_physics4d = nullptr;
        return 1;
    }
    g_physics4d = s_system.get();
    s_accumulator = 0.0f;
    register_physics4d_bindings(L);
    return 0;
}

void p_on_update(lua_State* L, float dt) {
    if (!s_system) return;

    const float fixed_dt = 1.0f / 60.0f;
    s_accumulator += dt;
    if (s_accumulator > 0.2f) s_accumulator = 0.2f;
    while (s_accumulator >= fixed_dt) {
        s_system->update(fixed_dt);
        s_accumulator -= fixed_dt;
    }

    // Always drain the queue so it can't grow without bound.
    const auto events = s_system->get_and_clear_events();
    for (const Physics4DEvent& e : events) {
        switch (e.type) {
            case Physics4DEventType::CollisionEnter:
                call_crayon_callback(L, "onCollision4DEnter", 7, [&] {
                    lua_pushinteger(L, e.bodyA);
                    lua_pushinteger(L, e.bodyB);
                    lua_pushnumber(L, e.normal.x);
                    lua_pushnumber(L, e.normal.y);
                    lua_pushnumber(L, e.normal.z);
                    lua_pushnumber(L, e.normal.w);
                    lua_pushnumber(L, e.impulse);
                });
                break;
            case Physics4DEventType::CollisionExit:
                call_crayon_callback(L, "onCollision4DExit", 2, [&] {
                    lua_pushinteger(L, e.bodyA);
                    lua_pushinteger(L, e.bodyB);
                });
                break;
            case Physics4DEventType::TriggerEnter:
                call_crayon_callback(L, "onTrigger4DEnter", 2, [&] {
                    lua_pushinteger(L, e.bodyA);
                    lua_pushinteger(L, e.bodyB);
                });
                break;
            case Physics4DEventType::TriggerExit:
                call_crayon_callback(L, "onTrigger4DExit", 2, [&] {
                    lua_pushinteger(L, e.bodyA);
                    lua_pushinteger(L, e.bodyB);
                });
                break;
        }
    }
}

void p_on_unload(void) {
    if (s_system) s_system->shutdown();
    s_system.reset();
    g_physics4d = nullptr;
    g_host = nullptr;
}

const CrayonPlugin k_plugin = {
    sizeof(CrayonPlugin),
    CRAYON_PLUGIN_API_VERSION,
    "physics4D",
    "1.0.0",
    "4D rigid-body physics (hv4d) with crayon.physics4D.* Lua API",
    p_on_load,
    p_on_register,
    p_on_update,
    p_on_unload,
};

} // namespace

extern "C" CRAYON_PLUGIN_EXPORT const CrayonPlugin* crayon_plugin_entry(void) {
    return &k_plugin;
}
