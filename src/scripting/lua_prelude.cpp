#include "lua_prelude.hpp"
#include "../core/log.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <string>

namespace crayon {

bool load_lua_prelude(lua_State* L) {
    bool ok = true;
    for (const PreludeChunk* c = kPreludeChunks; c->name; ++c) {
        std::string source;
        for (const char* const* p = c->pieces; *p; ++p) source += *p;
        const std::string chunk_name = std::string("=prelude/") + c->name;
        if (luaL_loadbuffer(L, source.data(), source.size(), chunk_name.c_str()) != 0 ||
            lua_pcall(L, 0, 0, 0) != 0) {
            CRAYON_LOG_ERROR("Lua prelude '{}' failed: {}", c->name, lua_tostring(L, -1));
            lua_pop(L, 1);
            ok = false;
        }
    }
    return ok;
}

} // namespace crayon
