#include "inspector_panel.hpp"
#include "../scene_context.hpp"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <format>

namespace crayon::editor {

InspectorPanel::InspectorPanel()
    : EditorPanel("Inspector") {}

void InspectorPanel::render_transform_control(const char* label, float* values, float reset_val) {
    ImGui::PushID(label);

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 80.0f);
    ImGui::Text("%s", label);
    ImGui::NextColumn();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 2.0f));

    float line_height = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 btn_size = ImVec2(line_height, line_height);
    float avail_w = ImGui::GetContentRegionAvail().x;
    float item_w = std::max(35.0f, (avail_w - (btn_size.x + 3.0f) * 3.0f) / 3.0f);

    // X
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.30f, 1.0f));
    if (ImGui::Button("X", btn_size)) values[0] = reset_val;
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(item_w);
    ImGui::DragFloat("##X", &values[0], 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::SameLine();

    // Y
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.75f, 0.30f, 1.0f));
    if (ImGui::Button("Y", btn_size)) values[1] = reset_val;
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(item_w);
    ImGui::DragFloat("##Y", &values[1], 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::SameLine();

    // Z
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.35f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.45f, 0.95f, 1.0f));
    if (ImGui::Button("Z", btn_size)) values[2] = reset_val;
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(item_w);
    ImGui::DragFloat("##Z", &values[2], 0.1f, 0.0f, 0.0f, "%.2f");

    ImGui::PopStyleVar();
    ImGui::Columns(1);
    ImGui::PopID();
}

void InspectorPanel::on_render() {
    if (!m_is_open) return;

    if (ImGui::Begin(m_title.c_str(), &m_is_open)) {
        render_content();
    }
    ImGui::End();
}

void InspectorPanel::render_content() {
    SceneObject* obj = SceneContext::get().get_selected_object();
    if (!obj) {
        ImGui::TextDisabled("No entity selected.");
        ImGui::TextWrapped("Select an entity from the Scene Hierarchy to view its components and properties.");
        return;
    }

        // Entity Header
        ImGui::InputText("Name", &obj->name);
        ImGui::Checkbox("Visible", &obj->visible);
        ImGui::SameLine();
        ImGui::Checkbox("Wireframe", &obj->wireframe);

        ImGui::Separator();

        // Transform Component
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            render_transform_control("Position", &obj->position.x, 0.0f);
            render_transform_control("Rotation", &obj->rotation.x, 0.0f);
            render_transform_control("Scale", &obj->scale.x, 1.0f);
        }

        // Mesh / Visual Component
        if (ImGui::CollapsingHeader("Visual & Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit4("Color Tint", &obj->color.r);
            ImGui::InputText("Asset Path", &obj->asset_path);
        }

        // Physics Component
        if (ImGui::CollapsingHeader("Physics Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable Physics", &obj->has_physics);
            if (obj->has_physics) {
                const char* types[] = { "Static", "Dynamic", "Kinematic" };
                ImGui::Combo("Body Type", &obj->physics_type, types, 3);
                if (obj->physics_type == 1) { // Dynamic
                    ImGui::DragFloat("Mass (kg)", &obj->mass, 0.1f, 0.01f, 1000.0f);
                }
                ImGui::SliderFloat("Friction", &obj->friction, 0.0f, 1.0f);
                ImGui::SliderFloat("Restitution", &obj->restitution, 0.0f, 1.0f);
            }
        }

        ImGui::Separator();

        // Export to Lua snippet
        if (ImGui::Button("Copy Lua Entity Table", ImVec2(-1, 0))) {
            std::string code = std::format(
                "local entity = {{\n"
                "    name = \"{}\",\n"
                "    pos = {{ {:.2f}, {:.2f}, {:.2f} }},\n"
                "    rot = {{ {:.2f}, {:.2f}, {:.2f} }},\n"
                "    scale = {{ {:.2f}, {:.2f}, {:.2f} }},\n"
                "    color = {{ {:.2f}, {:.2f}, {:.2f}, {:.2f} }},\n"
                "    asset = \"{}\",\n"
                "}}",
                obj->name,
                obj->position.x, obj->position.y, obj->position.z,
                obj->rotation.x, obj->rotation.y, obj->rotation.z,
                obj->scale.x, obj->scale.y, obj->scale.z,
                obj->color.r, obj->color.g, obj->color.b, obj->color.a,
                obj->asset_path
            );
            ImGui::SetClipboardText(code.c_str());
        }
}

} // namespace crayon::editor
