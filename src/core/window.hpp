#pragma once

#include <string>
#include <SDL3/SDL.h>

namespace crayon {

enum class ScalingMode {
    Integer,  // Pixel-perfect integer scaling with black bars (default)
    Aspect,   // Smooth aspect-ratio fit preserving proportions
    Stretch,  // Stretch to fill entire physical window
    Center    // 1:1 unscaled centered in window
};

struct ViewportInfo {
    int x = 0;
    int y = 0;
    int width = 320;
    int height = 240;
    int scale = 1;
};

class Window {
public:
    Window();
    ~Window();

    bool init(const std::string& title, int window_w, int window_h, int virtual_w, int virtual_h);
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

    // Advanced Window Controls
    void set_opacity(float opacity);
    float get_opacity() const;

    void set_always_on_top(bool on_top);
    bool is_always_on_top() const;

    void raise();
    void focus();
    void flash();

    void set_mouse_grab(bool grabbed);
    bool is_mouse_grabbed() const;

    void set_transparent(bool transparent);
    bool is_transparent() const { return m_transparent; }

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
    int m_window_w = 960;
    int m_window_h = 720;
    int m_virtual_w = 320;
    int m_virtual_h = 240;

    bool m_fullscreen = false;
    bool m_vsync = true;
    bool m_should_close = false;
    bool m_transparent = false;

    ScalingMode m_scaling_mode = ScalingMode::Integer;
    ViewportInfo m_viewport;
};

} // namespace crayon
