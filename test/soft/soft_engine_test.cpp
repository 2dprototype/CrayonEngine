// Headless end-to-end test of the software backend: the engine's real Texture / Batch2D /
// Font / Mesh3D / MeshRenderer3D / Model3D classes, no window, no OpenGL.
//   argv[1] = obj path, argv[2] = glb path, argv[3] = output prefix
#include "graphics/backend.hpp"
#include "graphics/batch2d.hpp"
#include "graphics/camera.hpp"
#include "graphics/mesh3d.hpp"
#include "graphics/model3d.hpp"
#include "graphics/soft/soft_raster.hpp"

#include <chrono>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

using namespace crayon;

static void write_ppm(const char* path, const soft::Rasterizer& r) {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", r.width(), r.height());
    for (int i = 0; i < r.width() * r.height(); ++i) {
        uint32_t p = r.pixels()[i];
        unsigned char rgb[3] = {(unsigned char)(p & 255), (unsigned char)((p >> 8) & 255), (unsigned char)((p >> 16) & 255)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    set_active_backend(RenderBackend::Software);
    const char* obj = argc > 1 ? argv[1] : "examples/assets/models/monkey.obj";
    const char* glb = argc > 2 ? argv[2] : "";
    const char* out = argc > 3 ? argv[3] : "/tmp/eng";

    const int W = 320, H = 240;
    soft::Rasterizer& fb = soft::framebuffer();
    fb.resize(W, H);

    MeshRenderer3D r3;
    if (!r3.init()) { printf("MeshRenderer3D init failed\n"); return 1; }
    Batch2D b2;
    if (!b2.init()) { printf("Batch2D init failed\n"); return 1; }

    Model3D monkey;
    if (!monkey.load_from_file(obj)) { printf("obj load failed\n"); return 1; }
    printf("obj model loaded: parts=%zu\n", monkey.get_parts().size());
    Model3D car;
    bool have_car = glb[0] && car.load_from_file(glb);
    if (glb[0]) printf("glb model loaded: %s parts=%zu\n", have_car ? "yes" : "NO", car.get_parts().size());

    // Procedural texture through the real Texture class (soft registry)
    unsigned char px[16 * 16 * 4];
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
        bool c = ((x / 4) + (y / 4)) & 1;
        unsigned char* p = &px[(y * 16 + x) * 4];
        p[0] = c ? 240 : 60; p[1] = c ? 200 : 120; p[2] = c ? 60 : 220; p[3] = 255;
    }
    Texture tex; tex.load_from_memory(px, 16, 16, 4, /*nearest=*/true);
    printf("texture id=%u valid=%d\n", tex.get_id(), (int)tex.is_valid());

    // soft-particle-like sprite with alpha (radial falloff) for billboard/2D blending tests
    unsigned char spr[32 * 32 * 4];
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) {
        float dx = (x - 15.5f) / 16.0f, dy = (y - 15.5f) / 16.0f;
        float a = std::max(0.0f, 1.0f - std::sqrt(dx * dx + dy * dy));
        unsigned char* p = &spr[(y * 32 + x) * 4];
        p[0] = 255; p[1] = 220; p[2] = 80; p[3] = (unsigned char)(a * 255);
    }
    Texture sprite; sprite.load_from_memory(spr, 32, 32, 4, /*nearest=*/false);

    Camera cam;
    cam.set_position({0.0f, 2.2f, 7.0f});
    cam.set_target({0.0f, 0.8f, 0.0f});
    cam.set_fov(60.0f);

    RetroEffects re = r3.get_retro_effects();
    re.jitter_enabled = false; re.affine_blend = 0.0f; re.fog_enabled = true; re.fog_start = 8; re.fog_end = 40;
    re.fog_color = {0.10f, 0.14f, 0.30f}; re.dither_enabled = true;
    r3.set_retro_effects(re);
    r3.set_point_light(0, {2.0f, 3.0f, 2.0f}, {1.0f, 0.6f, 0.3f}, 9.0f, 1.2f);

    auto frame = [&](float t) {
        fb.clear({0.10f, 0.14f, 0.30f, 1.0f});
        r3.begin(cam, float(W) / H);
        r3.draw_plane({0, 0, 0}, 40.0f, 40.0f, tex.get_id());
        r3.draw_grid_3d(20.0f, 20, 0.01f, {0.8f, 0.8f, 0.9f, 1.0f});
        r3.set_model_color({0.9f, 0.4f, 0.4f, 1.0f});
        r3.draw_cube({-3.0f, 0.5f, 0.0f}, {1, 1, 1}, tex.get_id(), {0, t, 0});
        r3.set_model_color({0.4f, 0.9f, 0.5f, 1.0f});
        r3.draw_sphere({3.0f, 0.6f, 0.0f}, 0.6f, 0);
        r3.set_model_color({1, 1, 1, 1});
        r3.push_matrix(); r3.translate({0, 0.9f, 0}); r3.rotate(t, {0, 1, 0});
        monkey.draw(r3, r3.get_current_transform(), tex.get_id());
        r3.pop_matrix();
        if (have_car) {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, -4.0f}) * glm::rotate(glm::mat4(1.0f), -t, {0, 1, 0});
            car.draw(r3, m, 0);
        }
        r3.draw_billboard(sprite.get_id(), {0.0f, 2.6f, 1.0f}, {2.0f, 2.0f});
        r3.draw_line_3d({-4, 0.02f, -4}, {4, 0.02f, 4}, {1, 0, 0, 1});
        r3.end();

        b2.begin(W, H);
        b2.draw_rect(4, 4, 150, 22, {0.0f, 0.0f, 0.0f, 0.55f});
        b2.draw_text("SOFT RASTERIZER", 8, 10, 1.0f, {1, 1, 1, 1});
        b2.draw_gradient_rect(4, H - 30, 120, 24, {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1});
        b2.draw_circle(W - 30, 30, 18, {1.0f, 0.8f, 0.2f, 0.8f}, true, 32);
        b2.draw_rounded_rect(W - 90, H - 44, 80, 34, 8, {0.2f, 0.6f, 1.0f, 0.9f}, true, 8);
        b2.draw_sprite(sprite.get_id(), W - 140, 60, 48, 48, 0, 0, 1, 1, {1, 1, 1, 1}, t * 0.7f, 24, 24);
        b2.push_scissor(100, 100, 40, 30);
        b2.draw_rect(90, 90, 70, 60, {1, 0, 1, 1});
        b2.pop_scissor();
        b2.end();
    };

    frame(0.7f);
    soft::Rasterizer::PostOptions po; po.dither = true; po.dither_levels = 32; po.vignette = true;
    fb.post_process(po);
    char path[256]; snprintf(path, sizeof path, "%s.ppm", out); write_ppm(path, fb);

    auto t0 = std::chrono::steady_clock::now();
    const int N = 200;
    for (int i = 0; i < N; ++i) frame(i * 0.03f);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / N;
    printf("full frame (3D scene + 2D HUD) %dx%d: %.2f ms (%.0f fps) | tris in %llu rasterized %llu\n", W, H, ms, 1000.0 / ms,
           (unsigned long long)fb.stats.triangles_in, (unsigned long long)fb.stats.triangles_rasterized);
    return 0;
}
