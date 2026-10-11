// Headless test + benchmark for the CPU rasterizer and mesh optimizer.
// Build:  g++ -std=c++20 -O3 -Isrc -Ivendor/glm -Ivendor/tinyobj/include \
//             test/soft/soft_raster_test.cpp src/graphics/soft/soft_raster.cpp -o soft_test
// Run:    ./soft_test examples/assets/models/monkey.obj out
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "graphics/mesh_optimize.hpp"
#include "graphics/soft/soft_raster.hpp"

#include <chrono>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

using namespace crayon;

static bool load_obj(const char* path, std::vector<soft::Vertex>& verts, std::vector<uint32_t>& idx) {
    tinyobj::attrib_t attrib; std::vector<tinyobj::shape_t> shapes; std::vector<tinyobj::material_t> mats;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &mats, &warn, &err, path, nullptr)) return false;
    for (auto& sh : shapes) {
        size_t off = 0;
        for (size_t f = 0; f < sh.mesh.num_face_vertices.size(); ++f) {
            size_t fv = sh.mesh.num_face_vertices[f];
            if (fv != 3) { off += fv; continue; }
            for (int k = 0; k < 3; ++k) {
                auto i = sh.mesh.indices[off + k];
                soft::Vertex v{};
                v.position = {attrib.vertices[3*i.vertex_index], attrib.vertices[3*i.vertex_index+1], attrib.vertices[3*i.vertex_index+2]};
                v.normal = i.normal_index >= 0 ? glm::vec3(attrib.normals[3*i.normal_index], attrib.normals[3*i.normal_index+1], attrib.normals[3*i.normal_index+2]) : glm::vec3(0,1,0);
                v.uv = i.texcoord_index >= 0 ? glm::vec2(attrib.texcoords[2*i.texcoord_index], 1.0f - attrib.texcoords[2*i.texcoord_index+1]) : glm::vec2(0);
                v.color = glm::vec4(1.0f);
                idx.push_back((uint32_t)verts.size());
                verts.push_back(v);
            }
            off += 3;
        }
    }
    return true;
}

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
    const char* obj = argc > 1 ? argv[1] : "examples/assets/models/monkey.obj";
    const char* out = argc > 2 ? argv[2] : "out";

    std::vector<soft::Vertex> verts; std::vector<uint32_t> idx;
    if (!load_obj(obj, verts, idx)) { printf("failed to load %s\n", obj); return 1; }
    size_t raw_v = verts.size();
    float acmr_before = mesh_opt::acmr(idx);
    auto t0 = std::chrono::steady_clock::now();
    mesh_opt::weld(verts, idx);
    size_t welded_v = verts.size();
    mesh_opt::reorder_for_cache(verts, idx);
    auto t1 = std::chrono::steady_clock::now();
    printf("mesh: %zu tris | verts %zu -> %zu (%.1fx) | ACMR %.2f -> %.2f | optimize %.1f ms\n",
           idx.size()/3, raw_v, verts.size(), double(raw_v)/double(welded_v),
           acmr_before, mesh_opt::acmr(idx),
           std::chrono::duration<double, std::milli>(t1 - t0).count());

    // sanity: same triangle geometry after optimize (compare sorted position triples)
    {
        std::vector<soft::Vertex> v2; std::vector<uint32_t> i2; load_obj(obj, v2, i2);
        auto key = [](const soft::Vertex& a, const soft::Vertex& b, const soft::Vertex& c) {
            std::vector<glm::vec3> p{a.position, b.position, c.position};
            std::sort(p.begin(), p.end(), [](auto& x, auto& y){ return std::tie(x.x,x.y,x.z) < std::tie(y.x,y.y,y.z); });
            return p;
        };
        std::vector<std::vector<glm::vec3>> A, B;
        for (size_t t = 0; t < i2.size(); t += 3) A.push_back(key(v2[i2[t]], v2[i2[t+1]], v2[i2[t+2]]));
        for (size_t t = 0; t < idx.size(); t += 3) B.push_back(key(verts[idx[t]], verts[idx[t+1]], verts[idx[t+2]]));
        auto cmp = [](auto& x, auto& y){ return std::tie(x[0].x,x[0].y,x[0].z,x[1].x,x[1].y,x[1].z,x[2].x,x[2].y,x[2].z) < std::tie(y[0].x,y[0].y,y[0].z,y[1].x,y[1].y,y[1].z,y[2].x,y[2].y,y[2].z); };
        std::sort(A.begin(), A.end(), cmp); std::sort(B.begin(), B.end(), cmp);
        printf("geometry preserved after weld+reorder: %s\n", A == B ? "YES" : "NO (BUG)");
    }

    // checker texture 64x64
    std::vector<unsigned char> px(64*64*4);
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) {
        bool c = ((x/8) + (y/8)) & 1;
        unsigned char* p = &px[(y*64+x)*4];
        p[0] = c ? 230 : 70; p[1] = c ? 180 : 90; p[2] = c ? 60 : 200; p[3] = 255;
    }
    soft::Texture tex; tex.create(px.data(), 64, 64, 4);

    // floor quad (big, crosses near plane to test clipping)
    std::vector<soft::Vertex> fv = {
        {{-50,0,-50},{0,1,0},{0,0},{1,1,1,1}}, {{-50,0, 50},{0,1,0},{0,25},{1,1,1,1}},
        {{ 50,0, 50},{0,1,0},{25,25},{1,1,1,1}}, {{ 50,0,-50},{0,1,0},{25,0},{1,1,1,1}}};
    std::vector<uint32_t> fi = {0,1,2, 0,2,3};

    const int W = 320, H = 240;
    soft::Rasterizer r; r.resize(W, H);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), float(W)/H, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0, 1.6f, 4.0f), glm::vec3(0, 0.6f, 0), glm::vec3(0, 1, 0));
    r.settings.fog = true; r.settings.fog_start = 6; r.settings.fog_end = 40; r.settings.fog_color = {0.15f,0.2f,0.35f};

    auto render = [&](float t) {
        r.clear({0.15f, 0.2f, 0.35f, 1.0f});
        r.begin(view, proj, {0, 1.6f, 4.0f});
        r.stats.reset();
        r.draw(fv.data(), fv.size(), fi.data(), fi.size(), glm::mat4(1.0f), &tex);
        glm::mat4 m = glm::translate(glm::mat4(1.0f), {0, 0.9f, 0}) * glm::rotate(glm::mat4(1.0f), t, {0,1,0});
        r.draw(verts.data(), verts.size(), idx.data(), idx.size(), m, &tex, {1.0f, 0.9f, 0.8f, 1.0f});
        r.draw_line({-2,0.01f,0}, {2,0.01f,0}, {1,0,0,1});
        r.draw_line({0,0.01f,-2}, {0,0.01f,2}, {0,1,0,1});
    };

    render(0.6f);
    char path[256];
    snprintf(path, sizeof path, "%s_persp.ppm", out); write_ppm(path, r);

    r.settings.affine = true; r.settings.jitter = true; r.settings.jitter_res = {160, 120}; r.settings.dither = true;
    r.settings.shading = soft::Shading::Flat;
    render(0.6f);
    snprintf(path, sizeof path, "%s_retro.ppm", out); write_ppm(path, r);

    // benchmark: default settings, 300 frames
    r.settings.affine = false; r.settings.jitter = false; r.settings.dither = false; r.settings.shading = soft::Shading::Gouraud;
    auto b0 = std::chrono::steady_clock::now();
    const int N = 300;
    for (int i = 0; i < N; ++i) render(i * 0.02f);
    auto b1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(b1 - b0).count() / N;
    printf("bench %dx%d: %.2f ms/frame (%.0f fps) | tris in %llu raster %llu | pixels %llu\n",
           W, H, ms, 1000.0/ms, (unsigned long long)r.stats.triangles_in,
           (unsigned long long)r.stats.triangles_rasterized, (unsigned long long)r.stats.pixels_shaded);
    return 0;
}
