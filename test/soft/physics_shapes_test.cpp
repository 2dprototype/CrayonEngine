// Real-physics tests (Jolt) for the new collision features:
//   heightfield terrain, convex hull bodies, compound bodies, and collision geometry authored in a
//   model (UBX_/UCX_/*_col nodes, custom properties) -- including a glTF written by this test.
#include "graphics/backend.hpp"
#include "graphics/model3d.hpp"
#include "graphics/terrain.hpp"
#include "physics/model_colliders.hpp"
#include "physics/physics_system.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <random>

using namespace crayon;
static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); ++g_fail; } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void simulate(PhysicsSystem& ps, float seconds) {
    for (int i = 0; i < int(seconds * 60); ++i) ps.update(1.0f / 60.0f, 1);
}

static const char* kCubeB64 = "AAAAvwAAAL8AAAC/AAAAPwAAAL8AAAC/AAAAPwAAAD8AAAC/AAAAvwAAAD8AAAC/AAAAvwAAAL8AAAA/AAAAPwAAAL8AAAA/AAAAPwAAAD8AAAA/AAAAvwAAAD8AAAA/AAADAAIAAAACAAEABAAFAAYABAAGAAcAAAAEAAcAAAAHAAMAAQACAAYAAQAGAAUAAAABAAUAAAAFAAQAAwAHAAYAAwAGAAIA";

// One unit cube mesh shared by every node; nodes position/scale it.
static std::string make_gltf() {
    std::string j = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0,1,2,3,4,5]}],
"nodes":[
 {"name":"Floor","mesh":0,"translation":[0,-0.5,0],"scale":[10,1,10]},
 {"name":"Floor_col.001","mesh":0,"translation":[0,-0.5,0],"scale":[10,1,10]},
 {"name":"UCX_Pillar","mesh":0,"translation":[3,1,0],"scale":[1,2,1]},
 {"name":"UBX_Crate","mesh":0,"translation":[-3,0.5,0],"scale":[1,1,1]},
 {"name":"spawn_player","translation":[0,2,5],"extras":{"team":"red","order":3,"tags":["a","b"],"enabled":true}},
 {"name":"Rock","mesh":0,"translation":[20,0,0],"extras":{"collision":"convex"}}],
"meshes":[{"name":"CubeMesh","primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
"accessors":[
 {"bufferView":0,"componentType":5126,"count":8,"type":"VEC3","min":[-0.5,-0.5,-0.5],"max":[0.5,0.5,0.5]},
 {"bufferView":1,"componentType":5123,"count":36,"type":"SCALAR"}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":96,"target":34962},{"buffer":0,"byteOffset":96,"byteLength":72,"target":34963}],
"buffers":[{"byteLength":168,"uri":"data:application/octet-stream;base64,)";
    j += kCubeB64;
    j += "\"}]}";
    return j;
}

int main() {
    set_active_backend(RenderBackend::Software);
    PhysicsSystem ps;
    PhysicsConfig cfg; cfg.num_worker_threads = 0;
    if (!ps.init(cfg)) { printf("physics init failed\n"); return 1; }

    printf("== heightfield terrain\n");
    {
        const int N = 33;
        std::vector<float> h(N * N);
        for (int z = 0; z < N; ++z) for (int x = 0; x < N; ++x) h[z * N + x] = 0.3f + 0.4f * (float(x) / (N - 1)) + 0.1f * std::sin(z * 0.4f);
        Terrain t; Terrain::Options o; o.size = {64, 10, 64}; o.origin = {-32, 2, -32};
        t.build(h, N, N, o);
        uint32_t body = ps.create_heightfield_body(o.origin, h, N, N, o.size);
        CHECK(body != 0, "heightfield body created");
        RaycastHit hit;
        int good = 0, total = 0; float worst = 0;
        for (float x = -28; x <= 28; x += 8) for (float z = -28; z <= 28; z += 8) {
            ps.raycast({x, 50, z}, {0, -1, 0}, 100, hit);
            ++total;
            float err = std::fabs(hit.position.y - t.height_at(x, z));
            worst = std::max(worst, err);
            if (hit.hit && err < 0.35f) ++good;
        }
        CHECK(good == total, "raycasts agree with the visual terrain at %d sample points (worst difference %.3f)", total, worst);
        uint32_t ball = ps.create_sphere({-10, 20, 5}, 0.5f, MotionType::Dynamic, 0.6f, 0.0f);
        simulate(ps, 4.0f);
        glm::vec3 bp = ps.get_position(ball);
        CHECK(std::fabs(bp.y - (t.height_at(bp.x, bp.z) + 0.5f)) < 0.6f && bp.y > 0.0f, "ball rolled to rest on the terrain (y=%.2f, ground+r=%.2f)", bp.y, t.height_at(bp.x, bp.z) + 0.5f);
        ps.destroy_body(ball); ps.destroy_body(body);
    }

    // static floor for the dynamic-body tests (top at y = 0)
    uint32_t floor_id = ps.create_box({0, -0.5f, 0}, {50, 0.5f, 50}, MotionType::Static, 0.6f, 0.0f);

    printf("== convex hull bodies\n");
    {
        ShapePartDesc dummy;
        std::vector<glm::vec3> cube;
        for (int i = 0; i < 8; ++i) cube.push_back({(i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f});
        uint32_t b = ps.create_convex_body({0, 3, 0}, cube, MotionType::Dynamic, 0.6f, 0.0f);
        CHECK(b != 0, "convex hull body created from 8 points");
        simulate(ps, 3.0f);
        CHECK(std::fabs(ps.get_position(b).y - 0.5f) < 0.08f, "dynamic cube hull rests on the floor (y=%.3f, expected 0.5)", ps.get_position(b).y);
        ps.destroy_body(b);

        // 5000 random points on a sphere of radius 0.8 -> simplified hull -> rolls/rests, centre ends at y ~ 0.8
        std::mt19937 rng(5); std::normal_distribution<float> nd(0, 1);
        std::vector<glm::vec3> cloud;
        for (int i = 0; i < 5000; ++i) { glm::vec3 d(nd(rng), nd(rng), nd(rng)); cloud.push_back(glm::normalize(d) * 0.8f); }
        auto simple = simplify_point_cloud(cloud, 128);
        CHECK(simple.size() == 128, "5000-point cloud simplified to %zu points", simple.size());
        uint32_t s = ps.create_convex_body({0, 4, 0}, simple, MotionType::Dynamic, 0.6f, 0.0f);
        simulate(ps, 4.0f);
        float y = ps.get_position(s).y;
        CHECK(s != 0 && y > 0.55f && y < 0.85f, "simplified hull settles on the floor (centre y=%.2f for r=0.8)", y);
        ps.destroy_body(s);
    }

    printf("== compound bodies\n");
    {
        ShapePartDesc slab; slab.kind = ShapePartDesc::Kind::Box; slab.half_extent = {1, 0.1f, 1};
        ShapePartDesc pole; pole.kind = ShapePartDesc::Kind::Box; pole.half_extent = {0.1f, 1, 0.1f}; pole.position = {0, 1.1f, 0};
        std::string err;
        uint32_t b = ps.create_compound_body({0, 3, 0}, {slab, pole}, MotionType::Dynamic, 0.6f, 0.0f, 1000.0f, glm::quat(1, 0, 0, 0), &err);
        CHECK(b != 0, "dynamic compound (slab + pole) created");
        simulate(ps, 4.0f);
        // resting on its slab: the slab's top face must be at y = 0.2 (get_position reports the centre of mass,
        // which the pole shifts upward, so measure the surface with a ray instead)
        RaycastHit rh;
        ps.raycast({0.8f, 10, 0}, {0, -1, 0}, 20, rh);
        CHECK(rh.hit && rh.body_id == b && std::fabs(rh.position.y - 0.2f) < 0.03f, "compound rests flat on its slab (slab top at y=%.3f, expected 0.2)", rh.position.y);
        ps.raycast({0.0f, 10, 0}, {0, -1, 0}, 20, rh);
        CHECK(rh.hit && std::fabs(rh.position.y - 2.2f) < 0.03f, "pole stands upright (top at y=%.3f, expected 2.2)", rh.position.y);
        ps.destroy_body(b);

        ShapePartDesc mesh; mesh.kind = ShapePartDesc::Kind::Mesh;
        mesh.vertices = {{-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1}}; mesh.indices = {0, 2, 1, 0, 3, 2};
        uint32_t bad = ps.create_compound_body({0, 5, 0}, {mesh}, MotionType::Dynamic, 0.5f, 0.2f, 1000.0f, glm::quat(1, 0, 0, 0), &err);
        CHECK(bad == 0 && err.find("static") != std::string::npos, "triangle meshes are refused for dynamic bodies: \"%s\"", err.c_str());
        uint32_t ok = ps.create_compound_body({0, -3, 0}, {mesh, slab}, MotionType::Static, 0.5f, 0.2f, 1000.0f, glm::quat(1, 0, 0, 0), &err);
        CHECK(ok != 0, "static compound with a mesh child is allowed");
        ps.destroy_body(ok);
    }

    printf("== authored collision in a model (glTF written by this test)\n");
    {
        std::string path = "/tmp/crayon_collision_test.gltf";
        std::ofstream(path) << make_gltf();
        Model3D model;
        CHECK(model.load_from_file(path), "glTF loaded");

        std::map<std::string, CollisionKind> kinds;
        std::map<std::string, bool> hidden;
        for (const auto& part : model.get_parts()) {
            const std::string& n = model.get_nodes()[part.node_index].name;
            kinds[n] = part.collision; hidden[n] = part.collision_only;
        }
        CHECK(kinds["Floor"] == CollisionKind::None && !hidden["Floor"], "'Floor' is a normal visible mesh");
        CHECK(kinds["Floor_col.001"] == CollisionKind::Mesh && hidden["Floor_col.001"], "'Floor_col.001' -> trimesh collision, hidden (Blender .001 suffix ignored)");
        CHECK(kinds["UCX_Pillar"] == CollisionKind::Convex && hidden["UCX_Pillar"], "'UCX_Pillar' -> convex hull, hidden");
        CHECK(kinds["UBX_Crate"] == CollisionKind::Box && hidden["UBX_Crate"], "'UBX_Crate' -> box, hidden");
        CHECK(kinds["Rock"] == CollisionKind::Convex && !hidden["Rock"], "custom property collision=\"convex\" -> convex hull but stays visible");
        CHECK(model.has_collision_parts(), "model reports collision parts");

        // name classifier
        CHECK(Model3D::classify_collision_name("Wall_col") == CollisionKind::Mesh && Model3D::classify_collision_name("col_Wall") == CollisionKind::Mesh &&
              Model3D::classify_collision_name("Wall-collision") == CollisionKind::Mesh && Model3D::classify_collision_name("Wall_col_convex") == CollisionKind::Convex &&
              Model3D::classify_collision_name("USP_ball") == CollisionKind::Sphere && Model3D::classify_collision_name("ucp_Leg") == CollisionKind::Capsule &&
              Model3D::classify_collision_name("Column") == CollisionKind::None && Model3D::classify_collision_name("Collar") == CollisionKind::None, "name classifier handles prefixes, suffixes, tokens, and rejects look-alikes (Column, Collar)");

        // custom properties
        const ModelNode* spawn = nullptr;
        for (const auto& n : model.get_nodes()) if (n.name == "spawn_player") spawn = &n;
        CHECK(spawn != nullptr, "empty node 'spawn_player' is present");
        if (spawn) {
            const ModelProperty* team = spawn->extras.find("team");
            const ModelProperty* order = spawn->extras.find("order");
            const ModelProperty* tags = spawn->extras.find("tags");
            const ModelProperty* en = spawn->extras.find("enabled");
            CHECK(team && team->string == "red" && order && order->number == 3.0 && en && en->boolean && tags && tags->array.size() == 2 && tags->array[1].string == "b",
                  "custom properties parsed (team, order, enabled, tags[])");
            CHECK(std::fabs(spawn->world_matrix[3].z - 5.0f) < 1e-4f && std::fabs(spawn->world_matrix[3].y - 2.0f) < 1e-4f, "node world position = (0,2,5)");
        }

        // build colliders
        ModelColliderOptions opt;
        std::vector<ShapePartDesc> parts; std::string err;
        CHECK(build_model_collider_parts(model, opt, parts, err), "collider parts built from authored nodes (%zu shapes)", parts.size());
        int boxes = 0, convex = 0, meshes = 0;
        for (const auto& p : parts) { boxes += p.kind == ShapePartDesc::Kind::Box; convex += p.kind == ShapePartDesc::Kind::Convex; meshes += p.kind == ShapePartDesc::Kind::Mesh; }
        CHECK(boxes == 1 && convex == 2 && meshes == 1, "1 box (UBX) + 2 hulls (UCX, Rock) + 1 merged trimesh (Floor_col): got %d / %d / %d", boxes, convex, meshes);
        for (const auto& p : parts) if (p.kind == ShapePartDesc::Kind::Box) CHECK(std::fabs(p.position.x + 3.0f) < 1e-3f && std::fabs(p.half_extent.y - 0.5f) < 1e-3f, "UBX box placed at (-3,0.5,0) with half extents 0.5");

        uint32_t level = ps.create_compound_body({0, 0, 0}, parts, MotionType::Static, 0.6f, 0.0f, 1000.0f, glm::quat(1, 0, 0, 0), &err);
        CHECK(level != 0, "static level body created from the model (%s)", err.c_str());
        uint32_t ball = ps.create_sphere({1, 6, 1}, 0.5f, MotionType::Dynamic, 0.6f, 0.0f);   // lands on the trimesh floor (top y=0)
        uint32_t onpillar = ps.create_sphere({3, 8, 0}, 0.5f, MotionType::Dynamic, 0.6f, 0.0f); // lands on the hull pillar (top y=2.0)
        uint32_t oncrate = ps.create_sphere({-3, 8, 0}, 0.5f, MotionType::Dynamic, 0.6f, 0.0f); // lands on the box crate (top y=1.0)
        simulate(ps, 4.0f);
        CHECK(std::fabs(ps.get_position(ball).y - 0.5f) < 0.12f, "ball rests on the authored trimesh floor (y=%.2f)", ps.get_position(ball).y);
        CHECK(std::fabs(ps.get_position(onpillar).y - 2.5f) < 0.25f, "ball rests on top of the convex pillar (y=%.2f, expected ~2.5)", ps.get_position(onpillar).y);
        CHECK(std::fabs(ps.get_position(oncrate).y - 1.5f) < 0.2f, "ball rests on top of the box crate (y=%.2f, expected ~1.5)", ps.get_position(oncrate).y);

        // the hidden collision geometry is not drawn / counted in the model bounds
        glm::vec3 mn, mx; model.get_bounds(mn, mx);
        CHECK(mx.x < 25.0f && mn.y > -1.1f, "collision-only parts do not inflate the model bounds");

        // visual-geometry fallback (no authored collision): a model without collision nodes
        Model3D plain;
        std::ofstream("/tmp/crayon_plain.gltf") << make_gltf();   // reuse, then ignore authored parts by loading a model whose parts all have collision==None
        plain.load_from_file("/tmp/crayon_plain.gltf");
        ModelColliderOptions dyn; dyn.dynamic = true; dyn.shape = "auto";
        // build from only the visible 'Floor' part by cloning a one-part model through create_from_mesh
        std::shared_ptr<Mesh3D> cube = Mesh3D::create_cube(1.0f);
        auto visual = Model3D::create_from_mesh(cube, "cube");
        std::vector<ShapePartDesc> vp;
        CHECK(build_model_collider_parts(*visual, dyn, vp, err) && vp.size() == 1 && vp[0].kind == ShapePartDesc::Kind::Convex, "visual-only model + dynamic -> convex hull (auto)");
        ModelColliderOptions st; st.shape = "auto";
        CHECK(build_model_collider_parts(*visual, st, vp, err) && vp.size() == 1 && vp[0].kind == ShapePartDesc::Kind::Mesh, "visual-only model + static -> triangle mesh (auto)");
        ModelColliderOptions bad; bad.shape = "mesh"; bad.dynamic = true;
        CHECK(!build_model_collider_parts(*visual, bad, vp, err) && err.find("static-only") != std::string::npos, "mesh shape for a moving body is a clear error: \"%s\"", err.c_str());
        ModelColliderOptions sc; sc.shape = "box"; sc.scale = {2, 4, 2};
        CHECK(build_model_collider_parts(*visual, sc, vp, err) && std::fabs(vp[0].half_extent.y - 2.0f) < 1e-3f, "scale is applied to the generated box (half y = %.2f)", vp[0].half_extent.y);
    }

    printf("\n%s (%d failures)\n", g_fail ? "PHYSICS SHAPES TEST FAILED" : "PHYSICS SHAPES TEST PASSED", g_fail);
    ps.shutdown();
    return g_fail ? 1 : 0;
}
