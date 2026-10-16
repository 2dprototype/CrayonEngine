#include "crayon_plugin.h"
#include <lua.h>
#include <lauxlib.h>

static int l_hello(lua_State* L) {
    lua_pushstring(L, "hello from custom plugin!");
    return 1;
}

static int on_load(const CrayonHostAPI* host) {
    if (CRAYON_HOST_HAS(host, log)) {
        host->log(CRAYON_LOG_LEVEL_INFO, "[myplugin] Loaded successfully!");
    }
    return 0;
}

static int on_register(lua_State* L) {
    lua_getglobal(L, "crayon");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }

    lua_newtable(L);
    lua_pushcfunction(L, l_hello);
    lua_setfield(L, -2, "hello");

    lua_setfield(L, -2, "myplugin");
    lua_pop(L, 1);
    return 0;
}

static void on_unload(void) {
}

static const CrayonPlugin k_plugin = {
    sizeof(CrayonPlugin),
    CRAYON_PLUGIN_API_VERSION,
    "myplugin",
    "1.0.0",
    "Minimal example plugin for Crayon Engine",
    on_load,
    on_register,
    NULL, /* on_update */
    on_unload,
};

CRAYON_PLUGIN_EXPORT const CrayonPlugin* crayon_plugin_entry(void) {
    return &k_plugin;
}
