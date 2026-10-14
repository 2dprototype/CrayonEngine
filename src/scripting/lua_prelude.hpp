#pragma once
// Built-in Lua libraries (crayon.json / sprite / tilemap / drawqueue / lights2d) embedded in the
// engine. Sources live in src/scripting/prelude/*.lua; run tools/embed_lua.py after editing them.
struct lua_State;

namespace crayon {

struct PreludeChunk {
    const char* name;
    const char* const* pieces;   // null-terminated list of string fragments
};
extern const PreludeChunk kPreludeChunks[];   // terminated by {nullptr, nullptr}

// Loads every embedded chunk into `L` (the global `crayon` table must already exist).
// Returns false (after logging) if a chunk fails.
bool load_lua_prelude(lua_State* L);

} // namespace crayon
