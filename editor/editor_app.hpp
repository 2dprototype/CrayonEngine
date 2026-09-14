#pragma once

#include <memory>
#include <vector>
#include <string>
#include "../src/core/engine.hpp"
#include "editor_camera.hpp"
#include "panels/editor_panel.hpp"
#include "panels/viewport_panel.hpp"
#include "panels/scene_panel.hpp"
#include "panels/inspector_panel.hpp"
#include "panels/asset_browser_panel.hpp"

namespace crayon::editor {

class EditorApp {
public:
    EditorApp();
    ~EditorApp();

    bool init(int width = 630, int height = 450, const std::string& title = "Crayon 3D Scene Editor");
    void run();
    void shutdown();

private:
    void render_menu_bar();
    void render_workspace(float dt);
    void render_scene_fbo();

    crayon::Engine m_engine;
    EditorCamera m_camera;

    std::shared_ptr<ViewportPanel> m_viewport_panel;
    std::shared_ptr<ScenePanel> m_scene_panel;
    std::shared_ptr<InspectorPanel> m_inspector_panel;
    std::shared_ptr<AssetBrowserPanel> m_asset_panel;

    bool m_running = false;
    bool m_show_save_modal = false;
    char m_save_path_buf[128] = "game/scenes/map1.scene.lua";
};

} // namespace crayon::editor
