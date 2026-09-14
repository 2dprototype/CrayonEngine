#pragma once

#include <memory>
#include <vector>
#include <string>
#include "../src/core/engine.hpp"
#include "panels/editor_panel.hpp"
#include "panels/viewport_panel.hpp"
#include "panels/scene_panel.hpp"
#include "panels/inspector_panel.hpp"
#include "panels/asset_browser_panel.hpp"
#include "panels/retro_fx_panel.hpp"
#include "panels/console_panel.hpp"
#include "panels/stats_panel.hpp"

namespace crayon::editor {

class EditorApp {
public:
    EditorApp();
    ~EditorApp();

    bool init(int width = 630, int height = 450, const std::string& title = "Crayon Game Editor");
    void run();
    void shutdown();

private:
    void render_workspace();
    void render_menu_bar();

    crayon::Engine m_engine;

    std::shared_ptr<ViewportPanel> m_viewport_panel;
    std::shared_ptr<ScenePanel> m_scene_panel;
    std::shared_ptr<InspectorPanel> m_inspector_panel;
    std::shared_ptr<AssetBrowserPanel> m_asset_panel;
    std::shared_ptr<RetroFXPanel> m_retro_panel;
    std::shared_ptr<ConsolePanel> m_console_panel;
    std::shared_ptr<StatsPanel> m_stats_panel;

    std::vector<std::shared_ptr<EditorPanel>> m_panels;

    bool m_first_frame = true;
    bool m_running = false;
    bool m_demo_window = false;
};

} // namespace crayon::editor
