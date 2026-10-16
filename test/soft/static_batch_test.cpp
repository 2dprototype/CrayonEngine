// StaticBatch3D: image must match individual draws; chunk culling and draw-call reduction.
#include "graphics/backend.hpp"
#include "graphics/camera.hpp"
#include "graphics/mesh3d.hpp"
#include "graphics/model3d.hpp"
#include "graphics/soft/soft_raster.hpp"
#include "graphics/static_batch3d.hpp"
#include <chrono>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

using namespace crayon;
static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); ++g_fail; } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(int argc, char** argv) {
    set_active_backend(RenderBackend::Software);
    soft::Rasterizer& fb = soft::framebuffer();
    fb.resize(320, 240);
    MeshRenderer3D r; r.init();
    RetroEffects re = r.get_retro_effects(); re.jitter_enabled = false; re.affine_blend = 0; re.fog_enabled = false; r.set_retro_effects(re);

    Model3D monkey;
    if (!monkey.load_from_file(argc > 1 ? argv[1] : "examples/assets/models/monkey.obj")) { printf("load failed\n"); return 1; }

    std::vector<glm::mat4> xf;
    for (int z = 0; z < 20; ++z) for (int x = 0; x < 20; ++x)
        xf.push_back(glm::rotate(glm::translate(glm::mat4(1), {x * 3.0f - 30.0f, 0.0f, -z * 3.0f}), 0.3f * (x + z), {0, 1, 0}));

    StaticBatch3D batch(15.0f);
    size_t parts = 0;
    for (auto& m : xf) parts += batch.add(monkey, m);
    batch.build();
    const auto& st = batch.stats();
    printf("== build\n");
    CHECK(st.instances == 400 && parts == 400, "400 instances queued");
    CHECK(st.groups > 4 && st.groups < 40, "grouped into %zu chunk meshes (vs 400 draws)", st.groups);
    CHECK(st.triangles == 400 * monkey.get_parts()[0].cpu_indices.size() / 3, "no triangles lost: %zu", st.triangles);

    Camera cam; cam.set_position({0, 6, 12}); cam.set_target({0, 0.5f, -20}); cam.set_fov(60);
    auto frame = [&](bool use_batch, std::vector<uint32_t>& out, uint32_t& culled) {
        fb.clear({0.1f, 0.12f, 0.2f, 1});
        r.begin(cam, 320.0f / 240.0f);
        r.set_directional_light({-0.4f, -1, -0.6f}, {1, 1, 1}, {0.3f, 0.3f, 0.3f});
        if (use_batch) batch.draw(r); else for (auto& m : xf) monkey.draw(r, m, 0);
        culled = r.get_culled_draw_count();
        r.end();
        out.assign(fb.pixels(), fb.pixels() + 320 * 240);
    };
    std::vector<uint32_t> a, b; uint32_t ca = 0, cb = 0;
    frame(false, a, ca); frame(true, b, cb);
    long long diff = 0; for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) ++diff;
    printf("== image + culling\n");
    CHECK(diff * 1000 < (long long)a.size(), "batched image matches individual draws (%lld of %zu px differ)", diff, a.size());
    CHECK(cb > 0 && cb < st.groups, "batch: %u of %zu chunk meshes culled (off-screen chunks never submitted)", cb, st.groups);
    CHECK(ca > 0, "individual: %u of 400 instances culled one by one", ca);

    printf("== speed (software backend, 320x240, 400 monkeys)\n");
    auto bench = [&](bool use_batch) {
        std::vector<uint32_t> tmp; uint32_t c;
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 20; ++i) frame(use_batch, tmp, c);
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 20;
    };
    double ind = bench(false), bat = bench(true);
    printf("  individual draws: %.2f ms/frame | static batch: %.2f ms/frame (%.2fx)  [CPU backend: coarse chunks transform more vertices than per-object culling skips; batching pays off on the GPU where draw calls cost]\n", ind, bat, ind / bat);

    batch.clear();
    CHECK(batch.stats().groups == 0, "clear() empties the batch");
    printf("\n%s (%d failures)\n", g_fail ? "STATIC BATCH TEST FAILED" : "STATIC BATCH TEST PASSED", g_fail);
    return g_fail ? 1 : 0;
}
