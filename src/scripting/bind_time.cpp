#include "lua_runtime.hpp"
#include "../core/engine.hpp"

namespace crayon {

static int l_time_get_time(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_time());
    return 1;
}

static int l_time_get_dt(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_dt());
    return 1;
}

void register_time_bindings(lua_State* L) {
    lua_getglobal(L, "crayon");
    lua_newtable(L);

    lua_pushcfunction(L, l_time_get_time);
    lua_setfield(L, -2, "get_time");

    lua_pushcfunction(L, l_time_get_dt);
    lua_setfield(L, -2, "get_dt");

    lua_setfield(L, -2, "time");
    lua_pop(L, 1);
}

} // namespace crayon
