#include "stats_panel.hpp"
#include "../../src/core/engine.hpp"
#include <imgui.h>

namespace crayon::editor {

StatsPanel::StatsPanel(crayon::Engine& engine)
    : EditorPanel("Profiler & Stats"), m_engine(engine), m_fps_history(100, 60.0f) {}

void StatsPanel::on_render() {
    if (!m_is_open) return;

    if (ImGui::Begin(m_title.c_str(), &m_is_open)) {
        render_content();
    }
    ImGui::End();
}

void StatsPanel::render_content() {
    float fps = m_engine.get_fps();
    float dt = m_engine.get_dt() * 1000.0f; // ms

    m_fps_history[m_history_index] = fps;
    m_history_index = (m_history_index + 1) % m_fps_history.size();

    // FPS Plot
    char overlay[32];
    snprintf(overlay, sizeof(overlay), "%.1f FPS (%.2f ms)", fps, dt);
    ImGui::PlotLines("##FPSPlot", m_fps_history.data(), (int)m_fps_history.size(),
                     m_history_index, overlay, 0.0f, 120.0f, ImVec2(-1, 60.0f));

    ImGui::Separator();

    // Display info
    int win_w = 0, win_h = 0;
    m_engine.get_window().get_window_size(win_w, win_h);
    int virt_w = m_engine.get_window().get_virtual_width();
    int virt_h = m_engine.get_window().get_virtual_height();

    if (ImGui::BeginTable("StatsTable", 2, ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Framerate");
        ImGui::TableNextColumn(); ImGui::Text("%.1f FPS", fps);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Frame Time");
        ImGui::TableNextColumn(); ImGui::Text("%.3f ms", dt);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Virtual Canvas");
        ImGui::TableNextColumn(); ImGui::Text("%d x %d", virt_w, virt_h);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Window Size");
        ImGui::TableNextColumn(); ImGui::Text("%d x %d", win_w, win_h);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Total Engine Time");
        ImGui::TableNextColumn(); ImGui::Text("%.1f s", m_engine.get_time());

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("VSync");
        ImGui::TableNextColumn(); ImGui::Text("%s", m_engine.get_window().get_vsync() ? "Enabled" : "Disabled");

        ImGui::EndTable();
    }
}

} // namespace crayon::editor
