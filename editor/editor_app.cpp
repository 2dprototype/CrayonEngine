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
    // Virtual game canvas default is 320x240 retro resolution
    if (!m_engine.init(width, height, 320, 240, title)) {
        CRAYON_LOG_ERROR("EditorApp failed to initialize Engine");
        return false;
    }

    // Default to game/main.lua if present
    if (std::filesystem::exists("game/main.lua")) {
        m_engine.set_game_script_path("game/main.lua");
        m_engine.request_hot_reload();
    }

    // 2. Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Apply sleek, modern dark theme
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

    // 4. Instantiate panels
    m_viewport_panel = std::make_shared<ViewportPanel>(m_engine);
    m_scene_panel = std::make_shared<ScenePanel>();
    m_inspector_panel = std::make_shared<InspectorPanel>();
    m_asset_panel = std::make_shared<AssetBrowserPanel>(m_engine);
    m_retro_panel = std::make_shared<RetroFXPanel>(m_engine);
    m_console_panel = std::make_shared<ConsolePanel>(m_engine);
    m_stats_panel = std::make_shared<StatsPanel>(m_engine);

    m_panels.push_back(m_viewport_panel);
    m_panels.push_back(m_scene_panel);
    m_panels.push_back(m_inspector_panel);
    m_panels.push_back(m_asset_panel);
    m_panels.push_back(m_retro_panel);
    m_panels.push_back(m_console_panel);
    m_panels.push_back(m_stats_panel);

    m_running = true;
    CRAYON_LOG_INFO("Crayon Game Editor initialized successfully ({}x{})", width, height);
    return true;
}

void EditorApp::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                SceneContext::get().populate_default_retro_scene();
                CRAYON_LOG_INFO("Reset to default retro scene");
            }
            if (ImGui::MenuItem("Save Scene (Lua)", "Ctrl+S")) {
                CRAYON_LOG_INFO("Scene saved to game/scenes/main.scene.lua");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                m_running = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            for (auto& panel : m_panels) {
                ImGui::MenuItem(panel->get_title().c_str(), nullptr, &panel->is_open());
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo Window", nullptr, &m_demo_window);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Game")) {
            if (ImGui::MenuItem("Play / Resume", "F6", m_viewport_panel->get_play_state() == PlayState::Play)) {
                m_viewport_panel->set_play_state(PlayState::Play);
            }
            if (ImGui::MenuItem("Pause", "F7", m_viewport_panel->get_play_state() == PlayState::Pause)) {
                m_viewport_panel->set_play_state(PlayState::Pause);
            }
            if (ImGui::MenuItem("Step Frame", "F8")) {
                m_viewport_panel->set_play_state(PlayState::Pause);
                m_engine.step_simulation(1.0f / 60.0f);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Hot Reload Script", "F5")) {
                m_engine.request_hot_reload();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Crayon Engine")) {
                CRAYON_LOG_INFO("Crayon Retro 2D+3D Game Engine (modern C++20 + LuaJIT)");
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorApp::render_workspace() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 pos = vp->WorkPos;
    ImVec2 size = vp->WorkSize;

    // Responsive panel dimensions
    float left_w = std::max(120.0f, std::min(size.x * 0.22f, 280.0f));
    float right_w = std::max(150.0f, std::min(size.x * 0.28f, 360.0f));
    float bottom_h = std::max(100.0f, std::min(size.y * 0.32f, 260.0f));

    float center_w = size.x - left_w - right_w;
    float center_h = size.y - bottom_h;

    ImGuiWindowFlags tile_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    // 1. Left Window: Scene Hierarchy
    if (m_scene_panel->is_open()) {
        ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y));
        ImGui::SetNextWindowSize(ImVec2(left_w, center_h));
        if (ImGui::Begin(m_scene_panel->get_title().c_str(), &m_scene_panel->is_open(), tile_flags)) {
            m_scene_panel->render_content();
        }
        ImGui::End();
    }

    // 2. Center Window: Viewport
    if (m_viewport_panel->is_open()) {
        ImGui::SetNextWindowPos(ImVec2(pos.x + left_w, pos.y));
        ImGui::SetNextWindowSize(ImVec2(center_w, center_h));
        m_viewport_panel->on_render();
    }

    // 3. Right Window: Tabbed (Inspector & Retro FX)
    ImGui::SetNextWindowPos(ImVec2(pos.x + left_w + center_w, pos.y));
    ImGui::SetNextWindowSize(ImVec2(right_w, center_h));
    if (ImGui::Begin("Inspector & FX##RightDock", nullptr, tile_flags)) {
        if (ImGui::BeginTabBar("RightTabBar")) {
            if (m_inspector_panel->is_open() && ImGui::BeginTabItem("Inspector")) {
                m_inspector_panel->render_content();
                ImGui::EndTabItem();
            }
            if (m_retro_panel->is_open() && ImGui::BeginTabItem("Retro FX Tuner")) {
                m_retro_panel->render_content();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    // 4. Bottom Window: Tabbed (Asset Browser, Console, Stats)
    ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y + center_h));
    ImGui::SetNextWindowSize(ImVec2(size.x, bottom_h));
    if (ImGui::Begin("Tools & Assets##BottomDock", nullptr, tile_flags)) {
        if (ImGui::BeginTabBar("BottomTabBar")) {
            if (m_asset_panel->is_open() && ImGui::BeginTabItem("Asset Browser")) {
                m_asset_panel->render_content();
                ImGui::EndTabItem();
            }
            if (m_console_panel->is_open() && ImGui::BeginTabItem("Console")) {
                m_console_panel->render_content();
                ImGui::EndTabItem();
            }
            if (m_stats_panel->is_open() && ImGui::BeginTabItem("Profiler & Stats")) {
                m_stats_panel->render_content();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
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

        // Hot Reload check
        if (m_engine.check_hot_reload()) {
            CRAYON_LOG_INFO("Hot-reloading script: {}", m_engine.get_game_script_path());
            m_engine.get_lua_runtime().reload_script(m_engine.get_game_script_path());
        }

        // Process Window & Input events
        m_engine.get_input().begin_frame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            m_engine.get_window().handle_event(event);

            // Forward to game input if viewport is focused or playing
            bool game_active = (m_viewport_panel->get_play_state() == PlayState::Play);
            if (game_active && m_viewport_panel->is_hovered()) {
                m_engine.get_input().handle_event(event, m_engine.get_window());

                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    float vx = 0, vy = 0;
                    if (m_viewport_panel->get_virtual_mouse_pos(vx, vy)) {
                        int btn = 1;
                        if (event.button.button == SDL_BUTTON_LEFT) btn = 1;
                        else if (event.button.button == SDL_BUTTON_RIGHT) btn = 2;
                        else if (event.button.button == SDL_BUTTON_MIDDLE) btn = 3;
                        m_engine.get_lua_runtime().call_mouse_down(vx, vy, btn);
                    }
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    float vx = 0, vy = 0;
                    if (m_viewport_panel->get_virtual_mouse_pos(vx, vy)) {
                        int btn = 1;
                        if (event.button.button == SDL_BUTTON_LEFT) btn = 1;
                        else if (event.button.button == SDL_BUTTON_RIGHT) btn = 2;
                        else if (event.button.button == SDL_BUTTON_MIDDLE) btn = 3;
                        m_engine.get_lua_runtime().call_mouse_up(vx, vy, btn);
                    }
                } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    float vx = 0, vy = 0;
                    if (m_viewport_panel->get_virtual_mouse_pos(vx, vy)) {
                        m_engine.get_lua_runtime().call_mouse_moved(vx, vy, event.motion.xrel, event.motion.yrel);
                    }
                }
            }

            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F5) {
                m_engine.request_hot_reload();
            }
        }

        // Update simulation if in play mode
        if (m_viewport_panel->get_play_state() == PlayState::Play) {
            m_engine.step_simulation(dt);
        }

        // 1. Render game world to virtual FBO
        m_engine.render_to_fbo();

        // 2. Begin ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Setup Workspace & Menu
        render_menu_bar();
        render_workspace();

        if (m_demo_window) {
            ImGui::ShowDemoWindow(&m_demo_window);
        }

        // 3. Render ImGui to main window
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
