#include "engine.hpp"
#include "log.hpp"
#include "../graphics/default_shaders.hpp"
#include "../graphics/model3d.hpp"
#include "../scripting/lua_runtime.hpp"
#include "../physics/physics_system.hpp"
#include "../physics/physics2d_system.hpp"
#include "../physics/physics4d_system.hpp"
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
    // Remember CLI overrides for soft_restart()
    m_cli_window_w  = window_w;
    m_cli_window_h  = window_h;
    m_cli_virtual_w = virtual_w;
    m_cli_virtual_h = virtual_h;
    m_cli_title     = title;

    // 1. Populate default configuration (only override if explicitly provided)
    if (!title.empty()) m_config.window.title = title;
    if (window_w > 0) m_config.window.width = window_w;
    if (window_h > 0) m_config.window.height = window_h;
    if (virtual_w > 0) m_config.window.virtual_width = virtual_w;
    if (virtual_h > 0) m_config.window.virtual_height = virtual_h;

    // 2. Initialize LuaRuntime early for config execution
    m_lua_runtime = std::make_unique<LuaRuntime>();
    if (!m_lua_runtime->init()) {
        CRAYON_LOG_ERROR("Engine failed to initialize LuaRuntime");
        return false;
    }

    // 3. Run config phase if a game script exists (like love.conf, inside target script)
    bool script_loaded = false;
    if (!m_game_script_path.empty()) {
        if (std::filesystem::exists(m_game_script_path)) {
            m_last_script_write_time = std::filesystem::last_write_time(m_game_script_path);
            script_loaded = m_lua_runtime->run_config_phase(m_game_script_path, m_config);
        } else {
            CRAYON_LOG_WARN("Game entry script not found: {}", m_game_script_path);
        }
    }
    
    // Apply the console flag from crayon.config() now that it's been read.
    set_console_enabled(m_config.console);

    // 4. Initialize Window with ALL overlay flags set at creation time.
    //    This avoids the "opaque borderless-to-be" window flashing for one
    //    or two frames before the transparency/borderless settings apply.
    WindowCreateInfo win_info;
    win_info.title          = m_config.window.title;
    win_info.window_width   = m_config.window.width;
    win_info.window_height  = m_config.window.height;
    win_info.virtual_width  = m_config.window.virtual_width;
    win_info.virtual_height = m_config.window.virtual_height;
    win_info.resizable      = m_config.window.resizable;
    win_info.transparent    = m_config.window.transparent;
    win_info.borderless     = m_config.window.borderless;
    win_info.always_on_top  = m_config.window.always_on_top;
    win_info.click_through  = m_config.window.click_through;
    win_info.skip_taskbar   = m_config.window.skip_taskbar;
    win_info.not_focusable  = m_config.window.not_focusable;
    win_info.utility_window = m_config.window.utility_window;
    win_info.opacity        = m_config.window.opacity;

    // Overlay-style windows must not be mapped until the first correct frame
    // is ready — otherwise the OS shows an opaque/bordered placeholder.
    win_info.hidden_at_start =
        m_config.window.transparent ||
        m_config.window.borderless  ||
        m_config.window.click_through ||
        m_config.window.opacity < 1.0f;

    if (!m_window.init(win_info)) {
        CRAYON_LOG_ERROR("Engine failed to initialize Window");
        return false;
    }

    // Runtime-only settings (need GL context / scaling table).
    m_window.set_fullscreen(m_config.window.fullscreen);
    m_window.set_vsync(m_config.window.vsync);
    m_window.set_scaling_mode_string(m_config.window.scaling);
    if (m_config.window.min_width > 1 || m_config.window.min_height > 1) {
        m_window.set_window_min_size(m_config.window.min_width, m_config.window.min_height);
    }

    // 5. Initialize virtual FBO and Batch2D
    if (!m_fbo.init(m_config.window.virtual_width, m_config.window.virtual_height)) {
        CRAYON_LOG_ERROR("Engine failed to initialize virtual FBO");
        return false;
    }

    if (!m_post_process_chain.init(m_config.window.virtual_width, m_config.window.virtual_height)) {
        CRAYON_LOG_ERROR("Engine failed to initialize PostProcessChain");
        return false;
    }

    if (!m_batch2d.init()) {
        CRAYON_LOG_ERROR("Engine failed to initialize Batch2D");
        return false;
    }

    // 6. Conditionally initialize MeshRenderer3D
    if (m_config.modules.mesh3d) {
        if (!m_mesh_renderer.init()) {
            CRAYON_LOG_ERROR("Engine failed to initialize MeshRenderer3D");
            return false;
        }
        m_mesh_renderer_initialized = true;
    }

    // 7. Initialize post-processing shader
    m_post_shader = std::make_unique<Shader>();
    if (!m_post_shader->load_from_memory(SHADER_POST_VS, SHADER_POST_FS)) {
        CRAYON_LOG_ERROR("Engine failed to compile Post-Processing Shader");
        return false;
    }

    // 8. Conditionally initialize Jolt PhysicsSystem
    if (m_config.modules.physics3d) {
        m_physics = std::make_unique<PhysicsSystem>();
        if (!m_physics->init()) {
            CRAYON_LOG_ERROR("Engine failed to initialize PhysicsSystem");
            return false;
        }
    } else {
        m_physics.reset();
        CRAYON_LOG_INFO("Physics3D System skipped (disabled in crayon.config for optimization)");
    }

    // 8b. Conditionally initialize Box2D Physics2DSystem
    if (m_config.modules.physics2d) {
        m_physics2d = std::make_unique<Physics2DSystem>();
        if (!m_physics2d->init()) {
            CRAYON_LOG_ERROR("Engine failed to initialize Physics2DSystem");
            return false;
        }
    } else {
        m_physics2d.reset();
        CRAYON_LOG_INFO("Physics2DSystem skipped (disabled in crayon.config for optimization)");
    }

    // 8c. Conditionally initialize the hv4d 4D Physics4DSystem
    if (m_config.modules.physics4d) {
        m_physics4d = std::make_unique<Physics4DSystem>();
        if (!m_physics4d->init()) {
            CRAYON_LOG_ERROR("Engine failed to initialize Physics4DSystem");
            return false;
        }
    } else {
        m_physics4d.reset();
        CRAYON_LOG_INFO("Physics4DSystem skipped (disabled in crayon.config for optimization)");
    }

    // 9. Conditionally initialize AudioSystem
    if (m_config.modules.audio) {
        if (!m_audio.init()) {
            CRAYON_LOG_WARN("Engine failed to initialize AudioSystem (continuing without audio)");
        }
    } else {
        CRAYON_LOG_INFO("AudioSystem skipped (disabled in crayon.config for optimization)");
    }

    // 10. Register Lua modules based on config
    m_lua_runtime->register_modules(m_config.modules);

    // 11. Apply graphics config
    set_clear_color(m_config.graphics.clear_color.r,
                    m_config.graphics.clear_color.g,
                    m_config.graphics.clear_color.b,
                    m_config.graphics.clear_color.a);

    if (m_config.modules.mesh3d) {
        auto retro = m_mesh_renderer.get_retro_effects();
        if (m_config.graphics.dither) retro.dither_enabled = true;
        if (m_config.graphics.crt) retro.crt_scanlines = true;
        if (m_config.graphics.vignette) retro.vignette = true;
        m_mesh_renderer.set_retro_effects(retro);
    }

    // 12. Call game init
    if (script_loaded) {
        m_lua_runtime->call_init();
    }

    m_running = true;
    CRAYON_LOG_INFO("Crayon Engine initialized successfully (Title='{}', {}x{}, Virt: {}x{})",
        m_config.window.title, m_config.window.width, m_config.window.height,
        m_config.window.virtual_width, m_config.window.virtual_height);
    return true;
}

void Engine::shutdown() {
    m_running = false;
    m_texture_cache.clear();
    m_mesh_cache.clear();
    m_model_cache.clear();

    if (m_lua_runtime) {
        m_lua_runtime->shutdown();
        m_lua_runtime.reset();
    }
    if (m_physics) {
        m_physics->shutdown();
        m_physics.reset();
    }
    if (m_physics2d) {
        m_physics2d->shutdown();
        m_physics2d.reset();
    }
    if (m_physics4d) {
        m_physics4d->shutdown();
        m_physics4d.reset();
    }
    if (m_config.modules.audio) {
        m_audio.shutdown();
    }
    if (m_config.modules.mesh3d) {
        m_mesh_renderer.shutdown();
    }
    m_post_process_chain.shutdown();
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
    bool ok = false;
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.ends_with(".gltf") || lower.ends_with(".glb")) {
        ok = mesh->load_from_gltf(path);
    } else {
        ok = mesh->load_from_obj(path);
    }

    if (!ok) {
        CRAYON_LOG_WARN("Failed to load model '{}', falling back to procedural cube", path);
        mesh = Mesh3D::create_cube(1.0f);
    }

    m_mesh_cache[path] = mesh;
    return mesh;
}

std::shared_ptr<Model3D> Engine::load_model3d(const std::string& path) {
    auto it = m_model_cache.find(path);
    if (it != m_model_cache.end()) {
        return it->second;
    }

    auto model = std::make_shared<Model3D>();
    if (!model->load_from_file(path)) {
        CRAYON_LOG_WARN("Failed to load 3D model '{}', falling back to procedural cube", path);
        model = Model3D::create_from_mesh(Mesh3D::create_cube(1.0f), "fallback_cube");
    }

    m_model_cache[path] = model;
    return model;
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

void Engine::soft_restart() {
    if (m_game_script_path.empty()) return;
    if (!std::filesystem::exists(m_game_script_path)) {
        CRAYON_LOG_WARN("soft_restart: script no longer exists: {}", m_game_script_path);
        return;
    }

    CRAYON_LOG_INFO("Soft restart: reloading '{}'", m_game_script_path);

    // ---------------------------------------------------------------
    // 1. Snapshot old config so we can warn about non-reloadable fields.
    // ---------------------------------------------------------------
    const EngineConfig old_config = m_config;

    // ---------------------------------------------------------------
    // 2. Clear resource caches so edited textures/meshes/models reload.
    // ---------------------------------------------------------------
    m_texture_cache.clear();
    m_texture_sizes.clear();
    m_mesh_cache.clear();
    m_model_cache.clear();

    // ---------------------------------------------------------------
    // 3. Tear down everything that owns runtime state.
    //    Lua first (so user cleanup runs), then subsystems.
    //    The SDL window + GL context stay alive.
    // ---------------------------------------------------------------
    if (m_lua_runtime) {
        m_lua_runtime->shutdown();
        m_lua_runtime.reset();
    }

    if (m_physics)   { m_physics->shutdown();   m_physics.reset();   }
    if (m_physics2d) { m_physics2d->shutdown(); m_physics2d.reset(); }
    if (m_physics4d) { m_physics4d->shutdown(); m_physics4d.reset(); }

    // Audio: only shut down if it was actually running.
    if (old_config.modules.audio) {
        m_audio.shutdown();
    }

    m_physics_accumulator   = 0.0f;
    m_physics2d_accumulator = 0.0f;
    m_physics4d_accumulator = 0.0f;

    // ---------------------------------------------------------------
    // 4. Reset config to defaults, re-apply CLI overrides, re-run
    //    crayon.config() on a fresh Lua state.
    // ---------------------------------------------------------------
    m_config = EngineConfig{};

    if (!m_cli_title.empty())    m_config.window.title          = m_cli_title;
    if (m_cli_window_w  > 0)     m_config.window.width          = m_cli_window_w;
    if (m_cli_window_h  > 0)     m_config.window.height         = m_cli_window_h;
    if (m_cli_virtual_w > 0)     m_config.window.virtual_width  = m_cli_virtual_w;
    if (m_cli_virtual_h > 0)     m_config.window.virtual_height = m_cli_virtual_h;

    m_lua_runtime = std::make_unique<LuaRuntime>();
    if (!m_lua_runtime->init()) {
        CRAYON_LOG_ERROR("soft_restart: failed to re-create LuaRuntime");
        return;
    }

    // run_config_phase loads AND executes the script chunk, then reads back t.*
    if (!m_lua_runtime->run_config_phase(m_game_script_path, m_config)) {
        CRAYON_LOG_ERROR("soft_restart: config phase failed — aborting");
        return;
    }
    
    // Apply the (possibly changed) console flag.
    set_console_enabled(m_config.console);

    // ---------------------------------------------------------------
    // 5. Warn about fields that can only take effect on a full restart.
    // ---------------------------------------------------------------
    if (m_config.window.transparent != old_config.window.transparent) {
        CRAYON_LOG_WARN("soft_restart: 'transparent' changed — requires full restart");
    }
    if (m_config.window.utility_window != old_config.window.utility_window) {
        CRAYON_LOG_WARN("soft_restart: 'utilityWindow' changed — requires full restart");
    }
    if (m_config.window.skip_taskbar != old_config.window.skip_taskbar) {
        CRAYON_LOG_WARN("soft_restart: 'skipTaskbar' changed — requires full restart");
    }
    if (m_config.window.not_focusable != old_config.window.not_focusable) {
        CRAYON_LOG_WARN("soft_restart: 'notFocusable' changed — requires full restart");
    }

    // ---------------------------------------------------------------
    // 6. Apply runtime-safe window settings.
    // ---------------------------------------------------------------
    if (m_config.window.virtual_width > 0 && m_config.window.virtual_height > 0) {
        m_window.set_resolution(m_config.window.virtual_width,
                                m_config.window.virtual_height);
    }
    if (m_config.window.width > 0 && m_config.window.height > 0) {
        m_window.set_window_size(m_config.window.width, m_config.window.height);
    }
    m_window.set_scaling_mode_string(m_config.window.scaling);
    m_window.set_title(m_config.window.title);
    m_window.set_resizable(m_config.window.resizable);
    m_window.set_bordered(!m_config.window.borderless);
    m_window.set_always_on_top(m_config.window.always_on_top);
    m_window.set_click_through(m_config.window.click_through);
    m_window.set_skip_taskbar(m_config.window.skip_taskbar);
    m_window.set_not_focusable(m_config.window.not_focusable);
    m_window.set_opacity(m_config.window.opacity);

    if (m_config.window.min_width > 1 || m_config.window.min_height > 1) {
        m_window.set_window_min_size(m_config.window.min_width,
                                     m_config.window.min_height);
    }

    // ---------------------------------------------------------------
    // 7. Rebuild FBO + post-process chain for the (possibly new) virtual size.
    // ---------------------------------------------------------------
    m_fbo.shutdown();
    if (!m_fbo.init(m_config.window.virtual_width, m_config.window.virtual_height)) {
        CRAYON_LOG_ERROR("soft_restart: failed to re-init FBO");
        return;
    }

    m_post_process_chain.shutdown();
    if (!m_post_process_chain.init(m_config.window.virtual_width,
                                   m_config.window.virtual_height)) {
        CRAYON_LOG_ERROR("soft_restart: failed to re-init PostProcessChain");
        return;
    }

    // ---------------------------------------------------------------
    // 8. MeshRenderer3D: init if newly enabled, shutdown if newly disabled.
    // ---------------------------------------------------------------
    if (m_config.modules.mesh3d && !m_mesh_renderer_initialized) {
        if (!m_mesh_renderer.init()) {
            CRAYON_LOG_ERROR("soft_restart: failed to re-init MeshRenderer3D");
            return;
        }
        m_mesh_renderer_initialized = true;
    } else if (!m_config.modules.mesh3d && m_mesh_renderer_initialized) {
        m_mesh_renderer.shutdown();
        m_mesh_renderer_initialized = false;
    }

    // ---------------------------------------------------------------
    // 9. Physics 3D / 2D / audio: rebuild per the new config.
    // ---------------------------------------------------------------
    if (m_config.modules.physics3d) {
        m_physics = std::make_unique<PhysicsSystem>();
        if (!m_physics->init()) {
            CRAYON_LOG_ERROR("soft_restart: failed to re-init PhysicsSystem");
            m_physics.reset();
        }
    }

    if (m_config.modules.physics2d) {
        m_physics2d = std::make_unique<Physics2DSystem>();
        if (!m_physics2d->init()) {
            CRAYON_LOG_ERROR("soft_restart: failed to re-init Physics2DSystem");
            m_physics2d.reset();
        }
    }

    if (m_config.modules.physics4d) {
        m_physics4d = std::make_unique<Physics4DSystem>();
        if (!m_physics4d->init()) {
            CRAYON_LOG_ERROR("soft_restart: failed to re-init Physics4DSystem");
            m_physics4d.reset();
        }
    }

    if (m_config.modules.audio) {
        if (!m_audio.init()) {
            CRAYON_LOG_WARN("soft_restart: audio init failed (continuing without)");
        }
    }

    // ---------------------------------------------------------------
    // 10. Re-apply graphics config.
    // ---------------------------------------------------------------
    set_clear_color(m_config.graphics.clear_color.r,
                    m_config.graphics.clear_color.g,
                    m_config.graphics.clear_color.b,
                    m_config.graphics.clear_color.a);

    if (m_config.modules.mesh3d && m_mesh_renderer_initialized) {
        auto retro = m_mesh_renderer.get_retro_effects();
        retro.dither_enabled = m_config.graphics.dither;
        retro.crt_scanlines  = m_config.graphics.crt;
        retro.vignette       = m_config.graphics.vignette;
        m_mesh_renderer.set_retro_effects(retro);
    }

    // ---------------------------------------------------------------
    // 11. Register Lua modules for the new config, then run crayon.init().
    //     (run_config_phase already executed the top-level chunk.)
    // ---------------------------------------------------------------
    m_lua_runtime->register_modules(m_config.modules);
    m_lua_runtime->call_init();

    // ---------------------------------------------------------------
    // 12. Remember the new mtime and clear the pending flag.
    // ---------------------------------------------------------------
    m_last_script_write_time = std::filesystem::last_write_time(m_game_script_path);
    m_hot_reload_requested   = false;

    CRAYON_LOG_INFO("Soft restart complete");
}

void Engine::render_to_fbo() {
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

    if (m_config.modules.mesh3d) {
        m_mesh_renderer.begin(m_camera, aspect);
    }
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
    if (m_config.modules.mesh3d) {
        m_mesh_renderer.end();
    }

    m_fbo.unbind();
}

void Engine::step_simulation(float dt) {
    if (m_physics && m_config.modules.physics3d) {
        const float fixed_dt = 1.0f / 60.0f;
        m_physics_accumulator += dt;
        if (m_physics_accumulator > 0.2f) m_physics_accumulator = 0.2f;
        while (m_physics_accumulator >= fixed_dt) {
            m_physics->update(fixed_dt);
            m_physics_accumulator -= fixed_dt;
        }

        if (m_lua_runtime && !m_game_script_path.empty()) {
            auto events = m_physics->get_and_clear_collision_events();
            for (const auto& evt : events) {
                switch (evt.type) {
                    case PhysicsEventType::CollisionEnter:
                        m_lua_runtime->call_collision_enter(evt.body_a, evt.body_b, evt.normal.x, evt.normal.y, evt.normal.z, evt.impulse);
                        break;
                    case PhysicsEventType::CollisionExit:
                        m_lua_runtime->call_collision_exit(evt.body_a, evt.body_b);
                        break;
                    case PhysicsEventType::TriggerEnter:
                        m_lua_runtime->call_trigger_enter(evt.body_a, evt.body_b);
                        break;
                    case PhysicsEventType::TriggerExit:
                        m_lua_runtime->call_trigger_exit(evt.body_a, evt.body_b);
                        break;
                }
            }
        }
    }

    if (m_physics2d && m_config.modules.physics2d) {
        const float fixed_dt = 1.0f / 60.0f;
        m_physics2d_accumulator += dt;
        if (m_physics2d_accumulator > 0.2f) m_physics2d_accumulator = 0.2f;
        while (m_physics2d_accumulator >= fixed_dt) {
            m_physics2d->update(fixed_dt);
            m_physics2d_accumulator -= fixed_dt;
        }

        if (m_lua_runtime && !m_game_script_path.empty()) {
            auto events = m_physics2d->getAndClearEvents();
            for (const auto& evt : events) {
                switch (evt.type) {
                    case Physics2DEventType::CollisionEnter:
                        m_lua_runtime->call_collision2d_enter(evt.bodyA, evt.bodyB, evt.normal.x, evt.normal.y, evt.impulse);
                        break;
                    case Physics2DEventType::CollisionExit:
                        m_lua_runtime->call_collision2d_exit(evt.bodyA, evt.bodyB);
                        break;
                    case Physics2DEventType::TriggerEnter:
                        m_lua_runtime->call_trigger2d_enter(evt.bodyA, evt.bodyB);
                        break;
                    case Physics2DEventType::TriggerExit:
                        m_lua_runtime->call_trigger2d_exit(evt.bodyA, evt.bodyB);
                        break;
                }
            }
        }
    }

    if (m_physics4d && m_config.modules.physics4d) {
        const float fixed_dt = 1.0f / 60.0f;
        m_physics4d_accumulator += dt;
        if (m_physics4d_accumulator > 0.2f) m_physics4d_accumulator = 0.2f;
        while (m_physics4d_accumulator >= fixed_dt) {
            m_physics4d->update(fixed_dt);
            m_physics4d_accumulator -= fixed_dt;
        }

        // Always drain the queue (so it can't grow without bound), but only dispatch when a script is running.
        auto events = m_physics4d->get_and_clear_events();
        if (m_lua_runtime && !m_game_script_path.empty()) {
            for (const auto& evt : events) {
                switch (evt.type) {
                    case Physics4DEventType::CollisionEnter:
                        m_lua_runtime->call_collision4d_enter(evt.bodyA, evt.bodyB, evt.normal.x, evt.normal.y,
                                                              evt.normal.z, evt.normal.w, evt.impulse);
                        break;
                    case Physics4DEventType::CollisionExit:
                        m_lua_runtime->call_collision4d_exit(evt.bodyA, evt.bodyB);
                        break;
                    case Physics4DEventType::TriggerEnter:
                        m_lua_runtime->call_trigger4d_enter(evt.bodyA, evt.bodyB);
                        break;
                    case Physics4DEventType::TriggerExit:
                        m_lua_runtime->call_trigger4d_exit(evt.bodyA, evt.bodyB);
                        break;
                }
            }
        }
    }

    if (m_config.modules.audio) {
        m_audio.update(dt);
    }

    if (!m_game_script_path.empty()) {
        m_lua_runtime->call_update(dt);
    }
}

void Engine::render_frame() {
    render_to_fbo();

    // 2. Post-Process and Blit to Window
    int win_w = 960, win_h = 720;
    m_window.get_window_size(win_w, win_h);
    PostProcessOptions opts;
    if (m_config.modules.mesh3d) {
        const auto& retro = m_mesh_renderer.get_retro_effects();
        opts.dither_enabled = retro.dither_enabled;
        opts.dither_levels = retro.dither_levels;
        opts.crt_scanlines = retro.crt_scanlines;
        opts.scanline_strength = retro.scanline_strength;
        opts.crt_curvature = retro.crt_curvature;
        opts.curvature_distort = retro.curvature_distort;
        opts.vignette = retro.vignette;
        opts.vignette_strength = retro.vignette_strength;
    } else {
        opts.dither_enabled = m_config.graphics.dither;
        opts.crt_scanlines = m_config.graphics.crt;
        opts.vignette = m_config.graphics.vignette;
    }
    opts.transparent = m_window.is_transparent();

    GLuint final_texture = m_fbo.get_color_texture();
    if (m_post_process_chain.hasActiveEffects()) {
        final_texture = m_post_process_chain.process(
            final_texture,
            m_config.window.virtual_width,
            m_config.window.virtual_height,
            static_cast<float>(m_total_time)
        );
    }

    m_fbo.blit_to_screen(m_window.get_viewport_info(), win_w, win_h, *m_post_shader, opts, final_texture);

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

        // Hot Reload check — full soft restart so config, caches,
        // and subsystem state are all rebuilt.
        if (check_hot_reload()) {
            soft_restart();
        }

        // Process Input and Window Events
        m_input.begin_frame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            m_window.handle_event(event);
            m_input.handle_event(event, m_window);

            if (m_lua_runtime && !m_game_script_path.empty()) {
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    const char* name = SDL_GetKeyName(event.key.key);
                    if (name && name[0] != '\0') {
                        std::string k = m_input.normalize_key(name);
                        m_lua_runtime->call_key_down(k, event.key.repeat != 0);
                    }
                } else if (event.type == SDL_EVENT_KEY_UP) {
                    const char* name = SDL_GetKeyName(event.key.key);
                    if (name && name[0] != '\0') {
                        std::string k = m_input.normalize_key(name);
                        m_lua_runtime->call_key_up(k);
                    }
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    float vx = 0.0f, vy = 0.0f;
                    m_window.window_to_virtual(event.button.x, event.button.y, vx, vy);
                    int btn = 1;
                    if (event.button.button == SDL_BUTTON_LEFT) btn = 1;
                    else if (event.button.button == SDL_BUTTON_RIGHT) btn = 2;
                    else if (event.button.button == SDL_BUTTON_MIDDLE) btn = 3;
                    else if (event.button.button == SDL_BUTTON_X1) btn = 4;
                    else if (event.button.button == SDL_BUTTON_X2) btn = 5;
                    m_lua_runtime->call_mouse_down(vx, vy, btn);
                } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    float vx = 0.0f, vy = 0.0f;
                    m_window.window_to_virtual(event.button.x, event.button.y, vx, vy);
                    int btn = 1;
                    if (event.button.button == SDL_BUTTON_LEFT) btn = 1;
                    else if (event.button.button == SDL_BUTTON_RIGHT) btn = 2;
                    else if (event.button.button == SDL_BUTTON_MIDDLE) btn = 3;
                    else if (event.button.button == SDL_BUTTON_X1) btn = 4;
                    else if (event.button.button == SDL_BUTTON_X2) btn = 5;
                    m_lua_runtime->call_mouse_up(vx, vy, btn);
                } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    float vx = 0.0f, vy = 0.0f;
                    m_window.window_to_virtual(event.motion.x, event.motion.y, vx, vy);
                    ViewportInfo vp = m_window.get_viewport_info();
                    float scale_x = (vp.width > 0 && m_window.get_virtual_width() > 0) ?
                        static_cast<float>(vp.width) / static_cast<float>(m_window.get_virtual_width()) : 1.0f;
                    float scale_y = (vp.height > 0 && m_window.get_virtual_height() > 0) ?
                        static_cast<float>(vp.height) / static_cast<float>(m_window.get_virtual_height()) : 1.0f;
                    float vdx = (scale_x > 0.0f) ? (event.motion.xrel / scale_x) : event.motion.xrel;
                    float vdy = (scale_y > 0.0f) ? (event.motion.yrel / scale_y) : event.motion.yrel;
                    m_lua_runtime->call_mouse_moved(vx, vy, vdx, vdy);
                } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                    float wx = event.wheel.x;
                    float wy = event.wheel.y;
                    if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                        wx = -wx;
                        wy = -wy;
                    }
                    m_lua_runtime->call_wheel_moved(wx, wy);
                } else if (event.type == SDL_EVENT_TEXT_INPUT) {
                    if (event.text.text) {
                        m_lua_runtime->call_text_input(event.text.text);
                    }
                                } else if (event.type == SDL_EVENT_DROP_BEGIN) {
                    float vx = 0.0f, vy = 0.0f;
                    m_window.window_to_virtual(event.drop.x, event.drop.y, vx, vy);
                    m_lua_runtime->call_drop_begin(vx, vy);
                } else if (event.type == SDL_EVENT_DROP_FILE) {
                    float vx = 0.0f, vy = 0.0f;
                    m_window.window_to_virtual(event.drop.x, event.drop.y, vx, vy);
                    std::string path = event.drop.data ? event.drop.data : "";
                    m_lua_runtime->call_drop_file(path, vx, vy);
                } else if (event.type == SDL_EVENT_DROP_TEXT) {
                    float vx = 0.0f, vy = 0.0f;
                    m_window.window_to_virtual(event.drop.x, event.drop.y, vx, vy);
                    std::string txt = event.drop.data ? event.drop.data : "";
                    m_lua_runtime->call_drop_text(txt, vx, vy);
                } else if (event.type == SDL_EVENT_DROP_POSITION) {
                    float vx = 0.0f, vy = 0.0f;
                    m_window.window_to_virtual(event.drop.x, event.drop.y, vx, vy);
                    m_lua_runtime->call_drop_position(vx, vy);
                } else if (event.type == SDL_EVENT_DROP_COMPLETE) {
                    m_lua_runtime->call_drop_complete();
                } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                    m_lua_runtime->call_gamepad_down(event.gbutton.button);
                } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
                    m_lua_runtime->call_gamepad_up(event.gbutton.button);
                } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                    float val = static_cast<float>(event.gaxis.value) / 32767.0f;
                    m_lua_runtime->call_gamepad_axis(event.gaxis.axis, val);
                } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                    m_lua_runtime->call_window_resized(event.window.data1, event.window.data2);
                } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                    m_lua_runtime->call_focus_changed(true);
                } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                    m_lua_runtime->call_focus_changed(false);
                } else if (event.type == SDL_EVENT_QUIT) {
                    m_lua_runtime->call_quit();
                }
            }

            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F5) {
                request_hot_reload();
            }
        }

        if (m_window.should_close()) {
            break;
        }

        // Simulation update
        if (!m_paused) {
            step_simulation(m_delta_time);
        }
        if (m_game_script_path.empty()) {
            if (m_input.is_key_pressed("escape")) {
                m_running = false;
            }
        }

        // Render Virtual Canvas & Blit
        render_frame();

        // Reveal the window only after the first fully-prepared frame has
        // been presented — this is what eliminates the transparent/borderless
        // "pop-in" flash.
        if (!m_first_present_done) {
            m_first_present_done = true;
            m_window.show();
        }

        // FPS limiting if configured
        if (m_config.fps_limit > 0) {
            double target_frame_time = 1.0 / static_cast<double>(m_config.fps_limit);
            auto frame_end = clock::now();
            std::chrono::duration<double> frame_duration = frame_end - current_time;
            if (frame_duration.count() < target_frame_time) {
                double sleep_sec = target_frame_time - frame_duration.count();
                SDL_Delay(static_cast<Uint32>(sleep_sec * 1000.0));
            }
        }
    }
}

} // namespace crayon
