# Crayon Plugin SDK

The **Crayon Plugin SDK** allows anyone to write and compile native plugins (`.dll` / `.so` / `.dylib`) for Crayon Engine **without downloading or rebuilding the engine source code**.

---

## SDK Contents

- **`include/`**
  - `crayon_plugin.h`: The stable C ABI interface (`CrayonPlugin`, `CrayonHostAPI`).
  - `lua.h`, `lauxlib.h`, `lualib.h`, `luaconf.h`, `lua.hpp`: Lua 5.1 / LuaJIT C headers.
- **`lib/`**
  - `lua51.lib` / `libluajit-5.1.dll.a`: Windows import libraries for Lua.
- **`bin/`**
  - `lua51.dll`: Windows runtime DLL for local testing.
- **`cmake/`**
  - `CrayonPlugin.cmake`: Optional CMake helper for CMake-based plugin projects.
- **`templates/minimal_c/`**
  - A ready-to-build starter plugin with `plugin.c`, `build.bat`, and `build.sh`.

---

## Quick Start: Writing a Plugin

A plugin is a shared library exporting ONE function:
```c
#include "crayon_plugin.h"
#include <lua.h>

static int l_hello(lua_State* L) {
    lua_pushstring(L, "hello from myplugin!");
    return 1;
}

static int on_register(lua_State* L) {
    lua_getglobal(L, "crayon");
    lua_newtable(L);
    lua_pushcfunction(L, l_hello);
    lua_setfield(L, -2, "hello");
    lua_setfield(L, -2, "myplugin"); // binds crayon.myplugin
    lua_pop(L, 1);
    return 0;
}

static const CrayonPlugin k_plugin = {
    sizeof(CrayonPlugin),
    CRAYON_PLUGIN_API_VERSION,
    "myplugin",
    "1.0.0",
    "My custom plugin description",
    NULL,          // on_load(const CrayonHostAPI* host)
    on_register,   // on_register(lua_State* L)
    NULL,          // on_update(lua_State* L, float dt)
    NULL,          // on_unload(void)
};

CRAYON_PLUGIN_EXPORT const CrayonPlugin* crayon_plugin_entry(void) {
    return &k_plugin;
}
```

---

## 1-Command Compilation

### Windows (MSVC)
```cmd
cl /LD /O2 plugin.c /I path\to\sdk\include path\to\sdk\lib\lua51.lib /link /OUT:myplugin.dll
```

### Windows (GCC / MinGW)
```cmd
gcc -shared -O3 plugin.c -I path/to/sdk/include -L path/to/sdk/lib -llua51 -s -o myplugin.dll
```

### Linux (GCC / Clang)
On Linux, Lua symbols are resolved dynamically from the `crayon` executable at load time:
```bash
gcc -shared -fPIC -O3 plugin.c -I path/to/sdk/include -s -o myplugin.so
```

### macOS (Clang)
```bash
clang -bundle -undefined dynamic_lookup -O3 plugin.c -I path/to/sdk/include -o myplugin.dylib
```

---

## Using Your Plugin in a Crayon Game

1. Put `myplugin.dll` (or `.so` / `.dylib`) in your game's folder or next to `crayon.exe`.
2. Enable it in your game's `main.lua`:
```lua
function crayon.config(t)
    t.plugins.myplugin = true
end

function crayon.init()
    crayon.print(crayon.myplugin.hello())
end
```
