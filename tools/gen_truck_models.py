#!/usr/bin/env python3
"""
Generates an articulated semi-truck: tractor, trailer, wheels and cargo crates.
Binary glTF (.glb). Pure python + numpy — no other dependencies.

Conventions (match Jolt vehicles / Crayon physics3d):
  * tractor / trailer: forward = +Z, up = +Y, left = +X.
  * Local origin of each = physics chassis center.
  * Wheel: axle along X, up along Y, centered at origin.
  * Cargo: 1 m cube centered at origin.

Usage:  python3 tools/gen_truck_models.py [output_dir]
"""
import json, math, os, struct, sys
import numpy as np


# ----------------------------------------------------------------------------
# tiny mesh builder
# ----------------------------------------------------------------------------
class Mesh:
    def __init__(self):
        self.prims = {}

    def _p(self, mat):
        return self.prims.setdefault(mat, ([], [], []))

    def tri(self, mat, a, b, c):
        P, N, I = self._p(mat)
        a, b, c = map(np.array, (a, b, c))
        n = np.cross(b - a, c - a)
        l = np.linalg.norm(n)
        n = n / l if l > 1e-12 else np.array([0, 1, 0])
        base = len(P)
        P.extend([a, b, c]); N.extend([n, n, n]); I.extend([base, base + 1, base + 2])

    def quad(self, mat, a, b, c, d):
        self.tri(mat, a, b, c); self.tri(mat, a, c, d)

    def box(self, mat, center, size):
        cx, cy, cz = center
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        v = lambda x, y, z: (cx + x * hx, cy + y * hy, cz + z * hz)
        self.quad(mat, v(-1, -1, 1), v(1, -1, 1), v(1, 1, 1), v(-1, 1, 1))
        self.quad(mat, v(1, -1, -1), v(-1, -1, -1), v(-1, 1, -1), v(1, 1, -1))
        self.quad(mat, v(1, -1, 1), v(1, -1, -1), v(1, 1, -1), v(1, 1, 1))
        self.quad(mat, v(-1, -1, -1), v(-1, -1, 1), v(-1, 1, 1), v(-1, 1, -1))
        self.quad(mat, v(-1, 1, 1), v(1, 1, 1), v(1, 1, -1), v(-1, 1, -1))
        self.quad(mat, v(-1, -1, -1), v(1, -1, -1), v(1, -1, 1), v(-1, -1, 1))

    def cylinder_x(self, mat, cx, cy, cz, radius, x0, x1, seg=24, cap=True):
        for i in range(seg):
            a0 = 2 * math.pi * i / seg
            a1 = 2 * math.pi * (i + 1) / seg
            y0, z0 = cy + radius * math.cos(a0), cz + radius * math.sin(a0)
            y1, z1 = cy + radius * math.cos(a1), cz + radius * math.sin(a1)
            self.quad(mat, (x0, y0, z0), (x0, y1, z1), (x1, y1, z1), (x1, y0, z0))
            if cap:
                self.tri(mat, (x1, cy, cz), (x1, y0, z0), (x1, y1, z1))
                self.tri(mat, (x0, cy, cz), (x0, y1, z1), (x0, y0, z0))

    def torus_x(self, mat, radius, tube, seg=32, side=12, squash=1.0):
        def pt(u, v):
            r = radius + tube * math.cos(v)
            return (tube * math.sin(v) * squash, r * math.cos(u), r * math.sin(u))
        for i in range(seg):
            u0 = 2 * math.pi * i / seg
            u1 = 2 * math.pi * (i + 1) / seg
            for j in range(side):
                v0 = 2 * math.pi * j / side
                v1 = 2 * math.pi * (j + 1) / side
                self.quad(mat, pt(u0, v0), pt(u1, v0), pt(u1, v1), pt(u0, v1))


# ----------------------------------------------------------------------------
# glTF writer  (fixed: mat_index is a dict)
# ----------------------------------------------------------------------------
def write_glb(path, mesh, materials, name, double_sided=("glass",)):
    for mname in double_sided:
        if mname in mesh.prims:
            P, N, I = mesh.prims[mname]
            base = len(P)
            P.extend(list(P[:base])); N.extend([-n for n in N[:base]])
            for k in range(0, len(I), 3):
                I.extend([I[k] + base, I[k + 2] + base, I[k + 1] + base])

    bin_data = bytearray()
    buffer_views, accessors, primitives, mats = [], [], [], []
    mat_index = {}                    # FIXED (was [], must be {})

    def pad4(b):
        while len(b) % 4:
            b.append(0)

    for mname, (P, N, I) in mesh.prims.items():
        if not I:
            continue
        Pn = np.array(P, dtype=np.float32)
        Nn = np.array(N, dtype=np.float32)
        In = np.array(I, dtype=np.uint32)

        def add(arr, target):
            pad4(bin_data)
            off = len(bin_data)
            bin_data.extend(arr.tobytes())
            buffer_views.append({"buffer": 0, "byteOffset": off,
                                 "byteLength": arr.nbytes, "target": target})
            return len(buffer_views) - 1

        bv_p, bv_n, bv_i = add(Pn, 34962), add(Nn, 34962), add(In, 34963)
        accessors.append({"bufferView": bv_p, "componentType": 5126, "count": len(Pn),
                          "type": "VEC3", "min": Pn.min(axis=0).tolist(),
                          "max": Pn.max(axis=0).tolist()})
        a_p = len(accessors) - 1
        accessors.append({"bufferView": bv_n, "componentType": 5126, "count": len(Nn), "type": "VEC3"})
        a_n = len(accessors) - 1
        accessors.append({"bufferView": bv_i, "componentType": 5125, "count": len(In), "type": "SCALAR"})
        a_i = len(accessors) - 1

        if mname not in mat_index:
            color, metal, rough = materials[mname]
            mats.append({"name": mname,
                         "pbrMetallicRoughness": {"baseColorFactor": list(color),
                                                  "metallicFactor": metal,
                                                  "roughnessFactor": rough}})
            mat_index[mname] = len(mats) - 1

        primitives.append({"attributes": {"POSITION": a_p, "NORMAL": a_n},
                           "indices": a_i, "material": mat_index[mname], "mode": 4})

    pad4(bin_data)
    gltf = {"asset": {"version": "2.0", "generator": "crayon gen_truck_models.py"},
            "scene": 0, "scenes": [{"nodes": [0]}],
            "nodes": [{"mesh": 0, "name": name}],
            "meshes": [{"name": name, "primitives": primitives}],
            "materials": mats, "accessors": accessors, "bufferViews": buffer_views,
            "buffers": [{"byteLength": len(bin_data)}]}

    js = json.dumps(gltf, separators=(",", ":")).encode()
    while len(js) % 4:
        js += b" "
    total = 12 + 8 + len(js) + 8 + len(bin_data)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A)); f.write(js)
        f.write(struct.pack("<II", len(bin_data), 0x004E4942)); f.write(bytes(bin_data))

    tris = sum(len(v[2]) // 3 for v in mesh.prims.values())
    print(f"wrote {path}  ({tris} triangles, {os.path.getsize(path)} bytes)")


# ============================================================================
# TRACTOR  (long-nose American semi cab)
#   Physics chassis half-extents: (1.2, 0.5, 2.8), origin = chassis centre.
#   Chassis box spans Y[-0.5,+0.5], Z[-2.8,+2.8] in LOCAL space.
#   Wheels: front axle z=+1.8, rear axle z=-1.8, both at y=-0.55, radius 0.55.
#   Fifth-wheel plate: local (0, +0.55, -2.5).
# ============================================================================
def build_tractor():
    m = Mesh()

    # --- frame rails -------------------------------------------------------
    for sx in (1, -1):
        m.box("chassis", (sx * 1.00, -0.35, 0), (0.30, 0.30, 5.60))
    # cross-members
    for z in (-2.0, -0.5, 1.0, 2.5):
        m.box("chassis", (0, -0.35, z), (2.20, 0.20, 0.15))

    # --- hood (front) ------------------------------------------------------
    m.box("paint", (0, 0.10, 2.00), (2.10, 0.75, 1.50))       # main hood
    m.box("paint", (0, 0.15, 2.80), (1.95, 0.60, 0.20))       # hood front lip
    m.box("trim",  (0, -0.05, 2.85), (1.80, 0.35, 0.08))       # grille frame
    for yy in (-0.15, 0.02, 0.19):
        m.box("chrome", (0, yy, 2.87), (1.75, 0.05, 0.04))     # grille slats
    # headlights
    for sx in (1, -1):
        m.box("light", (sx * 0.80, -0.05, 2.82), (0.35, 0.22, 0.05))
        m.box("light", (sx * 0.80, -0.25, 2.82), (0.20, 0.08, 0.05))
    # front bumper
    m.box("chrome", (0, -0.48, 2.85), (2.30, 0.22, 0.18))
    # cab steps
    for sx in (1, -1):
        m.box("chrome", (sx * 1.12, -0.35, 0.95), (0.24, 0.05, 0.28))

    # --- cab (middle) ------------------------------------------------------
    m.box("paint", (0, 1.05, 0.10), (2.35, 1.75, 1.65))
    # roof cap / fairing
    m.box("paint", (0, 2.05, 0.10), (2.25, 0.18, 1.60))
    # windshield (front of cab)
    m.box("glass", (0, 1.35, 0.95), (2.10, 0.85, 0.06))
    # side windows
    for sx in (1, -1):
        m.box("glass", (sx * 1.18, 1.25, 0.10), (0.03, 0.75, 1.20))
    # doors (thin decorative panels)
    for sx in (1, -1):
        m.box("trim", (sx * 1.18, 0.35, 0.10), (0.03, 0.30, 1.55))
        m.box("chrome", (sx * 1.19, 0.65, -0.30), (0.04, 0.05, 0.20))   # handle

    # --- sleeper -----------------------------------------------------------
    m.box("paint", (0, 1.20, -1.55), (2.35, 2.05, 1.90))
    m.box("paint", (0, 2.25, -1.55), (2.25, 0.18, 1.85))       # sleeper roof
    for sx in (1, -1):
        m.box("glass", (sx * 1.18, 1.60, -1.55), (0.03, 0.55, 0.90))

    # --- mirrors -----------------------------------------------------------
    for sx in (1, -1):
        m.box("chrome", (sx * 1.28, 1.70, 0.75), (0.05, 0.05, 0.35))
        m.box("trim",   (sx * 1.40, 1.55, 0.75), (0.08, 0.55, 0.18))

    # --- exhaust stacks ----------------------------------------------------
    for sx in (1, -1):
        m.box("chrome", (sx * 1.22, 1.55, -0.85), (0.16, 2.60, 0.16))
        m.box("chrome", (sx * 1.22, 2.85, -0.85), (0.20, 0.10, 0.20))

    # --- fuel tanks + battery boxes ---------------------------------------
    for sx in (1, -1):
        m.box("chrome", (sx * 1.10, -0.10, 0.20), (0.40, 0.55, 1.20))   # fuel
        m.box("chassis", (sx * 1.10, -0.10, -1.30), (0.40, 0.50, 0.80)) # toolbox

    # --- FIFTH-WHEEL HITCH (visible plate + kingpin socket) ---------------
    #   local Y = +0.55 (0.05 above chassis top of 0.5), Z = -2.5
    m.box("chrome", (0, 0.55, -2.50), (1.60, 0.10, 1.40))       # plate
    m.box("trim",   (0, 0.60, -2.50), (0.30, 0.15, 0.30))       # socket ring
    m.box("chrome", (0, 0.68, -2.50), (0.14, 0.12, 0.14))       # pin hole
    # support legs under the plate
    for sx in (1, -1):
        m.box("chrome", (sx * 0.55, 0.20, -2.50), (0.10, 0.60, 0.10))

    # --- mud flaps ---------------------------------------------------------
    for sx in (1, -1):
        m.box("trim", (sx * 1.05, -0.85, -2.70), (0.50, 0.40, 0.04))

    return m


TRACTOR_MATS = {
    "paint":   ((0.72, 0.10, 0.12, 1.0), 0.35, 0.35),   # red
    "chassis": ((0.07, 0.07, 0.08, 1.0), 0.20, 0.80),
    "trim":    ((0.05, 0.05, 0.06, 1.0), 0.05, 0.85),
    "chrome":  ((0.80, 0.82, 0.85, 1.0), 0.95, 0.22),
    "light":   ((1.00, 0.95, 0.72, 1.0), 0.00, 0.10),
    "glass":   ((0.09, 0.16, 0.24, 1.0), 0.00, 0.05),
}


# ============================================================================
# TRAILER  (long 13.6 m dry van)
#   Physics chassis half-extents: (1.25, 1.8, 5.0), origin = chassis centre.
#   Chassis box spans Y[-1.8,+1.8], Z[-5.0,+5.0] in LOCAL space.
#   Rear wheels: two axles at z=-3.5, z=-4.5, y=-1.85, radius 0.55.
#   Landing gear wheels: z=+3.5, y=-1.55, radius 0.55 (they sit 0.3 m above
#       the ground when the trailer is level, so they only touch down when
#       the trailer is dropped).
#   Kingpin: local (0, -0.80, +5.0) -- aligns with the tractor's fifth wheel.
# ============================================================================
def build_trailer():
    m = Mesh()

    # --- main cargo box ----------------------------------------------------
    m.box("body", (0, 0, 0), (2.50, 3.55, 10.0))                  # outer shell

    # --- vertical ribs (visual corrugation on both sides) -----------------
    for sx in (1, -1):
        for z in range(-4, 5):
            m.box("body_rib", (sx * 1.255, 0, z * 1.05), (0.02, 3.50, 0.05))

    # --- roof cap ----------------------------------------------------------
    m.box("trim", (0, 1.83, 0), (2.50, 0.06, 10.0))

    # --- under-frame -------------------------------------------------------
    m.box("chassis", (0, -1.85, 0), (2.40, 0.20, 10.0))

    # --- refrigeration unit (front top) -----------------------------------
    m.box("trim", (0, 1.30, 5.20), (1.60, 1.00, 0.30))
    for yy in (0.95, 1.15, 1.35, 1.55):
        m.box("chrome", (0, yy, 5.36), (1.50, 0.05, 0.03))

    # --- rear doors --------------------------------------------------------
    m.box("door",   (0, 0, -5.05), (2.40, 3.50, 0.06))
    m.box("trim",   (0, 0, -5.09), (0.04, 3.50, 0.03))            # door split
    for sx in (1, -1):
        for yy in (-1.40, -0.70, 0.00, 0.70, 1.40):
            m.box("chrome", (sx * 1.18, yy, -5.10), (0.05, 0.05, 0.05))    # hinges
        m.box("chrome", (sx * 0.20, -0.30, -5.12), (0.05, 0.30, 0.05))     # handles

    # --- rear bumper + lights ---------------------------------------------
    m.box("chrome", (0, -1.50, -5.30), (2.50, 0.20, 0.15))
    for sx in (1, -1):
        m.box("tail", (sx * 1.05, -1.30, -5.28), (0.30, 0.20, 0.05))
        m.box("tail", (sx * 1.05,  1.60, -5.10), (0.10, 0.10, 0.03))

    # --- side skirt / underrun guard --------------------------------------
    for sx in (1, -1):
        m.box("trim", (sx * 1.20, -1.55, 0), (0.06, 0.40, 9.80))

    # --- landing gear (visual, folded up) ---------------------------------
    for sx in (1, -1):
        m.box("chrome", (sx * 0.85, -1.20, 3.50), (0.16, 0.80, 0.16))
        m.box("chrome", (sx * 0.85, -1.55, 3.50), (0.28, 0.10, 0.28))  # foot pad

    # --- KINGPIN (visual) --------------------------------------------------
    #   local Y = -0.80, Z = +5.0 (matches physics hinge point)
    m.box("chrome", (0, -0.80, 5.00), (0.20, 0.30, 0.20))
    m.box("trim",   (0, -0.95, 5.00), (0.28, 0.06, 0.28))          # bearing plate
    # support plates from the chassis down to the kingpin
    m.box("chassis", (0, -1.50, 5.00), (1.20, 1.20, 0.30))

    return m


TRAILER_MATS = {
    "body":     ((0.90, 0.90, 0.88, 1.0), 0.10, 0.55),   # white box
    "body_rib": ((0.80, 0.80, 0.78, 1.0), 0.10, 0.60),   # slightly darker ribs
    "door":     ((0.85, 0.85, 0.83, 1.0), 0.10, 0.60),
    "chassis":  ((0.07, 0.07, 0.08, 1.0), 0.20, 0.80),
    "trim":     ((0.05, 0.05, 0.06, 1.0), 0.05, 0.85),
    "chrome":   ((0.80, 0.82, 0.85, 1.0), 0.95, 0.22),
    "tail":     ((0.95, 0.06, 0.06, 1.0), 0.00, 0.10),
}


# ============================================================================
# Wheel (same shape as the standalone truck wheel)
# ============================================================================
def build_truck_wheel(radius=0.55, width=0.40):
    m = Mesh()
    tube = width * 0.5
    rim_r = radius - tube * 0.92
    tire_ring_r = radius - tube

    m.torus_x("tire", tire_ring_r, tube * 0.98, seg=48, side=16)
    for sx in (1, -1):
        m.cylinder_x("tire_line", 0, 0, 0, rim_r * 1.02,
                     sx * (tube * 0.97), sx * (tube * 0.97 + 0.002), seg=48, cap=False)
    m.cylinder_x("rim", 0, 0, 0, rim_r, -width * 0.40, width * 0.40, seg=36, cap=False)

    for sx in (1, -1):
        xf = sx * width * 0.38
        n_sp = 6
        for k in range(n_sp):
            a = 2 * math.pi * k / n_sp
            a0, a1 = a - 0.20, a + 0.20
            r_in, r_out = rim_r * 0.24, rim_r * 0.94
            p = lambda r, aa: (xf, r * math.cos(aa), r * math.sin(aa))
            if sx > 0:
                m.quad("rim", p(r_in, a0), p(r_out, a0 - 0.06), p(r_out, a1 + 0.06), p(r_in, a1))
            else:
                m.quad("rim", p(r_in, a1), p(r_out, a1 + 0.06), p(r_out, a0 - 0.06), p(r_in, a0))
        m.cylinder_x("chrome", xf + sx * 0.002, 0, 0, rim_r * 0.24,
                     xf, xf + sx * 0.04, seg=20, cap=True)
        for k in range(8):
            a = 2 * math.pi * (k + 0.5) / 8
            m.cylinder_x("chrome", 0,
                         rim_r * 0.32 * math.cos(a), rim_r * 0.32 * math.sin(a),
                         0.022, xf, xf + sx * 0.02, seg=6, cap=True)

    m.cylinder_x("trim", 0, 0, 0, rim_r * 0.78, -width * 0.28, width * 0.28, seg=28, cap=True)

    for k in range(0, 48, 4):
        a0 = 2 * math.pi * k / 48
        a1 = 2 * math.pi * (k + 2) / 48
        rr = radius + 0.003
        pa = lambda aa, x: (x, rr * math.cos(aa), rr * math.sin(aa))
        m.quad("tire_line",
               pa(a0, -width * 0.14), pa(a0, width * 0.14),
               pa(a1, width * 0.14), pa(a1, -width * 0.14))
    return m


WHEEL_MATS = {
    "tire":      ((0.035, 0.035, 0.04, 1.0), 0.0, 0.95),
    "tire_line": ((0.22, 0.22, 0.24, 1.0), 0.0, 0.90),
    "rim":       ((0.72, 0.74, 0.78, 1.0), 0.75, 0.35),
    "chrome":    ((0.88, 0.90, 0.92, 1.0), 0.95, 0.18),
    "trim":      ((0.09, 0.09, 0.11, 1.0), 0.20, 0.65),
}


# ============================================================================
# Cargo crate (unchanged from earlier script)
# ============================================================================
def build_cargo(size=1.0):
    m = Mesh()
    hs = size * 0.5
    m.box("wood", (0, 0, 0), (size, size, size))
    e = 0.008
    for sx in (1, -1):
        for z in (-0.28, 0.0, 0.28):
            m.box("wood_dark", (sx * (hs + e), 0, z), (e * 2, size + 0.005, 0.035))
    for sz in (1, -1):
        for x in (-0.28, 0.0, 0.28):
            m.box("wood_dark", (x, 0, sz * (hs + e)), (0.035, size + 0.005, e * 2))
    for yy in (-0.30, 0.30):
        m.box("metal", (0, yy, 0), (size + 0.02, 0.06, size + 0.02))
    for sx in (1, -1):
        for sz in (1, -1):
            m.box("metal", (sx * hs, 0, sz * hs), (0.07, size + 0.02, 0.07))
    for sx in (1, -1):
        m.box("trim", (sx * (hs + 0.005), 0.0, 0), (0.04, 0.14, 0.28))
        m.box("trim", (sx * (hs + 0.010), 0.0, 0), (0.04, 0.06, 0.24))
    m.box("label", (0, hs + 0.005, 0.0), (0.55, 0.02, 0.30))
    return m


CARGO_MATS = {
    "wood":      ((0.62, 0.42, 0.24, 1.0), 0.0, 0.85),
    "wood_dark": ((0.38, 0.24, 0.13, 1.0), 0.0, 0.90),
    "metal":     ((0.45, 0.46, 0.50, 1.0), 0.85, 0.35),
    "trim":      ((0.10, 0.10, 0.11, 1.0), 0.10, 0.75),
    "label":     ((0.95, 0.92, 0.78, 1.0), 0.0, 0.55),
}


# ============================================================================
# main
# ============================================================================
if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(__file__), "..", "examples", "games", "truck_game", "assets"
    )
    os.makedirs(out, exist_ok=True)

    write_glb(os.path.join(out, "tractor.glb"),     build_tractor(),                 TRACTOR_MATS,     "tractor")
    write_glb(os.path.join(out, "trailer.glb"),     build_trailer(),                 TRAILER_MATS,     "trailer")
    write_glb(os.path.join(out, "truck_wheel.glb"), build_truck_wheel(0.55, 0.40),   WHEEL_MATS,       "truck_wheel")
    write_glb(os.path.join(out, "cargo_crate.glb"), build_cargo(1.0),                CARGO_MATS,       "cargo_crate")

    print("\nDone.")
    print("Expected names: tractor.glb  trailer.glb  truck_wheel.glb  cargo_crate.glb")