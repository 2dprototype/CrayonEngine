#!/usr/bin/env python3
"""Writes examples/assets/models/collision_demo.gltf: a tiny level showing the collision-node naming
convention you would use in Blender (name objects, export glTF 2.0 with Custom Properties enabled).

  Ground, Ramp, Crate, Pillar         visible meshes
  Ground_col, Ramp_col                -> triangle-mesh collision (hidden)         "<name>_col" / "col_<name>"
  UBX_Crate                           -> box collision (hidden)                    UBX_ / USP_ / UCP_ / UCX_ prefixes
  UCX_Pillar                          -> convex-hull collision (hidden)
  spawn_player, spawn_enemy           empties with custom properties (team, kind)
"""
import base64, json, math, os, struct

pos = [(-.5,-.5,-.5),(.5,-.5,-.5),(.5,.5,-.5),(-.5,.5,-.5),(-.5,-.5,.5),(.5,-.5,.5),(.5,.5,.5),(-.5,.5,.5)]
idx = [0,3,2,0,2,1, 4,5,6,4,6,7, 0,4,7,0,7,3, 1,2,6,1,6,5, 0,1,5,0,5,4, 3,7,6,3,6,2]
buf = b''.join(struct.pack('<3f', *p) for p in pos) + b''.join(struct.pack('<H', i) for i in idx)

def tilt(deg):   # rotation about Z
    a = math.radians(deg) / 2
    return [0, 0, math.sin(a), math.cos(a)]

nodes = [
    {"name": "Ground",     "mesh": 0, "translation": [0, -0.5, 0], "scale": [30, 1, 30]},
    {"name": "Ground_col", "mesh": 0, "translation": [0, -0.5, 0], "scale": [30, 1, 30]},
    {"name": "Ramp",       "mesh": 0, "translation": [-6, 0.9, 6], "rotation": tilt(18), "scale": [8, 0.4, 5]},
    {"name": "Ramp_col",   "mesh": 0, "translation": [-6, 0.9, 6], "rotation": tilt(18), "scale": [8, 0.4, 5]},
    {"name": "Crate",      "mesh": 0, "translation": [5, 0.75, -3], "scale": [1.5, 1.5, 1.5]},
    {"name": "UBX_Crate",  "mesh": 0, "translation": [5, 0.75, -3], "scale": [1.5, 1.5, 1.5]},
    {"name": "Pillar",     "mesh": 0, "translation": [-7, 2, -5], "scale": [1.6, 4, 1.6]},
    {"name": "UCX_Pillar", "mesh": 0, "translation": [-7, 2, -5], "scale": [1.6, 4, 1.6]},
    {"name": "spawn_player", "translation": [0, 1, 8], "extras": {"team": "blue", "kind": "player", "health": 100}},
    {"name": "spawn_enemy",  "translation": [8, 1, -8], "extras": {"team": "red", "kind": "enemy", "patrol": [8, -8, 2, -8]}},
]
gltf = {
    "asset": {"version": "2.0", "generator": "tools/make_collision_demo_gltf.py"},
    "scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}], "nodes": nodes,
    "meshes": [{"name": "UnitCube", "primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 8, "type": "VEC3", "min": [-.5, -.5, -.5], "max": [.5, .5, .5]},
        {"bufferView": 1, "componentType": 5123, "count": 36, "type": "SCALAR"}],
    "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 96, "target": 34962},
                    {"buffer": 0, "byteOffset": 96, "byteLength": 72, "target": 34963}],
    "buffers": [{"byteLength": len(buf), "uri": "data:application/octet-stream;base64," + base64.b64encode(buf).decode()}],
}
out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "examples/assets/models/collision_demo.gltf")
with open(out, "w") as f:
    json.dump(gltf, f, indent=1)
print("wrote", out)
