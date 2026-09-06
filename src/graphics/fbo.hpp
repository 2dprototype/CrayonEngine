#pragma once

#include <glad/glad.h>
#include "../core/window.hpp"
#include "shader.hpp"

namespace crayon {

struct PostProcessOptions {
    bool dither_enabled = true;
    float dither_levels = 32.0f; // 32.0 for 15-bit, 8.0 for 8-bit
    bool crt_scanlines = false;
    float scanline_strength = 0.25f;
    bool crt_curvature = false;
    float curvature_distort = 0.05f;
    bool vignette = false;
    float vignette_strength = 0.25f;
    bool transparent = false;
};

class FBO {
public:
    FBO();
    ~FBO();

    bool init(int width, int height);
    void shutdown();
    void resize(int width, int height);

    void bind();
    void unbind();

    void blit_to_screen(const ViewportInfo& vp, int window_w, int window_h, Shader& post_shader, const PostProcessOptions& opts);

    GLuint get_color_texture() const { return m_color_texture; }
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }

private:
    void create_fullscreen_quad();

    GLuint m_fbo = 0;
    GLuint m_color_texture = 0;
    GLuint m_rbo_depth_stencil = 0;

    GLuint m_quad_vao = 0;
    GLuint m_quad_vbo = 0;

    int m_width = 320;
    int m_height = 240;
};

} // namespace crayon
