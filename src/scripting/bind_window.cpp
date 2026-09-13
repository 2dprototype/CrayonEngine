#include "lua_runtime.hpp"
#include "../core/engine.hpp"

namespace crayon {

static int l_window_set_resolution(lua_State* L) {
    int w = static_cast<int>(luaL_checkinteger(L, 1));
    int h = static_cast<int>(luaL_checkinteger(L, 2));
    Engine::get().get_window().set_resolution(w, h);
    return 0;
}

static int l_window_set_window_size(lua_State* L) {
    int w = static_cast<int>(luaL_checkinteger(L, 1));
    int h = static_cast<int>(luaL_checkinteger(L, 2));
    Engine::get().get_window().set_window_size(w, h);
    return 0;
}

static int l_window_set_fullscreen(lua_State* L) {
    bool enabled = lua_toboolean(L, 1);
    Engine::get().get_window().set_fullscreen(enabled);
    return 0;
}

static int l_window_set_vsync(lua_State* L) {
    bool enabled = lua_toboolean(L, 1);
    Engine::get().get_window().set_vsync(enabled);
    return 0;
}

static int l_window_set_title(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    Engine::get().get_window().set_title(title);
    return 0;
}

static int l_window_get_fps(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_fps());
    return 1;
}

static int l_window_quit(lua_State* L) {
    (void)L;
    Engine::get().get_window().request_quit();
    return 0;
}

static int l_window_get_resolution(lua_State* L) {
    int w, h;
    Engine::get().get_window().get_resolution(w, h);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}

static int l_window_get_window_size(lua_State* L) {
    int w, h;
    Engine::get().get_window().get_window_size(w, h);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}

static int l_window_is_fullscreen(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_fullscreen());
    return 1;
}

static int l_window_get_vsync(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().get_vsync());
    return 1;
}

static int l_window_get_title(lua_State* L) {
    lua_pushstring(L, Engine::get().get_window().get_title().c_str());
    return 1;
}

static int l_window_set_position(lua_State* L) {
    int x = static_cast<int>(luaL_checkinteger(L, 1));
    int y = static_cast<int>(luaL_checkinteger(L, 2));
    Engine::get().get_window().set_window_position(x, y);
    return 0;
}

static int l_window_get_position(lua_State* L) {
    int x = 0, y = 0;
    Engine::get().get_window().get_window_position(x, y);
    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    return 2;
}

static int l_window_center(lua_State* L) {
    (void)L;
    Engine::get().get_window().center_window();
    return 0;
}

static int l_window_set_min_size(lua_State* L) {
    int w = static_cast<int>(luaL_checkinteger(L, 1));
    int h = static_cast<int>(luaL_checkinteger(L, 2));
    Engine::get().get_window().set_window_min_size(w, h);
    return 0;
}

static int l_window_set_max_size(lua_State* L) {
    int w = static_cast<int>(luaL_checkinteger(L, 1));
    int h = static_cast<int>(luaL_checkinteger(L, 2));
    Engine::get().get_window().set_window_max_size(w, h);
    return 0;
}

static int l_window_set_resizable(lua_State* L) {
    bool enabled = lua_toboolean(L, 1);
    Engine::get().get_window().set_resizable(enabled);
    return 0;
}

static int l_window_is_resizable(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_resizable());
    return 1;
}

static int l_window_set_bordered(lua_State* L) {
    bool enabled = lua_toboolean(L, 1);
    Engine::get().get_window().set_bordered(enabled);
    return 0;
}

static int l_window_is_bordered(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_bordered());
    return 1;
}

static int l_window_maximize(lua_State* L) {
    (void)L;
    Engine::get().get_window().maximize();
    return 0;
}

static int l_window_minimize(lua_State* L) {
    (void)L;
    Engine::get().get_window().minimize();
    return 0;
}

static int l_window_restore(lua_State* L) {
    (void)L;
    Engine::get().get_window().restore();
    return 0;
}

static int l_window_is_maximized(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_maximized());
    return 1;
}

static int l_window_is_minimized(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_minimized());
    return 1;
}

static int l_window_is_focused(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_focused());
    return 1;
}

static int l_window_set_scaling_mode(lua_State* L) {
    const char* mode = luaL_checkstring(L, 1);
    Engine::get().get_window().set_scaling_mode_string(mode);
    return 0;
}

static int l_window_get_scaling_mode(lua_State* L) {
    lua_pushstring(L, Engine::get().get_window().get_scaling_mode_string().c_str());
    return 1;
}

static int l_window_set_mouse_relative(lua_State* L) {
    bool enabled = lua_toboolean(L, 1);
    Engine::get().get_window().set_mouse_relative(enabled);
    return 0;
}

static int l_window_is_mouse_relative(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_mouse_relative());
    return 1;
}

static int l_window_show_cursor(lua_State* L) {
    bool show = lua_isboolean(L, 1) ? lua_toboolean(L, 1) : true;
    Engine::get().get_window().show_cursor(show);
    return 0;
}

static int l_window_is_cursor_visible(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_cursor_visible());
    return 1;
}

static int l_window_get_display_size(lua_State* L) {
    int w = 0, h = 0;
    Engine::get().get_window().get_display_size(w, h);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}

static int l_window_set_opacity(lua_State* L) {
    float opacity = static_cast<float>(luaL_checknumber(L, 1));
    Engine::get().get_window().set_opacity(opacity);
    return 0;
}

static int l_window_get_opacity(lua_State* L) {
    lua_pushnumber(L, Engine::get().get_window().get_opacity());
    return 1;
}

static int l_window_set_always_on_top(lua_State* L) {
    bool on_top = lua_toboolean(L, 1);
    Engine::get().get_window().set_always_on_top(on_top);
    return 0;
}

static int l_window_is_always_on_top(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_always_on_top());
    return 1;
}

static int l_window_raise(lua_State* L) {
    (void)L;
    Engine::get().get_window().raise();
    return 0;
}

static int l_window_focus(lua_State* L) {
    (void)L;
    Engine::get().get_window().focus();
    return 0;
}

static int l_window_flash(lua_State* L) {
    (void)L;
    Engine::get().get_window().flash();
    return 0;
}

static int l_window_set_mouse_grab(lua_State* L) {
    bool grab = lua_toboolean(L, 1);
    Engine::get().get_window().set_mouse_grab(grab);
    return 0;
}

static int l_window_is_mouse_grabbed(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_mouse_grabbed());
    return 1;
}

static int l_window_set_transparent(lua_State* L) {
    bool trans = lua_toboolean(L, 1);
    Engine::get().get_window().set_transparent(trans);
    return 0;
}

static int l_window_is_transparent(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_transparent());
    return 1;
}

void register_window_bindings(lua_State* L) {
    lua_getglobal(L, "crayon");
    lua_newtable(L);

    lua_pushcfunction(L, l_window_set_resolution);
    lua_setfield(L, -2, "setResolution");

    lua_pushcfunction(L, l_window_get_resolution);
    lua_setfield(L, -2, "getResolution");

    lua_pushcfunction(L, l_window_set_window_size);
    lua_setfield(L, -2, "setWindowSize");

    lua_pushcfunction(L, l_window_get_window_size);
    lua_setfield(L, -2, "getWindowSize");

    lua_pushcfunction(L, l_window_set_position);
    lua_setfield(L, -2, "setPosition");

    lua_pushcfunction(L, l_window_get_position);
    lua_setfield(L, -2, "getPosition");

    lua_pushcfunction(L, l_window_center);
    lua_setfield(L, -2, "center");

    lua_pushcfunction(L, l_window_set_min_size);
    lua_setfield(L, -2, "setMinSize");

    lua_pushcfunction(L, l_window_set_max_size);
    lua_setfield(L, -2, "setMaxSize");

    lua_pushcfunction(L, l_window_set_fullscreen);
    lua_setfield(L, -2, "setFullscreen");

    lua_pushcfunction(L, l_window_is_fullscreen);
    lua_setfield(L, -2, "isFullscreen");

    lua_pushcfunction(L, l_window_set_vsync);
    lua_setfield(L, -2, "setVsync");

    lua_pushcfunction(L, l_window_get_vsync);
    lua_setfield(L, -2, "getVsync");

    lua_pushcfunction(L, l_window_set_title);
    lua_setfield(L, -2, "setTitle");

    lua_pushcfunction(L, l_window_get_title);
    lua_setfield(L, -2, "getTitle");

    lua_pushcfunction(L, l_window_set_resizable);
    lua_setfield(L, -2, "setResizable");

    lua_pushcfunction(L, l_window_is_resizable);
    lua_setfield(L, -2, "isResizable");

    lua_pushcfunction(L, l_window_set_bordered);
    lua_setfield(L, -2, "setBordered");

    lua_pushcfunction(L, l_window_is_bordered);
    lua_setfield(L, -2, "isBordered");

    lua_pushcfunction(L, l_window_maximize);
    lua_setfield(L, -2, "maximize");

    lua_pushcfunction(L, l_window_minimize);
    lua_setfield(L, -2, "minimize");

    lua_pushcfunction(L, l_window_restore);
    lua_setfield(L, -2, "restore");

    lua_pushcfunction(L, l_window_is_maximized);
    lua_setfield(L, -2, "isMaximized");

    lua_pushcfunction(L, l_window_is_minimized);
    lua_setfield(L, -2, "isMinimized");

    lua_pushcfunction(L, l_window_is_focused);
    lua_setfield(L, -2, "isFocused");

    lua_pushcfunction(L, l_window_set_scaling_mode);
    lua_setfield(L, -2, "setScalingMode");

    lua_pushcfunction(L, l_window_get_scaling_mode);
    lua_setfield(L, -2, "getScalingMode");

    lua_pushcfunction(L, l_window_set_mouse_relative);
    lua_setfield(L, -2, "setMouseRelative");

    lua_pushcfunction(L, l_window_is_mouse_relative);
    lua_setfield(L, -2, "isMouseRelative");

    lua_pushcfunction(L, l_window_show_cursor);
    lua_setfield(L, -2, "showCursor");

    lua_pushcfunction(L, l_window_is_cursor_visible);
    lua_setfield(L, -2, "isCursorVisible");

    lua_pushcfunction(L, l_window_get_display_size);
    lua_setfield(L, -2, "getDisplaySize");
    
    lua_pushcfunction(L, l_window_get_fps);
    lua_setfield(L, -2, "getFps");

    lua_pushcfunction(L, l_window_quit);
    lua_setfield(L, -2, "quit");

    lua_pushcfunction(L, l_window_set_opacity);
    lua_setfield(L, -2, "setOpacity");

    lua_pushcfunction(L, l_window_get_opacity);
    lua_setfield(L, -2, "getOpacity");

    lua_pushcfunction(L, l_window_set_always_on_top);
    lua_setfield(L, -2, "setAlwaysOnTop");

    lua_pushcfunction(L, l_window_is_always_on_top);
    lua_setfield(L, -2, "isAlwaysOnTop");

    lua_pushcfunction(L, l_window_raise);
    lua_setfield(L, -2, "raise");

    lua_pushcfunction(L, l_window_focus);
    lua_setfield(L, -2, "focus");

    lua_pushcfunction(L, l_window_flash);
    lua_setfield(L, -2, "flash");

    lua_pushcfunction(L, l_window_set_mouse_grab);
    lua_setfield(L, -2, "setMouseGrab");

    lua_pushcfunction(L, l_window_is_mouse_grabbed);
    lua_setfield(L, -2, "isMouseGrabbed");

    lua_pushcfunction(L, l_window_set_transparent);
    lua_setfield(L, -2, "setTransparent");
    lua_pushcfunction(L, l_window_is_transparent);
    lua_setfield(L, -2, "isTransparent");

    lua_setfield(L, -2, "window");
    lua_pop(L, 1);
}

} // namespace crayon
