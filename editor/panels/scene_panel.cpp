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
        // Quick search filter
        ImGui::InputTextWithHint("##filter", "Filter entities...", m_filter, sizeof(m_filter));
        ImGui::Separator();

        // Add Entity Button / Context Menu
        if (ImGui::Button("+ Add Entity", ImVec2(-1, 0))) {
            ImGui::OpenPopup("AddEntityPopup");
        }

        if (ImGui::BeginPopup("AddEntityPopup")) {
            if (ImGui::MenuItem("3D Model Entity")) {
                SceneObject obj;
                obj.name = "New Model";
                obj.type = EntityType::Model3D;
                obj.position = glm::vec3(0.0f, 1.0f, 0.0f);
                SceneContext::get().add_object(obj);
            }
            if (ImGui::MenuItem("Primitive Cube")) {
                SceneObject obj;
                obj.name = "Cube Primitive";
                obj.type = EntityType::MeshPrimitive;
                obj.position = glm::vec3(0.0f, 0.5f, 0.0f);
                SceneContext::get().add_object(obj);
            }
            if (ImGui::MenuItem("2D Sprite")) {
                SceneObject obj;
                obj.name = "Sprite 2D";
                obj.type = EntityType::Sprite2D;
                SceneContext::get().add_object(obj);
            }
            if (ImGui::MenuItem("Physics Collider")) {
                SceneObject obj;
                obj.name = "Physics RigidBody";
                obj.type = EntityType::PhysicsBody;
                obj.has_physics = true;
                obj.physics_type = 1;
                SceneContext::get().add_object(obj);
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();

        // Entity List
        auto& objects = SceneContext::get().get_objects();
        int selected_id = SceneContext::get().get_selected_id();
        int id_to_remove = -1;

        std::string filter_str = m_filter;
        for (auto& c : filter_str) c = (char)::tolower(c);

        for (size_t i = 0; i < objects.size(); ++i) {
            auto& obj = objects[i];

            if (!filter_str.empty()) {
                std::string lower_name = obj.name;
                for (auto& c : lower_name) c = (char)::tolower(c);
                if (lower_name.find(filter_str) == std::string::npos) {
                    continue;
                }
            }

            ImGui::PushID(obj.id);

            const char* type_icon = "[3D]";
            if (obj.type == EntityType::Camera3D) type_icon = "[CAM]";
            else if (obj.type == EntityType::Sprite2D) type_icon = "[2D]";
            else if (obj.type == EntityType::MeshPrimitive) type_icon = "[PRM]";
            else if (obj.type == EntityType::PhysicsBody) type_icon = "[PHX]";

            std::string label = std::string(type_icon) + " " + obj.name;

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (obj.id == selected_id) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            bool opened = ImGui::TreeNodeEx((void*)(intptr_t)obj.id, flags, "%s", label.c_str());
            if (ImGui::IsItemClicked()) {
                SceneContext::get().set_selected_id(obj.id);
            }

            // Right click context menu
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Duplicate")) {
                    SceneObject dup = obj;
                    dup.name += " (Copy)";
                    dup.position += glm::vec3(0.5f, 0.0f, 0.5f);
                    SceneContext::get().add_object(dup);
                }
                if (ImGui::MenuItem("Delete")) {
                    id_to_remove = obj.id;
                }
                ImGui::EndPopup();
            }

            if (opened) {
                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (id_to_remove >= 0) {
            SceneContext::get().remove_object(id_to_remove);
        }
}

} // namespace crayon::editor
