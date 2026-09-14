#include "console_panel.hpp"
#include "../../src/core/engine.hpp"
#include "../../src/scripting/lua_runtime.hpp"
#include <imgui.h>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace crayon::editor {

static ConsolePanel* s_active_console = nullptr;

ConsolePanel::ConsolePanel(crayon::Engine& engine)
    : EditorPanel("Console"), m_engine(engine) {
    s_active_console = this;
    crayon::set_log_sink([](LogLevel level, const std::string& msg) {
        if (s_active_console) {
            s_active_console->add_log(level, msg);
        }
    });

    add_log(LogLevel::Info, "Crayon Engine Editor initialized.");
}

ConsolePanel::~ConsolePanel() {
    if (s_active_console == this) {
        crayon::set_log_sink(nullptr);
        s_active_console = nullptr;
    }
}

void ConsolePanel::add_log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%H:%M:%S");

    m_logs.push_back({ level, message, ss.str() });
    if (m_logs.size() > 1000) {
        m_logs.erase(m_logs.begin(), m_logs.begin() + 100);
    }
}

void ConsolePanel::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logs.clear();
}

void ConsolePanel::on_render() {
    if (!m_is_open) return;

    if (ImGui::Begin(m_title.c_str(), &m_is_open)) {
        render_content();
    }
    ImGui::End();
}

void ConsolePanel::render_content() {
    // Controls bar
        if (ImGui::Button("Clear")) {
            clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_auto_scroll);
        ImGui::SameLine();

        const char* levels[] = { "All", "Info", "Warn", "Error" };
        ImGui::SetNextItemWidth(90.0f);
        ImGui::Combo("##level", &m_filter_level, levels, 4);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        ImGui::InputTextWithHint("##search", "Search logs...", m_search_filter, sizeof(m_search_filter));

        ImGui::Separator();

        // Logs child region
        float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("LogScrollRegion", ImVec2(0, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar);

        std::string filter_str = m_search_filter;
        for (auto& c : filter_str) c = (char)::tolower(c);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& log : m_logs) {
                // Filter by severity
                if (m_filter_level == 1 && log.level != LogLevel::Info) continue;
                if (m_filter_level == 2 && log.level != LogLevel::Warn) continue;
                if (m_filter_level == 3 && log.level != LogLevel::Error) continue;

                // Filter by text
                if (!filter_str.empty()) {
                    std::string lower_msg = log.message;
                    for (auto& c : lower_msg) c = (char)::tolower(c);
                    if (lower_msg.find(filter_str) == std::string::npos) {
                        continue;
                    }
                }

                ImVec4 col;
                const char* prefix;
                switch (log.level) {
                    case LogLevel::Debug:
                        col = ImVec4(0.3f, 0.8f, 0.9f, 1.0f);
                        prefix = "[DEBUG]";
                        break;
                    case LogLevel::Info:
                        col = ImVec4(0.4f, 0.85f, 0.4f, 1.0f);
                        prefix = "[INFO] ";
                        break;
                    case LogLevel::Warn:
                        col = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
                        prefix = "[WARN] ";
                        break;
                    case LogLevel::Error:
                        col = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
                        prefix = "[ERROR]";
                        break;
                }

                ImGui::TextDisabled("[%s]", log.timestamp.c_str());
                ImGui::SameLine();
                ImGui::TextColored(col, "%s", prefix);
                ImGui::SameLine();
                ImGui::TextUnformatted(log.message.c_str());
            }

            if (m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        // Lua REPL Input
        ImGui::Text("Lua >");
        ImGui::SameLine();

        ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##LuaCommand", "Execute Lua command (e.g. print('test'))...", m_command_buf, sizeof(m_command_buf), input_flags)) {
            if (m_command_buf[0] != '\0') {
                std::string cmd = m_command_buf;
                add_log(LogLevel::Info, std::string("> ") + cmd);
                m_command_history.push_back(cmd);
                m_engine.get_lua_runtime().execute_string(cmd);
                m_command_buf[0] = '\0';
                ImGui::SetKeyboardFocusHere(-1); // Keep focus on input
            }
        }
}

} // namespace crayon::editor
