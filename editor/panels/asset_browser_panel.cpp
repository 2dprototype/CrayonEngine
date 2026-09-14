#include "asset_browser_panel.hpp"
#include "../../src/core/engine.hpp"
#include "../../src/core/log.hpp"
#include "../scene_context.hpp"
#include <imgui.h>
#include <algorithm>

namespace crayon::editor {

AssetBrowserPanel::AssetBrowserPanel(crayon::Engine& engine)
    : EditorPanel("Asset Browser"), m_engine(engine) {
    if (std::filesystem::exists("game")) {
        m_root_path = std::filesystem::current_path();
        m_current_path = m_root_path / "game";
    } else {
        m_root_path = std::filesystem::current_path();
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

        ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f);
        ImGui::SetNextItemWidth(190.0f);
        ImGui::InputTextWithHint("##asset_filter", "Search...", m_search_filter, sizeof(m_search_filter));

        ImGui::Separator();

        std::string filter_str = m_search_filter;
        for (auto& c : filter_str) c = (char)::tolower(c);

        // Grid / Table layout
        if (ImGui::BeginTable("AssetTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
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
                } else if (ext == ".lua") {
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[LUA]");
                } else if (ext == ".png" || ext == ".jpg") {
                    ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.9f, 1.0f), "[IMG]");
                } else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
                    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[3D]");
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

                // Double click interaction
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (is_dir) {
                        m_current_path = entry.path();
                    } else if (ext == ".lua") {
                        std::string script_rel = std::filesystem::relative(entry.path(), m_root_path).generic_string();
                        m_engine.set_game_script_path(script_rel);
                        m_engine.request_hot_reload();
                        CRAYON_LOG_INFO("Loaded game script from asset browser: {}", script_rel);
                    } else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
                        SceneObject* obj = SceneContext::get().get_selected_object();
                        if (obj) {
                            obj->asset_path = std::filesystem::relative(entry.path(), m_root_path).generic_string();
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
                if (ext == ".lua") {
                    if (ImGui::SmallButton("Run##btn")) {
                        std::string script_rel = std::filesystem::relative(entry.path(), m_root_path).generic_string();
                        m_engine.set_game_script_path(script_rel);
                        m_engine.request_hot_reload();
                    }
                } else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
                    if (ImGui::SmallButton("Assign##btn")) {
                        SceneObject* obj = SceneContext::get().get_selected_object();
                        if (obj) {
                            obj->asset_path = std::filesystem::relative(entry.path(), m_root_path).generic_string();
                        }
                    }
                }
            }
            ImGui::EndTable();
        }
}

} // namespace crayon::editor
