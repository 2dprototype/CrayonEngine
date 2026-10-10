#!/usr/bin/env python3
"""
Generates the models for examples/games/truck_sim as binary glTF (.glb):
  tractor.glb        cab-over semi tractor unit      (origin = chassis center, ground at y = -1.15)
  trailer.glb        40ft container-chassis trailer   (origin = chassis center, deck top at y = +0.15)
  trailer_legs.glb   landing gear (draw while unhitched)
  container_*.glb    40ft ribbed shipping containers (origin = center, 2.4 x 2.6 x 12.0 m)
  truck_wheel.glb    truck wheel/tyre, axle along X, radius 0.5, width 0.30
Conventions: forward = +Z, up = +Y, left = +X.  Requires numpy.  Uses tools/gen_car_models.py helpers.
Usage: python3 tools/gen_truck_models.py [output_dir]
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_car_models import Mesh, write_glb, build_wheel, WHEEL_MATS

# ---------------------------------------------------------------- helpers ---
def cyl(m, mat, c, radius, length, axis, seg=20, cap=True):
    """cylinder centered at c along 'x' | 'y' | 'z' (cyclic axis mapping keeps outward winding)"""
    cx, cy, cz = c
    def pt(a, cosv, sinv):
        if axis == 'x': return (cx + a, cy + radius * cosv, cz + radius * sinv)
        if axis == 'y': return (cx + radius * sinv, cy + a, cz + radius * cosv)
        return (cx + radius * cosv, cy + radius * sinv, cz + a)
    h = length / 2
    for i in range(seg):
        a0 = 2 * math.pi * i / seg; a1 = 2 * math.pi * (i + 1) / seg
        c0, s0, c1, s1 = math.cos(a0), math.sin(a0), math.cos(a1), math.sin(a1)
        m.quad(mat, pt(-h, c0, s0), pt(-h, c1, s1), pt(h, c1, s1), pt(h, c0, s0))
        if cap:
            m.tri(mat, pt(h, 0, 0), pt(h, c0, s0), pt(h, c1, s1))
            m.tri(mat, pt(-h, 0, 0), pt(-h, c1, s1), pt(-h, c0, s0))

def wedge_roof(m, mat, x0, x1, z_front, y_front, z_back, y_back, y_base):
    """sloped roof deflector: closed wedge between two heights"""
    a, b = (x0, y_base, z_front), (x1, y_base, z_front)
    c, d = (x0, y_base, z_back), (x1, y_base, z_back)
    e, f = (x0, y_front, z_front), (x1, y_front, z_front)
    g, h = (x0, y_back, z_back), (x1, y_back, z_back)
    m.quad(mat, e, f, h, g)                 # sloped top
    m.quad(mat, a, e, g, c)                 # side -x ... (double sided helps)
    m.quad(mat, g, e, a, c); m.quad(mat, f, b, d, h); m.quad(mat, h, d, b, f)
    m.quad(mat, a, b, f, e); m.quad(mat, e, f, b, a)
    m.quad(mat, c, g, h, d); m.quad(mat, d, h, g, c)

# ----------------------------------------------------------------- tractor ---
def build_tractor(paint=(0.10, 0.25, 0.65)):
    m = Mesh()
    # frame rails + cross members
    for sx in (1, -1):
        m.box("trim", (sx * 0.45, -0.25, -0.1), (0.18, 0.30, 7.0))
    for z in (-3.4, -2.6, -1.4, -0.4, 0.8, 1.8, 2.8):
        m.box("trim", (0, -0.28, z), (0.9, 0.10, 0.14))
    # engine / apron under the cab, bumper
    m.box("trim", (0, 0.05, 3.2), (2.2, 0.35, 0.8))
    m.box("chrome", (0, -0.12, 3.66), (2.35, 0.30, 0.22))
    m.box("trim", (0, -0.40, 3.62), (1.6, 0.18, 0.12))
    # cab shell (day cab + sleeper), roof deflector
    m.box("paint", (0, 1.40, 1.75), (2.40, 2.30, 3.50))
    m.box("paint", (0, 2.62, 1.30), (2.30, 0.10, 2.60))            # roof cap
    wedge_roof(m, "paint", -1.10, 1.10, 2.0, 2.67, -0.05, 3.05, 2.67)
    m.box("paint", (0, 0.55, 3.55), (2.40, 0.65, 0.12))            # lower front panel
    # windows
    m.box("glass", (0, 1.85, 3.512), (2.05, 1.0, 0.02))            # windshield
    for sx in (1, -1):
        m.box("glass", (sx * 1.203, 1.85, 2.80), (0.02, 0.95, 1.05))   # door window
        m.box("glass", (sx * 1.203, 1.95, 0.95), (0.02, 0.50, 0.90))   # sleeper window
        m.box("trim",  (sx * 1.21, 0.80, 2.80), (0.02, 0.05, 1.10))    # door sill line
        m.box("trim",  (sx * 1.215, 1.40, 2.20), (0.02, 1.40, 0.04))   # door gap
        # steps and mirrors
        m.box("trim", (sx * 1.15, 0.12, 2.9), (0.30, 0.05, 0.50))
        m.box("trim", (sx * 1.42, 1.95, 3.25), (0.05, 0.60, 0.22))
        m.box("trim", (sx * 1.28, 2.30, 3.30), (0.30, 0.04, 0.04))
        # headlights, corner/marker lights
        m.box("light", (sx * 0.88, 0.60, 3.625), (0.40, 0.22, 0.04))
        m.box("amber", (sx * 1.14, 0.60, 3.60), (0.12, 0.18, 0.10))
        # fenders over wheels
        m.box("paint", (sx * 1.00, -0.02, 2.4), (0.46, 0.04, 1.15))
        m.box("paint", (sx * 1.00, -0.02, -2.35), (0.50, 0.04, 2.9))
        # fuel tanks + exhaust stacks
        cyl(m, "tank", (sx * 1.00, -0.28, -0.35), 0.36, 1.5, 'z', seg=18)
        cyl(m, "chrome", (sx * 1.22, 1.6, -0.12), 0.07, 3.2, 'y', seg=12)
        m.box("trim", (sx * 1.22, 3.2, -0.12), (0.16, 0.05, 0.16))
        # mud flaps
        m.box("trim", (sx * 1.00, -0.75, -3.72), (0.60, 0.50, 0.03))
    # grille
    m.box("trim", (0, 0.58, 3.625), (1.30, 0.50, 0.03))
    for i in range(6):
        m.box("chrome", (0, 0.38 + i * 0.08, 3.64), (1.26, 0.03, 0.02))
    # roof marker lights and air horn
    for i in range(5):
        m.box("amber", (-0.6 + i * 0.3, 2.70, 3.12), (0.12, 0.07, 0.06))
    cyl(m, "chrome", (0.5, 3.05, 0.9), 0.06, 0.3, 'y', seg=10)
    # fifth wheel plate with slot
    m.box("plate", (0, 0.30, -2.2), (1.45, 0.24, 1.30))
    m.box("trim", (0, 0.425, -2.55), (0.30, 0.02, 0.65))
    m.box("chrome", (0, 0.40, -1.75), (0.18, 0.06, 0.12))
    # rear lights, air tank
    for sx in (1, -1):
        m.box("tail", (sx * 0.40, -0.20, -3.64), (0.22, 0.12, 0.04))
    cyl(m, "tank", (0, -0.12, -3.1), 0.14, 0.9, 'x', seg=12)
    return m

TRACTOR_MATS = {
    "paint": ((0.10, 0.25, 0.65, 1.0), 0.35, 0.40),
    "glass": ((0.10, 0.16, 0.22, 1.0), 0.0, 0.05),
    "trim":  ((0.05, 0.05, 0.06, 1.0), 0.1, 0.8),
    "chrome": ((0.80, 0.82, 0.86, 1.0), 1.0, 0.2),
    "light": ((1.00, 0.95, 0.75, 1.0), 0.0, 0.1),
    "amber": ((1.00, 0.60, 0.05, 1.0), 0.0, 0.2),
    "tail":  ((0.90, 0.05, 0.05, 1.0), 0.0, 0.2),
    "tank":  ((0.72, 0.74, 0.78, 1.0), 0.9, 0.3),
    "plate": ((0.14, 0.14, 0.15, 1.0), 0.6, 0.5),
}

# ------------------------------------------------------------------ trailer ---
def build_trailer():
    m = Mesh()
    L = 13.0
    for sx in (1, -1):
        m.box("beam", (sx * 0.55, 0.0, 0.0), (0.25, 0.30, L))                 # main I-beams
        m.box("beam", (sx * 1.18, 0.02, 0.0), (0.12, 0.20, L))                # outer rails
        m.box("trim", (sx * 1.20, -0.30, -1.5), (0.05, 0.18, 6.0))             # side underrun guard
    for i in range(14):
        m.box("beam", (0, -0.02, -6.2 + i * 0.95), (2.4, 0.12, 0.14))         # cross members
    m.box("deck", (0, 0.12, 0.0), (2.5, 0.06, 12.9))                          # deck
    # twist-lock corner castings for a 40ft container (container is 12.0 m long)
    for sx in (1, -1):
        for z in (5.9, -5.9):
            m.box("steel", (sx * 1.1, 0.21, z), (0.26, 0.12, 0.30))
            cyl(m, "chrome", (sx * 1.1, 0.30, z), 0.04, 0.10, 'y', seg=8)
    # kingpin + front plate
    m.box("plate", (0, -0.18, 5.9), (1.30, 0.05, 1.00))
    cyl(m, "chrome", (0, -0.24, 5.9), 0.09, 0.12, 'y', seg=14)
    m.box("beam", (0, -0.08, 5.9), (2.3, 0.20, 0.30))
    # rear tandem subframe, hangers, bumper, lights
    m.box("beam", (0, -0.35, -5.05), (2.3, 0.18, 2.7))
    for sx in (1, -1):
        m.box("beam", (sx * 1.0, -0.62, -5.05), (0.12, 0.45, 2.7))
        m.box("tail", (sx * 0.95, -0.15, -6.52), (0.30, 0.14, 0.04))
        m.box("amber", (sx * 1.22, 0.00, 5.85), (0.05, 0.08, 0.20))
        m.box("amber", (sx * 1.22, 0.00, -6.3), (0.05, 0.08, 0.20))
        m.box("trim", (sx * 1.0, -1.15, -6.1), (0.60, 0.55, 0.03))             # mud flaps
        m.box("trim", (sx * 1.0, -0.80, -4.4), (0.34, 0.04, 2.6))              # fenders ... low profile
    m.box("bumper", (0, -0.42, -6.62), (2.4, 0.28, 0.14))
    for i in range(10):                                                         # reflective tape
        m.box("tape_r" if i % 2 == 0 else "tape_w", (-1.08 + i * 0.24, -0.62, -6.70), (0.24, 0.07, 0.02))
    # air lines at the front
    cyl(m, "trim", (0.4, -0.05, 6.3), 0.03, 0.5, 'z', seg=8)
    cyl(m, "amber", (-0.4, -0.05, 6.3), 0.03, 0.5, 'z', seg=8)
    return m

TRAILER_MATS = {
    "beam":  ((0.12, 0.12, 0.14, 1.0), 0.4, 0.6),
    "deck":  ((0.22, 0.22, 0.24, 1.0), 0.3, 0.7),
    "steel": ((0.45, 0.46, 0.50, 1.0), 0.8, 0.4),
    "chrome": ((0.85, 0.85, 0.88, 1.0), 1.0, 0.2),
    "plate": ((0.18, 0.18, 0.19, 1.0), 0.6, 0.5),
    "trim":  ((0.04, 0.04, 0.05, 1.0), 0.1, 0.8),
    "tail":  ((0.90, 0.05, 0.05, 1.0), 0.0, 0.2),
    "amber": ((1.00, 0.60, 0.05, 1.0), 0.0, 0.2),
    "bumper": ((0.30, 0.30, 0.32, 1.0), 0.7, 0.4),
    "tape_r": ((0.85, 0.05, 0.05, 1.0), 0.0, 0.3),
    "tape_w": ((0.95, 0.95, 0.95, 1.0), 0.0, 0.3),
}

def build_legs():
    m = Mesh()
    for sx in (1, -1):
        m.box("leg", (sx * 1.15, -1.0, 4.0), (0.20, 1.70, 0.30))
        m.box("leg", (sx * 1.15, -1.82, 4.0), (0.55, 0.06, 0.60))        # foot
        m.box("chrome", (sx * 1.28, -0.75, 4.0), (0.05, 0.05, 0.05))     # crank
    m.box("leg", (0, -0.45, 4.0), (2.3, 0.12, 0.14))                       # cross brace
    return m

LEG_MATS = {"leg": ((0.55, 0.55, 0.58, 1.0), 0.7, 0.5), "chrome": ((0.85, 0.85, 0.88, 1.0), 1.0, 0.2)}

# ---------------------------------------------------------------- container ---
def build_container():
    m = Mesh()
    hx, hy, hz = 1.2, 1.3, 6.0
    m.box("body", (0, 0.0, 0.0), (2.30, 2.54, 11.92))
    # corrugation ribs on both sides and the roof
    n = 58
    for i in range(n):
        z = -hz + 0.35 + i * ((2 * hz - 0.7) / (n - 1))
        for sx in (1, -1):
            m.box("body", (sx * (hx - 0.045), 0.0, z), (0.07, 2.30, 0.09))
        m.box("body", (0, hy - 0.045, z), (2.05, 0.07, 0.09))
    # corner castings + frame rails
    for sx in (1, -1):
        for sy in (1, -1):
            for sz in (1, -1):
                m.box("steel", (sx * (hx - 0.08), sy * (hy - 0.08), sz * (hz - 0.10)), (0.17, 0.17, 0.20))
            m.box("frame", (sx * (hx - 0.03), sy * (hy - 0.03), 0), (0.06, 0.06, 12.0))
            m.box("frame", (0, sy * (hy - 0.03), sx * (hz - 0.03)), (2.40, 0.06, 0.06))
    for sy in (1, -1):
        for sx in (1, -1):
            m.box("frame", (sx * (hx - 0.03), sy * (hy - 0.03), 0), (0.06, 0.06, 12.0))
    # rear doors (z = -hz) with locking rods
    m.box("door", (0, 0.0, -hz - 0.005), (2.2, 2.4, 0.04))
    for k in (-0.72, -0.30, 0.30, 0.72):
        cyl(m, "chrome", (k, 0.0, -hz - 0.04), 0.022, 2.35, 'y', seg=8)
    m.box("trim", (0, 0.0, -hz - 0.03), (0.04, 2.40, 0.03))
    # front wall panel & label panels (white) on both sides
    m.box("body", (0, 0.0, hz + 0.005), (2.25, 2.40, 0.04))
    for sx in (1, -1):
        m.box("label", (sx * (hx + 0.002), 0.35, 1.2), (0.01, 0.55, 3.2))
        m.box("label", (sx * (hx + 0.002), -0.45, -3.5), (0.01, 0.30, 2.0))
    return m

def container_mats(color):
    return {
        "body":  ((color[0], color[1], color[2], 1.0), 0.2, 0.55),
        "door":  ((color[0] * 0.85, color[1] * 0.85, color[2] * 0.85, 1.0), 0.2, 0.6),
        "steel": ((0.35, 0.36, 0.40, 1.0), 0.8, 0.5),
        "frame": ((color[0] * 0.6, color[1] * 0.6, color[2] * 0.6, 1.0), 0.3, 0.6),
        "chrome": ((0.8, 0.8, 0.84, 1.0), 1.0, 0.25),
        "trim":  ((0.05, 0.05, 0.05, 1.0), 0.1, 0.8),
        "label": ((0.93, 0.93, 0.90, 1.0), 0.0, 0.6),
    }

# -------------------------------------------------------------------- main ---
if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "examples", "games", "truck_sim", "assets")
    os.makedirs(out, exist_ok=True)
    write_glb(os.path.join(out, "tractor.glb"), build_tractor(), TRACTOR_MATS, "tractor")
    write_glb(os.path.join(out, "trailer.glb"), build_trailer(), TRAILER_MATS, "trailer")
    write_glb(os.path.join(out, "trailer_legs.glb"), build_legs(), LEG_MATS, "trailer_legs")
    for name, col in (("red", (0.70, 0.10, 0.08)), ("blue", (0.10, 0.28, 0.65)),
                      ("green", (0.12, 0.45, 0.20)), ("orange", (0.90, 0.45, 0.08))):
        write_glb(os.path.join(out, f"container_{name}.glb"), build_container(), container_mats(col), f"container_{name}")
    wm = dict(WHEEL_MATS)
    wm["rim"] = ((0.62, 0.64, 0.68, 1.0), 0.9, 0.35)
    write_glb(os.path.join(out, "truck_wheel.glb"), build_wheel(0.5, 0.30), wm, "truck_wheel", double_sided=("tire_line",))
