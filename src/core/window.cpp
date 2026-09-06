#include "window.hpp"
#include "log.hpp"
#include <glad/glad.h>
#include <algorithm>

namespace crayon {

Window::Window() = default;

Window::~Window() {
    shutdown();
}

bool Window::init(const std::string& title, int window_w, int window_h, int virtual_w, int virtual_h) {
    m_window_w = window_w;
    m_window_h = window_h;
    m_virtual_w = virtual_w;
    m_virtual_h = virtual_h;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        CRAYON_LOG_ERROR("Failed to initialize SDL3: {}", SDL_GetError());
        return false;
    }

    // Configure OpenGL 3.3 Core Profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    m_window = SDL_CreateWindow(title.c_str(), m_window_w, m_window_h, flags);
    if (!m_window) {
        CRAYON_LOG_ERROR("Failed to create SDL3 window: {}", SDL_GetError());
        return false;
    }

    m_gl_context = SDL_GL_CreateContext(m_window);
    if (!m_gl_context) {
        CRAYON_LOG_ERROR("Failed to create OpenGL context: {}", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(m_window, m_gl_context);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        CRAYON_LOG_ERROR("Failed to initialize GLAD OpenGL loader");
        return false;
    }

    SDL_GL_SetSwapInterval(m_vsync ? 1 : 0);

    CRAYON_LOG_INFO("OpenGL initialized: {} (Renderer: {})",
        reinterpret_cast<const char*>(glGetString(GL_VERSION)),
        reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    update_viewport();
    return true;
}

void Window::shutdown() {
    if (m_gl_context) {
        SDL_GL_DestroyContext(m_gl_context);
        m_gl_context = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

void Window::set_resolution(int w, int h) {
    if (w <= 0 || h <= 0) return;
    m_virtual_w = w;
    m_virtual_h = h;
    update_viewport();
}

void Window::set_window_size(int w, int h) {
    if (w <= 0 || h <= 0) return;
    m_window_w = w;
    m_window_h = h;
    if (m_window) {
        SDL_SetWindowSize(m_window, w, h);
    }
    update_viewport();
}

void Window::set_window_position(int x, int y) {
    if (m_window) {
        SDL_SetWindowPosition(m_window, x, y);
    }
}

void Window::get_window_position(int& x, int& y) const {
    if (m_window) {
        SDL_GetWindowPosition(m_window, &x, &y);
    } else {
        x = 0; y = 0;
    }
}

void Window::center_window() {
    if (m_window) {
        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

void Window::set_window_min_size(int w, int h) {
    if (m_window) {
        SDL_SetWindowMinimumSize(m_window, w, h);
    }
}

void Window::set_window_max_size(int w, int h) {
    if (m_window) {
        SDL_SetWindowMaximumSize(m_window, w, h);
    }
}

void Window::set_fullscreen(bool enabled) {
    m_fullscreen = enabled;
    if (m_window) {
        SDL_SetWindowFullscreen(m_window, enabled);
    }
}

void Window::set_vsync(bool enabled) {
    m_vsync = enabled;
    SDL_GL_SetSwapInterval(m_vsync ? 1 : 0);
}

void Window::set_title(const std::string& title) {
    m_title = title;
    if (m_window) {
        SDL_SetWindowTitle(m_window, title.c_str());
    }
}

void Window::set_resizable(bool resizable) {
    if (m_window) {
        SDL_SetWindowResizable(m_window, resizable);
    }
}

bool Window::is_resizable() const {
    if (m_window) {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_RESIZABLE) != 0;
    }
    return false;
}

void Window::set_bordered(bool bordered) {
    if (m_window) {
        SDL_SetWindowBordered(m_window, bordered);
    }
}

bool Window::is_bordered() const {
    if (m_window) {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_BORDERLESS) == 0;
    }
    return true;
}

void Window::maximize() {
    if (m_window) {
        SDL_MaximizeWindow(m_window);
    }
}

void Window::minimize() {
    if (m_window) {
        SDL_MinimizeWindow(m_window);
    }
}

void Window::restore() {
    if (m_window) {
        SDL_RestoreWindow(m_window);
    }
}

bool Window::is_maximized() const {
    if (m_window) {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0;
    }
    return false;
}

bool Window::is_minimized() const {
    if (m_window) {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0;
    }
    return false;
}

bool Window::is_focused() const {
    if (m_window) {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_INPUT_FOCUS) != 0;
    }
    return false;
}

void Window::set_scaling_mode(ScalingMode mode) {
    m_scaling_mode = mode;
    update_viewport();
}

void Window::set_scaling_mode_string(const std::string& mode) {
    if (mode == "integer") {
        set_scaling_mode(ScalingMode::Integer);
    } else if (mode == "aspect") {
        set_scaling_mode(ScalingMode::Aspect);
    } else if (mode == "stretch") {
        set_scaling_mode(ScalingMode::Stretch);
    } else if (mode == "center") {
        set_scaling_mode(ScalingMode::Center);
    }
}

std::string Window::get_scaling_mode_string() const {
    switch (m_scaling_mode) {
        case ScalingMode::Integer: return "integer";
        case ScalingMode::Aspect:  return "aspect";
        case ScalingMode::Stretch: return "stretch";
        case ScalingMode::Center:  return "center";
        default: return "integer";
    }
}

void Window::set_mouse_relative(bool relative) {
    if (m_window) {
        SDL_SetWindowRelativeMouseMode(m_window, relative);
    }
}

bool Window::is_mouse_relative() const {
    if (m_window) {
        return SDL_GetWindowRelativeMouseMode(m_window);
    }
    return false;
}

void Window::show_cursor(bool show) {
    if (show) {
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}

bool Window::is_cursor_visible() const {
    return SDL_CursorVisible();
}

void Window::get_display_size(int& w, int& h) const {
    SDL_DisplayID disp = SDL_GetPrimaryDisplay();
    SDL_Rect rect{0, 0, 1920, 1080};
    if (disp != 0 && SDL_GetDisplayBounds(disp, &rect)) {
        w = rect.w;
        h = rect.h;
    } else {
        w = 1920;
        h = 1080;
    }
}

void Window::swap_buffers() {
    if (m_window) {
        SDL_GL_SwapWindow(m_window);
    }
}

void Window::handle_event(const SDL_Event& event) {
    if (event.type == SDL_EVENT_QUIT) {
        m_should_close = true;
    } else if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        SDL_GetWindowSize(m_window, &m_window_w, &m_window_h);
        update_viewport();
    }
}

ViewportInfo Window::get_viewport_info() const {
    return m_viewport;
}

void Window::window_to_virtual(float win_x, float win_y, float& virt_x, float& virt_y) const {
    if (m_viewport.width <= 0 || m_viewport.height <= 0) {
        virt_x = 0;
        virt_y = 0;
        return;
    }

    float scale_x = static_cast<float>(m_viewport.width) / static_cast<float>(m_virtual_w);
    float scale_y = static_cast<float>(m_viewport.height) / static_cast<float>(m_virtual_h);

    virt_x = (win_x - static_cast<float>(m_viewport.x)) / scale_x;
    virt_y = (win_y - static_cast<float>(m_viewport.y)) / scale_y;
}

void Window::update_viewport() {
    switch (m_scaling_mode) {
        case ScalingMode::Integer: {
            int scale_x = m_window_w / m_virtual_w;
            int scale_y = m_window_h / m_virtual_h;
            int scale = std::max(1, std::min(scale_x, scale_y));

            int vp_w = m_virtual_w * scale;
            int vp_h = m_virtual_h * scale;
            int vp_x = (m_window_w - vp_w) / 2;
            int vp_y = (m_window_h - vp_h) / 2;

            m_viewport.x = vp_x;
            m_viewport.y = vp_y;
            m_viewport.width = vp_w;
            m_viewport.height = vp_h;
            m_viewport.scale = scale;
            break;
        }
        case ScalingMode::Aspect: {
            float s = std::min(static_cast<float>(m_window_w) / m_virtual_w, static_cast<float>(m_window_h) / m_virtual_h);
            if (s <= 0.0f) s = 1.0f;
            int vp_w = std::max(1, static_cast<int>(m_virtual_w * s));
            int vp_h = std::max(1, static_cast<int>(m_virtual_h * s));
            int vp_x = (m_window_w - vp_w) / 2;
            int vp_y = (m_window_h - vp_h) / 2;

            m_viewport.x = vp_x;
            m_viewport.y = vp_y;
            m_viewport.width = vp_w;
            m_viewport.height = vp_h;
            m_viewport.scale = std::max(1, static_cast<int>(s));
            break;
        }
        case ScalingMode::Stretch: {
            m_viewport.x = 0;
            m_viewport.y = 0;
            m_viewport.width = m_window_w;
            m_viewport.height = m_window_h;
            m_viewport.scale = 1;
            break;
        }
        case ScalingMode::Center: {
            m_viewport.width = m_virtual_w;
            m_viewport.height = m_virtual_h;
            m_viewport.x = (m_window_w - m_virtual_w) / 2;
            m_viewport.y = (m_window_h - m_virtual_h) / 2;
            m_viewport.scale = 1;
            break;
        }
    }
}

} // namespace crayon
