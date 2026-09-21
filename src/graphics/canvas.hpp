#pragma once

#include <glad/glad.h>
#include <memory>
#include "texture.hpp"

namespace crayon {

class Canvas {
public:
    Canvas(int width, int height);
    ~Canvas();

    bool init();
    void destroy();

    void bind();
    void unbind();

    void clear(float r, float g, float b, float a = 1.0f);

    GLuint get_fbo() const { return m_fbo; }
    GLuint get_texture_id() const { return m_color_texture; }
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }
    bool is_valid() const { return m_fbo != 0 && m_color_texture != 0; }

    static std::shared_ptr<Canvas> create(int width, int height);

private:
    GLuint m_fbo = 0;
    GLuint m_color_texture = 0;
    GLuint m_depth_stencil_rbo = 0;
    int m_width = 0;
    int m_height = 0;
};

} // namespace crayon
