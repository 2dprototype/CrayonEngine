#pragma once

#include "editor_panel.hpp"
#include <filesystem>

namespace crayon {
    class Engine;
}

namespace crayon::editor {

class AssetBrowserPanel : public EditorPanel {
public:
    explicit AssetBrowserPanel(crayon::Engine& engine);
    ~AssetBrowserPanel() override = default;

    void on_render() override;
    void render_content() override;

private:
    crayon::Engine& m_engine;
    std::filesystem::path m_current_path;
    std::filesystem::path m_root_path;
    char m_search_filter[64] = "";
};

} // namespace crayon::editor
