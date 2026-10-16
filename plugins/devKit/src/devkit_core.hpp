#pragma once

#include <crayon_plugin.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <deque>

struct CustomTab {
    std::string name;
    int lua_ref{-1};
};

class DevKit {
public:
    static DevKit& instance();

    bool init(const CrayonHostAPI* host);
    void shutdown();

    void on_update(struct lua_State* L, float dt);
    void on_pre_swap(SDL_Window* window);
    bool handle_event(const SDL_Event* event);

    // Visibility & Hotkeys
    bool is_visible() const { return m_visible; }
    void set_visible(bool v) { m_visible = v; }
    void toggle_visible() { m_visible = !m_visible; }
    void set_toggle_key(SDL_Keycode key) { m_toggle_key = key; }

    // Time Control
    float get_time_scale() const { return m_time_scale; }
    void set_time_scale(float scale) { m_time_scale = (scale < 0.0f) ? 0.0f : scale; }
    bool is_paused() const { return m_paused; }
    void set_paused(bool p) { m_paused = p; }
    void step_frame() { m_step_frame_requested = true; }
    bool consume_step_frame();

    // Custom Tabs
    void add_custom_tab(const std::string& name, int lua_ref);

    // Logging & Console
    void add_log(const std::string& text, bool is_error = false);
    void clear_logs();

    bool want_capture_mouse() const;
    bool want_capture_keyboard() const;
    void ensure_frame_started();

    void log(int level, const char* msg);

private:
    DevKit();
    ~DevKit();

    void apply_theme();
    bool install_hooks();
    void remove_hooks();

    void render_ui(struct lua_State* L);
    void render_profiler_tab();
    void render_time_tab();
    void render_console_tab(struct lua_State* L);
    void render_visuals_tab(struct lua_State* L);
    void render_system_tab();
    void render_custom_tabs(struct lua_State* L);

    const CrayonHostAPI* m_host{nullptr};
    struct lua_State* m_L{nullptr};
    SDL_Window* m_window{nullptr};
    SDL_GLContext m_gl_context{nullptr};

    bool m_visible{false};
    SDL_Keycode m_toggle_key{SDLK_F3};

    bool m_imgui_context_created{false};
    bool m_initialized{false};
    bool m_frame_active{false};
    bool m_hook_installed{false};

    // Time control
    float m_time_scale{1.0f};
    bool m_paused{false};
    bool m_step_frame_requested{false};

    // Profiler metrics
    std::vector<float> m_frame_times;
    float m_fps_accumulator{0.0f};
    int m_fps_counter{0};
    float m_current_fps{60.0f};
    float m_dt{0.016f};

    // Console
    struct LogEntry {
        std::string text;
        bool is_error;
    };
    std::deque<LogEntry> m_logs;
    char m_input_buffer[512]{0};
    std::vector<std::string> m_history;
    int m_history_pos{-1};
    bool m_scroll_to_bottom{false};

    // Retro Visuals Tuning State
    bool m_crt_enabled{true};
    float m_scanline_strength{0.25f};
    float m_curvature{0.05f};
    float m_vignette{0.25f};
    bool m_dither_enabled{true};
    float m_clear_color[3]{0.08f, 0.12f, 0.16f};

    // Custom Tabs
    std::vector<CustomTab> m_custom_tabs;
};
