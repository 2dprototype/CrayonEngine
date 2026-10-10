#include "android_storage.hpp"
#include "log.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

namespace crayon::android {

namespace {

bool is_game_dir(const fs::path& dir) {
    std::error_code ec;
    return fs::is_directory(dir, ec) && (fs::exists(dir / ".crayonproj", ec) || fs::exists(dir / "main.lua", ec));
}

// Reads a file from the APK's assets/ folder (SDL_IOFromFile falls back to the
// AssetManager on Android for relative paths).
bool read_asset(const std::string& rel, std::vector<char>& out) {
    size_t size = 0;
    void* data = SDL_LoadFile(rel.c_str(), &size);
    if (!data) return false;
    out.assign(static_cast<char*>(data), static_cast<char*>(data) + size);
    SDL_free(data);
    return true;
}

// Checks if a directory contains a game directly, or scans its subdirectories for projects
fs::path find_project_in_location(const fs::path& base_dir) {
    std::error_code ec;
    if (!fs::exists(base_dir, ec) || !fs::is_directory(base_dir, ec)) {
        return "";
    }

    // 1. Direct game in this directory (.crayonproj or main.lua)
    if (is_game_dir(base_dir)) {
        return base_dir;
    }

    // 2. Check for active project file: active.txt or active_project.txt
    for (const char* active_file : {"active.txt", "active_project.txt"}) {
        fs::path act = base_dir / active_file;
        if (fs::exists(act, ec)) {
            std::ifstream f(act);
            std::string sub_name;
            if (f && std::getline(f, sub_name)) {
                while (!sub_name.empty() && (sub_name.back() == '\r' || sub_name.back() == ' ' || sub_name.back() == '\n')) {
                    sub_name.pop_back();
                }
                if (!sub_name.empty()) {
                    fs::path candidate = base_dir / sub_name;
                    if (is_game_dir(candidate)) {
                        CRAYON_LOG_INFO("Android: using active project from '{}': '{}'", base_dir.string(), candidate.string());
                        return candidate;
                    }
                }
            }
        }
    }

    // 3. Scan subdirectories for game projects
    std::vector<std::pair<fs::file_time_type, fs::path>> projects;
    for (const auto& entry : fs::directory_iterator(base_dir, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_directory(ec) && is_game_dir(entry.path())) {
            projects.push_back({entry.last_write_time(ec), entry.path()});
        }
    }

    if (!projects.empty()) {
        // Sort by last modified time, newest first
        std::sort(projects.begin(), projects.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;
        });
        CRAYON_LOG_INFO("Android: found {} projects in '{}', loading latest: '{}'",
                        projects.size(), base_dir.string(), projects[0].second.string());
        return projects[0].second;
    }

    return "";
}

bool extract_apk_game_to(const fs::path& target_dir) {
    std::error_code ec;

    std::vector<char> list_bytes;
    if (!read_asset("crayon_assets.txt", list_bytes)) {
        return is_game_dir(target_dir);
    }

    std::istringstream list(std::string(list_bytes.begin(), list_bytes.end()));
    std::string stamp;
    std::vector<std::string> files;
    for (std::string line; std::getline(list, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        if (line.rfind("# stamp:", 0) == 0) { stamp = line.substr(8); continue; }
        if (line[0] == '#') continue;
        files.push_back(line);
    }

    const fs::path stamp_file = target_dir / ".crayon_stamp";
    {
        std::ifstream f(stamp_file);
        std::string existing;
        if (f && std::getline(f, existing) && !stamp.empty() && existing == stamp && is_game_dir(target_dir)) {
            return true;
        }
    }

    CRAYON_LOG_INFO("Android: extracting {} packaged game files to '{}'", files.size(), target_dir.string());
    fs::create_directories(target_dir, ec);

    for (const auto& rel : files) {
        if (rel.find("..") != std::string::npos || rel[0] == '/') continue;
        std::vector<char> bytes;
        if (!read_asset("game/" + rel, bytes)) continue;
        fs::path dest = target_dir / rel;
        fs::create_directories(dest.parent_path(), ec);
        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (out) {
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        }
    }

    std::ofstream(stamp_file, std::ios::trunc) << stamp << '\n';
    return is_game_dir(target_dir);
}

} // namespace

std::string prepare_game_root() {
    std::error_code ec;

    // ---- 1. Check primary shared storage directories ---------------------------
    // The user can place projects in /sdcard/Crayon or /sdcard/CrayonProjects
    const std::vector<fs::path> candidate_roots = {
        "/sdcard/Crayon",
        "/storage/emulated/0/Crayon",
        "/sdcard/CrayonProjects",
        "/storage/emulated/0/CrayonProjects"
    };

    for (const auto& candidate : candidate_roots) {
        fs::path found = find_project_in_location(candidate);
        if (!found.empty()) {
            CRAYON_LOG_INFO("Android: using project from shared storage '{}'", found.string());
            return found.string();
        }
    }

    // ---- 2. Check app external files directory ----------------------------------
    // /sdcard/Android/data/<package>/files/game or /sdcard/Android/data/<package>/files/
    if (const char* ext = SDL_GetAndroidExternalStoragePath()) {
        fs::path ext_path(ext);
        fs::path found = find_project_in_location(ext_path / "game");
        if (!found.empty()) {
            CRAYON_LOG_INFO("Android: using project from external files '{}'", found.string());
            return found.string();
        }
        found = find_project_in_location(ext_path);
        if (!found.empty()) {
            CRAYON_LOG_INFO("Android: using project from external files root '{}'", found.string());
            return found.string();
        }
    }

    // ---- 3. Default folder setup & demo extraction -----------------------------
    // Try to create /sdcard/Crayon so the user sees a ready-to-use directory on phone
    fs::path preferred_shared = "/sdcard/Crayon";
    fs::create_directories(preferred_shared, ec);

    if (fs::exists(preferred_shared, ec)) {
        // Create a helpful README file
        fs::path readme = preferred_shared / "README.txt";
        if (!fs::exists(readme, ec)) {
            std::ofstream rf(readme);
            if (rf) {
                rf << "=== Crayon Engine for Android ===\n\n"
                   << "How to play games:\n"
                   << "1. Create a game folder here (e.g. /sdcard/Crayon/mygame/)\n"
                   << "2. Put your main.lua and/or .crayonproj inside it\n"
                   << "3. Put all your assets (sprites, sounds, models) in that folder\n"
                   << "4. Open Crayon Engine and it will run your project!\n\n"
                   << "Tips:\n"
                   << "- If you have multiple games, the engine runs the most recently modified one.\n"
                   << "- Or write the folder name in 'active.txt' to choose which game to launch.\n";
            }
        }

        fs::path demo_dir = preferred_shared / "demo";
        if (extract_apk_game_to(demo_dir)) {
            return demo_dir.string();
        }
    }

    // ---- 4. Internal storage fallback ------------------------------------------
    const char* internal = SDL_GetAndroidInternalStoragePath();
    if (!internal) {
        CRAYON_LOG_ERROR("Android: no internal storage path available");
        return "";
    }
    fs::path internal_game = fs::path(internal) / "game";
    if (extract_apk_game_to(internal_game)) {
        return internal_game.string();
    }

    CRAYON_LOG_ERROR("Android: could not find or extract any valid game project");
    return "";
}

} // namespace crayon::android
