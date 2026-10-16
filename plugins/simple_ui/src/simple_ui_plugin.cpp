#include <crayon_plugin.h>
#include "simple_ui_core.hpp"
#include "simple_ui_lua.hpp"

static int simple_ui_on_load(const CrayonHostAPI* host) {
    if (!SimpleUI::instance().init(host)) {
        return -1;
    }
    return 0;
}

static int simple_ui_on_register(lua_State* L) {
    return simple_ui_lua_register(L);
}

static void simple_ui_on_update(lua_State* L, float dt) {
    (void)L;
    SimpleUI::instance().on_update(dt);
}

static void simple_ui_on_unload(void) {
    SimpleUI::instance().shutdown();
}

static const CrayonPlugin s_simple_ui_plugin = {
    sizeof(CrayonPlugin),
    CRAYON_PLUGIN_API_VERSION,
    "simpleUI",
    "1.0.0",
    "Native high-resolution UI library immune to retro/post-process filters",
    simple_ui_on_load,
    simple_ui_on_register,
    simple_ui_on_update,
    simple_ui_on_unload
};

extern "C" CRAYON_PLUGIN_EXPORT const CrayonPlugin* crayon_plugin_entry(void) {
    return &s_simple_ui_plugin;
}
