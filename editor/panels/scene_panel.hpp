#pragma once

#include "editor_panel.hpp"

namespace crayon::editor {

class ScenePanel : public EditorPanel {
public:
    ScenePanel();
    ~ScenePanel() override = default;

    void on_render() override;
    void render_content() override;

private:
    char m_filter[64] = "";
};

} // namespace crayon::editor
