#include "image_data.hpp"
#include "png_write.hpp"
#include <stb_image.h>
#include <cmath>
#include <cstring>

namespace crayon {

static inline uint8_t to_byte(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

ImageData::ImageData(int width, int height)
    : m_w(std::max(1, width)), m_h(std::max(1, height)), m_px(size_t(m_w) * m_h * 4, 0) {}

void ImageData::get_pixel(int x, int y, float& r, float& g, float& b, float& a) const {
    if (!in_bounds(x, y)) { r = g = b = a = 0.0f; return; }
    const uint8_t* p = &m_px[(size_t(y) * m_w + x) * 4];
    r = p[0] / 255.0f; g = p[1] / 255.0f; b = p[2] / 255.0f; a = p[3] / 255.0f;
}

void ImageData::set_pixel(int x, int y, float r, float g, float b, float a) {
    if (!in_bounds(x, y)) return;
    uint8_t* p = &m_px[(size_t(y) * m_w + x) * 4];
    p[0] = to_byte(r); p[1] = to_byte(g); p[2] = to_byte(b); p[3] = to_byte(a);
}

void ImageData::fill(float r, float g, float b, float a) {
    const uint8_t c[4] = {to_byte(r), to_byte(g), to_byte(b), to_byte(a)};
    for (size_t i = 0; i < m_px.size(); i += 4) std::memcpy(&m_px[i], c, 4);
}

void ImageData::paste(const ImageData& src, int dx, int dy, int sx, int sy, int sw, int sh) {
    if (sw <= 0) sw = src.m_w - sx;
    if (sh <= 0) sh = src.m_h - sy;
    // clip source region to the source image
    if (sx < 0) { dx -= sx; sw += sx; sx = 0; }
    if (sy < 0) { dy -= sy; sh += sy; sy = 0; }
    sw = std::min(sw, src.m_w - sx);
    sh = std::min(sh, src.m_h - sy);
    // clip destination
    if (dx < 0) { sx -= dx; sw += dx; dx = 0; }
    if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
    sw = std::min(sw, m_w - dx);
    sh = std::min(sh, m_h - dy);
    if (sw <= 0 || sh <= 0) return;
    for (int y = 0; y < sh; ++y) {
        std::memmove(&m_px[(size_t(dy + y) * m_w + dx) * 4], &src.m_px[(size_t(sy + y) * src.m_w + sx) * 4], size_t(sw) * 4);
    }
}

std::shared_ptr<ImageData> ImageData::clone() const {
    auto c = std::make_shared<ImageData>(m_w, m_h);
    c->m_px = m_px;
    return c;
}

void ImageData::flip_vertical() {
    const size_t row = size_t(m_w) * 4;
    std::vector<uint8_t> tmp(row);
    for (int y = 0; y < m_h / 2; ++y) {
        uint8_t* a = &m_px[size_t(y) * row];
        uint8_t* b = &m_px[size_t(m_h - 1 - y) * row];
        std::memcpy(tmp.data(), a, row);
        std::memcpy(a, b, row);
        std::memcpy(b, tmp.data(), row);
    }
}

std::shared_ptr<ImageData> ImageData::load(const std::string& path) {
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) return nullptr;
    auto img = std::make_shared<ImageData>(w, h);
    std::memcpy(img->m_px.data(), data, size_t(w) * h * 4);
    stbi_image_free(data);
    return img;
}

bool ImageData::save_png(const std::string& path) const {
    return png::write_file(path, m_px.data(), m_w, m_h);
}

} // namespace crayon
