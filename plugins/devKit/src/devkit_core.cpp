#include "devkit_core.hpp"
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
typedef void (*PFN_glBindFramebuffer)(GLenum target, GLuint framebuffer);
#else
#include <GL/gl.h>
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#if defined(_WIN32)
typedef void (APIENTRY *PFN_glBindFramebuffer)(GLenum target, GLuint framebuffer);
#else
typedef void (*PFN_glBindFramebuffer)(GLenum target, GLuint framebuffer);
#endif
#endif

static PFN_glBindFramebuffer g_glBindFramebuffer = nullptr;

#include <lua.hpp>
#include <cstring>
#include <string>
#include <sstream>
#include <algorithm>

#if defined(_WIN32)
typedef void (SDLCALL *PFN_SDL_GL_SwapWindow)(SDL_Window* window);
static PFN_SDL_GL_SwapWindow g_orig_SDL_GL_SwapWindow = nullptr;

static void SDLCALL Hooked_SDL_GL_SwapWindow(SDL_Window* window) {
    DevKit::instance().on_pre_swap(window);
    if (g_orig_SDL_GL_SwapWindow) {
        g_orig_SDL_GL_SwapWindow(window);
    } else {
        SDL_GL_SwapWindow(window);
    }
}

static bool HookIAT(HMODULE hModule, const char* dllName, const char* funcName, void* hookFunc, void** origFunc) {
    if (!hModule) return false;
    auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

    auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(hModule) + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

    IMAGE_DATA_DIRECTORY importDataDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDataDir.VirtualAddress == 0 || importDataDir.Size == 0) return false;

    auto importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(reinterpret_cast<uint8_t*>(hModule) + importDataDir.VirtualAddress);
    for (; importDesc->Name != 0; importDesc++) {
        const char* currentDll = reinterpret_cast<const char*>(reinterpret_cast<uint8_t*>(hModule) + importDesc->Name);
        if (_stricmp(currentDll, dllName) == 0) {
            auto thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<uint8_t*>(hModule) + importDesc->FirstThunk);
            auto origThunk = (importDesc->OriginalFirstThunk != 0)
                ? reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<uint8_t*>(hModule) + importDesc->OriginalFirstThunk)
                : thunk;

            for (; thunk->u1.Function != 0; thunk++, origThunk++) {
                if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) continue;
                auto importByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(reinterpret_cast<uint8_t*>(hModule) + origThunk->u1.AddressOfData);
                if (strcmp(reinterpret_cast<const char*>(importByName->Name), funcName) == 0) {
                    DWORD oldProtect = 0;
                    if (VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                        if (origFunc && !*origFunc) {
                            *origFunc = reinterpret_cast<void*>(thunk->u1.Function);
                        }
                        thunk->u1.Function = reinterpret_cast<uintptr_t>(hookFunc);
                        VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
#endif

static bool SDLCALL DevKit_EventWatch(void* userdata, SDL_Event* event) {
    (void)userdata;
    return DevKit::instance().handle_event(event);
}

DevKit& DevKit::instance() {
    static DevKit s_inst;
    return s_inst;
}

DevKit::DevKit() {
    m_frame_times.resize(120, 16.6f);
}

DevKit::~DevKit() {
    shutdown();
}

void DevKit::log(int level, const char* msg) {
    if (m_host && m_host->log) {
        m_host->log(level, msg);
    }
}

bool DevKit::init(const CrayonHostAPI* host) {
    m_host = host;
    log(CRAYON_LOG_LEVEL_INFO, "[devKit] Initializing DevKit Plugin (Press F3 or ` to toggle)...");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    apply_theme();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    m_imgui_context_created = true;

    add_log("Crayon Engine DevKit Initialized. Type help() or Lua code below.");

    install_hooks();
    SDL_AddEventWatch(DevKit_EventWatch, nullptr);

    return true;
}

void DevKit::shutdown() {
    remove_hooks();
    SDL_RemoveEventWatch(DevKit_EventWatch, nullptr);

    if (m_initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        m_initialized = false;
    }
    if (m_imgui_context_created) {
        ImGui::DestroyContext();
        m_imgui_context_created = false;
        m_frame_active = false;
        log(CRAYON_LOG_LEVEL_INFO, "[devKit] Shutdown complete.");
    }
}

bool DevKit::install_hooks() {
#if defined(_WIN32)
    HMODULE mainExe = GetModuleHandleA(nullptr);
    if (HookIAT(mainExe, "SDL3.dll", "SDL_GL_SwapWindow",
                reinterpret_cast<void*>(Hooked_SDL_GL_SwapWindow),
                reinterpret_cast<void**>(&g_orig_SDL_GL_SwapWindow))) {
        m_hook_installed = true;
        log(CRAYON_LOG_LEVEL_INFO, "[devKit] IAT hook installed for SDL_GL_SwapWindow.");
        return true;
    }
    log(CRAYON_LOG_LEVEL_WARN, "[devKit] Warning: Could not install IAT hook for SDL_GL_SwapWindow.");
#endif
    return false;
}

void DevKit::remove_hooks() {
#if defined(_WIN32)
    if (m_hook_installed && g_orig_SDL_GL_SwapWindow) {
        HMODULE mainExe = GetModuleHandleA(nullptr);
        HookIAT(mainExe, "SDL3.dll", "SDL_GL_SwapWindow",
                reinterpret_cast<void*>(g_orig_SDL_GL_SwapWindow), nullptr);
        m_hook_installed = false;
    }
#endif
}

bool DevKit::handle_event(const SDL_Event* event) {
    if (!event) return true;

    // Hotkey toggle (F3 or backtick / tilde)
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        if (event->key.key == m_toggle_key || event->key.key == SDLK_GRAVE) {
            toggle_visible();
            return true;
        }
    }

    if (m_initialized && m_visible) {
        ImGui_ImplSDL3_ProcessEvent(event);
    }
    return true;
}

void DevKit::apply_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.09f, 0.12f, 0.96f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.10f, 0.12f, 0.15f, 0.85f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.09f, 0.10f, 0.13f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.35f, 0.50f, 0.65f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.14f, 0.16f, 0.20f, 0.90f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.20f, 0.25f, 0.32f, 0.90f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.25f, 0.32f, 0.40f, 0.95f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.07f, 0.14f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.10f, 0.22f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.06f, 0.10f, 0.15f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.06f, 0.07f, 0.09f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.22f, 0.30f, 0.40f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.28f, 0.38f, 0.50f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.35f, 0.48f, 0.65f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.25f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.20f, 0.70f, 0.95f, 0.90f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.30f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.14f, 0.32f, 0.52f, 0.85f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.20f, 0.42f, 0.68f, 0.95f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.24f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.14f, 0.30f, 0.48f, 0.75f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.20f, 0.40f, 0.62f, 0.85f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.24f, 0.48f, 0.75f, 0.95f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.35f, 0.50f, 0.65f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.12f, 0.18f, 0.25f, 0.90f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.22f, 0.38f, 0.58f, 0.90f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.16f, 0.32f, 0.52f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.25f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.40f, 0.40f, 1.00f);
}

void DevKit::ensure_frame_started() {
    if (!m_imgui_context_created) return;

    if (!m_initialized) {
        SDL_Window* win = m_window ? m_window : SDL_GL_GetCurrentWindow();
        SDL_GLContext ctx = m_gl_context ? m_gl_context : SDL_GL_GetCurrentContext();
        if (win && ctx) {
            m_window = win;
            m_gl_context = ctx;
            ImGui_ImplSDL3_InitForOpenGL(win, ctx);
            ImGui_ImplOpenGL3_Init("#version 130");
            m_initialized = true;
        }
    }

    if (!m_frame_active) {
        if (m_initialized) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            m_frame_active = true;
        } else {
            ImGuiIO& io = ImGui::GetIO();
            if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
                io.DisplaySize = ImVec2(1280.0f, 720.0f);
            }
            ImGui::NewFrame();
            m_frame_active = true;
        }
    }
}

void DevKit::on_update(lua_State* L, float dt) {
    m_L = L;
    m_dt = dt;

    // Track frame timing metrics
    float frame_ms = dt * 1000.0f;
    m_frame_times.erase(m_frame_times.begin());
    m_frame_times.push_back(frame_ms);

    m_fps_accumulator += dt;
    m_fps_counter++;
    if (m_fps_accumulator >= 0.5f) {
        m_current_fps = static_cast<float>(m_fps_counter) / m_fps_accumulator;
        m_fps_accumulator = 0.0f;
        m_fps_counter = 0;
    }

    ensure_frame_started();
}

void DevKit::on_pre_swap(SDL_Window* window) {
    if (!window) return;
    m_window = window;

    if (!m_initialized) {
        SDL_GLContext ctx = SDL_GL_GetCurrentContext();
        if (!ctx) return;
        m_gl_context = ctx;

        ImGui_ImplSDL3_InitForOpenGL(window, ctx);
        ImGui_ImplOpenGL3_Init("#version 130");
        m_initialized = true;
    }

    if (m_frame_active) {
        if (m_visible) {
            render_ui(m_L);
        }

        ImGui::Render();
        m_frame_active = false;

        // Render directly to screen backbuffer (framebuffer 0)
        GLint last_fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_fbo);
        GLint last_viewport[4];
        glGetIntegerv(GL_VIEWPORT, last_viewport);

        if (!g_glBindFramebuffer) {
            g_glBindFramebuffer = reinterpret_cast<PFN_glBindFramebuffer>(SDL_GL_GetProcAddress("glBindFramebuffer"));
        }

        if (g_glBindFramebuffer) {
            g_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        glViewport(0, 0, w, h);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (g_glBindFramebuffer) {
            g_glBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
        }
        glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    }
}

bool DevKit::consume_step_frame() {
    if (m_step_frame_requested) {
        m_step_frame_requested = false;
        return true;
    }
    return false;
}

void DevKit::add_log(const std::string& text, bool is_error) {
    m_logs.push_back({text, is_error});
    if (m_logs.size() > 500) {
        m_logs.pop_front();
    }
    m_scroll_to_bottom = true;
}

void DevKit::clear_logs() {
    m_logs.clear();
}

void DevKit::add_custom_tab(const std::string& name, int lua_ref) {
    m_custom_tabs.push_back({name, lua_ref});
}

// -----------------------------------------------------------------------------
// UI Rendering
// -----------------------------------------------------------------------------
void DevKit::render_ui(lua_State* L) {
    ImGui::SetNextWindowSize(ImVec2(720, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Crayon DevKit :: Live Engine Inspector [F3 to toggle]", &m_visible)) {
        if (ImGui::BeginTabBar("DevKitTabs")) {
            if (ImGui::BeginTabItem("Profiler")) {
                render_profiler_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Time & Stepper")) {
                render_time_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Lua Console")) {
                render_console_tab(L);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Visuals & Shaders")) {
                render_visuals_tab(L);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("System")) {
                render_system_tab();
                ImGui::EndTabItem();
            }
            render_custom_tabs(L);

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void DevKit::render_profiler_tab() {
    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "LIVE PERFORMANCE METRICS");
    ImGui::Separator();

    ImGui::Text("FPS: %.1f  |  Frame Time: %.2f ms", m_current_fps, m_dt * 1000.0f);

    float min_ms = 999.0f, max_ms = 0.0f, avg_ms = 0.0f;
    for (float f : m_frame_times) {
        if (f < min_ms) min_ms = f;
        if (f > max_ms) max_ms = f;
        avg_ms += f;
    }
    avg_ms /= (float)m_frame_times.size();

    ImGui::Text("Min: %.2f ms  |  Max: %.2f ms  |  Avg: %.2f ms", min_ms, max_ms, avg_ms);

    ImGui::PlotLines("Frame History (ms)", m_frame_times.data(), (int)m_frame_times.size(),
                     0, nullptr, 0.0f, 33.3f, ImVec2(0, 100));

    ImGui::Spacing();
    ImGui::Separator();

    ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::Text("Physical Window: %.0f x %.0f", disp.x, disp.y);
}

void DevKit::render_time_tab() {
    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "SIMULATION TIME & STEPPING");
    ImGui::Separator();

    if (m_paused) {
        if (ImGui::Button("RESUME SIMULATION", ImVec2(180, 36))) {
            m_paused = false;
        }
    } else {
        if (ImGui::Button("PAUSE SIMULATION", ImVec2(180, 36))) {
            m_paused = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("STEP 1 FRAME", ImVec2(140, 36))) {
        m_step_frame_requested = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Advances the game by exactly 1 tick while paused");
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text("Time Dilation Scale: %.2fx", m_time_scale);
    ImGui::SliderFloat("Scale", &m_time_scale, 0.05f, 3.0f, "%.2fx");

    ImGui::Spacing();
    ImGui::Text("Presets:");
    if (ImGui::SmallButton("0.1x Slow-Mo")) m_time_scale = 0.1f;
    ImGui::SameLine();
    if (ImGui::SmallButton("0.5x Half"))    m_time_scale = 0.5f;
    ImGui::SameLine();
    if (ImGui::SmallButton("1.0x Realtime"))m_time_scale = 1.0f;
    ImGui::SameLine();
    if (ImGui::SmallButton("2.0x Double"))  m_time_scale = 2.0f;
}

void DevKit::render_console_tab(lua_State* L) {
    if (ImGui::Button("Clear Log")) {
        clear_logs();
    }

    ImGui::Separator();

    // Scrollable log area
    float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : m_logs) {
        if (entry.is_error) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", entry.text.c_str());
        } else {
            ImGui::TextUnformatted(entry.text.c_str());
        }
    }

    if (m_scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        m_scroll_to_bottom = false;
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Command input
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (ImGui::InputText("##CommandInput", m_input_buffer, sizeof(m_input_buffer), input_flags)) {
        if (m_input_buffer[0] != '\0' && L) {
            std::string cmd = m_input_buffer;
            add_log("> " + cmd, false);
            m_history.push_back(cmd);

            // Execute in Lua
            int err = luaL_dostring(L, cmd.c_str());
            if (err != 0) {
                const char* msg = lua_tostring(L, -1);
                add_log(msg ? msg : "Error executing Lua", true);
                lua_pop(L, 1);
            } else {
                if (lua_gettop(L) > 0) {
                    const char* res = lua_tostring(L, -1);
                    if (res) add_log("= " + std::string(res), false);
                    lua_pop(L, 1);
                }
            }

            m_input_buffer[0] = '\0';
            reclaim_focus = true;
        }
    }

    ImGui::SetItemDefaultFocus();
    if (reclaim_focus) {
        ImGui::SetKeyboardFocusHere(-1);
    }
}

void DevKit::render_visuals_tab(lua_State* L) {
    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "RETRO SHADERS & VISUAL TUNING");
    ImGui::Separator();

    bool changed = false;
    if (ImGui::Checkbox("CRT Scanlines", &m_crt_enabled)) changed = true;
    if (m_crt_enabled) {
        if (ImGui::SliderFloat("Scanline Strength", &m_scanline_strength, 0.0f, 1.0f, "%.2f")) changed = true;
        if (ImGui::SliderFloat("Curvature Distortion", &m_curvature, 0.0f, 0.25f, "%.3f")) changed = true;
        if (ImGui::SliderFloat("Vignette Intensity", &m_vignette, 0.0f, 1.0f, "%.2f")) changed = true;
    }

    if (ImGui::Checkbox("Dithering Filter", &m_dither_enabled)) changed = true;

    if (changed && L) {
        // Call crayon.graphics.setRetroEffects if available
        lua_getglobal(L, "crayon");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "graphics");
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "setRetroEffects");
                if (lua_isfunction(L, -1)) {
                    lua_newtable(L);
                    lua_pushnumber(L, m_crt_enabled ? m_scanline_strength : 0.0);
                    lua_setfield(L, -2, "scanlines");
                    lua_pushnumber(L, m_crt_enabled ? m_curvature : 0.0);
                    lua_setfield(L, -2, "curvature");
                    lua_pushnumber(L, m_crt_enabled ? m_vignette : 0.0);
                    lua_setfield(L, -2, "vignette");
                    lua_pushboolean(L, m_dither_enabled);
                    lua_setfield(L, -2, "dither");

                    lua_pcall(L, 1, 0, 0);
                } else {
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
}

void DevKit::render_system_tab() {
    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "SYSTEM & ENVIRONMENT");
    ImGui::Separator();

    ImGui::Text("Plugin: devKit v1.0.0");
    ImGui::Text("Compiler: C++20 (GCC/MinGW)");
    ImGui::Text("UI Backend: Dear ImGui (SDL3 + OpenGL3)");
    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text("Hotkey: F3 (or ` / ~)");
    ImGui::Text("Status: Overlay rendering to physical screen backbuffer");
}

void DevKit::render_custom_tabs(lua_State* L) {
    if (!L) return;

    for (const auto& tab : m_custom_tabs) {
        if (tab.lua_ref != -1) {
            if (ImGui::BeginTabItem(tab.name.c_str())) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, tab.lua_ref);
                if (lua_isfunction(L, -1)) {
                    if (lua_pcall(L, 0, 0, 0) != 0) {
                        const char* err = lua_tostring(L, -1);
                        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", err ? err : "unknown");
                        lua_pop(L, 1);
                    }
                } else {
                    lua_pop(L, 1);
                }
                ImGui::EndTabItem();
            }
        }
    }
}

bool DevKit::want_capture_mouse() const {
    if (!m_imgui_context_created || !m_visible) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool DevKit::want_capture_keyboard() const {
    if (!m_imgui_context_created || !m_visible) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}
