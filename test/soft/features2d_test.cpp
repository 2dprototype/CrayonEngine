// Headless test: 2D masking and the 2D light map (multiply-blended over a scene).
#include "graphics/backend.hpp"
#include "graphics/batch2d.hpp"
#include "graphics/image_data.hpp"
#include "graphics/lightmap2d.hpp"
#include "graphics/soft/soft_raster.hpp"
#include "graphics/texture.hpp"
#include <cstdio>

using namespace crayon;
static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); ++g_fail; } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static bool near(float a, float b) { return a > b - 0.04f && a < b + 0.04f; }
static uint32_t px(int x, int y) { return soft::framebuffer().pixels()[y * soft::framebuffer().width() + x]; }
static uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) { return r | (g << 8) | (b << 16) | (255u << 24); }

static void write_ppm(const char* path) {
    const soft::Rasterizer& r = soft::framebuffer();
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", r.width(), r.height());
    for (int i = 0; i < r.width() * r.height(); ++i) { uint32_t p = r.pixels()[i]; fputc(p & 255, f); fputc((p >> 8) & 255, f); fputc((p >> 16) & 255, f); }
    fclose(f);
}

int main() {
    set_active_backend(RenderBackend::Software);
    soft::Rasterizer& fb = soft::framebuffer();
    fb.resize(160, 120);
    Batch2D b2; b2.init();

    printf("== 2D mask\n");
    auto frame = [&](int mode) {   // 0 = normal, 1 = mask, 2 = inverted mask
        fb.clear({0, 0, 0, 1});
        b2.begin(160, 120);
        if (mode) b2.begin_mask();
        if (mode) b2.draw_circle(80, 60, 30, {1, 1, 1, 1}, true, 48);
        if (mode) b2.use_mask(mode == 2);
        b2.draw_rect(0, 0, 160, 120, {1, 0, 0, 1}, true, 1.0f);
        if (mode) b2.end_mask();
        b2.draw_rect(0, 0, 10, 10, {0, 1, 0, 1}, true, 1.0f);   // after endMask: unclipped again
        b2.end();
    };
    frame(1);
    CHECK(px(80, 60) == rgb(255, 0, 0) && px(100, 60) == rgb(255, 0, 0), "mask: red inside the circle");
    CHECK(px(20, 60) == rgb(0, 0, 0) && px(140, 100) == rgb(0, 0, 0) && px(80, 20) == rgb(0, 0, 0), "mask: outside stays background");
    CHECK(px(5, 5) == rgb(0, 255, 0), "after endMask drawing is unclipped (green corner)");
    frame(2);
    CHECK(px(80, 60) == rgb(0, 0, 0) && px(100, 60) == rgb(0, 0, 0), "inverted mask: inside the circle untouched");
    CHECK(px(20, 60) == rgb(255, 0, 0) && px(140, 100) == rgb(255, 0, 0), "inverted mask: red everywhere outside");
    frame(0);
    CHECK(px(80, 60) == rgb(255, 0, 0) && px(20, 60) == rgb(255, 0, 0), "no mask: full red (mask state fully reset)");
    {   // endMask() omitted: Batch2D::end() must still reset state
        fb.clear({0, 0, 0, 1}); b2.begin(160, 120);
        b2.begin_mask(); b2.draw_circle(80, 60, 20, {1, 1, 1, 1}, true, 32); b2.use_mask(false); b2.end();
        fb.clear({0, 0, 0, 1}); b2.begin(160, 120);
        b2.draw_rect(0, 0, 160, 120, {0, 0, 1, 1}, true, 1.0f); b2.end();
        CHECK(px(5, 5) == rgb(0, 0, 255), "mask auto-closed at end of frame (next frame draws normally)");
    }

    printf("== 2D light map\n");
    {
        ImageData lm(64, 64);
        std::vector<Light2D> ls;
        Light2D p; p.x = 20; p.y = 32; p.radius = 16; p.r = 1; p.g = 0.9f; p.b = 0.6f; ls.push_back(p);
        Light2D sp; sp.x = 44; sp.y = 32; sp.radius = 18; sp.spot = true; sp.direction = 0.0f; sp.cone = 0.5f; sp.r = 0.4f; sp.g = 0.7f; sp.b = 1.0f; ls.push_back(sp);
        render_lightmap(lm, 0.1f, 0.1f, 0.15f, ls, LightMapView{});
        float r, g, b, a;
        lm.get_pixel(20, 32, r, g, b, a);   CHECK(r > 0.85f && g > 0.8f, "point light core is bright (r=%.2f g=%.2f)", r, g);
        lm.get_pixel(20, 60, r, g, b, a);   CHECK(near(r, 0.1f) && near(b, 0.15f), "far from lights = ambient (r=%.2f b=%.2f)", r, b);
        lm.get_pixel(20, 32 + 10, r, g, b, a);
        float mid = r; lm.get_pixel(20, 32 + 14, r, g, b, a);
        CHECK(mid > r && r >= 0.1f, "intensity falls off with distance (%.2f -> %.2f)", mid, r);
        lm.get_pixel(44 + 10, 32, r, g, b, a); float ahead = b;
        lm.get_pixel(44 - 10, 32, r, g, b, a); float behind = b;
        CHECK(ahead > 0.28f && near(behind, 0.15f), "spot lights ahead (%.2f) but not behind (%.2f)", ahead, behind);
        lm.get_pixel(44 + 8, 32 + 12, r, g, b, a);
        CHECK(near(b, 0.15f), "spot: outside the cone is dark (%.2f)", b);

        // Visual: checkerboard floor lit by the light map through the real multiply blend path.
        ImageData big(160, 120);
        std::vector<Light2D> L2;
        Light2D a1; a1.x = 50; a1.y = 60; a1.radius = 55; a1.r = 1; a1.g = 0.75f; a1.b = 0.4f; a1.intensity = 1.1f; L2.push_back(a1);
        Light2D a2; a2.x = 118; a2.y = 45; a2.radius = 60; a2.spot = true; a2.direction = 2.4f; a2.cone = 0.55f; a2.r = 0.5f; a2.g = 0.8f; a2.b = 1.0f; L2.push_back(a2);
        render_lightmap(big, 0.12f, 0.12f, 0.2f, L2, LightMapView{});
        Texture lt; lt.load_from_memory(big.data(), 160, 120, 4, true);
        fb.clear({0, 0, 0, 1});
        b2.begin(160, 120);
        for (int y = 0; y < 120; y += 10) for (int x = 0; x < 160; x += 10)
            b2.draw_rect(float(x), float(y), 10, 10, ((x + y) / 10) % 2 ? glm::vec4(0.8f, 0.8f, 0.8f, 1) : glm::vec4(0.55f, 0.5f, 0.45f, 1), true, 1.0f);
        b2.set_blend_mode(BlendMode::Multiply);
        b2.draw_sprite(lt.get_id(), 0, 0, 160, 120, 0, 0, 1, 1, {1, 1, 1, 1});
        b2.set_blend_mode(BlendMode::Alpha);
        b2.end();
        uint32_t lit = px(50, 60), dark = px(150, 110);
        CHECK(((lit & 255) > (dark & 255) + 60), "multiply blend: lit floor (r=%u) much brighter than ambient floor (r=%u)", lit & 255, dark & 255);
        write_ppm("/tmp/lights2d.ppm");
    }
    printf("\n%s (%d failures)\n", g_fail ? "2D FEATURES TEST FAILED" : "2D FEATURES TEST PASSED", g_fail);
    return g_fail ? 1 : 0;
}
