#include "scene_panel.hpp"
#include "../scene_context.hpp"
#include <imgui.h>
#include <string>

namespace crayon::editor {

ScenePanel::ScenePanel()
    : EditorPanel("Scene Hierarchy") {}

void ScenePanel::on_render() {
    if (!m_is_open) return;

    if (ImGui::Begin(m_title.c_str(), &m_is_open)) {
        render_content();
    }
    ImGui::End();
}

void ScenePanel::render_content() {
    // Search filter
    ImGui::InputTextWithHint("##filter", "Filter 3D objects...", m_filter, sizeof(m_filter));
    ImGui::Separator();

    // Add 3D Entity Button
    if (ImGui::Button("+ Add 3D Object", ImVec2(-1, 0))) {
        ImGui::OpenPopup("Add3DObjectPopup");
    }

    if (ImGui::BeginPopup("Add3DObjectPopup")) {
        if (ImGui::MenuItem("Cube")) {
            SceneEntity e;
            e.name = "Cube";
            e.type = "cube";
            e.position = glm::vec3(0.0f, 0.5f, 0.0f);
            SceneContext::get().add_entity(e);
        }
        if (ImGui::MenuItem("Plane")) {
            SceneEntity e;
            e.name = "Plane";
            e.type = "plane";
            e.scale = glm::vec3(10.0f, 1.0f, 10.0f);
            e.position = glm::vec3(0.0f, 0.0f, 0.0f);
            SceneContext::get().add_entity(e);
        }
        if (ImGui::MenuItem("Sphere")) {
            SceneEntity e;
            e.name = "Sphere";
            e.type = "sphere";
            e.position = glm::vec3(0.0f, 0.5f, 0.0f);
            SceneContext::get().add_entity(e);
        }
        if (ImGui::MenuItem("Cylinder")) {
            SceneEntity e;
            e.name = "Cylinder";
            e.type = "cylinder";
            e.position = glm::vec3(0.0f, 0.5f, 0.0f);
            SceneContext::get().add_entity(e);
        }
        if (ImGui::MenuItem("Custom Model (.obj / .gltf)")) {
            SceneEntity e;
            e.name = "CustomModel";
            e.type = "model";
            e.asset_path = "models/cube.obj";
            e.position = glm::vec3(0.0f, 0.5f, 0.0f);
            SceneContext::get().add_entity(e);
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Entity List
    auto& entities = SceneContext::get().get_entities();
    int selected_id = SceneContext::get().get_selected_id();
    int id_to_remove = -1;

    std::string filter_str = m_filter;
    for (auto& c : filter_str) c = (char)::tolower(c);

    for (size_t i = 0; i < entities.size(); ++i) {
        auto& entity = entities[i];

        if (!filter_str.empty()) {
            std::string lower_name = entity.name;
            for (auto& c : lower_name) c = (char)::tolower(c);
            if (lower_name.find(filter_str) == std::string::npos) {
                continue;
            }
        }

        ImGui::PushID(entity.id);

        const char* type_icon = "[3D]";
        if (entity.type == "plane") type_icon = "[PLN]";
        else if (entity.type == "sphere") type_icon = "[SPH]";
        else if (entity.type == "cylinder") type_icon = "[CYL]";
        else if (entity.type == "model") type_icon = "[MDL]";

        std::string label = std::string(type_icon) + " " + entity.name;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (entity.id == selected_id) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool opened = ImGui::TreeNodeEx((void*)(intptr_t)entity.id, flags, "%s", label.c_str());
        if (ImGui::IsItemClicked()) {
            SceneContext::get().set_selected_id(entity.id);
        }

        // Right click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Duplicate")) {
                SceneEntity dup = entity;
                dup.name += "_copy";
                dup.position += glm::vec3(1.0f, 0.0f, 1.0f);
                SceneContext::get().add_entity(dup);
            }
            if (ImGui::MenuItem("Delete")) {
                id_to_remove = entity.id;
            }
            ImGui::EndPopup();
        }

        if (opened) {
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    if (id_to_remove >= 0) {
        SceneContext::get().remove_entity(id_to_remove);
    }
}

} // namespace crayon::editor
