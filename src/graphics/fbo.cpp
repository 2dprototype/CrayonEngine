#include "fbo.hpp"
#include "../core/log.hpp"

namespace crayon {

FBO::FBO() = default;

FBO::~FBO() {
    shutdown();
}

bool FBO::init(int width, int height) {
    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Color texture
    glGenTextures(1, &m_color_texture);
    glBindTexture(GL_TEXTURE_2D, m_color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color_texture, 0);

    // Depth & Stencil Renderbuffer
    glGenRenderbuffers(1, &m_rbo_depth_stencil);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo_depth_stencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo_depth_stencil);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        CRAYON_LOG_ERROR("Virtual Framebuffer is not complete!");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    create_fullscreen_quad();

    CRAYON_LOG_INFO("Virtual FBO initialized at {}x{}", m_width, m_height);
    return true;
}

void FBO::shutdown() {
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_color_texture) {
        glDeleteTextures(1, &m_color_texture);
        m_color_texture = 0;
    }
    if (m_rbo_depth_stencil) {
        glDeleteRenderbuffers(1, &m_rbo_depth_stencil);
        m_rbo_depth_stencil = 0;
    }
    if (m_quad_vao) {
        glDeleteVertexArrays(1, &m_quad_vao);
        m_quad_vao = 0;
    }
    if (m_quad_vbo) {
        glDeleteBuffers(1, &m_quad_vbo);
        m_quad_vbo = 0;
    }
}

void FBO::resize(int width, int height) {
    if (width == m_width && height == m_height) return;
    shutdown();
    init(width, height);
}

void FBO::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
}

void FBO::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FBO::create_fullscreen_quad() {
    if (m_quad_vao != 0) return;

    float quad_vertices[] = {
        // Pos (x, y)  // UV (u, v)
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,

         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f
    };

    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);

    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);
}

void FBO::blit_to_screen(const ViewportInfo& vp, int window_w, int window_h, Shader& post_shader, const PostProcessOptions& opts) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Clear whole window (including pillarbox/letterbox areas)
    glViewport(0, 0, window_w, window_h);
    if (opts.transparent) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    } else {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT);

    // Render scaled virtual canvas into centered aspect-ratio viewport
    glViewport(vp.x, vp.y, vp.width, vp.height);
    glDisable(GL_DEPTH_TEST);
    if (opts.transparent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    post_shader.bind();
    post_shader.set_int("u_screen_texture", 0);
    post_shader.set_int("u_dither_enabled", opts.dither_enabled ? 1 : 0);
    post_shader.set_float("u_dither_levels", opts.dither_levels);
    post_shader.set_vec2("u_virtual_res", glm::vec2(static_cast<float>(m_width), static_cast<float>(m_height)));

    post_shader.set_int("u_crt_scanlines", opts.crt_scanlines ? 1 : 0);
    post_shader.set_float("u_scanline_strength", opts.scanline_strength);
    post_shader.set_int("u_crt_curvature", opts.crt_curvature ? 1 : 0);
    post_shader.set_float("u_curvature_distort", opts.curvature_distort);
    post_shader.set_int("u_vignette", opts.vignette ? 1 : 0);
    post_shader.set_float("u_vignette_strength", opts.vignette_strength);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_color_texture);

    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    post_shader.unbind();
}

} // namespace crayon
