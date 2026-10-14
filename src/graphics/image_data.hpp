#pragma once
// CPU-side RGBA8 image. Lets scripts build, edit, load and save pixels, and turn them into
// textures (graphics.newImageData / newTexture / texture:update / screenshots).
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace crayon {

class ImageData {
public:
    ImageData(int width, int height);

    int width() const { return m_w; }
    int height() const { return m_h; }
    bool in_bounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_w && y < m_h; }
    const uint8_t* data() const { return m_px.data(); }
    uint8_t* data() { return m_px.data(); }

    void get_pixel(int x, int y, float& r, float& g, float& b, float& a) const;
    void set_pixel(int x, int y, float r, float g, float b, float a);
    void fill(float r, float g, float b, float a);
    // Copies a (sx,sy,sw,sh) region of `src` to (dx,dy); clipped to both images, no blending.
    void paste(const ImageData& src, int dx, int dy, int sx, int sy, int sw, int sh);
    std::shared_ptr<ImageData> clone() const;

    // Takes a bottom-up GL readback (row 0 = bottom) and flips it to top-down.
    void flip_vertical();

    static std::shared_ptr<ImageData> load(const std::string& path);
    bool save_png(const std::string& path) const;

private:
    int m_w, m_h;
    std::vector<uint8_t> m_px;
};

} // namespace crayon
