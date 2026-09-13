#include "lua_runtime.hpp"
#include "../core/engine.hpp"

namespace crayon {

static int l_input_is_down(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    bool down = Engine::get().get_input().is_key_down(key);
    lua_pushboolean(L, down);
    return 1;
}

static int l_input_is_pressed(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    bool pressed = Engine::get().get_input().is_key_pressed(key);
    lua_pushboolean(L, pressed);
    return 1;
}

static int l_input_is_released(lua_State* L) {
    const char* key = luaL_checkstring(L, 1);
    bool released = Engine::get().get_input().is_key_released(key);
    lua_pushboolean(L, released);
    return 1;
}

static int l_input_get_mouse_pos(lua_State* L) {
    float x = 0.0f, y = 0.0f;
    Engine::get().get_input().get_mouse_pos(x, y);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

static int l_input_get_mouse_delta(lua_State* L) {
    float dx = 0.0f, dy = 0.0f;
    Engine::get().get_input().get_mouse_delta(dx, dy);
    lua_pushnumber(L, dx);
    lua_pushnumber(L, dy);
    return 2;
}

static int l_input_get_mouse_window_pos(lua_State* L) {
    float x = 0.0f, y = 0.0f;
    Engine::get().get_input().get_mouse_window_pos(x, y);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

static int l_input_is_mouse_down(lua_State* L) {
    bool down = false;
    if (lua_isnumber(L, 1)) {
        int btn = static_cast<int>(lua_tointeger(L, 1));
        down = Engine::get().get_input().is_mouse_down(btn);
    } else if (lua_isstring(L, 1)) {
        down = Engine::get().get_input().is_mouse_down(lua_tostring(L, 1));
    }
    lua_pushboolean(L, down);
    return 1;
}

static int l_input_is_mouse_pressed(lua_State* L) {
    bool pressed = false;
    if (lua_isnumber(L, 1)) {
        int btn = static_cast<int>(lua_tointeger(L, 1));
        pressed = Engine::get().get_input().is_mouse_pressed(btn);
    } else if (lua_isstring(L, 1)) {
        pressed = Engine::get().get_input().is_mouse_pressed(lua_tostring(L, 1));
    }
    lua_pushboolean(L, pressed);
    return 1;
}

static int l_input_is_mouse_released(lua_State* L) {
    bool rel = false;
    if (lua_isnumber(L, 1)) {
        int btn = static_cast<int>(lua_tointeger(L, 1));
        rel = Engine::get().get_input().is_mouse_released(btn);
    } else if (lua_isstring(L, 1)) {
        rel = Engine::get().get_input().is_mouse_released(lua_tostring(L, 1));
    }
    lua_pushboolean(L, rel);
    return 1;
}

static int l_input_get_mouse_wheel(lua_State* L) {
    float wx = Engine::get().get_input().get_mouse_wheel_x();
    float wy = Engine::get().get_input().get_mouse_wheel_y();
    lua_pushnumber(L, wx);
    lua_pushnumber(L, wy);
    return 2;
}

static int l_input_set_mouse_position(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    Engine::get().get_input().set_mouse_position(Engine::get().get_window(), x, y);
    return 0;
}

static int l_input_any_key_pressed(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().any_key_pressed());
    return 1;
}

static int l_input_get_pressed_keys(lua_State* L) {
    const auto& keys = Engine::get().get_input().get_pressed_keys();
    lua_createtable(L, static_cast<int>(keys.size()), 0);
    int idx = 1;
    for (const auto& k : keys) {
        lua_pushstring(L, k.c_str());
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

static int l_input_is_shift_down(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_shift_down());
    return 1;
}

static int l_input_is_ctrl_down(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_ctrl_down());
    return 1;
}

static int l_input_is_alt_down(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_alt_down());
    return 1;
}

static int l_input_is_gui_down(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_gui_down());
    return 1;
}

static int l_input_is_caps_lock(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_caps_lock());
    return 1;
}

static int l_input_start_text_input(lua_State* L) {
    (void)L;
    Engine::get().get_input().start_text_input(Engine::get().get_window());
    return 0;
}

static int l_input_stop_text_input(lua_State* L) {
    (void)L;
    Engine::get().get_input().stop_text_input(Engine::get().get_window());
    return 0;
}

static int l_input_is_text_input_active(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_input().is_text_input_active(Engine::get().get_window()));
    return 1;
}

static int l_input_get_text_input(lua_State* L) {
    lua_pushstring(L, Engine::get().get_input().get_text_input().c_str());
    return 1;
}

static int l_input_get_clipboard(lua_State* L) {
    lua_pushstring(L, Engine::get().get_input().get_clipboard_text().c_str());
    return 1;
}

static int l_input_set_clipboard(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    Engine::get().get_input().set_clipboard_text(text);
    return 0;
}

static int l_input_gamepad_is_down(lua_State* L) {
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    bool down = Engine::get().get_input().gamepad_is_down(btn);
    lua_pushboolean(L, down);
    return 1;
}

static int l_input_gamepad_axis(lua_State* L) {
    int axis = static_cast<int>(luaL_checkinteger(L, 1));
    float val = Engine::get().get_input().gamepad_axis(axis);
    lua_pushnumber(L, val);
    return 1;
}

void register_input_bindings(lua_State* L) {
    lua_getglobal(L, "crayon");
    lua_newtable(L);

    lua_pushcfunction(L, l_input_is_down);
    lua_setfield(L, -2, "isDown");

    lua_pushcfunction(L, l_input_is_pressed);
    lua_setfield(L, -2, "isPressed");

    lua_pushcfunction(L, l_input_is_released);
    lua_setfield(L, -2, "isReleased");

    lua_pushcfunction(L, l_input_any_key_pressed);
    lua_setfield(L, -2, "anyKeyPressed");

    lua_pushcfunction(L, l_input_get_pressed_keys);
    lua_setfield(L, -2, "getPressedKeys");

    // Key Modifiers
    lua_pushcfunction(L, l_input_is_shift_down);
    lua_setfield(L, -2, "isShiftDown");

    lua_pushcfunction(L, l_input_is_ctrl_down);
    lua_setfield(L, -2, "isCtrlDown");

    lua_pushcfunction(L, l_input_is_alt_down);
    lua_setfield(L, -2, "isAltDown");

    lua_pushcfunction(L, l_input_is_gui_down);
    lua_setfield(L, -2, "isGuiDown");

    lua_pushcfunction(L, l_input_is_caps_lock);
    lua_setfield(L, -2, "isCapsLock");
    // Mouse
    lua_pushcfunction(L, l_input_get_mouse_pos);
    lua_setfield(L, -2, "getMousePos");

    lua_pushcfunction(L, l_input_get_mouse_window_pos);
    lua_setfield(L, -2, "getMouseWindowPos");

    lua_pushcfunction(L, l_input_get_mouse_delta);
    lua_setfield(L, -2, "getMouseDelta");

    lua_pushcfunction(L, l_input_is_mouse_down);
    lua_setfield(L, -2, "isMouseDown");

    lua_pushcfunction(L, l_input_is_mouse_pressed);
    lua_setfield(L, -2, "isMousePressed");

    lua_pushcfunction(L, l_input_is_mouse_released);
    lua_setfield(L, -2, "isMouseReleased");

    lua_pushcfunction(L, l_input_get_mouse_wheel);
    lua_setfield(L, -2, "getMouseWheel");

    lua_pushcfunction(L, l_input_set_mouse_position);
    lua_setfield(L, -2, "setMousePosition");

    // Text Input & Clipboard
    lua_pushcfunction(L, l_input_start_text_input);
    lua_setfield(L, -2, "startTextInput");

    lua_pushcfunction(L, l_input_stop_text_input);
    lua_setfield(L, -2, "stopTextInput");
    
    lua_pushcfunction(L, l_input_is_text_input_active);
    lua_setfield(L, -2, "isTextInputActive");

    lua_pushcfunction(L, l_input_get_text_input);
    lua_setfield(L, -2, "getTextInput");

    lua_pushcfunction(L, l_input_get_clipboard);
    lua_setfield(L, -2, "getClipboard");

    lua_pushcfunction(L, l_input_set_clipboard);
    lua_setfield(L, -2, "setClipboard");
    
    // Gamepad
    lua_pushcfunction(L, l_input_gamepad_is_down);
    lua_setfield(L, -2, "gamepadIsDown");

    lua_pushcfunction(L, l_input_gamepad_axis);
    lua_setfield(L, -2, "gamepadAxis");

    lua_setfield(L, -2, "input");
    lua_pop(L, 1);
}

} // namespace crayon
