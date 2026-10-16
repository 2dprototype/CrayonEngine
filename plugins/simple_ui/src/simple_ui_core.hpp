#pragma once

#include <crayon_plugin.h>
#include <SDL3/SDL.h>
#include <imgui.h>

class SimpleUI {
public:
    static SimpleUI& instance();

    bool init(const CrayonHostAPI* host);
    void shutdown();

    void on_update(float dt);
    void on_pre_swap(SDL_Window* window);
    bool handle_event(const SDL_Event* event);

    bool is_initialized() const { return m_initialized; }
    bool has_active_frame() const { return m_frame_active; }
    bool want_capture_mouse() const;
    bool want_capture_keyboard() const;
    void ensure_frame_started();

    void log(int level, const char* msg);

private:
    SimpleUI();
    ~SimpleUI();

    void apply_theme();
    bool install_hooks();
    void remove_hooks();

    const CrayonHostAPI* m_host{nullptr};
    SDL_Window* m_window{nullptr};
    SDL_GLContext m_gl_context{nullptr};

    bool m_imgui_context_created{false};
    bool m_initialized{false};
    bool m_frame_active{false};
    bool m_hook_installed{false};
};
