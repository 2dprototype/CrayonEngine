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

static int l_input_is_mouse_down(lua_State* L) {
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    bool down = Engine::get().get_input().is_mouse_down(btn);
    lua_pushboolean(L, down);
    return 1;
}

static int l_input_is_mouse_pressed(lua_State* L) {
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    bool pressed = Engine::get().get_input().is_mouse_pressed(btn);
    lua_pushboolean(L, pressed);
    return 1;
}

static int l_input_is_mouse_released(lua_State* L) {
    int btn = static_cast<int>(luaL_checkinteger(L, 1));
    bool rel = Engine::get().get_input().is_mouse_released(btn);
    lua_pushboolean(L, rel);
    return 1;
}

static int l_input_get_mouse_wheel(lua_State* L) {
    float wheel = Engine::get().get_input().get_mouse_wheel();
    lua_pushnumber(L, wheel);
    lua_pushnumber(L, wheel);
    return 2;
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
    lua_setfield(L, -2, "is_down");

    lua_pushcfunction(L, l_input_is_pressed);
    lua_setfield(L, -2, "is_pressed");

    lua_pushcfunction(L, l_input_is_released);
    lua_setfield(L, -2, "is_released");

    lua_pushcfunction(L, l_input_get_mouse_pos);
    lua_setfield(L, -2, "get_mouse_pos");

    lua_pushcfunction(L, l_input_get_mouse_delta);
    lua_setfield(L, -2, "get_mouse_delta");

    lua_pushcfunction(L, l_input_is_mouse_down);
    lua_setfield(L, -2, "is_mouse_down");

    lua_pushcfunction(L, l_input_is_mouse_pressed);
    lua_setfield(L, -2, "is_mouse_pressed");

    lua_pushcfunction(L, l_input_is_mouse_released);
    lua_setfield(L, -2, "is_mouse_released");

    lua_pushcfunction(L, l_input_get_mouse_wheel);
    lua_setfield(L, -2, "get_mouse_wheel");

    lua_pushcfunction(L, l_input_gamepad_is_down);
    lua_setfield(L, -2, "gamepad_is_down");

    lua_pushcfunction(L, l_input_gamepad_axis);
    lua_setfield(L, -2, "gamepad_axis");

    lua_setfield(L, -2, "input");
    lua_pop(L, 1);
}

} // namespace crayon
