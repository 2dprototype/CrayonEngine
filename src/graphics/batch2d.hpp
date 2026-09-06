#pragma once

#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "shader.hpp"
#include "texture.hpp"

namespace crayon {

struct Vertex2D {
    glm::vec2 position;
    glm::vec2 uv;
    glm::vec4 color;
};

enum class BlendMode {
    Alpha,
    Additive,
    Multiply,
    None
};

class Batch2D {
public:
    static constexpr size_t MAX_QUADS = 16384;
    static constexpr size_t MAX_VERTICES = MAX_QUADS * 4;
    static constexpr size_t MAX_INDICES = MAX_QUADS * 6;

    Batch2D();
    ~Batch2D();

    bool init();
    void shutdown();

    void begin(int virtual_w, int virtual_h);
    void end();
    void flush();

    // Blend Mode & Scissor
    void set_blend_mode(BlendMode mode);
    BlendMode get_blend_mode() const { return m_blend_mode; }
    void set_scissor(int x, int y, int w, int h);
    void reset_scissor();

    // 2D Camera
    void set_camera2d(float x, float y, float zoom = 1.0f, float angle_rad = 0.0f, float ox = 0.0f, float oy = 0.0f);
    void reset_camera2d();

    // Sprites & Textures
    void draw_sprite(GLuint texture_id, float x, float y, float w, float h,
                    float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f,
                    const glm::vec4& color = glm::vec4(1.0f),
                    float angle_rad = 0.0f, float origin_x = 0.0f, float origin_y = 0.0f);

    void draw_sprite_tiled(GLuint texture_id, float x, float y, float w, float h,
                          float tile_w, float tile_h, const glm::vec4& color = glm::vec4(1.0f),
                          float ox = 0.0f, float oy = 0.0f);

    void draw_sprite_9slice(GLuint texture_id, float x, float y, float w, float h,
                           float left, float top, float right, float bottom,
                           int tex_w, int tex_h, const glm::vec4& color = glm::vec4(1.0f));

    // Basic 2D Primitives
    void draw_point(float x, float y, float size, const glm::vec4& color);
    void draw_line(float x1, float y1, float x2, float y2, const glm::vec4& color, float thickness = 1.0f);
    void draw_rect(float x, float y, float w, float h, const glm::vec4& color, bool filled = true, float thickness = 1.0f);
    void draw_rounded_rect(float x, float y, float w, float h, float radius, const glm::vec4& color, bool filled = true, int segments = 8);
    void draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, const glm::vec4& color, bool filled = true);
    void draw_quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const glm::vec4& color, bool filled = true);
    void draw_polygon(const std::vector<glm::vec2>& points, const glm::vec4& color, bool filled = true);
    void draw_circle(float cx, float cy, float radius, const glm::vec4& color, bool filled = true, int segments = 24);
    void draw_ellipse(float cx, float cy, float rx, float ry, const glm::vec4& color, bool filled = true, int segments = 24);
    void draw_arc(float cx, float cy, float radius, float start_angle, float end_angle, const glm::vec4& color, bool filled = true, int segments = 16);
    void draw_ring(float cx, float cy, float inner_radius, float outer_radius, const glm::vec4& color, bool filled = true, int segments = 24);

    // Text & Font
    void draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color);
    float get_text_width(const std::string& text, float scale = 1.0f) const;
    float get_text_height(const std::string& text, float scale = 1.0f) const;

    GLuint get_white_texture_id() const;
    GLuint get_font_texture_id() const;

private:
    void init_font_texture();

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    std::unique_ptr<Shader> m_shader;
    std::shared_ptr<Texture> m_white_texture;
    std::shared_ptr<Texture> m_font_texture;

    std::vector<Vertex2D> m_vertices;
    GLuint m_current_texture = 0;
    glm::mat4 m_proj_matrix{1.0f};
    glm::mat4 m_current_proj_view{1.0f};

    BlendMode m_blend_mode = BlendMode::Alpha;
    bool m_scissor_active = false;

    int m_virtual_w = 320;
    int m_virtual_h = 240;
};

} // namespace crayon
