// Verifies auto culling on the software backend:
//  1. Backface culling of closed primitives must not change a single pixel (winding is correct)
//     while rasterizing far fewer triangles.
//  2. Two-sided geometry (plane) is not backface-culled.
//  3. Frustum culling skips off-screen objects and counts them.
//  4. Max draw distance culls far objects.
#include "graphics/backend.hpp"
#include "graphics/batch2d.hpp"
#include "graphics/camera.hpp"
#include "graphics/mesh3d.hpp"
#include "graphics/soft/soft_raster.hpp"
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

using namespace crayon;

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); ++g_fail; } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main() {
    set_active_backend(RenderBackend::Software);
    soft::Rasterizer& fb = soft::framebuffer();
    fb.resize(160, 120);
    MeshRenderer3D r;
    r.init();
    r.set_shading_mode(ShadingMode::Gouraud);
    RetroEffects re = r.get_retro_effects();
    re.jitter_enabled = false; re.affine_blend = 0.0f; re.fog_enabled = false;
    r.set_retro_effects(re);

    struct Prim { const char* name; std::function<void()> draw; };
    std::vector<Prim> prims = {
        {"cube",     [&] { r.draw_cube({0, 0, 0}, {1.5f, 1.0f, 1.2f}, 0, {0.3f, 0.5f, 0.1f}); }},
        {"sphere",   [&] { r.draw_sphere({0, 0, 0}, 0.9f, 0, {0, 0, 0}); }},
        {"cylinder", [&] { r.draw_cylinder({0, 0, 0}, 0.6f, 1.6f, 0, {0.5f, 0, 0.3f}); }},
        {"cone",     [&] { r.draw_cone({0, 0, 0}, 0.7f, 1.6f, 0, {0.4f, 0.2f, 0}); }},
        {"pyramid",  [&] { r.draw_pyramid({0, 0, 0}, 1.4f, 1.4f, 0, {0.2f, 0.7f, 0}); }},
        {"torus",    [&] { r.draw_torus({0, 0, 0}, 0.8f, 0.3f, 0, {0.8f, 0.2f, 0}); }},
        {"capsule",  [&] { r.draw_capsule({0, 0, 0}, 0.4f, 1.0f, 0, {0.3f, 0, 0.6f}); }},
    };

    const glm::vec3 cams[] = {{0, 0, 5}, {4, 3, 4}, {-5, -2, 3}, {0, 6, 0.5f}, {0.2f, -6, 0.2f}, {3, 0.2f, -5}};

    printf("== backface culling must not change the image of closed primitives\n");
    for (auto& p : prims) {
        long long diff_total = 0, tris_on = 0, tris_off = 0;
        for (const glm::vec3& c : cams) {
            Camera cam; cam.set_position(c); cam.set_target({0, 0, 0}); cam.set_fov(55.0f);
            auto render = [&](bool auto_cull, std::vector<uint32_t>& out, long long& tris) {
                r.set_auto_cull(auto_cull);
                fb.clear({0.1f, 0.1f, 0.2f, 1}); fb.stats.reset();
                r.begin(cam, 160.0f / 120.0f);
                r.set_directional_light(glm::vec3(-0.4f, -1.0f, -0.6f), glm::vec3(1.0f), glm::vec3(0.3f));
                r.set_model_color({0.9f, 0.7f, 0.5f, 1.0f});
                p.draw();
                r.end();
                out.assign(fb.pixels(), fb.pixels() + 160 * 120);
                tris += (long long)fb.stats.triangles_rasterized;
            };
            std::vector<uint32_t> a, b;
            render(true, a, tris_on);
            render(false, b, tris_off);
            for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) ++diff_total;
        }
        CHECK(diff_total == 0, "%-9s identical image (diff px = %lld) | triangles %lld -> %lld (%.0f%% rasterized)",
              p.name, diff_total, tris_off, tris_on, tris_off ? 100.0 * tris_on / tris_off : 0.0);
    }

    printf("== two-sided plane is never backface-culled\n");
    {
        Camera cam; cam.set_position({0, -3, 0.1f}); cam.set_target({0, 0, 0}); // looking at the plane from below
        r.set_auto_cull(true);
        fb.clear({0, 0, 0, 1}); r.begin(cam, 160.0f / 120.0f);
        r.set_model_color({1, 1, 1, 1});
        r.draw_plane({0, 0, 0}, 4, 4, 0, {0, 0, 0});
        r.end();
        long long lit = 0;
        for (int i = 0; i < 160 * 120; ++i) if (fb.pixels()[i] != fb.pixels()[0]) ++lit;
        CHECK(lit > 500, "plane visible from below (%lld pixels differ from background)", lit);
    }

    printf("== frustum + distance culling\n");
    {
        Camera cam; cam.set_position({0, 0, 5}); cam.set_target({0, 0, 0}); cam.set_fov(60.0f);
        r.set_auto_cull(true);
        r.begin(cam, 160.0f / 120.0f);
        r.draw_cube({0, 0, 0}, {1, 1, 1}, 0, {0, 0, 0});            // visible
        uint32_t c0 = r.get_culled_draw_count();
        r.draw_cube({0, 0, 20}, {1, 1, 1}, 0, {0, 0, 0});           // behind camera
        r.draw_cube({60, 0, 0}, {1, 1, 1}, 0, {0, 0, 0});           // far to the side
        r.draw_sphere({0, 80, 0}, 1.0f, 0, {0, 0, 0});              // way above
        uint32_t c1 = r.get_culled_draw_count();
        CHECK(c0 == 0, "visible cube not culled");
        CHECK(c1 - c0 == 3, "3 off-screen objects culled (got %u)", c1 - c0);
        r.set_max_draw_distance(30.0f);
        r.draw_cube({0, 0, -40}, {1, 1, 1}, 0, {0, 0, 0});          // in frustum but 45 away
        CHECK(r.get_culled_draw_count() - c1 == 1, "distance-culled object beyond 30 units");
        r.set_max_draw_distance(0.0f);
        r.set_auto_cull(false);
        uint32_t c2 = r.get_culled_draw_count();
        r.draw_cube({0, 0, 20}, {1, 1, 1}, 0, {0, 0, 0});
        CHECK(r.get_culled_draw_count() == c2, "auto cull disabled -> nothing culled");
        r.end();
    }
    printf("== 2D offscreen culling\n");
    {
        Batch2D b;
        b.init();
        auto frame = [&](bool cull, std::vector<uint32_t>& out, uint32_t& culled) {
            b.set_offscreen_culling(cull);
            fb.clear({0.1f, 0.1f, 0.2f, 1});
            b.begin(160, 120);
            for (int i = 0; i < 400; ++i) {                      // grid of rects, ~90% outside the 160x120 view
                float x = (i % 20) * 30.0f - 200.0f, y = (i / 20) * 25.0f - 150.0f;
                b.draw_rect(x, y, 22, 18, {0.2f + 0.04f * (i % 20), 0.6f, 0.9f - 0.02f * (i / 20), 1.0f}, true, 1.0f);
            }
            b.draw_text("culling", 4, 4, 1.0f, {1, 1, 1, 1});
            b.end();
            culled = b.get_culled_quad_count();
            out.assign(fb.pixels(), fb.pixels() + 160 * 120);
        };
        std::vector<uint32_t> a, c; uint32_t culled_on = 0, culled_off = 0;
        frame(true, a, culled_on);
        frame(false, c, culled_off);
        long long diff = 0; for (size_t i = 0; i < a.size(); ++i) if (a[i] != c[i]) ++diff;
        CHECK(diff == 0, "image identical with 2D culling on/off (diff px = %lld)", diff);
        CHECK(culled_on > 250, "offscreen quads culled: %u of ~400", culled_on);
        CHECK(culled_off == 0, "nothing culled when disabled");
    }
    printf("\n%s (%d failures)\n", g_fail ? "CULLING TEST FAILED" : "CULLING TEST PASSED", g_fail);
    return g_fail ? 1 : 0;
}
