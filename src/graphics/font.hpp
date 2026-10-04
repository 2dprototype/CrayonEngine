#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <imstb_truetype.h>

namespace crayon {

class Batch2D;

enum class TextAlign {
    Left = 0,
    Center = 1,
    Right = 2
};

class Font {
public:
    Font();
    ~Font();

    // Owns a GL texture; copying would double-delete it.
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    bool load_from_file(const std::string& filepath, float pixel_height = 16.0f, bool nearest_filter = true);
    bool load_from_memory(const unsigned char* data, size_t size, float pixel_height = 16.0f, bool nearest_filter = true);
    void destroy();

    GLuint get_texture_id() const { return m_texture_id; }
    float get_pixel_height() const { return m_pixel_height; }
    float get_ascent() const { return m_ascent; }
    float get_descent() const { return m_descent; }
    float get_line_gap() const { return m_line_gap; }
    float get_line_height() const { return m_line_height; }

    glm::vec2 measure_text(std::string_view text, float scale = 1.0f) const;

    void draw(Batch2D& batch, std::string_view text, float x, float y, float scale,
              const glm::vec4& color, float wrap_width = -1.0f, TextAlign align = TextAlign::Left) const;

private:
    void render_line(Batch2D& batch, std::string_view line, float x, float y, float scale, const glm::vec4& color) const;
    float line_width(std::string_view line, float scale) const;

    GLuint m_texture_id = 0;
    int m_atlas_w = 512;
    int m_atlas_h = 512;
    float m_pixel_height = 16.0f;

    float m_ascent = 0.0f;
    float m_descent = 0.0f;
    float m_line_gap = 0.0f;
    float m_line_height = 16.0f;

    stbtt_bakedchar m_cdata[96]{}; // ASCII 32..127
    bool m_valid = false;
};

} // namespace crayon
