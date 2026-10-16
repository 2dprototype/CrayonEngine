# New graphics features (GPU and CPU renderers)

Everything here works on both backends (see `RENDERERS.md`). Examples: `examples/25_2d_features.lua`,
`26_3d_features.lua`, `28_collision.lua`; self-tests: `20_renderer_selftest.lua`, `27_features_selftest.lua`.
Run the headless test suite with `tools/run_tests.sh`.

## Culling (automatic)
* **Frustum culling** for every mesh: primitives, custom meshes, models, billboards, skinned meshes (bind-pose
  box grown by 100%). **Distance culling**: `graphics.setMaxDrawDistance(d)`.
* **Backface culling** is applied automatically to *single-sided* geometry only: closed primitives and glTF parts
  whose material has `doubleSided = false`. Planes, OBJ models and double-sided materials are never culled.
  (Fixed along the way: sphere/cylinder/cone/torus triangle winding and the cone's inward normals.)
* **2D offscreen culling** drops quads outside the view at flush time. Skipped while a custom shader is active.
* `graphics.setAutoCull(bool)`, `getAutoCull()`, `setOffscreenCulling(bool)`, `getCullStats() -> draws3D, quads2D`,
  `isSphereVisible(x,y,z,r)`, `isBoxVisible(minx,miny,minz,maxx,maxy,maxz)`.
* `graphics.drawModelLOD({m1,m2,m3}, {d1,d2,d3}, x,y,z, rx,ry,rz, sx,sy,sz, tex)` -> level drawn (nil if beyond the
  last distance). With one fewer distance than models the last model is used at any range.
* Not implemented: occlusion culling.

## 2D
| Feature | API |
|---|---|
| Sprite sheets / atlas / animation | `crayon.sprite.newSheet/newAtlas/newAnimation/newAnimator` |
| Tilemaps, Tiled JSON import (flip flags, animated tiles, objects, collision helpers) | `crayon.tilemap.new/loadTiled`, `map:draw{view=...}` |
| Texture filter / wrap | `loadTexture(path,{filter=,wrap=})`, `tex:setFilter("nearest"\|"linear"\|"mipmap")`, `tex:setWrap("repeat"\|"clamp"\|"mirror")` |
| Pixel data | `graphics.newImageData(w,h[,r,g,b,a])`, `loadImageData`, `img:getPixel/setPixel/fill/paste/copy/mapPixels/save`, `graphics.newTexture(img)`, `tex:update(img)`, `tex:destroy()` |
| Screenshots | `graphics.screenshot(path)`, `captureFrame()` + `getCapture() -> ImageData` (game frame before post effects) |
| Text effects | `drawText(s,x,y,{outline={color=,thickness=}, shadow={dx=,dy=,color=}})` |
| 2D lights | `crayon.lights2d.new{resolution=}`, `:setAmbient/add/draw`, low-level `graphics.renderLightmap` |
| Masking | `graphics.beginMask()`, draw shape, `useMask([invert])`, draw content, `endMask()` |
| Layers / z-order | `crayon.drawqueue.new()`, `q:add(z, fn)`, `q:flush()` |
| JSON | `crayon.json.decode/encode` |

## 3D
| Feature | API |
|---|---|
| Transparency | draws with colour alpha < 1 (or glTF `alphaMode=BLEND`) are queued, sorted back-to-front and blended; flushed at the end of 3D, when depth test is turned off, or `graphics.flushTransparent()` |
| Skybox | `graphics.loadSkybox(panorama \| ImageData \| {6 faces})`, `graphics.drawSkybox(sky[,r,g,b])` - fills only uncovered pixels, call first or last |
| Static batching + chunked culling | `b = graphics.newStaticBatch(chunk)`, `b:add(model,x,y,z,rx,ry,rz,sx,sy,sz,tex)`, `b:build()`, `b:draw()` |
| Terrain | `graphics.createTerrain(heights \| ImageData \| path, opts)`, `t:draw(tex)`, `getHeightAt`, `getNormalAt`, `contains`, `raycast` |

Notes
* Static batching reduces draw calls (a GPU win). On the CPU renderer coarse chunks can transform more vertices than
  per-object culling skips, so prefer small chunk sizes there.
* Mask shapes define the mask by coverage (sprite alpha is ignored).
* Screenshots are written as uncompressed PNG.

## Collision (physics3D)
Lua module `crayon.physics3D`. Verified with real Jolt physics (`test/soft/physics_shapes_test.cpp`).
| Feature | API |
|---|---|
| Level collision from a model (Blender workflow) | `physics3D.createModelBody(model, {x,y,z, rx,ry,rz, scale, motion="static", shape="auto", friction, restitution, density})` |
| Terrain collider (heightfield) | `terrain:createBody([friction, restitution])`, or `physics3D.createHeightfieldBody(rows, {width,height,depth,x,y,z})` |
| Convex hull bodies (dynamic props) | `physics3D.createConvexBody(x,y,z, points \| model, {motion="dynamic", ...})` |
| Compound bodies | `physics3D.createCompoundBody(x,y,z, {{shape="box",hx=,hy=,hz=,x=,y=,z=,rx=,ry=,rz=}, {shape="sphere",radius=}, {shape="capsule"\|"cylinder",radius=,halfHeight=}, {shape="convex",points=}, {shape="mesh",vertices=,indices=}}, opts)` |
| Spawn points / custom properties | `model:getNodes()` -> `{name, x,y,z (local), wx,wy,wz (world), wrx,wry,wrz (world euler, drawModel order), hasMesh, collision, properties={...}}` |
| Debugging | `model:getCollisionParts()`, `model:hasCollision()`, `model:setCollisionVisible(true)` |

**Authoring collision in Blender / any DCC tool** (glTF 2.0 or OBJ; object/node names decide):
`UBX_name` box, `USP_name` sphere, `UCP_name` capsule, `UCX_name` convex hull, `name_col` / `col_name` /
`name-collision` triangle mesh (add `_convex`, `_hull`, `_box`, `_sphere` or `_capsule` as another token to pick a
shape). Blender's `.001` suffixes are ignored. Collision nodes are never drawn and do not affect model bounds
or static batches. A node custom property `collision = "mesh"|"convex"|"box"|"sphere"|"capsule"|"none"` overrides
the name (the node stays visible unless `collision_only = true`). Enable *Custom Properties* in Blender's glTF
export so empties' properties (spawn points, tags) arrive in `properties`.
Without collision nodes, `createModelBody` uses the visible geometry: triangle mesh for static bodies, convex hull
for moving ones (`shape` can force `"mesh"|"convex"|"box"|"sphere"|"capsule"`).

Notes: triangle meshes are static-only (moving bodies get convex hulls / primitives instead; asking for a moving
mesh is a clear error). Hulls over 256 points are simplified (`maxHullPoints`). Box/sphere/capsule fits use the
node's own orientation; non-uniform `scale` on rotated primitives is approximate. Terrain heights are resampled
to Jolt's grid (multiple of 4 samples per side); physics and visual surfaces agree to within a fraction of a cell
(diagonal triangulation can differ slightly). `examples/28_collision.lua` + `examples/assets/models/collision_demo.gltf`
(written by `tools/make_collision_demo_gltf.py`) show everything.

## Not implemented yet
Shadows, normal/emissive/specular materials, multiple cameras / viewports / 3D render-to-texture, decals and trails,
extra post effects (bloom, blur, colour grading), occlusion culling, GPU instancing.
