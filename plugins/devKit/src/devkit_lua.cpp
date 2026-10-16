#include "devkit_lua.hpp"
#include "devkit_core.hpp"
#include <lua.hpp>

static int l_set_visible(lua_State* L) {
    bool v = lua_toboolean(L, 1);
    DevKit::instance().set_visible(v);
    return 0;
}

static int l_is_visible(lua_State* L) {
    lua_pushboolean(L, DevKit::instance().is_visible());
    return 1;
}

static int l_toggle(lua_State* L) {
    (void)L;
    DevKit::instance().toggle_visible();
    return 0;
}

static int l_get_time_scale(lua_State* L) {
    lua_pushnumber(L, DevKit::instance().get_time_scale());
    return 1;
}

static int l_set_time_scale(lua_State* L) {
    float scale = (float)luaL_checknumber(L, 1);
    DevKit::instance().set_time_scale(scale);
    return 0;
}

static int l_is_paused(lua_State* L) {
    lua_pushboolean(L, DevKit::instance().is_paused());
    return 1;
}

static int l_set_paused(lua_State* L) {
    bool p = lua_toboolean(L, 1);
    DevKit::instance().set_paused(p);
    return 0;
}

static int l_step_frame(lua_State* L) {
    (void)L;
    DevKit::instance().step_frame();
    return 0;
}

static int l_add_tab(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    DevKit::instance().add_custom_tab(name, ref);
    return 0;
}

static int l_log(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    bool is_err = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : false;
    DevKit::instance().add_log(text, is_err);
    return 0;
}

static int l_want_capture_mouse(lua_State* L) {
    lua_pushboolean(L, DevKit::instance().want_capture_mouse());
    return 1;
}

static int l_want_capture_keyboard(lua_State* L) {
    lua_pushboolean(L, DevKit::instance().want_capture_keyboard());
    return 1;
}

static const luaL_Reg devkit_lib[] = {
    {"setVisible",          l_set_visible},
    {"isVisible",           l_is_visible},
    {"toggle",              l_toggle},
    {"getTimeScale",        l_get_time_scale},
    {"setTimeScale",        l_set_time_scale},
    {"isPaused",            l_is_paused},
    {"setPaused",           l_set_paused},
    {"stepFrame",           l_step_frame},
    {"addTab",              l_add_tab},
    {"log",                 l_log},
    {"wantCaptureMouse",    l_want_capture_mouse},
    {"wantCaptureKeyboard", l_want_capture_keyboard},
    {nullptr, nullptr}
};

int devkit_lua_register(lua_State* L) {
    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "crayon");
    }

    lua_newtable(L);
    for (const luaL_Reg* l = devkit_lib; l->name; ++l) {
        lua_pushcfunction(L, l->func);
        lua_setfield(L, -2, l->name);
    }

    // Register strictly as crayon.devKit
    lua_setfield(L, -2, "devKit");

    lua_pop(L, 1);
    return 0;
}
