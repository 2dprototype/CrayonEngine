// Headless test of texture sampling state on the software backend: wrap modes, filters,
// in-place update (texture:update) and ImageData -> Texture (graphics.newTexture).
#include "graphics/backend.hpp"
#include "graphics/batch2d.hpp"
#include "graphics/image_data.hpp"
#include "graphics/soft/soft_raster.hpp"
#include "graphics/texture.hpp"
#include <cstdio>

using namespace crayon;
static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); ++g_fail; } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t px(int x, int y) { return soft::framebuffer().pixels()[y * soft::framebuffer().width() + x]; }
static uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) { return r | (g << 8) | (b << 16) | (255u << 24); }

int main() {
    set_active_backend(RenderBackend::Software);
    soft::Rasterizer& fb = soft::framebuffer();
    fb.resize(128, 128);
    Batch2D b2; b2.init();

    // 2x2 texture: red, green / blue, white
    ImageData img(2, 2);
    img.set_pixel(0, 0, 1, 0, 0, 1); img.set_pixel(1, 0, 0, 1, 0, 1);
    img.set_pixel(0, 1, 0, 0, 1, 1); img.set_pixel(1, 1, 1, 1, 1, 1);

    Texture tex;
    CHECK(tex.load_from_memory(img.data(), 2, 2, 4, /*nearest=*/true), "newTexture-style creation from ImageData (id=%u)", tex.get_id());
    CHECK(Texture::find(tex.get_id()) == &tex, "Texture::find(id) returns the live texture");

    auto draw_uv = [&](float u1, float v1) {
        fb.clear({0, 0, 0, 1});
        b2.begin(128, 128);
        b2.draw_sprite(tex.get_id(), 0, 0, 128, 128, 0, 0, u1, v1, {1, 1, 1, 1});   // 128px for uv 0..u1
        b2.end();
    };

    printf("== wrap modes (uv 0..2 over 128px: second tile starts at x=64)\n");
    tex.set_wrap(TextureWrap::Repeat); draw_uv(2, 2);
    CHECK(px(16, 16) == rgb(255, 0, 0) && px(80, 16) == rgb(255, 0, 0), "repeat: tile 2 repeats red (px 16 and 80)");
    CHECK(px(48, 16) == rgb(0, 255, 0) && px(112, 16) == rgb(0, 255, 0), "repeat: green repeats");
    tex.set_wrap(TextureWrap::Clamp); draw_uv(2, 2);
    CHECK(px(16, 16) == rgb(255, 0, 0) && px(112, 112) == rgb(255, 255, 255), "clamp: far corner holds the edge texel (white)");
    CHECK(px(80, 16) == rgb(0, 255, 0), "clamp: past u=1 stays on the edge column (green)");
    tex.set_wrap(TextureWrap::Mirror); draw_uv(2, 2);
    CHECK(px(48, 16) == rgb(0, 255, 0) && px(80, 16) == rgb(0, 255, 0), "mirror: green mirrored around x=64");
    CHECK(px(16, 16) == rgb(255, 0, 0) && px(112, 16) == rgb(255, 0, 0), "mirror: red at both outer edges");
    tex.set_wrap(TextureWrap::Repeat);

    printf("== filter modes\n");
    tex.set_filter(true, false); draw_uv(1, 1);
    uint32_t nearest_mid = px(32, 16);
    CHECK(nearest_mid == rgb(255, 0, 0) || nearest_mid == rgb(0, 255, 0), "nearest: crisp texel colour 0x%08x", nearest_mid);
    tex.set_filter(false, false); draw_uv(1, 1);
    uint32_t lin = px(64, 16);
    uint32_t r = lin & 255, g = (lin >> 8) & 255;
    CHECK(r > 40 && r < 215 && g > 40 && g < 215, "linear: boundary blends red/green (r=%u g=%u)", r, g);
    CHECK(tex.get_wrap() == TextureWrap::Repeat && !tex.is_nearest(), "state getters reflect the changes");

    printf("== texture:update / ImageData editing\n");
    tex.set_filter(true, false);
    img.fill(0, 0, 1, 1);
    CHECK(tex.update_rgba(img.data(), 2, 2), "update_rgba accepts same-size data");
    draw_uv(1, 1);
    CHECK(px(10, 10) == rgb(0, 0, 255) && px(100, 100) == rgb(0, 0, 255), "texture now solid blue after update");
    CHECK(!tex.update_rgba(img.data(), 4, 4), "wrong-size update is rejected");

    printf("== mipmaps toggle on a larger texture\n");
    ImageData big(64, 64);
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) big.set_pixel(x, y, (x + y) % 2, (x + y) % 2, (x + y) % 2, 1);   // 1px checker
    Texture t2; t2.load_from_memory(big.data(), 64, 64, 4, true);
    const soft::Texture* st = soft::TextureRegistry::get(t2.get_id());
    CHECK(st && st->level_count() == 1, "nearest texture has no mip chain (levels=%d)", st ? st->level_count() : -1);
    t2.set_filter(false, true);
    CHECK(st && st->level_count() == 7, "mipmap filter builds 7 levels for 64x64 (levels=%d)", st ? st->level_count() : -1);
    t2.set_filter(false, false);
    CHECK(st && st->level_count() == 1, "linear (no mips) drops the chain");

    printf("\n%s (%d failures)\n", g_fail ? "TEXTURE STATE TEST FAILED" : "TEXTURE STATE TEST PASSED", g_fail);
    return g_fail ? 1 : 0;
}
