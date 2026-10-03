#include <climits>

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

static int l_window_is_transparent(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_transparent());
    return 1;
}

static int l_window_set_click_through(lua_State* L) {
    bool enabled = lua_toboolean(L, 1);
    Engine::get().get_window().set_click_through(enabled);
    return 0;
}

static int l_window_is_click_through(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_click_through());
    return 1;
}

static int l_window_get_displays(lua_State* L) {
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (!ids || count == 0) {
        lua_newtable(L);
        if (ids) SDL_free(ids);
        return 1;
    }
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    lua_createtable(L, count, 0);
    int n = 0;
    for (int i = 0; i < count; ++i) {
        SDL_Rect r;
        if (!SDL_GetDisplayBounds(ids[i], &r)) continue;
        lua_createtable(L, 0, 5);
        lua_pushinteger(L, r.x); lua_setfield(L, -2, "x");
        lua_pushinteger(L, r.y); lua_setfield(L, -2, "y");
        lua_pushinteger(L, r.w); lua_setfield(L, -2, "w");
        lua_pushinteger(L, r.h); lua_setfield(L, -2, "h");
        lua_pushboolean(L, ids[i] == primary); lua_setfield(L, -2, "primary");
        lua_rawseti(L, -2, ++n);
    }
    SDL_free(ids);
    return 1;
}

static int l_window_get_virtual_desktop_bounds(lua_State* L) {
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (!ids || count == 0) {
        lua_pushinteger(L, 0);    lua_pushinteger(L, 0);
        lua_pushinteger(L, 1920); lua_pushinteger(L, 1080);
        if (ids) SDL_free(ids);
        return 4;
    }
    int x0 = INT_MAX, y0 = INT_MAX, x1 = INT_MIN, y1 = INT_MIN;
    for (int i = 0; i < count; ++i) {
        SDL_Rect r;
        if (SDL_GetDisplayBounds(ids[i], &r)) {
            if (r.x          < x0) x0 = r.x;
            if (r.y          < y0) y0 = r.y;
            if (r.x + r.w    > x1) x1 = r.x + r.w;
            if (r.y + r.h    > y1) y1 = r.y + r.h;
        }
    }
    SDL_free(ids);
    if (x0 == INT_MAX) { x0 = 0; y0 = 0; x1 = 1920; y1 = 1080; }
    lua_pushinteger(L, x0);
    lua_pushinteger(L, y0);
    lua_pushinteger(L, x1 - x0);
    lua_pushinteger(L, y1 - y0);
    return 4;
}

static int l_window_show(lua_State* L) {
    (void)L;
    Engine::get().get_window().show();
    return 0;
}

static int l_window_hide(lua_State* L) {
    (void)L;
    Engine::get().get_window().hide();
    return 0;
}

static int l_window_is_visible(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_visible());
    return 1;
}

static int l_window_set_skip_taskbar(lua_State* L) {
    bool skip = lua_toboolean(L, 1);
    Engine::get().get_window().set_skip_taskbar(skip);
    return 0;
}

static int l_window_is_skip_taskbar(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_skip_taskbar());
    return 1;
}

static int l_window_set_not_focusable(lua_State* L) {
    bool nf = lua_toboolean(L, 1);
    Engine::get().get_window().set_not_focusable(nf);
    return 0;
}

static int l_window_is_not_focusable(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_not_focusable());
    return 1;
}

static int l_window_set_utility_window(lua_State* L) {
    bool util = lua_toboolean(L, 1);
    Engine::get().get_window().set_utility_window(util);
    return 0;
}

static int l_window_is_utility_window(lua_State* L) {
    lua_pushboolean(L, Engine::get().get_window().is_utility_window());
    return 1;
}

static int l_window_get_usable_bounds(lua_State* L) {
    SDL_DisplayID disp = SDL_GetPrimaryDisplay();
    SDL_Rect r{0, 0, 1920, 1080};
    if (disp == 0 || !SDL_GetDisplayUsableBounds(disp, &r)) {
        // fall back to full bounds
        if (disp != 0) SDL_GetDisplayBounds(disp, &r);
    }
    lua_pushinteger(L, r.x);
    lua_pushinteger(L, r.y);
    lua_pushinteger(L, r.w);
    lua_pushinteger(L, r.h);
    return 4;
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
    
    lua_pushcfunction(L, l_window_get_usable_bounds);
    lua_setfield(L, -2, "getUsableBounds");
    
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

    lua_pushcfunction(L, l_window_is_transparent);
    lua_setfield(L, -2, "isTransparent");
    
    lua_pushcfunction(L, l_window_set_click_through);
    lua_setfield(L, -2, "setClickThrough");

    lua_pushcfunction(L, l_window_is_click_through);
    lua_setfield(L, -2, "isClickThrough");

    lua_pushcfunction(L, l_window_get_displays);
    lua_setfield(L, -2, "getDisplays");

    lua_pushcfunction(L, l_window_get_virtual_desktop_bounds);
    lua_setfield(L, -2, "getVirtualDesktopBounds");
    
    // ---- Visibility ----
    lua_pushcfunction(L, l_window_show);
    lua_setfield(L, -2, "show");

    lua_pushcfunction(L, l_window_hide);
    lua_setfield(L, -2, "hide");

    lua_pushcfunction(L, l_window_is_visible);
    lua_setfield(L, -2, "isVisible");

    // ---- Overlay window flags ----
    lua_pushcfunction(L, l_window_set_skip_taskbar);
    lua_setfield(L, -2, "setSkipTaskbar");

    lua_pushcfunction(L, l_window_is_skip_taskbar);
    lua_setfield(L, -2, "isSkipTaskbar");

    lua_pushcfunction(L, l_window_set_not_focusable);
    lua_setfield(L, -2, "setNotFocusable");

    lua_pushcfunction(L, l_window_is_not_focusable);
    lua_setfield(L, -2, "isNotFocusable");

    lua_pushcfunction(L, l_window_set_utility_window);
    lua_setfield(L, -2, "setUtilityWindow");

    lua_pushcfunction(L, l_window_is_utility_window);
    lua_setfield(L, -2, "isUtilityWindow");

    lua_setfield(L, -2, "window");
    lua_pop(L, 1);
}

} // namespace crayon
