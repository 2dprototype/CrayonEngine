#include "inspector_panel.hpp"
#include "../scene_context.hpp"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <format>
#include <algorithm>

namespace crayon::editor {

InspectorPanel::InspectorPanel()
    : EditorPanel("Object Inspector") {}

void InspectorPanel::render_transform_control(const char* label, float* values, float reset_val) {
    ImGui::PushID(label);

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 75.0f);
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
    SceneEntity* entity = SceneContext::get().get_selected_entity();
    if (!entity) {
        ImGui::TextDisabled("No 3D object selected.");
        ImGui::TextWrapped("Select an object from the Scene Hierarchy to inspect its 3D transform, model, and collider.");
        return;
    }

    // Entity Header
    ImGui::InputText("Name", &entity->name);
    ImGui::Checkbox("Visible", &entity->visible);

    ImGui::Separator();

    // Transform Component
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        render_transform_control("Position", &entity->position.x, 0.0f);
        render_transform_control("Rotation", &entity->rotation.x, 0.0f);
        render_transform_control("Scale", &entity->scale.x, 1.0f);
    }

    // Mesh / Model Component
    if (ImGui::CollapsingHeader("Mesh & Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* types[] = { "cube", "plane", "sphere", "cylinder", "model" };
        int current_type = 0;
        for (int i = 0; i < 5; ++i) {
            if (entity->type == types[i]) {
                current_type = i;
                break;
            }
        }
        if (ImGui::Combo("Geometry Type", &current_type, types, 5)) {
            entity->type = types[current_type];
        }

        if (entity->type == "model") {
            ImGui::InputText("Asset Path", &entity->asset_path);
            ImGui::TextDisabled("Path to .obj, .gltf, or .glb");
        }

        ImGui::ColorEdit4("Color Tint", &entity->color.r);
    }

    // Physics / Collision Component
    if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Collider", &entity->has_collider);
        if (entity->has_collider) {
            const char* col_types[] = { "box", "sphere", "plane" };
            int current_col = 0;
            for (int i = 0; i < 3; ++i) {
                if (entity->collider_type == col_types[i]) {
                    current_col = i;
                    break;
                }
            }
            if (ImGui::Combo("Collider Shape", &current_col, col_types, 3)) {
                entity->collider_type = col_types[current_col];
            }

            const char* motions[] = { "static", "dynamic" };
            int current_motion = (entity->collider_motion == "dynamic") ? 1 : 0;
            if (ImGui::Combo("Motion Type", &current_motion, motions, 2)) {
                entity->collider_motion = motions[current_motion];
            }
        }
    }

    ImGui::Separator();

    // Export to Lua snippet button
    if (ImGui::Button("Copy Lua Object Table", ImVec2(-1, 0))) {
        std::string code = std::format(
            "local obj = {{\n"
            "    name = \"{}\",\n"
            "    type = \"{}\",\n"
            "    asset = \"{}\",\n"
            "    pos = {{ {:.2f}, {:.2f}, {:.2f} }},\n"
            "    rot = {{ {:.2f}, {:.2f}, {:.2f} }},\n"
            "    scale = {{ {:.2f}, {:.2f}, {:.2f} }},\n"
            "    color = {{ {:.2f}, {:.2f}, {:.2f}, {:.2f} }},\n"
            "    has_collider = {},\n"
            "    collider_type = \"{}\",\n"
            "    collider_motion = \"{}\"\n"
            "}}",
            entity->name,
            entity->type,
            entity->asset_path,
            entity->position.x, entity->position.y, entity->position.z,
            entity->rotation.x, entity->rotation.y, entity->rotation.z,
            entity->scale.x, entity->scale.y, entity->scale.z,
            entity->color.r, entity->color.g, entity->color.b, entity->color.a,
            entity->has_collider ? "true" : "false",
            entity->collider_type,
            entity->collider_motion
        );
        ImGui::SetClipboardText(code.c_str());
    }
}

} // namespace crayon::editor
