#include "editor_app.hpp"
#include "../src/core/log.hpp"

int main(int argc, char* argv[]) {
    int start_w = 630;
    int start_h = 450;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--window" && i + 1 < argc) {
            std::string res = argv[++i];
            size_t x_pos = res.find('x');
            if (x_pos != std::string::npos) {
                start_w = std::stoi(res.substr(0, x_pos));
                start_h = std::stoi(res.substr(x_pos + 1));
            }
        }
    }

    CRAYON_LOG_INFO("Starting Crayon Game Editor ({}x{})...", start_w, start_h);

    crayon::editor::EditorApp app;
    if (!app.init(start_w, start_h, "Crayon Game Editor")) {
        CRAYON_LOG_ERROR("Failed to initialize Crayon Game Editor");
        return 1;
    }

    app.run();

    CRAYON_LOG_INFO("Crayon Game Editor exited cleanly.");
    return 0;
}
