#include "viewport_panel.hpp"
#include "../scene_context.hpp"
#include "../../src/core/engine.hpp"
#include <algorithm>

namespace crayon::editor {

ViewportPanel::ViewportPanel(crayon::Engine& engine, EditorCamera& camera)
    : EditorPanel("3D Viewport"), m_engine(engine), m_camera(camera) {}

void ViewportPanel::handle_input(float dt) {
    if (!m_is_hovered) return;

    ImGuiIO& io = ImGui::GetIO();

    // Mouse scroll zooming
    if (io.MouseWheel != 0.0f) {
        m_camera.process_mouse_scroll(io.MouseWheel);
    }

    // Middle mouse button panning
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        m_camera.process_mouse_pan(io.MouseDelta.x, io.MouseDelta.y);
    }

    // Right mouse button free-look + WASD fly
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        m_camera.process_mouse_movement(io.MouseDelta.x, io.MouseDelta.y);

        if (ImGui::IsKeyDown(ImGuiKey_W)) m_camera.process_keyboard(0, dt);
        if (ImGui::IsKeyDown(ImGuiKey_S)) m_camera.process_keyboard(1, dt);
        if (ImGui::IsKeyDown(ImGuiKey_A)) m_camera.process_keyboard(2, dt);
        if (ImGui::IsKeyDown(ImGuiKey_D)) m_camera.process_keyboard(3, dt);
        if (ImGui::IsKeyDown(ImGuiKey_E)) m_camera.process_keyboard(4, dt);
        if (ImGui::IsKeyDown(ImGuiKey_Q)) m_camera.process_keyboard(5, dt);
    }
}

void ViewportPanel::render_toolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

    if (ImGui::Button("+ Cube")) {
        SceneEntity e;
        e.name = "Cube";
        e.type = "cube";
        e.position = glm::vec3(0.0f, 0.5f, 0.0f);
        SceneContext::get().add_entity(e);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Plane")) {
        SceneEntity e;
        e.name = "Plane";
        e.type = "plane";
        e.scale = glm::vec3(10.0f, 1.0f, 10.0f);
        e.position = glm::vec3(0.0f, 0.0f, 0.0f);
        SceneContext::get().add_entity(e);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Sphere")) {
        SceneEntity e;
        e.name = "Sphere";
        e.type = "sphere";
        e.position = glm::vec3(0.0f, 0.5f, 0.0f);
        SceneContext::get().add_entity(e);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Cylinder")) {
        SceneEntity e;
        e.name = "Cylinder";
        e.type = "cylinder";
        e.position = glm::vec3(0.0f, 0.5f, 0.0f);
        SceneContext::get().add_entity(e);
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::Checkbox("Grid", &m_show_grid);
    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        m_camera.reset();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::DragFloat("##speed", &m_camera.get_move_speed(), 0.5f, 1.0f, 50.0f, "Spd: %.0f");

    ImGui::SameLine();
    const glm::vec3& cam_p = m_camera.get_position();
    ImGui::TextDisabled("| Cam: [%.1f, %.1f, %.1f]", cam_p.x, cam_p.y, cam_p.z);

    ImGui::PopStyleVar(2);
    ImGui::Separator();
}

void ViewportPanel::render_canvas() {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 32.0f) avail.x = 32.0f;
    if (avail.y < 32.0f) avail.y = 32.0f;

    m_image_pos = ImGui::GetCursorScreenPos();
    m_image_size = avail;

    GLuint tex_id = m_engine.get_fbo().get_color_texture();
    // Invert Y for OpenGL FBO texture in ImGui
    ImGui::Image((ImTextureID)(intptr_t)tex_id, m_image_size, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

    m_is_hovered = ImGui::IsItemHovered();
    m_is_focused = ImGui::IsWindowFocused();

    // Border around active canvas
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(m_image_pos, ImVec2(m_image_pos.x + m_image_size.x, m_image_pos.y + m_image_size.y),
                       IM_COL32(70, 75, 90, 180), 2.0f, 0, 1.0f);
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

} // namespace crayon::editor
