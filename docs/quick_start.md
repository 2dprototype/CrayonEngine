# Crayon Engine - Lua API Documentation

## Overview
Crayon Engine is a 2D/3D game engine with retro aesthetics, modern physics, and Lua scripting. All engine functionality is accessed through the global `crayon` table.

## Table of Contents
1. [Window Module](#window-module)
2. [Graphics Module](#graphics-module)
3. [Input Module](#input-module)
4. [Time Module](#time-module)
5. [Physics Module](#physics-module)
6. [Complete Examples](#complete-examples)

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
| `loadTexture(path)` | Load texture, returns texture ID |
| `getTextureSize(id)` | Returns texture width, height |
| `getWhiteTexture()` | Returns white texture ID (fallback) |

### Models (3D)

| Function | Description |
|----------|-------------|
| `loadModel(name)` | Load/create model, returns handle |
| `createMesh(data)` | Create custom mesh, returns handle |

**Model Names**: "cube", "plane", "sphere", "cylinder", "cone", "pyramid", "torus", "capsule", "grid", or path to .obj file

**Mesh Data**:
```lua
{
    vertices = {
        {pos = {x,y,z}, norm = {x,y,z}, uv = {u,v}, color = {r,g,b,a}},
        -- ...
    },
    indices = {0,1,2, 0,2,3, ...}  -- Triangle indices
}
```

### 3D Drawing

| Function | Description |
|----------|-------------|
| `drawModel(id, x, y, z, rx, ry, rz, sx, sy, sz, tex)` | Draw loaded model |
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
| `drawLines3d(points)` | Draw multiple 3D lines |
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
| `drawPolygon(mode, points)` | Draw polygon (points: {{x,y}, ...}) |
| `drawCircle(mode, cx, cy, radius, segs)` | Draw circle |
| `drawEllipse(mode, cx, cy, rx, ry, segs)` | Draw ellipse |
| `drawArc(mode, cx, cy, radius, a0, a1, segs)` | Draw arc |
| `drawRing(mode, cx, cy, innerR, outerR, segs)` | Draw ring |
| `drawPie(mode, cx, cy, radius, a1, a2, segs)` | Draw pie slice |
| `drawGradientRect(x, y, w, h, cTl, cTr, cBr, cBl)` | Draw gradient rect |
| `drawGradientH(x, y, w, h, cLeft, cRight)` | Horizontal gradient |
| `drawGradientV(x, y, w, h, cTop, cBottom)` | Vertical gradient |
| `drawPolyline(points, thick, loop)` | Draw polyline |
| `drawBezier(x0,y0, x1,y1, x2,y2, thick, segs)` | Draw quadratic bezier |
| `drawBezier(x0,y0, x1,y1, x2,y2, x3,y3, thick, segs)` | Draw cubic bezier (4 control points) |

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
| `createBox(x, y, z, hx, hy, hz, motion, friction, restitution, density)` | Create box body |
| `createSphere(x, y, z, radius, motion, friction, restitution, density)` | Create sphere body |
| `createCapsule(x, y, z, halfH, radius, motion, friction, restitution, density)` | Create capsule body |
| `createCylinder(x, y, z, halfH, radius, motion, friction, restitution, density)` | Create cylinder body |
| `createPlane(x, y, z, nx, ny, nz, halfExtent)` | Create static plane body |

**Motion Types**: "static", "kinematic", "dynamic" (or 0, 1, 2)

### Body Methods (returned userdata)

| Method | Description |
|--------|-------------|
| `body:getId()` | Returns body ID |
| `body:isValid()` | Body still exists? |
| `body:isActive()` | Body active? |
| `body:setActive(active)` | Activate/deactivate body |
| `body:destroy()` | Destroy this body |

### Transforms

| Method | Description |
|--------|-------------|
| `body:getPosition()` | Returns x, y, z |
| `body:setPosition(x, y, z [, activate])` | Set position |
| `body:getRotation()` | Returns euler x, y, z (radians) |
| `body:setRotation(rx, ry, rz [, activate])` | Set rotation (radians) |

### Dynamics

| Method | Description |
|--------|-------------|
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
| `body:setDamping(linearDamping, angularDamping)` | Set damping |
| `body:setSensor(isSensor)` | Make body a sensor (trigger) |
| `body:isSensor()` | Is body a sensor? |

### World Settings

| Function | Description |
|----------|-------------|
| `setGravity(gx, gy, gz)` | Set world gravity |
| `getGravity()` | Returns gx, gy, gz |
| `destroyAll()` | Destroy all bodies |
| `getBodyCount()` | Returns total bodies, active bodies |

### Raycast

| Function | Description |
|----------|-------------|
| `raycast(ox, oy, oz, dx, dy, dz, maxDist)` | Cast ray. Returns hit, pos x/y/z, normal x/y/z, distance, body |

### Constraints

| Function | Description |
|----------|-------------|
| `createPointConstraint(b1, b2, px, py, pz)` | Create point constraint |
| `createHingeConstraint(b1, b2, px, py, pz, ax, ay, az, minAngle, maxAngle)` | Create hinge constraint |
| `createDistanceConstraint(b1, b2, p1x, p1y, p1z, p2x, p2y, p2z, minD, maxD)` | Create distance constraint |
| `createFixedConstraint(b1, b2)` | Create fixed constraint |
| `destroyConstraint(c)` | Destroy constraint |

**Constraint Methods**: `:destroy()`, `:isValid()`, `:getId()`

### Queries & Debug

| Function | Description |
|----------|-------------|
| `overlapSphere(cx, cy, cz, radius)` | Returns table of body userdata in sphere |
| `drawDebug(r, g, b, a, sr, sg, sb, sa)` | Draw physics debug (AABB wireframes) |

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

---

## Physics2D Module
Access via: `crayon.physics2d`

| Function | Description |
|----------|-------------|
| `createBody(x, y, mass)` | Create a 2D physics body |

**Body Methods**: `:getPosition()`, `:setPosition(x, y)`, `:getVelocity()`, `:setVelocity(vx, vy)`, `:getAngle()`, `:setAngle(angle)`, `:applyForce(fx, fy)`, `:isValid()`, `:destroy()`

---

## Complete Examples

### Example 1: Basic Game Loop

```lua
-- game.lua

function crayon.init()
    -- Setup window
    crayon.window.setResolution(320, 240)
    crayon.window.setWindowSize(960, 720)
    crayon.window.setTitle("My Game")
    
    -- Setup graphics
    crayon.graphics.setColor(1, 1, 1)
end

function crayon.update(dt)
    if crayon.input.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Draw some text
    crayon.graphics.drawText("Hello World!", 10, 10, 2)
    
    -- Draw FPS
    local fps = crayon.window.getFps()
    crayon.graphics.drawText("FPS: " .. tostring(fps), 10, 30, 1)
end
```

### Example 2: 2D Sprite & Shapes

```lua
function crayon.init()
    crayon.window.setResolution(640, 480)
    crayon.window.setWindowSize(1280, 960)
    crayon.window.setTitle("2D Demo")
    
    tex = crayon.graphics.loadTexture("assets/player.png")
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Sprites
    local x, y = crayon.input.getMousePos()
    crayon.graphics.drawSprite(tex, x, y, 32, 32, 0, 16, 16)
    
    -- Sprite part (sprite sheet)
    crayon.graphics.drawSpritePart(tex, 100, 100, 0, 0, 16, 16, 32, 32)
    
    -- Tiled sprite
    crayon.graphics.drawSpriteTiled(tex, 200, 200, 100, 100, 16, 16)
    
    -- 9-slice
    crayon.graphics.drawSprite9slice(tex, 300, 300, 64, 64, 8, 8, 8, 8)
    
    -- Shapes
    crayon.graphics.setColor(1, 0, 0, 1)
    crayon.graphics.drawRect("fill", 10, 10, 50, 50)
    
    crayon.graphics.setColor(0, 1, 0, 1)
    crayon.graphics.drawCircle("fill", 100, 100, 30)
    
    crayon.graphics.setColor(1, 1, 0, 1)
    crayon.graphics.drawLine(200, 200, 300, 300, 2)
    
    -- Polygon (hexagon)
    local hex = {}
    for i = 0, 5 do
        local angle = i * 3.14159 / 3
        hex[i+1] = {400 + math.cos(angle) * 40, 300 + math.sin(angle) * 40}
    end
    crayon.graphics.setColor(0.5, 0, 1, 1)
    crayon.graphics.drawPolygon("fill", hex)
    
    -- Text
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("Hello 2D!", 20, 200, 2)
end
```

### Example 3: 3D Scene with Physics

```lua
-- 3D platformer example

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setWindowSize(960, 720)
    crayon.window.setTitle("3D Physics Demo")
    
    -- Setup camera
    crayon.graphics.setCamera3d({
        position = {0, 3, 8},
        target = {0, 0, 0},
        fov = 60
    })
    
    -- Setup lighting
    crayon.graphics.setLight(-0.5, -1, -0.7, 1, 0.95, 0.9, 0.25, 0.25, 0.3)
    
    -- Retro effects
    crayon.graphics.setRetroEffects({
        jitterResolution = {160, 120},
        affine = 1.0,
        dither = true,
        colorDepth = 32,
        fog = {startDist = 10, endDist = 30, color = {0.1, 0.1, 0.2}}
    })
    
    -- Load textures
    floor_tex = crayon.graphics.loadTexture("assets/floor.png")
    player_tex = crayon.graphics.loadTexture("assets/player.png")
    
    -- Create physics floor
    floor = crayon.physics.createPlane(0, -0.5, 0, 0, 1, 0, 50)
    
    -- Create player
    player = crayon.physics.createSphere(0, 2, 0, 0.5, "dynamic", 0.5, 0.5)
    player:setFriction(0.8)
    player:setRestitution(0.3)
    
    -- Create some obstacles
    obstacles = {}
    for i = 1, 5 do
        local x = (i - 3) * 2
        local z = 3
        local id = crayon.physics.createBox(x, 0.5, z, 0.5, 0.5, 0.5, "static")
        table.insert(obstacles, {id = id, x = x, z = z})
    end
end

function crayon.update(dt)
    -- Player controls
    local speed = 5
    local jump_force = 5
    
    local vx, vy, vz = player:getVelocity()
    if crayon.input.isDown("w") or crayon.input.isDown("up") then
        player:setVelocity(vx, vy, -speed)
    end
    if crayon.input.isDown("s") or crayon.input.isDown("down") then
        player:setVelocity(vx, vy, speed)
    end
    if crayon.input.isDown("a") or crayon.input.isDown("left") then
        player:setVelocity(-speed, vy, vz)
    end
    if crayon.input.isDown("d") or crayon.input.isDown("right") then
        player:setVelocity(speed, vy, vz)
    end
    
    if crayon.input.isPressed("space") then
        player:setVelocity(vx, jump_force, vz)
    end
    
    -- Quit
    if crayon.input.isPressed("escape") then
        crayon.window.quit()
    end
    
    -- Physics camera follows player
    local px, py, pz = player:getPosition()
    crayon.graphics.setCamera3d({
        position = {px, 3, pz + 8},
        target = {px, 0, pz},
        fov = 60
    })
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Draw floor
    crayon.graphics.drawPlane(0, 0, 0, 20, 20, floor_tex, 0, 0, 0)
    
    -- Draw player
    local px, py, pz = player:getPosition()
    crayon.graphics.drawSphere(px, py, pz, 0.5, player_tex)
    
    -- Draw obstacles
    for _, obs in ipairs(obstacles) do
        crayon.graphics.drawCube(obs.x, 0.5, obs.z, 1, 1, 1)
    end
    
    -- Debug: physics visualization
    crayon.physics.drawDebug(0.2, 1, 0.4, 1, 0.5, 0.5, 0.5, 1)
    
    -- UI overlay
    crayon.graphics.setColor(1, 1, 1, 1)
    local fps = crayon.window.getFps()
    crayon.graphics.drawText("FPS: " .. tostring(fps), 10, 10, 1)
    crayon.graphics.drawText("WASD to move, Space to jump", 10, 30, 1)
end
```

### Example 4: 3D Model Loading

```lua
function crayon.init()
    crayon.window.setResolution(640, 480)
    crayon.window.setTitle("3D Models")
    
    crayon.graphics.setCamera3d({
        position = {0, 3, 10},
        target = {0, 0, 0}
    })
    
    crayon.graphics.setLight(-0.5, -1, -0.7, 1, 0.95, 0.9, 0.3, 0.3, 0.35)
    
    -- Load models
    cube = crayon.graphics.loadModel("cube")
    sphere = crayon.graphics.loadModel("sphere")
    torus = crayon.graphics.loadModel("torus")
    
    -- Load custom OBJ
    character = crayon.graphics.loadModel("assets/character.obj")
    tex = crayon.graphics.loadTexture("assets/character.png")
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    local t = crayon.time.getTime()
    
    -- Draw models with transforms
    crayon.graphics.drawModel(cube, -4, 0, 0, t, t*0.5, 0, 1, 1, 1)
    crayon.graphics.drawModel(sphere, -1.5, 0.5, 0, 0, t, 0, 1, 1, 1)
    crayon.graphics.drawModel(torus, 1.5, 0.5, 0, t, t*0.7, t*0.3, 1, 1, 1)
    
    -- Custom model with texture
    crayon.graphics.drawModel(character, 4, 0, 0, 0, t, 0, 1, 1, 1, tex)
    
    -- Using transform stack
    crayon.graphics.pushMatrix()
    crayon.graphics.translate(0, 2, 0)
    crayon.graphics.rotate(t, 0, 1, 0)
    crayon.graphics.scale(1.5, 1.5, 1.5)
    crayon.graphics.drawModel(sphere, 0, 0, 0, 0, 0, 0, 1, 1, 1)
    crayon.graphics.popMatrix()
    
    -- UI
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("3D Models Demo", 10, 10, 2)
end
```

### Example 5: 2D Camera & Transforms

```lua
function crayon.init()
    crayon.window.setResolution(640, 480)
    crayon.window.setTitle("2D Camera Demo")
    
    tex = crayon.graphics.loadTexture("assets/character.png")
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    local t = crayon.time.getTime()
    
    -- Camera controls
    local cx = math.sin(t * 0.3) * 100
    local cy = math.cos(t * 0.2) * 50
    local zoom = 1.0 + math.sin(t * 0.5) * 0.3
    
    crayon.graphics.setCamera2d({
        x = cx, y = cy,
        zoom = zoom,
        angle = t * 0.1
    })
    
    -- Draw grid
    for i = -20, 20 do
        for j = -15, 15 do
            local x = i * 32
            local y = j * 32
            local color = (i + j) % 2 == 0 and {0.2, 0.2, 0.3, 1} or {0.3, 0.3, 0.4, 1}
            crayon.graphics.setColor(unpack(color))
            crayon.graphics.drawRect("fill", x, y, 32, 32)
        end
    end
    
    -- Draw character
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawSprite(tex, 0, 0, 32, 32, t, 16, 16)
    
    -- Reset camera for UI
    crayon.graphics.resetCamera2d()
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("2D Camera Demo", 10, 10, 2)
    crayon.graphics.drawText("Zoom: " .. string.format("%.2f", zoom), 10, 30, 1)
end
```

### Example 6: Input Handling

```lua
function crayon.init()
    crayon.window.setResolution(640, 480)
    crayon.window.setTitle("Input Demo")
    
    crayon.input.startTextInput()
end

function crayon.update(dt)
    -- Keyboard
    if crayon.input.isPressed("space") then
        print("Space pressed!")
    end
    
    -- Modifiers
    if crayon.input.isShiftDown() and crayon.input.isPressed("s") then
        print("Shift+S pressed!")
    end
    
    -- Mouse
    local mx, my = crayon.input.getMousePos()
    local mdx, mdy = crayon.input.getMouseDelta()
    
    if crayon.input.isMousePressed("left") then
        print("Left click at:", mx, my)
    end
    
    -- Gamepad
    if crayon.input.gamepadIsDown(0) then  -- A button
        print("A button held!")
    end
    
    local lx = crayon.input.gamepadAxis(0)   -- Left stick X
    local ly = crayon.input.gamepadAxis(1)   -- Left stick Y
    
    -- Text input
    local text = crayon.input.getTextInput()
    if text ~= "" then
        print("Text input:", text)
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    local mx, my = crayon.input.getMousePos()
    local mwx, mwy = crayon.input.getMouseWindowPos()
    
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("Input Demo", 10, 10, 2)
    crayon.graphics.drawText("Mouse: " .. tostring(mx) .. ", " .. tostring(my), 10, 40, 1)
    crayon.graphics.drawText("Window Mouse: " .. tostring(mwx) .. ", " .. tostring(mwy), 10, 55, 1)
    
    local text = crayon.input.getTextInput()
    crayon.graphics.drawText("Text: " .. text, 10, 70, 1)
    
    -- Draw crosshair at mouse position
    crayon.graphics.setColor(1, 0, 0, 1)
    crayon.graphics.drawLine(mx - 10, my, mx + 10, my, 1)
    crayon.graphics.drawLine(mx, my - 10, mx, my + 10, 1)
end
```

### Example 7: Physics Constraints

```lua
function crayon.init()
    crayon.window.setResolution(640, 480)
    crayon.window.setTitle("Constraints Demo")
    
    crayon.graphics.setCamera3d({
        position = {0, 5, 15},
        target = {0, 2, 0}
    })
    
    -- Create two dynamic bodies
    body1 = crayon.physics.createBox(-2, 3, 0, 0.5, 0.5, 0.5, "dynamic")
    body2 = crayon.physics.createBox(2, 3, 0, 0.5, 0.5, 0.5, "dynamic")
    
    body1:setMotionType("kinematic")  -- Make one kinematic
    
    -- Create constraint between them
    constraint = crayon.physics.createDistanceConstraint(
        body1, body2,
        -2, 3, 0,  -- Point on body1
        2, 3, 0,   -- Point on body2
        2, 4       -- Min and max distance
    )
    
    -- Static floor
    floor = crayon.physics.createPlane(0, -0.5, 0, 0, 1, 0, 20)
end

function crayon.update(dt)
    -- Rotate kinematic body
    local t = crayon.time.getTime()
    local px = math.sin(t * 2) * 2
    local pz = math.cos(t * 2) * 2
    body1:setPosition(px, 3, pz)
    body1:setRotation(t * 1.57, t * 0.78, 0)  -- radians
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Draw bodies
    crayon.graphics.drawCube(-2, 3, 0, 1, 1, 1, 0, 0, 0, 0)
    crayon.graphics.drawCube(2, 3, 0, 1, 1, 1, 0, 0, 0, 0)
    crayon.graphics.drawPlane(0, 0, 0, 20, 20)
    
    -- Debug
    crayon.physics.drawDebug()
end
```

### Example 8: Raycasting

```lua
function crayon.init()
    crayon.window.setResolution(640, 480)
    crayon.window.setTitle("Raycast Demo")
    
    crayon.graphics.setCamera3d({
        position = {0, 10, 10},
        target = {0, 0, 0}
    })
    
    -- Create some obstacles
    obstacles = {}
    for i = 1, 8 do
        local angle = i * 3.14159 / 4
        local x = math.cos(angle) * 3
        local z = math.sin(angle) * 3
        local id = crayon.physics.createBox(x, 0.5, z, 0.5, 0.5, 0.5, "static")
        table.insert(obstacles, {id = id, x = x, z = z})
    end
end

function crayon.update(dt)
    -- Raycast from camera center
    local mx, my = crayon.input.getMousePos()
    local origin_x, origin_y, origin_z, dir_x, dir_y, dir_z = crayon.graphics.unproject(mx, my)
    
    local hit, hx, hy, hz, nx, ny, nz, dist, body = crayon.physics.raycast(
        origin_x, origin_y, origin_z,
        dir_x, dir_y, dir_z,
        100
    )
    
    if hit then
        print("Hit at:", hx, hy, hz, "Distance:", dist)
        last_hit = {hx, hy, hz}
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Draw obstacles
    for _, obs in ipairs(obstacles) do
        crayon.graphics.drawCube(obs.x, 0.5, obs.z, 1, 1, 1)
    end
    
    -- Draw hit point
    if last_hit then
        crayon.graphics.setColor(1, 0, 0, 1)
        crayon.graphics.drawSphere(last_hit[1], last_hit[2], last_hit[3], 0.2, 0)
    end
    
    -- UI
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("Click to raycast", 10, 10, 2)
end
```

### Example 9: Character Controller

```lua
function crayon.init()
    crayon.window.setResolution(640, 480)
    crayon.window.setTitle("Character Controller")
    
    crayon.graphics.setCamera3d({
        position = {0, 5, 10},
        target = {0, 1, 0}
    })
    
    -- Static ground
    crayon.physics.createPlane(0, 0, 0, 0, 1, 0, 100)
    
    -- Create character
    player = crayon.physics.createCharacter({
        pos = {0, 1, 0},
        radius = 0.4,
        halfHeight = 0.6,
        mass = 80.0,
        maxSlopeAngleDeg = 50.0
    })
end

function crayon.update(dt)
    -- Simple kinematic character movement
    local speed = 5.0
    local px, py, pz = player:getPosition()
    local vx, vy, vz = player:getLinearVelocity()
    
    local move_x, move_z = 0, 0
    if crayon.input.isDown("w") then move_z = move_z - speed end
    if crayon.input.isDown("s") then move_z = move_z + speed end
    if crayon.input.isDown("a") then move_x = move_x - speed end
    if crayon.input.isDown("d") then move_x = move_x + speed end
    
    player:setLinearVelocity(move_x, vy, move_z)
    
    if crayon.input.isPressed("space") and player:isSupported() then
        player:setLinearVelocity(move_x, 8.0, move_z)
    end
    
    -- Update camera to follow
    crayon.graphics.setCamera3d({
        position = {px, py + 4, pz + 8},
        target = {px, py, pz}
    })
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    crayon.graphics.drawPlane(0, 0, 0, 50, 50)
    
    local px, py, pz = player:getPosition()
    crayon.graphics.drawCapsule(px, py, pz, 0.4, 1.2)
    
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("State: " .. player:getGroundState(), 10, 10, 1)
    crayon.graphics.drawText("Supported: " .. tostring(player:isSupported()), 10, 25, 1)
end
```

---

## Quick Reference Card

### Core Functions
```lua
crayon.init()      -- Called once on startup
crayon.update(dt)  -- Called every frame
crayon.draw()      -- Called every frame for rendering
```

### Common Patterns

**Loading Resources**:
```lua
tex = crayon.graphics.loadTexture("path.png")
model = crayon.graphics.loadModel("path.obj")
```

**Creating Physics Bodies**:
```lua
body = crayon.physics.createBox(x, y, z, hx, hy, hz, "dynamic", 0.5, 0.2)
```

**Getting Body State**:
```lua
x, y, z = body:getPosition()
vx, vy, vz = body:getVelocity()
```

**Input Checking**:
```lua
if crayon.input.isPressed("space") then
    -- Jump!
end
```

**Drawing 2D**:
```lua
crayon.graphics.setColor(r, g, b, a)
crayon.graphics.drawRect("fill", x, y, w, h)
```

**Drawing 3D**:
```lua
crayon.graphics.drawCube(x, y, z, sx, sy, sz, tex, rx, ry, rz)
```
