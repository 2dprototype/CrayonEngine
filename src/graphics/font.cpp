#define STB_TRUETYPE_IMPLEMENTATION
#include "font.hpp"
#include "batch2d.hpp"
#include "../core/log.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace crayon {

Font::Font() = default;

Font::~Font() {
    destroy();
}

void Font::destroy() {
    if (m_texture_id) {
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
    }
    m_valid = false;
}

bool Font::load_from_file(const std::string& filepath, float pixel_height, bool nearest_filter) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        CRAYON_LOG_WARN("Font: Failed to open file '{}'", filepath);
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        CRAYON_LOG_WARN("Font: Failed to read file data from '{}'", filepath);
        return false;
    }

    return load_from_memory(buffer.data(), buffer.size(), pixel_height, nearest_filter);
}

bool Font::load_from_memory(const unsigned char* data, size_t size, float pixel_height, bool nearest_filter) {
    destroy();

    if (!data || size == 0) return false;

    m_pixel_height = std::max(6.0f, pixel_height);

    // Pick suitable atlas dimensions
    if (m_pixel_height <= 24.0f) {
        m_atlas_w = 256;
        m_atlas_h = 256;
    } else if (m_pixel_height <= 56.0f) {
        m_atlas_w = 512;
        m_atlas_h = 512;
    } else {
        m_atlas_w = 1024;
        m_atlas_h = 1024;
    }

    std::vector<unsigned char> mono_bitmap(m_atlas_w * m_atlas_h, 0);

    int res = stbtt_BakeFontBitmap(
        data, 0,
        m_pixel_height,
        mono_bitmap.data(), m_atlas_w, m_atlas_h,
        32, 96,
        m_cdata
    );

    if (res <= 0) {
        // Did not fit, retry with 1024x1024
        m_atlas_w = 1024;
        m_atlas_h = 1024;
        mono_bitmap.assign(m_atlas_w * m_atlas_h, 0);
        res = stbtt_BakeFontBitmap(
            data, 0,
            m_pixel_height,
            mono_bitmap.data(), m_atlas_w, m_atlas_h,
            32, 96,
            m_cdata
        );
        if (res <= 0) {
            CRAYON_LOG_WARN("Font: Atlas overflow for font size {}", m_pixel_height);
            return false;
        }
    }

    // Convert 1-channel alpha into 4-channel (255, 255, 255, alpha) for standard 2D batch shader
    std::vector<unsigned char> rgba_bitmap(m_atlas_w * m_atlas_h * 4, 255);
    for (int i = 0; i < m_atlas_w * m_atlas_h; ++i) {
        rgba_bitmap[i * 4 + 0] = 255;
        rgba_bitmap[i * 4 + 1] = 255;
        rgba_bitmap[i * 4 + 2] = 255;
        rgba_bitmap[i * 4 + 3] = mono_bitmap[i];
    }

    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_atlas_w, m_atlas_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_bitmap.data());

    GLenum filter = nearest_filter ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Extract font metrics
    stbtt_fontinfo info;
    if (stbtt_InitFont(&info, data, 0)) {
        int ascent, descent, line_gap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
        float scale = stbtt_ScaleForPixelHeight(&info, m_pixel_height);
        m_ascent = ascent * scale;
        m_descent = descent * scale;
        m_line_gap = line_gap * scale;
        m_line_height = (ascent - descent + line_gap) * scale;
    } else {
        m_ascent = m_pixel_height * 0.8f;
        m_descent = -m_pixel_height * 0.2f;
        m_line_gap = m_pixel_height * 0.1f;
        m_line_height = m_pixel_height * 1.15f;
    }

    m_valid = true;
    CRAYON_LOG_INFO("Loaded TTF Font (Size: {}px, Atlas: {}x{})", m_pixel_height, m_atlas_w, m_atlas_h);
    return true;
}

glm::vec2 Font::measure_text(const std::string& text, float scale) const {
    if (!m_valid || text.empty()) return glm::vec2(0.0f);

    float max_w = 0.0f;
    float current_w = 0.0f;
    int line_count = 1;

    for (char ch : text) {
        if (ch == '\n') {
            max_w = std::max(max_w, current_w);
            current_w = 0.0f;
            line_count++;
            continue;
        }
        auto c = static_cast<unsigned char>(ch);
        if (c < 32 || c >= 128) continue;

        const auto& bc = m_cdata[c - 32];
        current_w += bc.xadvance * scale;
    }
    max_w = std::max(max_w, current_w);
    float total_h = line_count * m_line_height * scale;
    return glm::vec2(max_w, total_h);
}

void Font::render_line(Batch2D& batch, const std::string& line, float start_x, float start_y, float scale, const glm::vec4& color) const {
    float cur_x = start_x;
    float cur_y = start_y + m_ascent * scale; // align to baseline

    for (char ch : line) {
        auto c = static_cast<unsigned char>(ch);
        if (c < 32 || c >= 128) continue;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(m_cdata, m_atlas_w, m_atlas_h, c - 32, &cur_x, &cur_y, &q, 1);

        float w = (q.x1 - q.x0) * scale;
        float h = (q.y1 - q.y0) * scale;
        float x = start_x + (q.x0 - start_x) * scale;
        float y = start_y + (q.y0 - start_y) * scale;

        batch.draw_sprite(
            m_texture_id,
            x, y, w, h,
            q.s0, q.t0, q.s1, q.t1,
            color
        );
    }
}

void Font::draw(Batch2D& batch, const std::string& text, float x, float y, float scale,
                const glm::vec4& color, float wrap_width, TextAlign align) const {
    if (!m_valid || text.empty()) return;

    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string raw_line;

    while (std::getline(stream, raw_line)) {
        if (wrap_width <= 0.0f) {
            lines.push_back(raw_line);
        } else {
            // Word wrapping
            std::istringstream words(raw_line);
            std::string word;
            std::string current_line;

            while (words >> word) {
                std::string test_line = current_line.empty() ? word : current_line + " " + word;
                float test_w = measure_text(test_line, scale).x;
                if (test_w > wrap_width && !current_line.empty()) {
                    lines.push_back(current_line);
                    current_line = word;
                } else {
                    current_line = test_line;
                }
            }
            if (!current_line.empty()) {
                lines.push_back(current_line);
            }
        }
    }

    float line_h = m_line_height * scale;
    float cur_y = y;

    for (const auto& l : lines) {
        float line_w = measure_text(l, scale).x;
        float cur_x = x;
        if (align == TextAlign::Center) {
            float target_w = (wrap_width > 0.0f) ? wrap_width : line_w;
            cur_x = x + (target_w - line_w) * 0.5f;
        } else if (align == TextAlign::Right) {
            float target_w = (wrap_width > 0.0f) ? wrap_width : line_w;
            cur_x = x + (target_w - line_w);
        }

        render_line(batch, l, cur_x, cur_y, scale, color);
        cur_y += line_h;
    }
}

} // namespace crayon
