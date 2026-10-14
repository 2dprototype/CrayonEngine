#include "lightmap2d.hpp"
#include <algorithm>
#include <cmath>

namespace crayon {

void render_lightmap(ImageData& out, float ar, float ag, float ab,
                     const std::vector<Light2D>& lights, const LightMapView& v) {
    const int W = out.width(), H = out.height();
    std::vector<float> acc(size_t(W) * H * 3);
    for (size_t i = 0; i < acc.size(); i += 3) { acc[i] = ar; acc[i + 1] = ag; acc[i + 2] = ab; }

    for (const Light2D& l : lights) {
        if (l.radius <= 0.0f || l.intensity <= 0.0f) continue;
        // light centre and radius in light-map pixels
        const float cx = (l.x - v.offset_x) * v.scale_x, cy = (l.y - v.offset_y) * v.scale_y;
        const float rx = l.radius * v.scale_x, ry = l.radius * v.scale_y;
        const int x0 = std::max(0, int(std::floor(cx - rx))), x1 = std::min(W - 1, int(std::ceil(cx + rx)));
        const int y0 = std::max(0, int(std::floor(cy - ry))), y1 = std::min(H - 1, int(std::ceil(cy + ry)));
        const float cos_dir = std::cos(l.direction), sin_dir = std::sin(l.direction);
        const float soft_start = std::cos(std::min(l.cone, 3.1415f));
        const float soft_end = std::cos(std::min(l.cone * (1.0f - std::clamp(l.softness, 0.0f, 1.0f)), 3.1415f));

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const float dx = ((x + 0.5f) - cx) / rx, dy = ((y + 0.5f) - cy) / ry;
                const float d2 = dx * dx + dy * dy;
                if (d2 >= 1.0f) continue;
                const float d = std::sqrt(d2);
                float att = std::pow(1.0f - d, l.falloff) * l.intensity;
                if (l.spot && d > 1e-5f) {
                    const float c = (dx * cos_dir + dy * sin_dir) / d;   // cos(angle between pixel dir and light dir)
                    if (c <= soft_start) continue;
                    const float t = std::clamp((c - soft_start) / std::max(soft_end - soft_start, 1e-4f), 0.0f, 1.0f);
                    att *= t * t * (3.0f - 2.0f * t);
                }
                float* p = &acc[(size_t(y) * W + x) * 3];
                p[0] += l.r * att; p[1] += l.g * att; p[2] += l.b * att;
            }
        }
    }

    uint8_t* px = out.data();
    for (size_t i = 0, n = size_t(W) * H; i < n; ++i) {
        for (int c = 0; c < 3; ++c) px[i * 4 + c] = uint8_t(std::clamp(acc[i * 3 + c], 0.0f, 1.0f) * 255.0f + 0.5f);
        px[i * 4 + 3] = 255;
    }
}

} // namespace crayon
