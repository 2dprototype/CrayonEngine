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
    if (size <= 0) {
        // tellg() returns -1 for unreadable paths (e.g. directories); converting that
        // to size_t below would request an enormous allocation.
        CRAYON_LOG_WARN("Font: '{}' is empty or unreadable", filepath);
        return false;
    }
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

    // Upload the glyph coverage as a single-channel R8 texture (4x smaller than RGBA8)
    // and swizzle it to (1, 1, 1, coverage) so the standard 2D batch shader, which
    // computes texture(...) * color, behaves exactly as with the old RGBA atlas.
    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);

    GLint prev_align = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_align);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_atlas_w, m_atlas_h, 0, GL_RED, GL_UNSIGNED_BYTE, mono_bitmap.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_align);

    const GLint swizzle[4] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
    // Per-channel calls: GL_TEXTURE_SWIZZLE_RGBA (the vector form) does not exist in GLES 3.0.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, swizzle[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, swizzle[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, swizzle[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, swizzle[3]);

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

glm::vec2 Font::measure_text(std::string_view text, float scale) const {
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

float Font::line_width(std::string_view line, float scale) const {
    float w = 0.0f;
    for (char ch : line) {
        auto c = static_cast<unsigned char>(ch);
        if (c < 32 || c >= 128) continue;
        w += m_cdata[c - 32].xadvance * scale;
    }
    return w;
}

void Font::render_line(Batch2D& batch, std::string_view line, float start_x, float start_y, float scale, const glm::vec4& color) const {
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

namespace {
inline bool is_wrap_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}
}

void Font::draw(Batch2D& batch, std::string_view text, float x, float y, float scale,
                const glm::vec4& color, float wrap_width, TextAlign align) const {
    if (!m_valid || text.empty()) return;

    const float line_h = m_line_height * scale;
    float cur_y = y;

    // Positions and renders one finished line. `known_w` < 0 means "measure if needed".
    auto emit = [&](std::string_view line, float known_w) {
        float cur_x = x;
        if (align != TextAlign::Left) {
            float line_w = (known_w >= 0.0f) ? known_w : line_width(line, scale);
            float target_w = (wrap_width > 0.0f) ? wrap_width : line_w;
            if (align == TextAlign::Center) cur_x = x + (target_w - line_w) * 0.5f;
            else                            cur_x = x + (target_w - line_w);
        }
        render_line(batch, line, cur_x, cur_y, scale, color);
        cur_y += line_h;
    };

    // Scratch buffer reused across calls so wrapping doesn't allocate every draw.
    static thread_local std::string s_current;
    const float space_w = m_cdata[0].xadvance * scale; // ' ' is glyph 32 -> index 0

    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string_view raw = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);

        if (wrap_width <= 0.0f) {
            emit(raw, -1.0f);
        } else {
            // Word wrapping (runs of whitespace collapse to a single space)
            s_current.clear();
            float cur_w = 0.0f;
            bool emitted_any = false;

            size_t i = 0;
            while (i < raw.size()) {
                while (i < raw.size() && is_wrap_space(raw[i])) ++i;
                if (i >= raw.size()) break;
                size_t start = i;
                while (i < raw.size() && !is_wrap_space(raw[i])) ++i;
                std::string_view word = raw.substr(start, i - start);
                float ww = line_width(word, scale);

                if (s_current.empty()) {
                    s_current.assign(word);
                    cur_w = ww;
                } else {
                    float test_w = cur_w + space_w + ww;
                    if (test_w > wrap_width) {
                        emit(s_current, cur_w);
                        emitted_any = true;
                        s_current.assign(word);
                        cur_w = ww;
                    } else {
                        s_current.push_back(' ');
                        s_current.append(word);
                        cur_w = test_w;
                    }
                }
            }
            if (!s_current.empty()) {
                emit(s_current, cur_w);
            } else if (!emitted_any) {
                cur_y += line_h; // blank line: keep paragraph spacing instead of collapsing it
            }
        }

        if (nl == std::string_view::npos) break;
        pos = nl + 1;
        if (pos == text.size()) break; // trailing '\n' does not start another line
    }
}

} // namespace crayon
