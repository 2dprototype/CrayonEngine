#include "viewport_panel.hpp"
#include "../../src/core/engine.hpp"
#include "../../src/scripting/lua_runtime.hpp"
#include <algorithm>
#include <cmath>

namespace crayon::editor {

ViewportPanel::ViewportPanel(crayon::Engine& engine)
    : EditorPanel("Viewport"), m_engine(engine) {}

void ViewportPanel::set_play_state(PlayState state) {
    m_play_state = state;
    if (m_play_state == PlayState::Play) {
        m_engine.set_paused(false);
    } else if (m_play_state == PlayState::Pause) {
        m_engine.set_paused(true);
    } else { // Edit
        m_engine.set_paused(true);
    }
}

void ViewportPanel::render_toolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 4.0f));

    // Play / Pause / Step / Reload buttons
    bool is_playing = (m_play_state == PlayState::Play);
    bool is_paused = (m_play_state == PlayState::Pause);

    if (is_playing) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.25f, 1.0f));
    }
    if (ImGui::Button(is_playing ? "Playing##btn" : "Play##btn")) {
        set_play_state(PlayState::Play);
    }
    if (is_playing) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (is_paused) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.45f, 0.15f, 1.0f));
    }
    if (ImGui::Button(is_paused ? "Paused##btn" : "Pause##btn")) {
        set_play_state(PlayState::Pause);
    }
    if (is_paused) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::Button("Step##btn")) {
        set_play_state(PlayState::Pause);
        m_engine.step_simulation(1.0f / 60.0f);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reload (F5)##btn")) {
        m_engine.request_hot_reload();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::Checkbox("Aspect Fit", &m_aspect_locked);
    ImGui::SameLine();
    ImGui::Checkbox("Integer Scale", &m_pixel_perfect);

    // Resolution display
    int virt_w = m_engine.get_window().get_virtual_width();
    int virt_h = m_engine.get_window().get_virtual_height();
    ImGui::SameLine();
    ImGui::TextDisabled("| Canvas: %dx%d @ %.1f FPS", virt_w, virt_h, m_engine.get_fps());

    ImGui::PopStyleVar(2);
    ImGui::Separator();
}

void ViewportPanel::render_canvas() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 32.0f) avail.x = 32.0f;
    if (avail.y < 32.0f) avail.y = 32.0f;

    int virt_w = m_engine.get_window().get_virtual_width();
    int virt_h = m_engine.get_window().get_virtual_height();
    if (virt_w <= 0) virt_w = 320;
    if (virt_h <= 0) virt_h = 240;

    float aspect = static_cast<float>(virt_w) / static_cast<float>(virt_h);

    float target_w = avail.x;
    float target_h = avail.y;

    if (m_aspect_locked) {
        if (m_pixel_perfect) {
            int scale_x = static_cast<int>(avail.x / virt_w);
            int scale_y = static_cast<int>(avail.y / virt_h);
            int scale = std::max(1, std::min(scale_x, scale_y));
            target_w = static_cast<float>(virt_w * scale);
            target_h = static_cast<float>(virt_h * scale);
        } else {
            if (avail.x / avail.y > aspect) {
                target_w = avail.y * aspect;
                target_h = avail.y;
            } else {
                target_w = avail.x;
                target_h = avail.x / aspect;
            }
        }
    }

    // Centered placement
    float pad_x = (avail.x - target_w) * 0.5f;
    float pad_y = (avail.y - target_h) * 0.5f;

    ImVec2 cursor_start = ImGui::GetCursorScreenPos();
    m_viewport_pos = cursor_start;
    m_viewport_size = avail;

    m_image_pos = ImVec2(cursor_start.x + pad_x, cursor_start.y + pad_y);
    m_image_size = ImVec2(target_w, target_h);

    if (pad_x > 0.0f || pad_y > 0.0f) {
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + pad_x, ImGui::GetCursorPosY() + pad_y));
    }

    GLuint tex_id = m_engine.get_fbo().get_color_texture();
    // Invert Y for OpenGL FBO texture in ImGui
    ImGui::Image((ImTextureID)(intptr_t)tex_id, m_image_size, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

    m_is_hovered = ImGui::IsItemHovered();
    m_is_focused = ImGui::IsWindowFocused();

    // Border around active canvas
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(m_image_pos, ImVec2(m_image_pos.x + m_image_size.x, m_image_pos.y + m_image_size.y),
                       IM_COL32(80, 85, 100, 180), 3.0f, 0, 1.0f);
}

void ViewportPanel::on_render() {
    if (!m_is_open) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin(m_title.c_str(), &m_is_open, flags)) {
        render_toolbar();
        render_canvas();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

bool ViewportPanel::get_virtual_mouse_pos(float& out_vx, float& out_vy) const {
    if (!m_is_hovered || m_image_size.x <= 0.0f || m_image_size.y <= 0.0f) {
        return false;
    }

    ImVec2 mouse = ImGui::GetMousePos();
    float rel_x = mouse.x - m_image_pos.x;
    float rel_y = mouse.y - m_image_pos.y;

    if (rel_x < 0.0f || rel_x > m_image_size.x || rel_y < 0.0f || rel_y > m_image_size.y) {
        return false;
    }

    int virt_w = m_engine.get_window().get_virtual_width();
    int virt_h = m_engine.get_window().get_virtual_height();

    out_vx = (rel_x / m_image_size.x) * static_cast<float>(virt_w);
    out_vy = (rel_y / m_image_size.y) * static_cast<float>(virt_h);
    return true;
}

} // namespace crayon::editor
