#pragma once

#include "editor_panel.hpp"

namespace crayon {
    class Engine;
}

namespace crayon::editor {

class RetroFXPanel : public EditorPanel {
public:
    explicit RetroFXPanel(crayon::Engine& engine);
    ~RetroFXPanel() override = default;

    void on_render() override;
    void render_content() override;

private:
    crayon::Engine& m_engine;
};

} // namespace crayon::editor
