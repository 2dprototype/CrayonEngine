-- ============================================================================
-- Example 03: Complete 3D Primitives Gallery & Shading Modes
-- Crayon Engine
--
-- Crayon 3D primitive signatures (arg order matters!):
--   drawCube     (x, y, z, sx, sy, sz, tex, rx, ry, rz)
--   drawSphere   (x, y, z, radius, tex, rx, ry, rz)
--   drawCylinder (x, y, z, radius, height, tex, rx, ry, rz)
--   drawCone     (x, y, z, radius, height, tex, rx, ry, rz)
--   drawPyramid  (x, y, z, baseSize, height, tex, rx, ry, rz)
--   drawTorus    (x, y, z, radius, tube, tex, rx, ry, rz)
--   drawCapsule  (x, y, z, radius, height, tex, rx, ry, rz)
--   drawPlane    (x, y, z, w, d, tex, rx, ry, rz)
-- ============================================================================

local cam = {
    yaw = 45.0,
    pitch = -25.0,
    dist = 11.0,
    target_y = 0.5
}

local textures = {}
local shading_modes = {"flat", "gouraud", "unlit"}
local cur_shading_idx = 1
local use_textures = true
local timer = 0

-- Each primitive provides a uniform interface:
--     draw(x, y, z, scale, tex, rx, ry, rz)
-- The wrapper adapts that to the real Crayon signature.
local primitives = {
    {
        name = "Cube",
        draw = function(x, y, z, s, tex, rx, ry, rz)
            crayon.graphics.drawCube(x, y, z, s, s, s, tex, rx, ry, rz)
        end
    },
    {
        name = "Sphere",
        draw = function(x, y, z, s, tex, rx, ry, rz)
            crayon.graphics.drawSphere(x, y, z, s * 0.6, tex, rx, ry, rz)
        end
    },
    {
        name = "Cylinder",
        draw = function(x, y, z, s, tex, rx, ry, rz)
            crayon.graphics.drawCylinder(x, y, z, s * 0.45, s * 1.2, tex, rx, ry, rz)
        end
    },
    {
        name = "Cone",
        draw = function(x, y, z, s, tex, rx, ry, rz)
            crayon.graphics.drawCone(x, y, z, s * 0.5, s * 1.2, tex, rx, ry, rz)
        end
    },
    {
        name = "Pyramid",
        draw = function(x, y, z, s, tex, rx, ry, rz)
            crayon.graphics.drawPyramid(x, y, z, s, s * 1.2, tex, rx, ry, rz)
        end
    },
    {
        name = "Torus",
        draw = function(x, y, z, s, tex, rx, ry, rz)
            crayon.graphics.drawTorus(x, y, z, s * 0.8, s * 0.3, tex, rx, ry, rz)
        end
    },
    {
        name = "Capsule",
        draw = function(x, y, z, s, tex, rx, ry, rz)
            crayon.graphics.drawCapsule(x, y, z, s * 0.4, s * 0.9, tex, rx, ry, rz)
        end
    },
    {
        name = "Plane",
        draw = function(x, y, z, s, tex, rx, ry, rz)
            -- Plane lies flat by default; tilt it up so it faces the camera
            crayon.graphics.drawPlane(x, y, z, s * 1.6, s * 1.6, tex, math.pi * 0.5, ry, rz)
        end
    }
}

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("03 - 3D Primitives Gallery [1: Shading, 2: Textures]")

    textures.crate = crayon.graphics.loadTexture("game/assets/textures/crate.bmp")
    textures.brick = crayon.graphics.loadTexture("game/assets/textures/brick.bmp")
    textures.grass = crayon.graphics.loadTexture("game/assets/textures/grass.bmp")
    textures.coin  = crayon.graphics.loadTexture("game/assets/textures/coin.bmp")

    crayon.graphics.setRetroEffects({
        jitterResolution = {240, 160},
        affine = 1.0,
        dither = true
    })

    -- Warm sunlight
    crayon.graphics.setLight(
        -0.5, -0.8, -0.4,
        1.0, 0.95, 0.85,
        0.25, 0.25, 0.3
    )

    -- Dynamic colored point light
    crayon.graphics.setPointLightEnabled(0, true)
end

function crayon.update(dt)
    timer = timer + dt

    -- Camera orbit controls
    if crayon.input.isDown("left") or crayon.input.isDown("a") then
        cam.yaw = cam.yaw - 45.0 * dt
    end
    if crayon.input.isDown("right") or crayon.input.isDown("d") then
        cam.yaw = cam.yaw + 45.0 * dt
    end
    if crayon.input.isDown("up") or crayon.input.isDown("w") then
        cam.pitch = math.min(-5.0, cam.pitch + 35.0 * dt)
    end
    if crayon.input.isDown("down") or crayon.input.isDown("s") then
        cam.pitch = math.max(-80.0, cam.pitch - 35.0 * dt)
    end

    -- Zoom
    local wheel_x, wheel_y = crayon.input.getMouseWheel()
    if wheel_y and wheel_y ~= 0 then
        cam.dist = math.max(4.0, math.min(22.0, cam.dist - wheel_y * 1.0))
    end

    -- Toggle shading mode
    if crayon.input.isPressed("1") or crayon.input.isPressed("tab") then
        cur_shading_idx = (cur_shading_idx % #shading_modes) + 1
        crayon.graphics.setShadingMode(shading_modes[cur_shading_idx])
    end

    -- Toggle textures
    if crayon.input.isPressed("2") then
        use_textures = not use_textures
    end

    -- Animated point light
    local light_x = math.cos(timer * 2.0) * 3.5
    local light_z = math.sin(timer * 2.0) * 3.5
    crayon.graphics.setPointLight(
        0,
        light_x, 1.8, light_z,
        0.3, 0.8, 1.0,     -- Cyan glow
        8.0, 2.5           -- Radius & Intensity
    )

    if crayon.input.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.07, 0.08, 0.12)

    -- Camera orbit position
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local cx = math.cos(rad_pitch) * math.cos(rad_yaw) * cam.dist
    local cy = -math.sin(rad_pitch) * cam.dist + cam.target_y
    local cz = math.cos(rad_pitch) * math.sin(rad_yaw) * cam.dist

    crayon.graphics.setCamera3d({
        position = {cx, cy, cz},
        target = {0, cam.target_y, 0},
        up = {0, 1, 0},
        fov = 55.0,
        near = 0.1,
        far = 100.0
    })

    -- 1. Reference floor grid
    crayon.graphics.setColor(0.2, 0.3, 0.45, 0.5)
    crayon.graphics.drawGrid3d(16, 16, 0)

    -- 2. World coordinate axes
    crayon.graphics.setColor(1.0, 0.2, 0.2, 1.0)
    crayon.graphics.drawLine3d(0, 0.01, 0, 1.5, 0.01, 0)   -- X = Red
    crayon.graphics.setColor(0.2, 1.0, 0.2, 1.0)
    crayon.graphics.drawLine3d(0, 0, 0, 0, 1.5, 0)         -- Y = Green
    crayon.graphics.setColor(0.2, 0.4, 1.0, 1.0)
    crayon.graphics.drawLine3d(0, 0.01, 0, 0, 0.01, 1.5)   -- Z = Blue

    -- 3. Arrange 8 primitives in a circle
    local num_prim = #primitives
    local circle_r = 4.0
    local tex_list = {textures.crate, textures.brick, textures.grass, textures.coin}

    for i, prim in ipairs(primitives) do
        local angle = ((i - 1) / num_prim) * math.pi * 2
        local px = math.cos(angle) * circle_r
        local pz = math.sin(angle) * circle_r
        local rot = timer * 40.0 + i * 45.0
        local rad_rot = math.rad(rot)

        -- Cylindrical pedestal
        -- drawCylinder(x, y, z, radius, height, tex, rx, ry, rz)
        crayon.graphics.setColor(0.35, 0.38, 0.45, 1.0)
        crayon.graphics.drawCylinder(
            px, -0.3, pz,
            0.65, 0.6,
            use_textures and textures.brick or 0,
            0, 0, 0
        )

        -- Selected texture for this primitive
        local tex = use_textures and tex_list[((i - 1) % #tex_list) + 1] or 0

        -- Primary rotating primitive, with uniform wrapper interface
        local py = 0.7 + math.sin(timer * 2.0 + i) * 0.15
        crayon.graphics.setColor(0.9, 0.9, 0.95, 1.0)
        prim.draw(px, py, pz, 0.75, tex, rad_rot * 0.6, rad_rot, 0)
    end

    -- 4. Animated point light marker (glowing sphere)
    local light_x = math.cos(timer * 2.0) * 3.5
    local light_z = math.sin(timer * 2.0) * 3.5
    crayon.graphics.setShadingMode("unlit")
    crayon.graphics.setColor(0.3, 0.9, 1.0, 1.0)
    -- drawSphere(x, y, z, radius, tex, rx, ry, rz)
    crayon.graphics.drawSphere(light_x, 1.8, light_z, 0.15, 0, 0, 0, 0)
    crayon.graphics.setShadingMode(shading_modes[cur_shading_idx])

    -- ========================================================================
    -- 2D HUD overlay
    -- ========================================================================
    crayon.graphics.setColor(0.08, 0.1, 0.16, 0.85)
    crayon.graphics.drawRect("fill", 4, 4, 312, 22)
    crayon.graphics.setColor(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.drawRect("line", 4, 4, 312, 22)

    crayon.graphics.setColor(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.drawText("3D PRIMITIVES & SHADING GALLERY", 8, 10, 1.0)

    local fps = math.floor(crayon.window.getFps() + 0.5)
    crayon.graphics.setColor(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.drawText("FPS:" .. fps, 265, 10, 1.0)

    crayon.graphics.setColor(0.06, 0.08, 0.14, 0.9)
    crayon.graphics.drawRect("fill", 4, 200, 312, 36)
    crayon.graphics.setColor(0.25, 0.35, 0.6, 1.0)
    crayon.graphics.drawRect("line", 4, 200, 312, 36)

    crayon.graphics.setColor(0.9, 0.9, 0.3, 1.0)
    crayon.graphics.drawText("[1/TAB] Shading: " .. string.upper(shading_modes[cur_shading_idx]), 10, 205, 1.0)

    crayon.graphics.setColor(0.4, 0.85, 1.0, 1.0)
    crayon.graphics.drawText("[2] Textures: " .. (use_textures and "ON " or "OFF"), 170, 205, 1.0)

    crayon.graphics.setColor(0.7, 0.75, 0.85, 1.0)
    crayon.graphics.drawText("WASD / Arrows: Orbit Camera | Scroll: Zoom", 10, 220, 1.0)
end