#include "engine.hpp"
#include "log.hpp"
#include "../graphics/default_shaders.hpp"
#include "../scripting/lua_runtime.hpp"
#include "../physics/physics_system.hpp"
#include <filesystem>

namespace crayon {

static Engine* s_instance = nullptr;

Engine& Engine::get() {
    return *s_instance;
}

Engine::Engine() {
    s_instance = this;
}

Engine::~Engine() {
    shutdown();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool Engine::init(int window_w, int window_h, int virtual_w, int virtual_h, const std::string& title) {
    if (!m_window.init(title, window_w, window_h, virtual_w, virtual_h)) {
        CRAYON_LOG_ERROR("Engine failed to initialize Window");
        return false;
    }

    if (!m_fbo.init(virtual_w, virtual_h)) {
        CRAYON_LOG_ERROR("Engine failed to initialize virtual FBO");
        return false;
    }

    if (!m_batch2d.init()) {
        CRAYON_LOG_ERROR("Engine failed to initialize Batch2D");
        return false;
    }

    if (!m_mesh_renderer.init()) {
        CRAYON_LOG_ERROR("Engine failed to initialize MeshRenderer3D");
        return false;
    }

    m_post_shader = std::make_unique<Shader>();
    if (!m_post_shader->load_from_memory(SHADER_POST_VS, SHADER_POST_FS)) {
        CRAYON_LOG_ERROR("Engine failed to compile Post-Processing Shader");
        return false;
    }

    m_physics = std::make_unique<PhysicsSystem>();
    if (!m_physics->init()) {
        CRAYON_LOG_ERROR("Engine failed to initialize PhysicsSystem");
        return false;
    }

    m_lua_runtime = std::make_unique<LuaRuntime>();
    if (!m_lua_runtime->init()) {
        CRAYON_LOG_ERROR("Engine failed to initialize LuaRuntime");
        return false;
    }

    if (!m_game_script_path.empty()) {
        if (std::filesystem::exists(m_game_script_path)) {
            m_last_script_write_time = std::filesystem::last_write_time(m_game_script_path);
            m_lua_runtime->load_script(m_game_script_path);
        } else {
            CRAYON_LOG_WARN("Game entry script not found: {}", m_game_script_path);
        }
    }

    m_running = true;
    CRAYON_LOG_INFO("Crayon Engine initialized successfully");
    return true;
}

void Engine::shutdown() {
    m_running = false;
    m_texture_cache.clear();
    m_mesh_cache.clear();

    if (m_lua_runtime) {
        m_lua_runtime->shutdown();
        m_lua_runtime.reset();
    }
    if (m_physics) {
        m_physics->shutdown();
        m_physics.reset();
    }
    m_mesh_renderer.shutdown();
    m_batch2d.shutdown();
    m_fbo.shutdown();
    m_window.shutdown();
}

void Engine::set_clear_color(float r, float g, float b, float a) {
    m_clear_color = glm::vec4(r, g, b, a);
}

void Engine::set_active_color(float r, float g, float b, float a) {
    m_active_color = glm::vec4(r, g, b, a);
}

GLuint Engine::load_texture(const std::string& path) {
    auto it = m_texture_cache.find(path);
    if (it != m_texture_cache.end()) {
        return it->second->get_id();
    }

    auto tex = std::make_shared<Texture>();
    if (!tex->load_from_file(path, true)) {
        CRAYON_LOG_WARN("Failed to load texture '{}', falling back to white texture", path);
        return m_batch2d.get_white_texture_id();
    }

    GLuint id = tex->get_id();
    m_texture_sizes[id] = { tex->get_width(), tex->get_height() };
    m_texture_cache[path] = tex;
    return id;
}

bool Engine::get_texture_size(GLuint tex_id, int& w, int& h) const {
    auto it = m_texture_sizes.find(tex_id);
    if (it != m_texture_sizes.end()) {
        w = it->second.first;
        h = it->second.second;
        return true;
    }
    w = 0; h = 0;
    return false;
}

std::shared_ptr<Mesh3D> Engine::load_model(const std::string& path) {
    auto it = m_mesh_cache.find(path);
    if (it != m_mesh_cache.end()) {
        return it->second;
    }

    auto mesh = std::make_shared<Mesh3D>();
    if (!mesh->load_from_obj(path)) {
        CRAYON_LOG_WARN("Failed to load model '{}', falling back to procedural cube", path);
        mesh = Mesh3D::create_cube(1.0f);
    }

    m_mesh_cache[path] = mesh;
    return mesh;
}

void Engine::request_hot_reload() {
    m_hot_reload_requested = true;
}

bool Engine::check_hot_reload() {
    if (m_hot_reload_requested) {
        m_hot_reload_requested = false;
        return true;
    }

    if (!m_game_script_path.empty() && std::filesystem::exists(m_game_script_path)) {
        auto current_time = std::filesystem::last_write_time(m_game_script_path);
        if (current_time != m_last_script_write_time) {
            m_last_script_write_time = current_time;
            return true;
        }
    }
    return false;
}

void Engine::render_frame() {
    // If virtual resolution was changed in Window, resize FBO
    int virt_w = m_window.get_virtual_width();
    int virt_h = m_window.get_virtual_height();
    if (m_fbo.get_width() != virt_w || m_fbo.get_height() != virt_h) {
        m_fbo.resize(virt_w, virt_h);
    }

    // 1. Render Scene to Virtual FBO
    m_fbo.bind();
    glViewport(0, 0, virt_w, virt_h);

    glm::vec4 clear_col = m_clear_color;
    if (m_game_script_path.empty()) {
        clear_col = glm::vec4(0.0f, 0.2f, 0.65f, 1.0f); // Retro blue screen
    }

    glClearColor(clear_col.r, clear_col.g, clear_col.b, clear_col.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    float aspect = static_cast<float>(virt_w) / static_cast<float>(virt_h);

    m_mesh_renderer.begin(m_camera, aspect);
    m_batch2d.begin(virt_w, virt_h);

    if (m_game_script_path.empty()) {
        std::string title = "Crayon Engine";
        float scale = 2.0f;
        float tw = m_batch2d.get_text_width(title, scale);
        float th = m_batch2d.get_text_height(title, scale);
        float tx = (static_cast<float>(virt_w) - tw) * 0.5f;
        float ty = (static_cast<float>(virt_h) - th) * 0.5f;

        // Shadow
        m_batch2d.draw_text(title, tx + 1.0f, ty + 1.0f, scale, glm::vec4(0.0f, 0.08f, 0.35f, 0.8f));
        // Main text
        m_batch2d.draw_text(title, tx, ty, scale, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else {
        m_lua_runtime->call_draw();
    }

    m_batch2d.end();
    m_mesh_renderer.end();

    m_fbo.unbind();

    // 2. Post-Process and Blit to Window
    int win_w = 960, win_h = 720;
    m_window.get_window_size(win_w, win_h);
    const auto& retro = m_mesh_renderer.get_retro_effects();
    PostProcessOptions opts;
    opts.dither_enabled = retro.dither_enabled;
    opts.dither_levels = retro.dither_levels;
    opts.crt_scanlines = retro.crt_scanlines;
    opts.scanline_strength = retro.scanline_strength;
    opts.crt_curvature = retro.crt_curvature;
    opts.curvature_distort = retro.curvature_distort;
    opts.vignette = retro.vignette;
    opts.vignette_strength = retro.vignette_strength;
    opts.transparent = m_window.is_transparent();

    m_fbo.blit_to_screen(m_window.get_viewport_info(), win_w, win_h, *m_post_shader, opts);

    // 3. Swap SDL Buffers
    m_window.swap_buffers();
}

void Engine::run() {
    using clock = std::chrono::high_resolution_clock;
    auto prev_time = clock::now();

    while (m_running && !m_window.should_close()) {
        auto current_time = clock::now();
        std::chrono::duration<double> elapsed = current_time - prev_time;
        prev_time = current_time;

        m_delta_time = static_cast<float>(elapsed.count());
        if (m_delta_time > 0.1f) m_delta_time = 0.1f; // Clamp to avoid spiral of death
        m_total_time += m_delta_time;

        // FPS calculation
        m_frame_count++;
        m_fps_timer += m_delta_time;
        if (m_fps_timer >= 0.5) {
            m_fps = static_cast<float>(m_frame_count) / static_cast<float>(m_fps_timer);
            m_frame_count = 0;
            m_fps_timer = 0.0;
        }

        // Hot Reload check
        if (check_hot_reload()) {
            CRAYON_LOG_INFO("Hot-reloading script: {}", m_game_script_path);
            m_lua_runtime->reload_script(m_game_script_path);
        }

        // Process Input and Window Events
        m_input.begin_frame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            m_window.handle_event(event);
            m_input.handle_event(event, m_window);

            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F5) {
                request_hot_reload();
            }
        }

        if (m_window.should_close()) {
            break;
        }

        // Fixed-timestep physics simulation (60 Hz)
        if (m_physics) {
            const float fixed_dt = 1.0f / 60.0f;
            m_physics_accumulator += m_delta_time;
            if (m_physics_accumulator > 0.2f) m_physics_accumulator = 0.2f; // clamp spiral
            while (m_physics_accumulator >= fixed_dt) {
                m_physics->update(fixed_dt);
                m_physics_accumulator -= fixed_dt;
            }
        }

        // Game Update
        if (!m_game_script_path.empty()) {
            m_lua_runtime->call_update(m_delta_time);
        } else {
            if (m_input.is_key_pressed("escape")) {
                m_running = false;
            }
        }

        // Render Virtual Canvas & Blit
        render_frame();
    }
}

} // namespace crayon
