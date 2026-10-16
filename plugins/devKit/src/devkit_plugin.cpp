#include <crayon_plugin.h>
#include "devkit_core.hpp"
#include "devkit_lua.hpp"

static int devkit_on_load(const CrayonHostAPI* host) {
    if (!DevKit::instance().init(host)) {
        return -1;
    }
    return 0;
}

static int devkit_on_register(lua_State* L) {
    return devkit_lua_register(L);
}

static void devkit_on_update(lua_State* L, float dt) {
    DevKit::instance().on_update(L, dt);
}

static void devkit_on_unload(void) {
    DevKit::instance().shutdown();
}

static const CrayonPlugin s_devkit_plugin = {
    sizeof(CrayonPlugin),
    CRAYON_PLUGIN_API_VERSION,
    "devKit",
    "1.0.0",
    "In-game developer overlay, profiler, live REPL & debugger",
    devkit_on_load,
    devkit_on_register,
    devkit_on_update,
    devkit_on_unload
};

extern "C" CRAYON_PLUGIN_EXPORT const CrayonPlugin* crayon_plugin_entry(void) {
    return &s_devkit_plugin;
}
