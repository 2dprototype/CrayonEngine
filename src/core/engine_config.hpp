#pragma once

#include <string>
#include <glm/vec4.hpp>

namespace crayon {

struct WindowConfig {
    std::string title = "Crayon Engine";
    int width = 320*2;
    int height = 240*2;
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
    std::string scaling = "integer"; // "integer", "aspect", "stretch", "center"
};

struct ModulesConfig {
    bool physics = true;    // Jolt 3D & 2D physics subsystem
    bool audio = true;      // MiniAudio audio playback/spatial subsystem
    bool mesh3d = true;     // 3D mesh & model renderer subsystem
    bool particles = true;  // Particle emitters
    bool input = true;      // Input handling
    bool fs = true;         // Virtual/physical filesystem
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
    int fps_limit = 0; // 0 = uncapped / vsync
    bool console = true;
};

} // namespace crayon
