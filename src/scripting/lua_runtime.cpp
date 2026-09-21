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
    register_audio_bindings(m_L);
    register_physics3d_bindings(m_L);
    register_particle_bindings(m_L);
    register_math_bindings(m_L);
    register_fs_bindings(m_L);
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

bool LuaRuntime::execute_string(const std::string& code) {
    if (!m_L) return false;

    int err_func = push_error_handler();

    if (luaL_loadstring(m_L, code.c_str()) != 0) {
        const char* err = lua_tostring(m_L, -1);
        CRAYON_LOG_ERROR("Lua compilation error:\n{}", err ? err : "unknown");
        lua_pop(m_L, 2);
        return false;
    }

    if (lua_pcall(m_L, 0, 0, err_func) != 0) {
        const char* err = lua_tostring(m_L, -1);
        CRAYON_LOG_ERROR("Lua runtime error:\n{}", err ? err : "unknown");
        lua_pop(m_L, 2);
        return false;
    }

    lua_pop(m_L, 1);
    return true;
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

static bool get_crayon_func(lua_State* L, const char* name) {
    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    lua_remove(L, -2);
    return true;
}

void LuaRuntime::call_key_down(const std::string& key, bool is_repeat) {
    if (!m_L) return;
    int err_func = push_error_handler();
    const char* func_name = nullptr;
    if (get_crayon_func(m_L, "keydown")) {
        func_name = "crayon.keydown";
    } else if (get_crayon_func(m_L, "keypressed")) {
        func_name = "crayon.keypressed";
    }
    if (func_name) {
        lua_pushstring(m_L, key.c_str());
        lua_pushboolean(m_L, is_repeat);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in {}():\n{}", func_name, err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_key_up(const std::string& key) {
    if (!m_L) return;
    int err_func = push_error_handler();
    const char* func_name = nullptr;
    if (get_crayon_func(m_L, "keyup")) {
        func_name = "crayon.keyup";
    } else if (get_crayon_func(m_L, "keyreleased")) {
        func_name = "crayon.keyreleased";
    }
    if (func_name) {
        lua_pushstring(m_L, key.c_str());
        if (lua_pcall(m_L, 1, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in {}():\n{}", func_name, err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_mouse_down(float x, float y, int button) {
    if (!m_L) return;
    int err_func = push_error_handler();
    const char* func_name = nullptr;
    if (get_crayon_func(m_L, "mousedown")) {
        func_name = "crayon.mousedown";
    } else if (get_crayon_func(m_L, "mousepressed")) {
        func_name = "crayon.mousepressed";
    }
    if (func_name) {
        lua_pushnumber(m_L, x);
        lua_pushnumber(m_L, y);
        lua_pushinteger(m_L, button);
        if (lua_pcall(m_L, 3, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in {}():\n{}", func_name, err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_mouse_up(float x, float y, int button) {
    if (!m_L) return;
    int err_func = push_error_handler();
    const char* func_name = nullptr;
    if (get_crayon_func(m_L, "mouseup")) {
        func_name = "crayon.mouseup";
    } else if (get_crayon_func(m_L, "mousereleased")) {
        func_name = "crayon.mousereleased";
    }
    if (func_name) {
        lua_pushnumber(m_L, x);
        lua_pushnumber(m_L, y);
        lua_pushinteger(m_L, button);
        if (lua_pcall(m_L, 3, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in {}():\n{}", func_name, err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_mouse_moved(float x, float y, float dx, float dy) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "mousemoved")) {
        lua_pushnumber(m_L, x);
        lua_pushnumber(m_L, y);
        lua_pushnumber(m_L, dx);
        lua_pushnumber(m_L, dy);
        if (lua_pcall(m_L, 4, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.mousemoved():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_wheel_moved(float dx, float dy) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "wheelmoved")) {
        lua_pushnumber(m_L, dx);
        lua_pushnumber(m_L, dy);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.wheelmoved():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_text_input(const std::string& text) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "textinput")) {
        lua_pushstring(m_L, text.c_str());
        if (lua_pcall(m_L, 1, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.textinput():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_gamepad_down(int button) {
    if (!m_L) return;
    int err_func = push_error_handler();
    const char* func_name = nullptr;
    if (get_crayon_func(m_L, "gamepaddown")) {
        func_name = "crayon.gamepaddown";
    } else if (get_crayon_func(m_L, "gamepadpressed")) {
        func_name = "crayon.gamepadpressed";
    }
    if (func_name) {
        lua_pushinteger(m_L, button);
        if (lua_pcall(m_L, 1, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in {}():\n{}", func_name, err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_gamepad_up(int button) {
    if (!m_L) return;
    int err_func = push_error_handler();
    const char* func_name = nullptr;
    if (get_crayon_func(m_L, "gamepadup")) {
        func_name = "crayon.gamepadup";
    } else if (get_crayon_func(m_L, "gamepadreleased")) {
        func_name = "crayon.gamepadreleased";
    }
    if (func_name) {
        lua_pushinteger(m_L, button);
        if (lua_pcall(m_L, 1, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in {}():\n{}", func_name, err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_gamepad_axis(int axis, float value) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "gamepadaxis")) {
        lua_pushinteger(m_L, axis);
        lua_pushnumber(m_L, value);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.gamepadaxis():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_window_resized(int w, int h) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "windowResized") || get_crayon_func(m_L, "resize")) {
        lua_pushinteger(m_L, w);
        lua_pushinteger(m_L, h);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.windowResized():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_focus_changed(bool focused) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "focusChanged") || get_crayon_func(m_L, "focus")) {
        lua_pushboolean(m_L, focused);
        if (lua_pcall(m_L, 1, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.focusChanged():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_quit() {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "quit")) {
        if (lua_pcall(m_L, 0, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.quit():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_collision_enter(uint32_t body_a, uint32_t body_b, float nx, float ny, float nz, float impulse) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onCollisionEnter") || get_crayon_func(m_L, "collisionEnter")) {
        lua_pushinteger(m_L, body_a);
        lua_pushinteger(m_L, body_b);
        lua_pushnumber(m_L, nx);
        lua_pushnumber(m_L, ny);
        lua_pushnumber(m_L, nz);
        lua_pushnumber(m_L, impulse);
        if (lua_pcall(m_L, 6, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onCollisionEnter():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_collision_exit(uint32_t body_a, uint32_t body_b) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onCollisionExit") || get_crayon_func(m_L, "collisionExit")) {
        lua_pushinteger(m_L, body_a);
        lua_pushinteger(m_L, body_b);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onCollisionExit():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_trigger_enter(uint32_t sensor_id, uint32_t other_body_id) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onTriggerEnter") || get_crayon_func(m_L, "triggerEnter")) {
        lua_pushinteger(m_L, sensor_id);
        lua_pushinteger(m_L, other_body_id);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onTriggerEnter():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_trigger_exit(uint32_t sensor_id, uint32_t other_body_id) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onTriggerExit") || get_crayon_func(m_L, "triggerExit")) {
        lua_pushinteger(m_L, sensor_id);
        lua_pushinteger(m_L, other_body_id);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onTriggerExit():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

} // namespace crayon
