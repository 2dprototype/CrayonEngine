/*
 * crayon_plugin.h - the Crayon Engine plugin SDK (C ABI).
 *
 * A plugin is a shared library (.dll / .so / .dylib) that exports ONE symbol:
 *
 *     CRAYON_PLUGIN_EXPORT const CrayonPlugin* crayon_plugin_entry(void);
 *
 * The engine loads it when the game asks for it in crayon.config():
 *
 *     function crayon.config(t)
 *         t.plugins.physics4D = true      -- loads physics4D.dll / physics4D.so
 *     end
 *
 * Why a C ABI?  Crossing a DLL boundary with C++ types (std::string, std::vector, classes with
 * vtables ...) is only safe when the engine and the plugin are built by the very same compiler,
 * runtime and flags.  Plain C structs and function pointers are safe between ANY compilers, so
 * plugins can be built separately from the engine and third-party plugins just work.
 *
 * Rules of the road
 *   - Never include engine headers from a plugin.  Everything the engine offers goes through
 *     CrayonHostAPI below.  Everything the plugin offers goes through CrayonPlugin.
 *   - The Lua C API (lua.h / lauxlib.h from vendor/luajit/include) IS shared: the engine and the
 *     plugin talk to the very same lua_State.  On Windows link the plugin against the same
 *     lua51.dll the engine uses; on Linux/macOS leave the lua_* symbols undefined and let the
 *     engine (built with exported symbols) resolve them.  crayon_add_plugin() does this for you.
 *   - Do not let C++ exceptions escape any callback.
 *   - Lua state is closed BEFORE on_unload() runs, so userdata __gc metamethods are safe to
 *     implement, but on_unload() itself must not touch any lua_State.
 */
#ifndef CRAYON_PLUGIN_H
#define CRAYON_PLUGIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque here; identical to the typedef in LuaJIT's lua.h (so including both is fine). */
struct lua_State;

/* Bump when CrayonPlugin / CrayonHostAPI change in an INCOMPATIBLE way.  Appending new fields
 * to the end of CrayonHostAPI is compatible and does not need a bump: plugins test for them
 * with CRAYON_HOST_HAS(). */
#define CRAYON_PLUGIN_API_VERSION 1u

#define CRAYON_PLUGIN_ENTRY_NAME "crayon_plugin_entry"

#if defined(_WIN32) || defined(__CYGWIN__)
#  define CRAYON_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#  define CRAYON_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#  define CRAYON_PLUGIN_EXPORT
#endif

/* ---- log levels for CrayonHostAPI::log ------------------------------------------------- */
enum {
    CRAYON_LOG_LEVEL_DEBUG = 0,
    CRAYON_LOG_LEVEL_INFO  = 1,
    CRAYON_LOG_LEVEL_WARN  = 2,
    CRAYON_LOG_LEVEL_ERROR = 3
};

/* ---------------------------------------------------------------------------------------
 * CrayonHostAPI: what the ENGINE offers the plugin.  Handed to on_load() and valid until
 * on_unload() returns.  All vectors are plain float arrays: p[3] = x,y,z   rgba[4] = r,g,b,a.
 * ------------------------------------------------------------------------------------- */
typedef struct CrayonHostAPI {
    uint32_t    struct_size;     /* sizeof(CrayonHostAPI) as the ENGINE was built */
    uint32_t    api_version;     /* CRAYON_PLUGIN_API_VERSION the engine implements */
    const char* engine_version;

    /* Writes a line to the engine log / in-game console. */
    void (*log)(int level, const char* message);

    /* The main Lua state (same one on_register / on_update receive). */
    struct lua_State* (*get_lua_state)(void);

    /* ---- 3D debug drawing.  Only call while crayon.draw() is running (inside the 3D pass).
     *      No-ops if the game disabled t.modules.mesh3D. ---- */
    void (*get_model_color)(float out_rgba[4]);
    void (*set_model_color)(const float rgba[4]);
    void (*draw_triangle_3d)(const float p0[3], const float p1[3], const float p2[3], const float rgba[4]);
    void (*draw_sphere_3d)(const float center[3], float radius);
    void (*draw_line_3d)(const float a[3], const float b[3], const float rgba[4]);

    /* Fast batched lines: begin, add_line... N times, end. */
    void (*line_batch_begin)(void);
    void (*line_batch_add)(const float a[3], const float b[3], const float rgba[4]);
    void (*line_batch_wire_sphere)(const float center[3], float radius, const float rgba[4], int rings, int sectors);
    void (*line_batch_end)(void);

    /* ---- append new fields BELOW this line, never above ---- */
} CrayonHostAPI;

/* True if the engine that loaded us is new enough to provide `field`. */
#define CRAYON_HOST_HAS(host, field) \
    ((host) && (host)->struct_size >= offsetof(CrayonHostAPI, field) + sizeof((host)->field) && (host)->field)

/* ---------------------------------------------------------------------------------------
 * CrayonPlugin: what the PLUGIN offers the engine.  Returned (a pointer to a static object)
 * by crayon_plugin_entry().  Every callback is optional except `name`.
 * ------------------------------------------------------------------------------------- */
typedef struct CrayonPlugin {
    uint32_t    struct_size;     /* sizeof(CrayonPlugin) */
    uint32_t    api_version;     /* CRAYON_PLUGIN_API_VERSION this plugin was built against */

    const char* name;            /* Lua-facing id, e.g. "physics4D" (should match the t.plugins key) */
    const char* version;         /* free-form, e.g. "1.0.0" */
    const char* description;     /* one line, may be NULL */

    /* Called once right after the library is loaded.  Return 0 on success; non-zero aborts the
     * load (the library is unloaded and crayon.<name> raises a helpful error when used). */
    int  (*on_load)(const CrayonHostAPI* host);

    /* Called every time a Lua state is (re)created, after the engine's own modules are
     * registered and BEFORE crayon.init().  Create your `crayon.<name>` table here.
     * Return 0 on success. */
    int  (*on_register)(struct lua_State* L);

    /* Called once per frame, before crayon.update(dt).  `dt` is the variable frame time in
     * seconds - keep your own accumulator if you want a fixed timestep.  Skipped while the
     * engine is paused. */
    void (*on_update)(struct lua_State* L, float dt);

    /* Called once when the plugin is going away (game exit or hot-reload), after the Lua state
     * has been closed.  Free everything here; the library is unloaded right afterwards. */
    void (*on_unload)(void);
} CrayonPlugin;

/* Signature of the exported entry point. */
typedef const CrayonPlugin* (*CrayonPluginEntryFn)(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CRAYON_PLUGIN_H */
