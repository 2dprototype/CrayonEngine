#include "core/engine.hpp"
#include "core/log.hpp"
#include <string>

int main(int argc, char* argv[]) {
    std::string script_path = "";
    int win_w = 320;
    int win_h = 240;
    int virt_w = 320;
    int virt_h = 240;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--game" || arg == "-g") && i + 1 < argc) {
            script_path = argv[++i];
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
            script_path = arg;
        }
    }

    if (!script_path.empty()) {
        CRAYON_LOG_INFO("Starting Crayon Engine with script '{}' (Virtual: {}x{}, Window: {}x{})",
            script_path, virt_w, virt_h, win_w, win_h);
    } else {
        CRAYON_LOG_INFO("Starting Crayon Engine with no script (Virtual: {}x{}, Window: {}x{})",
            virt_w, virt_h, win_w, win_h);
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
