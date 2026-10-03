#pragma once

#include <string>
#include <vector>
#include <SDL3/SDL.h>

namespace crayon {

enum class ScalingMode {
    Integer,
    Aspect,
    Stretch,
    Center
};

struct ViewportInfo {
    int x = 0;
    int y = 0;
    int width = 320;
    int height = 240;
    int scale = 1;
};

// Every field here is applied AT CREATION TIME. This exists specifically so
// that overlay-style windows (screenpets, sticky notes, HUDs) never flash an
// opaque/bordered placeholder before their final properties take effect.
struct WindowCreateInfo {
    std::string title = "Crayon Engine";
    int window_width = 960;
    int window_height = 720;
    int virtual_width = 320;
    int virtual_height = 240;

    bool resizable = true;
    bool high_dpi = true;

    // Overlay flags — see WindowConfig for full docs
    bool  transparent    = false;
    bool  borderless     = false;
    bool  always_on_top  = false;
    bool  click_through  = false;
    bool  skip_taskbar   = false;
    bool  not_focusable  = false;
    bool  utility_window = false;
    float opacity        = 1.0f;

    // If true, the window is created hidden. The caller is expected to call
    // show() once the first fully-prepared frame has been presented.
    bool hidden_at_start = false;
};

class Window {
public:
    Window();
    ~Window();

    bool init(const WindowCreateInfo& info);
    void shutdown();

    // Virtual Resolution
    void set_resolution(int w, int h);
    void get_resolution(int& w, int& h) const { w = m_virtual_w; h = m_virtual_h; }
    int get_virtual_width() const { return m_virtual_w; }
    int get_virtual_height() const { return m_virtual_h; }

    // Physical Window Size & Position
    void set_window_size(int w, int h);
    void get_window_size(int& w, int& h) const { w = m_window_w; h = m_window_h; }

    void set_window_position(int x, int y);
    void get_window_position(int& x, int& y) const;
    void center_window();

    void set_window_min_size(int w, int h);
    void set_window_max_size(int w, int h);

    // Window States
    void set_fullscreen(bool enabled);
    bool is_fullscreen() const { return m_fullscreen; }

    void set_vsync(bool enabled);
    bool get_vsync() const { return m_vsync; }

    void set_title(const std::string& title);
    const std::string& get_title() const { return m_title; }

    void set_resizable(bool resizable);
    bool is_resizable() const;

    void set_bordered(bool bordered);
    bool is_bordered() const;

    void maximize();
    void minimize();
    void restore();
    bool is_maximized() const;
    bool is_minimized() const;
    bool is_focused() const;

    // Visibility
    void show();
    void hide();
    bool is_visible() const;

    // Advanced Window Controls
    void  set_opacity(float opacity);
    float get_opacity() const;

    void set_always_on_top(bool on_top);
    bool is_always_on_top() const;

    void raise();
    void focus();
    void flash();

    void set_mouse_grab(bool grabbed);
    bool is_mouse_grabbed() const;

    bool is_transparent() const { return m_transparent; }
    void set_click_through(bool enabled);
    bool is_click_through() const { return m_click_through; }

    // Overlay-oriented window flags
    void set_skip_taskbar(bool skip);
    bool is_skip_taskbar() const { return m_skip_taskbar; }

    void set_not_focusable(bool not_focusable);
    bool is_not_focusable() const { return m_not_focusable; }

    void set_utility_window(bool utility);
    bool is_utility_window() const { return m_utility_window; }

    // Scaling Modes
    void set_scaling_mode(ScalingMode mode);
    ScalingMode get_scaling_mode() const { return m_scaling_mode; }
    void set_scaling_mode_string(const std::string& mode);
    std::string get_scaling_mode_string() const;

    // Mouse & Cursor Controls
    void set_mouse_relative(bool relative);
    bool is_mouse_relative() const;

    void show_cursor(bool show);
    bool is_cursor_visible() const;

    // Display Monitor Info
    void get_display_size(int& w, int& h) const;

    // Engine Lifecycle
    void swap_buffers();
    bool should_close() const { return m_should_close; }
    void request_quit() { m_should_close = true; }

    void handle_event(const SDL_Event& event);

    ViewportInfo get_viewport_info() const;
    void window_to_virtual(float win_x, float win_y, float& virt_x, float& virt_y) const;
    void virtual_to_window(float virt_x, float virt_y, float& win_x, float& win_y) const;

    SDL_Window* get_sdl_window() const { return m_window; }
    SDL_GLContext get_gl_context() const { return m_gl_context; }

private:
    void update_viewport();

    SDL_Window* m_window = nullptr;
    SDL_GLContext m_gl_context = nullptr;

    std::string m_title = "Crayon Engine";
    int m_window_w = 320;
    int m_window_h = 240;
    int m_virtual_w = 320;
    int m_virtual_h = 240;

    bool m_fullscreen = false;
    bool m_vsync = true;
    bool m_should_close = false;
    bool m_transparent = false;
    bool m_click_through = false;
    bool m_skip_taskbar = false;
    bool m_not_focusable = false;
    bool m_utility_window = false;

    ScalingMode m_scaling_mode = ScalingMode::Integer;
    ViewportInfo m_viewport;
};

} // namespace crayon