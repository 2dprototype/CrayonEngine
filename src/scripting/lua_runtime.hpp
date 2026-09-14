#pragma once

#include <string>
#include <memory>
#include <lua.hpp>

namespace crayon {

class Model3D;

struct LuaModel {
    std::shared_ptr<Model3D> model;
};

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

    // Event Callbacks (Love2D style)
    void call_key_down(const std::string& key, bool is_repeat);
    void call_key_up(const std::string& key);
    void call_mouse_down(float x, float y, int button);
    void call_mouse_up(float x, float y, int button);
    void call_mouse_moved(float x, float y, float dx, float dy);
    void call_wheel_moved(float dx, float dy);
    void call_text_input(const std::string& text);
    void call_gamepad_down(int button);
    void call_gamepad_up(int button);
    void call_gamepad_axis(int axis, float value);

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
