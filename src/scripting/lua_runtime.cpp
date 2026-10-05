#include "lua_runtime.hpp"
#include "../core/log.hpp"
#include "../core/engine_config.hpp"

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

static int l_disabled_module_error(lua_State* L) {
    const char* mod_name = (const char*)lua_touserdata(L, lua_upvalueindex(1));
    return luaL_error(L, "Module '%s' is disabled in crayon.config(). Set t.modules.%s = true to enable it.", mod_name, mod_name);
}

static void register_disabled_stub(lua_State* L, const char* name) {
    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_newtable(L);      // dummy module table
    lua_newtable(L);      // metatable
    lua_pushstring(L, "__index");
    lua_pushlightuserdata(L, (void*)name);
    lua_pushcclosure(L, l_disabled_module_error, 1);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, name);
    lua_pop(L, 1); // pop crayon
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
    register_modules(ModulesConfig{});
}

void LuaRuntime::register_modules(const ModulesConfig& modules) {
    if (!m_L) return;

    register_window_bindings(m_L);
    register_graphics_bindings(m_L);
    register_time_bindings(m_L);
    register_math_bindings(m_L);
    register_print_bindings(m_L);

    if (modules.input) {
        register_input_bindings(m_L);
    } else {
        register_disabled_stub(m_L, "key");
        register_disabled_stub(m_L, "mouse");
        register_disabled_stub(m_L, "gamepad");
    }

    if (modules.audio) {
        register_audio_bindings(m_L);
    } else {
        register_disabled_stub(m_L, "audio");
    }

    if (modules.physics3d) {
        register_physics3d_bindings(m_L);
    } else {
        register_disabled_stub(m_L, "physics3D");
    }

    if (modules.physics2d) {
        register_physics2d_bindings(m_L);
    } else {
        register_disabled_stub(m_L, "physics2D");
    }

    if (modules.physics4d) {
        register_physics4d_bindings(m_L);
    } else {
        register_disabled_stub(m_L, "physics4D");
    }

    if (modules.particles) {
        register_particle_bindings(m_L);
    } else {
        register_disabled_stub(m_L, "particles");
    }

    if (modules.fs) {
        register_fs_bindings(m_L);
    } else {
        register_disabled_stub(m_L, "fs");
    }
}

bool LuaRuntime::run_config_phase(const std::string& filepath, EngineConfig& config) {
    if (!m_L) return false;

    int err_func = push_error_handler();

    // 1. Create table 't' with strict camelCase keys
    lua_newtable(m_L);

    lua_pushstring(m_L, config.identity.c_str());
    lua_setfield(m_L, -2, "identity");

    lua_pushstring(m_L, config.version.c_str());
    lua_setfield(m_L, -2, "version");

    lua_pushinteger(m_L, config.fps_limit);
    lua_setfield(m_L, -2, "fpsLimit");

    lua_pushboolean(m_L, config.console);
    lua_setfield(m_L, -2, "console");

    // t.window (camelCase)
    lua_newtable(m_L);
    lua_pushstring(m_L, config.window.title.c_str());
    lua_setfield(m_L, -2, "title");
    lua_pushinteger(m_L, config.window.width);
    lua_setfield(m_L, -2, "width");
    lua_pushinteger(m_L, config.window.height);
    lua_setfield(m_L, -2, "height");
    lua_pushinteger(m_L, config.window.virtual_width);
    lua_setfield(m_L, -2, "virtualWidth");
    lua_pushinteger(m_L, config.window.virtual_height);
    lua_setfield(m_L, -2, "virtualHeight");
    lua_pushinteger(m_L, config.window.min_width);
    lua_setfield(m_L, -2, "minWidth");
    lua_pushinteger(m_L, config.window.min_height);
    lua_setfield(m_L, -2, "minHeight");
    lua_pushboolean(m_L, config.window.resizable);
    lua_setfield(m_L, -2, "resizable");
    lua_pushboolean(m_L, config.window.fullscreen);
    lua_setfield(m_L, -2, "fullscreen");
    lua_pushboolean(m_L, config.window.vsync);
    lua_setfield(m_L, -2, "vsync");
    lua_pushboolean(m_L, config.window.transparent);
    lua_setfield(m_L, -2, "transparent");
    lua_pushboolean(m_L, config.window.borderless);
    lua_setfield(m_L, -2, "borderless");
    lua_pushboolean(m_L, config.window.always_on_top);
    lua_setfield(m_L, -2, "alwaysOnTop");
    lua_pushboolean(m_L, config.window.click_through);
    lua_setfield(m_L, -2, "clickThrough");
    lua_pushstring(m_L, config.window.scaling.c_str());
    lua_setfield(m_L, -2, "scaling");
    lua_pushnumber(m_L, config.window.opacity);
    lua_setfield(m_L, -2, "opacity");
    lua_pushboolean(m_L, config.window.skip_taskbar);
    lua_setfield(m_L, -2, "skipTaskbar");
    lua_pushboolean(m_L, config.window.not_focusable);
    lua_setfield(m_L, -2, "notFocusable");
    lua_pushboolean(m_L, config.window.utility_window);
    lua_setfield(m_L, -2, "utilityWindow");
    lua_setfield(m_L, -2, "window");

    // t.modules (camelCase)
    lua_newtable(m_L);
    lua_pushboolean(m_L, config.modules.physics3d);
    lua_setfield(m_L, -2, "physics3D");
    lua_pushboolean(m_L, config.modules.physics2d);
    lua_setfield(m_L, -2, "physics2D");
    lua_pushboolean(m_L, config.modules.physics4d);
    lua_setfield(m_L, -2, "physics4D");
    lua_pushboolean(m_L, config.modules.audio);
    lua_setfield(m_L, -2, "audio");
    lua_pushboolean(m_L, config.modules.mesh3d);
    lua_setfield(m_L, -2, "mesh3D");
    lua_pushboolean(m_L, config.modules.particles);
    lua_setfield(m_L, -2, "particles");
    lua_pushboolean(m_L, config.modules.input);
    lua_setfield(m_L, -2, "input");
    lua_pushboolean(m_L, config.modules.fs);
    lua_setfield(m_L, -2, "fs");
    lua_setfield(m_L, -2, "modules");

    // t.graphics (camelCase)
    lua_newtable(m_L);
    lua_newtable(m_L);
    lua_pushnumber(m_L, config.graphics.clear_color.r);
    lua_rawseti(m_L, -2, 1);
    lua_pushnumber(m_L, config.graphics.clear_color.g);
    lua_rawseti(m_L, -2, 2);
    lua_pushnumber(m_L, config.graphics.clear_color.b);
    lua_rawseti(m_L, -2, 3);
    lua_pushnumber(m_L, config.graphics.clear_color.a);
    lua_rawseti(m_L, -2, 4);
    lua_setfield(m_L, -2, "clearColor");
    lua_pushboolean(m_L, config.graphics.dither);
    lua_setfield(m_L, -2, "dither");
    lua_pushboolean(m_L, config.graphics.crt);
    lua_setfield(m_L, -2, "crt");
    lua_pushboolean(m_L, config.graphics.vignette);
    lua_setfield(m_L, -2, "vignette");
    lua_setfield(m_L, -2, "graphics");

    // 2. Load and parse the script file
    if (luaL_loadfile(m_L, filepath.c_str()) != 0) {
        const char* err = lua_tostring(m_L, -1);
        CRAYON_LOG_ERROR("Failed to parse script for config phase '{}':\n{}", filepath, err ? err : "unknown error");
        lua_pop(m_L, 3); // pop error, table t, err_func
        return false;
    }

    // 3. Execute top-level script chunk
    if (lua_pcall(m_L, 0, 0, err_func) != 0) {
        const char* err = lua_tostring(m_L, -1);
        CRAYON_LOG_ERROR("Error executing script during config phase '{}':\n{}", filepath, err ? err : "unknown error");
        lua_pop(m_L, 2); // pop error, table t
        lua_pop(m_L, 1); // pop err_func
        return false;
    }

    // 4. Check for crayon.config only (no aliases allowed)
    bool has_config = false;
    lua_getglobal(m_L, "crayon");
    if (lua_istable(m_L, -1)) {
        lua_getfield(m_L, -1, "config");
        if (lua_isfunction(m_L, -1)) {
            has_config = true;
            lua_remove(m_L, -2);   // pop crayon, leaving config_func on top
        } else {
            lua_pop(m_L, 2);       // pop non-function "config" and the crayon table
        }
    } else {
        lua_pop(m_L, 1);           // crayon wasn't a table (defensive)
    }

    if (has_config) {
        // Stack: err_func, table t, config_func
        lua_pushvalue(m_L, -2); // push copy of table t as argument
        if (lua_pcall(m_L, 1, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.config():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1); // pop error
        }
    }

    // 5. Read back modified values from table t (at top of stack before err_func)
    lua_getfield(m_L, -1, "identity");
    if (lua_isstring(m_L, -1)) config.identity = lua_tostring(m_L, -1);
    lua_pop(m_L, 1);

    lua_getfield(m_L, -1, "version");
    if (lua_isstring(m_L, -1)) config.version = lua_tostring(m_L, -1);
    lua_pop(m_L, 1);

    lua_getfield(m_L, -1, "fpsLimit");
    if (lua_isnumber(m_L, -1)) config.fps_limit = static_cast<int>(lua_tointeger(m_L, -1));
    lua_pop(m_L, 1);

    lua_getfield(m_L, -1, "console");
    if (lua_isboolean(m_L, -1)) config.console = lua_toboolean(m_L, -1);
    lua_pop(m_L, 1);

    // Read t.window (strict camelCase)
    lua_getfield(m_L, -1, "window");
    if (lua_istable(m_L, -1)) {
        lua_getfield(m_L, -1, "title");
        if (lua_isstring(m_L, -1)) config.window.title = lua_tostring(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "width");
        if (lua_isnumber(m_L, -1)) config.window.width = static_cast<int>(lua_tointeger(m_L, -1));
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "height");
        if (lua_isnumber(m_L, -1)) config.window.height = static_cast<int>(lua_tointeger(m_L, -1));
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "virtualWidth");
        if (lua_isnumber(m_L, -1)) config.window.virtual_width = static_cast<int>(lua_tointeger(m_L, -1));
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "virtualHeight");
        if (lua_isnumber(m_L, -1)) config.window.virtual_height = static_cast<int>(lua_tointeger(m_L, -1));
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "minWidth");
        if (lua_isnumber(m_L, -1)) config.window.min_width = static_cast<int>(lua_tointeger(m_L, -1));
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "minHeight");
        if (lua_isnumber(m_L, -1)) config.window.min_height = static_cast<int>(lua_tointeger(m_L, -1));
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "resizable");
        if (lua_isboolean(m_L, -1)) config.window.resizable = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "fullscreen");
        if (lua_isboolean(m_L, -1)) config.window.fullscreen = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "vsync");
        if (lua_isboolean(m_L, -1)) config.window.vsync = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "transparent");
        if (lua_isboolean(m_L, -1)) config.window.transparent = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "borderless");
        if (lua_isboolean(m_L, -1)) config.window.borderless = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "alwaysOnTop");
        if (lua_isboolean(m_L, -1)) config.window.always_on_top = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "clickThrough");
        if (lua_isboolean(m_L, -1)) config.window.click_through = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "scaling");
        if (lua_isstring(m_L, -1)) config.window.scaling = lua_tostring(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "opacity");
        if (lua_isnumber(m_L, -1)) {
            float op = static_cast<float>(lua_tonumber(m_L, -1));
            if (op < 0.0f) op = 0.0f;
            if (op > 1.0f) op = 1.0f;
            config.window.opacity = op;
        }
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "skipTaskbar");
        if (lua_isboolean(m_L, -1)) config.window.skip_taskbar = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "notFocusable");
        if (lua_isboolean(m_L, -1)) config.window.not_focusable = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "utilityWindow");
        if (lua_isboolean(m_L, -1)) config.window.utility_window = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);
    }
    lua_pop(m_L, 1); // pop t.window
    
    // Read t.modules (strict camelCase)
    lua_getfield(m_L, -1, "modules");
    if (lua_istable(m_L, -1)) {
        lua_getfield(m_L, -1, "physics3D");
        if (lua_isboolean(m_L, -1)) config.modules.physics3d = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "physics2D");
        if (lua_isboolean(m_L, -1)) config.modules.physics2d = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "physics4D");
        if (lua_isboolean(m_L, -1)) config.modules.physics4d = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "audio");
        if (lua_isboolean(m_L, -1)) config.modules.audio = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "mesh3D");
        if (lua_isboolean(m_L, -1)) config.modules.mesh3d = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "particles");
        if (lua_isboolean(m_L, -1)) config.modules.particles = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "input");
        if (lua_isboolean(m_L, -1)) config.modules.input = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "fs");
        if (lua_isboolean(m_L, -1)) config.modules.fs = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);
    }
    lua_pop(m_L, 1); // pop t.modules

    // Read t.graphics (strict camelCase)
    lua_getfield(m_L, -1, "graphics");
    if (lua_istable(m_L, -1)) {
        lua_getfield(m_L, -1, "clearColor");
        if (lua_istable(m_L, -1)) {
            lua_rawgeti(m_L, -1, 1);
            if (lua_isnumber(m_L, -1)) config.graphics.clear_color.r = static_cast<float>(lua_tonumber(m_L, -1));
            lua_pop(m_L, 1);

            lua_rawgeti(m_L, -1, 2);
            if (lua_isnumber(m_L, -1)) config.graphics.clear_color.g = static_cast<float>(lua_tonumber(m_L, -1));
            lua_pop(m_L, 1);

            lua_rawgeti(m_L, -1, 3);
            if (lua_isnumber(m_L, -1)) config.graphics.clear_color.b = static_cast<float>(lua_tonumber(m_L, -1));
            lua_pop(m_L, 1);

            lua_rawgeti(m_L, -1, 4);
            if (lua_isnumber(m_L, -1)) config.graphics.clear_color.a = static_cast<float>(lua_tonumber(m_L, -1));
            lua_pop(m_L, 1);
        }
        lua_pop(m_L, 1); // pop clearColor

        lua_getfield(m_L, -1, "dither");
        if (lua_isboolean(m_L, -1)) config.graphics.dither = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "crt");
        if (lua_isboolean(m_L, -1)) config.graphics.crt = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);

        lua_getfield(m_L, -1, "vignette");
        if (lua_isboolean(m_L, -1)) config.graphics.vignette = lua_toboolean(m_L, -1);
        lua_pop(m_L, 1);
    }
    lua_pop(m_L, 1); // pop t.graphics

    lua_pop(m_L, 1); // pop table t
    lua_pop(m_L, 1); // pop err_func

    CRAYON_LOG_INFO("Configuration loaded from '{}': Title='{}', Win={}x{}, Virt={}x{}, Physics3D={}, Physics2D={}, Physics4D={}, Audio={}, Mesh3D={}",
        filepath, config.window.title, config.window.width, config.window.height,
        config.window.virtual_width, config.window.virtual_height,
        config.modules.physics3d ? "ON" : "OFF",
        config.modules.physics2d ? "ON" : "OFF",
        config.modules.physics4d ? "ON" : "OFF",
        config.modules.audio ? "ON" : "OFF",
        config.modules.mesh3d ? "ON" : "OFF");

    return true;
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
    if (get_crayon_func(m_L, "keydown")) {
        lua_pushstring(m_L, key.c_str());
        lua_pushboolean(m_L, is_repeat);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.keydown():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_key_up(const std::string& key) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "keyup")) {
        lua_pushstring(m_L, key.c_str());
        if (lua_pcall(m_L, 1, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.keyup():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_mouse_down(float x, float y, int button) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "mousedown")) {
        lua_pushnumber(m_L, x);
        lua_pushnumber(m_L, y);
        lua_pushinteger(m_L, button);
        if (lua_pcall(m_L, 3, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.mousedown():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_mouse_up(float x, float y, int button) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "mouseup")) {
        lua_pushnumber(m_L, x);
        lua_pushnumber(m_L, y);
        lua_pushinteger(m_L, button);
        if (lua_pcall(m_L, 3, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.mouseup():\n{}", err ? err : "unknown error");
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
    if (get_crayon_func(m_L, "gamepaddown")) {
        lua_pushinteger(m_L, button);
        if (lua_pcall(m_L, 1, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.gamepaddown():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_gamepad_up(int button) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "gamepadup")) {
        lua_pushinteger(m_L, button);
        if (lua_pcall(m_L, 1, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.gamepadup():\n{}", err ? err : "unknown error");
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
    if (get_crayon_func(m_L, "windowResized")) {
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
    if (get_crayon_func(m_L, "focusChanged")) {
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
    if (get_crayon_func(m_L, "onCollisionEnter")) {
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
    if (get_crayon_func(m_L, "onCollisionExit")) {
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
    if (get_crayon_func(m_L, "onTriggerEnter")) {
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
    if (get_crayon_func(m_L, "onTriggerExit")) {
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

void LuaRuntime::call_collision4d_enter(uint32_t body_a, uint32_t body_b, float nx, float ny, float nz, float nw, float impulse) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onCollision4DEnter")) {
        lua_pushinteger(m_L, body_a);
        lua_pushinteger(m_L, body_b);
        lua_pushnumber(m_L, nx);
        lua_pushnumber(m_L, ny);
        lua_pushnumber(m_L, nz);
        lua_pushnumber(m_L, nw);
        lua_pushnumber(m_L, impulse);
        if (lua_pcall(m_L, 7, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onCollision4DEnter():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_collision4d_exit(uint32_t body_a, uint32_t body_b) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onCollision4DExit")) {
        lua_pushinteger(m_L, body_a);
        lua_pushinteger(m_L, body_b);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onCollision4DExit():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_trigger4d_enter(uint32_t sensor_id, uint32_t other_body_id) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onTrigger4DEnter")) {
        lua_pushinteger(m_L, sensor_id);
        lua_pushinteger(m_L, other_body_id);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onTrigger4DEnter():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_trigger4d_exit(uint32_t sensor_id, uint32_t other_body_id) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onTrigger4DExit")) {
        lua_pushinteger(m_L, sensor_id);
        lua_pushinteger(m_L, other_body_id);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onTrigger4DExit():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_collision2d_enter(uint32_t body_a, uint32_t body_b, float nx, float ny, float impulse) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onCollision2DEnter")) {
        lua_pushinteger(m_L, body_a);
        lua_pushinteger(m_L, body_b);
        lua_pushnumber(m_L, nx);
        lua_pushnumber(m_L, ny);
        lua_pushnumber(m_L, impulse);
        if (lua_pcall(m_L, 5, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onCollision2DEnter():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_collision2d_exit(uint32_t body_a, uint32_t body_b) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onCollision2DExit")) {
        lua_pushinteger(m_L, body_a);
        lua_pushinteger(m_L, body_b);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onCollision2DExit():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_trigger2d_enter(uint32_t sensor_id, uint32_t other_body_id) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onTrigger2DEnter")) {
        lua_pushinteger(m_L, sensor_id);
        lua_pushinteger(m_L, other_body_id);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onTrigger2DEnter():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_trigger2d_exit(uint32_t sensor_id, uint32_t other_body_id) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "onTrigger2DExit")) {
        lua_pushinteger(m_L, sensor_id);
        lua_pushinteger(m_L, other_body_id);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.onTrigger2DExit():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_drop_begin(float x, float y) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "dropBegin")) {
        lua_pushnumber(m_L, x);
        lua_pushnumber(m_L, y);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.dropBegin():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_drop_file(const std::string& path, float x, float y) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "dropFile")) {
        lua_pushstring(m_L, path.c_str());
        lua_pushnumber(m_L, x);
        lua_pushnumber(m_L, y);
        if (lua_pcall(m_L, 3, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.dropFile():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_drop_text(const std::string& text, float x, float y) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "dropText")) {
        lua_pushstring(m_L, text.c_str());
        lua_pushnumber(m_L, x);
        lua_pushnumber(m_L, y);
        if (lua_pcall(m_L, 3, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.dropText():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_drop_position(float x, float y) {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "dropPosition")) {
        lua_pushnumber(m_L, x);
        lua_pushnumber(m_L, y);
        if (lua_pcall(m_L, 2, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.dropPosition():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

void LuaRuntime::call_drop_complete() {
    if (!m_L) return;
    int err_func = push_error_handler();
    if (get_crayon_func(m_L, "dropComplete")) {
        if (lua_pcall(m_L, 0, 0, err_func) != 0) {
            const char* err = lua_tostring(m_L, -1);
            CRAYON_LOG_ERROR("Error in crayon.dropComplete():\n{}", err ? err : "unknown error");
            lua_pop(m_L, 1);
        }
    }
    lua_pop(m_L, 1);
}

} // namespace crayon
