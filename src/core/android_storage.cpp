#include "android_storage.hpp"
#include "log.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace crayon::android {

namespace {

bool is_game_dir(const fs::path& dir) {
    std::error_code ec;
    return fs::exists(dir / ".crayonproj", ec) || fs::exists(dir / "main.lua", ec);
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

} // namespace

std::string prepare_game_root() {
    // ---- 1. Dev override in external app storage --------------------------------
    if (const char* ext = SDL_GetAndroidExternalStoragePath()) {
        fs::path dev = fs::path(ext) / "game";
        if (is_game_dir(dev)) {
            CRAYON_LOG_INFO("Android: using dev game folder '{}'", dev.string());
            return dev.string();
        }
    }

    // ---- 2. Packaged game: extract from APK -------------------------------------
    const char* internal = SDL_GetAndroidInternalStoragePath();
    if (!internal) {
        CRAYON_LOG_ERROR("Android: no internal storage path: {}", SDL_GetError());
        return "";
    }
    const fs::path root = fs::path(internal) / "game";

    std::vector<char> list_bytes;
    if (!read_asset("crayon_assets.txt", list_bytes)) {
        // No manifest in the APK. Maybe the game was extracted by an earlier run.
        if (is_game_dir(root)) return root.string();
        CRAYON_LOG_ERROR("Android: APK has no crayon_assets.txt and no game was found");
        return "";
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

    // Skip the copy when this exact build was already extracted.
    const fs::path stamp_file = root / ".crayon_stamp";
    {
        std::ifstream f(stamp_file);
        std::string existing;
        if (f && std::getline(f, existing) && !stamp.empty() && existing == stamp && is_game_dir(root)) {
            return root.string();
        }
    }

    CRAYON_LOG_INFO("Android: extracting {} game files to '{}'", files.size(), root.string());
    std::error_code ec;
    fs::create_directories(root, ec);

    for (const auto& rel : files) {
        // Reject anything that could escape the game directory.
        if (rel.find("..") != std::string::npos || rel[0] == '/') {
            CRAYON_LOG_WARN("Android: skipping suspicious asset path '{}'", rel);
            continue;
        }
        std::vector<char> bytes;
        if (!read_asset("game/" + rel, bytes)) {
            CRAYON_LOG_WARN("Android: could not read asset 'game/{}'", rel);
            continue;
        }
        fs::path dest = root / rel;
        fs::create_directories(dest.parent_path(), ec);
        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (!out) {
            CRAYON_LOG_WARN("Android: could not write '{}'", dest.string());
            continue;
        }
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    std::ofstream(stamp_file, std::ios::trunc) << stamp << '\n';

    if (!is_game_dir(root)) {
        CRAYON_LOG_ERROR("Android: extracted game has neither .crayonproj nor main.lua");
        return "";
    }
    return root.string();
}

} // namespace crayon::android
