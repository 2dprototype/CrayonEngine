#pragma once

#include "editor_panel.hpp"

namespace crayon::editor {

class InspectorPanel : public EditorPanel {
public:
    InspectorPanel();
    ~InspectorPanel() override = default;

    void on_render() override;
    void render_content() override;

private:
    void render_transform_control(const char* label, float* values, float reset_val = 0.0f);
};

} // namespace crayon::editor
