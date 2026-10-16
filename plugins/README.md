# Crayon plugins

A plugin is a native shared library (`.dll` / `.so` / `.dylib`) that adds a `crayon.<name>` Lua API
(and optionally a per-frame hook) to the engine. Heavy or rarely-used features live here instead of in
the engine binary - **4D physics is the first one**.

## Using a plugin

1. Put the library next to the `crayon` binary (or in a `plugins/` folder there, or in the project folder).
   Extra search path: the `CRAYON_PLUGIN_PATH` environment variable (`;` on Windows, `:` elsewhere).
2. Ask for it in `crayon.config()`:

```lua
function crayon.config(t)
    t.plugins.physics4D = true                  -- physics4D.dll / libphysics4D.so / physics4D.dylib
    t.plugins.mything   = "libs/mything.dll"    -- explicit path (relative to the project or the binary)
end

function crayon.init()
    crayon.physics4D.createTesseract(0, 5, 0, 0, 1)
end
```

If a plugin can't be loaded the game still starts; `crayon.<name>` then raises an error that tells you why
(file not found + where it looked, wrong API version, ...). The same text is written to the log.

## Building

Plugins can be built completely standalone without rebuilding the engine:

### Dedicated Plugin Build Scripts
```cmd
:: Build individual plugins directly:
build_physics4D.bat          :: or plugins\build_physics4D.bat
build_activation.bat         :: or plugins\build_activation.bat

:: Unix / macOS:
./build_physics4D.sh         :: or ./plugins/build_physics4D.sh
./build_activation.sh        :: or ./plugins/build_activation.sh
```

### All Plugins
```cmd
plugins\build.bat            :: all plugins -> plugins\bin\*.dll
./plugins/build.sh           :: Linux / macOS
```

To build a standalone plugin with zero engine source code, see the standalone **`sdk/`** directory.

## Writing your own plugin

1. Create `plugins/myplugin/CMakeLists.txt`:

```cmake
crayon_add_plugin(myplugin SOURCES src/myplugin.cpp)
```

2. Write the plugin against `plugins/include/crayon_plugin.h` - **do not include engine headers**:

```cpp
#include <lua.hpp>
#include "crayon_plugin.h"

static const CrayonHostAPI* host;

static int l_hello(lua_State* L) { lua_pushstring(L, "hello from a plugin"); return 1; }

static int on_load(const CrayonHostAPI* h) { host = h; return 0; }
static int on_register(lua_State* L) {            // runs for every (re)created Lua state, before crayon.init()
    lua_getglobal(L, "crayon");
    lua_newtable(L);
    lua_pushcfunction(L, l_hello);  lua_setfield(L, -2, "hello");
    lua_setfield(L, -2, "myplugin");              // crayon.myplugin.hello()
    lua_pop(L, 1);
    return 0;
}

static const CrayonPlugin plugin = {
    sizeof(CrayonPlugin), CRAYON_PLUGIN_API_VERSION,
    "myplugin", "1.0.0", "says hello",
    on_load, on_register, /*on_update*/ nullptr, /*on_unload*/ nullptr };

extern "C" CRAYON_PLUGIN_EXPORT const CrayonPlugin* crayon_plugin_entry(void) { return &plugin; }
```

3. Enable it with `t.plugins.myplugin = true`.

Lifecycle: `on_load(host)` -> `on_register(L)` (every Lua state) -> `on_update(L, dt)` every frame before
`crayon.update` (not while paused) -> Lua state closed -> `on_unload()`. On hot-reload (soft restart) the
whole cycle runs again with the library freshly loaded.

Rules (details in the header): C ABI only across the boundary, no C++ exceptions escaping callbacks,
`on_unload` must not touch any `lua_State`, and the plugin shares the engine's `lua_State` (Windows: link
`lua51.dll` - done by `crayon_add_plugin`; Linux/macOS: `lua_*` symbols come from the crayon binary).
Rebuild plugins when `CRAYON_PLUGIN_API_VERSION` changes.

## Shipped plugins

| plugin | Lua API | notes |
|---|---|---|
| `activation` | `crayon.activation.*` (see plugins/activation/README.md) | offline license keys, trial, feature flags; ships a `crayon_keygen` tool |
| `physics4D` | `crayon.physics4D.*` (see docs/quick_start.md) | hv4d 4D rigid bodies; steps at a fixed 60 Hz, draws via the host debug-drawing API |

Android: plugins are not built/loaded there yet (the app can only dlopen libraries from its APK lib dir).
