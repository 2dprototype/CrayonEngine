#include "canvas.hpp"
#include "../core/log.hpp"

namespace crayon {

Canvas::Canvas(int width, int height)
    : m_width(width), m_height(height) {}

Canvas::~Canvas() {
    destroy();
}

bool Canvas::init() {
    destroy();

    if (m_width <= 0 || m_height <= 0) {
        CRAYON_LOG_ERROR("Canvas: Invalid dimensions {}x{}", m_width, m_height);
        return false;
    }

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Color texture attachment
    glGenTextures(1, &m_color_texture);
    glBindTexture(GL_TEXTURE_2D, m_color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color_texture, 0);

    // Depth & stencil renderbuffer attachment for 3D drawing or depth buffering inside canvas
    glGenRenderbuffers(1, &m_depth_stencil_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depth_stencil_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depth_stencil_rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        CRAYON_LOG_ERROR("Canvas: Framebuffer is not complete for {}x{}!", m_width, m_height);
        destroy();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void Canvas::destroy() {
    if (m_depth_stencil_rbo) {
        glDeleteRenderbuffers(1, &m_depth_stencil_rbo);
        m_depth_stencil_rbo = 0;
    }
    if (m_color_texture) {
        glDeleteTextures(1, &m_color_texture);
        m_color_texture = 0;
    }
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
}

void Canvas::bind() {
    if (!m_fbo) return;

    GLint current = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current);

    // Remember what was bound before us (only the first time, so a nested
    // bind() on the same canvas can't overwrite the saved target with ourselves).
    if (static_cast<GLuint>(current) != m_fbo) {
        m_prev_fbo = current;
        glGetIntegerv(GL_VIEWPORT, m_prev_viewport);
        m_has_prev = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
}

void Canvas::unbind() {
    if (m_has_prev) {
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_prev_fbo));
        glViewport(m_prev_viewport[0], m_prev_viewport[1], m_prev_viewport[2], m_prev_viewport[3]);
        m_has_prev = false;
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void Canvas::clear(float r, float g, float b, float a) {
    if (!m_fbo) return;

    // If we're already the active target (inside renderTo), just clear.
    GLint current = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current);
    const bool was_bound = (static_cast<GLuint>(current) == m_fbo);

    if (!was_bound) bind();

    // Scissor / color mask must not limit the clear.
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    if (scissor) glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if (scissor) glEnable(GL_SCISSOR_TEST);

    // Previously clear() left the canvas bound forever when called outside
    // renderTo(); put the old target back.
    if (!was_bound) unbind();
}

std::shared_ptr<Canvas> Canvas::create(int width, int height) {
    auto canvas = std::make_shared<Canvas>(width, height);
    if (!canvas->init()) {
        return nullptr;
    }
    return canvas;
}

} // namespace crayon
