// Transparent 3D draws: deferred, sorted back-to-front, blended, no depth writes.
#include "graphics/backend.hpp"
#include "graphics/camera.hpp"
#include "graphics/mesh3d.hpp"
#include "graphics/soft/soft_raster.hpp"
#include "graphics/texture.hpp"
#include <cstdio>
#include <cmath>

using namespace crayon;
static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); ++g_fail; } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct RGB { int r, g, b; };
static RGB px(int x, int y) { uint32_t p = soft::framebuffer().pixels()[y * soft::framebuffer().width() + x]; return {int(p & 255), int((p >> 8) & 255), int((p >> 16) & 255)}; }
static bool near_rgb(RGB a, int r, int g, int b, int tol = 6) { return std::abs(a.r - r) <= tol && std::abs(a.g - g) <= tol && std::abs(a.b - b) <= tol; }

int main() {
    set_active_backend(RenderBackend::Software);
    soft::Rasterizer& fb = soft::framebuffer();
    fb.resize(160, 120);
    MeshRenderer3D r; r.init();
    RetroEffects re = r.get_retro_effects(); re.jitter_enabled = false; re.affine_blend = 0; re.fog_enabled = false; r.set_retro_effects(re);
    r.set_shading_mode(ShadingMode::Unlit);

    unsigned char white[4] = {255, 255, 255, 255};
    Texture wt; wt.load_from_memory(white, 1, 1, 4, true);

    Camera cam; cam.set_position({0, 6, 0.01f}); cam.set_target({0, 0, 0}); cam.set_fov(50);
    const int cx = 80, cy = 60;

    // layout (looking down): opaque red cube (top at y=0.5) -> far plane (blue, y=1.5) -> near plane (green, y=3)
    auto scene = [&](int order, bool opaque_first) {
        fb.clear({0.1f, 0.12f, 0.2f, 1}); fb.stats.reset();
        r.begin(cam, 160.0f / 120.0f);
        auto cube = [&] { r.set_model_color({1, 0, 0, 1}); r.draw_cube({0, 0, 0}, {2, 1, 2}, wt.get_id(), {0, 0, 0}); };
        auto far_plane = [&] { r.set_model_color({0, 0, 1, 0.5f}); r.draw_plane({0, 1.5f, 0}, 3, 3, wt.get_id(), {0, 0, 0}); };
        auto near_plane = [&] { r.set_model_color({0, 1, 0, 0.5f}); r.draw_plane({0, 3.0f, 0}, 3, 3, wt.get_id(), {0, 0, 0}); };
        if (opaque_first) cube();
        if (order == 0) { far_plane(); near_plane(); } else { near_plane(); far_plane(); }
        if (!opaque_first) cube();
        r.end();
    };

    printf("== sorting: result is independent of draw order\n");
    scene(0, true);  RGB a = px(cx, cy); RGB a_bg = px(20, 20);
    scene(1, true);  RGB b = px(cx, cy);
    scene(0, false); RGB c = px(cx, cy);
    scene(1, false); RGB d = px(cx, cy);
    printf("  centre pixels: far-first %d,%d,%d | near-first %d,%d,%d | cube-last %d,%d,%d / %d,%d,%d\n", a.r, a.g, a.b, b.r, b.g, b.b, c.r, c.g, c.b, d.r, d.g, d.b);
    CHECK(near_rgb(a, b.r, b.g, b.b, 1), "far-then-near equals near-then-far (sorted)");
    CHECK(near_rgb(a, c.r, c.g, c.b, 1) && near_rgb(a, d.r, d.g, d.b, 1), "opaque drawn after the transparents still shows through them");

    printf("== blending maths (cube red -> blue plane a=.5 -> green plane a=.5, back to front)\n");
    // after blue: 0.5*red + 0.5*blue = (127,0,127); after green: 0.5*that + 0.5*green = (64,128,64)
    CHECK(near_rgb(a, 64, 128, 64, 4), "centre = (64,128,64), got (%d,%d,%d)", a.r, a.g, a.b);
    // over background (planes are 3x3; look just outside the cube but inside the planes)
    scene(0, true);
    RGB e = px(cx + 28, cy);   // inside the planes, outside the 2x2 cube
    RGB bg = px(5, 5);
    printf("  background (%d,%d,%d); plane-over-background sample (%d,%d,%d)\n", bg.r, bg.g, bg.b, e.r, e.g, e.b);
    CHECK(e.g > bg.g + 40 && e.b > bg.b + 15, "planes tint the background where there is no cube");

    printf("== queue behaviour\n");
    r.begin(cam, 160.0f / 120.0f);
    r.set_model_color({0, 1, 0, 0.5f}); r.draw_plane({0, 1, 0}, 1, 1, wt.get_id(), {0, 0, 0});
    CHECK(r.get_transparent_queue_size() == 1, "alpha<1 draw is queued");
    r.set_model_color({1, 1, 1, 1}); r.draw_cube({0, 0, 0}, {1, 1, 1}, wt.get_id(), {0, 0, 0});
    CHECK(r.get_transparent_queue_size() == 1, "opaque draw is not queued");
    r.flush_transparent();
    CHECK(r.get_transparent_queue_size() == 0, "flush_transparent() empties the queue");
    r.end();

    printf("\n%s (%d failures)\n", g_fail ? "TRANSPARENCY TEST FAILED" : "TRANSPARENCY TEST PASSED", g_fail);
    return g_fail ? 1 : 0;
}
