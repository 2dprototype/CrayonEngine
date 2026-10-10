#!/usr/bin/env python3
"""
Generates the car body and the wheel for examples/games/car_game as binary glTF (.glb).
Pure python + numpy, no other dependencies.

Conventions (match Jolt vehicles / Crayon physics3d):
  * car:   forward = +Z, up = +Y, left = +X.  Origin = body center (center of mass).
  * wheel: axle along X, up along Y, centered at origin (symmetric, so it works on both sides).
Usage:  python3 tools/gen_car_models.py [output_dir]
"""
import json, math, struct, sys, os
import numpy as np

# ----------------------------------------------------------------------------
# tiny mesh builder
# ----------------------------------------------------------------------------
class Mesh:
    def __init__(self):
        self.prims = {}  # material name -> (positions, normals, indices)

    def _p(self, mat):
        return self.prims.setdefault(mat, ([], [], []))

    def tri(self, mat, a, b, c, flat=True):
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
        cx, cy, cz = center; hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        v = lambda x, y, z: (cx + x * hx, cy + y * hy, cz + z * hz)
        self.quad(mat, v(-1, -1, 1), v(1, -1, 1), v(1, 1, 1), v(-1, 1, 1))     # +z
        self.quad(mat, v(1, -1, -1), v(-1, -1, -1), v(-1, 1, -1), v(1, 1, -1)) # -z
        self.quad(mat, v(1, -1, 1), v(1, -1, -1), v(1, 1, -1), v(1, 1, 1))     # +x
        self.quad(mat, v(-1, -1, -1), v(-1, -1, 1), v(-1, 1, 1), v(-1, 1, -1))# -x
        self.quad(mat, v(-1, 1, 1), v(1, 1, 1), v(1, 1, -1), v(-1, 1, -1))     # +y
        self.quad(mat, v(-1, -1, -1), v(1, -1, -1), v(1, -1, 1), v(-1, -1, 1))# -y

    def loft(self, mat, sections, closed_ends=True):
        """sections: list of rings; each ring = list of (x,y,z) with equal count, ordered
        counter-clockwise when seen from +Z looking toward -Z (i.e. from the front)."""
        n = len(sections[0])
        for s in range(len(sections) - 1):
            A, B = sections[s], sections[s + 1]
            for i in range(n):
                j = (i + 1) % n
                self.quad(mat, A[i], A[j], B[j], B[i])
        if closed_ends:
            for ring, flip in ((sections[0], True), (sections[-1], False)):
                c = np.mean(ring, axis=0)
                for i in range(n):
                    j = (i + 1) % n
                    if flip: self.tri(mat, c, ring[j], ring[i])
                    else:    self.tri(mat, c, ring[i], ring[j])

    def cylinder_x(self, mat, cx, cy, cz, radius, x0, x1, seg=24, cap=True):
        for i in range(seg):
            a0 = 2 * math.pi * i / seg; a1 = 2 * math.pi * (i + 1) / seg
            y0, z0 = cy + radius * math.cos(a0), cz + radius * math.sin(a0)
            y1, z1 = cy + radius * math.cos(a1), cz + radius * math.sin(a1)
            self.quad(mat, (x0, y0, z0), (x0, y1, z1), (x1, y1, z1), (x1, y0, z0))
            if cap:
                self.tri(mat, (x1, cy, cz), (x1, y0, z0), (x1, y1, z1))
                self.tri(mat, (x0, cy, cz), (x0, y1, z1), (x0, y0, z0))

    def torus_x(self, mat, radius, tube, seg=32, side=12, squash=1.0):
        """tire: torus around X axis, tube squashed along X by 'squash' (tire width)."""
        def pt(u, v):
            r = radius + tube * math.cos(v)
            return (tube * math.sin(v) * squash, r * math.cos(u), r * math.sin(u))
        for i in range(seg):
            u0 = 2 * math.pi * i / seg; u1 = 2 * math.pi * (i + 1) / seg
            for j in range(side):
                v0 = 2 * math.pi * j / side; v1 = 2 * math.pi * (j + 1) / side
                self.quad(mat, pt(u0, v0), pt(u1, v0), pt(u1, v1), pt(u0, v1))

# ----------------------------------------------------------------------------
# glTF writer
# ----------------------------------------------------------------------------
def write_glb(path, mesh, materials, name, double_sided=("glass", "tire_line")):
    # thin decal-like surfaces are emitted double-sided so they never vanish to backface culling
    for mname in double_sided:
        if mname in mesh.prims:
            P, N, I = mesh.prims[mname]
            base = len(P)
            P.extend(list(P[:base])); N.extend([-n for n in N[:base]])
            for k in range(0, len(I), 3):
                I.extend([I[k] + base, I[k + 2] + base, I[k + 1] + base])
    bin_data = bytearray()
    buffer_views, accessors, primitives, mats, mat_index = [], [], [], [], {}
    def pad4(b):
        while len(b) % 4: b.append(0)
    for mname, (P, N, I) in mesh.prims.items():
        if not I: continue
        Pn = np.array(P, dtype=np.float32); Nn = np.array(N, dtype=np.float32); In = np.array(I, dtype=np.uint32)
        def add(arr, target):
            pad4(bin_data)
            off = len(bin_data); bin_data.extend(arr.tobytes())
            buffer_views.append({"buffer": 0, "byteOffset": off, "byteLength": arr.nbytes, "target": target})
            return len(buffer_views) - 1
        bv_p, bv_n, bv_i = add(Pn, 34962), add(Nn, 34962), add(In, 34963)
        accessors.append({"bufferView": bv_p, "componentType": 5126, "count": len(Pn), "type": "VEC3",
                          "min": Pn.min(axis=0).tolist(), "max": Pn.max(axis=0).tolist()})
        a_p = len(accessors) - 1
        accessors.append({"bufferView": bv_n, "componentType": 5126, "count": len(Nn), "type": "VEC3"}); a_n = len(accessors) - 1
        accessors.append({"bufferView": bv_i, "componentType": 5125, "count": len(In), "type": "SCALAR"}); a_i = len(accessors) - 1
        if mname not in mat_index:
            color, metal, rough = materials[mname]
            mats.append({"name": mname, "pbrMetallicRoughness": {"baseColorFactor": list(color), "metallicFactor": metal, "roughnessFactor": rough}})
            mat_index[mname] = len(mats) - 1
        primitives.append({"attributes": {"POSITION": a_p, "NORMAL": a_n}, "indices": a_i, "material": mat_index[mname], "mode": 4})
    pad4(bin_data)
    gltf = {"asset": {"version": "2.0", "generator": "crayon gen_car_models.py"},
            "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": name}],
            "meshes": [{"name": name, "primitives": primitives}], "materials": mats,
            "accessors": accessors, "bufferViews": buffer_views, "buffers": [{"byteLength": len(bin_data)}]}
    js = json.dumps(gltf, separators=(",", ":")).encode()
    while len(js) % 4: js += b" "
    total = 12 + 8 + len(js) + 8 + len(bin_data)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A)); f.write(js)
        f.write(struct.pack("<II", len(bin_data), 0x004E4942)); f.write(bytes(bin_data))
    tris = sum(len(v[2]) // 3 for v in mesh.prims.values())
    print(f"wrote {path}  ({tris} triangles, {os.path.getsize(path)} bytes)")

# ----------------------------------------------------------------------------
# the car body (sports coupe), forward = +Z, origin = chassis center
# ----------------------------------------------------------------------------
def ring(z, half_w, y_bot, y_shoulder, y_top, top_w, n_round=3):
    """cross-section ring (CCW seen from +Z): bottom -> right(-x) side -> top -> left(+x) side"""
    pts = [(half_w * 0.85, y_bot), (-half_w * 0.85, y_bot), (-half_w, y_bot + 0.08), (-half_w, y_shoulder),
           (-top_w, y_top), (top_w, y_top), (half_w, y_shoulder), (half_w, y_bot + 0.08)]
    return [(x, y, z) for x, y in pts]

def build_car():
    m = Mesh()
    # body shell lofted from sections (z, half_width, bottom, shoulder, top, top_width)
    # y is relative to chassis center; wheels attach at y=-0.25 -> body bottom ~ -0.30
    S = [
        (2.18, 0.70, -0.28, -0.12, -0.02, 0.50),   # nose tip
        (2.05, 0.82, -0.30, -0.02,  0.10, 0.62),
        (1.60, 0.90, -0.32,  0.10,  0.22, 0.74),   # hood
        (1.00, 0.92, -0.32,  0.12,  0.24, 0.78),
        (0.55, 0.92, -0.32,  0.13,  0.50, 0.66),   # windshield base
        (0.05, 0.92, -0.32,  0.13,  0.68, 0.58),   # roof front
        (-0.60, 0.92, -0.32, 0.13,  0.68, 0.58),   # roof rear
        (-1.10, 0.92, -0.32, 0.14,  0.46, 0.66),   # rear window base
        (-1.55, 0.92, -0.32, 0.16,  0.22, 0.76),   # trunk
        (-2.05, 0.88, -0.30, 0.14,  0.18, 0.74),
        (-2.20, 0.80, -0.28, 0.06,  0.10, 0.66),   # tail
    ]
    secs = [ring(*s) for s in S]
    # paint zones: whole shell is paint; glass drawn as separate inset quads on top
    m.loft("paint", secs)

    # windows: windshield, rear glass, side glass (slightly outside the shell surface)
    e = 0.004
    ws_b, ws_t = S[4], S[5]
    # windshield (sloped quad between sections 4 and 5)
    zb, zt = S[4][0], S[5][0]
    yb, yt = S[4][4], S[5][4]
    wb, wt = S[4][5], S[5][5]
    m.quad("glass", (-wb + 0.08, yb + 0.01, zb + 0.01), (wb - 0.08, yb + 0.01, zb + 0.01), (wt - 0.06, yt - 0.01, zt + 0.01), (-wt + 0.06, yt - 0.01, zt + 0.01))
    # rear glass
    zb, zt = S[6][0], S[7][0]
    yb, yt = S[6][4], S[7][4]
    wb, wt = S[6][5], S[7][5]
    m.quad("glass", (wb - 0.06, yb - 0.01, zb - 0.01), (-wb + 0.06, yb - 0.01, zb - 0.01), (-wt + 0.08, yt + 0.01, zt - 0.01), (wt - 0.08, yt + 0.01, zt - 0.01))
    # side windows (both sides)
    for sx in (1, -1):
        zf = (S[4][0] + S[5][0]) / 2; zr = (S[6][0] + S[7][0]) / 2
        # approximate cabin side surface: lerp between shoulder and roof edge
        def side_x(sec, t): return sx * (sec[1] + (sec[5] - sec[1]) * t)
        def side_y(sec, t): return sec[3] + (sec[4] - sec[3]) * t
        top_f = (side_x(S[5], 1.0) + sx * 0.005, S[5][4] - 0.03, S[5][0] + 0.02)
        top_r = (side_x(S[6], 1.0) + sx * 0.005, S[6][4] - 0.03, S[6][0] - 0.02)
        bot_f = (sx * (S[4][1] - 0.004), S[4][3] + 0.10, S[4][0] + 0.10)
        bot_r = (sx * (S[7][1] - 0.004), S[7][3] + 0.10, S[7][0] - 0.10)
        if sx > 0: m.quad("glass", bot_r, bot_f, top_f, top_r)
        else:      m.quad("glass", bot_f, bot_r, top_r, top_f)

    # stripes on hood + roof + trunk (racing stripe)
    def stripe(z0, y0, z1, y1, hw=0.12, lift=0.004):
        m.quad("stripe", (-hw, y0 + lift, z0), (hw, y0 + lift, z0), (hw, y1 + lift, z1), (-hw, y1 + lift, z1))
    stripe(2.05, 0.10, 1.60, 0.22); stripe(1.60, 0.22, 1.00, 0.24); stripe(1.00, 0.24, 0.57, 0.50)
    stripe(0.55, 0.50, 0.05, 0.68); stripe(0.05, 0.68, -0.60, 0.68); stripe(-0.60, 0.68, -1.10, 0.46)
    stripe(-1.10, 0.46, -1.55, 0.22); stripe(-1.55, 0.22, -2.05, 0.18)

    # headlights / taillights
    for sx in (1, -1):
        m.box("light", (sx * 0.58, 0.02, 2.14), (0.34, 0.10, 0.06))
        m.box("tail", (sx * 0.62, 0.09, -2.19), (0.40, 0.08, 0.05))
        m.box("trim", (sx * 0.62, -0.20, 2.17), (0.22, 0.06, 0.05))      # fog/intake
        # mirrors
        m.box("paint", (sx * 1.00, 0.30, 0.50), (0.14, 0.08, 0.10))
        m.box("trim", (sx * 0.95, 0.27, 0.50), (0.10, 0.03, 0.03))
    # bumpers / splitter / diffuser / grille
    m.box("trim", (0, -0.30, 2.14), (1.60, 0.07, 0.22))
    m.box("trim", (0, -0.30, -2.16), (1.55, 0.07, 0.22))
    m.box("trim", (0, -0.06, 2.20), (0.78, 0.12, 0.04))
    # exhaust pipes
    for sx in (1, -1):
        m.box("chrome", (sx * 0.46, -0.24, -2.24), (0.10, 0.10, 0.08))
    # spoiler
    m.box("paint", (0, 0.40, -2.06), (1.30, 0.04, 0.34))
    for sx in (1, -1):
        m.box("trim", (sx * 0.45, 0.28, -2.06), (0.05, 0.24, 0.10))
    # wheel arch trims (dark boxes behind the tires, visual only)
    for sx in (1, -1):
        for z in (1.25, -1.25):
            m.box("trim", (sx * 0.92, -0.26, z), (0.03, 0.12, 1.00))
    return m

CAR_MATS = {
    "paint":  ((0.85, 0.10, 0.10, 1.0), 0.45, 0.35),
    "glass":  ((0.10, 0.18, 0.26, 1.0), 0.0, 0.05),
    "stripe": ((0.96, 0.96, 0.96, 1.0), 0.0, 0.5),
    "light":  ((1.00, 0.95, 0.70, 1.0), 0.0, 0.1),
    "tail":   ((0.95, 0.05, 0.05, 1.0), 0.0, 0.1),
    "trim":   ((0.05, 0.05, 0.06, 1.0), 0.0, 0.8),
    "chrome": ((0.75, 0.77, 0.80, 1.0), 1.0, 0.2),
}

# ----------------------------------------------------------------------------
# the wheel: tire + rim + spokes + brake disc. Axle along X. Symmetric about x=0.
# ----------------------------------------------------------------------------
def build_wheel(radius=0.36, width=0.26):
    m = Mesh()
    tube = width * 0.5                       # half width of the tire cross-section
    rim_r = radius - tube * 0.95             # rim radius so that tire section fits
    tire_ring_r = radius - tube              # torus major radius
    squash = 1.0
    m.torus_x("tire", tire_ring_r, tube * 0.98, seg=40, side=14, squash=1.0)
    # sidewall lettering ring (light) on both sides
    for sx in (1, -1):
        m.cylinder_x("tire_line", 0, 0, 0, rim_r * 1.05, sx * (tube * 0.97), sx * (tube * 0.97 + 0.002), seg=40, cap=False)
    # rim barrel + lips
    m.cylinder_x("rim", 0, 0, 0, rim_r, -width * 0.42, width * 0.42, seg=32, cap=False)
    for sx in (1, -1):
        # rim face discs (outer faces) with 5 spokes cut out visually: draw spokes as wedges
        xf = sx * width * 0.40
        n_sp = 5
        for k in range(n_sp):
            a = 2 * math.pi * k / n_sp
            a0, a1 = a - 0.18, a + 0.18
            r_in, r_out = rim_r * 0.20, rim_r * 0.96
            p = lambda r, aa: (xf, r * math.cos(aa), r * math.sin(aa))
            if sx > 0: m.quad("rim", p(r_in, a0), p(r_out, a0 - 0.05), p(r_out, a1 + 0.05), p(r_in, a1))
            else:      m.quad("rim", p(r_in, a1), p(r_out, a1 + 0.05), p(r_out, a0 - 0.05), p(r_in, a0))
        # center cap
        m.cylinder_x("chrome", xf + sx * 0.002, 0, 0, rim_r * 0.20, xf, xf + sx * 0.03, seg=20, cap=True)
        # lug nuts
        for k in range(5):
            a = 2 * math.pi * (k + 0.5) / 5
            m.cylinder_x("chrome", 0, rim_r * 0.34 * math.cos(a), rim_r * 0.34 * math.sin(a), 0.02, xf, xf + sx * 0.02, seg=8, cap=True)
    # inner dark disc (brake disc) visible through spokes
    m.cylinder_x("trim", 0, 0, 0, rim_r * 0.80, -width * 0.30, width * 0.30, seg=28, cap=True)
    # one asymmetric mark on the TIRE tread so wheel spin is visible
    for k in range(0, 40, 5):
        a0 = 2 * math.pi * k / 40; a1 = 2 * math.pi * (k + 1) / 40
        rr = radius + 0.002
        pa = lambda aa, x: (x, rr * math.cos(aa), rr * math.sin(aa))
        m.quad("tire_line", pa(a0, -width * 0.12), pa(a0, width * 0.12), pa(a1, width * 0.12), pa(a1, -width * 0.12))
    return m

WHEEL_MATS = {
    "tire":      ((0.04, 0.04, 0.045, 1.0), 0.0, 0.95),
    "tire_line": ((0.30, 0.30, 0.32, 1.0), 0.0, 0.9),
    "rim":       ((0.80, 0.82, 0.86, 1.0), 0.9, 0.25),
    "chrome":    ((0.92, 0.92, 0.95, 1.0), 1.0, 0.15),
    "trim":      ((0.10, 0.10, 0.12, 1.0), 0.2, 0.6),
}

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "examples", "games", "car_game", "assets")
    os.makedirs(out, exist_ok=True)
    write_glb(os.path.join(out, "car_body.glb"), build_car(), CAR_MATS, "car_body")
    write_glb(os.path.join(out, "wheel.glb"), build_wheel(0.36, 0.26), WHEEL_MATS, "wheel")
    