#include "editor_app.hpp"
#include "editor_theme.hpp"
#include "scene_context.hpp"
#include "../src/core/log.hpp"
#include "../src/scripting/lua_runtime.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <algorithm>

namespace crayon::editor {

EditorApp::EditorApp() = default;

EditorApp::~EditorApp() {
    shutdown();
}

bool EditorApp::init(int width, int height, const std::string& title) {
    // 1. Initialize Engine subsystems with starting resolution (630x450, resizable)
    // We intentionally DO NOT load main.lua for the scene editor!
    if (!m_engine.init(width, height, 480, 360, title)) {
        CRAYON_LOG_ERROR("EditorApp failed to initialize Engine");
        return false;
    }

    // 2. Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Apply modern dark theme
    apply_modern_dark_theme();

    // 3. Setup Platform/Renderer backends
    SDL_Window* sdl_win = m_engine.get_window().get_sdl_window();
    SDL_GLContext gl_ctx = m_engine.get_window().get_gl_context();

    if (!ImGui_ImplSDL3_InitForOpenGL(sdl_win, gl_ctx)) {
        CRAYON_LOG_ERROR("Failed to initialize ImGui SDL3 backend");
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        CRAYON_LOG_ERROR("Failed to initialize ImGui OpenGL3 backend");
        return false;
    }

    // 4. Instantiate 3D scene editing panels
    m_viewport_panel = std::make_shared<ViewportPanel>(m_engine, m_camera);
    m_scene_panel = std::make_shared<ScenePanel>();
    m_inspector_panel = std::make_shared<InspectorPanel>();
    m_asset_panel = std::make_shared<AssetBrowserPanel>(m_engine);

    m_running = true;
    CRAYON_LOG_INFO("Crayon 3D Scene Editor initialized successfully ({}x{})", width, height);
    return true;
}

void EditorApp::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                SceneContext::get().reset_to_default_scene();
                CRAYON_LOG_INFO("Reset to default 3D scene");
            }
            if (ImGui::MenuItem("Open Scene (.scene.lua)...", "Ctrl+O")) {
                SceneContext::get().load_from_lua(SceneContext::get().get_scene_path(), m_engine.get_lua_runtime());
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                SceneContext::get().save_to_lua(SceneContext::get().get_scene_path());
            }
            if (ImGui::MenuItem("Save Scene As...")) {
                m_show_save_modal = true;
                snprintf(m_save_path_buf, sizeof(m_save_path_buf), "%s", SceneContext::get().get_scene_path().c_str());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                m_running = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add 3D")) {
            if (ImGui::MenuItem("Cube Primitive")) {
                SceneEntity e;
                e.name = "Cube";
                e.type = "cube";
                e.position = glm::vec3(0.0f, 0.5f, 0.0f);
                SceneContext::get().add_entity(e);
            }
            if (ImGui::MenuItem("Ground Plane")) {
                SceneEntity e;
                e.name = "Plane";
                e.type = "plane";
                e.scale = glm::vec3(10.0f, 1.0f, 10.0f);
                e.position = glm::vec3(0.0f, 0.0f, 0.0f);
                SceneContext::get().add_entity(e);
            }
            if (ImGui::MenuItem("Sphere Primitive")) {
                SceneEntity e;
                e.name = "Sphere";
                e.type = "sphere";
                e.position = glm::vec3(0.0f, 0.5f, 0.0f);
                SceneContext::get().add_entity(e);
            }
            if (ImGui::MenuItem("Cylinder Primitive")) {
                SceneEntity e;
                e.name = "Cylinder";
                e.type = "cylinder";
                e.position = glm::vec3(0.0f, 0.5f, 0.0f);
                SceneContext::get().add_entity(e);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene Hierarchy", nullptr, &m_scene_panel->is_open());
            ImGui::MenuItem("3D Viewport", nullptr, &m_viewport_panel->is_open());
            ImGui::MenuItem("Object Inspector", nullptr, &m_inspector_panel->is_open());
            ImGui::MenuItem("Model Browser", nullptr, &m_asset_panel->is_open());
            ImGui::EndMenu();
        }

        // Display current map name in menu bar
        ImGui::SameLine(ImGui::GetWindowWidth() - 220.0f);
        ImGui::TextDisabled("Map: %s", SceneContext::get().get_scene_name().c_str());

        ImGui::EndMainMenuBar();
    }

    // Save As Modal Dialog
    if (m_show_save_modal) {
        ImGui::OpenPopup("Save Scene As##Modal");
    }

    if (ImGui::BeginPopupModal("Save Scene As##Modal", &m_show_save_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter scene file path to save (.scene.lua):");
        ImGui::InputText("##savepath", m_save_path_buf, sizeof(m_save_path_buf));
        ImGui::Separator();

        if (ImGui::Button("Save##btn", ImVec2(100, 0))) {
            SceneContext::get().save_to_lua(m_save_path_buf);
            m_show_save_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##btn", ImVec2(100, 0))) {
            m_show_save_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorApp::render_scene_fbo() {
    auto& fbo = m_engine.get_fbo();
    int virt_w = m_engine.get_window().get_virtual_width();
    int virt_h = m_engine.get_window().get_virtual_height();
    if (fbo.get_width() != virt_w || fbo.get_height() != virt_h) {
        fbo.resize(virt_w, virt_h);
    }

    fbo.bind();
    glViewport(0, 0, virt_w, virt_h);

    glClearColor(0.10f, 0.11f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    float aspect = static_cast<float>(virt_w) / static_cast<float>(virt_h);

    crayon::Camera cam = m_camera.to_crayon_camera();
    auto& renderer = m_engine.get_mesh_renderer();
    auto& batch2d = m_engine.get_batch2d();

    renderer.begin(cam, aspect);
    batch2d.begin(virt_w, virt_h);

    // 1. Draw 3D floor grid & axes
    if (m_viewport_panel->is_grid_enabled()) {
        renderer.draw_grid_3d(40.0f, 40, 0.0f, glm::vec4(0.35f, 0.35f, 0.45f, 0.6f));
        renderer.draw_axes_3d(glm::vec3(0.0f, 0.01f, 0.0f), 2.0f);
    }

    // 2. Render all 3D scene objects and selection wireframes
    SceneContext::get().render_scene_3d(renderer, m_engine);

    batch2d.end();
    renderer.end();

    fbo.unbind();
}

void EditorApp::render_workspace(float dt) {
    // 1. Update camera movement from viewport input
    m_viewport_panel->handle_input(dt);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 pos = vp->WorkPos;
    ImVec2 size = vp->WorkSize;

    // Responsive panel dimensions
    float left_w = std::max(120.0f, std::min(size.x * 0.22f, 260.0f));
    float right_w = std::max(160.0f, std::min(size.x * 0.30f, 360.0f));
    float bottom_h = std::max(90.0f, std::min(size.y * 0.30f, 240.0f));

    float center_w = size.x - left_w - right_w;
    float center_h = size.y - bottom_h;

    ImGuiWindowFlags tile_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Left Pane: Scene Hierarchy Tree
    if (m_scene_panel->is_open()) {
        ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y));
        ImGui::SetNextWindowSize(ImVec2(left_w, center_h));
        if (ImGui::Begin(m_scene_panel->get_title().c_str(), &m_scene_panel->is_open(), tile_flags)) {
            m_scene_panel->render_content();
        }
        ImGui::End();
    }

    // Center Pane: 3D Scene Viewport
    if (m_viewport_panel->is_open()) {
        ImGui::SetNextWindowPos(ImVec2(pos.x + left_w, pos.y));
        ImGui::SetNextWindowSize(ImVec2(center_w, center_h));
        m_viewport_panel->on_render();
    }

    // Right Pane: 3D Object Inspector
    if (m_inspector_panel->is_open()) {
        ImGui::SetNextWindowPos(ImVec2(pos.x + left_w + center_w, pos.y));
        ImGui::SetNextWindowSize(ImVec2(right_w, center_h));
        if (ImGui::Begin(m_inspector_panel->get_title().c_str(), &m_inspector_panel->is_open(), tile_flags)) {
            m_inspector_panel->render_content();
        }
        ImGui::End();
    }

    // Bottom Pane: 3D Model & Scene Asset Picker
    if (m_asset_panel->is_open()) {
        ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y + center_h));
        ImGui::SetNextWindowSize(ImVec2(size.x, bottom_h));
        if (ImGui::Begin(m_asset_panel->get_title().c_str(), &m_asset_panel->is_open(), tile_flags)) {
            m_asset_panel->render_content();
        }
        ImGui::End();
    }
}

void EditorApp::run() {
    using clock = std::chrono::high_resolution_clock;
    auto prev_time = clock::now();

    while (m_running && !m_engine.get_window().should_close()) {
        auto current_time = clock::now();
        std::chrono::duration<double> elapsed = current_time - prev_time;
        prev_time = current_time;

        float dt = static_cast<float>(elapsed.count());
        if (dt > 0.1f) dt = 0.1f;

        // Process SDL Events
        m_engine.get_input().begin_frame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            m_engine.get_window().handle_event(event);
        }

        // 1. Render 3D scene to FBO using EditorCamera
        render_scene_fbo();

        // 2. Begin ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Setup 3D Editor Menu & Workspace
        render_menu_bar();
        render_workspace(dt);

        // 3. Render ImGui to physical window
        ImGui::Render();

        int win_w = 0, win_h = 0;
        m_engine.get_window().get_window_size(win_w, win_h);
        glViewport(0, 0, win_w, win_h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        m_engine.get_window().swap_buffers();
    }
}

void EditorApp::shutdown() {
    if (!m_running) return;
    m_running = false;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    m_engine.shutdown();
}

} // namespace crayon::editor
