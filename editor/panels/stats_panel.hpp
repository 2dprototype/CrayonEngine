#pragma once

#include "editor_panel.hpp"
#include <vector>

namespace crayon {
    class Engine;
}

namespace crayon::editor {

class StatsPanel : public EditorPanel {
public:
    explicit StatsPanel(crayon::Engine& engine);
    ~StatsPanel() override = default;

    void on_render() override;
    void render_content() override;

private:
    crayon::Engine& m_engine;
    std::vector<float> m_fps_history;
    int m_history_index = 0;
};

} // namespace crayon::editor
