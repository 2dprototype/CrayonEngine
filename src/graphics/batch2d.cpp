#include "batch2d.hpp"
#include "default_shaders.hpp"
#include "../core/log.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>

namespace crayon {

// Embedded 8x8 ASCII font for chars 32 (' ') through 126 ('~')
// 95 characters * 8 bytes = 760 bytes
static const unsigned char FONT_8X8_DATA[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 ' '
    {0x18,0x3c,0x3c,0x18,0x18,0x00,0x18,0x00}, // 33 '!'
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, // 34 '"'
    {0x6c,0x6c,0xfe,0x6c,0xfe,0x6c,0x6c,0x00}, // 35 '#'
    {0x18,0x3e,0x60,0x3c,0x06,0x7c,0x18,0x00}, // 36 '$'
    {0x00,0xc6,0xcc,0x18,0x30,0x66,0xc6,0x00}, // 37 '%'
    {0x38,0x6c,0x38,0x76,0xdc,0xcc,0x76,0x00}, // 38 '&'
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // 39 '\''
    {0x0c,0x18,0x30,0x30,0x30,0x18,0x0c,0x00}, // 40 '('
    {0x30,0x18,0x0c,0x0c,0x0c,0x18,0x30,0x00}, // 41 ')'
    {0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00}, // 42 '*'
    {0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00}, // 43 '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // 44 ','
    {0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00}, // 45 '-'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // 46 '.'
    {0x06,0x0c,0x18,0x30,0x60,0xc0,0x80,0x00}, // 47 '/'
    {0x3c,0x66,0xc3,0xc3,0xc3,0x66,0x3c,0x00}, // 48 '0'
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7e,0x00}, // 49 '1'
    {0x3c,0x66,0x06,0x0c,0x18,0x30,0x7e,0x00}, // 50 '2'
    {0x3c,0x66,0x06,0x1c,0x06,0x66,0x3c,0x00}, // 51 '3'
    {0x0c,0x1c,0x34,0x64,0xfe,0x0c,0x0c,0x00}, // 52 '4'
    {0x7e,0x60,0x7c,0x06,0x06,0x66,0x3c,0x00}, // 53 '5'
    {0x1c,0x30,0x60,0x7c,0x66,0x66,0x3c,0x00}, // 54 '6'
    {0x7e,0x06,0x0c,0x18,0x30,0x30,0x30,0x00}, // 55 '7'
    {0x3c,0x66,0x66,0x3c,0x66,0x66,0x3c,0x00}, // 56 '8'
    {0x3c,0x66,0x66,0x3e,0x06,0x0c,0x38,0x00}, // 57 '9'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // 58 ':'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // 59 ';'
    {0x0c,0x18,0x30,0x60,0x30,0x18,0x0c,0x00}, // 60 '<'
    {0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00}, // 61 '='
    {0x30,0x18,0x0c,0x06,0x0c,0x18,0x30,0x00}, // 62 '>'
    {0x3c,0x66,0x06,0x0c,0x18,0x00,0x18,0x00}, // 63 '?'
    {0x3c,0x42,0x99,0xa5,0xa5,0x99,0x42,0x3c}, // 64 '@'
    {0x18,0x3c,0x66,0x66,0x7e,0x66,0x66,0x00}, // 65 'A'
    {0x7c,0x66,0x66,0x7c,0x66,0x66,0x7c,0x00}, // 66 'B'
    {0x3c,0x66,0x60,0x60,0x60,0x66,0x3c,0x00}, // 67 'C'
    {0x78,0x6c,0x66,0x66,0x66,0x6c,0x78,0x00}, // 68 'D'
    {0x7e,0x60,0x60,0x7c,0x60,0x60,0x7e,0x00}, // 69 'E'
    {0x7e,0x60,0x60,0x7c,0x60,0x60,0x60,0x00}, // 70 'F'
    {0x3c,0x66,0x60,0x6e,0x66,0x66,0x3e,0x00}, // 71 'G'
    {0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x00}, // 72 'H'
    {0x3c,0x18,0x18,0x18,0x18,0x18,0x3c,0x00}, // 73 'I'
    {0x1e,0x0c,0x0c,0x0c,0x0c,0x6c,0x38,0x00}, // 74 'J'
    {0x66,0x6c,0x78,0x70,0x78,0x6c,0x66,0x00}, // 75 'K'
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7e,0x00}, // 76 'L'
    {0xc6,0xee,0xfe,0xd6,0xc6,0xc6,0xc6,0x00}, // 77 'M'
    {0xc6,0xe6,0xf6,0xde,0xce,0xc6,0xc6,0x00}, // 78 'N'
    {0x3c,0x66,0xc3,0xc3,0xc3,0x66,0x3c,0x00}, // 79 'O'
    {0x7c,0x66,0x66,0x7c,0x60,0x60,0x60,0x00}, // 80 'P'
    {0x3c,0x66,0xc3,0xc3,0xdb,0x6e,0x3d,0x00}, // 81 'Q'
    {0x7c,0x66,0x66,0x7c,0x78,0x6c,0x66,0x00}, // 82 'R'
    {0x3c,0x66,0x60,0x3c,0x06,0x66,0x3c,0x00}, // 83 'S'
    {0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 84 'T'
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3c,0x00}, // 85 'U'
    {0x66,0x66,0x66,0x66,0x66,0x3c,0x18,0x00}, // 86 'V'
    {0xc6,0xc6,0xc6,0xd6,0xfe,0xee,0xc6,0x00}, // 87 'W'
    {0x66,0x66,0x3c,0x18,0x3c,0x66,0x66,0x00}, // 88 'X'
    {0x66,0x66,0x66,0x3c,0x18,0x18,0x18,0x00}, // 89 'Y'
    {0x7e,0x06,0x0c,0x18,0x30,0x60,0x7e,0x00}, // 90 'Z'
    {0x3c,0x30,0x30,0x30,0x30,0x30,0x3c,0x00}, // 91 '['
    {0xc0,0x60,0x30,0x18,0x0c,0x06,0x02,0x00}, // 92 '\'
    {0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0x00}, // 93 ']'
    {0x10,0x38,0x6c,0xc6,0x00,0x00,0x00,0x00}, // 94 '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff}, // 95 '_'
    {0x30,0x18,0x0c,0x00,0x00,0x00,0x00,0x00}, // 96 '`'
    {0x00,0x00,0x3c,0x06,0x3e,0x66,0x3e,0x00}, // 97 'a'
    {0x60,0x60,0x7c,0x66,0x66,0x66,0x7c,0x00}, // 98 'b'
    {0x00,0x00,0x3c,0x66,0x60,0x66,0x3c,0x00}, // 99 'c'
    {0x06,0x06,0x3e,0x66,0x66,0x66,0x3e,0x00}, // 100 'd'
    {0x00,0x00,0x3c,0x66,0x7e,0x60,0x3c,0x00}, // 101 'e'
    {0x1c,0x30,0x78,0x30,0x30,0x30,0x30,0x00}, // 102 'f'
    {0x00,0x00,0x3e,0x66,0x66,0x3e,0x06,0x3c}, // 103 'g'
    {0x60,0x60,0x7c,0x66,0x66,0x66,0x66,0x00}, // 104 'h'
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3c,0x00}, // 105 'i'
    {0x06,0x00,0x06,0x06,0x06,0x66,0x3c,0x00}, // 106 'j'
    {0x60,0x60,0x66,0x6c,0x78,0x6c,0x66,0x00}, // 107 'k'
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3c,0x00}, // 108 'l'
    {0x00,0x00,0x66,0xff,0xdb,0xdb,0xc3,0x00}, // 109 'm'
    {0x00,0x00,0x7c,0x66,0x66,0x66,0x66,0x00}, // 110 'n'
    {0x00,0x00,0x3c,0x66,0x66,0x66,0x3c,0x00}, // 111 'o'
    {0x00,0x00,0x7c,0x66,0x66,0x7c,0x60,0x60}, // 112 'p'
    {0x00,0x00,0x3e,0x66,0x66,0x3e,0x06,0x06}, // 113 'q'
    {0x00,0x00,0x7c,0x66,0x60,0x60,0x60,0x00}, // 114 'r'
    {0x00,0x00,0x3e,0x60,0x3c,0x06,0x7c,0x00}, // 115 's'
    {0x18,0x18,0x7e,0x18,0x18,0x18,0x0e,0x00}, // 116 't'
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3e,0x00}, // 117 'u'
    {0x00,0x00,0x66,0x66,0x66,0x3c,0x18,0x00}, // 118 'v'
    {0x00,0x00,0xc3,0xdb,0xdb,0xff,0x66,0x00}, // 119 'w'
    {0x00,0x00,0x66,0x3c,0x18,0x3c,0x66,0x00}, // 120 'x'
    {0x00,0x00,0x66,0x66,0x66,0x3e,0x06,0x3c}, // 121 'y'
    {0x00,0x00,0x7e,0x0c,0x18,0x30,0x7e,0x00}, // 122 'z'
    {0x0e,0x18,0x18,0x70,0x18,0x18,0x0e,0x00}, // 123 '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // 124 '|'
    {0x70,0x18,0x18,0x0e,0x18,0x18,0x70,0x00}, // 125 '}'
    {0x76,0xdc,0x00,0x00,0x00,0x00,0x00,0x00}  // 126 '~'
};

Batch2D::Batch2D() = default;

Batch2D::~Batch2D() {
    shutdown();
}

void Batch2D::init_font_texture() {
    // Atlas layout: 16 columns x 6 rows of 8x8 characters = 128 x 48 pixels
    const int atlas_w = 128;
    const int atlas_h = 48;
    std::vector<unsigned char> pixels(atlas_w * atlas_h * 4, 0);

    for (int i = 0; i < 95; ++i) {
        int col = i % 16;
        int row = i / 16;
        int start_x = col * 8;
        int start_y = row * 8;

        for (int y = 0; y < 8; ++y) {
            unsigned char row_bits = FONT_8X8_DATA[i][y];
            for (int x = 0; x < 8; ++x) {
                bool bit = (row_bits & (1 << (7 - x))) != 0;
                int px = start_x + x;
                int py = start_y + y;
                int idx = (py * atlas_w + px) * 4;
                if (bit) {
                    pixels[idx + 0] = 255;
                    pixels[idx + 1] = 255;
                    pixels[idx + 2] = 255;
                    pixels[idx + 3] = 255;
                } else {
                    pixels[idx + 0] = 0;
                    pixels[idx + 1] = 0;
                    pixels[idx + 2] = 0;
                    pixels[idx + 3] = 0;
                }
            }
        }
    }

    m_font_texture = std::make_shared<Texture>();
    m_font_texture->load_from_memory(pixels.data(), atlas_w, atlas_h, 4, true);
}

bool Batch2D::init() {
    m_shader = std::make_unique<Shader>();
    if (!m_shader->load_from_memory(SHADER_2D_VS, SHADER_2D_FS)) {
        CRAYON_LOG_ERROR("Failed to compile 2D Batch Shader");
        return false;
    }

    m_white_texture = Texture::create_white();
    init_font_texture();

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    // Setup dynamic VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(Vertex2D), nullptr, GL_DYNAMIC_DRAW);

    // Setup static EBO
    std::vector<GLuint> indices(MAX_INDICES);
    for (size_t i = 0, offset = 0; i < MAX_INDICES; i += 6, offset += 4) {
        indices[i + 0] = static_cast<GLuint>(offset + 0);
        indices[i + 1] = static_cast<GLuint>(offset + 1);
        indices[i + 2] = static_cast<GLuint>(offset + 2);
        indices[i + 3] = static_cast<GLuint>(offset + 2);
        indices[i + 4] = static_cast<GLuint>(offset + 3);
        indices[i + 5] = static_cast<GLuint>(offset + 0);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_INDICES * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // Attributes: Pos (vec2), UV (vec2), Color (vec4)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<void*>(offsetof(Vertex2D, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<void*>(offsetof(Vertex2D, uv)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), reinterpret_cast<void*>(offsetof(Vertex2D, color)));

    glBindVertexArray(0);

    m_vertices.reserve(MAX_VERTICES);
    CRAYON_LOG_INFO("Batch2D initialized (Max quads: {})", MAX_QUADS);
    return true;
}

void Batch2D::shutdown() {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ebo) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
}

void Batch2D::begin(int virtual_w, int virtual_h) {
    m_virtual_w = virtual_w;
    m_virtual_h = virtual_h;
    m_proj_matrix = glm::ortho(0.0f, static_cast<float>(virtual_w), static_cast<float>(virtual_h), 0.0f, -1.0f, 1.0f);
    m_current_proj_view = m_proj_matrix;
    m_vertices.clear();
    m_current_texture = 0;
    m_blend_mode = BlendMode::Alpha;
    if (m_scissor_active) {
        glDisable(GL_SCISSOR_TEST);
        m_scissor_active = false;
    }
}

void Batch2D::end() {
    flush();
    if (m_scissor_active) {
        glDisable(GL_SCISSOR_TEST);
        m_scissor_active = false;
    }
}

void Batch2D::set_blend_mode(BlendMode mode) {
    if (m_blend_mode != mode) {
        flush();
        m_blend_mode = mode;
    }
}

void Batch2D::set_scissor(int x, int y, int w, int h) {
    flush();
    glEnable(GL_SCISSOR_TEST);
    int gl_y = m_virtual_h - (y + h);
    glScissor(x, gl_y, std::max(0, w), std::max(0, h));
    m_scissor_active = true;
}

void Batch2D::reset_scissor() {
    flush();
    glDisable(GL_SCISSOR_TEST);
    m_scissor_active = false;
}

void Batch2D::set_camera2d(float x, float y, float zoom, float angle_rad, float ox, float oy) {
    flush();
    glm::mat4 view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(ox, oy, 0.0f));
    if (angle_rad != 0.0f) {
        view = glm::rotate(view, angle_rad, glm::vec3(0.0f, 0.0f, 1.0f));
    }
    view = glm::scale(view, glm::vec3(zoom, zoom, 1.0f));
    view = glm::translate(view, glm::vec3(-x, -y, 0.0f));
    m_current_proj_view = m_proj_matrix * view;
}

void Batch2D::reset_camera2d() {
    flush();
    m_current_proj_view = m_proj_matrix;
}

void Batch2D::flush() {
    if (m_vertices.empty()) return;

    switch (m_blend_mode) {
        case BlendMode::Alpha:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::Additive:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
        case BlendMode::Multiply:
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            break;
        case BlendMode::None:
            glDisable(GL_BLEND);
            break;
    }
    glDisable(GL_DEPTH_TEST);

    m_shader->bind();
    m_shader->set_mat4("u_proj", m_current_proj_view);
    m_shader->set_int("u_texture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_current_texture != 0 ? m_current_texture : m_white_texture->get_id());

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertices.size() * sizeof(Vertex2D), m_vertices.data());

    size_t quad_count = m_vertices.size() / 4;
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quad_count * 6), GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
    m_shader->unbind();

    m_vertices.clear();
}

void Batch2D::draw_sprite(GLuint texture_id, float x, float y, float w, float h,
                         float u0, float v0, float u1, float v1,
                         const glm::vec4& color,
                         float angle_rad, float origin_x, float origin_y) {
    if (texture_id == 0) texture_id = m_white_texture->get_id();

    if (m_current_texture != 0 && m_current_texture != texture_id) {
        flush();
    }
    if (m_vertices.size() + 4 > MAX_VERTICES) {
        flush();
    }
    m_current_texture = texture_id;

    glm::vec2 p0(-origin_x, -origin_y);
    glm::vec2 p1(w - origin_x, -origin_y);
    glm::vec2 p2(w - origin_x, h - origin_y);
    glm::vec2 p3(-origin_x, h - origin_y);

    if (angle_rad != 0.0f) {
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);

        auto rot = [&](glm::vec2 p) {
            return glm::vec2(p.x * c - p.y * s, p.x * s + p.y * c);
        };

        p0 = rot(p0);
        p1 = rot(p1);
        p2 = rot(p2);
        p3 = rot(p3);
    }

    glm::vec2 pos(x, y);
    p0 += pos;
    p1 += pos;
    p2 += pos;
    p3 += pos;

    m_vertices.push_back({ p0, {u0, v0}, color });
    m_vertices.push_back({ p1, {u1, v0}, color });
    m_vertices.push_back({ p2, {u1, v1}, color });
    m_vertices.push_back({ p3, {u0, v1}, color });
}

void Batch2D::draw_sprite_tiled(GLuint texture_id, float x, float y, float w, float h,
                              float tile_w, float tile_h, const glm::vec4& color,
                              float ox, float oy) {
    if (tile_w <= 0.001f || tile_h <= 0.001f) return;
    float u0 = ox / tile_w;
    float v0 = oy / tile_h;
    float u1 = u0 + (w / tile_w);
    float v1 = v0 + (h / tile_h);
    draw_sprite(texture_id, x, y, w, h, u0, v0, u1, v1, color);
}

void Batch2D::draw_sprite_9slice(GLuint texture_id, float x, float y, float w, float h,
                               float left, float top, float right, float bottom,
                               int tex_w, int tex_h, const glm::vec4& color) {
    if (tex_w <= 0 || tex_h <= 0) return;

    float tw = static_cast<float>(tex_w);
    float th = static_cast<float>(tex_h);

    float u0 = 0.0f;
    float u1 = left / tw;
    float u2 = (tw - right) / tw;
    float u3 = 1.0f;

    float v0 = 0.0f;
    float v1 = top / th;
    float v2 = (th - bottom) / th;
    float v3 = 1.0f;

    float x0 = x;
    float x1 = x + left;
    float x2 = x + w - right;

    float y0 = y;
    float y1 = y + top;
    float y2 = y + h - bottom;

    // Top-Left, Top-Center, Top-Right
    draw_sprite(texture_id, x0, y0, left, top, u0, v0, u1, v1, color);
    draw_sprite(texture_id, x1, y0, x2 - x1, top, u1, v0, u2, v1, color);
    draw_sprite(texture_id, x2, y0, right, top, u2, v0, u3, v1, color);

    // Mid-Left, Mid-Center, Mid-Right
    draw_sprite(texture_id, x0, y1, left, y2 - y1, u0, v1, u1, v2, color);
    draw_sprite(texture_id, x1, y1, x2 - x1, y2 - y1, u1, v1, u2, v2, color);
    draw_sprite(texture_id, x2, y1, right, y2 - y1, u2, v1, u3, v2, color);

    // Bottom-Left, Bottom-Center, Bottom-Right
    draw_sprite(texture_id, x0, y2, left, bottom, u0, v2, u1, v3, color);
    draw_sprite(texture_id, x1, y2, x2 - x1, bottom, u1, v2, u2, v3, color);
    draw_sprite(texture_id, x2, y2, right, bottom, u2, v2, u3, v3, color);
}

void Batch2D::draw_point(float x, float y, float size, const glm::vec4& color) {
    float half = size * 0.5f;
    draw_rect(x - half, y - half, size, size, color, true);
}

void Batch2D::draw_rect(float x, float y, float w, float h, const glm::vec4& color, bool filled, float thickness) {
    if (filled) {
        draw_sprite(m_white_texture->get_id(), x, y, w, h, 0, 0, 1, 1, color);
    } else {
        draw_rect(x, y, w, thickness, color, true);
        draw_rect(x, y + h - thickness, w, thickness, color, true);
        draw_rect(x, y + thickness, thickness, h - 2 * thickness, color, true);
        draw_rect(x + w - thickness, y + thickness, thickness, h - 2 * thickness, color, true);
    }
}

void Batch2D::draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, const glm::vec4& color, bool filled) {
    if (filled) {
        if (m_current_texture != 0 && m_current_texture != m_white_texture->get_id()) flush();
        if (m_vertices.size() + 4 > MAX_VERTICES) flush();
        m_current_texture = m_white_texture->get_id();

        m_vertices.push_back({ {x1, y1}, {0, 0}, color });
        m_vertices.push_back({ {x2, y2}, {1, 0}, color });
        m_vertices.push_back({ {x3, y3}, {1, 1}, color });
        m_vertices.push_back({ {x3, y3}, {0, 1}, color });
    } else {
        draw_line(x1, y1, x2, y2, color);
        draw_line(x2, y2, x3, y3, color);
        draw_line(x3, y3, x1, y1, color);
    }
}

void Batch2D::draw_quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const glm::vec4& color, bool filled) {
    if (filled) {
        if (m_current_texture != 0 && m_current_texture != m_white_texture->get_id()) flush();
        if (m_vertices.size() + 4 > MAX_VERTICES) flush();
        m_current_texture = m_white_texture->get_id();

        m_vertices.push_back({ {x1, y1}, {0, 0}, color });
        m_vertices.push_back({ {x2, y2}, {1, 0}, color });
        m_vertices.push_back({ {x3, y3}, {1, 1}, color });
        m_vertices.push_back({ {x4, y4}, {0, 1}, color });
    } else {
        draw_line(x1, y1, x2, y2, color);
        draw_line(x2, y2, x3, y3, color);
        draw_line(x3, y3, x4, y4, color);
        draw_line(x4, y4, x1, y1, color);
    }
}

void Batch2D::draw_polygon(const std::vector<glm::vec2>& points, const glm::vec4& color, bool filled) {
    if (points.size() < 3) return;

    if (filled) {
        // Centroid fan triangulation: robust for stars, regular polygons, and convex shapes
        glm::vec2 center(0.0f);
        for (const auto& p : points) {
            center += p;
        }
        center /= static_cast<float>(points.size());

        for (size_t i = 0; i < points.size(); ++i) {
            const auto& p1 = points[i];
            const auto& p2 = points[(i + 1) % points.size()];
            draw_triangle(center.x, center.y, p1.x, p1.y, p2.x, p2.y, color, true);
        }
    } else {
        for (size_t i = 0; i < points.size(); ++i) {
            const auto& p1 = points[i];
            const auto& p2 = points[(i + 1) % points.size()];
            draw_line(p1.x, p1.y, p2.x, p2.y, color);
        }
    }
}

void Batch2D::draw_rounded_rect(float x, float y, float w, float h, float radius, const glm::vec4& color, bool filled, int segments) {
    radius = std::max(0.0f, std::min(radius, std::min(w * 0.5f, h * 0.5f)));
    if (radius <= 0.0f) {
        draw_rect(x, y, w, h, color, filled);
        return;
    }

    if (filled) {
        // Central body & horizontal cross
        draw_rect(x + radius, y, w - 2.0f * radius, h, color, true);
        draw_rect(x, y + radius, radius, h - 2.0f * radius, color, true);
        draw_rect(x + w - radius, y + radius, radius, h - 2.0f * radius, color, true);

        // Four corner arcs
        draw_arc(x + radius, y + radius, radius, 3.14159265f, 4.71238898f, color, true, segments);
        draw_arc(x + w - radius, y + radius, radius, 4.71238898f, 6.2831853f, color, true, segments);
        draw_arc(x + w - radius, y + h - radius, radius, 0.0f, 1.5707963f, color, true, segments);
        draw_arc(x + radius, y + h - radius, radius, 1.5707963f, 3.14159265f, color, true, segments);
    } else {
        draw_line(x + radius, y, x + w - radius, y, color);
        draw_line(x + radius, y + h, x + w - radius, y + h, color);
        draw_line(x, y + radius, x, y + h - radius, color);
        draw_line(x + w, y + radius, x + w, y + h - radius, color);

        draw_arc(x + radius, y + radius, radius, 3.14159265f, 4.71238898f, color, false, segments);
        draw_arc(x + w - radius, y + radius, radius, 4.71238898f, 6.2831853f, color, false, segments);
        draw_arc(x + w - radius, y + h - radius, radius, 0.0f, 1.5707963f, color, false, segments);
        draw_arc(x + radius, y + h - radius, radius, 1.5707963f, 3.14159265f, color, false, segments);
    }
}

void Batch2D::draw_line(float x1, float y1, float x2, float y2, const glm::vec4& color, float thickness) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0001f) return;

    float angle = std::atan2(dy, dx);
    draw_sprite(m_white_texture->get_id(), x1, y1, len, thickness, 0, 0, 1, 1, color, angle, 0.0f, thickness * 0.5f);
}

void Batch2D::draw_circle(float cx, float cy, float radius, const glm::vec4& color, bool filled, int segments) {
    draw_ellipse(cx, cy, radius, radius, color, filled, segments);
}

void Batch2D::draw_ellipse(float cx, float cy, float rx, float ry, const glm::vec4& color, bool filled, int segments) {
    if (segments < 3) segments = 16;
    float step = (2.0f * 3.14159265359f) / segments;

    if (filled) {
        for (int i = 0; i < segments; ++i) {
            float a0 = i * step;
            float a1 = (i + 1) * step;

            float x0 = cx + std::cos(a0) * rx;
            float y0 = cy + std::sin(a0) * ry;
            float x1 = cx + std::cos(a1) * rx;
            float y1 = cy + std::sin(a1) * ry;

            draw_triangle(cx, cy, x0, y0, x1, y1, color, true);
        }
    } else {
        for (int i = 0; i < segments; ++i) {
            float a0 = i * step;
            float a1 = (i + 1) * step;
            draw_line(cx + std::cos(a0) * rx, cy + std::sin(a0) * ry,
                      cx + std::cos(a1) * rx, cy + std::sin(a1) * ry,
                      color, 1.0f);
        }
    }
}

void Batch2D::draw_arc(float cx, float cy, float radius, float start_angle, float end_angle, const glm::vec4& color, bool filled, int segments) {
    if (segments < 2) segments = 8;
    float diff = end_angle - start_angle;
    float step = diff / segments;

    if (filled) {
        for (int i = 0; i < segments; ++i) {
            float a0 = start_angle + i * step;
            float a1 = start_angle + (i + 1) * step;
            float x0 = cx + std::cos(a0) * radius;
            float y0 = cy + std::sin(a0) * radius;
            float x1 = cx + std::cos(a1) * radius;
            float y1 = cy + std::sin(a1) * radius;
            draw_triangle(cx, cy, x0, y0, x1, y1, color, true);
        }
    } else {
        for (int i = 0; i < segments; ++i) {
            float a0 = start_angle + i * step;
            float a1 = start_angle + (i + 1) * step;
            draw_line(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius,
                      cx + std::cos(a1) * radius, cy + std::sin(a1) * radius,
                      color, 1.0f);
        }
    }
}

void Batch2D::draw_ring(float cx, float cy, float inner_radius, float outer_radius, const glm::vec4& color, bool filled, int segments) {
    if (segments < 3) segments = 16;
    float step = (2.0f * 3.14159265359f) / segments;

    if (filled) {
        for (int i = 0; i < segments; ++i) {
            float a0 = i * step;
            float a1 = (i + 1) * step;

            float cos0 = std::cos(a0), sin0 = std::sin(a0);
            float cos1 = std::cos(a1), sin1 = std::sin(a1);

            float ox0 = cx + cos0 * outer_radius;
            float oy0 = cy + sin0 * outer_radius;
            float ox1 = cx + cos1 * outer_radius;
            float oy1 = cy + sin1 * outer_radius;

            float ix0 = cx + cos0 * inner_radius;
            float iy0 = cy + sin0 * inner_radius;
            float ix1 = cx + cos1 * inner_radius;
            float iy1 = cy + sin1 * inner_radius;

            draw_quad(ix0, iy0, ox0, oy0, ox1, oy1, ix1, iy1, color, true);
        }
    } else {
        draw_circle(cx, cy, outer_radius, color, false, segments);
        draw_circle(cx, cy, inner_radius, color, false, segments);
    }
}

void Batch2D::draw_text(const std::string& text, float x, float y, float scale, const glm::vec4& color) {
    if (!m_font_texture) return;
    GLuint font_id = m_font_texture->get_id();

    float cursor_x = x;
    float cursor_y = y;
    float char_w = 8.0f * scale;
    float char_h = 8.0f * scale;

    const float atlas_w = 128.0f;
    const float atlas_h = 48.0f;

    for (char c : text) {
        if (c == '\n') {
            cursor_x = x;
            cursor_y += char_h;
            continue;
        }

        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc > 126) uc = '?';

        int idx = uc - 32;
        int col = idx % 16;
        int row = idx / 16;

        float u0 = (col * 8.0f) / atlas_w;
        float v0 = (row * 8.0f) / atlas_h;
        float u1 = ((col + 1) * 8.0f) / atlas_w;
        float v1 = ((row + 1) * 8.0f) / atlas_h;

        draw_sprite(font_id, cursor_x, cursor_y, char_w, char_h, u0, v0, u1, v1, color);
        cursor_x += char_w;
    }
}

float Batch2D::get_text_width(const std::string& text, float scale) const {
    float max_len = 0.0f;
    float cur_len = 0.0f;
    for (char c : text) {
        if (c == '\n') {
            if (cur_len > max_len) max_len = cur_len;
            cur_len = 0.0f;
        } else {
            cur_len += 1.0f;
        }
    }
    if (cur_len > max_len) max_len = cur_len;
    return max_len * 8.0f * scale;
}

float Batch2D::get_text_height(const std::string& text, float scale) const {
    if (text.empty()) return 0.0f;
    float lines = 1.0f;
    for (char c : text) {
        if (c == '\n') lines += 1.0f;
    }
    return lines * 8.0f * scale;
}

GLuint Batch2D::get_white_texture_id() const {
    return m_white_texture ? m_white_texture->get_id() : 0;
}

GLuint Batch2D::get_font_texture_id() const {
    return m_font_texture ? m_font_texture->get_id() : 0;
}

} // namespace crayon
