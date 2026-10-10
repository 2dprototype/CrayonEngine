# hv4d — 4D rigid-body physics (C++17 library)

`hv4d` is a self-contained C++ re-creation of the **physics engine from _hypervis_**
(a 4D renderer + physics sandbox written in Rust by Tyler Zhang). It has **no dependencies**
— no renderer, no windowing, no third-party code — just the standard library. Crayon Engine
consumes it from `src/physics/physics4d_system.*` and `src/scripting/bind_physics4d.cpp`; nothing in
this folder knows about Crayon.

## What's in the box

| Header | What it gives you |
|--------|-------------------|
| `hv4d/math.hpp` | `Vec4`, `Bivec4`, `Trivec4`, `Quadvec4`, `Rotor4`, `Mat4`; geometric-algebra products, rotor integration + re-normalisation, 4D "cross product", orthonormal basis |
| `hv4d/todd_coxeter.hpp` | Todd–Coxeter coset enumeration over Coxeter groups |
| `hv4d/mesh.hpp` | `Mesh` (full vertex/edge/face/cell topology) generated from a Schläfli symbol `{p,q,r}` — all six regular convex 4-polytopes; closest-point queries; tetrahedral decomposition; hypervolume + inertia; `ClipMesh`; geodesic subdivision |
| `hv4d/physics.hpp` | `Body`, `Collider` (half-space / hypersphere / convex polytope), SAT + clipping contact generation, sequential-impulse solver with 3D-tangent-space friction, GJK + EPA, `World` (events, raycast, overlap queries) |
| `hv4d/slice.hpp` | CPU hyperplane slicing → 3D triangles, an exact wireframe of the cross-section, sphere / half-space slices |
| `hv4d/hv4d.hpp` | umbrella header |

## Quick start

```cpp
#include <hv4d/hv4d.hpp>
using namespace hv4d;

World world;                                   // gravity (0,-9.8,0,0) by default
world.create_arena(6.0f);                      // floor + walls on x, z, w
BodyId box  = world.create_polytope(RegularSolid::EightCell, Vec4(0, 3, 0, 0.3f));
BodyId ball = world.create_hypersphere(Vec4(2, 1, 0, 0), 0.5f);
world.get(ball)->vel.linear = Vec4(-3, 2, 0, 0);

for (int i = 0; i < 600; ++i) world.update(1.0f / 60.0f);

// look at the 3D cross-section at w = 0
SlicePlane slice = SlicePlane::at_w(0.0f);
std::vector<SliceSegment> wire;
slice_body_wireframe(*world.get(box), slice, wire);   // each segment: two Vec3 points
```

Rotations are `Rotor4`s (quaternions do not generalise to 4D). Angular velocity is a `Bivec4` in
the body frame, planes ordered `xy, xz, xw, yz, yw, zw`.

## Build

As part of Crayon, the root `CMakeLists.txt` does `add_subdirectory(vendor/hypervis4d)` and links the
`hypervis4d` target. Standalone (with the self-tests):

```bash
cmake -S vendor/hypervis4d -B build_hv4d -DHV4D_BUILD_TESTS=ON
cmake --build build_hv4d
ctest --test-dir build_hv4d --output-on-failure
```

or without CMake: `g++ -std=c++17 -O2 -Iinclude tests/test_hv4d.cpp src/*.cpp -o test_hv4d && ./test_hv4d`.

The test suite (3,000+ checks) covers: rotor drift and angular-velocity convention, topology counts / Euler
characteristic / hypervolume / convexity of all six polytopes, contact-normal orientation, resting stability of
every polytope, stacking, momentum transfer, w-separation, enter/exit/trigger events, determinism, GJK/EPA
against known depths and against SAT, slice geometry (area, wireframe), raycasts and overlap queries.

## Performance (single thread, `-O3`)

Mixed tesseract / 16-cell / hypersphere piles, steady state: **~0.7 ms/step at 10 bodies, ~2.4 ms at 50,
~8 ms at 100** (≈1 touching pair per body). The broadphase is a plain O(n²) loop with a bounding-hypersphere
early-out, so cost grows quadratically; past a few hundred bodies you would want a spatial grid / sweep-and-prune.

## How this differs from upstream *hypervis*

**Not ported:** the wgpu renderer, window/input handling and the demo scenes in `main.rs` — Crayon's own
renderer draws the debug view instead. (Upstream's GPU compute-shader slicer is replaced by a CPU one.)

**Bugs found in the Rust original and fixed here**

1. `Mesh::closest_point_to` — the edge projection used `(a - p)·ab` (inverted sign), snapping to the wrong
   endpoint for edge-region queries. This affected sphere-vs-polytope contacts. *(Found by the tests.)*
2. The separating-axis cache initialised its projection span to `(-inf, +inf)`, so the cache never rejected
   anything; it now actually short-circuits frames.
3. `Vec4::wedge_bv` had a typo in its `yzw` term (unused by the physics).
4. The EPA prototype used un-normalised facet normals; GJK did not reduce its simplex. Both are rewritten
   (GJK/EPA are *not* wired into upstream's world — here they power the `testOverlap` query).
5. Hypersphere–hypersphere contact points sat at `a.pos + depth·n`; they are now the middle of the overlap.
   A hypersphere whose centre is inside a polytope is now pushed out instead of being ignored.
6. Rotational inertia is computed from the actual shape (`Mesh::inertia_per_mass`, `m R²/3` for a hypersphere)
   instead of a hard-coded `mass / 6`. (For a tesseract this is exactly the old value.)

**Added:** configurable gravity; per-body materials (friction combined as `√(fa·fb)`), damping, gravity scale,
forces and torques; sensors with enter/exit events; world raycast and overlap queries; mass/static bookkeeping;
general (tilted) slicing hyperplanes; exact slice wireframes; `Mesh::scaled`; deterministic id-based bodies.

## Known limitations

* O(n²) broadphase, single threaded, no sleeping / island solving.
* Inertia is a scalar (isotropic) — exact for tesseracts and hyperspheres, an approximation for the others
  (same simplification as upstream).
* No joints / constraints, no continuous collision detection (fast thin objects can tunnel).
* Edge–face contacts produce a single contact point (as upstream), so some resting configurations are slightly
  less stable than vertex–cell ones.
* `float` precision throughout.
