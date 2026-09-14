#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include "window.hpp"
#include "input.hpp"
#include "../graphics/fbo.hpp"
#include "../graphics/shader.hpp"
#include "../graphics/texture.hpp"
#include "../graphics/camera.hpp"
#include "../graphics/batch2d.hpp"
#include "../graphics/mesh3d.hpp"

namespace crayon {

class LuaRuntime;
class PhysicsSystem;
class Model3D;

class Engine {
public:
    static Engine& get();

    Engine();
    ~Engine();

    bool init(int window_w = 960, int window_h = 720, int virtual_w = 320, int virtual_h = 240, const std::string& title = "Crayon Engine");
    void shutdown();

    void run();

    // Hot Reloading
    void request_hot_reload();
    bool check_hot_reload();

    // Subsystem Accessors
    Window& get_window() { return m_window; }
    Input& get_input() { return m_input; }
    FBO& get_fbo() { return m_fbo; }
    Batch2D& get_batch2d() { return m_batch2d; }
    MeshRenderer3D& get_mesh_renderer() { return m_mesh_renderer; }
    Camera& get_camera() { return m_camera; }
    LuaRuntime& get_lua_runtime() { return *m_lua_runtime; }
    PhysicsSystem& get_physics() { return *m_physics; }

    // Graphics state
    void set_clear_color(float r, float g, float b, float a = 1.0f);
    void set_active_color(float r, float g, float b, float a = 1.0f);
    const glm::vec4& get_active_color() const { return m_active_color; }

    // Resource Management
    GLuint load_texture(const std::string& path);
    bool get_texture_size(GLuint tex_id, int& w, int& h) const;
    std::shared_ptr<Mesh3D> load_model(const std::string& path);
    std::shared_ptr<Model3D> load_model3d(const std::string& path);

    // Time & Performance
    double get_time() const { return m_total_time; }
    float get_dt() const { return m_delta_time; }
    float get_fps() const { return m_fps; }

    const std::string& get_game_script_path() const { return m_game_script_path; }
    void set_game_script_path(const std::string& path) { m_game_script_path = path; }

    void render_to_fbo();
    void step_simulation(float dt);
    bool is_paused() const { return m_paused; }
    void set_paused(bool p) { m_paused = p; }
    bool is_running() const { return m_running; }
    void set_running(bool r) { m_running = r; }

private:
    void render_frame();

    Window m_window;
    Input m_input;
    FBO m_fbo;
    Batch2D m_batch2d;
    MeshRenderer3D m_mesh_renderer;
    Camera m_camera;
    std::unique_ptr<Shader> m_post_shader;
    std::unique_ptr<LuaRuntime> m_lua_runtime;
    std::unique_ptr<PhysicsSystem> m_physics;
    float m_physics_accumulator = 0.0f;

    std::unordered_map<std::string, std::shared_ptr<Texture>> m_texture_cache;
    std::unordered_map<GLuint, std::pair<int, int>> m_texture_sizes;
    std::unordered_map<std::string, std::shared_ptr<Mesh3D>> m_mesh_cache;
    std::unordered_map<std::string, std::shared_ptr<Model3D>> m_model_cache;

    glm::vec4 m_clear_color{0.08f, 0.08f, 0.12f, 1.0f};
    glm::vec4 m_active_color{1.0f, 1.0f, 1.0f, 1.0f};

    std::string m_game_script_path = "";
    std::filesystem::file_time_type m_last_script_write_time;

    double m_total_time = 0.0;
    float m_delta_time = 0.016667f;
    float m_fps = 60.0f;
    int m_frame_count = 0;
    double m_fps_timer = 0.0;

    bool m_hot_reload_requested = false;
    bool m_running = false;
    bool m_paused = false;
};

} // namespace crayon
