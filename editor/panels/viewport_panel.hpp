#pragma once

#include "editor_panel.hpp"
#include "../editor_camera.hpp"
#include <imgui.h>

namespace crayon {
    class Engine;
}

namespace crayon::editor {

class ViewportPanel : public EditorPanel {
public:
    ViewportPanel(crayon::Engine& engine, EditorCamera& camera);
    ~ViewportPanel() override = default;

    void on_render() override;

    bool is_hovered() const { return m_is_hovered; }
    bool is_focused() const { return m_is_focused; }
    bool is_grid_enabled() const { return m_show_grid; }

    void handle_input(float dt);

private:
    void render_toolbar();
    void render_canvas();

    crayon::Engine& m_engine;
    EditorCamera& m_camera;

    ImVec2 m_viewport_pos{0.0f, 0.0f};
    ImVec2 m_viewport_size{0.0f, 0.0f};
    ImVec2 m_image_pos{0.0f, 0.0f};
    ImVec2 m_image_size{0.0f, 0.0f};

    bool m_is_hovered = false;
    bool m_is_focused = false;
    bool m_show_grid = true;
};

} // namespace crayon::editor
