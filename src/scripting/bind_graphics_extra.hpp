#pragma once
struct lua_State;

namespace crayon {
// Registers the extra graphics API (ImageData, newTexture, texture filter/wrap, screenshots...)
// into the `crayon.graphics` table, which must be on top of the Lua stack.
void register_graphics_extra_bindings(lua_State* L);
}
