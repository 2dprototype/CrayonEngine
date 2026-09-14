#pragma once

#include "editor_panel.hpp"
#include "../../src/core/log.hpp"
#include <vector>
#include <mutex>

namespace crayon {
    class Engine;
}

namespace crayon::editor {

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string timestamp;
};

class ConsolePanel : public EditorPanel {
public:
    explicit ConsolePanel(crayon::Engine& engine);
    ~ConsolePanel() override;

    void on_render() override;
    void render_content() override;

    void add_log(LogLevel level, const std::string& message);
    void clear();

private:
    crayon::Engine& m_engine;
    std::vector<LogEntry> m_logs;
    std::mutex m_mutex;

    bool m_auto_scroll = true;
    int m_filter_level = 0; // 0=All, 1=Info, 2=Warn, 3=Error
    char m_search_filter[64] = "";
    char m_command_buf[256] = "";
    std::vector<std::string> m_command_history;
    int m_history_pos = -1;
};

} // namespace crayon::editor
