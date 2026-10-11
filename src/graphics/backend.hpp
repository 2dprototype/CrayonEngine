#pragma once
// Which renderer is driving the engine. Chosen once at startup (before the Window is
// created) from --renderer=, the CRAYON_RENDERER env var or crayon.config, and fixed
// afterwards. The Lua API is identical for both backends.
//
//   OpenGL   - GPU path (GL 3.3 core / GLES 3.0): shaders, FBOs, post-process chain.
//   Software - CPU path: crayon::soft::Rasterizer + a CPU 2D batcher, presented through an
//              SDL window surface. Needs no GPU and no OpenGL driver.
#include <algorithm>
#include <cctype>
#include <string>

namespace crayon {

enum class RenderBackend { OpenGL, Software };
enum class RendererRequest { Auto, OpenGL, Software };

namespace detail {
inline RenderBackend& backend_ref() { static RenderBackend b = RenderBackend::OpenGL; return b; }
inline RendererRequest& request_ref() { static RendererRequest r = RendererRequest::Auto; return r; }
inline bool& locked_ref() { static bool l = false; return l; }
} // namespace detail

inline RenderBackend active_backend() { return detail::backend_ref(); }
inline bool soft_backend() { return detail::backend_ref() == RenderBackend::Software; }
inline void set_active_backend(RenderBackend b) { detail::backend_ref() = b; }
inline RendererRequest requested_renderer() { return detail::request_ref(); }
inline void set_requested_renderer(RendererRequest r) { detail::request_ref() = r; }

// True when --renderer or CRAYON_RENDERER chose the backend (crayon.config must not override it).
inline bool renderer_locked() { return detail::locked_ref(); }
inline void set_renderer_locked(bool v) { detail::locked_ref() = v; }

// "gl" / "opengl" / "gpu" -> OpenGL, "soft" / "software" / "cpu" -> Software, anything else -> Auto
inline RendererRequest parse_renderer_request(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (s == "gl" || s == "opengl" || s == "gpu") return RendererRequest::OpenGL;
    if (s == "soft" || s == "software" || s == "cpu") return RendererRequest::Software;
    return RendererRequest::Auto;
}

} // namespace crayon
