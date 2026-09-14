#include "asset_browser_panel.hpp"
#include "../../src/core/engine.hpp"
#include "../../src/core/log.hpp"
#include "../scene_context.hpp"
#include <imgui.h>
#include <algorithm>

namespace crayon::editor {

AssetBrowserPanel::AssetBrowserPanel(crayon::Engine& engine)
    : EditorPanel("3D Models & Assets"), m_engine(engine) {
    m_root_path = std::filesystem::current_path();
    if (std::filesystem::exists("models")) {
        m_current_path = m_root_path / "models";
    } else if (std::filesystem::exists("game/assets/models")) {
        m_current_path = m_root_path / "game/assets/models";
    } else {
        m_current_path = m_root_path;
    }
}

void AssetBrowserPanel::on_render() {
    if (!m_is_open) return;

    if (ImGui::Begin(m_title.c_str(), &m_is_open)) {
        render_content();
    }
    ImGui::End();
}

void AssetBrowserPanel::render_content() {
    // Navigation bar (Up button + breadcrumb path)
    bool has_parent = (m_current_path != m_root_path && m_current_path.has_parent_path());
    if (!has_parent) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(".. (Up)")) {
        m_current_path = m_current_path.parent_path();
    }
    if (!has_parent) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    std::string rel_path = std::filesystem::relative(m_current_path, m_root_path).generic_string();
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "/%s", rel_path.c_str());

    ImGui::SameLine(ImGui::GetWindowWidth() - 180.0f);
    ImGui::SetNextItemWidth(170.0f);
    ImGui::InputTextWithHint("##asset_filter", "Search models...", m_search_filter, sizeof(m_search_filter));

    ImGui::Separator();

    std::string filter_str = m_search_filter;
    for (auto& c : filter_str) c = (char)::tolower(c);

    // Grid / Table layout
    if (ImGui::BeginTable("AssetTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableHeadersRow();

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(m_current_path, ec)) {
            std::string filename = entry.path().filename().string();
            if (filename.empty() || filename[0] == '.') continue;

            if (!filter_str.empty()) {
                std::string lower_fn = filename;
                for (auto& c : lower_fn) c = (char)::tolower(c);
                if (lower_fn.find(filter_str) == std::string::npos) {
                    continue;
                }
            }

            bool is_dir = entry.is_directory(ec);
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = (char)::tolower(c);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Icon / Type tag
            if (is_dir) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[DIR]");
            } else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[3D]");
            } else if (ext == ".png" || ext == ".jpg") {
                ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.9f, 1.0f), "[TEX]");
            } else if (ext == ".lua") {
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[SCN]");
            } else {
                ImGui::TextDisabled("[FILE]");
            }

            ImGui::TableNextColumn();
            // Clickable name
            if (ImGui::Selectable(filename.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                if (is_dir) {
                    m_current_path = entry.path();
                }
            }

            std::string rel_asset = std::filesystem::relative(entry.path(), m_root_path).generic_string();

            // Double click interaction
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (is_dir) {
                    m_current_path = entry.path();
                } else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
                    SceneEntity* selected = SceneContext::get().get_selected_entity();
                    if (selected && selected->type == "model") {
                        selected->asset_path = rel_asset;
                    } else {
                        SceneEntity e;
                        e.name = entry.path().stem().string();
                        e.type = "model";
                        e.asset_path = rel_asset;
                        e.position = glm::vec3(0.0f, 0.5f, 0.0f);
                        SceneContext::get().add_entity(e);
                    }
                }
            }

            ImGui::TableNextColumn();
            if (!is_dir) {
                uintmax_t size_bytes = entry.file_size(ec);
                if (size_bytes < 1024) {
                    ImGui::Text("%llu B", size_bytes);
                } else if (size_bytes < 1024 * 1024) {
                    ImGui::Text("%.1f KB", size_bytes / 1024.0f);
                } else {
                    ImGui::Text("%.1f MB", size_bytes / (1024.0f * 1024.0f));
                }
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableNextColumn();
            if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
                if (ImGui::SmallButton("+ Spawn")) {
                    SceneEntity e;
                    e.name = entry.path().stem().string();
                    e.type = "model";
                    e.asset_path = rel_asset;
                    e.position = glm::vec3(0.0f, 0.5f, 0.0f);
                    SceneContext::get().add_entity(e);
                }
            } else if (ext == ".lua") {
                if (ImGui::SmallButton("Open SCN")) {
                    SceneContext::get().load_from_lua(rel_asset, m_engine.get_lua_runtime());
                }
            }
        }
        ImGui::EndTable();
    }
}

} // namespace crayon::editor
