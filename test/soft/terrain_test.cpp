#include "graphics/backend.hpp"
#include "graphics/camera.hpp"
#include "graphics/mesh3d.hpp"
#include "graphics/soft/soft_raster.hpp"
#include "graphics/terrain.hpp"
#include "graphics/texture.hpp"
#include <cmath>
#include <cstdio>
using namespace crayon;
static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); ++g_fail; } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main() {
    set_active_backend(RenderBackend::Software);
    auto& fb = soft::framebuffer(); fb.resize(320, 240);
    MeshRenderer3D r; r.init();

    // heightmap: h = 0.5 + 0.35*sin(x)cos(z) rolling hills + a plane section
    const int N = 129;
    std::vector<float> h(N * N);
    for (int z = 0; z < N; ++z) for (int x = 0; x < N; ++x)
        h[z * N + x] = 0.5f + 0.35f * std::sin(x * 0.12f) * std::cos(z * 0.09f);
    Terrain t; Terrain::Options o; o.size = {128, 20, 128}; o.origin = {-64, 0, -64}; o.chunk_cells = 32;
    o.uv_repeat_x = o.uv_repeat_z = 16; o.color_low = {0.3f, 0.5f, 0.2f, 1}; o.color_high = {0.9f, 0.9f, 0.8f, 1};
    CHECK(t.build(h, N, N, o), "terrain built from %dx%d heights", N, N);
    CHECK(t.chunk_count() == 16 && t.triangle_count() == 128 * 128 * 2, "%zu chunks, %zu triangles", t.chunk_count(), t.triangle_count());

    printf("== height / normal queries\n");
    float hv = t.height_at(-64 + 10, -64 + 20);   // exactly on grid vertex (10,20) cell units: dx = 1
    float expect = (0.5f + 0.35f * std::sin(10 * 0.12f) * std::cos(20 * 0.09f)) * 20.0f;
    CHECK(std::fabs(hv - expect) < 1e-3f, "height at a vertex = %.3f (expected %.3f)", hv, expect);
    float mid = t.height_at(-64 + 10.5f, -64 + 20.25f);
    float lo = std::min({t.height_at(-64 + 10, -64 + 20), t.height_at(-64 + 11, -64 + 20), t.height_at(-64 + 10, -64 + 21), t.height_at(-64 + 11, -64 + 21)});
    float hi = std::max({t.height_at(-64 + 10, -64 + 20), t.height_at(-64 + 11, -64 + 20), t.height_at(-64 + 10, -64 + 21), t.height_at(-64 + 11, -64 + 21)});
    CHECK(mid >= lo - 1e-4f && mid <= hi + 1e-4f, "interpolated height stays inside the cell's range (%.3f in [%.3f, %.3f])", mid, lo, hi);
    glm::vec3 n = t.normal_at(0, 0);
    CHECK(n.y > 0.5f && std::fabs(glm::length(n) - 1.0f) < 1e-4f, "normal is a unit vector pointing up (%.2f, %.2f, %.2f)", n.x, n.y, n.z);
    CHECK(!t.contains(100, 0) && t.height_at(100, 0) == 0.0f, "outside the terrain: contains=false, height=origin.y");

    printf("== raycast\n");
    glm::vec3 hit;
    bool ok = t.raycast({0, 60, 0}, {0, -1, 0}, 200, hit);
    CHECK(ok && std::fabs(hit.y - t.height_at(0, 0)) < 1e-2f, "ray straight down hits the ground at y=%.3f (height_at=%.3f)", hit.y, t.height_at(0, 0));
    CHECK(!t.raycast({0, 60, 0}, {0, 1, 0}, 200, hit), "ray pointing up misses");
    ok = t.raycast({-60, 40, -60}, {1, -0.35f, 1}, 400, hit);
    CHECK(ok && std::fabs(hit.y - t.height_at(hit.x, hit.z)) < 1e-2f, "oblique ray lands on the surface (%.1f, %.1f, %.1f)", hit.x, hit.y, hit.z);

    printf("== render + chunk culling\n");
    unsigned char px[16 * 16 * 4];
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) { bool c = ((x / 4) + (y / 4)) & 1; unsigned char* p = &px[(y * 16 + x) * 4]; p[0] = c ? 200 : 140; p[1] = c ? 190 : 150; p[2] = c ? 150 : 110; p[3] = 255; }
    Texture tex; tex.load_from_memory(px, 16, 16, 4, true);
    Camera cam; cam.set_position({-20, 22, 40}); cam.set_target({-5, 8, -10}); cam.set_fov(60);
    RetroEffects re = r.get_retro_effects(); re.jitter_enabled = false; re.affine_blend = 0; re.fog_enabled = true; re.fog_start = 40; re.fog_end = 120; re.fog_color = {0.55f, 0.7f, 0.9f}; r.set_retro_effects(re);
    fb.clear({0.55f, 0.7f, 0.9f, 1}); r.begin(cam, 320.0f / 240.0f);
    r.set_directional_light({-0.5f, -1, -0.3f}, {1, 0.95f, 0.85f}, {0.35f, 0.38f, 0.45f});
    t.draw(r, tex.get_id());
    uint32_t culled = r.get_culled_draw_count(); r.end();
    CHECK(culled > 0 && culled < t.chunk_count(), "%u of %zu chunks culled for this camera", culled, t.chunk_count());
    FILE* f = fopen("/tmp/terrain.ppm", "wb"); fprintf(f, "P6\n320 240\n255\n");
    for (int i = 0; i < 320 * 240; ++i) { uint32_t p = fb.pixels()[i]; fputc(p & 255, f); fputc((p >> 8) & 255, f); fputc((p >> 16) & 255, f); } fclose(f);
    printf("\n%s (%d failures)\n", g_fail ? "TERRAIN TEST FAILED" : "TERRAIN TEST PASSED", g_fail);
    return g_fail ? 1 : 0;
}
