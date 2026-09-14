#include "lua_runtime.hpp"
#include "../core/engine.hpp"

namespace crayon {

// ============================================================================
// crayon.key Bindings
// ============================================================================

static int l_key_is_down(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* key = luaL_checkstring(L, i);
        if (Engine::get().get_input().is_key_down(key)) {
            lua_pushboolean(L, true);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

static int l_key_is_pressed(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* key = luaL_checkstring(L, i);
        if (Engine::get().get_input().is_key_pressed(key)) {
            lua_pushboolean(L, true);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

static int l_key_is_released(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* key = luaL_checkstring(L, i);
        if (Engine::get().get_input().is_key_released(key)) {
            lua_pushboolean(L, true);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

static int l_key_is_scancode_down(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* scancode = luaL_checkstring(L, i);
        if (Engine::get().get_input().is_scancode_down(scancode)) {
            lua_pushboolean(L, true);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

static int l_key_any_pressed(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().any_key_pressed());
    return 1;
}

static int l_key_any_down(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().any_key_down());
    return 1;
}

static int l_key_get_pressed_keys(lua_State* L) {
    const auto& keys = Engine::get().get_input().get_pressed_keys();
    lua_createtable(L, static_cast<int>(keys.size()), 0);
    int idx = 1;
    for (const auto& k : keys) {
        lua_pushstring(L, k.c_str());
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

static int l_key_get_down_keys(lua_State* L) {
    const auto& keys = Engine::get().get_input().get_down_keys();
    lua_createtable(L, static_cast<int>(keys.size()), 0);
    int idx = 1;
    for (const auto& k : keys) {
        lua_pushstring(L, k.c_str());
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

static int l_key_is_shift_down(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_shift_down());
    return 1;
}

static int l_key_is_ctrl_down(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_ctrl_down());
    return 1;
}

static int l_key_is_alt_down(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_alt_down());
    return 1;
}

static int l_key_is_gui_down(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_gui_down());
    return 1;
}

static int l_key_is_caps_lock(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_caps_lock());
    return 1;
}

static int l_key_set_text_input(lua_State* L) {
    bool enable = lua_isboolean(L, 1) ? lua_toboolean(L, 1) : true;
    if (enable) {
        Engine::get().get_input().start_text_input(Engine::get().get_window());
    } else {
        Engine::get().get_input().stop_text_input(Engine::get().get_window());
    }
    return 0;
}

static int l_key_has_text_input(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_text_input_active(Engine::get().get_window()));
    return 1;
}

static int l_key_get_text_input(lua_State* L) {
    lua_pushstring(L, Engine::get().get_input().get_text_input().c_str());
    return 1;
}

static int l_key_get_clipboard(lua_State* L) {
    lua_pushstring(L, Engine::get().get_input().get_clipboard_text().c_str());
    return 1;
}

static int l_key_set_clipboard(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    Engine::get().get_input().set_clipboard_text(text);
    return 0;
}

// ============================================================================
// crayon.mouse Bindings
// ============================================================================

static int l_mouse_is_down(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        bool down = false;
        if (lua_isnumber(L, i)) {
            down = Engine::get().get_input().is_mouse_down(static_cast<int>(lua_tointeger(L, i)));
        } else if (lua_isstring(L, i)) {
            down = Engine::get().get_input().is_mouse_down(lua_tostring(L, i));
        }
        if (down) {
            lua_pushboolean(L, true);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

static int l_mouse_is_pressed(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        bool pressed = false;
        if (lua_isnumber(L, i)) {
            pressed = Engine::get().get_input().is_mouse_pressed(static_cast<int>(lua_tointeger(L, i)));
        } else if (lua_isstring(L, i)) {
            pressed = Engine::get().get_input().is_mouse_pressed(lua_tostring(L, i));
        }
        if (pressed) {
            lua_pushboolean(L, true);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

static int l_mouse_is_released(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        bool released = false;
        if (lua_isnumber(L, i)) {
            released = Engine::get().get_input().is_mouse_released(static_cast<int>(lua_tointeger(L, i)));
        } else if (lua_isstring(L, i)) {
            released = Engine::get().get_input().is_mouse_released(lua_tostring(L, i));
        }
        if (released) {
            lua_pushboolean(L, true);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

static int l_mouse_get_position(lua_State* L) {
    float x = 0.0f, y = 0.0f;
    Engine::get().get_input().get_mouse_pos(x, y);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

static int l_mouse_get_x(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_input().get_mouse_x());
    return 1;
}

static int l_mouse_get_y(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_input().get_mouse_y());
    return 1;
}

static int l_mouse_get_delta(lua_State* L) {
    float dx = 0.0f, dy = 0.0f;
    Engine::get().get_input().get_mouse_delta(dx, dy);
    lua_pushnumber(L, dx);
    lua_pushnumber(L, dy);
    return 2;
}

static int l_mouse_get_delta_x(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_input().get_mouse_delta_x());
    return 1;
}

static int l_mouse_get_delta_y(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_input().get_mouse_delta_y());
    return 1;
}

static int l_mouse_get_window_position(lua_State* L) {
    float x = 0.0f, y = 0.0f;
    Engine::get().get_input().get_mouse_window_pos(x, y);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

static int l_mouse_get_window_x(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_input().get_mouse_window_x());
    return 1;
}

static int l_mouse_get_window_y(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_input().get_mouse_window_y());
    return 1;
}

static int l_mouse_get_window_delta(lua_State* L) {
    float dx = 0.0f, dy = 0.0f;
    Engine::get().get_input().get_mouse_window_delta(dx, dy);
    lua_pushnumber(L, dx);
    lua_pushnumber(L, dy);
    return 2;
}

static int l_mouse_get_wheel(lua_State* L) {
    float wx = Engine::get().get_input().get_mouse_wheel_x();
    float wy = Engine::get().get_input().get_mouse_wheel_y();
    lua_pushnumber(L, wx);
    lua_pushnumber(L, wy);
    return 2;
}

static int l_mouse_get_wheel_x(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_input().get_mouse_wheel_x());
    return 1;
}

static int l_mouse_get_wheel_y(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_input().get_mouse_wheel_y());
    return 1;
}

static int l_mouse_set_position(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    Engine::get().get_input().set_mouse_position(Engine::get().get_window(), x, y);
    return 0;
}

static int l_mouse_set_window_position(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    Engine::get().get_input().set_mouse_window_position(Engine::get().get_window(), x, y);
    return 0;
}

static int l_mouse_set_visible(lua_State* L) {
    bool visible = lua_isboolean(L, 1) ? lua_toboolean(L, 1) : true;
    Engine::get().get_window().show_cursor(visible);
    return 0;
}

static int l_mouse_is_visible(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_cursor_visible());
    return 1;
}

static int l_mouse_set_grabbed(lua_State* L) {
    bool grab = lua_toboolean(L, 1);
    Engine::get().get_window().set_mouse_grab(grab);
    return 0;
}

static int l_mouse_is_grabbed(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_mouse_grabbed());
    return 1;
}

static int l_mouse_set_relative_mode(lua_State* L) {
    bool rel = lua_toboolean(L, 1);
    Engine::get().get_window().set_mouse_relative(rel);
    return 0;
}

static int l_mouse_is_relative_mode(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_mouse_relative());
    return 1;
}

// ============================================================================
// crayon.gamepad Bindings
// ============================================================================

static int l_gamepad_is_down(lua_State* L) {
    bool down = false;
    if (lua_isnumber(L, 1)) {
        down = Engine::get().get_input().gamepad_is_down(static_cast<int>(lua_tointeger(L, 1)));
    } else if (lua_isstring(L, 1)) {
        down = Engine::get().get_input().gamepad_is_down(lua_tostring(L, 1));
    }
    lua_pushboolean(L, down);
    return 1;
}

static int l_gamepad_is_pressed(lua_State* L) {
    bool pressed = false;
    if (lua_isnumber(L, 1)) {
        pressed = Engine::get().get_input().gamepad_is_pressed(static_cast<int>(lua_tointeger(L, 1)));
    } else if (lua_isstring(L, 1)) {
        pressed = Engine::get().get_input().gamepad_is_pressed(lua_tostring(L, 1));
    }
    lua_pushboolean(L, pressed);
    return 1;
}

static int l_gamepad_is_released(lua_State* L) {
    bool rel = false;
    if (lua_isnumber(L, 1)) {
        rel = Engine::get().get_input().gamepad_is_released(static_cast<int>(lua_tointeger(L, 1)));
    } else if (lua_isstring(L, 1)) {
        rel = Engine::get().get_input().gamepad_is_released(lua_tostring(L, 1));
    }
    lua_pushboolean(L, rel);
    return 1;
}

static int l_gamepad_get_axis(lua_State* L) {
    float val = 0.0f;
    if (lua_isnumber(L, 1)) {
        val = Engine::get().get_input().gamepad_axis(static_cast<int>(lua_tointeger(L, 1)));
    } else if (lua_isstring(L, 1)) {
        val = Engine::get().get_input().gamepad_axis(lua_tostring(L, 1));
    }
    lua_pushnumber(L, val);
    return 1;
}

static int l_gamepad_is_connected(lua_State* L) {
    int idx = static_cast<int>(luaL_optinteger(L, 1, 0));
    lua_pushboolean(L, Engine::get().get_input().gamepad_is_connected(idx));
    return 1;
}

static int l_gamepad_get_name(lua_State* L) {
    int idx = static_cast<int>(luaL_optinteger(L, 1, 0));
    lua_pushstring(L, Engine::get().get_input().gamepad_get_name(idx).c_str());
    return 1;
}

static int l_gamepad_get_count(lua_State* L) {
    lua_pushinteger(L, Engine::get().get_input().gamepad_get_count());
    return 1;
}

// ============================================================================
// Registration
// ============================================================================

void register_input_bindings(lua_State* L) {
    lua_getglobal(L, "crayon");

    // 1. crayon.key
    lua_newtable(L);
    lua_pushcfunction(L, l_key_is_down);
    lua_setfield(L, -2, "isDown");
    lua_pushcfunction(L, l_key_is_pressed);
    lua_setfield(L, -2, "isPressed");
    lua_pushcfunction(L, l_key_is_released);
    lua_setfield(L, -2, "isReleased");
    lua_pushcfunction(L, l_key_is_scancode_down);
    lua_setfield(L, -2, "isScancodeDown");
    lua_pushcfunction(L, l_key_any_pressed);
    lua_setfield(L, -2, "anyPressed");
    lua_pushcfunction(L, l_key_any_down);
    lua_setfield(L, -2, "anyDown");
    lua_pushcfunction(L, l_key_get_pressed_keys);
    lua_setfield(L, -2, "getPressedKeys");
    lua_pushcfunction(L, l_key_get_down_keys);
    lua_setfield(L, -2, "getDownKeys");
    lua_pushcfunction(L, l_key_is_shift_down);
    lua_setfield(L, -2, "isShiftDown");
    lua_pushcfunction(L, l_key_is_ctrl_down);
    lua_setfield(L, -2, "isCtrlDown");
    lua_pushcfunction(L, l_key_is_alt_down);
    lua_setfield(L, -2, "isAltDown");
    lua_pushcfunction(L, l_key_is_gui_down);
    lua_setfield(L, -2, "isGuiDown");
    lua_pushcfunction(L, l_key_is_caps_lock);
    lua_setfield(L, -2, "isCapsLock");
    lua_pushcfunction(L, l_key_set_text_input);
    lua_setfield(L, -2, "setTextInput");
    lua_pushcfunction(L, l_key_has_text_input);
    lua_setfield(L, -2, "hasTextInput");
    lua_pushcfunction(L, l_key_get_text_input);
    lua_setfield(L, -2, "getTextInput");
    lua_pushcfunction(L, l_key_get_clipboard);
    lua_setfield(L, -2, "getClipboard");
    lua_pushcfunction(L, l_key_set_clipboard);
    lua_setfield(L, -2, "setClipboard");
    lua_setfield(L, -2, "key");

    // 2. crayon.mouse
    lua_newtable(L);
    lua_pushcfunction(L, l_mouse_is_down);
    lua_setfield(L, -2, "isDown");
    lua_pushcfunction(L, l_mouse_is_pressed);
    lua_setfield(L, -2, "isPressed");
    lua_pushcfunction(L, l_mouse_is_released);
    lua_setfield(L, -2, "isReleased");
    lua_pushcfunction(L, l_mouse_get_position);
    lua_setfield(L, -2, "getPosition");
    lua_pushcfunction(L, l_mouse_get_x);
    lua_setfield(L, -2, "getX");
    lua_pushcfunction(L, l_mouse_get_y);
    lua_setfield(L, -2, "getY");
    lua_pushcfunction(L, l_mouse_get_delta);
    lua_setfield(L, -2, "getDelta");
    lua_pushcfunction(L, l_mouse_get_delta_x);
    lua_setfield(L, -2, "getDeltaX");
    lua_pushcfunction(L, l_mouse_get_delta_y);
    lua_setfield(L, -2, "getDeltaY");
    lua_pushcfunction(L, l_mouse_get_window_position);
    lua_setfield(L, -2, "getWindowPosition");
    lua_pushcfunction(L, l_mouse_get_window_x);
    lua_setfield(L, -2, "getWindowX");
    lua_pushcfunction(L, l_mouse_get_window_y);
    lua_setfield(L, -2, "getWindowY");
    lua_pushcfunction(L, l_mouse_get_window_delta);
    lua_setfield(L, -2, "getWindowDelta");
    lua_pushcfunction(L, l_mouse_get_wheel);
    lua_setfield(L, -2, "getWheel");
    lua_pushcfunction(L, l_mouse_get_wheel_x);
    lua_setfield(L, -2, "getWheelX");
    lua_pushcfunction(L, l_mouse_get_wheel_y);
    lua_setfield(L, -2, "getWheelY");
    lua_pushcfunction(L, l_mouse_set_position);
    lua_setfield(L, -2, "setPosition");
    lua_pushcfunction(L, l_mouse_set_window_position);
    lua_setfield(L, -2, "setWindowPosition");
    lua_pushcfunction(L, l_mouse_set_visible);
    lua_setfield(L, -2, "setVisible");
    lua_pushcfunction(L, l_mouse_is_visible);
    lua_setfield(L, -2, "isVisible");
    lua_pushcfunction(L, l_mouse_set_grabbed);
    lua_setfield(L, -2, "setGrabbed");
    lua_pushcfunction(L, l_mouse_is_grabbed);
    lua_setfield(L, -2, "isGrabbed");
    lua_pushcfunction(L, l_mouse_set_relative_mode);
    lua_setfield(L, -2, "setRelativeMode");
    lua_pushcfunction(L, l_mouse_is_relative_mode);
    lua_setfield(L, -2, "isRelativeMode");
    lua_setfield(L, -2, "mouse");

    // 3. crayon.gamepad
    lua_newtable(L);
    lua_pushcfunction(L, l_gamepad_is_down);
    lua_setfield(L, -2, "isDown");
    lua_pushcfunction(L, l_gamepad_is_pressed);
    lua_setfield(L, -2, "isPressed");
    lua_pushcfunction(L, l_gamepad_is_released);
    lua_setfield(L, -2, "isReleased");
    lua_pushcfunction(L, l_gamepad_get_axis);
    lua_setfield(L, -2, "getAxis");
    lua_pushcfunction(L, l_gamepad_is_connected);
    lua_setfield(L, -2, "isConnected");
    lua_pushcfunction(L, l_gamepad_get_name);
    lua_setfield(L, -2, "getName");
    lua_pushcfunction(L, l_gamepad_get_count);
    lua_setfield(L, -2, "getCount");
    lua_setfield(L, -2, "gamepad");

    lua_pop(L, 1); // pop crayon
}

} // namespace crayon
