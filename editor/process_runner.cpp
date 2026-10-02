#include "process_runner.hpp"

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <unistd.h>
  #include <signal.h>
  #include <sys/wait.h>
#endif

namespace crayon::editor {

struct ProcessRunner::Impl {
#ifdef _WIN32
    HANDLE process = nullptr;
    HANDLE read_pipe = nullptr;
#else
    pid_t pid = -1;
    int read_fd = -1;
#endif
};

ProcessRunner::~ProcessRunner() { stop(); delete m_impl; }

void ProcessRunner::push_line(const std::string& line) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_lines.push_back(line);
    if (m_lines.size() > 5000) m_lines.erase(m_lines.begin(), m_lines.begin() + 1000);
    ++m_version;
}

void ProcessRunner::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_lines.clear();
    ++m_version;
}

void ProcessRunner::snapshot(std::vector<std::string>& out, unsigned& version) {
    std::lock_guard<std::mutex> lk(m_mutex);
    out = m_lines;
    version = m_version;
}

void ProcessRunner::join_reader() {
    if (m_reader.joinable()) m_reader.join();
}

bool ProcessRunner::is_running() { return m_running.load(); }

#ifdef _WIN32

bool ProcessRunner::start(const std::string& exe, const std::string& dir) {
    stop();
    join_reader();
    if (!m_impl) m_impl = new Impl();

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) { push_line("[editor] Failed to create output pipe."); return false; }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    std::string cmd = "\"" + exe + "\" \"" + dir + "\"";
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back('\0');

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, dir.c_str(), &si, &pi);
    CloseHandle(wr);
    if (!ok) {
        CloseHandle(rd);
        push_line("[editor] Could not launch: " + exe + " (error " + std::to_string(GetLastError()) + ")");
        return false;
    }
    CloseHandle(pi.hThread);
    m_impl->process = pi.hProcess;
    m_impl->read_pipe = rd;
    m_running = true;
    m_reader = std::thread(&ProcessRunner::reader_loop, this);
    return true;
}

void ProcessRunner::reader_loop() {
    std::string pending;
    char chunk[1024];
    DWORD n = 0;
    while (ReadFile(m_impl->read_pipe, chunk, sizeof(chunk), &n, nullptr) && n > 0) {
        for (DWORD i = 0; i < n; ++i) {
            char c = chunk[i];
            if (c == '\n') {
                if (!pending.empty() && pending.back() == '\r') pending.pop_back();
                push_line(pending);
                pending.clear();
            } else pending += c;
        }
    }
    if (!pending.empty()) push_line(pending);
    DWORD code = 0;
    WaitForSingleObject(m_impl->process, 2000);
    GetExitCodeProcess(m_impl->process, &code);
    m_exit_code = static_cast<int>(code);
    CloseHandle(m_impl->read_pipe); m_impl->read_pipe = nullptr;
    CloseHandle(m_impl->process);   m_impl->process = nullptr;
    push_line("[editor] Process exited with code " + std::to_string(m_exit_code));
    m_running = false;
}

void ProcessRunner::stop() {
    if (m_running && m_impl && m_impl->process) {
        TerminateProcess(m_impl->process, 0);
    }
    join_reader();
}

#else

bool ProcessRunner::start(const std::string& exe, const std::string& dir) {
    stop();
    join_reader();
    if (!m_impl) m_impl = new Impl();
    int fds[2];
    if (pipe(fds) != 0) { push_line("[editor] Failed to create output pipe."); return false; }
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); push_line("[editor] fork failed."); return false; }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]); close(fds[1]);
        if (chdir(dir.c_str()) != 0) _exit(126);
        execlp(exe.c_str(), exe.c_str(), dir.c_str(), (char*)nullptr);
        _exit(127);
    }
    close(fds[1]);
    m_impl->pid = pid;
    m_impl->read_fd = fds[0];
    m_running = true;
    m_reader = std::thread(&ProcessRunner::reader_loop, this);
    return true;
}

void ProcessRunner::reader_loop() {
    std::string pending;
    char chunk[1024];
    ssize_t n;
    while ((n = read(m_impl->read_fd, chunk, sizeof(chunk))) > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            if (chunk[i] == '\n') { push_line(pending); pending.clear(); }
            else pending += chunk[i];
        }
    }
    if (!pending.empty()) push_line(pending);
    int status = 0;
    waitpid(m_impl->pid, &status, 0);
    m_exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    close(m_impl->read_fd);
    m_impl->pid = -1;
    push_line("[editor] Process exited with code " + std::to_string(m_exit_code));
    m_running = false;
}

void ProcessRunner::stop() {
    if (m_running && m_impl && m_impl->pid > 0) kill(m_impl->pid, SIGTERM);
    join_reader();
}

#endif

} // namespace crayon::editor
