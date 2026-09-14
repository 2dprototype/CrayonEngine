#pragma once

#include "editor_panel.hpp"
#include <imgui.h>

namespace crayon {
    class Engine;
}

namespace crayon::editor {

enum class PlayState {
    Edit,
    Play,
    Pause
};

class ViewportPanel : public EditorPanel {
public:
    explicit ViewportPanel(crayon::Engine& engine);
    ~ViewportPanel() override = default;

    void on_render() override;

    PlayState get_play_state() const { return m_play_state; }
    void set_play_state(PlayState state);

    bool is_hovered() const { return m_is_hovered; }
    bool is_focused() const { return m_is_focused; }

    // Map screen mouse pos to virtual canvas pos
    bool get_virtual_mouse_pos(float& out_vx, float& out_vy) const;

private:
    void render_toolbar();
    void render_canvas();

    crayon::Engine& m_engine;
    PlayState m_play_state = PlayState::Play;

    ImVec2 m_viewport_pos{0.0f, 0.0f};
    ImVec2 m_viewport_size{0.0f, 0.0f};
    ImVec2 m_image_pos{0.0f, 0.0f};
    ImVec2 m_image_size{0.0f, 0.0f};

    bool m_is_hovered = false;
    bool m_is_focused = false;
    bool m_aspect_locked = true;
    bool m_pixel_perfect = false;
    bool m_step_requested = false;
};

} // namespace crayon::editor
