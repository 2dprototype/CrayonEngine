# Crayon Engine - Lua API Documentation

## Overview
Crayon Engine is a 2D/3D game engine with retro aesthetics, modern physics, and Lua scripting. All engine functionality is accessed through the global `crayon` table.

## Table of Contents
1. [Time Module](#time-module)
2. [Window Module](#window-module)
3. [Input Module](#input-module)
4. [Graphics Module](#graphics-module)
5. [Physics3D Module](#physics3d-module)
6. [Physics2D Module](#physics2d-module)
7. [Lua Callbacks](#lua-callbacks)
8. [Complete Examples](#complete-examples)

---

## Time Module
Access via: `crayon.time`

| Function | Description |
|----------|-------------|
| `getTime()` | Total elapsed time (seconds) |
| `getDt()` | Delta time (seconds since last frame) |

```lua
local t = crayon.time.getTime()
local dt = crayon.time.getDt()
```

---

## Window Module
Access via: `crayon.window`

### Resolution & Size

| Function | Description |
|----------|-------------|
| `setResolution(w, h)` | Set virtual resolution (default: 320x240) |
| `getResolution()` | Returns virtual width, height |
| `setWindowSize(w, h)` | Set physical window size |
| `getWindowSize()` | Returns window width, height |
| `setMinSize(w, h)` | Set minimum window size |
| `setMaxSize(w, h)` | Set maximum window size |
| `getDisplaySize()` | Returns primary monitor width, height |

### Position

| Function | Description |
|----------|-------------|
| `setPosition(x, y)` | Set window position on screen |
| `getPosition()` | Returns window x, y |
| `center()` | Center window on screen |

### Display State

| Function | Description |
|----------|-------------|
| `setFullscreen(enabled)` | Toggle fullscreen |
| `isFullscreen()` | Returns fullscreen state |
| `setVsync(enabled)` | Toggle VSync |
| `getVsync()` | Returns VSync state |
| `setResizable(enabled)` | Allow/disable resizing |
| `isResizable()` | Returns resizable state |
| `setBordered(enabled)` | Show/hide window borders |
| `isBordered()` | Returns bordered state |
| `maximize()` | Maximize window |
| `minimize()` | Minimize window |
| `restore()` | Restore window from min/max |
| `isMaximized()` | Returns maximized state |
| `isMinimized()` | Returns minimized state |
| `isFocused()` | Returns focused state |
| `raise()` | Raise window to front |
| `focus()` | Focus window |
| `flash()` | Flash taskbar icon |

### Title

| Function | Description |
|----------|-------------|
| `setTitle(title)` | Set window title |
| `getTitle()` | Returns window title |

### Scaling

| Function | Description |
|----------|-------------|
| `setScalingMode(mode)` | Set scaling: "integer", "aspect", "stretch", "center" |
| `getScalingMode()` | Returns current scaling mode |

### Opacity & Layering

| Function | Description |
|----------|-------------|
| `setOpacity(opacity)` | Set window opacity (0.0–1.0) |
| `getOpacity()` | Returns window opacity |
| `setAlwaysOnTop(enabled)` | Keep window on top |
| `isAlwaysOnTop()` | Returns always-on-top state |
| `setTransparent(enabled)` | Enable transparent window |
| `isTransparent()` | Returns transparency state |

### Mouse & Cursor

| Function | Description |
|----------|-------------|
| `setMouseGrab(enabled)` | Grab mouse to window |
| `isMouseGrabbed()` | Returns mouse grab state |
| `setMouseRelative(enabled)` | Enable relative mouse mode |
| `isMouseRelative()` | Returns relative mouse state |
| `showCursor(show)` | Show/hide cursor |
| `isCursorVisible()` | Returns cursor visibility |

### Misc

| Function | Description |
|----------|-------------|
| `getFps()` | Returns current FPS |
| `quit()` | Quit the engine |

### Window Examples

```lua
crayon.window.setResolution(640, 360)
crayon.window.setWindowSize(1280, 720)
crayon.window.setTitle("My Game")
crayon.window.setVsync(true)
crayon.window.setScalingMode("integer")

local w, h = crayon.window.getDisplaySize()
print("Display:", w, "x", h)
```

---

## Input Module

> **Note:** The API is split into three sub-tables: `crayon.key`, `crayon.mouse`, and `crayon.gamepad`. There is **no** `crayon.input` table.

### `crayon.key`

| Function | Description |
|----------|-------------|
| `isDown(...keys)` | Returns true if **any** listed key is held |
| `isPressed(...keys)` | Returns true if **any** listed key was just pressed |
| `isReleased(...keys)` | Returns true if **any** listed key was just released |
| `isScancodeDown(...codes)` | Returns true if any scancode is held |
| `anyPressed()` | Returns true if any key was just pressed |
| `anyDown()` | Returns true if any key is held |
| `getPressedKeys()` | Returns array of key names pressed this frame |
| `getDownKeys()` | Returns array of key names currently held |
| `isShiftDown()` | Shift held? |
| `isCtrlDown()` | Ctrl held? |
| `isAltDown()` | Alt held? |
| `isGuiDown()` | GUI (Windows/Command) held? |
| `isCapsLock()` | Caps Lock active? |

**Text Input & Clipboard:**

| Function | Description |
|----------|-------------|
| `setTextInput([enabled])` | Start/stop text input (IME). Default: true |
| `hasTextInput()` | Returns true if text input is active |
| `getTextInput()` | Get current text input buffer |
| `getClipboard()` | Get clipboard text |
| `setClipboard(text)` | Set clipboard text |

**Key Names**: `"a"`–`"z"`, `"space"`, `"enter"`, `"escape"`, `"tab"`, `"backspace"`, `"up"`, `"down"`, `"left"`, `"right"`, `"shift"`, `"ctrl"`, `"alt"`, `"gui"`, `"f1"`–`"f12"`.

### `crayon.mouse`

| Function | Description |
|----------|-------------|
| `isDown(...buttons)` | Any listed button held? |
| `isPressed(...buttons)` | Any listed button just pressed? |
| `isReleased(...buttons)` | Any listed button just released? |
| `getPosition()` | Returns virtual x, y |
| `getX()` / `getY()` | Returns virtual x or y |
| `getDelta()` | Returns mouse delta x, y |
| `getDeltaX()` / `getDeltaY()` | Returns delta x or y |
| `getWindowPosition()` | Returns window x, y |
| `getWindowX()` / `getWindowY()` | Returns window x or y |
| `getWindowDelta()` | Returns window delta x, y |
| `getWheel()` | Returns wheel x, y |
| `getWheelX()` / `getWheelY()` | Returns wheel x or y |
| `setPosition(x, y)` | Set mouse virtual position |
| `setWindowPosition(x, y)` | Set mouse window position |
| `setVisible(visible)` | Show/hide cursor |
| `isVisible()` | Cursor visible? |
| `setGrabbed(grab)` | Grab mouse to window |
| `isGrabbed()` | Mouse grabbed? |
| `setRelativeMode(enabled)` | Enable relative mode |
| `isRelativeMode()` | Relative mode active? |

**Button Types**: `1`–`5`, `"left"`/`"l"`/`"1"`, `"middle"`/`"mid"`/`"m"`/`"2"`, `"right"`/`"r"`/`"3"`, `"x1"`/`"mouse4"`/`"4"`, `"x2"`/`"mouse5"`/`"5"`.

### `crayon.gamepad`

| Function | Description |
|----------|-------------|
| `isDown(button)` | Gamepad button held? |
| `isPressed(button)` | Gamepad button just pressed? |
| `isReleased(button)` | Gamepad button just released? |
| `getAxis(axis)` | Get axis value (-1 to 1) |
| `isConnected([idx])` | Gamepad connected? Default idx: 0 |
| `getName([idx])` | Gamepad name string |
| `getCount()` | Number of connected gamepads |

### Input Examples

```lua
-- Keyboard
if crayon.key.isPressed("space") then jump() end
if crayon.key.isDown("w", "up") then moveUp() end
if crayon.key.isShiftDown() then run() end

-- Mouse
local mx, my = crayon.mouse.getPosition()
local dx, dy = crayon.mouse.getDelta()
if crayon.mouse.isPressed(1) then shoot() end
local wx, wy = crayon.mouse.getWheel()

-- Gamepad
if crayon.gamepad.isDown(0) then fire() end
local lx = crayon.gamepad.getAxis("leftx")
```

---

## Graphics Module
Access via: `crayon.graphics`

### Color & Canvas

| Function | Description |
|----------|-------------|
| `clear(r, g, b [, a])` | Clear screen with color (0–1) |
| `setColor(r, g, b [, a])` | Set active drawing color (0–1) |

### Retro Effects

| Function | Description |
|----------|-------------|
| `setRetroEffects(opts)` | Apply retro visual effects |

**Options Table:**
```lua
{
    jitterResolution = {160, 120},  -- Pixel snap resolution, or nil/false to disable
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
| `getCameraRay(sx, sy)` | Returns `{origin={x,y,z}, direction={x,y,z}}` for screen pos |

**Camera Table:**
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

Alternative: `setCamera3d(x, y, z [, yaw, pitch, fov])` — sets position and orients using yaw/pitch in degrees.

### Lighting

| Function | Description |
|----------|-------------|
| `setLight(dx, dy, dz [, lr, lg, lb, ar, ag, ab])` | Set directional light |
| `setPointLight(idx, x, y, z [, r, g, b, radius, intensity])` | Set point light (idx 0-based or 1-based accepted) |
| `setPointLightEnabled(idx, enabled)` | Enable/disable point light |
| `setShadingMode(mode)` | Set shading: "gouraud", "flat", "unlit" |

### Textures

| Function | Description |
|----------|-------------|
| `loadTexture(path)` | Load texture, returns `Graphics.Texture` userdata |
| `getTextureSize(tex_or_id)` | Returns width, height |
| `getWhiteTexture()` | Returns white 1x1 texture userdata |

**Texture Methods**: `:getSize()`, `:getWidth()`, `:getHeight()`, `:getId()`, `:isValid()`

### Models & Meshes

| Function | Description |
|----------|-------------|
| `loadModel(name)` | Load/create model. Primitives or file path (`.obj`, `.gltf`, `.glb`) |
| `createMesh(data)` | Create custom mesh from `{vertices, indices}` |
| `createAnimator(model)` | Create skeletal `Graphics.Animator` from a skinned model |

**Primitive Names**: `"cube"`, `"plane"`, `"sphere"`, `"cylinder"`, `"cone"`, `"pyramid"`, `"torus"`, `"capsule"`, `"grid"`.

**Model Methods:**
- `:isValid()`
- `:getNodeCount()` / `:getNode(idx_or_name)` / `:getNodes()`
- `:getPartCount()` / `:getPartName(idx)` → name, material_name
- `:getPartTexture(idx)` / `:setPartTexture(idx, tex)` / `:setPartColor(idx, r, g, b [, a])`
- `:getBounds()` → minX, minY, minZ, maxX, maxY, maxZ
- `:getCenter()` → cx, cy, cz
- `:getSize()` → sx, sy, sz
- `:getTriangles()` → table of `{{p1},{p2},{p3}}`
- `:isSkinned()` / `:getJointCount()` / `:getJointName(idx)` / `:getJointIndex(name)` / `:getJointNames()`
- `:getAnimationCount()` / `:getAnimationNames()` / `:getAnimationDuration(name_or_idx)`
- `:createAnimator()` → `Graphics.Animator`
- `:createPhysicsSkeleton()` → `Physics3D.Skeleton`
- `:drawSkinned(animator_or_pose, x, y, z, rx, ry, rz, sx, sy, sz, tex)`

### Skeletal Animation (`Graphics.Animator`)

| Method | Description |
|--------|-------------|
| `:play(clip [, loop, speed])` | Start clip (defaults: loop=true, speed=1.0) |
| `:stop()` | Stop and reset time |
| `:pause()` / `:resume()` | Pause / resume |
| `:isPlaying()` | Playback state |
| `:getCurrentAnimation()` | Current clip name |
| `:getTime()` / `:setTime(t)` | Playback timestamp |
| `:getDuration()` | Duration of active clip |
| `:setSpeed(s)` / `:getSpeed()` | Speed multiplier |
| `:crossFade(targetClip [, duration, loop])` | Crossfade (default duration: 0.2s) |
| `:crossFadeFromCurrentPose(targetClip [, duration, loop])` | Crossfade preserving pose |
| `:blend(clipA, clipB, factor)` | 1D locomotion blend (factor 0–1) |
| `:setLayerClip(layer, clip [, loop, speed])` | Set layer clip |
| `:setLayerWeight(layer, weight)` | Set layer weight |
| `:setLayerMask(layer, rootJointName [, includeChildren])` | Bone hierarchy mask |
| `:setUpdateRate(fps)` | LOD tick throttling |
| `:update(dt)` | Advance by dt seconds |
| `:applyToPhysicsPose(pose [, x,y,z, qx,qy,qz,qw])` | Transfer to `SkeletonPose`. Returns bool |
| `:capturePhysicsPose(pose)` | Transfer from pose. Returns bool |
| `:getModel()` | Associated Model |

### 3D Drawing

| Function | Description |
|----------|-------------|
| `drawModel(model, x, y, z, rx, ry, rz, sx, sy, sz, tex)` | Draw static model |
| `drawModelSkinned(model, anim_or_pose, x, y, z, rx, ry, rz, sx, sy, sz, tex)` | Draw skinned model |
| `drawModelNode(model, node, x, y, z, rx, ry, rz, sx, sy, sz, tex)` | Draw single node |
| `drawCube(x, y, z, sx, sy, sz, tex, rx, ry, rz)` | Draw cube |
| `drawPlane(x, y, z, w, d, tex, rx, ry, rz)` | Draw plane |
| `drawSphere(x, y, z, radius, tex, rx, ry, rz)` | Draw sphere |
| `drawCylinder(x, y, z, radius, height, tex, rx, ry, rz)` | Draw cylinder |
| `drawCone(x, y, z, radius, height, tex, rx, ry, rz)` | Draw cone |
| `drawPyramid(x, y, z, baseSize, height, tex, rx, ry, rz)` | Draw pyramid |
| `drawTorus(x, y, z, radius, tube, tex, rx, ry, rz)` | Draw torus |
| `drawCapsule(x, y, z, radius, height, tex, rx, ry, rz)` | Draw capsule |
| `drawBillboard(x, y, z, w, h, tex, mode, u0, v0, u1, v1)` | Billboard ("cylindrical" or nil) |
| `drawBillboardRot(tex, x, y, z, w, h, angle, mode, color)` | Rotated billboard |
| `drawLine3d(x1, y1, z1, x2, y2, z2)` | 3D line |
| `drawLines3d(points)` | Multiple 3D segments |
| `drawGrid3d([size, divs, y])` | 3D grid |
| `drawTriangle3d(p1, p2, p3 [, tex])` | 3D triangle |
| `drawQuad3d(p1, p2, p3, p4 [, tex])` | 3D quad |
| `drawAxes3d(x, y, z, size)` | Coordinate axes |
| `drawCubeWires(x, y, z, sx, sy, sz [, color, rx, ry, rz])` | Wireframe cube |
| `drawCapsuleWires(x, y, z, radius, halfH [, color, rx, ry, rz])` | Wireframe capsule |
| `drawCylinderWires(x, y, z, radius, halfH [, color, rx, ry, rz])` | Wireframe cylinder |
| `drawRay3d(sx, sy, sz, dx, dy, dz, length, color)` | Draw ray |
| `drawSkeleton(positions, connections [, color])` | Draw skeleton |
| `drawSegmentedMesh(meshes, transforms [, textures])` | Draw segmented mesh |
| `project(x, y, z)` | Returns sx, sy, visible |
| `unproject(sx, sy)` | Returns ox, oy, oz, dx, dy, dz |

### 2D Drawing

| Function | Description |
|----------|-------------|
| `drawSprite(tex, x, y, w, h, rot, ox, oy)` | Draw sprite |
| `drawSpritePart(tex, x, y, u0, v0, u1, v1, w, h, rot, ox, oy)` | Draw sprite part |
| `drawSpriteTiled(tex, x, y, w, h, tileW, tileH, ox, oy)` | Tiled sprite |
| `drawSprite9slice(tex, x, y, w, h, left, top, right, bottom [, texW, texH])` | 9-slice sprite |
| `drawTextureRot(tex, x, y, w, h, angle [, ox, oy])` | Rotated texture |
| `drawPoint(x, y, size)` | Draw point |
| `drawLine(x1, y1, x2, y2, thick)` | Draw line |
| `drawRect(mode, x, y, w, h [, thick])` | Rect ("fill" or "line") |
| `drawRoundedRect(mode, x, y, w, h, radius [, segs])` | Rounded rect |
| `drawRoundedRectEx(mode, x, y, w, h, rtl, rtr, rbr, rbl [, segs])` | Per-corner rounded |
| `drawTriangle(mode, x1, y1, x2, y2, x3, y3)` | Triangle |
| `drawQuad(mode, x1, y1, x2, y2, x3, y3, x4, y4)` | Quad |
| `drawPolygon(mode, points)` | Polygon (`{{x,y},...}` or flat) |
| `drawPolyline(points, thick, loop)` | Polyline |
| `drawCircle(mode, cx, cy, r [, segs])` | Circle |
| `drawEllipse(mode, cx, cy, rx, ry [, segs])` | Ellipse |
| `drawArc(mode, cx, cy, r, a0, a1 [, segs])` | Arc |
| `drawRing(mode, cx, cy, innerR, outerR [, segs])` | Ring |
| `drawPie(mode, cx, cy, r, a1, a2 [, segs])` | Pie slice |
| `drawBezier(x0, y0, x1, y1, x2, y2 [, x3, y3, thick, segs])` | Quadratic or cubic |
| `drawGradientRect(x, y, w, h, cTl, cTr, cBr, cBl)` | 4-corner gradient |
| `drawGradientH(x, y, w, h, cLeft, cRight)` | Horizontal gradient |
| `drawGradientV(x, y, w, h, cTop, cBottom)` | Vertical gradient |
| `drawText(text, x, y, scale)` | Draw text |
| `getTextWidth(text, scale)` | Text width |
| `getTextHeight(text, scale)` | Text height |

### State

| Function | Description |
|----------|-------------|
| `setBlendMode(mode)` | "alpha", "additive", "multiply", "none" |
| `setScissor(x, y, w, h)` / `resetScissor()` | Scissor rect |
| `pushScissor(x, y, w, h)` / `popScissor()` | Scissor stack |
| `setCamera2d(cam)` / `resetCamera2d()` | 2D camera |

**2D Camera Table:**
```lua
{ x = 0, y = 0, zoom = 1.0, angle = 0.0, originX = 0, originY = 0 }
```

### Transform Stacks

**3D:** `pushMatrix()`, `popMatrix()`, `translate(x,y,z)`, `rotate(angle, ax,ay,az)`, `scale(sx,sy,sz)`

**2D:** `pushMatrix2d()`, `popMatrix2d()`, `translate2d(x,y)`, `rotate2d(angle)`, `scale2d(sx,sy)`

---

## Physics3D Module
Access via: `crayon.physics3d`

### Body Creation

| Function | Description |
|----------|-------------|
| `createBox(x, y, z, hx, hy, hz [, motion, friction, restitution, density])` | Box body |
| `createSphere(x, y, z, radius [, motion, friction, restitution, density])` | Sphere body |
| `createCapsule(x, y, z, halfH, radius [, motion, friction, restitution, density])` | Capsule body |
| `createCylinder(x, y, z, halfH, radius [, motion, friction, restitution, density])` | Cylinder body |
| `createPlane(x, y, z [, nx, ny, nz, halfExtent])` | Static plane |
| `createMeshBody(x, y, z, model, friction, restitution)` | Mesh from Model |
| `createMeshBody(model, friction, restitution)` | Mesh at origin |
| `createMeshBody(x, y, z, vertices, indices, friction, restitution)` | From flat tables |
| `createMeshBody(meshData, friction, restitution)` | `meshData = {vertices={...}, indices={...}}` |

**Motion Types**: `"static"`, `"kinematic"`, `"dynamic"` (or `0`, `1`, `2`). Default: `"dynamic"`.

### Body Methods

| Method | Description |
|--------|-------------|
| `:getId()` | Returns body ID |
| `:isValid()` | Body still exists? |
| `:isActive()` / `:setActive(active)` | Active state |
| `:destroy()` | Destroy body |
| `:getPosition()` / `:setPosition(x, y, z [, activate])` | Position |
| `:getRotation()` / `:setRotation(rx, ry, rz [, activate])` | Euler rotation (radians) |
| `:getVelocity()` / `:setVelocity(vx, vy, vz)` | Linear velocity |
| `:getAngularVelocity()` / `:setAngularVelocity(wx, wy, wz)` | Angular velocity |
| `:applyForce(fx, fy, fz)` | Apply force |
| `:applyImpulse(ix, iy, iz)` | Apply impulse |
| `:applyTorque(tx, ty, tz)` | Apply torque |
| `:setGravityFactor(f)` | Gravity multiplier |
| `:setFriction(f)` / `:setRestitution(r)` | Physics material |
| `:setMotionType(type)` | Change motion type |
| `:setDamping(linear [, angular])` | Set damping |
| `:setSensor(bool)` / `:isSensor()` | Sensor (trigger) mode |

### World Operations

| Function | Description |
|----------|-------------|
| `step([dt, collisionSteps])` | Advance simulation (default: 1/60s, 1 step) |
| `setGravity(gx, gy, gz)` / `getGravity()` | World gravity |
| `destroyAll()` | Destroy all bodies |
| `getBodyCount()` | Returns total, active body count |
| `getBody(id)` | Returns Body or nil |
| `raycast(ox, oy, oz, dx, dy, dz [, maxDist])` | Returns `hit, px,py,pz, nx,ny,nz, dist, body` |
| `overlapSphere(cx, cy, cz, radius)` | Array of bodies in sphere |
| `drawDebug([opts])` or `drawDebug(r,g,b,a, sr,sg,sb,sa [, flags])` | Debug visualization |

**Debug Flags:**
```lua
{
    shapes = true, softBodies = true, constraints = true,
    softBodyConstraints = true, softBodyRods = true,
    bounds = false, velocities = false
}
```

### Constraints

| Function | Description |
|----------|-------------|
| `createPointConstraint(b1, b2, px, py, pz)` | Point constraint |
| `createHingeConstraint(b1, b2, px, py, pz, ax, ay, az [, minAngle, maxAngle])` | Hinge |
| `createDistanceConstraint(b1, b2, p1x, p1y, p1z, p2x, p2y, p2z [, minD, maxD])` | Distance |
| `createFixedConstraint(b1, b2)` | Fixed |
| `destroyConstraint(c)` | Destroy (userdata or id) |

**Constraint Methods**: `:destroy()`, `:isValid()`, `:getId()`

### Characters

| Function | Description |
|----------|-------------|
| `createCharacter(opts)` | Create character controller |
| `createCharacterVirtual(opts)` | Create kinematic virtual character |

**Character Options:**
```lua
{
    pos = {x, y, z},
    radius = 0.4,
    halfHeight = 0.6,
    mass = 80.0,
    friction = 0.5,
    gravityFactor = 1.0,
    maxSlopeAngleDeg = 45.0,
    motion = "dynamic"
}
```

**Virtual Character Extra Options:**
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
| `createWheeledVehicle(cfg)` | Wheeled vehicle |
| `createTrackedVehicle(cfg)` | Tracked vehicle |
| `createMotorcycle(cfg)` | Motorcycle |

**Wheel Config:**
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

**Wheeled Vehicle Config:**
```lua
{
    chassis = body,
    wheels = { wheel1, wheel2, ... },
    maxPitchRollAngle = 0.5,
    engineMaxTorque = 500.0,
    engineMinRpm = 1000.0,
    engineMaxRpm = 6000.0
}
```

**Tracked Vehicle Config:**
```lua
{
    chassis = body,
    leftWheels = { ... },
    rightWheels = { ... },
    engineMaxTorque = 500.0
}
```

**Motorcycle Config:**
```lua
{
    chassis = body,
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

### Skeleton / Pose / Mapper

| Function | Description |
|----------|-------------|
| `createSkeleton(joints)` | Create skeleton from `{{name, parentIndex}, ...}` or `{{name="X", parentIndex=N}, ...}` |
| `createSkeletonPose(skel)` | Create pose |
| `createSkeletonMapper(skelLow, skelHigh, poseLow, poseHigh)` | Create mapper |
| `createRagdoll(cfg)` | Create ragdoll |

**Skeleton Methods**: `:getId()`, `:isValid()`, `:destroy()`

**SkeletonPose Methods**: `:getId()`, `:isValid()`, `:destroy()`, `:setJoint(idx, tx, ty, tz, rx, ry, rz, rw)`, `:calculateMatrices()`, `:getJointMatrix(idx)` → 16-element array, `:setRootOffset(x,y,z)`, `:getRootOffset()`, `:getJointCount()`

**SkeletonMapper Methods**: `:getId()`, `:isValid()`, `:destroy()`, `:map(poseLow, poseHighLocal, poseHighOutModel)`, `:mapReverse(poseHighModel, poseLowOutModel)`, `:lockAllTranslations(skelHigh, neutralPose)`

### Ragdoll

**Ragdoll Config:**
```lua
{
    disableParentChildCollisions = false,
    stabilize = true,
    parts = {
        {
            name = "Head",
            parentJointIndex = 0,
            position = {x, y, z},
            rotation = {x, y, z, w},        -- quaternion
            shapeType = "capsule",          -- "box", "sphere", "capsule"
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

**Ragdoll Methods**:
- Core: `:getId()`, `:isValid()`, `:destroy()`, `:activate()`, `:isActive()`, `:getPartCount()`, `:getSkeletonId()`
- Pose: `:setPose(pose)`, `:getPose(pose)`, `:setHardKeying(enabled)`
- Driving: `:driveToPoseKinematics(pose, dt)`, `:driveToPoseMotors(pose)`, `:driveToPoseMotorsVelocity(prevPose, pose, dt)`
- Velocities: `:setLinearVelocity(x,y,z)`, `:setLinearAndAngularVelocity(lx,ly,lz, ax,ay,az)`, `:addLinearVelocity(x,y,z)`, `:addImpulse(x,y,z)`, `:addImpulseToPart(partIdx, x,y,z)`, `:addImpulseToPartAtPos(partIdx, ix,iy,iz, px,py,pz)`, `:resetWarmStart()`
- Transforms: `:getRootTransform()` → px,py,pz, qx,qy,qz,qw; `:getGroundOrientation()` → "back"/"belly"/"unknown"; `:getBounds()` → 6 values
- Parts: `:getBodyId(partIdx)`, `:getPartPosition(i)`, `:getPartRotation(i)`, `:getPartLinearVelocity(i)`, `:getLinearVelocity()`, `:getBody(i)`
- Motors: `:setMotorsStiffness(k [, c, maxTorque])`, `:setPartMotor(i, k [, c, maxTorque])`, `:setPartMotionType(i, motion)`, `:setPartFriction(i, f)`, `:setPartRestitution(i, r)`

### Soft Bodies

| Function | Description |
|----------|-------------|
| `createSoftBody(config)` | Full config soft body |
| `createSoftBodyCloth(opts_or_x, y, z, w, h, segX, segY, compliance, bendCompliance, pinCorners, addLra)` | Cloth |
| `createSoftBodyCube(opts_or_x, y, z, size, gridSize, compliance, pressure)` | Soft cube |
| `createSoftBodySphere(opts_or_x, y, z, radius, rings, sectors, compliance, pressure)` | Soft sphere |
| `createSoftBodyRod(opts)` | Rod from points |
| `destroySoftBody(sb_or_id)` | Destroy (userdata or id) |

**Cloth Options:**
```lua
{ x=0, y=0, z=0, width=4.0, height=4.0, segmentsX=10, segmentsY=10,
  compliance=0.0, bendCompliance=0.01, pinCorners=true, addLra=true }
```

**Cube Options:** `{ x=0, y=0, z=0, size=2.0, gridSize=3, compliance=0.0, pressure=0.0 }`

**Sphere Options:** `{ x=0, y=0, z=0, radius=1.0, rings=8, sectors=12, compliance=0.0, pressure=500.0 }`

**Rod Options:**
```lua
{
    points = { {x,y,z}, {x,y,z}, ... },   -- required
    stretchCompliance = 0.0,
    bendTwistCompliance = 0.001,
    pinRoot = true
}
```

**Full Config (`createSoftBody`):**
```lua
{
    position = {x, y, z},
    rotation = {x, y, z, w},              -- quaternion
    vertices = { {x, y, z, mass=1.0}, ... },
    faces = { {v1, v2, v3}, ... },        -- 1-indexed
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

    autoGenerateConstraints = true,
    compliance = 0.0,
    shearCompliance = 0.0,
    bendCompliance = 0.0,
    lraMultiplier = 1.0,
    bendType = "dihedral",                -- "dihedral", "bend", "none"
    lraType = "euclidean"                 -- "euclidean", "geodesic", "none"
}
```

**SoftBody Methods:**

| Method | Description |
|--------|-------------|
| `:getId()` / `:isValid()` / `:destroy()` | Identity & lifetime |
| `:getBodyId()` | Underlying physics body ID |
| `:getPosition()` / `:setPosition(x,y,z)` | Body position |
| `:getRotation()` / `:setRotation(x,y,z,w)` | Body rotation (quaternion) |
| `:getVertexCount()` | Vertex count |
| `:getVertex(idx)` | `px, py, pz, vx, vy, vz, invMass` (1-indexed) |
| `:setVertex(idx, x, y, z [, vx, vy, vz [, invMass]])` | Set vertex data |
| `:getVertices()` / `:getVerticesFlat()` | Vertex positions |
| `:getFaces()` / `:getFacesFlat()` | Faces (1-indexed) |
| `:getPressure()` / `:setPressure(p)` | Pressure |
| `:getNumIterations()` / `:setNumIterations(n)` | Solver iterations |
| `:getVolume()` | Current volume |
| `:applyImpulse(ix, iy, iz)` | Impulse to all vertices |
| `:applyImpulse(vertexIdx, ix, iy, iz)` | Impulse to one vertex |
| `:applyForce(fx, fy, fz)` | Force to all vertices |
| `:applyForce(vertexIdx, fx, fy, fz)` | Force to one vertex |
| `:skinVertices(jointMatrices [, hardSkin])` | Skin to 16-element matrices |
| `:setSkinnedMaxDistanceMultiplier(mult)` | Skinned distance multiplier |
| `:getRodTransform(rodIdx)` | `px, py, pz, qx, qy, qz, qw` |
| `:activate()` / `:isActive()` | Wake state |

---

## Physics2D Module
Access via: `crayon.physics2d`

| Function | Description |
|----------|-------------|
| `createBody([x, y, mass])` | Create 2D body |

**Body Methods**: `:getPosition()`, `:setPosition(x,y)`, `:getVelocity()`, `:setVelocity(vx,vy)`, `:getAngle()`, `:setAngle(a)`, `:applyForce(fx,fy)`, `:isValid()`, `:destroy()`

---

## Lua Callbacks

The runtime invokes these optional globals or `crayon.*` functions:

| Callback | Args |
|----------|------|
| `crayon.init()` | — |
| `crayon.update(dt)` | dt |
| `crayon.draw()` | — |
| `crayon.keydown(key, isRepeat)` or `crayon.keypressed` | key, is_repeat |
| `crayon.keyup(key)` or `crayon.keyreleased` | key |
| `crayon.mousedown(x, y, button)` or `crayon.mousepressed` | x, y, btn |
| `crayon.mouseup(x, y, button)` or `crayon.mousereleased` | x, y, btn |
| `crayon.mousemoved(x, y, dx, dy)` | x, y, dx, dy |
| `crayon.wheelmoved(dx, dy)` | dx, dy |
| `crayon.textinput(text)` | text |
| `crayon.gamepaddown(btn)` or `crayon.gamepadpressed` | btn |
| `crayon.gamepadup(btn)` or `crayon.gamepadreleased` | btn |
| `crayon.gamepadaxis(axis, value)` | axis, value |

---

## Complete Examples

### Example 1: Minimal 2D Game

```lua
local player = { x = 160, y = 120, speed = 100 }

function crayon.init()
    crayon.window.setTitle("2D Game")
    crayon.window.setResolution(320, 240)
    crayon.window.setScalingMode("integer")
end

function crayon.update(dt)
    local dx, dy = 0, 0
    if crayon.key.isDown("a", "left") then dx = dx - 1 end
    if crayon.key.isDown("d", "right") then dx = dx + 1 end
    if crayon.key.isDown("w", "up") then dy = dy - 1 end
    if crayon.key.isDown("s", "down") then dy = dy + 1 end
    player.x = player.x + dx * player.speed * dt
    player.y = player.y + dy * player.speed * dt
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.15)
    crayon.graphics.setColor(1, 0.8, 0.2)
    crayon.graphics.drawRect("fill", player.x - 8, player.y - 8, 16, 16)
    crayon.graphics.setColor(1, 1, 1)
    crayon.graphics.drawText("FPS: " .. math.floor(crayon.window.getFps()), 4, 4, 1)
end
```

### Example 2: 3D Scene with Physics

```lua
local cam = { yaw = 0, pitch = -20, dist = 10 }
local ground, ball

function crayon.init()
    crayon.window.setResolution(640, 360)
    crayon.graphics.setRetroEffects({
        jitterResolution = {320, 180},
        fog = { startDist = 10, endDist = 50, color = {0.1, 0.1, 0.15} },
        crt = { scanlines = 0.2, vignette = 0.15 }
    })

    ground = crayon.physics3d.createPlane(0, 0, 0)
    ball = crayon.physics3d.createSphere(0, 5, 0, 0.5)
end

function crayon.update(dt)
    crayon.physics3d.step(dt)
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.15)

    local px, py, pz = ball:getPosition()
    crayon.graphics.setCamera3d(0, py + 4, 10, -20, 0, 60)
    crayon.graphics.setLight(0.5, -1, 0.3)

    crayon.graphics.setColor(0.3, 0.4, 0.5)
    crayon.graphics.drawPlane(0, 0, 0, 20, 20, 0)

    crayon.graphics.setColor(1, 0.3, 0.3)
    crayon.graphics.drawSphere(px, py, pz, 0.5, 0)
end
```

### Example 3: Raycast Picking

```lua
function crayon.update(dt)
    if crayon.mouse.isPressed(1) then
        local mx, my = crayon.mouse.getPosition()
        local ray = crayon.graphics.getCameraRay(mx, my)
        local o, d = ray.origin, ray.direction

        local hit, px, py, pz, nx, ny, nz, dist, body =
            crayon.physics3d.raycast(o.x, o.y, o.z, d.x, d.y, d.z, 1000)

        if hit then
            print("Hit at", px, py, pz, "distance:", dist)
        end
    end
end
```

### Example 4: Skeletal Animation

```lua
local model, animator, physics_skel, pose

function crayon.init()
    model = crayon.graphics.loadModel("character.glb")
    animator = model:createAnimator()
    animator:play("Idle", true)

    physics_skel = model:createPhysicsSkeleton()
    pose = crayon.physics3d.createSkeletonPose(physics_skel)
end

function crayon.update(dt)
    animator:update(dt)
    animator:applyToPhysicsPose(pose)
    pose:calculateMatrices()
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.15)
    crayon.graphics.setCamera3d({ position = {0, 2, 5}, target = {0, 1, 0}, fov = 60 })
    crayon.graphics.drawModelSkinned(model, animator, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0)
end
```

### Example 5: Cloth Simulation

```lua
local cloth

function crayon.init()
    crayon.window.setResolution(640, 360)
    cloth = crayon.physics3d.createSoftBodyCloth({
        x = 0, y = 5, z = 0,
        width = 4, height = 4,
        segmentsX = 12, segmentsY = 12,
        compliance = 0.0,
        bendCompliance = 0.01,
        pinCorners = true
    })
end

function crayon.update(dt)
    crayon.physics3d.step(dt)
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.15)
    crayon.graphics.setCamera3d({ position = {0, 4, 8}, target = {0, 3, 0}, fov = 60 })

    local verts = cloth:getVertices()
    local faces = cloth:getFaces()
    crayon.graphics.setColor(0.8, 0.3, 0.4)

    for _, f in ipairs(faces) do
        local p1, p2, p3 = verts[f[1]], verts[f[2]], verts[f[3]]
        crayon.graphics.drawTriangle3d(p1, p2, p3)
    end
end
```
