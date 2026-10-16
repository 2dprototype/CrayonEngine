#pragma once

// Process-wide state of the physics4D plugin (one plugin instance per process).
#include "crayon_plugin.h"

namespace crayon {

class Physics4DSystem;

extern Physics4DSystem*     g_physics4d;   // the live system; valid between on_register() and on_unload()
extern const CrayonHostAPI* g_host;        // engine services (logging, debug drawing)

void register_physics4d_bindings(struct lua_State* L);   // physics4d_lua.cpp

} // namespace crayon
