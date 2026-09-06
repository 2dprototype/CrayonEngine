#include "lua_runtime.hpp"
#include "../core/log.hpp"

namespace crayon {

static int lua_error_handler(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    if (!msg) msg = "Unknown Lua error";

    lua_getglobal(L, "debug");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "traceback");
        if (lua_isfunction(L, -1)) {
            lua_pushstring(L, msg);
            lua_pushinteger(L, 2); // Skip error handler frame
            lua_call(L, 2, 1);
            return 1;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    lua_pushstring(L, msg);
    return 1;
}

LuaRuntime::LuaRuntime() = default;

LuaRuntime::~LuaRuntime() {
    shutdown();
}

bool LuaRuntime::init() {
    m_L = luaL_newstate();
    if (!m_L) {
        CRAYON_LOG_ERROR("Failed to create Lua state");
        return false;
    }

    luaL_openlibs(m_L);

    // Create global table 'crayon'
    lua_newtable(m_L);
    lua_setglobal(m_L, "crayon");

    register_modules();

    m_initialized = true;
    CRAYON_LOG_INFO("LuaRuntime initialized (LuaJIT)");
    return true;
}

void LuaRuntime::shutdown() {
    if (m_L) {
        lua_close(m_L);
        m_L = nullptr;
    }
    m_initialized = false;
}

void LuaRuntime::register_modules() {
    register_window_bindings(m_L);
    register_graphics_bindings(m_L);
    register_input_bindings(m_L);
    register_time_bindings(m_L);
}

int LuaRuntime::push_error_handler() {
    lua_pushcfunction(m_L, lua_error_handler);
    return lua_gettop(m_L);
}

bool LuaRuntime::load_script(const std::string& filepath) {
    if (!m_L) return false;

    int err_func = push_error_handler();

    if (luaL_loadfile(m_L, filepath.c_str()) != 0) {
        const char* err = lua_tostring(m_L, -1);
        CRAYON_LOG_ERROR("Failed to parse script '{}':\n{}", filepath, err ? err : "unknown error");
        lua_pop(m_L, 2); // pop error and handler
        return false;
    }

    if (lua_pcall(m_L, 0, 0, err_func) != 0) {
        const char* err = lua_tostring(m_L, -1);
        CRAYON_LOG_ERROR("Error executing script '{}':\n{}", filepath, err ? err : "unknown error");
        lua_pop(m_L, 2); // pop error and handler
        return false;
    }

    lua_pop(m_L, 1); // pop error handler

    call_init();
    return true;
}

bool LuaRuntime::reload_script(const std::string& filepath) {
    CRAYON_LOG_INFO("Reloading script: {}", filepath);
    return load_script(filepath);
}

void LuaRuntime::call_init() {
    if (!m_L) return;

    int err_func = push_error_handler();

    lua_getglobal(m_L, "crayon");
    if (lua_istable(m_L, -1)) {
        lua_getfield(m_L, -1, "init");
        if (lua_isfunction(m_L, -1)) {
            if (lua_pcall(m_L, 0, 0, err_func) != 0) {
                const char* err = lua_tostring(m_L, -1);
                CRAYON_LOG_ERROR("Error in crayon.init():\n{}", err ? err : "unknown error");
                lua_pop(m_L, 1);
            }
        } else {
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1); // pop crayon table
    lua_pop(m_L, 1); // pop err_func
}

void LuaRuntime::call_update(float dt) {
    if (!m_L) return;

    int err_func = push_error_handler();

    lua_getglobal(m_L, "crayon");
    if (lua_istable(m_L, -1)) {
        lua_getfield(m_L, -1, "update");
        if (lua_isfunction(m_L, -1)) {
            lua_pushnumber(m_L, dt);
            if (lua_pcall(m_L, 1, 0, err_func) != 0) {
                const char* err = lua_tostring(m_L, -1);
                CRAYON_LOG_ERROR("Error in crayon.update():\n{}", err ? err : "unknown error");
                lua_pop(m_L, 1);
            }
        } else {
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
    lua_pop(m_L, 1);
}

void LuaRuntime::call_draw() {
    if (!m_L) return;

    int err_func = push_error_handler();

    lua_getglobal(m_L, "crayon");
    if (lua_istable(m_L, -1)) {
        lua_getfield(m_L, -1, "draw");
        if (lua_isfunction(m_L, -1)) {
            if (lua_pcall(m_L, 0, 0, err_func) != 0) {
                const char* err = lua_tostring(m_L, -1);
                CRAYON_LOG_ERROR("Error in crayon.draw():\n{}", err ? err : "unknown error");
                lua_pop(m_L, 1);
            }
        } else {
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
    lua_pop(m_L, 1);
}

} // namespace crayon
