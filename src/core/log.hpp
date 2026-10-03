#pragma once

#include <iostream>
#include <string>
#include <format>
#include <chrono>
#include <functional>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

namespace crayon {

#ifdef _WIN32
inline void enable_windows_ansi() {
    static bool initialized = false;
    if (!initialized) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
        initialized = true;
    }
}
#endif

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
    Raw        // output with no prefix; used by crayon.print()
};

using LogSinkFn = std::function<void(LogLevel, const std::string&)>;
inline LogSinkFn s_log_sink = nullptr;
inline std::mutex s_log_mutex;
inline bool s_console_enabled = true;

inline void set_log_sink(LogSinkFn sink) {
    std::lock_guard<std::mutex> lock(s_log_mutex);
    s_log_sink = sink;
}

// Global switch driven by `t.console` in crayon.config(). When false, log
// output is still delivered to the sink (e.g. an in-game console) but is
// not written to stdout.
inline void set_console_enabled(bool enabled) {
    s_console_enabled = enabled;

#ifdef _WIN32
    // The terminal window is owned by *our process* on Windows (conhost.exe
    // spawned it when crayon.exe started). Hide/show it in lockstep with the
    // console flag. SDL/glad/Lua logging all route through this switch.
    if (HWND hwnd = GetConsoleWindow()) {
        ShowWindow(hwnd, enabled ? SW_SHOW : SW_HIDE);
    }
#endif
}

inline bool is_console_enabled() { return s_console_enabled; }

inline void log_message(LogLevel level, const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(s_log_mutex);
        if (s_log_sink) {
            s_log_sink(level, message);
        }
    }

    if (!s_console_enabled) return;

#ifdef _WIN32
    enable_windows_ansi();
#endif

    // Raw messages bypass prefix and colour entirely.
    if (level == LogLevel::Raw) {
        std::cout << message << '\n';
        return;
    }

    const char* prefix = "[INFO]";
    const char* color = "\033[0m";

    switch (level) {
        case LogLevel::Debug:
            prefix = "[DEBUG]"; color = "\033[36m"; break;
        case LogLevel::Info:
            prefix = "[INFO] "; color = "\033[32m"; break;
        case LogLevel::Warn:
            prefix = "[WARN] "; color = "\033[33m"; break;
        case LogLevel::Error:
            prefix = "[ERROR]"; color = "\033[31m"; break;
        case LogLevel::Raw:
            break; // unreachable — handled above
    }

    std::cout << color << prefix << " " << message << "\033[0m\n";
}

template<typename... Args>
inline void log_info(std::format_string<Args...> fmt, Args&&... args) {
    log_message(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline void log_warn(std::format_string<Args...> fmt, Args&&... args) {
    log_message(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline void log_error(std::format_string<Args...> fmt, Args&&... args) {
    log_message(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
inline void log_debug(std::format_string<Args...> fmt, Args&&... args) {
    log_message(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace crayon

#define CRAYON_LOG_INFO(...)  crayon::log_info(__VA_ARGS__)
#define CRAYON_LOG_WARN(...)  crayon::log_warn(__VA_ARGS__)
#define CRAYON_LOG_ERROR(...) crayon::log_error(__VA_ARGS__)
#define CRAYON_LOG_DEBUG(...) crayon::log_debug(__VA_ARGS__)