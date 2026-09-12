#pragma once

#include <string>
#include <lua.hpp>

namespace crayon {

class LuaRuntime {
public:
    LuaRuntime();
    ~LuaRuntime();

    bool init();
    void shutdown();

    bool load_script(const std::string& filepath);
    bool reload_script(const std::string& filepath);

    void call_init();
    void call_update(float dt);
    void call_draw();

    lua_State* get_state() const { return m_L; }

private:
    void register_modules();
    int push_error_handler();

    lua_State* m_L = nullptr;
    bool m_initialized = false;
};

void register_window_bindings(lua_State* L);
void register_graphics_bindings(lua_State* L);
void register_input_bindings(lua_State* L);
void register_time_bindings(lua_State* L);
void register_physics3d_bindings(lua_State* L);
void register_physics2d_bindings(lua_State* L);

} // namespace crayon
