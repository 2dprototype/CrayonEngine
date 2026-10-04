#include "core/engine.hpp"
#include "core/project.hpp"
#include "core/log.hpp"
#include <iostream>
#include <string>
#include <filesystem>

static void print_help() {
    std::cout << "Crayon Engine - Fantasy Console & 2D/3D Game Runtime\n\n"
              << "Usage:\n"
              << "  crayon <path_to_script>.lua       Run a Lua script directly\n"
              << "  crayon <folder_path>              Run a project folder containing .crayonproj\n"
              << "  crayon <path_to_.crayonproj>      Run a game using its .crayonproj manifest\n"
              << "  crayon                            Run project in current directory if .crayonproj exists\n\n"
              << "Options:\n"
              << "  --game, -g <path>                 Specify entry script path or project folder\n"
              << "  --window <width>x<height>         Override physical window resolution (e.g. 1280x720)\n"
              << "  --res <width>x<height>            Override virtual canvas resolution (e.g. 320x240)\n"
              << "  --version, -v                     Display version information\n"
              << "  --help, -h                        Display this help message\n";
}

int main(int argc, char* argv[]) {
    std::string target_input = "";
    int win_w = 0;
    int win_h = 0;
    int virt_w = 0;
    int virt_h = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "Crayon Engine version 0.0.2\n";
            return 0;
        } else if ((arg == "--game" || arg == "-g") && i + 1 < argc) {
            target_input = argv[++i];
        } else if (arg == "--res" && i + 1 < argc) {
            std::string res = argv[++i];
            size_t x_pos = res.find('x');
            if (x_pos != std::string::npos) {
                virt_w = std::stoi(res.substr(0, x_pos));
                virt_h = std::stoi(res.substr(x_pos + 1));
            }
        } else if (arg == "--window" && i + 1 < argc) {
            std::string res = argv[++i];
            size_t x_pos = res.find('x');
            if (x_pos != std::string::npos) {
                win_w = std::stoi(res.substr(0, x_pos));
                win_h = std::stoi(res.substr(x_pos + 1));
            }
        } else if (arg[0] != '-') {
            target_input = arg;
        }
    }

    std::string script_path = "";

    // Helper: every entry point below funnels through this so that all
    // runtime asset paths (textures, models, fonts, shaders, audio, Lua
    // require() ...) resolve relative to the project root, never to the
    // shell's working directory.
    auto set_project_root = [](const std::filesystem::path& dir) {
        std::error_code ec;
        std::filesystem::current_path(dir, ec);
        if (ec) {
            CRAYON_LOG_WARN("Could not chdir to project root '{}': {}",
                            dir.string(), ec.message());
        }
    };

    // 1. If target_input is empty or ".", check current working directory for .crayonproj
    if (target_input.empty() || target_input == ".") {
        std::filesystem::path local_manifest = std::filesystem::absolute(".crayonproj").lexically_normal();
        if (std::filesystem::exists(local_manifest)) {
            crayon::CrayonProject proj;
            if (crayon::loadCrayonProject(local_manifest, proj)) {
                script_path = proj.resolveTargetScript(std::filesystem::current_path());
                // Already inside the project root; nothing to chdir to.
            } else {
                return 1;
            }
        }
    } else {
        std::filesystem::path p = std::filesystem::absolute(target_input).lexically_normal();

        if (!std::filesystem::exists(p)) {
            CRAYON_LOG_ERROR("Target path not found: {}", target_input);
            return 1;
        }

        // 2. Direct .crayonproj file passed: crayon path/to/.crayonproj
        if (std::filesystem::is_regular_file(p) && (p.filename() == ".crayonproj" || p.extension() == ".crayonproj")) {
            crayon::CrayonProject proj;
            if (!crayon::loadCrayonProject(p, proj)) {
                return 1;
            }
            std::filesystem::path project_dir = p.parent_path();
            if (project_dir.empty()) {
                project_dir = std::filesystem::current_path();
            }
            script_path = proj.resolveTargetScript(project_dir);
            set_project_root(project_dir);
        }
        // 3. Project folder passed: crayon <folder_path>
        else if (std::filesystem::is_directory(p)) {
            std::filesystem::path manifest = p / ".crayonproj";
            if (!std::filesystem::exists(manifest)) {
                CRAYON_LOG_ERROR("No .crayonproj file found in folder '{}'", p.string());
                return 1;
            }
            crayon::CrayonProject proj;
            if (!crayon::loadCrayonProject(manifest, proj)) {
                return 1;
            }
            script_path = proj.resolveTargetScript(p);
            set_project_root(p);
        }
        // 4. Direct Lua script: crayon <path_to_script>.lua
        //    Assets resolve relative to the SCRIPT'S directory, so running
        //    `crayon /some/game/main.lua` behaves identically to running
        //    `crayon /some/game` when a .crayonproj is present.
        else if (std::filesystem::is_regular_file(p)) {
            script_path = p.string();

            std::filesystem::path script_dir = p.parent_path();
            if (!script_dir.empty()) {
                set_project_root(script_dir);
            }
        }
    }

    if (!script_path.empty()) {
        if (!std::filesystem::exists(script_path)) {
            CRAYON_LOG_ERROR("Target entry script not found: {}", script_path);
            return 1;
        }
        CRAYON_LOG_INFO("Starting Crayon Engine with script '{}'", script_path);
        CRAYON_LOG_INFO("Asset root: '{}'", std::filesystem::current_path().string());
    } else {
        CRAYON_LOG_INFO("Starting Crayon Engine with no script");
    }

    crayon::Engine engine;
    engine.set_game_script_path(script_path);

    if (!engine.init(win_w, win_h, virt_w, virt_h, "Crayon Engine")) {
        CRAYON_LOG_ERROR("Failed to initialize Crayon Engine");
        return 1;
    }

    engine.run();

    CRAYON_LOG_INFO("Crayon Engine exited cleanly");
    return 0;
}