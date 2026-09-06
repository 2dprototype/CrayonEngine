-- ============================================================================
-- Example 03: Complete 3D Primitives Gallery & Shading Modes
-- Crayon Engine
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

local primitives = {
    { name = "Cube",     draw = function(...) crayon.graphics.draw_cube(...) end },
    { name = "Sphere",   draw = function(...) crayon.graphics.draw_sphere(...) end },
    { name = "Cylinder", draw = function(...) crayon.graphics.draw_cylinder(...) end },
    { name = "Cone",     draw = function(...) crayon.graphics.draw_cone(...) end },
    { name = "Pyramid",  draw = function(...) crayon.graphics.draw_pyramid(...) end },
    { name = "Torus",    draw = function(...) crayon.graphics.draw_torus(...) end },
    { name = "Capsule",  draw = function(...) crayon.graphics.draw_capsule(...) end },
    { name = "Plane",    draw = function(...) crayon.graphics.draw_plane(...) end }
}

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("03 - 3D Primitives Gallery [1: Shading, 2: Textures]")

    textures.crate = crayon.graphics.load_texture("game/assets/textures/crate.bmp")
    textures.brick = crayon.graphics.load_texture("game/assets/textures/brick.bmp")
    textures.grass = crayon.graphics.load_texture("game/assets/textures/grass.bmp")
    textures.coin  = crayon.graphics.load_texture("game/assets/textures/coin.bmp")

    crayon.graphics.set_retro_effects({
        jitter_resolution = {240, 160},
        affine = 1.0,
        dither = true
    })

    -- Warm sunlight
    crayon.graphics.set_light(
        -0.5, -0.8, -0.4,
        1.0, 0.95, 0.85,
        0.25, 0.25, 0.3
    )

    -- Dynamic colored point light
    crayon.graphics.set_point_light_enabled(0, true)
end

function crayon.update(dt)
    timer = timer + dt

    -- Camera Orbit Controls
    if crayon.input.is_down("left") or crayon.input.is_down("a") then
        cam.yaw = cam.yaw - 45.0 * dt
    end
    if crayon.input.is_down("right") or crayon.input.is_down("d") then
        cam.yaw = cam.yaw + 45.0 * dt
    end
    if crayon.input.is_down("up") or crayon.input.is_down("w") then
        cam.pitch = math.min(-5.0, cam.pitch + 35.0 * dt)
    end
    if crayon.input.is_down("down") or crayon.input.is_down("s") then
        cam.pitch = math.max(-80.0, cam.pitch - 35.0 * dt)
    end

    -- Zoom
    local wheel_y = crayon.input.get_mouse_wheel() or 0
    if wheel_y ~= 0 then
        cam.dist = math.max(4.0, math.min(22.0, cam.dist - wheel_y * 1.0))
    end

    -- Toggle Shading Mode
    if crayon.input.is_pressed("1") or crayon.input.is_pressed("tab") then
        cur_shading_idx = (cur_shading_idx % #shading_modes) + 1
        crayon.graphics.set_shading_mode(shading_modes[cur_shading_idx])
    end

    -- Toggle Textures
    if crayon.input.is_pressed("2") then
        use_textures = not use_textures
    end

    -- Update animated point light position & color
    local light_x = math.cos(timer * 2.0) * 3.5
    local light_z = math.sin(timer * 2.0) * 3.5
    crayon.graphics.set_point_light(
        0,
        light_x, 1.8, light_z,
        0.3, 0.8, 1.0,     -- Cyan glow
        8.0, 2.5           -- Radius & Intensity
    )

    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.07, 0.08, 0.12)

    -- Calculate camera orbit position
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local cx = math.cos(rad_pitch) * math.cos(rad_yaw) * cam.dist
    local cy = -math.sin(rad_pitch) * cam.dist + cam.target_y
    local cz = math.cos(rad_pitch) * math.sin(rad_yaw) * cam.dist

    crayon.graphics.set_camera3d({
        position = {cx, cy, cz},
        target = {0, cam.target_y, 0},
        up = {0, 1, 0},
        fov = 55.0,
        near = 0.1,
        far = 100.0
    })

    -- 1. Reference Floor Grid
    crayon.graphics.set_color(0.2, 0.3, 0.45, 0.5)
    crayon.graphics.draw_grid_3d(16, 0.8)

    -- 2. World Coordinate Axes
    crayon.graphics.set_color(1.0, 0.2, 0.2, 1.0)
    crayon.graphics.draw_line_3d(0, 0.01, 0, 1.5, 0.01, 0) -- X = Red
    crayon.graphics.set_color(0.2, 1.0, 0.2, 1.0)
    crayon.graphics.draw_line_3d(0, 0, 0, 0, 1.5, 0)       -- Y = Green
    crayon.graphics.set_color(0.2, 0.4, 1.0, 1.0)
    crayon.graphics.draw_line_3d(0, 0.01, 0, 0, 0.01, 1.5) -- Z = Blue

    -- 3. Arrange 8 Primitives in a Circle
    local num_prim = #primitives
    local circle_r = 4.0
    local tex_list = {textures.crate, textures.brick, textures.grass, textures.coin}

    for i, prim in ipairs(primitives) do
        local angle = ((i - 1) / num_prim) * math.pi * 2
        local px = math.cos(angle) * circle_r
        local pz = math.sin(angle) * circle_r
        local rot = timer * 40.0 + i * 45.0
        local rad_rot = math.rad(rot)

        -- Cylindrical Pedestal
        crayon.graphics.set_color(0.35, 0.38, 0.45, 1.0)
        crayon.graphics.draw_cylinder(px, -0.3, pz, 0, 0, 0, 0.65, 0.6, 0.65, use_textures and textures.brick or nil)

        -- Selected Texture
        local tex = use_textures and tex_list[((i - 1) % #tex_list) + 1] or nil

        -- Primary Rotating Primitive
        crayon.graphics.set_color(0.9, 0.9, 0.95, 1.0)
        prim.draw(px, 0.7 + math.sin(timer * 2.0 + i) * 0.15, pz,
                  rad_rot * 0.6, rad_rot, 0,
                  0.75, 0.75, 0.75,
                  tex)
    end

    -- 4. Dynamic Point Light Marker (Small glowing sphere)
    local light_x = math.cos(timer * 2.0) * 3.5
    local light_z = math.sin(timer * 2.0) * 3.5
    crayon.graphics.set_shading_mode("unlit")
    crayon.graphics.set_color(0.3, 0.9, 1.0, 1.0)
    crayon.graphics.draw_sphere(light_x, 1.8, light_z, 0, 0, 0, 0.15, 0.15, 0.15)
    -- Restore active shading mode
    crayon.graphics.set_shading_mode(shading_modes[cur_shading_idx])

    -- ========================================================================
    -- 2D HUD OVERLAY
    -- ========================================================================
    crayon.graphics.set_color(0.08, 0.1, 0.16, 0.85)
    crayon.graphics.draw_rect("fill", 4, 4, 312, 22)
    crayon.graphics.set_color(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.draw_rect("line", 4, 4, 312, 22)

    crayon.graphics.set_color(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.draw_text("3D PRIMITIVES & SHADING GALLERY", 8, 10, 1.0)

    local fps = math.floor(crayon.window.get_fps() + 0.5)
    crayon.graphics.set_color(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.draw_text("FPS:" .. fps, 265, 10, 1.0)

    -- Bottom Controls & Mode Bar
    crayon.graphics.set_color(0.06, 0.08, 0.14, 0.9)
    crayon.graphics.draw_rect("fill", 4, 200, 312, 36)
    crayon.graphics.set_color(0.25, 0.35, 0.6, 1.0)
    crayon.graphics.draw_rect("line", 4, 200, 312, 36)

    crayon.graphics.set_color(0.9, 0.9, 0.3, 1.0)
    crayon.graphics.draw_text("[1/TAB] Shading: " .. string.upper(shading_modes[cur_shading_idx]), 10, 205, 1.0)

    crayon.graphics.set_color(0.4, 0.85, 1.0, 1.0)
    crayon.graphics.draw_text("[2] Textures: " .. (use_textures and "ON " or "OFF"), 170, 205, 1.0)

    crayon.graphics.set_color(0.7, 0.75, 0.85, 1.0)
    crayon.graphics.draw_text("WASD / Arrows: Orbit Camera | Scroll: Zoom", 10, 220, 1.0)
end
