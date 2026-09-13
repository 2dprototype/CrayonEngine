# Crayon Engine - Lua API Documentation

## Overview
Crayon Engine is a 2D/3D game engine with retro aesthetics, modern physics, and Lua scripting. All engine functionality is accessed through the global `crayon` table.

## Table of Contents
1. [Window Module](#window-module)
2. [Graphics Module](#graphics-module)
3. [Input Module](#input-module)
4. [Time Module](#time-module)
5. [Physics Module](#physics-module)
6. [Physics2D Module](#physics2d-module)
7. [Complete Examples](#complete-examples)

---

## Window Module
Access via: `crayon.window`

### Functions

| Function | Description |
|----------|-------------|
| `setResolution(w, h)` | Set virtual resolution (default: 320x240) |
| `getResolution()` | Returns current virtual resolution |
| `setWindowSize(w, h)` | Set physical window size |
| `getWindowSize()` | Returns current window size |
| `setPosition(x, y)` | Set window position on screen |
| `getPosition()` | Returns window position |
| `center()` | Center window on screen |
| `setMinSize(w, h)` | Set minimum window size |
| `setMaxSize(w, h)` | Set maximum window size |
| `setFullscreen(enabled)` | Toggle fullscreen |
| `isFullscreen()` | Returns fullscreen state |
| `setVsync(enabled)` | Toggle VSync |
| `getVsync()` | Returns VSync state |
| `setTitle(title)` | Set window title |
| `getTitle()` | Returns window title |
| `setResizable(enabled)` | Allow/disable window resizing |
| `isResizable()` | Returns resizable state |
| `setBordered(enabled)` | Show/hide window borders |
| `isBordered()` | Returns bordered state |
| `maximize()` | Maximize window |
| `minimize()` | Minimize window |
| `restore()` | Restore window from min/max |
| `isMaximized()` | Returns maximized state |
| `isMinimized()` | Returns minimized state |
| `isFocused()` | Returns focused state |
| `setScalingMode(mode)` | Set scaling: "integer", "aspect", "stretch", "center" |
| `getScalingMode()` | Returns current scaling mode |
| `setOpacity(opacity)` | Set window opacity (0.0-1.0) |
| `getOpacity()` | Returns window opacity |
| `setAlwaysOnTop(enabled)` | Keep window on top |
| `isAlwaysOnTop()` | Returns always-on-top state |
| `raise()` | Raise window to front |
| `focus()` | Focus window |
| `flash()` | Flash taskbar icon |
| `setMouseGrab(enabled)` | Grab mouse to window |
| `isMouseGrabbed()` | Returns mouse grab state |
| `setTransparent(enabled)` | Enable transparent window (requires OS support) |
| `isTransparent()` | Returns transparency state |
| `showCursor(show)` | Show/hide cursor |
| `isCursorVisible()` | Returns cursor visibility |
| `setMouseRelative(enabled)` | Enable relative mouse mode |
| `isMouseRelative()` | Returns relative mouse state |
| `getDisplaySize()` | Get primary monitor resolution |
| `getFps()` | Returns current FPS |
| `quit()` | Quit the engine |

### Window Module Examples

```lua
-- Basic window setup
crayon.window.setResolution(640, 360)
crayon.window.setWindowSize(1280, 720)
crayon.window.setTitle("My Game")
crayon.window.setVsync(true)

-- Scaling modes
crayon.window.setScalingMode("integer")  -- Pixel-perfect
crayon.window.setScalingMode("aspect")   -- Letterbox
crayon.window.setScalingMode("stretch")  -- Fill screen

-- Fullscreen toggle
if crayon.input.isPressed("f11") then
    crayon.window.setFullscreen(not crayon.window.isFullscreen())
end

-- Get display info
local w, h = crayon.window.getDisplaySize()
print("Display:", w, "x", h)
```

---

## Graphics Module
Access via: `crayon.graphics`

### Color & Canvas

| Function | Description |
|----------|-------------|
| `clear(r, g, b [, a])` | Clear screen with color (0-1) |
| `setColor(r, g, b [, a])` | Set active drawing color (0-1) |

### Retro Effects

| Function | Description |
|----------|-------------|
| `setRetroEffects(opts)` | Apply retro visual effects |

**Options Table**:
```lua
{
    jitterResolution = {160, 120},  -- Pixel snap resolution, or nil to disable
    affine = 1.0,                    -- 0=perspective, 1=affine texture mapping
    dither = true,                   -- Enable dithering
    colorDepth = 32,                 -- 32 or 8 (bits per channel)
    ditherLevels = 32,               -- Override colorDepth
    fog = {                          -- Distance fog
        startDist = 5.0,
        endDist = 25.0,
        color = {0.1, 0.1, 0.2}
    },
    crt = {                          -- CRT effects
        scanlines = 0.25,            -- or true for default
        curvature = 0.05,            -- or true for default
        vignette = 0.25              -- or true for default
    },
    scanlines = 0.25,                -- Shortcut for crt.scanlines
    curvature = 0.05,                -- Shortcut for crt.curvature
    vignette = 0.25                  -- Shortcut for crt.vignette
}
```

### 3D Camera

| Function | Description |
|----------|-------------|
| `setCamera3d(cam)` | Set 3D camera parameters |
| `getCameraRay(sx, sy)` | Get 3D ray from screen position |

**Camera Table**:
```lua
{
    position = {x, y, z},
    target = {x, y, z},
    up = {x, y, z},       -- default: {0, 1, 0}
    fov = 60,             -- field of view in degrees
    near = 0.1,
    far = 1000.0,
    ortho = false,        -- true for orthographic projection
    orthoSize = 10.0      -- half-height in world units
}
```

Alternatively, `setCamera3d(x, y, z [, yaw, pitch, fov])` sets position and orients using yaw/pitch degrees.

### Lighting

| Function | Description |
|----------|-------------|
| `setLight(dx, dy, dz, lr, lg, lb, ar, ag, ab)` | Set directional light |
| `setPointLight(idx, x, y, z, r, g, b, radius, intensity)` | Set point light (idx: 0-3) |
| `setPointLightEnabled(idx, enabled)` | Enable/disable point light |
| `setShadingMode(mode)` | Set shading: "gouraud", "flat", "unlit" |

### Textures

| Function | Description |
|----------|-------------|
| `loadTexture(path)` | Load texture, returns `Graphics.Texture` userdata |
| `getTextureSize(tex)` | Returns texture width, height |
| `getWhiteTexture()` | Returns white texture (fallback) |

**Texture Methods**: `:getSize()`, `:getWidth()`, `:getHeight()`, `:getId()`, `:isValid()`

### Models & Meshes

| Function | Description |
|----------|-------------|
| `loadModel(name)` | Load/create model, returns handle (`.obj`, `.gltf`, `.glb`, or primitive) |
| `createMesh(data)` | Create custom mesh, returns `Graphics.Model` handle |
| `createAnimator(model)` | Create skeletal `Graphics.Animator` from a skinned model |

**Model Names**: "cube", "plane", "sphere", "cylinder", "cone", "pyramid", "torus", "capsule", "grid", or path to `.obj`, `.gltf`, `.glb` file.

**Model Methods**:
- `:isValid()` — Returns true if the underlying model loaded correctly.
- `:getNodeCount()` — Returns number of nodes in the hierarchy.
- `:getNode(idx_or_name)` — Returns a node table `{name, index, parent, x, y, z, rx, ry, rz, rw, sx, sy, sz}`, or `nil` if not found.
- `:getNodes()` — Returns array table of all node tables.
- `:getPartCount()` — Returns number of drawable parts (materials).
- `:getPartName(idx)` — Returns `name, material_name` (1-indexed).
- `:getPartTexture(idx)` — Returns texture ID for a part.
- `:setPartTexture(idx, tex)` — Assign a texture to a part.
- `:setPartColor(idx, r, g, b [, a])` — Set a part color override.
- `:getBounds()` — Returns `minX, minY, minZ, maxX, maxY, maxZ`.
- `:getCenter()` — Returns `cx, cy, cz`.
- `:getSize()` — Returns `sx, sy, sz`.
- `:getTriangles()` — Returns a table of triangles (`{{p1},{p2},{p3}}`).
- `:isSkinned()` — Returns true if the model has skin/joint data.
- `:getJointCount()` — Returns total joints/bones.
- `:getJointName(idx)` — Returns joint name string (1-indexed).
- `:getJointIndex(name)` — Returns joint index (1-indexed) or -1.
- `:getJointNames()` — Returns array table of all joint names.
- `:getAnimationCount()` — Returns number of embedded animations.
- `:getAnimationNames()` — Returns array table of animation clip names.
- `:getAnimationDuration(name_or_idx)` — Returns duration in seconds.
- `:createAnimator()` — Creates and returns a `Graphics.Animator`.
- `:createPhysicsSkeleton()` — Creates a 1:1 `Physics3D.Skeleton` from glTF skin joints.
- `:drawSkinned(animator_or_pose, x, y, z, rx, ry, rz, sx, sy, sz, tex)` — Render skinned mesh directly.

### Skeletal Animation (`Graphics.Animator`)
Created via `model:createAnimator()` or `crayon.graphics.createAnimator(model)`.

| Method | Description |
|--------|-------------|
| `:play(clip [, loop, speed])` | Start playing an animation clip (defaults: loop=true, speed=1.0) |
| `:stop()` | Stop playback and reset time to 0 |
| `:pause()` / `:resume()` | Pause or resume playback |
| `:isPlaying()` | Returns boolean playback state |
| `:getCurrentAnimation()` | Returns current playing clip name |
| `:getTime()` / `:setTime(t)` | Get or set current playback timestamp (seconds) |
| `:getDuration()` | Returns duration of the active animation clip |
| `:setSpeed(speed)` / `:getSpeed()` | Set or get playback speed multiplier |
| `:crossFade(targetClip [, duration, loop])` | Smooth crossfade between animations (default duration: 0.2s) |
| `:blend(clipA, clipB, factor)` | 1D locomotion blend tree with synchronized phase (factor: 0.0 to 1.0) |
| `:setLayerClip(layer, clip [, loop, speed])` | Set animation clip for multi-layer evaluation |
| `:setLayerWeight(layer, weight)` | Set layer blend weight (0.0 to 1.0) |
| `:setLayerMask(layer, rootJointName [, includeChildren])` | Mask layer to specific bone hierarchy |
| `:setUpdateRate(fps)` | LOD tick throttling (0 = every frame, or target Hz like 30, 15) |
| `:update(dt)` | Advance animation state by `dt` seconds |
| `:applyToPhysicsPose(skeletonPose)` | Transfer current animated bone transforms to a `Physics3D.SkeletonPose` |
| `:capturePhysicsPose(skeletonPose)` | Transfer physics ragdoll transforms into animator |
| `:getModel()` | Returns associated Model userdata |

### 3D Drawing

| Function | Description |
|----------|-------------|
| `drawModel(id, x, y, z, rx, ry, rz, sx, sy, sz, tex)` | Draw loaded static model |
| `drawModelSkinned(model, anim_or_pose, x, y, z, rx, ry, rz, sx, sy, sz, tex)` | Draw skinned glTF model with `Animator` or `SkeletonPose` |
| `drawModelNode(model, node, x, y, z, rx, ry, rz, sx, sy, sz, tex)` | Draw a single node (index or name) |
| `drawCube(x, y, z, sx, sy, sz, tex, rx, ry, rz)` | Draw cube |
| `drawPlane(x, y, z, w, d, tex, rx, ry, rz)` | Draw plane |
| `drawSphere(x, y, z, radius, tex, rx, ry, rz)` | Draw sphere |
| `drawCylinder(x, y, z, radius, height, tex, rx, ry, rz)` | Draw cylinder |
| `drawCone(x, y, z, radius, height, tex, rx, ry, rz)` | Draw cone |
| `drawPyramid(x, y, z, baseSize, height, tex, rx, ry, rz)` | Draw pyramid |
| `drawTorus(x, y, z, radius, tube, tex, rx, ry, rz)` | Draw torus |
| `drawCapsule(x, y, z, radius, height, tex, rx, ry, rz)` | Draw capsule |
| `drawBillboard(x, y, z, w, h, tex, mode, u0, v0, u1, v1)` | Draw billboard (mode: "cylindrical" or nil for spherical) |
| `drawBillboardRot(tex, x, y, z, w, h, angle, mode, color)` | Rotated billboard |
| `drawLine3d(x1,y1,z1, x2,y2,z2)` | Draw 3D line |
| `drawLines3d(points)` | Draw multiple 3D line segments |
| `drawGrid3d(size, divs, y)` | Draw 3D grid |
| `drawTriangle3d(p1, p2, p3, tex)` | Draw 3D triangle |
| `drawQuad3d(p1, p2, p3, p4, tex)` | Draw 3D quad |
| `drawAxes3d(x, y, z, size)` | Draw coordinate axes |
| `drawCubeWires(x,y,z, sx,sy,sz, color, rx,ry,rz)` | Draw wireframe cube |
| `drawCapsuleWires(x,y,z, radius, halfH, color, rx,ry,rz)` | Draw wireframe capsule |
| `drawCylinderWires(x,y,z, radius, halfH, color, rx,ry,rz)` | Draw wireframe cylinder |
| `drawRay3d(sx,sy,sz, dx,dy,dz, length, color)` | Draw a ray |
| `drawSkeleton(positions, connections, color)` | Draw a skeleton |
| `drawSegmentedMesh(meshes, transforms, textures)` | Draw segmented mesh |

### 2D Drawing

| Function | Description |
|----------|-------------|
| `drawSprite(tex, x, y, w, h, rot, ox, oy)` | Draw sprite |
| `drawSpritePart(tex, x, y, u0, v0, u1, v1, w, h, rot, ox, oy)` | Draw sprite part |
| `drawSpriteTiled(tex, x, y, w, h, tileW, tileH, ox, oy)` | Draw tiled sprite |
| `drawSprite9slice(tex, x, y, w, h, left, top, right, bottom, texW, texH)` | Draw 9-slice sprite |
| `drawTextureRot(tex, x, y, w, h, angle, ox, oy)` | Draw rotated texture |
| `drawPoint(x, y, size)` | Draw point |
| `drawLine(x1,y1, x2,y2, thick)` | Draw line |
| `drawRect(mode, x, y, w, h, thick)` | Draw rectangle (mode: "fill" or "line") |
| `drawRoundedRect(mode, x, y, w, h, radius, segs)` | Draw rounded rectangle |
| `drawRoundedRectEx(mode, x, y, w, h, rtl, rtr, rbr, rbl, segs)` | Per-corner rounded rect |
| `drawTriangle(mode, x1,y1, x2,y2, x3,y3)` | Draw triangle |
| `drawQuad(mode, x1,y1, x2,y2, x3,y3, x4,y4)` | Draw quad |
| `drawPolygon(mode, points)` | Draw polygon (points: `{{x,y}, ...}` or flat `{x1,y1, x2,y2, ...}`) |
| `drawCircle(mode, cx, cy, radius, segs)` | Draw circle |
| `drawEllipse(mode, cx, cy, rx, ry, segs)` | Draw ellipse |
| `drawArc(mode, cx, cy, radius, a0, a1, segs)` | Draw arc |
| `drawRing(mode, cx, cy, innerR, outerR, segs)` | Draw ring |
| `drawPie(mode, cx, cy, radius, a1, a2, segs)` | Draw pie slice |
| `drawGradientRect(x, y, w, h, cTl, cTr, cBr, cBl)` | Draw gradient rect (colors as `{r,g,b,a}`) |
| `drawGradientH(x, y, w, h, cLeft, cRight)` | Horizontal gradient |
| `drawGradientV(x, y, w, h, cTop, cBottom)` | Vertical gradient |
| `drawPolyline(points, thick, loop)` | Draw polyline |
| `drawBezier(x0,y0, x1,y1, x2,y2, thick, segs)` | Draw quadratic bezier |
| `drawBezier(x0,y0, x1,y1, x2,y2, x3,y3, thick, segs)` | Draw cubic bezier |

### 2D Transforms & Camera

| Function | Description |
|----------|-------------|
| `pushMatrix2d()` | Push 2D transform matrix |
| `popMatrix2d()` | Pop 2D transform matrix |
| `translate2d(x, y)` | Translate 2D |
| `rotate2d(angle)` | Rotate 2D (radians) |
| `scale2d(sx, sy)` | Scale 2D |
| `setCamera2d(cam)` | Set 2D camera |
| `resetCamera2d()` | Reset 2D camera |

**2D Camera Table**:
```lua
{
    x = 0, y = 0,      -- Position
    zoom = 1.0,        -- Zoom factor
    angle = 0.0,       -- Rotation (radians)
    originX = 0,       -- Origin offset X
    originY = 0        -- Origin offset Y
}
```

### 3D Transforms

| Function | Description |
|----------|-------------|
| `pushMatrix()` | Push 3D transform matrix |
| `popMatrix()` | Pop 3D transform matrix |
| `translate(x, y, z)` | Translate 3D |
| `rotate(angle, ax, ay, az)` | Rotate 3D (radians) |
| `scale(sx, sy, sz)` | Scale 3D |

### Scissor

| Function | Description |
|----------|-------------|
| `setScissor(x, y, w, h)` | Set scissor rect |
| `resetScissor()` | Reset scissor |
| `pushScissor(x, y, w, h)` | Push scissor with intersection |
| `popScissor()` | Pop scissor |

### Blend Modes

| Function | Description |
|----------|-------------|
| `setBlendMode(mode)` | Set blend mode: "alpha", "additive", "multiply", "none" |

### Text

| Function | Description |
|----------|-------------|
| `drawText(text, x, y, scale)` | Draw text (8x8 monospace font) |
| `getTextWidth(text, scale)` | Get text width |
| `getTextHeight(text, scale)` | Get text height |

### 3D Helpers

| Function | Description |
|----------|-------------|
| `project(x, y, z)` | Project 3D point to screen (returns x, y, visible) |
| `unproject(sx, sy)` | Get 3D ray from screen (returns origin x,y,z, dir x,y,z) |

---

## Input Module
Access via: `crayon.input`

### Keyboard

| Function | Description |
|----------|-------------|
| `isDown(key)` | Key currently held? |
| `isPressed(key)` | Key just pressed? |
| `isReleased(key)` | Key just released? |
| `anyKeyPressed()` | Any key pressed? |
| `getPressedKeys()` | List of pressed keys |

**Key Names**: "a", "b", ..., "z", "space", "enter", "escape", "tab", "backspace", "up", "down", "left", "right", "shift", "ctrl", "alt", "gui", "f1", "f2", ..., "f12"

### Modifiers

| Function | Description |
|----------|-------------|
| `isShiftDown()` | Shift held? |
| `isCtrlDown()` | Ctrl held? |
| `isAltDown()` | Alt held? |
| `isGuiDown()` | GUI (Windows/Command) held? |
| `isCapsLock()` | Caps Lock active? |

### Mouse

| Function | Description |
|----------|-------------|
| `getMousePos()` | Returns virtual x, y |
| `getMouseWindowPos()` | Returns window x, y |
| `getMouseDelta()` | Returns mouse delta x, y |
| `isMouseDown(btn)` | Mouse button held? |
| `isMousePressed(btn)` | Mouse button just pressed? |
| `isMouseReleased(btn)` | Mouse button just released? |
| `getMouseWheel()` | Returns wheel x, y |
| `setMousePosition(x, y)` | Set mouse position |

**Button Types**: 1-5, "left"/"l"/"1", "middle"/"mid"/"m"/"2", "right"/"r"/"3", "x1"/"mouse4"/"4", "x2"/"mouse5"/"5"

### Text Input & Clipboard

| Function | Description |
|----------|-------------|
| `startTextInput()` | Start text input (IME) |
| `stopTextInput()` | Stop text input |
| `isTextInputActive()` | Text input active? |
| `getTextInput()` | Get input text |
| `getClipboard()` | Get clipboard text |
| `setClipboard(text)` | Set clipboard text |

### Gamepad

| Function | Description |
|----------|-------------|
| `gamepadIsDown(btn)` | Gamepad button held? |
| `gamepadAxis(axis)` | Get gamepad axis value (-1 to 1) |

---

## Time Module
Access via: `crayon.time`

| Function | Description |
|----------|-------------|
| `getTime()` | Total elapsed time (seconds) |
| `getDt()` | Delta time (seconds since last frame) |

---

## Physics Module
Access via: `crayon.physics3d`

### Body Creation

| Function | Description |
|----------|-------------|
| `createBox(x, y, z, hx, hy, hz [, motion, friction, restitution, density])` | Create box body |
| `createSphere(x, y, z, radius [, motion, friction, restitution, density])` | Create sphere body |
| `createCapsule(x, y, z, halfH, radius [, motion, friction, restitution, density])` | Create capsule body |
| `createCylinder(x, y, z, halfH, radius [, motion, friction, restitution, density])` | Create cylinder body |
| `createPlane(x, y, z [, nx, ny, nz, halfExtent])` | Create static plane body |
| `createMeshBody(x, y, z, model, friction, restitution)` | Create static mesh body from `Graphics.Model` |
| `createMeshBody(model, friction, restitution)` | Same, at origin |
| `createMeshBody(x, y, z, vertices, indices, friction, restitution)` | From flat vertex/indices tables |
| `createMeshBody(meshData, friction, restitution)` | `meshData = {vertices={...}, indices={...}}` |

**Motion Types**: "static", "kinematic", "dynamic" (or 0, 1, 2). Default: "dynamic".

### Body Methods

| Method | Description |
|--------|-------------|
| `body:getId()` | Returns body ID |
| `body:isValid()` | Body still exists? |
| `body:isActive()` | Body active? |
| `body:setActive(active)` | Activate/deactivate body |
| `body:destroy()` | Destroy this body |
| `body:getPosition()` | Returns x, y, z |
| `body:setPosition(x, y, z [, activate])` | Set position |
| `body:getRotation()` | Returns euler x, y, z (radians) |
| `body:setRotation(rx, ry, rz [, activate])` | Set rotation (radians) |
| `body:getVelocity()` | Returns vx, vy, vz |
| `body:setVelocity(vx, vy, vz)` | Set linear velocity |
| `body:getAngularVelocity()` | Returns wx, wy, wz |
| `body:setAngularVelocity(wx, wy, wz)` | Set angular velocity |
| `body:applyForce(fx, fy, fz)` | Apply force |
| `body:applyImpulse(ix, iy, iz)` | Apply impulse |
| `body:applyTorque(tx, ty, tz)` | Apply torque |
| `body:setGravityFactor(factor)` | Set gravity multiplier |
| `body:setFriction(friction)` | Set friction |
| `body:setRestitution(restitution)` | Set restitution |
| `body:setMotionType(type)` | Change motion type |
| `body:setDamping(linearDamping [, angularDamping])` | Set damping |
| `body:setSensor(isSensor)` | Make body a sensor (trigger) |
| `body:isSensor()` | Is body a sensor? |

### World Settings

| Function | Description |
|----------|-------------|
| `step([dt, collisionSteps])` | Manually advance physics simulation (default: 1/60s, 1 step) |
| `setGravity(gx, gy, gz)` | Set world gravity |
| `getGravity()` | Returns gx, gy, gz |
| `destroyAll()` | Destroy all bodies |
| `getBodyCount()` | Returns total bodies, active bodies |

> Note: `crayon.update(dt)` is called every frame by the runtime. Calling `crayon.physics.step(dt)` manually lets you drive the simulation yourself (useful for fixed-timestep or headless contexts).

### Raycast

| Function | Description |
|----------|-------------|
| `raycast(ox, oy, oz, dx, dy, dz, maxDist)` | Returns `hit, posX, posY, posZ, normalX, normalY, normalZ, distance, body`. When no hit, only `false` is returned. |

### Queries & Debug

| Function | Description |
|----------|-------------|
| `overlapSphere(cx, cy, cz, radius)` | Returns table of body userdata in sphere |
| `drawDebug([opts])` | Draw physics debug visualization |
| `drawDebug(r, g, b, a, sr, sg, sb, sa [, flags])` | Draw with explicit active/sleep colors |

**Debug Flags Table** (`opts` or trailing `flags` arg):
```lua
{
    shapes = true,                 -- Draw collision shapes
    softBodies = true,             -- Draw soft bodies
    constraints = true,            -- Draw constraints
    softBodyConstraints = true,    -- Draw soft body constraints
    softBodyRods = true,           -- Draw soft body rods
    bounds = false,                -- Draw bounding boxes
    velocities = false             -- Draw velocity vectors
}
```

### Constraints

| Function | Description |
|----------|-------------|
| `createPointConstraint(b1, b2, px, py, pz)` | Create point constraint |
| `createHingeConstraint(b1, b2, px, py, pz, ax, ay, az [, minAngle, maxAngle])` | Create hinge constraint |
| `createDistanceConstraint(b1, b2, p1x, p1y, p1z, p2x, p2y, p2z [, minD, maxD])` | Create distance constraint |
| `createFixedConstraint(b1, b2)` | Create fixed constraint |
| `destroyConstraint(c)` | Destroy constraint (userdata or id) |

**Constraint Methods**: `:destroy()`, `:isValid()`, `:getId()`

### Characters

| Function | Description |
|----------|-------------|
| `createCharacter(opts)` | Create a character controller |
| `createCharacterVirtual(opts)` | Create a virtual character (kinematic) |

**Character Options Table**:
```lua
{
    pos = {x, y, z},
    radius = 0.4,
    halfHeight = 0.6,           -- capsule half height
    mass = 80.0,
    friction = 0.5,
    gravityFactor = 1.0,
    maxSlopeAngleDeg = 45.0,
    motion = "dynamic"
}
```

**Virtual Character Extra Options**:
```lua
{
    maxStrength = 100.0,
    stepHeight = 0.5,
    predictiveContactDistance = 0.1,
    innerBody = true
}
```

**Character Methods**: `:getId()`, `:isValid()`, `:destroy()`, `:getBodyId()`, `:getPosition()`, `:setPosition(x,y,z)`, `:getRotation()`, `:setRotation(x,y,z,w)`, `:getLinearVelocity()`, `:setLinearVelocity(vx,vy,vz)`, `:isSupported()`, `:getGroundState()`, `:getGroundNormal()`, `:getGroundVelocity()`, `:getGroundPosition()`

**Ground states**: `"onGround"`, `"onSteepGround"`, `"notSupported"`, `"inAir"`

**Virtual Character Methods**: same as Character plus `:update(dt)`

### Vehicles

| Function | Description |
|----------|-------------|
| `createWheeledVehicle(cfg)` | Create a wheeled vehicle |
| `createTrackedVehicle(cfg)` | Create a tracked vehicle |
| `createMotorcycle(cfg)` | Create a motorcycle |

**Wheel Config** (each entry in `wheels` / `frontWheel` / `rearWheel` / `leftWheels` / `rightWheels`):
```lua
{
    position = {x, y, z},
    radius = 0.3,
    width = 0.2,
    suspensionMinLength = 0.1,
    suspensionMaxLength = 0.5,
    suspensionSpring = 1000.0,
    suspensionDamping = 50.0,
    maxSteerAngleRad = 0.5,
    maxBrakeTorque = 100.0,
    maxHandBrakeTorque = 200.0,
    isFront = true,
    isDrive = true
}
```

**Wheeled Vehicle Config**:
```lua
{
    chassis = chassisBody,         -- body userdata or id
    wheels = { wheel1, wheel2, ... },
    maxPitchRollAngle = 0.5,
    engineMaxTorque = 500.0,
    engineMinRpm = 1000.0,
    engineMaxRpm = 6000.0
}
```

**Tracked Vehicle Config**:
```lua
{
    chassis = chassisBody,
    leftWheels = { wheel1, wheel2, ... },
    rightWheels = { wheel1, wheel2, ... },
    engineMaxTorque = 500.0
}
```

**Motorcycle Config**:
```lua
{
    chassis = chassisBody,
    frontWheel = { ... },
    rearWheel = { ... },
    maxLeanAngleRad = 0.7,
    leanSpringConstant = 500.0,
    leanSpringDamping = 50.0,
    leanSmoothingFactor = 0.15,
    engineMaxTorque = 200.0
}
```

**Vehicle Methods**: `:getId()`, `:isValid()`, `:destroy()`, `:setInputWheeled(forward, steer, brake, handbrake)`, `:setInputTracked(leftRatio, rightRatio, brake)`, `:setInputMotorcycle(forward, steer, brake)`, `:enableLeanController(enabled)`, `:isLeanControllerEnabled()`, `:getLeanAngle()`, `:getSpeedKmh()`, `:getEngineRpm()`, `:getTransmissionGear()`, `:getWheelCount()`, `:getWheelTransform(idx)`

### Skeleton / Ragdoll

| Function | Description |
|----------|-------------|
| `createSkeleton(joints)` | Create a skeleton |
| `createSkeletonPose(skel)` | Create a pose for a skeleton |
| `createSkeletonMapper(skelLow, skelHigh, poseLow, poseHigh)` | Create a mapper |
| `createRagdoll(cfg)` | Create a ragdoll |

**Skeleton joint entry**:
```lua
{ name = "Spine", parentIndex = 0 }
```
*(Tip: You can also create a skeleton directly from any skinned glTF model using `local skel = model:createPhysicsSkeleton()`)*

**Skeleton Methods**: `:getId()`, `:isValid()`, `:destroy()`

**SkeletonPose Methods**: `:getId()`, `:isValid()`, `:destroy()`, `:setJoint(idx, tx, ty, tz, rx, ry, rz, rw)`, `:calculateMatrices()`, `:getJointMatrix(idx)`, `:setRootOffset(x,y,z)`, `:getRootOffset()`, `:getJointCount()`

**SkeletonMapper Methods**: `:getId()`, `:isValid()`, `:destroy()`, `:map(poseLow, poseHighLocal, poseHighOutModel)`, `:mapReverse(poseHighModel, poseLowOutModel)`, `:lockAllTranslations(skelHigh, neutralPose)`

**Ragdoll Config**:
```lua
{
    disableParentChildCollisions = false,
    stabilize = true,
    parts = {
        {
            name = "Head",
            parentJointIndex = 0,
            position = {x, y, z},
            rotation = {x, y, z, w},
            shapeType = "capsule",       -- "box", "sphere", "capsule"
            halfExtent = {hx, hy, hz},
            radius = 0.15,
            halfHeight = 0.1,
            motion = "dynamic",
            mass = 5.0,
            friction = 0.5,
            swingLimitY = 0.5,
            swingLimitZ = 0.5,
            twistMin = -0.3,
            twistMax = 0.3,
            enableMotors = true,
            motorSpringK = 100.0,
            motorDampingC = 10.0,
            motorMaxTorque = 50.0
        }
    }
}
```

**Ragdoll Methods**: `:getId()`, `:isValid()`, `:destroy()`, `:setPose(pose)`, `:driveToPoseKinematics(pose, dt)`, `:driveToPoseMotors(pose)`, `:driveToPoseMotorsVelocity(prevPose, pose, dt)`, `:getPose(pose)`, `:setHardKeying(enabled)`, `:activate()`, `:isActive()`, `:getBodyId(partIdx)`, `:getPartCount()`, `:getSkeletonId()`

### Soft Bodies

| Function | Description |
|----------|-------------|
| `createSoftBody(config)` | Create a soft body from a full config table (see below) |
| `createSoftBodyCloth(opts_or_x, y, z, w, h, segX, segY, compliance, bendCompliance, pinCorners, addLra)` | Create a cloth soft body |
| `createSoftBodyCube(opts_or_x, y, z, size, gridSize, compliance, pressure)` | Create a soft cube |
| `createSoftBodySphere(opts_or_x, y, z, radius, rings, sectors, compliance, pressure)` | Create a soft sphere |
| `createSoftBodyRod(opts)` | Create a rod from a `points` array |
| `destroySoftBody(sb_or_id)` | Destroy a soft body (userdata or id) |

**Cloth Options Table** (all optional):
```lua
{
    x = 0, y = 0, z = 0,
    width = 4.0, height = 4.0,
    segmentsX = 10, segmentsY = 10,
    compliance = 0.0,
    bendCompliance = 0.01,
    pinCorners = true,
    addLra = true
}
```

**Cube Options Table**:
```lua
{ x=0, y=0, z=0, size=2.0, gridSize=3, compliance=0.0, pressure=0.0 }
```

**Sphere Options Table**:
```lua
{ x=0, y=0, z=0, radius=1.0, rings=8, sectors=12, compliance=0.0, pressure=500.0 }
```

**Rod Options Table**:
```lua
{
    points = { {x,y,z}, {x,y,z}, ... },   -- required
    stretchCompliance = 0.0,
    bendTwistCompliance = 0.001,
    pinRoot = true
}
```

**Full Config (`createSoftBody`)**:
```lua
{
    position = {x, y, z},
    rotation = {x, y, z, w},              -- quaternion
    vertices = { {x, y, z, mass=1.0}, ... },
    faces = { {v1, v2, v3}, ... },        -- 1-indexed vertex indices
    edges = { {v1, v2, compliance=0.0}, ... },        -- or `edgeConstraints`
    bends = { {v1, v2, v3, v4, compliance}, ... },    -- or `dihedralBendConstraints`
    volumes = { {v1, v2, v3, v4, compliance}, ... },  -- or `volumeConstraints`
    tethers = { {kinematicV, dynamicV, maxDistance}, ... },  -- or `lraConstraints`
    rods = { {v1, v2, compliance}, ... },             -- or `rodStretchShearConstraints`
    rodBendTwistConstraints = { {rod0, rod1, compliance}, ... },

    pressure = 0.0,
    vertexRadius = 0.0,
    linearDamping = 0.0,
    maxLinearVelocity = 0.0,
    friction = 0.5,
    restitution = 0.2,
    gravityFactor = 1.0,
    numIterations = 3,
    updatePosition = true,
    allowSleeping = true,
    facesDoubleSided = true,

    -- Auto-constraint generation (if `faces` is set and edges omitted):
    autoGenerateConstraints = true,
    compliance = 0.0,
    shearCompliance = 0.0,
    bendCompliance = 0.0,
    lraMultiplier = 1.0,
    bendType = "dihedral",                -- "dihedral", "distance", "none"
    lraType = "euclidean"                 -- "euclidean", "geodesic", "none"
}
```

**SoftBody Methods**:

| Method | Description |
|--------|-------------|
| `:getId()` | Returns soft body ID |
| `:isValid()` | Soft body still exists? |
| `:destroy()` | Destroy this soft body |
| `:getBodyId()` | Returns underlying physics body ID |
| `:getPosition()` / `:setPosition(x,y,z)` | Get or set body position |
| `:getRotation()` / `:setRotation(x,y,z,w)` | Get or set body rotation (quaternion) |
| `:getVertexCount()` | Returns number of vertices |
| `:getVertex(idx)` | Returns `posX, posY, posZ, velX, velY, velZ, invMass` (1-indexed) |
| `:setVertex(idx, x, y, z [, vx, vy, vz [, invMass]])` | Set vertex position (and optionally velocity / inverse mass) |
| `:getVertices()` | Returns array of `{x, y, z}` |
| `:getVerticesFlat()` | Returns flat `{x,y,z, x,y,z, ...}` |
| `:getFaces()` | Returns array of `{i, j, k}` (1-indexed) |
| `:getFacesFlat()` | Returns flat `{i,j,k, i,j,k, ...}` (1-indexed) |
| `:getPressure()` / `:setPressure(p)` | Get or set pressure |
| `:getNumIterations()` / `:setNumIterations(n)` | Get or set solver iterations |
| `:getVolume()` | Returns current volume |
| `:applyImpulse(ix, iy, iz)` | Apply impulse to all vertices |
| `:applyImpulse(vertexIdx, ix, iy, iz)` | Apply impulse to one vertex (1-indexed) |
| `:applyForce(fx, fy, fz)` | Apply force to all vertices |
| `:applyForce(vertexIdx, fx, fy, fz)` | Apply force to one vertex |
| `:skinVertices(jointMatrices [, hardSkin])` | Skin vertices to a list of 4x4 matrices (16-element tables) |
| `:setSkinnedMaxDistanceMultiplier(mult)` | Set skinned max distance multiplier |
| `:getRodTransform(rodIdx)` | Returns `posX, posY, posZ, rotX, rotY, rotZ, rotW` (1-indexed) |
| `:activate()` | Wake soft body |
| `:isActive()` | Is soft body active? |

