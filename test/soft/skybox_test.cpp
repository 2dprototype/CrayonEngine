// Skybox: face selection, equirect conversion, depth-aware compositing (software backend).
#include "graphics/backend.hpp"
#include "graphics/camera.hpp"
#include "graphics/mesh3d.hpp"
#include "graphics/skybox.hpp"
#include "graphics/soft/soft_raster.hpp"
#include "graphics/texture.hpp"
#include <cmath>
#include <cstdio>

using namespace crayon;
static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); ++g_fail; } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
struct RGB { int r, g, b; };
static RGB px(int x, int y) { uint32_t p = soft::framebuffer().pixels()[y * soft::framebuffer().width() + x]; return {int(p & 255), int((p >> 8) & 255), int((p >> 16) & 255)}; }
static bool near_rgb(RGB a, int r, int g, int b, int tol = 4) { return std::abs(a.r - r) <= tol && std::abs(a.g - g) <= tol && std::abs(a.b - b) <= tol; }

int main() {
    set_active_backend(RenderBackend::Software);
    soft::Rasterizer& fb = soft::framebuffer(); fb.resize(96, 72);
    MeshRenderer3D r; r.init();

    // six solid faces: +X red, -X cyan, +Y green, -Y magenta, +Z blue, -Z yellow
    const RGB cols[6] = {{255, 0, 0}, {0, 255, 255}, {0, 255, 0}, {255, 0, 255}, {0, 0, 255}, {255, 255, 0}};
    std::array<std::shared_ptr<ImageData>, 6> faces;
    for (int i = 0; i < 6; ++i) { faces[i] = std::make_shared<ImageData>(16, 16); faces[i]->fill(cols[i].r / 255.0f, cols[i].g / 255.0f, cols[i].b / 255.0f, 1); }
    Skybox sky;
    CHECK(sky.load_faces(faces) && sky.face_size() == 16, "six faces loaded");

    printf("== face selection (look along each axis; centre pixel = that face)\n");
    const char* names[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
    const glm::vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int i = 0; i < 6; ++i) {
        Camera cam; cam.set_position({5, 7, -3});   // position must not matter: the sky follows the camera
        cam.set_target(glm::vec3(5, 7, -3) + dirs[i]);
        if (i == 2 || i == 3) cam.set_up({0, 0, i == 2 ? -1.0f : 1.0f});
        cam.set_fov(60);
        fb.clear({0, 0, 0, 1});
        r.begin(cam, 96.0f / 72.0f);
        r.draw_skybox(sky);
        r.end();
        CHECK(near_rgb(px(48, 36), cols[i].r, cols[i].g, cols[i].b), "looking %s -> (%d,%d,%d)", names[i], px(48, 36).r, px(48, 36).g, px(48, 36).b);
    }

    printf("== equirect conversion (R = u, G = v of the panorama)\n");
    ImageData pano(256, 128);
    for (int y = 0; y < 128; ++y) for (int x = 0; x < 256; ++x) pano.set_pixel(x, y, (x + 0.5f) / 256.0f, (y + 0.5f) / 128.0f, 0.5f, 1);
    Skybox eq; CHECK(eq.load_equirect(pano, 64), "panorama converted to 64px faces");
    auto look = [&](glm::vec3 d, glm::vec3 up) {
        Camera cam; cam.set_position({0, 0, 0}); cam.set_target(d); cam.set_up(up); cam.set_fov(40);
        fb.clear({0, 0, 0, 1}); r.begin(cam, 96.0f / 72.0f); r.draw_skybox(eq); r.end();
        return px(48, 36);
    };
    RGB fwd = look({0, 0, -1}, {0, 1, 0});  CHECK(near_rgb(fwd, 128, 128, 128, 6), "-Z = panorama centre (u=.5,v=.5): (%d,%d,%d)", fwd.r, fwd.g, fwd.b);
    RGB px_ = look({1, 0, 0}, {0, 1, 0});   CHECK(near_rgb(px_, 191, 128, 128, 6), "+X = u .75: (%d,%d,%d)", px_.r, px_.g, px_.b);
    RGB nx = look({-1, 0, 0}, {0, 1, 0});   CHECK(near_rgb(nx, 64, 128, 128, 6), "-X = u .25: (%d,%d,%d)", nx.r, nx.g, nx.b);
    RGB up = look({0, 1, 0}, {0, 0, -1});   CHECK(up.g < 10, "up = top row of the panorama (v~0): g=%d", up.g);
    RGB dn = look({0, -1, 0}, {0, 0, 1});   CHECK(dn.g > 245, "down = bottom row (v~1): g=%d", dn.g);

    printf("== depth-aware compositing\n");
    Camera cam; cam.set_position({0, 0, 4}); cam.set_target({0, 0, 0}); cam.set_fov(60);
    unsigned char white[4] = {255, 255, 255, 255}; Texture wt; wt.load_from_memory(white, 1, 1, 4, true);
    r.set_shading_mode(ShadingMode::Unlit);
    for (int order = 0; order < 2; ++order) {
        fb.clear({0, 0, 0, 1}); r.begin(cam, 96.0f / 72.0f);
        r.set_model_color({1, 0.5f, 0, 1});
        if (order == 0) r.draw_skybox(sky);
        r.draw_cube({0, 0, 0}, {2, 2, 2}, wt.get_id(), {0, 0, 0});
        if (order == 1) r.draw_skybox(sky);
        r.end();
        RGB c = px(48, 36), s = px(4, 4);
        CHECK(near_rgb(c, 255, 128, 0, 3) && !near_rgb(s, 255, 128, 0, 30), "sky %s: cube stays in front (%d,%d,%d), sky around it (%d,%d,%d)", order ? "last" : "first", c.r, c.g, c.b, s.r, s.g, s.b);
    }
    {   // tint
        fb.clear({0, 0, 0, 1}); r.begin(cam, 96.0f / 72.0f); r.draw_skybox(sky, {0.5f, 0.5f, 0.5f}); r.end();
        RGB t = px(48, 36); CHECK(t.r + t.g + t.b > 0 && t.r <= 130 && t.g <= 130 && t.b <= 130, "tint darkens the sky (%d,%d,%d)", t.r, t.g, t.b);
    }
    printf("\n%s (%d failures)\n", g_fail ? "SKYBOX TEST FAILED" : "SKYBOX TEST PASSED", g_fail);
    return g_fail ? 1 : 0;
}
