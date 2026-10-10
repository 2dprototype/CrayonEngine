# Crayon Engine

A lightweight 2D/3D game engine with retro aesthetics, modern physics, and Lua scripting. Built with C++20, OpenGL 3.3, SDL3, and Jolt Physics.

## logo

![Crayon Engine Logo](icon/icon_256.png)

## Features

- **Full Lua Scripting** — Complete engine API exposed to LuaJIT
- **2D & 3D Rendering** — Batched 2D sprites + 3D mesh rendering
- **Skeletal Animation** — Skinned models, layered blending, physics-pose integration
- **Virtual Resolution** — Pixel-perfect scaling with 4 modes (`integer`, `aspect`, `stretch`, `center`)
- **Retro Effects** — Jitter, affine mapping, dithering, CRT scanlines, curvature, vignette, distance fog
- **3D Lighting** — Directional light + 4 point lights + 4 spot lights
- **Shading Modes** — Gouraud, Flat, Unlit
- **3D Physics** — Jolt Physics with rigid bodies, constraints, raycasting, sensors, characters, vehicles, ragdolls, skeletons, and soft bodies
- **2D Physics** — (Box2D) rigid bodies, joints, and fixtures
- **Post-Processing** — Stackable effect chain with custom shaders and uniforms
- **Particles** — CPU particle emitters with 2D/3D rendering
- **Audio** — SFX, positional 3D audio, streamed music, per-channel volume, **procedural sound synthesis**, and **live Lua-driven block generators**
- **Input** — Keyboard, mouse, gamepad, text input, clipboard
- **Pretty Printer** — Colorized, cycle-safe `crayon.print(...)` for debugging tables
- **Project System** — `.crayonproj` manifests for games and demos
- **Hot Reload** — Instant script reload on file change or F5
- **Asset Loading** — PNG/JPG textures, OBJ/GLTF/GLB models, procedural meshes

## Platforms

- **Windows** — `build.bat` (MinGW, OpenGL 3.3 core).
- **Android** — NDK + Gradle project in [`android/`](android/), OpenGL ES 3.0, touch input
  (`crayon.touch`). See [docs/android.md](docs/android.md).

## Quick Start

### Command Line

```bash
# Run a Lua script directly
crayon game/main.lua

# Run a project folder (must contain a .crayonproj manifest)
crayon path/to/my_project/

# Run a project by its manifest
crayon path/to/my_project/.crayonproj

# Run the project in the current directory (if .crayonproj exists)
crayon
```

### CLI Options

| Option | Description |
|--------|-------------|
| `--game, -g <path>` | Specify entry script path or project folder |
| `--window <W>x<H>` | Override physical window resolution (e.g. `1280x720`) |
| `--res <W>x<H>` | Override virtual canvas resolution (e.g. `320x240`) |
| `--version, -v` | Display version information |
| `--help, -h` | Display help message |

Example with overrides:

```bash
crayon my_game/ --window 1280x720 --res 320x240
```

### Project Manifest (`.crayonproj`)

A `.crayonproj` file marks a directory as a Crayon project and tells the engine which script to run.

- Passing a **folder** — the engine looks for `<folder>/.crayonproj`
- Passing a **`.crayonproj` file** — the engine uses it directly and changes the working directory to its parent
- Passing **nothing** — the engine looks for `.crayonproj` in the current directory

### Basic Game Script

```lua
function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("My Game")
end

function crayon.update(dt)
    if crayon.key.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("Hello World!", 10, 10, 2)
end
```

> **Note:** All engine APIs use **camelCase** (e.g. `setResolution`, `drawRect`, `isPressed`). Body, model, and other object handles expose methods via `:` (e.g. `body:getPosition()`). Numeric suffixes use a capital `D` — `setCamera3D`, `drawLine2D`, `physics3D`.

## Configuration

Engine startup is configured through an optional `crayon.config(t)` function that runs before `crayon.init()`.

```lua
function crayon.config(t)
    t.identity = "MyGame"
    t.version = "1.0.0"
    t.fpsLimit = 60
    t.console = false

    t.window = {
        title = "My Game",
        width = 1280, height = 720,
        virtualWidth = 320, virtualHeight = 240,
        minWidth = 640, minHeight = 360,
        resizable = true,
        fullscreen = false,
        vsync = true,
        transparent = false,
        borderless = false,
        alwaysOnTop = false,
        clickThrough = false,
        opacity = 1.0,
        skipTaskbar = false,
        notFocusable = false,
        utilityWindow = false,
        scaling = "integer"           -- "integer" | "aspect" | "stretch" | "center"
    }

    t.modules = {
        physics3D = true,
        physics2D = true,
        audio = true,
        mesh3D = true,
        particles = true,
        input = true,
        fs = true
    }

    t.graphics = {
        clearColor = {0.1, 0.1, 0.15, 1.0},
        dither = false,
        crt = false,
        vignette = false
    }
end
```

## Graphics API Examples

### 2D Rendering
```lua
-- Sprites
local tex = crayon.graphics.loadTexture("player.png")
crayon.graphics.drawSprite(tex, x, y, 32, 32, angle, 16, 16)

-- Shapes
crayon.graphics.setColor(1, 0, 0, 1)
crayon.graphics.drawRect("fill", 10, 10, 50, 50)
crayon.graphics.drawCircle("fill", 100, 100, 30)
crayon.graphics.drawRoundedRect("fill", 10, 10, 80, 40, 8, 12)
crayon.graphics.drawGradientH(10, 60, 100, 30, {1,0,0,1}, {0,0,1,1})

-- Text
crayon.graphics.drawText("Score: " .. score, 10, 10, 2)

-- Rich text markup
crayon.graphics.drawTextMarkup("[wave]Hello[/wave] [color=1,0,0]World[/color]", 10, 10, {
    scale = 2, wrapWidth = 200, align = "center"
})

-- 2D Camera
crayon.graphics.setCamera2D({ x = 0, y = 0, zoom = 1.0, angle = 0 })

-- Transform stack (2D)
crayon.graphics.pushMatrix2D()
crayon.graphics.translate2D(100, 100)
crayon.graphics.rotate2D(1.57)
crayon.graphics.drawSprite(tex, 0, 0, 32, 32)
crayon.graphics.popMatrix2D()

-- Scissor stack
crayon.graphics.pushScissor(10, 10, 100, 100)
crayon.graphics.popScissor()
```

### 3D Rendering
```lua
-- 3D Camera
crayon.graphics.setCamera3D({
    position = {0, 3, 8},
    target   = {0, 0, 0},
    fov      = 60,
    near     = 0.1,
    far      = 1000.0
})

-- Lighting
crayon.graphics.setLight(-0.5, -1, -0.7, 1, 0.95, 0.9, 0.25, 0.25, 0.3)
crayon.graphics.setPointLight(0, 0, 5, 0, 1, 1, 1, 10, 1)
crayon.graphics.setSpotLight(0, 0, 5, 0, 0, -1, 0, 15, 1, 1, 1, 1, 15, 25)
crayon.graphics.setShadingMode("gouraud")

-- 3D Objects
crayon.graphics.drawCube(x, y, z, 1, 1, 1, tex, rx, ry, rz)
crayon.graphics.drawSphere(x, y, z, 0.5, tex)
crayon.graphics.drawBillboard(x, y, z, 1, 1, tex, "cylindrical")

-- Models
local model = crayon.graphics.loadModel("cube")  -- or "sphere", "torus", or .obj/.gltf/.glb path
crayon.graphics.drawModel(model, x, y, z, rx, ry, rz, 1, 1, 1, tex)

-- 3D Transforms
crayon.graphics.pushMatrix()
crayon.graphics.translate(0, 2, 0)
crayon.graphics.rotate(t, 0, 1, 0)
crayon.graphics.scale(1.5, 1.5, 1.5)
crayon.graphics.drawSphere(0, 0, 0, 0.5)
crayon.graphics.popMatrix()

-- Lines & Debug
crayon.graphics.drawLine3D(x1, y1, z1, x2, y2, z2)
crayon.graphics.drawGrid3D(20, 20, 0)
crayon.graphics.drawAxes3D(0, 0, 0, 1)
crayon.graphics.drawCubeWires(0, 1, 0, 1, 1, 1)
```

### Retro Effects
```lua
crayon.graphics.setRetroEffects({
    jitterResolution = {160, 120},   -- Pixel snap resolution
    affine           = 1.0,          -- 0 = perspective, 1 = affine
    dither           = true,
    colorDepth       = 32,           -- 8 or 32
    fog = {
        startDist = 5.0,
        endDist   = 25.0,
        color     = {0.1, 0.1, 0.2}
    },
    crt = {
        scanlines = 0.25,
        curvature = 0.05,
        vignette  = 0.25
    }
})
```

### Post-Processing
```lua
crayon.graphics.pushEffect("bloom2D", { intensity = 1.5, threshold = 0.8 })
-- ... draw scene ...
crayon.graphics.setEffectUniform("intensity", 2.0)
crayon.graphics.popEffect()

-- Update by name
crayon.graphics.setEffectUniform("bloom2D", "threshold", 0.9)
```

**Built-in effects:** `"chromatic"`, `"vignette"`, `"dissolve"`, `"vhs"`, `"bloom2D"`, `"pixelate"`, `"radialBlur"`, `"filmGrain"`.

### Canvas
```lua
local canvas = crayon.graphics.createCanvas(320, 240)
canvas:renderTo(function()
    canvas:clear(0, 0, 0, 1)
    crayon.graphics.setColor(1, 0.5, 0)
    crayon.graphics.drawCircle("fill", 160, 120, 50)
end)
crayon.graphics.drawSprite(canvas:getTexture(), 0, 0)
```

### Camera2D Object
```lua
local cam = crayon.graphics.newCamera2D(160, 120, 2.0)
cam:setTarget(player.x, player.y)
cam:setDeadzone(40, 30)
cam:setBounds(0, 0, 640, 480)
cam:shake(5.0, 0.3)
cam:update(dt)
cam:apply()
```

### Skeletal Animation
```lua
local model    = crayon.graphics.loadModel("character.glb")
local animator = model:createAnimator()
animator:play("Idle", true)

local physics_skel = model:createPhysicsSkeleton()
local pose         = crayon.physics3D.createSkeletonPose(physics_skel)

function crayon.update(dt)
    animator:update(dt)
    animator:applyToPhysicsPose(pose)
    pose:calculateMatrices()
end

function crayon.draw()
    crayon.graphics.drawModelSkinned(model, animator, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0)
end
```

## Physics API Examples

### Physics3D Bodies
```lua
-- Create bodies
local box    = crayon.physics3D.createBox(0, 2, 0, 0.5, 0.5, 0.5, "dynamic", 0.5, 0.2)
local sphere = crayon.physics3D.createSphere(0, 5, 0, 0.5, "dynamic", 0.5, 0.5)
local floor  = crayon.physics3D.createPlane(0, -0.5, 0, 0, 1, 0, 50)

-- Motion types: "static", "kinematic", "dynamic"
box:setMotionType("kinematic")

-- Transforms (method calls)
local x, y, z = box:getPosition()
box:setPosition(0, 3, 0, true)
local rx, ry, rz = box:getRotation()
box:setRotation(0, 0.785, 0, true)

-- Dynamics
local vx, vy, vz = box:getVelocity()
box:setVelocity(5, 0, 0)
box:applyForce(0, -9.81, 0)
box:applyImpulse(0, 5, 0)
box:applyTorque(0, 1, 0)

-- Properties
box:setFriction(0.8)
box:setRestitution(0.3)
box:setGravityFactor(1.0)
box:setDamping(0.1, 0.1)
box:setSensor(false)
box:setMotionQuality("linearCast")  -- enable CCD
box:setPlanarLock("xz")             -- constrain to XZ plane

-- Lifetime
box:isValid()
box:destroy()
```

### Physics3D Constraints
```lua
-- Point constraint
local c1 = crayon.physics3D.createPointConstraint(body1, body2, px, py, pz)

-- Hinge constraint (pivot + axis)
local c2 = crayon.physics3D.createHingeConstraint(
    body1, body2,
    px, py, pz,    -- pivot
    ax, ay, az     -- axis
)

-- Distance constraint
local c3 = crayon.physics3D.createDistanceConstraint(
    body1, body2,
    p1x, p1y, p1z,
    p2x, p2y, p2z,
    min_dist, max_dist
)

-- Fixed constraint
local c4 = crayon.physics3D.createFixedConstraint(body1, body2)

-- Destroy
crayon.physics3D.destroyConstraint(c1)
```

### Physics3D Queries
```lua
-- Raycast (returns hit + hit data + body handle)
local hit, hx, hy, hz, nx, ny, nz, dist, body = crayon.physics3D.raycast(
    ox, oy, oz,    -- origin
    dx, dy, dz,    -- direction
    max_dist
)

-- Overlap sphere
local hits = crayon.physics3D.overlapSphere(cx, cy, cz, radius)
for _, body in ipairs(hits) do
    print("Body in sphere:", body:getId())
end

-- Debug visualization
crayon.physics3D.drawDebug(0.2, 1, 0.4, 1, 0.5, 0.5, 0.5, 1)
```

### Physics3D World
```lua
crayon.physics3D.setGravity(0, -9.81, 0)
crayon.physics3D.step(dt)                -- step the simulation
crayon.physics3D.getBodyCount()          -- returns total, active
crayon.physics3D.getBody(id)             -- returns body or nil
crayon.physics3D.destroyAll()            -- clears the world
```

### Physics3D Characters
```lua
local hero = crayon.physics3D.createCharacter({
    pos = {0, 1, 0},
    radius = 0.4,
    halfHeight = 0.6,
    mass = 80.0,
    maxSlopeAngleDeg = 45
})

if hero:isSupported() then
    print("Grounded:", hero:getGroundState())
end
local nx, ny, nz = hero:getGroundNormal()
local bx, by, bz = hero:getPosition()
hero:setPosition(bx, by + 0.1, bz)

-- Virtual character (kinematic, driven by user velocity)
local ghost = crayon.physics3D.createCharacterVirtual({
    pos = {0, 1, 0}, radius = 0.4, halfHeight = 0.6,
    stepHeight = 0.4, innerBody = true
})
ghost:setLinearVelocity(0, 0, -3)
ghost:update(dt)
```

### Physics3D Vehicles
```lua
local chassis = crayon.physics3D.createBox(0, 1, 0, 1, 0.3, 0.5, "dynamic", 0.5, 0.2)

local car = crayon.physics3D.createWheeledVehicle({
    chassis = chassis,
    engineMaxTorque = 500,
    engineMinRpm = 1000,
    engineMaxRpm = 6000,
    wheels = {
        { position = {-0.7, -0.3,  0.5}, radius = 0.3, isFront = true,  isDrive = true },
        { position = { 0.7, -0.3,  0.5}, radius = 0.3, isFront = true,  isDrive = false },
        { position = {-0.7, -0.3, -0.5}, radius = 0.3, isFront = false, isDrive = true },
        { position = { 0.7, -0.3, -0.5}, radius = 0.3, isFront = false, isDrive = false },
    }
})

car:setInputWheeled(1.0, 0.2, 0.0, false)
print(car:getSpeedKmh(), car:getEngineRpm(), car:getTransmissionGear())
```

### Physics3D Ragdolls
```lua
local ragdoll = crayon.physics3D.createRagdoll({
    stabilize = true,
    parts = {
        { name = "Hips",  parentJointIndex = -1, shapeType = "capsule",
          position = {0, 1.0, 0}, halfHeight = 0.1, radius = 0.12,
          motion = "kinematic", mass = 8.0 },
        { name = "Spine", parentJointIndex = 0, shapeType = "capsule",
          position = {0, 0.25, 0}, halfHeight = 0.15, radius = 0.12,
          motion = "dynamic", mass = 10.0, enableMotors = true,
          motorSpringK = 200, motorDampingC = 20, motorMaxTorque = 80 }
    }
})

ragdoll:addImpulse(0, 5, 0)
ragdoll:driveToPoseMotorsVelocity(prevPose, targetPose, dt)
local px, py, pz, qx, qy, qz, qw = ragdoll:getRootTransform()
```

### Physics3D Soft Bodies
```lua
-- Cloth
local cloth = crayon.physics3D.createSoftBodyCloth({
    x = 0, y = 5, z = 0, width = 4, height = 4,
    segmentsX = 12, segmentsY = 12,
    compliance = 0.0, bendCompliance = 0.01,
    pinCorners = true
})

-- Pressurized sphere (balloon)
local balloon = crayon.physics3D.createSoftBodySphere({
    x = 0, y = 3, z = 0, radius = 1.0,
    rings = 10, sectors = 16,
    pressure = 800.0
})

-- Soft cube
local jello = crayon.physics3D.createSoftBodyCube({
    x = 0, y = 2, z = 0, size = 1.5,
    gridSize = 4, compliance = 0.0005
})

-- Rod / rope
local rope = crayon.physics3D.createSoftBodyRod({
    points = { {0,5,0}, {0,4,0}, {0,3,0}, {0,2,0} },
    stretchCompliance = 0.0,
    bendTwistCompliance = 0.001,
    pinRoot = true
})

-- Read & write vertices
local px, py, pz, vx, vy, vz, inv_m = cloth:getVertex(1)
cloth:applyForce(0, -9.81, 0)
cloth:applyImpulse(10, 0, 0, 0, 0, 0)  -- impulse to a single vertex

-- Skin to a skeleton
cloth:skinVertices(jointMatrices, false)
```

### Physics2D Bodies
```lua
crayon.physics2D.setGravity(0, 980)
crayon.physics2D.setMeterScale(32.0)

local ground = crayon.physics2D.createBody("static", 160, 220)
ground:addBox(320, 20)

local box = crayon.physics2D.createBody("dynamic", 160, 100, {
    fixedRotation = false,
    linearDamping = 0.05,
    gravityScale = 1.0
})
box:addBox(20, 20, 0, 0, 0, 1.0, 0.3, 0.1)

local bx, by = box:getPosition()
box:setLinearVelocity(50, 0)
box:applyImpulse(0, 200)
box:setAwake(true)
```

### Physics2D Joints
```lua
local j = crayon.physics2D.createRevoluteJoint(
    bodyA, bodyB,
    ax, ay,
    true, -1.57, 1.57,     -- enableLimit, lower, upper
    true, 2.0, 100.0,      -- enableMotor, speed, maxTorque
    false                  -- collideConnected
)
j:destroy()
```

## Input API Examples

Input is split into three independent sub-tables: `crayon.key`, `crayon.mouse`, and `crayon.gamepad`. The legacy `crayon.input` alias table has been **removed** — use the sub-tables directly.

```lua
-- Keyboard (crayon.key)
if crayon.key.isDown("w") then moveForward() end
if crayon.key.isPressed("space") then jump() end
if crayon.key.isReleased("shift") then stopSprint() end
if crayon.key.isDown("w", "up") then moveForward() end   -- any-of semantics

if crayon.key.isShiftDown() and crayon.key.isPressed("s") then
    print("Shift+S pressed!")
end

-- Mouse (crayon.mouse)
local mx, my   = crayon.mouse.getPosition()
local dx, dy   = crayon.mouse.getDelta()
local wx, wy   = crayon.mouse.getWheel()
if crayon.mouse.isPressed("left") then
    print("Left click at:", mx, my)
end

-- Text input / clipboard
crayon.key.setTextInput(true)
local text = crayon.key.getTextInput()
crayon.key.setClipboard("Hello!")

-- Gamepad (crayon.gamepad)
if crayon.gamepad.isDown(0) then fire() end             -- A button
local lx = crayon.gamepad.getAxis("leftx")
local ly = crayon.gamepad.getAxis("lefty")
print(crayon.gamepad.getCount(), crayon.gamepad.getName(0))
```

## Window API Examples

```lua
-- Basic setup
crayon.window.setResolution(320, 240)
crayon.window.setWindowSize(960, 720)
crayon.window.setTitle("My Game")
crayon.window.setVsync(true)

-- Scaling modes
crayon.window.setScalingMode("integer")  -- Pixel-perfect
crayon.window.setScalingMode("aspect")   -- Letterbox
crayon.window.setScalingMode("stretch")  -- Fill screen
crayon.window.setScalingMode("center")   -- 1:1 unscaled

-- Fullscreen toggle
if crayon.key.isPressed("f11") then
    crayon.window.setFullscreen(not crayon.window.isFullscreen())
end

-- Window state
crayon.window.maximize()
crayon.window.minimize()
crayon.window.restore()
crayon.window.setPosition(100, 100)
crayon.window.center()

-- Advanced
crayon.window.setOpacity(0.9)
crayon.window.setAlwaysOnTop(true)
crayon.window.setMouseGrab(true)
crayon.window.showCursor(false)

-- Get info
local w, h   = crayon.window.getWindowSize()
local rx, ry = crayon.window.getResolution()
local fps    = crayon.window.getFps()
local dw, dh = crayon.window.getDisplaySize()
```

## Audio API Examples

```lua
-- Sound effects
local coin = crayon.audio.loadSound("coin.wav")
crayon.audio.playSound(coin, { volume = 0.8, pitch = 1.0, priority = 5 })

-- Positional 3D audio
crayon.audio.setListenerPosition(0, 2, 0)
crayon.audio.setListenerOrientation(0, 0, -1, 0, 1, 0)
crayon.audio.playSound3D(coin, 5, 1, 3, { volume = 1.0, minDist = 1, maxDist = 25 })

-- Music
crayon.audio.playMusic("bgm.ogg", { loop = true, fadeIn = 2.0 })
crayon.audio.setMusicVolume(0.6)
crayon.audio.stopMusic(1.0)

-- Volume control
crayon.audio.setMasterVolume(0.9)
crayon.audio.setSfxVolume(0.7)
```

### Procedural Sound

```lua
-- One-shot synth voice from a wave template
local blip = crayon.audio.createSound({
    wave = "square", freq = 880, duration = 0.12,
    envelope = { attack = 0.001, decay = 0.03, sustain = 0.4, release = 0.08 },
    filter   = { cutoff = 3000, highpass = true },
    pitchSweep = { from = 1200, to = 400, time = 0.1 }
})
crayon.audio.playSound(blip)

-- Bake a buffer by sampling a Lua function
local pad = crayon.audio.createBuffer(44100, function(t, i)
    return math.sin(t * 220.0 * 6.28318) * 0.4
end)
crayon.audio.playSound(pad)
```

### Live Block Generator

```lua
local voice = crayon.audio.playSound(crayon.audio.loadSound("placeholder.wav"))

crayon.audio.setBlockGenerator(voice, 256, function(tStart, sampleRate, frames)
    local out = {}
    for i = 0, frames - 1 do
        local t = tStart + i / sampleRate
        out[i + 1] = math.sin(t * 220.0 * 6.28318) * math.exp(-t * 2.0) * 0.6
    end
    return out
end)

-- Later:
crayon.audio.clearBlockGenerator(voice)
```

### Live Voice Control

```lua
crayon.audio.setVoiceGain(voice, 0.5, 0.25)   -- gain, ramp seconds
crayon.audio.setVoicePitch(voice, 0.8)
crayon.audio.setVoicePan(voice, -0.5)
crayon.audio.setVoicePosition(voice, 5, 1, 3)
crayon.audio.setVoiceLoop(voice, true)
```

### Audio Stats

```lua
print(crayon.audio.getActiveVoiceCount(), "/", crayon.audio.getMaxVoices())
```

## Particles API Examples

```lua
local fire = crayon.particles.createEmitter({
    maxParticles = 500,
    emissionRate = 100,
    lifetimeMin  = 0.3,
    lifetimeMax  = 0.8,
    sizeStart    = 0.6,
    sizeEnd      = 0.1,
    gravity      = -2.0,
    blendMode    = "additive",
    is3d         = true
})
fire:setPosition(0, 1, 0)

function crayon.update(dt)
    fire:update(dt)
    if crayon.key.isPressed("space") then
        fire:burst(100)
    end
end

function crayon.draw()
    fire:draw3D()
end
```

## Time API

```lua
local total = crayon.time.getTime()   -- total elapsed seconds
local dt    = crayon.time.getDt()     -- frame delta time
local fps   = crayon.time.getFps()    -- frames per second
```

## Math API

```lua
-- Scalars
crayon.math.lerp(a, b, t)
crayon.math.clamp(v, min, max)
crayon.math.smoothstep(e0, e1, x)
crayon.math.remap(v, inMin, inMax, outMin, outMax)
crayon.math.damp(a, b, lambda, dt)
crayon.math.noise(x [, y])

-- Vectors
crayon.math.vec2Normalize(x, y)
crayon.math.vec3Cross(x1,y1,z1, x2,y2,z2)
crayon.math.vec3Distance(x1,y1,z1, x2,y2,z2)

-- Quaternions
crayon.math.quatFromEuler(pitch, yaw, roll)
crayon.math.quatSlerp(q1x,q1y,q1z,q1w, q2x,q2y,q2z,q2w, t)
```

## File System API

```lua
local data = crayon.fs.readText("config.json")
crayon.fs.writeText("saves/slot1.dat", "player data")
crayon.fs.appendText("log.txt", "new line\n")

if crayon.fs.exists("mods/") then
    for _, f in ipairs(crayon.fs.listDir("mods/")) do
        print(f)
    end
end

local save_dir = crayon.fs.getSaveDir("MyStudio", "MyGame")
```

## Print API

`crayon.print(...)` is a colorized, cycle-safe pretty-printer for Lua values, ideal for debugging nested tables.

```lua
crayon.print("player", { hp = 100, pos = {1, 2, 3}, alive = true })
-- player {
--   alive = true,
--   hp = 100,
--   pos = { 1, 2, 3 }
-- }
```

## Lua Callbacks

The runtime invokes these optional `crayon.*` functions if they are defined:

| Callback | Args |
|----------|------|
| `crayon.config(t)` | t |
| `crayon.init()` | — |
| `crayon.update(dt)` | dt |
| `crayon.draw()` | — |
| `crayon.quit()` | — |
| `crayon.keydown(key, isRepeat)` | key, is_repeat |
| `crayon.keyup(key)` | key |
| `crayon.mousedown(x, y, button)` | x, y, btn |
| `crayon.mouseup(x, y, button)` | x, y, btn |
| `crayon.mousemoved(x, y, dx, dy)` | x, y, dx, dy |
| `crayon.wheelmoved(dx, dy)` | dx, dy |
| `crayon.textinput(text)` | text |
| `crayon.gamepaddown(btn)` | btn |
| `crayon.gamepadup(btn)` | btn |
| `crayon.gamepadaxis(axis, value)` | axis, value |
| `crayon.windowResized(w, h)` | w, h |
| `crayon.focusChanged(focused)` | bool |
| `crayon.onCollisionEnter(a, b, nx, ny, nz, impulse)` | body ids + contact info |
| `crayon.onCollisionExit(a, b)` | body ids |
| `crayon.onTriggerEnter(sensor, other)` | body ids |
| `crayon.onTriggerExit(sensor, other)` | body ids |
| `crayon.onCollision2DEnter(a, b, nx, ny, impulse)` | body ids + contact info |
| `crayon.onCollision2DExit(a, b)` | body ids |
| `crayon.onTrigger2DEnter(sensor, other)` | body ids |
| `crayon.onTrigger2DExit(sensor, other)` | body ids |
| `crayon.dropBegin(x, y)` | x, y |
| `crayon.dropFile(path, x, y)` | path, x, y |
| `crayon.dropText(text, x, y)` | text, x, y |
| `crayon.dropPosition(x, y)` | x, y |
| `crayon.dropComplete()` | — |

## Build Requirements

| Dependency | Version | Purpose |
|------------|---------|---------|
| C++ Compiler | C++20 | Language standard |
| OpenGL | 3.3+ | Graphics rendering |
| SDL3 | Latest | Windowing & input |
| GLM | Latest | Math operations |
| GLAD | Latest | OpenGL loading |
| Jolt Physics | Latest | 3D physics |
| Box2D | Latest | 2D physics |
| LuaJIT | Latest | Scripting |
| stb_image | Latest | Texture loading |
| tinyobjloader | Latest | OBJ model loading |


## License

MIT License — See LICENSE file for details.