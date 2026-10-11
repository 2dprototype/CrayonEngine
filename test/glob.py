# c17_scalable.py — Low-poly C-17 Globemaster III (fully scalable)
import math, os, random

try:
    from PIL import Image, ImageDraw, ImageFilter
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

OUT_OBJ   = "top_c17.obj"
OUT_MTL   = "top_c17.mtl"
OUT_PROPS = "top_c17.properties"
TEX_DIR   = "textures"
TEX_HULL  = "top_c17_0.png"
TEX_WING  = "top_c17_1.png"
TEX_DARK  = "top_c17_2.png"

# =====================================================================
#  MASTER SCALE
#  SCALE = 1.0  ->  1 unit = 0.4 m
#  SCALE = 0.08 ->  small model (~10 units long)
# =====================================================================
SCALE = 0.08

# =====================================================================
#  ACCURATE C-17 PROPORTIONS
# =====================================================================
FUSE_LEN     = 132.6 * SCALE
FUSE_R       = 8.575 * SCALE
N_FUSE_SEGS  = 16
N_FUSE_RINGS = 24

WING_SPAN     = 64.675 * SCALE
WING_SWEEP    = 25.0
WING_ANHEDRAL = 3.5
WING_ROOT_CH  = 28.5 * SCALE
WING_TIP_CH   = 7.0 * SCALE
WING_T_ROOT   = 3.6 * SCALE
WING_T_TIP    = 1.2 * SCALE

ENGINE_R      = 2.8 * SCALE
ENGINE_L      = 10.5 * SCALE
ENGINE_X_IN   = 22.0 * SCALE
ENGINE_X_OUT  = 44.0 * SCALE
ENGINE_DROP   = 3.2 * SCALE

FIN_H         = 26.0 * SCALE
FIN_CH_BASE   = 20.0 * SCALE
FIN_CH_TOP    = 12.0 * SCALE
FIN_SWEEP     = 28.0
HSTAB_SPAN    = 24.75 * SCALE
HSTAB_CH_ROOT = 12.0 * SCALE
HSTAB_CH_TIP  = 6.0 * SCALE

WINGLET_H     = 7.5 * SCALE
WINGLET_CH    = 5.0 * SCALE

LIGHT_DIR = (0.42, 0.72, 0.55)


def norm3(v):
    L = math.sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2])
    return (v[0]/L, v[1]/L, v[2]/L) if L > 1e-9 else (0.0, 1.0, 0.0)

def dot(a, b):
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]

LD = norm3(LIGHT_DIR)
def shade(n, amb=0.48, dif=0.62):
    s = amb + dif * max(0.0, dot(n, LD))
    return (s, s, s)

verts = []
uvs   = []
faces_hull = []
faces_wing = []
faces_dark = []

def add_v(x, y, z, color_rgb=(1.0, 1.0, 1.0)):
    r, g, b = color_rgb
    verts.append((x, y, z, r, g, b))
    return len(verts)

def add_vt(u, v):
    uvs.append((u, v))
    return len(uvs)

def add_quad(v1, v2, v3, v4, uv1, uv2, uv3, uv4, target_list):
    target_list.append(((v1, uv1), (v2, uv2), (v3, uv3), (v4, uv4)))

def add_tri(v1, v2, v3, uv1, uv2, uv3, target_list):
    target_list.append(((v1, uv1), (v2, uv2), (v3, uv3)))


# =====================================================================
#  1. FUSELAGE & DORSAL WING FAIRING
# =====================================================================
def fuse_profile(t):
    t = max(0.0, min(1.0, t))
    if t < 0.15:
        u = t / 0.15
        r = FUSE_R * math.pow(math.sin(u * math.pi * 0.5), 0.7)
        zoff = -0.6 * SCALE * math.pow(1.0 - u, 2)
        dorsal = 0.0
    elif t < 0.68:
        r = FUSE_R
        zoff = 0.0
        if 0.32 <= t <= 0.54:
            du = math.sin((t - 0.32) / 0.22 * math.pi)
            dorsal = du * 1.8 * SCALE
        else:
            dorsal = 0.0
    else:
        u = (t - 0.68) / 0.32
        r = FUSE_R * (1.0 - 0.85 * math.pow(u, 0.85))
        zoff = math.pow(u, 1.8) * 13.5 * SCALE
        dorsal = 0.0
    return r, zoff, dorsal

fuse_rings = []
fuse_uv_rows = []

for i in range(N_FUSE_RINGS + 1):
    t = i / N_FUSE_RINGS
    cy_base = FUSE_LEN * 0.5 - t * FUSE_LEN
    r, zoff, dorsal = fuse_profile(t)

    row_v = []
    row_vt = []
    for k in range(N_FUSE_SEGS):
        a = 2.0 * math.pi * k / N_FUSE_SEGS
        ca, sa = math.cos(a), math.sin(a)

        cx = ca * r
        cz = sa * r + zoff
        if sa > 0.0:
            cz += dorsal * math.pow(sa, 1.2)

        nrm = norm3((ca, 0.0, sa))
        v_idx = add_v(cx, cy_base, cz, shade(nrm))
        vt_idx = add_vt(k / N_FUSE_SEGS, t)

        row_v.append(v_idx)
        row_vt.append(vt_idx)

    fuse_rings.append(row_v)
    fuse_uv_rows.append(row_vt)

# Nose cap
nose_y = FUSE_LEN * 0.5
nose_v = add_v(0.0, nose_y, -0.6 * SCALE, shade((0.0, 1.0, 0.0)))
nose_vt = add_vt(0.5, 0.0)
for k in range(N_FUSE_SEGS):
    kn = (k + 1) % N_FUSE_SEGS
    add_tri(nose_v, fuse_rings[0][kn], fuse_rings[0][k],
            nose_vt, fuse_uv_rows[0][kn], fuse_uv_rows[0][k], faces_hull)

# Tail cap
tail_y = -FUSE_LEN * 0.5
_, tail_zoff, _ = fuse_profile(1.0)
tail_v = add_v(0.0, tail_y, tail_zoff, shade((0.0, -1.0, 0.0)))
tail_vt = add_vt(0.5, 1.0)
last_r = fuse_rings[-1]
last_vt = fuse_uv_rows[-1]
for k in range(N_FUSE_SEGS):
    kn = (k + 1) % N_FUSE_SEGS
    add_tri(tail_v, last_r[k], last_r[kn],
            tail_vt, last_vt[k], last_vt[kn], faces_hull)

# Fuselage quad strips
for i in range(N_FUSE_RINGS):
    for k in range(N_FUSE_SEGS):
        kn = (k + 1) % N_FUSE_SEGS
        v1, v2 = fuse_rings[i][k], fuse_rings[i+1][k]
        v3, v4 = fuse_rings[i+1][kn], fuse_rings[i][kn]
        t1, t2 = fuse_uv_rows[i][k], fuse_uv_rows[i+1][k]
        t3, t4 = fuse_uv_rows[i+1][kn], fuse_uv_rows[i][kn]
        add_quad(v1, v2, v3, v4, t1, t2, t3, t4, faces_hull)


# =====================================================================
#  2. HIGH WING & WINGLETS
# =====================================================================
def generate_airfoil_ring(x_pos, y_le, chord, z_mid, thick):
    y_te = y_le - chord
    pts = [
        (y_le, z_mid),
        (y_le - chord * 0.25, z_mid + thick * 0.50),
        (y_le - chord * 0.65, z_mid + thick * 0.40),
        (y_te, z_mid),
        (y_le - chord * 0.65, z_mid - thick * 0.35),
        (y_le - chord * 0.25, z_mid - thick * 0.45),
    ]
    ring_v = []
    ring_vt = []
    sweep_r = math.radians(WING_SWEEP)

    for idx, (py, pz) in enumerate(pts):
        if idx in (1, 2):
            nrm = (0.0, 0.0, 1.0)
        elif idx in (4, 5):
            nrm = (0.0, 0.0, -1.0)
        elif idx == 0:
            nrm = norm3((math.sin(sweep_r), math.cos(sweep_r), 0.0))
        else:
            nrm = (-math.sin(sweep_r), -math.cos(sweep_r), 0.0)

        v_idx = add_v(x_pos, py, pz, shade(nrm))
        u_val = abs(x_pos) / WING_SPAN
        v_val = (y_le - py) / max(0.1 * SCALE, chord)
        vt_idx = add_vt(u_val, v_val)
        ring_v.append(v_idx)
        ring_vt.append(vt_idx)
    return ring_v, ring_vt


def build_full_wing():
    tan_sweep = math.tan(math.radians(WING_SWEEP))
    tan_anh   = math.tan(math.radians(WING_ANHEDRAL))

    stations = [0.0, 8.0 * SCALE, ENGINE_X_IN, ENGINE_X_OUT, WING_SPAN]

    for side in (-1, +1):
        prev_v = None
        prev_vt = None

        for st_x in stations:
            x_val = side * st_x
            frac  = st_x / WING_SPAN

            chord = WING_ROOT_CH + (WING_TIP_CH - WING_ROOT_CH) * frac
            thick = WING_T_ROOT + (WING_T_TIP - WING_T_ROOT) * frac

            y_le = (WING_ROOT_CH * 0.45) - (st_x * tan_sweep)
            z_mid = (FUSE_R + 1.2 * SCALE) - (st_x * tan_anh)

            ring_v, ring_vt = generate_airfoil_ring(x_val, y_le, chord, z_mid, thick)

            if prev_v is not None:
                for k in range(6):
                    kn = (k + 1) % 6
                    if side > 0:
                        v1, v2, v3, v4 = prev_v[k], prev_v[kn], ring_v[kn], ring_v[k]
                        t1, t2, t3, t4 = prev_vt[k], prev_vt[kn], ring_vt[kn], ring_vt[k]
                    else:
                        v1, v2, v3, v4 = ring_v[k], ring_v[kn], prev_v[kn], prev_v[k]
                        t1, t2, t3, t4 = ring_vt[k], ring_vt[kn], prev_vt[kn], prev_vt[k]
                    add_quad(v1, v2, v3, v4, t1, t2, t3, t4, faces_wing)

            prev_v = ring_v
            prev_vt = ring_vt

        tip_x    = side * WING_SPAN
        tip_y_le = (WING_ROOT_CH * 0.45) - (WING_SPAN * tan_sweep)
        tip_z    = (FUSE_R + 1.2 * SCALE) - (WING_SPAN * tan_anh)

        wl_v = []
        wl_vt = []
        wl_pts = [
            (tip_y_le,                        tip_z),
            (tip_y_le - WING_TIP_CH * 0.4,    tip_z + WINGLET_H * 0.5),
            (tip_y_le - WING_TIP_CH * 0.9,    tip_z + WINGLET_H),
            (tip_y_le - WING_TIP_CH * 1.1,    tip_z + WINGLET_H * 0.9),
            (tip_y_le - WING_TIP_CH * 0.7,    tip_z + WINGLET_H * 0.4),
        ]

        for py, pz in wl_pts:
            nrm = (side * 0.8, 0.0, 0.6)
            v_i = add_v(tip_x + side * 0.8 * SCALE, py, pz, shade(nrm))
            vt_i = add_vt(0.95, 0.5)
            wl_v.append(v_i)
            wl_vt.append(vt_i)

        for k in range(1, len(wl_v) - 2):
            if side > 0:
                add_tri(wl_v[0], wl_v[k], wl_v[k+1],
                        wl_vt[0], wl_vt[k], wl_vt[k+1], faces_wing)
            else:
                add_tri(wl_v[0], wl_v[k+1], wl_v[k],
                        wl_vt[0], wl_vt[k+1], wl_vt[k], faces_wing)

build_full_wing()


# =====================================================================
#  3. ENGINES & PYLONS
# =====================================================================
def build_engine_and_pylon(side, x_span):
    sx = side * x_span
    tan_sweep = math.tan(math.radians(WING_SWEEP))
    tan_anh   = math.tan(math.radians(WING_ANHEDRAL))

    wing_le_y  = (WING_ROOT_CH * 0.45) - (x_span * tan_sweep)
    wing_z     = (FUSE_R + 1.2 * SCALE) - (x_span * tan_anh)
    wing_thick = WING_T_ROOT + (WING_T_TIP - WING_T_ROOT) * (x_span / WING_SPAN)
    wing_bot_z = wing_z - wing_thick * 0.4

    eng_y = wing_le_y + 1.8 * SCALE
    eng_z = wing_bot_z - ENGINE_DROP

    p_top_z   = wing_bot_z + 0.3 * SCALE
    p_bot_z   = eng_z + ENGINE_R - 0.2 * SCALE
    p_y_front = eng_y + 1.5 * SCALE
    p_y_back  = wing_le_y - 2.5 * SCALE
    pw        = 0.35 * SCALE

    p_pts = [
        (sx - pw, p_y_front, p_top_z), (sx + pw, p_y_front, p_top_z),
        (sx + pw, p_y_back,  p_top_z), (sx - pw, p_y_back,  p_top_z),
        (sx - pw, p_y_front, p_bot_z), (sx + pw, p_y_front, p_bot_z),
        (sx + pw, p_y_back,  p_bot_z), (sx - pw, p_y_back,  p_bot_z),
    ]
    p_idx = [add_v(px, py, pz, shade((side, 0.2, 0.0))) for px, py, pz in p_pts]
    p_uv  = [add_vt(0.5, 0.5) for _ in range(8)]

    add_quad(p_idx[0], p_idx[4], p_idx[5], p_idx[1], p_uv[0], p_uv[4], p_uv[5], p_uv[1], faces_wing)
    add_quad(p_idx[1], p_idx[5], p_idx[6], p_idx[2], p_uv[1], p_uv[5], p_uv[6], p_uv[2], faces_wing)
    add_quad(p_idx[2], p_idx[6], p_idx[7], p_idx[3], p_uv[2], p_uv[6], p_uv[7], p_uv[3], faces_wing)
    add_quad(p_idx[3], p_idx[7], p_idx[4], p_idx[0], p_uv[3], p_uv[7], p_uv[4], p_uv[0], faces_wing)

    N_ENG_SEGS = 12
    nac_profile = [
        (eng_y + ENGINE_L * 0.50, ENGINE_R * 0.85),
        (eng_y + ENGINE_L * 0.25, ENGINE_R * 1.00),
        (eng_y - ENGINE_L * 0.20, ENGINE_R * 0.95),
        (eng_y - ENGINE_L * 0.50, ENGINE_R * 0.78),
    ]

    nac_rings = []
    nac_uvs   = []

    for iy, (py, pr) in enumerate(nac_profile):
        r_v, r_vt = [], []
        for k in range(N_ENG_SEGS):
            a = 2.0 * math.pi * k / N_ENG_SEGS
            ca, sa = math.cos(a), math.sin(a)
            cx = sx + ca * pr
            cz = eng_z + sa * pr
            nrm = norm3((ca, 0.0, sa))
            r_v.append(add_v(cx, py, cz, shade(nrm)))
            r_vt.append(add_vt(k / N_ENG_SEGS, iy / len(nac_profile)))
        nac_rings.append(r_v)
        nac_uvs.append(r_vt)

    for i in range(len(nac_rings) - 1):
        for k in range(N_ENG_SEGS):
            kn = (k + 1) % N_ENG_SEGS
            v1, v2 = nac_rings[i][k], nac_rings[i+1][k]
            v3, v4 = nac_rings[i+1][kn], nac_rings[i][kn]
            t1, t2 = nac_uvs[i][k], nac_uvs[i+1][k]
            t3, t4 = nac_uvs[i+1][kn], nac_uvs[i][kn]
            add_quad(v1, v2, v3, v4, t1, t2, t3, t4, faces_wing)

    in_v  = add_v(sx, nac_profile[0][0]  - 0.8 * SCALE, eng_z, (0.1, 0.1, 0.1))
    in_vt = add_vt(0.5, 0.0)
    ex_v  = add_v(sx, nac_profile[-1][0] - 1.2 * SCALE, eng_z, (0.05, 0.05, 0.05))
    ex_vt = add_vt(0.5, 1.0)

    for k in range(N_ENG_SEGS):
        kn = (k + 1) % N_ENG_SEGS
        add_tri(in_v, nac_rings[0][kn], nac_rings[0][k],
                in_vt, nac_uvs[0][kn], nac_uvs[0][k], faces_dark)
        add_tri(ex_v, nac_rings[-1][k], nac_rings[-1][kn],
                ex_vt, nac_uvs[-1][k], nac_uvs[-1][kn], faces_dark)

for s in (-1, +1):
    build_engine_and_pylon(s, ENGINE_X_IN)
    build_engine_and_pylon(s, ENGINE_X_OUT)


# =====================================================================
#  4. T-TAIL
# =====================================================================
def build_ttail():
    fin_base_y_front = -38.0 * SCALE
    fin_base_y_back  = fin_base_y_front - FIN_CH_BASE

    t_f = 0.5 - (fin_base_y_front / FUSE_LEN)
    t_b = 0.5 - (fin_base_y_back  / FUSE_LEN)
    r_f, z_f, _ = fuse_profile(t_f)
    r_b, z_b, _ = fuse_profile(t_b)

    fin_base_z_front = z_f + r_f - 0.4 * SCALE
    fin_base_z_back  = z_b + r_b - 0.4 * SCALE

    fin_top_z       = fin_base_z_front + FIN_H
    fin_top_y_front = fin_base_y_front - (FIN_H * math.tan(math.radians(FIN_SWEEP)))
    fin_top_y_back  = fin_top_y_front - FIN_CH_TOP

    w_base = 0.9 * SCALE
    w_top  = 0.55 * SCALE

    fin_pts = [
        (-w_base, fin_base_y_front, fin_base_z_front), (+w_base, fin_base_y_front, fin_base_z_front),
        (+w_base, fin_base_y_back,  fin_base_z_back),  (-w_base, fin_base_y_back,  fin_base_z_back),
        (-w_top,  fin_top_y_front,  fin_top_z),        (+w_top,  fin_top_y_front,  fin_top_z),
        (+w_top,  fin_top_y_back,   fin_top_z),        (-w_top,  fin_top_y_back,   fin_top_z),
    ]
    f_idx = [add_v(px, py, pz, shade((abs(px), 0.0, 0.8))) for px, py, pz in fin_pts]
    f_uv  = [add_vt(0.5, 0.5) for _ in range(8)]

    add_quad(f_idx[0], f_idx[4], f_idx[5], f_idx[1], f_uv[0], f_uv[4], f_uv[5], f_uv[1], faces_hull)
    add_quad(f_idx[1], f_idx[5], f_idx[6], f_idx[2], f_uv[1], f_uv[5], f_uv[6], f_uv[2], faces_hull)
    add_quad(f_idx[2], f_idx[6], f_idx[7], f_idx[3], f_uv[2], f_uv[6], f_uv[7], f_uv[3], faces_hull)
    add_quad(f_idx[3], f_idx[7], f_idx[4], f_idx[0], f_uv[3], f_uv[7], f_uv[4], f_uv[0], faces_hull)

    hs_tan_sweep = math.tan(math.radians(20.0))
    hs_z = fin_top_z - 0.2 * SCALE

    hs_stations = [-HSTAB_SPAN, 0.0, HSTAB_SPAN]
    hs_rings, hs_uvs = [], []

    for st_x in hs_stations:
        frac  = abs(st_x) / HSTAB_SPAN
        chord = HSTAB_CH_ROOT + (HSTAB_CH_TIP - HSTAB_CH_ROOT) * frac
        y_le  = fin_top_y_front + 1.2 * SCALE - (abs(st_x) * hs_tan_sweep)
        y_te  = y_le - chord
        thick = SCALE * (1.0 - 0.4 * frac)

        r_v, r_vt = [], []
        pts = [
            (y_le, hs_z),
            (y_le - chord * 0.3, hs_z + thick * 0.5),
            (y_te, hs_z),
            (y_le - chord * 0.3, hs_z - thick * 0.5),
        ]
        for py, pz in pts:
            nrm = (0.0, 0.0, 1.0 if pz >= hs_z else -1.0)
            r_v.append(add_v(st_x, py, pz, shade(nrm)))
            r_vt.append(add_vt(0.5, 0.5))
        hs_rings.append(r_v)
        hs_uvs.append(r_vt)

    for i in range(len(hs_rings) - 1):
        for k in range(4):
            kn = (k + 1) % 4
            v1, v2 = hs_rings[i][k], hs_rings[i+1][k]
            v3, v4 = hs_rings[i+1][kn], hs_rings[i][kn]
            t1, t2 = hs_uvs[i][k], hs_uvs[i+1][k]
            t3, t4 = hs_uvs[i+1][kn], hs_uvs[i][kn]
            add_quad(v1, v2, v3, v4, t1, t2, t3, t4, faces_hull)

build_ttail()


# =====================================================================
#  5. COCKPIT WINDOWS & CARGO RAMP
# =====================================================================
def build_details():
    cp_y = FUSE_LEN * 0.5 - 11.5 * SCALE
    cp_z = 2.8 * SCALE
    cp_v = [
        add_v(-3.2 * SCALE, cp_y + 1.2 * SCALE, cp_z,               (0.15, 0.20, 0.25)),
        add_v(+3.2 * SCALE, cp_y + 1.2 * SCALE, cp_z,               (0.15, 0.20, 0.25)),
        add_v(+2.8 * SCALE, cp_y - 2.0 * SCALE, cp_z + 2.2 * SCALE, (0.15, 0.20, 0.25)),
        add_v(-2.8 * SCALE, cp_y - 2.0 * SCALE, cp_z + 2.2 * SCALE, (0.15, 0.20, 0.25)),
    ]
    cp_vt = [add_vt(0.5, 0.5) for _ in range(4)]
    add_quad(cp_v[0], cp_v[1], cp_v[2], cp_v[3], cp_vt[0], cp_vt[1], cp_vt[2], cp_vt[3], faces_dark)

    ramp_y = -FUSE_LEN * 0.5 + 14.0 * SCALE
    t_r = 0.5 - (ramp_y / FUSE_LEN)
    _, r_z, _ = fuse_profile(t_r)
    ramp_v = [
        add_v(-2.8 * SCALE, ramp_y,                 r_z + 0.5 * SCALE, (0.1, 0.1, 0.12)),
        add_v(+2.8 * SCALE, ramp_y,                 r_z + 0.5 * SCALE, (0.1, 0.1, 0.12)),
        add_v(+2.2 * SCALE, ramp_y - 12.0 * SCALE,  r_z + 6.5 * SCALE, (0.1, 0.1, 0.12)),
        add_v(-2.2 * SCALE, ramp_y - 12.0 * SCALE,  r_z + 6.5 * SCALE, (0.1, 0.1, 0.12)),
    ]
    ramp_vt = [add_vt(0.5, 0.5) for _ in range(4)]
    add_quad(ramp_v[0], ramp_v[3], ramp_v[2], ramp_v[1],
             ramp_vt[0], ramp_vt[3], ramp_vt[2], ramp_vt[1], faces_dark)

build_details()


# =====================================================================
#  6. COORDINATE SYSTEM CONVERSION (fixes plane orientation)
#  Model is authored with:  X = span (right), Y = fuselage (nose +Y), Z = up
#  Game expects:            X = span,          Y = up,              Z = -forward (nose)
#  Mapping:  new_x = old_x,  new_y = old_z,  new_z = -old_y
# =====================================================================
verts = [(v[0], v[2], -v[1], v[3], v[4], v[5]) for v in verts]


# =====================================================================
#  7. RECENTERING & MODEL EXPORT
# =====================================================================
xs = [v[0] for v in verts]; ys = [v[1] for v in verts]; zs = [v[2] for v in verts]
cx, cy, cz = (max(xs)+min(xs))*0.5, (max(ys)+min(ys))*0.5, (max(zs)+min(zs))*0.5
verts = [(v[0]-cx, v[1]-cy, v[2]-cz, v[3], v[4], v[5]) for v in verts]

# Report final bounds (useful for setting collider dimensions in the game)
fx = [v[0] for v in verts]; fy = [v[1] for v in verts]; fz = [v[2] for v in verts]
print(f"Model bounds:  X[{min(fx):.3f},{max(fx):.3f}]  Y[{min(fy):.3f},{max(fy):.3f}]  Z[{min(fz):.3f},{max(fz):.3f}]")
print(f"Full size (WxHxL) = {(max(fx)-min(fx)):.3f} x {(max(fy)-min(fy)):.3f} x {(max(fz)-min(fz)):.3f}")

with open(OUT_OBJ, 'w') as f:
    f.write("# C-17 Globemaster III - scalable geometry\n")
    f.write(f"# SCALE = {SCALE}\n")
    f.write(f"# Orientation: Y-up, -Z forward (nose points toward -Z)\n")
    f.write(f"mtllib {OUT_MTL}\n")
    f.write("o top_c17\n")
    for x, y, z, r, g, b in verts:
        f.write(f"v {x:.6f} {y:.6f} {z:.6f} {r:.4f} {g:.4f} {b:.4f}\n")
    for u, v in uvs:
        f.write(f"vt {u:.6f} {v:.6f}\n")

    f.write("g top_c17_part0\nusemtl top_c17_mat0\n")
    for fc in faces_hull:
        f.write("f " + " ".join(f"{v}/{vt}" for v, vt in fc) + "\n")

    f.write("g top_c17_part1\nusemtl top_c17_mat1\n")
    for fc in faces_wing:
        f.write("f " + " ".join(f"{v}/{vt}" for v, vt in fc) + "\n")

    f.write("g top_c17_part2\nusemtl top_c17_mat2\n")
    for fc in faces_dark:
        f.write("f " + " ".join(f"{v}/{vt}" for v, vt in fc) + "\n")

with open(OUT_MTL, 'w') as f:
    f.write("# Materials for C-17 Globemaster III\n\n")
    for name, tex in [("top_c17_mat0", TEX_HULL),
                      ("top_c17_mat1", TEX_WING),
                      ("top_c17_mat2", TEX_DARK)]:
        f.write(f"newmtl {name}\n")
        f.write("Ka 1.0 1.0 1.0\nKd 1.0 1.0 1.0\nKs 0.1 0.1 0.1\nd 1.0\nillum 1\n")
        f.write(f"map_Kd {TEX_DIR}/{tex}\n\n")

with open(OUT_PROPS, 'w') as f:
    f.write("name=Globemaster\nscale=1.0\nrotate=0\nyoffset=0\nfit=true\ncull=back\n")


# =====================================================================
#  8. TEXTURE GENERATION
# =====================================================================
def gen_textures(size=256):
    if not HAS_PIL:
        print("PIL not found -- skipped texture image creation.")
        return
    os.makedirs(TEX_DIR, exist_ok=True)
    random.seed(1776)

    img_hull = Image.new('RGB', (size, size), (138, 142, 138))
    draw_h = ImageDraw.Draw(img_hull)
    for y in range(0, size, 16):
        draw_h.line((0, y, size, y), fill=(120, 124, 120), width=1)
    draw_h.ellipse((size//4 - 12, size//2 - 12, size//4 + 12, size//2 + 12), fill=(30, 60, 120))
    draw_h.rectangle((size//4 - 20, size//2 - 4, size//4 + 20, size//2 + 4), fill=(230, 235, 240))
    img_hull.filter(ImageFilter.GaussianBlur(0.3)).save(os.path.join(TEX_DIR, TEX_HULL))

    img_wing = Image.new('RGB', (size, size), (128, 132, 128))
    draw_w = ImageDraw.Draw(img_wing)
    for x in range(0, size, 20):
        draw_w.line((x, 0, x, size), fill=(110, 114, 110), width=1)
    img_wing.filter(ImageFilter.GaussianBlur(0.3)).save(os.path.join(TEX_DIR, TEX_WING))

    img_dark = Image.new('RGB', (size, size), (25, 28, 32))
    img_dark.save(os.path.join(TEX_DIR, TEX_DARK))

gen_textures()

print(f"Successfully generated scalable C-17 model (SCALE={SCALE}):")
print(f" - {OUT_OBJ} ({len(verts)} vertices, {len(faces_hull)+len(faces_wing)+len(faces_dark)} faces)")
print(f" - {OUT_MTL}")
print(f" - {OUT_PROPS}")