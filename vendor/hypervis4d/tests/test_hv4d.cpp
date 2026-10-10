// Standalone self-test for hv4d (no framework needed).
//   g++ -std=c++17 -O2 -Iinclude tests/test_hv4d.cpp src/*.cpp -o test_hv4d && ./test_hv4d
// or build the `hv4d_tests` target with -DHV4D_BUILD_TESTS=ON.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

#include "hv4d/hv4d.hpp"

using namespace hv4d;

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        if (cond) {                                                         \
            ++g_passed;                                                     \
        } else {                                                            \
            ++g_failed;                                                     \
            std::printf("  FAIL %s:%d  %s\n       ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                                       \
            std::printf("\n");                                              \
        }                                                                   \
    } while (0)

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }
static bool finite_body(const Body& b) {
    return b.pos.is_finite() && b.vel.linear.is_finite() && std::isfinite(b.vel.angular.length()) &&
           std::isfinite(b.rotation.mag());
}

static void section(const char* name) { std::printf("[%s]\n", name); }

// ---------------------------------------------------------------------------
static void test_math() {
    section("math");
    Rotor4 r;
    for (int i = 0; i < 100000; ++i) r.update(Bivec4(1, 2, 3, 4, 5, 6) * 0.001f);
    CHECK(near(r.mag(), 1.0f, 1e-4f), "rotor magnitude drifted: %f", r.mag());
    CHECK(near(r.weird_term(), 0.0f, 1e-4f), "rotor weird term: %f", r.weird_term());
    CHECK(near(r.to_matrix().determinant(), 1.0f, 1e-3f), "det=%f", r.to_matrix().determinant());

    // rotation preserves lengths and dot products
    Vec4 a(1, 2, 3, 4), b(-2, 0.5f, 1, 3);
    CHECK(near(r.rotate(a).length(), a.length(), 1e-4f), "length not preserved");
    CHECK(near(r.rotate(a).dot(r.rotate(b)), a.dot(b), 1e-3f), "dot not preserved");

    // rotor * reverse = identity action
    Vec4 back = r.reverse().rotate(r.rotate(a));
    CHECK((back - a).length() < 1e-3f, "reverse rotation does not invert");

    // composition: (R1*R2).rotate(v) == R1.rotate(R2.rotate(v))
    Rotor4 r1 = Rotor4::from_angles(Bivec4(0.3f, -0.2f, 0.5f, 0.1f, 0.7f, -0.4f));
    Rotor4 r2 = Rotor4::from_angles(Bivec4(-0.6f, 0.1f, 0.2f, 0.9f, -0.3f, 0.25f));
    Vec4 c1 = (r1 * r2).rotate(a), c2 = r1.rotate(r2.rotate(a));
    CHECK((c1 - c2).length() < 1e-4f, "composition mismatch (%f)", (c1 - c2).length());

    // angular velocity convention: update(w*dt) moves p by p.left_contract(w)*dt (what vel_at assumes)
    const Bivec4 planes[6] = {{1, 0, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 0},
                              {0, 0, 0, 1, 0, 0}, {0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 1}};
    Vec4 p(0.3f, -0.7f, 0.5f, 0.9f);
    for (int i = 0; i < 6; ++i) {
        Rotor4 R;
        R.update(planes[i] * 1e-3f);
        const Vec4 numeric = (R.rotate(p) - p) / 1e-3f;
        const Vec4 formula = p.left_contract_bv(planes[i]);
        CHECK((numeric - formula).length() < 5e-3f, "plane %d: angular velocity convention off", i);
    }

    // triple cross product is perpendicular to all three arguments
    Vec4 u(1, 2, 3, 4), v(-1, 0.5f, 2, 1), w(0.3f, -2, 1, 1);
    Vec4 t = triple_cross_product(u, v, w);
    CHECK(near(t.dot(u), 0, 1e-4f) && near(t.dot(v), 0, 1e-4f) && near(t.dot(w), 0, 1e-4f), "tcp not perpendicular");

    Vec4 basis[3];
    Vec4 n = Vec4(0.2f, 0.1f, -0.7f, 0.5f).normalized();
    orthonormal_basis(n, basis);
    for (int i = 0; i < 3; ++i) {
        CHECK(near(basis[i].length(), 1, 1e-4f), "basis not unit");
        CHECK(near(basis[i].dot(n), 0, 1e-4f), "basis not perpendicular to n");
        for (int j = i + 1; j < 3; ++j) CHECK(near(basis[i].dot(basis[j]), 0, 1e-4f), "basis not orthogonal");
    }
}

// ---------------------------------------------------------------------------
static void test_meshes() {
    section("meshes");
    struct E { const char* n; RegularSolid s; int V, E, F, C; float vol; };
    // hypervolumes for circumradius 1
    const E ex[] = {
        {"5-cell", RegularSolid::FiveCell, 5, 10, 10, 5, 0.1456f},
        {"8-cell", RegularSolid::EightCell, 16, 32, 24, 8, 1.0f},
        {"16-cell", RegularSolid::SixteenCell, 8, 24, 32, 16, 0.6667f},
        {"24-cell", RegularSolid::TwentyFourCell, 24, 96, 96, 24, 2.0f},
        {"120-cell", RegularSolid::OneTwentyCell, 600, 1200, 720, 120, 4.1926f},
        {"600-cell", RegularSolid::SixHundredCell, 120, 720, 1200, 600, 3.8627f},
    };
    for (const E& e : ex) {
        Mesh m = Mesh::from_regular_solid(e.s);
        CHECK((int)m.vertices.size() == e.V && (int)m.edges.size() == e.E && (int)m.faces.size() == e.F &&
                  (int)m.cells.size() == e.C,
              "%s: V%zu E%zu F%zu C%zu", e.n, m.vertices.size(), m.edges.size(), m.faces.size(), m.cells.size());
        CHECK((int)m.vertices.size() - (int)m.edges.size() + (int)m.faces.size() - (int)m.cells.size() == 0,
              "%s euler", e.n);
        CHECK(near(m.volume, e.vol, 2e-3f), "%s volume %f vs %f", e.n, m.volume, e.vol);
        for (const Vec4& v : m.vertices) CHECK(near(v.length(), 1.0f, 1e-4f), "%s vertex off sphere", e.n);
        // convexity + outward normals
        int bad = 0;
        for (const Cell& c : m.cells) {
            const float d = m.cell_representative_vertex(c).dot(c.normal);
            for (const Vec4& v : m.vertices)
                if (v.dot(c.normal) > d + 1e-4f) ++bad;
        }
        CHECK(bad == 0, "%s: %d vertices outside cell planes", e.n, bad);
        for (const Face& f : m.faces) CHECK(f.hd_cell >= 0 && f.tl_cell >= 0, "%s face w/o 2 cells", e.n);
    }

    Mesh t = Mesh::from_regular_solid(RegularSolid::EightCell);
    CHECK(near(t.inertia_per_mass, 1.0f / 6.0f, 1e-4f), "tesseract inertia %f", t.inertia_per_mass);
    Mesh t2 = t.scaled(2.0f);
    CHECK(near(t2.volume, 16.0f, 1e-2f), "scaled volume %f (expect 16)", t2.volume);
    CHECK(near(t2.inertia_per_mass, 4.0f / 6.0f, 1e-3f), "scaled inertia %f", t2.inertia_per_mass);

    // closest point
    Vec4 q(3, 0.1f, -0.2f, 0.3f);
    Vec4 cp = t.closest_point_to(q);
    CHECK(near(cp.x, 0.5f, 1e-4f) && near(cp.y, 0.1f, 1e-4f) && near(cp.z, -0.2f, 1e-4f) && near(cp.w, 0.3f, 1e-4f),
          "closest face point (%f %f %f %f)", cp.x, cp.y, cp.z, cp.w);
    Vec4 cc = t.closest_point_to(Vec4(2, 2, 2, 2));
    CHECK(near(cc.x, 0.5f, 1e-4f) && near(cc.y, 0.5f, 1e-4f) && near(cc.z, 0.5f, 1e-4f) && near(cc.w, 0.5f, 1e-4f),
          "closest corner point");
    // edge-region queries: three coordinates out of range -> nearest point is on an edge
    Vec4 ce = t.closest_point_to(Vec4(2, 2, 2, 0.2f));
    CHECK(near(ce.x, 0.5f, 1e-4f) && near(ce.y, 0.5f, 1e-4f) && near(ce.z, 0.5f, 1e-4f) && near(ce.w, 0.2f, 1e-4f),
          "closest edge point (%f %f %f %f)", ce.x, ce.y, ce.z, ce.w);
    CHECK(t.contains(Vec4(0.1f, 0.1f, 0.1f, 0.1f)) && !t.contains(Vec4(0.6f, 0, 0, 0)), "contains");

    // geodesic sphere
    TetraSoup soup = tetra_soup_from_mesh(Mesh::from_regular_solid(RegularSolid::SixHundredCell));
    TetraSoup geo = make_geodesic(soup, 3, 0.75f);
    bool on_sphere = true;
    for (const Vec4& p : geo.positions) on_sphere &= near(p.length(), 0.75f, 1e-4f);
    CHECK(on_sphere && !geo.indices.empty() && geo.indices.size() % 4 == 0, "geodesic");
}

// ---------------------------------------------------------------------------
static void test_contacts() {
    section("contact generation");
    World w;
    BodyId a = w.create_polytope(RegularSolid::EightCell, Vec4(0, 0, 0, 0));
    BodyId b = w.create_polytope(RegularSolid::EightCell, Vec4(0.9f, 0.05f, 0.02f, 0.01f));
    BodyId c = w.create_polytope(RegularSolid::EightCell, Vec4(2.5f, 0, 0, 0));

    auto m = w.collision().detect_collisions(a, b, *w.get(a), *w.get(b));
    CHECK(m.has_value() && !m->contacts.empty(), "overlapping tesseracts must collide");
    if (m) {
        CHECK(m->normal.x > 0.9f, "normal should point a->b along +x, got (%f %f %f %f)", m->normal.x, m->normal.y,
              m->normal.z, m->normal.w);
        CHECK(near(m->normal.length(), 1.0f, 1e-3f), "normal not unit");
        CHECK(m->depth > 0.05f && m->depth < 0.2f, "depth %f (expect ~0.1)", m->depth);
        for (const Vec4& p : m->contacts) CHECK(p.is_finite(), "contact finite");
    }
    // swapped arguments flip the normal
    auto m2 = w.collision().detect_collisions(b, a, *w.get(b), *w.get(a));
    CHECK(m2.has_value() && m2->normal.x < -0.9f, "swapped normal should flip");
    CHECK(!w.collision().detect_collisions(a, c, *w.get(a), *w.get(c)).has_value(), "separated -> no collision");

    // rotated: still detects
    w.get(b)->rotation = Rotor4::from_angles(Bivec4(0.4f, 0.3f, 0.5f, 0.2f, 0.6f, 0.1f));
    w.get(b)->pos = Vec4(0.75f, 0.0f, 0.0f, 0.0f);
    m = w.collision().detect_collisions(a, b, *w.get(a), *w.get(b));
    CHECK(m.has_value() && !m->contacts.empty(), "rotated overlap collides");
    w.get(b)->pos = Vec4(2.0f, 0.0f, 0.0f, 0.0f);
    CHECK(!w.collision().detect_collisions(a, b, *w.get(a), *w.get(b)).has_value(), "rotated but far apart");

    // sphere vs half-space
    World w2;
    BodyId fl = w2.create_half_space(Vec4(0, 0, 0, 0), Vec4(0, 1, 0, 0));
    BodyId sp = w2.create_hypersphere(Vec4(0, 0.4f, 0, 0), 0.5f);
    auto ms = w2.collision().detect_collisions(fl, sp, *w2.get(fl), *w2.get(sp));
    CHECK(ms && near(ms->depth, 0.1f, 1e-4f) && near(ms->normal.y, 1.0f, 1e-5f), "sphere/floor");
    auto msr = w2.collision().detect_collisions(sp, fl, *w2.get(sp), *w2.get(fl));
    CHECK(msr && near(msr->normal.y, -1.0f, 1e-5f), "sphere/floor (swapped)");

    // tesseract vs half-space: lying flat => 8 contact vertices
    BodyId tb = w2.create_polytope(RegularSolid::EightCell, Vec4(0, 0.45f, 0, 0));
    auto mt = w2.collision().detect_collisions(fl, tb, *w2.get(fl), *w2.get(tb));
    CHECK(mt && mt->contacts.size() == 8 && near(mt->depth, 0.05f, 1e-4f), "tesseract/floor: %zu contacts depth=%f",
          mt ? mt->contacts.size() : 0, mt ? mt->depth : -1.0f);

    // mesh vs sphere (incl. centre inside)
    BodyId ms1 = w2.create_polytope(RegularSolid::EightCell, Vec4(5, 5, 5, 5));
    BodyId ss1 = w2.create_hypersphere(Vec4(5.9f, 5, 5, 5), 0.5f);
    auto mm = w2.collision().detect_collisions(ms1, ss1, *w2.get(ms1), *w2.get(ss1));
    CHECK(mm && near(mm->depth, 0.1f, 1e-3f) && mm->normal.x > 0.99f, "mesh/sphere face contact");
    w2.get(ss1)->pos = Vec4(5.4f, 5, 5, 5);
    mm = w2.collision().detect_collisions(ms1, ss1, *w2.get(ms1), *w2.get(ss1));
    CHECK(mm && mm->normal.x > 0.99f && mm->depth > 0.5f, "mesh/sphere centre inside");

    // sphere vs sphere
    World w3;
    BodyId s1 = w3.create_hypersphere(Vec4(0, 0, 0, 0), 0.5f);
    BodyId s2 = w3.create_hypersphere(Vec4(0.8f, 0, 0, 0.3f), 0.5f);
    auto mss = w3.collision().detect_collisions(s1, s2, *w3.get(s1), *w3.get(s2));
    CHECK(mss && near(mss->depth, 1.0f - std::sqrt(0.64f + 0.09f), 1e-4f), "sphere/sphere depth");
}

// ---------------------------------------------------------------------------
static void test_simulation() {
    section("simulation");
    const float dt = 1.0f / 60.0f;

    // 1. a tesseract dropped on the floor comes to rest at half its edge length
    {
        World w;
        w.create_arena(6.0f);
        BodyId t = w.create_polytope(RegularSolid::EightCell, Vec4(0, 3, 0, 0));
        for (int i = 0; i < 60 * 6; ++i) w.update(dt);
        const Body* b = w.get(t);
        CHECK(finite_body(*b), "tesseract finite");
        CHECK(near(b->pos.y, 0.5f, 0.06f), "tesseract rest height %f (expect ~0.5)", b->pos.y);
        CHECK(b->vel.linear.length() < 0.15f, "tesseract not at rest: |v|=%f", b->vel.linear.length());
        CHECK(std::fabs(b->pos.x) < 0.5f && std::fabs(b->pos.z) < 0.5f && std::fabs(b->pos.w) < 0.5f,
              "tesseract drifted sideways");
    }

    // 2. a hypersphere bounces (restitution) and settles at its radius
    {
        World w;
        w.create_arena(6.0f);
        BodyId s = w.create_hypersphere(Vec4(0, 4, 0, 0), 0.5f);
        w.get(s)->material.restitution = 0.6f;
        float max_after_bounce = 0.0f;
        bool hit = false;
        for (int i = 0; i < 60 * 8; ++i) {
            w.update(dt);
            const Body* b = w.get(s);
            if (b->vel.linear.y > 0.5f) hit = true;
            if (hit) max_after_bounce = std::max(max_after_bounce, b->pos.y);
        }
        CHECK(hit && max_after_bounce > 0.9f, "sphere should bounce (peak %f)", max_after_bounce);
        CHECK(near(w.get(s)->pos.y, 0.5f, 0.06f), "sphere rest %f", w.get(s)->pos.y);
    }

    // 3. every regular solid drops and rests without blowing up
    {
        const RegularSolid solids[] = {RegularSolid::FiveCell, RegularSolid::EightCell, RegularSolid::SixteenCell,
                                       RegularSolid::TwentyFourCell, RegularSolid::OneTwentyCell,
                                       RegularSolid::SixHundredCell};
        const char* names[] = {"5-cell", "8-cell", "16-cell", "24-cell", "120-cell", "600-cell"};
        for (int i = 0; i < 6; ++i) {
            World w;
            w.create_arena(6.0f);
            BodyId id = w.create_polytope(solids[i], Vec4(0.1f, 3, 0.05f, -0.1f), 0.8f);
            w.get(id)->rotation = Rotor4::from_angles(Bivec4(0.3f, 0.2f, 0.4f, 0.1f, 0.5f, 0.25f));
            auto t0 = std::chrono::steady_clock::now();
            for (int s = 0; s < 60 * 6; ++s) w.update(dt);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            const Body* b = w.get(id);
            CHECK(finite_body(*b), "%s finite", names[i]);
            // lowest vertex must not be far below the floor
            float lowest = 1e9f;
            for (const Vec4& v : b->collider.mesh->vertices) lowest = std::min(lowest, b->body_pos_to_world(v).y);
            CHECK(lowest > -0.08f, "%s sank through floor (lowest %f)", names[i], lowest);
            CHECK(b->vel.linear.length() < 1.0f, "%s speed %f", names[i], b->vel.linear.length());
            std::printf("  %-9s rest y=%.3f lowest=%.3f  360 steps in %.1f ms\n", names[i], b->pos.y, lowest, ms);
        }
    }

    // 4. a small stack stays up
    {
        World w;
        w.create_arena(6.0f);
        BodyId s0 = w.create_polytope(RegularSolid::EightCell, Vec4(0, 0.5f, 0, 0));
        BodyId s1 = w.create_polytope(RegularSolid::EightCell, Vec4(0, 1.55f, 0, 0));
        BodyId s2 = w.create_polytope(RegularSolid::EightCell, Vec4(0, 2.6f, 0, 0));
        for (int i = 0; i < 60 * 5; ++i) w.update(dt);
        CHECK(finite_body(*w.get(s0)) && finite_body(*w.get(s1)) && finite_body(*w.get(s2)), "stack finite");
        CHECK(w.get(s1)->pos.y > w.get(s0)->pos.y + 0.8f && w.get(s2)->pos.y > w.get(s1)->pos.y + 0.8f,
              "stack collapsed: y=%f %f %f", w.get(s0)->pos.y, w.get(s1)->pos.y, w.get(s2)->pos.y);
        CHECK(std::fabs(w.get(s2)->pos.x) < 0.6f && std::fabs(w.get(s2)->pos.w) < 0.6f, "stack toppled sideways");
    }

    // 5. a hypersphere knocks over a tesseract (momentum transfer)
    {
        World w;
        w.gravity = Vec4::zero();
        BodyId s = w.create_hypersphere(Vec4(-2, 0, 0, 0), 0.5f, 2.0f);
        BodyId t = w.create_polytope(RegularSolid::EightCell, Vec4(0, 0, 0, 0), 1.0f, 1.0f);
        w.get(s)->vel.linear = Vec4(4, 0, 0, 0);
        w.get(s)->material.restitution = 0.0f;
        w.get(t)->material.restitution = 0.0f;
        const Vec4 p0 = w.get(s)->vel.linear * w.get(s)->mass;
        for (int i = 0; i < 60 * 2; ++i) w.update(dt);
        const Vec4 p1 = w.get(s)->vel.linear * w.get(s)->mass + w.get(t)->vel.linear * w.get(t)->mass;
        CHECK(w.get(t)->vel.linear.x > 1.0f, "tesseract should be pushed (vx=%f)", w.get(t)->vel.linear.x);
        CHECK(near(p1.x, p0.x, 0.8f), "linear momentum along x: %f -> %f", p0.x, p1.x);
        CHECK(finite_body(*w.get(s)) && finite_body(*w.get(t)), "finite after hit");
    }

    // 6. fourth-dimension separation: same xyz, different w => no collision
    {
        World w;
        w.gravity = Vec4::zero();
        BodyId a = w.create_polytope(RegularSolid::EightCell, Vec4(0, 0, 0, 0));
        BodyId b = w.create_polytope(RegularSolid::EightCell, Vec4(0, 0, 0, 3.0f));
        for (int i = 0; i < 30; ++i) w.update(dt);
        CHECK(w.last_contacts().empty(), "bodies separated in w must not interact");
        (void)a; (void)b;
    }

    // 7. events: enter / exit and triggers
    {
        World w;
        w.create_arena(6.0f);
        BodyId s = w.create_hypersphere(Vec4(0, 1.0f, 0, 0), 0.5f);
        BodyId sensor = w.create_polytope(RegularSolid::EightCell, Vec4(4, 1, 0, 0), 1.5f, 1.0f, true);
        w.get(sensor)->sensor = true;
        int enters = 0, exits = 0, tenter = 0, texit = 0;
        for (int i = 0; i < 60 * 2; ++i) {
            if (i == 60) w.get(s)->pos = Vec4(4, 1, 0, 0);       // teleport into the sensor
            if (i == 90) w.get(s)->pos = Vec4(-4, 1, 0, 0);      // and out again
            w.update(dt);
            for (const WorldEvent& e : w.take_events()) {
                if (e.type == WorldEventType::CollisionEnter) ++enters;
                if (e.type == WorldEventType::CollisionExit) ++exits;
                if (e.type == WorldEventType::TriggerEnter) { ++tenter; CHECK(e.a == sensor, "sensor listed first"); }
                if (e.type == WorldEventType::TriggerExit) ++texit;
            }
        }
        CHECK(enters >= 1 && exits >= 1, "collision events enter=%d exit=%d", enters, exits);
        CHECK(tenter == 1 && texit == 1, "trigger events enter=%d exit=%d", tenter, texit);
    }

    // 8. determinism
    {
        auto run = []() {
            World w;
            w.create_arena(6.0f);
            for (int i = 0; i < 4; ++i)
                w.create_polytope(i % 2 ? RegularSolid::EightCell : RegularSolid::SixteenCell,
                                  Vec4(0.2f * i, 1.0f + 1.3f * i, 0.1f * i, 0.0f), 0.7f);
            for (int i = 0; i < 200; ++i) w.update(1.0f / 60.0f);
            return w.get(3)->pos;
        };
        Vec4 r1 = run(), r2 = run();
        CHECK((r1 - r2).length() == 0.0f, "simulation must be deterministic");
    }
}

// ---------------------------------------------------------------------------
static void test_gjk() {
    section("gjk / epa");
    World w;
    BodyId s1 = w.create_hypersphere(Vec4(0, 0, 0, 0), 0.5f);
    BodyId s2 = w.create_hypersphere(Vec4(1.5f, 0, 0, 0), 0.5f);
    GjkResult r = gjk_epa(*w.get(s1), *w.get(s2));
    CHECK(!r.intersecting && near(r.distance, 0.5f, 1e-3f), "sphere distance %f", r.distance);
    CHECK(r.separating_direction.x > 0.99f, "separating direction A->B");

    w.get(s2)->pos = Vec4(0.7f, 0, 0, 0);
    r = gjk_epa(*w.get(s1), *w.get(s2));
    CHECK(r.intersecting && r.epa_ok, "overlapping spheres: intersecting=%d epa=%d", r.intersecting, r.epa_ok);
    CHECK(near(r.penetration_depth, 0.3f, 0.02f), "sphere depth %f (expect 0.3)", r.penetration_depth);
    CHECK(r.penetration_normal.x > 0.95f, "sphere EPA normal A->B (%f)", r.penetration_normal.x);

    BodyId t1 = w.create_polytope(RegularSolid::EightCell, Vec4(10, 0, 0, 0));
    BodyId t2 = w.create_polytope(RegularSolid::EightCell, Vec4(10.8f, 0.0f, 0.0f, 0.0f));
    r = gjk_epa(*w.get(t1), *w.get(t2));
    CHECK(r.intersecting && r.epa_ok, "overlapping tesseracts");
    CHECK(near(r.penetration_depth, 0.2f, 0.02f), "tesseract depth %f (expect 0.2)", r.penetration_depth);
    CHECK(r.penetration_normal.x > 0.95f, "tesseract EPA normal (%f %f %f %f)", r.penetration_normal.x,
          r.penetration_normal.y, r.penetration_normal.z, r.penetration_normal.w);

    w.get(t2)->pos = Vec4(12.0f, 0.3f, 0.0f, 0.0f);
    r = gjk_epa(*w.get(t1), *w.get(t2));
    CHECK(!r.intersecting && near(r.distance, 1.0f, 2e-3f), "tesseract gap %f (expect 1.0)", r.distance);

    // agree with SAT on a batch of random-ish poses
    int disagree = 0, total = 0;
    BodyId a = w.create_polytope(RegularSolid::SixteenCell, Vec4(30, 0, 0, 0));
    BodyId b = w.create_polytope(RegularSolid::EightCell, Vec4(30, 0, 0, 0));
    for (int i = 0; i < 60; ++i) {
        const float f = static_cast<float>(i);
        w.get(a)->rotation = Rotor4::from_angles(Bivec4(0.1f * f, 0.2f, 0.05f * f, 0.3f, 0.01f * f, 0.4f));
        w.get(b)->rotation = Rotor4::from_angles(Bivec4(0.3f, 0.07f * f, 0.2f, 0.02f * f, 0.5f, 0.1f));
        w.get(b)->pos = Vec4(30 + 0.04f * f, 0.5f - 0.02f * f, 0.1f, 0.3f - 0.01f * f);
        const bool sat = w.collision().detect_collisions(a, b, *w.get(a), *w.get(b)).has_value();
        const bool gjk = gjk_epa(*w.get(a), *w.get(b)).intersecting;
        ++total;
        if (sat != gjk) ++disagree;
        w.collision().clear_cache();
    }
    CHECK(disagree <= 2, "SAT vs GJK disagree on %d / %d poses", disagree, total);
}

// ---------------------------------------------------------------------------
static void test_slice() {
    section("slice");
    World w;
    BodyId t = w.create_polytope(RegularSolid::EightCell, Vec4(0, 0, 0, 0));
    SlicePlane p = SlicePlane::at_w(0.0f);

    std::vector<SliceTriangle> tris;
    slice_body_triangles(*w.get(t), p, tris);
    // The cube's 6 square faces come from the 6 side cells (each fan-triangulated),
    // none from the two +-w cells that are parallel to the slice.
    float area = 0;
    int from_w_cells = 0;
    for (const auto& tr : tris) {
        area += 0.5f * length(cross(tr.p[1] - tr.p[0], tr.p[2] - tr.p[0]));
        const Vec4 cn = w.get(t)->collider.mesh->cells[tr.cell].normal;
        if (std::fabs(cn.w) > 0.5f) ++from_w_cells;
    }
    CHECK(near(area, 6.0f, 0.05f), "cube surface area %f (expect 6)", area);
    CHECK(from_w_cells == 0, "no triangles from +-w cells");

    std::vector<SliceSegment> segs;
    slice_body_wireframe(*w.get(t), p, segs);
    CHECK(segs.size() == 12, "cube wireframe = 12 edges, got %zu", segs.size());
    for (const auto& s : segs) CHECK(near(length(s.a - s.b), 1.0f, 1e-3f), "edge length");

    // shifted slice: still a cube; beyond the body: nothing
    p = SlicePlane::at_w(0.3f);
    tris.clear(); segs.clear();
    slice_body_triangles(*w.get(t), p, tris);
    slice_body_wireframe(*w.get(t), p, segs);
    area = 0;
    for (const auto& tr : tris) area += 0.5f * length(cross(tr.p[1] - tr.p[0], tr.p[2] - tr.p[0]));
    CHECK(near(area, 6.0f, 0.05f) && segs.size() == 12, "w=0.3 still a cube (area %f, %zu edges)", area, segs.size());
    p = SlicePlane::at_w(0.6f);
    tris.clear(); segs.clear();
    slice_body_triangles(*w.get(t), p, tris);
    slice_body_wireframe(*w.get(t), p, segs);
    CHECK(tris.empty() && segs.empty(), "w=0.6 misses the tesseract");

    // rotating in the xw plane by 45deg: slice at w=0 becomes a longer rectangular prism-like shape
    w.get(t)->rotation = Rotor4::from_angles(Bivec4(0, 0, PI / 4.0f, 0, 0, 0));
    p = SlicePlane::at_w(0.0f);
    tris.clear();
    slice_body_triangles(*w.get(t), p, tris);
    CHECK(!tris.empty(), "rotated slice has geometry");
    float xmax = 0;
    for (const auto& tr : tris) for (const auto& v : tr.p) xmax = std::max(xmax, std::fabs(v.x));
    CHECK(near(xmax, 0.5f * std::sqrt(2.0f), 0.02f), "45 deg xw rotation stretches slice to x=+-0.707 (got %f)", xmax);

    // sphere slice
    BodyId s = w.create_hypersphere(Vec4(1, 2, 3, 0.3f), 0.5f);
    Vec3 c; float r;
    CHECK(slice_sphere(*w.get(s), SlicePlane::at_w(0.0f), c, r), "sphere slice");
    CHECK(near(r, 0.4f, 1e-4f) && near(c.x, 1, 1e-5f) && near(c.y, 2, 1e-5f) && near(c.z, 3, 1e-5f), "radius %f", r);
    CHECK(!slice_sphere(*w.get(s), SlicePlane::at_w(1.0f), c, r), "sphere slice miss");

    // half-space slice
    BodyId h = w.create_half_space(Vec4(0, 1, 0, 0), Vec4(0, 1, 0.5f, 0.5f));
    Vec3 n3; float off;
    CHECK(slice_half_space(*w.get(h), SlicePlane::at_w(0.0f), n3, off), "halfspace slice");
    CHECK(near(off, 0.5f * 0 + 1.0f * 0.707107f * 0 + w.get(h)->collider.normal.y * 1.0f, 1e-4f), "offset %f", off);

    // general (tilted) slice plane keeps the topology of a section of a tesseract
    SlicePlane tilt = SlicePlane::from_normal(Vec4(0.2f, 0.1f, 0.0f, 1.0f), Vec4(0, 0, 0, 0));
    BodyId t2 = w.create_polytope(RegularSolid::EightCell, Vec4(5, 5, 5, 5));
    w.get(t2)->pos = Vec4(0, 0, 0, 0);
    w.get(t2)->rotation = Rotor4::identity();
    tris.clear();
    slice_body_triangles(*w.get(t2), tilt, tris);
    CHECK(!tris.empty(), "tilted slice");
    // every sliced vertex lies on the hyperplane when unprojected
    bool on_plane = true;
    for (const auto& tr : tris)
        for (const auto& v : tr.p) on_plane &= near(tilt.signed_distance(tilt.unproject(v)), 0.0f, 1e-4f);
    CHECK(on_plane, "unprojected points lie on plane");
}

// ---------------------------------------------------------------------------
static void test_queries() {
    section("queries");
    World w;
    BodyId t = w.create_polytope(RegularSolid::EightCell, Vec4(0, 0, 0, 0));
    BodyId s = w.create_hypersphere(Vec4(5, 0, 0, 0), 0.5f);
    BodyId f = w.create_half_space(Vec4(0, -3, 0, 0), Vec4(0, 1, 0, 0));

    RayHit h = w.raycast(Vec4(-5, 0, 0, 0), Vec4(1, 0, 0, 0));
    CHECK(h.hit && h.body == t && near(h.t, 4.5f, 1e-3f) && near(h.normal.x, -1.0f, 1e-3f), "ray hits tesseract face");
    h = w.raycast(Vec4(1.5f, 0, 0, 0), Vec4(1, 0, 0, 0));
    CHECK(h.hit && h.body == s && near(h.t, 3.0f, 1e-3f), "ray hits sphere");
    h = w.raycast(Vec4(0, 5, 0, 0), Vec4(0, -1, 0, 0));
    CHECK(h.hit && h.body == t && near(h.t, 4.5f, 1e-3f), "ray down hits tesseract top");
    h = w.raycast(Vec4(3, 5, 0, 0), Vec4(0, -1, 0, 0));
    CHECK(h.hit && h.body == f && near(h.t, 8.0f, 1e-3f), "ray hits floor");
    h = w.raycast(Vec4(0, 0, 0, 5), Vec4(0, 0, 0, -1));
    CHECK(h.hit && h.body == t && near(h.t, 4.5f, 1e-3f), "ray along w hits tesseract");
    h = w.raycast(Vec4(0, 0, 5, 0.9f), Vec4(0, 0, -1, 0));
    CHECK(!h.hit || h.body != t, "ray passing by in w misses");

    auto ov = w.overlap_sphere(Vec4(0.9f, 0, 0, 0), 0.5f);
    bool has_t = false, has_s = false;
    for (BodyId id : ov) { has_t |= id == t; has_s |= id == s; }
    CHECK(has_t && !has_s, "overlap sphere");

    // mass bookkeeping: static <-> dynamic round trip keeps the original mass
    {
        Body* bt = w.get(t);
        CHECK(near(bt->mass, 1.0f, 1e-6f) && near(bt->moment_inertia_scalar, 1.0f / 6.0f, 1e-4f), "dynamic mass props");
        bt->set_stationary(true);
        CHECK(bt->mass == 0.0f && bt->moment_inertia_scalar == 0.0f, "static has zero mass");
        bt->set_stationary(false);
        CHECK(near(bt->mass, 1.0f, 1e-6f) && near(bt->moment_inertia_scalar, 1.0f / 6.0f, 1e-4f), "restored mass");
        bt->set_dynamic_mass(3.0f);
        CHECK(near(bt->mass, 3.0f, 1e-6f) && near(bt->moment_inertia_scalar, 0.5f, 1e-4f), "inertia scales with mass");
        bt->set_dynamic_mass(1.0f);
        CHECK(w.get(s)->collider.solid == RegularSolid::EightCell && bt->collider.solid == RegularSolid::EightCell, "solid tag");
        BodyId sx = w.create_polytope(RegularSolid::SixteenCell, Vec4(9, 9, 9, 9));
        CHECK(w.get(sx)->collider.solid == RegularSolid::SixteenCell, "solid tag 16-cell");
    }

    CHECK(w.remove_body(s) && !w.valid(s) && w.valid(t) && w.get(t) != nullptr, "remove body keeps others valid");
}

int main() {
    test_math();
    test_meshes();
    test_contacts();
    test_simulation();
    test_gjk();
    test_slice();
    test_queries();
    std::printf("\n%d checks passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
