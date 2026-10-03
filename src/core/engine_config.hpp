#pragma once

#include <string>
#include <glm/vec4.hpp>

namespace crayon {

struct WindowConfig {
    std::string title = "Crayon Engine";
    int width = 320;
    int height = 240;
    int virtual_width = 320;
    int virtual_height = 240;
    int min_width = 1;
    int min_height = 1;
    bool resizable = true;
    bool fullscreen = false;
    bool vsync = true;
    bool transparent = false;
    bool borderless = false;
    bool always_on_top = false;
    bool click_through = false;

    // ---- Overlay / screenpet flags (all applied at window creation) ----
    bool  skip_taskbar   = false;  // hide from taskbar / Alt+Tab (Windows: WS_EX_TOOLWINDOW)
    bool  not_focusable  = false;  // never steal focus from the user's real work
    bool  utility_window = false;  // system utility chrome; on macOS also hides from Dock
    float opacity        = 1.0f;   // 0.0 – 1.0, applied before the window is shown

    std::string scaling = "integer"; // "integer", "aspect", "stretch", "center"
};

struct ModulesConfig {
    bool physics3d = true;
    bool physics2d = true;
    bool audio = true;
    bool mesh3d = true;
    bool particles = true;
    bool input = true;
    bool fs = true;
};

struct GraphicsConfig {
    glm::vec4 clear_color{0.08f, 0.08f, 0.12f, 1.0f};
    bool dither = false;
    bool crt = false;
    bool vignette = false;
};

struct EngineConfig {
    std::string identity = "crayon_game";
    std::string version = "1.0.0";
    WindowConfig window;
    ModulesConfig modules;
    GraphicsConfig graphics;
    int fps_limit = 0;
    bool console = true;
};

} // namespace crayon