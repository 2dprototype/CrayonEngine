#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace crayon::editor {

// Launches crayon.exe as a separate process and captures its output.
class ProcessRunner {
public:
    ProcessRunner() = default;
    ~ProcessRunner();

    bool start(const std::string& exe, const std::string& target_dir);
    void stop();
    bool is_running();
    int  exit_code() const { return m_exit_code; }
    unsigned version() const { return m_version.load(); }

    // Thread-safe snapshot of console lines; 'version' increments on every change.
    void snapshot(std::vector<std::string>& out, unsigned& version);
    void clear();
    void push_line(const std::string& line);

private:
    void reader_loop();
    void join_reader();

    std::mutex m_mutex;
    std::vector<std::string> m_lines;
    std::atomic<unsigned> m_version{0};
    std::atomic<bool> m_running{false};
    std::thread m_reader;
    int m_exit_code = 0;

    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace crayon::editor
