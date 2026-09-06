-- ============================================================================
-- Example 06: Orthographic 3D Isometric Diorama & World Builder
-- Crayon Engine
-- ============================================================================

local cam = {
    yaw = 45.0,
    pitch = -35.264, -- Classic true isometric angle: asin(tan(30 deg))
    ortho_size = 8.0,
    target_x = 0.0,
    target_y = 0.0,
    target_z = 0.0
}

local textures = {}
local cursor_x = 0
local cursor_z = 0
local timer = 0

-- Grid of placed blocks: map key "x,z" -> height
local blocks = {}

function set_block(x, z, h)
    blocks[x .. "," .. z] = h
end

function get_block(x, z)
    return blocks[x .. "," .. z] or 0
end

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("06 - Isometric Ortho 3D [Arrows: Move Cursor, SPACE: Build, Q/E: Rotate]")

    textures.crate = crayon.graphics.load_texture("game/assets/textures/crate.bmp")
    textures.brick = crayon.graphics.load_texture("game/assets/textures/brick.bmp")
    textures.grass = crayon.graphics.load_texture("game/assets/textures/grass.bmp")

    crayon.graphics.set_retro_effects({
        jitter_resolution = {240, 160},
        affine = 1.0,
        dither = true
    })

    -- Warm isometric directional lighting
    crayon.graphics.set_light(
        -0.6, -1.0, -0.4,
        1.0, 0.95, 0.85,
        0.35, 0.35, 0.4
    )

    -- Preset building structures
    for x = -3, 3 do
        for z = -3, 3 do
            if math.abs(x) == 3 or math.abs(z) == 3 then
                set_block(x, z, 1)
            end
        end
    end
    set_block(0, 0, 3)
    set_block(0, 1, 2)
    set_block(1, 0, 2)
    set_block(-1, 0, 1)
end

function crayon.update(dt)
    timer = timer + dt

    -- Cursor movement on isometric grid
    if crayon.input.is_pressed("up") or crayon.input.is_pressed("w") then
        cursor_z = math.max(-4, cursor_z - 1)
    end
    if crayon.input.is_pressed("down") or crayon.input.is_pressed("s") then
        cursor_z = math.min(4, cursor_z + 1)
    end
    if crayon.input.is_pressed("left") or crayon.input.is_pressed("a") then
        cursor_x = math.max(-4, cursor_x - 1)
    end
    if crayon.input.is_pressed("right") or crayon.input.is_pressed("d") then
        cursor_x = math.min(4, cursor_x + 1)
    end

    -- Build block (SPACE) / Remove block (X or BACKSPACE)
    if crayon.input.is_pressed("space") then
        local cur_h = get_block(cursor_x, cursor_z)
        if cur_h < 5 then
            set_block(cursor_x, cursor_z, cur_h + 1)
        end
    end
    if crayon.input.is_pressed("x") or crayon.input.is_pressed("backspace") then
        local cur_h = get_block(cursor_x, cursor_z)
        if cur_h > 0 then
            set_block(cursor_x, cursor_z, cur_h - 1)
        end
    end

    -- Rotate isometric view by 90 degrees (Q / E)
    if crayon.input.is_pressed("q") then
        cam.yaw = cam.yaw - 90.0
    end
    if crayon.input.is_pressed("e") then
        cam.yaw = cam.yaw + 90.0
    end

    -- Zoom in / out (Scroll Wheel or R / F)
    local wheel_y = crayon.input.get_mouse_wheel() or 0
    if wheel_y ~= 0 then
        cam.ortho_size = math.max(4.0, math.min(16.0, cam.ortho_size - wheel_y * 1.0))
    end
    if crayon.input.is_down("r") then
        cam.ortho_size = math.max(4.0, cam.ortho_size - 6.0 * dt)
    end
    if crayon.input.is_down("f") then
        cam.ortho_size = math.min(16.0, cam.ortho_size + 6.0 * dt)
    end

    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.09, 0.11, 0.16)

    -- Setup Orthographic 3D Camera!
    local dist = 25.0
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local cx = math.cos(rad_pitch) * math.cos(rad_yaw) * dist
    local cy = -math.sin(rad_pitch) * dist
    local cz = math.cos(rad_pitch) * math.sin(rad_yaw) * dist

    crayon.graphics.set_camera3d({
        position = {cx, cy, cz},
        target = {cam.target_x, cam.target_y, cam.target_z},
        up = {0, 1, 0},
        ortho = true,
        ortho_size = cam.ortho_size,
        near = 0.1,
        far = 100.0
    })

    -- 1. Base Island Foundation
    crayon.graphics.set_color(0.7, 0.85, 0.7, 1.0)
    crayon.graphics.draw_plane(0, -0.01, 0, 0, 0, 0, 5, 1, 5, textures.grass)

    -- 2. Floor Grid
    crayon.graphics.set_color(0.2, 0.35, 0.45, 0.5)
    crayon.graphics.draw_grid_3d(10, 1.0)

    -- 3. Draw All Placed Blocks
    for x = -4, 4 do
        for z = -4, 4 do
            local h = get_block(x, z)
            for y = 1, h do
                local by = y - 0.5
                local tex = (y == 1) and textures.brick or textures.crate
                crayon.graphics.set_color(0.9, 0.9, 0.95, 1.0)
                crayon.graphics.draw_cube(x, by, z, 0, 0, 0, 0.98, 0.98, 0.98, tex)
            end
        end
    end

    -- 4. Animated Isometric Cursor Wireframe Box
    local cur_top = get_block(cursor_x, cursor_z)
    local cursor_y = cur_top + 0.5 + math.sin(timer * 6.0) * 0.08

    crayon.graphics.set_shading_mode("unlit")
    crayon.graphics.set_color(1.0, 0.8, 0.1, 0.7)
    crayon.graphics.draw_cube(cursor_x, cursor_y, cursor_z, 0, 0, 0, 1.02, 1.02, 1.02)
    crayon.graphics.set_shading_mode("gouraud")

    -- ========================================================================
    -- 2D HUD OVERLAY
    -- ========================================================================
    crayon.graphics.set_color(0.06, 0.08, 0.14, 0.85)
    crayon.graphics.draw_rect("fill", 4, 4, 312, 22)
    crayon.graphics.set_color(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.draw_rect("line", 4, 4, 312, 22)

    crayon.graphics.set_color(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.draw_text("3D ISOMETRIC (ORTHOGRAPHIC)", 8, 10, 1.0)

    crayon.graphics.set_color(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.draw_text("Cursor: (" .. cursor_x .. ", " .. cursor_z .. ")", 215, 10, 1.0)

    -- Bottom Controls
    crayon.graphics.set_color(0.06, 0.08, 0.14, 0.85)
    crayon.graphics.draw_rect("fill", 4, 204, 312, 32)
    crayon.graphics.set_color(0.25, 0.35, 0.6, 1.0)
    crayon.graphics.draw_rect("line", 4, 204, 312, 32)

    crayon.graphics.set_color(0.9, 0.9, 0.95, 1.0)
    crayon.graphics.draw_text("Arrows: Move | [SPACE]: Place | [X]: Delete", 10, 208, 1.0)
    crayon.graphics.set_color(0.6, 0.75, 0.9, 1.0)
    crayon.graphics.draw_text("Q/E: Rotate View 90 deg | Scroll/R/F: Zoom", 10, 222, 1.0)
end
