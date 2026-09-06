#pragma once

#include <iostream>
#include <string>
#include <format>
#include <chrono>

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
    Error
};

inline void log_message(LogLevel level, const std::string& message) {
#ifdef _WIN32
    enable_windows_ansi();
#endif

    const char* prefix = "[INFO]";
    const char* color = "\033[0m"; // Reset

    switch (level) {
        case LogLevel::Debug:
            prefix = "[DEBUG]";
            color = "\033[36m"; // Cyan
            break;
        case LogLevel::Info:
            prefix = "[INFO] ";
            color = "\033[32m"; // Green
            break;
        case LogLevel::Warn:
            prefix = "[WARN] ";
            color = "\033[33m"; // Yellow
            break;
        case LogLevel::Error:
            prefix = "[ERROR]";
            color = "\033[31m"; // Red
            break;
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