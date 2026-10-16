#include "simple_ui_core.hpp"
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

#include <cstring>
#include <string>

#if defined(_WIN32)
typedef void (SDLCALL *PFN_SDL_GL_SwapWindow)(SDL_Window* window);
static PFN_SDL_GL_SwapWindow g_orig_SDL_GL_SwapWindow = nullptr;

static void SDLCALL Hooked_SDL_GL_SwapWindow(SDL_Window* window) {
    SimpleUI::instance().on_pre_swap(window);
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

static bool SDLCALL SimpleUI_EventWatch(void* userdata, SDL_Event* event) {
    (void)userdata;
    return SimpleUI::instance().handle_event(event);
}

SimpleUI& SimpleUI::instance() {
    static SimpleUI s_inst;
    return s_inst;
}

SimpleUI::SimpleUI() = default;
SimpleUI::~SimpleUI() {
    shutdown();
}

void SimpleUI::log(int level, const char* msg) {
    if (m_host && m_host->log) {
        m_host->log(level, msg);
    }
}

bool SimpleUI::init(const CrayonHostAPI* host) {
    m_host = host;
    log(CRAYON_LOG_LEVEL_INFO, "[simple_ui] Initializing Simple UI plugin (native overlay)...");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    apply_theme();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    m_imgui_context_created = true;

    install_hooks();
    SDL_AddEventWatch(SimpleUI_EventWatch, nullptr);

    return true;
}

void SimpleUI::shutdown() {
    remove_hooks();
    SDL_RemoveEventWatch(SimpleUI_EventWatch, nullptr);

    if (m_initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        m_initialized = false;
    }
    if (m_imgui_context_created) {
        ImGui::DestroyContext();
        m_imgui_context_created = false;
        m_frame_active = false;
        log(CRAYON_LOG_LEVEL_INFO, "[simple_ui] Shutdown complete.");
    }
}

bool SimpleUI::install_hooks() {
#if defined(_WIN32)
    HMODULE mainExe = GetModuleHandleA(nullptr);
    if (HookIAT(mainExe, "SDL3.dll", "SDL_GL_SwapWindow",
                reinterpret_cast<void*>(Hooked_SDL_GL_SwapWindow),
                reinterpret_cast<void**>(&g_orig_SDL_GL_SwapWindow))) {
        m_hook_installed = true;
        log(CRAYON_LOG_LEVEL_INFO, "[simple_ui] IAT hook installed for SDL_GL_SwapWindow (post-process immune).");
        return true;
    }
    log(CRAYON_LOG_LEVEL_WARN, "[simple_ui] Warning: Could not install IAT hook for SDL_GL_SwapWindow.");
#endif
    return false;
}

void SimpleUI::remove_hooks() {
#if defined(_WIN32)
    if (m_hook_installed && g_orig_SDL_GL_SwapWindow) {
        HMODULE mainExe = GetModuleHandleA(nullptr);
        HookIAT(mainExe, "SDL3.dll", "SDL_GL_SwapWindow",
                reinterpret_cast<void*>(g_orig_SDL_GL_SwapWindow), nullptr);
        m_hook_installed = false;
    }
#endif
}

bool SimpleUI::handle_event(const SDL_Event* event) {
    if (m_initialized && event) {
        ImGui_ImplSDL3_ProcessEvent(event);
    }
    return true;
}

void SimpleUI::apply_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 5.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 5.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 4.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.94f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.53f, 0.58f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.10f, 0.12f, 0.15f, 0.95f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.14f, 0.18f, 0.70f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.12f, 0.14f, 0.17f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.24f, 0.28f, 0.34f, 0.65f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.19f, 0.24f, 0.85f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.26f, 0.32f, 0.85f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.31f, 0.38f, 0.95f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.14f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.10f, 0.12f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.09f, 0.11f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.28f, 0.33f, 0.40f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.35f, 0.42f, 0.50f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.42f, 0.50f, 0.60f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.35f, 0.68f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.30f, 0.60f, 0.95f, 0.90f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.18f, 0.38f, 0.65f, 0.85f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.24f, 0.48f, 0.80f, 0.95f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.28f, 0.55f, 0.90f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.18f, 0.36f, 0.60f, 0.75f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.24f, 0.44f, 0.72f, 0.85f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.28f, 0.50f, 0.80f, 0.95f);
    colors[ImGuiCol_Separator]             = ImVec4(0.24f, 0.28f, 0.34f, 0.65f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.35f, 0.65f, 0.95f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.40f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.20f, 0.42f, 0.70f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.30f, 0.58f, 0.90f, 0.80f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.40f, 0.70f, 1.00f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.15f, 0.20f, 0.26f, 0.86f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.24f, 0.44f, 0.72f, 0.85f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.38f, 0.62f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.12f, 0.15f, 0.20f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.16f, 0.25f, 0.36f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.19f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.31f, 0.35f, 0.42f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.23f, 0.26f, 0.32f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.10f, 0.10f, 0.12f, 0.65f);
}

void SimpleUI::ensure_frame_started() {
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

void SimpleUI::on_update(float dt) {
    (void)dt;
    ensure_frame_started();
}

void SimpleUI::on_pre_swap(SDL_Window* window) {
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
        ImGui::Render();
        m_frame_active = false;

        // Save current GL state
        GLint last_fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_fbo);
        GLint last_viewport[4];
        glGetIntegerv(GL_VIEWPORT, last_viewport);

        if (!g_glBindFramebuffer) {
            g_glBindFramebuffer = reinterpret_cast<PFN_glBindFramebuffer>(SDL_GL_GetProcAddress("glBindFramebuffer"));
        }

        // Render directly to default backbuffer (framebuffer 0)
        // This is executed AFTER crayon's post-processing/retro filters have already finished!
        if (g_glBindFramebuffer) {
            g_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        glViewport(0, 0, w, h);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Restore engine state
        if (g_glBindFramebuffer) {
            g_glBindFramebuffer(GL_FRAMEBUFFER, last_fbo);
        }
        glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    }
}

bool SimpleUI::want_capture_mouse() const {
    if (!m_imgui_context_created) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool SimpleUI::want_capture_keyboard() const {
    if (!m_imgui_context_created) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}
