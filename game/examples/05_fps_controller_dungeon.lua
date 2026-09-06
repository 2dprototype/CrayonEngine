-- ============================================================================
-- Example 05: 3D FPS Dungeon with Relative Mouse Look & Torch Light
-- Crayon Engine
-- ============================================================================

local cam = {
    x = 0.0,
    y = 0.8,
    z = 1.5,
    yaw = 0.0,
    pitch = 0.0,
    fov = 65.0
}

local mouse_captured = true
local textures = {}
local timer = 0
local score = 0

-- Pickups placed in dungeon (first one directly in line of sight!)
local coins = {
    {x =  0, z = -1.5, active = true},
    {x = -3, z = -3.0, active = true},
    {x =  3, z = -3.0, active = true},
    {x = -3, z =  1.0, active = true},
    {x =  3, z =  1.0, active = true},
    {x =  0, z = -5.0, active = true}
}

-- Dungeon map layout (1 = wall, 0 = empty hall)
local map_w = 9
local map_h = 9
local map = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1
}

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("05 - FPS Dungeon [WASD: Move, Mouse: Look, M: Mouse Lock]")

    textures.crate = crayon.graphics.load_texture("game/assets/textures/crate.bmp")
    textures.brick = crayon.graphics.load_texture("game/assets/textures/brick.bmp")
    textures.coin  = crayon.graphics.load_texture("game/assets/textures/coin.bmp")

    crayon.graphics.set_retro_effects({
        jitter_resolution = {160, 120},
        affine = 1.0,
        dither = true,
        fog = { start = 4.0, ["end"] = 18.0, color = {0.04, 0.04, 0.07} }
    })

    -- Low dark ambient light for moody dungeon atmosphere
    crayon.graphics.set_light(
        -0.2, -1.0, -0.2,
        0.2, 0.2, 0.25,      -- Dim directional
        0.1, 0.1, 0.15       -- Ambient
    )

    -- Enable torch point light
    crayon.graphics.set_point_light_enabled(0, true)

    -- Lock mouse cursor for FPS look
    crayon.window.set_mouse_relative(true)
    crayon.window.show_cursor(false)
end

function crayon.update(dt)
    timer = timer + dt

    -- Toggle mouse capture with 'M'
    if crayon.input.is_pressed("m") then
        mouse_captured = not mouse_captured
        crayon.window.set_mouse_relative(mouse_captured)
        crayon.window.show_cursor(not mouse_captured)
    end

    -- Mouse Look
    if mouse_captured then
        local mdx, mdy = crayon.input.get_mouse_delta()
        local sensitivity = 0.22
        cam.yaw = cam.yaw + mdx * sensitivity
        cam.pitch = math.max(-85.0, math.min(85.0, cam.pitch - mdy * sensitivity))
    end

    -- Arrow keys fallback look
    if crayon.input.is_down("left") then cam.yaw = cam.yaw - 90.0 * dt end
    if crayon.input.is_down("right") then cam.yaw = cam.yaw + 90.0 * dt end
    if crayon.input.is_down("up") then cam.pitch = math.min(85.0, cam.pitch + 70.0 * dt) end
    if crayon.input.is_down("down") then cam.pitch = math.max(-85.0, cam.pitch - 70.0 * dt) end

    -- FPS WASD Movement
    local move_spd = 3.5 * dt
    local rad_yaw = math.rad(cam.yaw)
    local fwd_x = math.sin(rad_yaw)
    local fwd_z = -math.cos(rad_yaw)
    local right_x = math.cos(rad_yaw)
    local right_z = math.sin(rad_yaw)

    local move_x, move_z = 0, 0
    if crayon.input.is_down("w") then
        move_x = move_x + fwd_x * move_spd
        move_z = move_z + fwd_z * move_spd
    end
    if crayon.input.is_down("s") then
        move_x = move_x - fwd_x * move_spd
        move_z = move_z - fwd_z * move_spd
    end
    if crayon.input.is_down("a") then
        move_x = move_x - right_x * move_spd
        move_z = move_z - right_z * move_spd
    end
    if crayon.input.is_down("d") then
        move_x = move_x + right_x * move_spd
        move_z = move_z + right_z * move_spd
    end

    -- Simple wall collision
    local new_x = cam.x + move_x
    local new_z = cam.z + move_z
    local cell_x = math.floor((new_x + 9) / 2) + 1
    local cell_z = math.floor((new_z + 9) / 2) + 1

    if cell_x >= 1 and cell_x <= map_w and cell_z >= 1 and cell_z <= map_h then
        local idx = (cell_z - 1) * map_w + cell_x
        if map[idx] == 0 then
            cam.x = new_x
            cam.z = new_z
        end
    else
        cam.x = new_x
        cam.z = new_z
    end

    -- Dynamic Flickering Torch Light at player position
    local flicker = math.sin(timer * 15.0) * 0.05 + math.cos(timer * 23.0) * 0.04
    crayon.graphics.set_point_light(
        0,
        cam.x, cam.y, cam.z,
        1.0, 0.75 + flicker, 0.4, -- Warm firelight
        6.5 + flicker * 2.0,       -- Radius
        3.2 + flicker              -- Intensity
    )

    -- Collect Pickups
    for _, c in ipairs(coins) do
        if c.active then
            local dist = math.sqrt((cam.x - c.x)^2 + (cam.z - c.z)^2)
            if dist < 0.8 then
                c.active = false
                score = score + 100
            end
        end
    end

    if crayon.input.is_pressed("escape") then
        if mouse_captured then
            mouse_captured = false
            crayon.window.set_mouse_relative(false)
            crayon.window.show_cursor(true)
        else
            crayon.window.quit()
        end
    end
end

function crayon.draw()
    crayon.graphics.clear(0.04, 0.04, 0.07)

    -- Calculate Look Target Vector
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local look_x = math.sin(rad_yaw) * math.cos(rad_pitch)
    local look_y = math.sin(rad_pitch)
    local look_z = -math.cos(rad_yaw) * math.cos(rad_pitch)

    crayon.graphics.set_camera3d({
        position = {cam.x, cam.y, cam.z},
        target = {cam.x + look_x, cam.y + look_y, cam.z + look_z},
        up = {0, 1, 0},
        fov = cam.fov,
        near = 0.1,
        far = 40.0
    })

    -- 1. Dungeon Floor & Ceiling
    crayon.graphics.set_color(0.5, 0.5, 0.55, 1.0)
    crayon.graphics.draw_plane(0, 0, 0, 20, 20, textures.brick)
    crayon.graphics.draw_plane(0, 2.0, 0, 20, 20, textures.brick, math.pi, 0, 0)

    -- 2. Dungeon Brick Walls
    for cz = 1, map_h do
        for cx = 1, map_w do
            local idx = (cz - 1) * map_w + cx
            if map[idx] == 1 then
                local wx = (cx - 1) * 2 - 8
                local wz = (cz - 1) * 2 - 8
                crayon.graphics.set_color(0.8, 0.8, 0.85, 1.0)
                crayon.graphics.draw_cube(wx, 1.0, wz, 2.0, 2.0, 2.0, textures.brick)
            end
        end
    end

    -- 3. Animated Floating Coin Pickups
    for _, c in ipairs(coins) do
        if c.active then
            local float_y = 0.7 + math.sin(timer * 4.0) * 0.1
            crayon.graphics.set_color(1.0, 0.95, 0.3, 1.0)
            crayon.graphics.draw_billboard(c.x, float_y, c.z, 0.8, 0.8, textures.coin, "spherical")
        end
    end

    -- ========================================================================
    -- 2D HUD OVERLAY
    -- ========================================================================
    -- Crosshair
    local cx, cy = 160, 120
    crayon.graphics.set_color(1.0, 1.0, 1.0, 0.7)
    crayon.graphics.draw_line(cx - 5, cy, cx + 5, cy, 1.0)
    crayon.graphics.draw_line(cx, cy - 5, cx, cy + 5, 1.0)

    -- Top Header Panel
    crayon.graphics.set_color(0.06, 0.08, 0.12, 0.85)
    crayon.graphics.draw_rect("fill", 4, 4, 312, 22)
    crayon.graphics.set_color(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.draw_rect("line", 4, 4, 312, 22)

    crayon.graphics.set_color(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.draw_text("3D FPS DUNGEON CRAWLER", 8, 10, 1.0)

    crayon.graphics.set_color(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.draw_text("SCORE: " .. score, 235, 10, 1.0)

    -- Bottom Controls & Status
    crayon.graphics.set_color(0.06, 0.08, 0.12, 0.85)
    crayon.graphics.draw_rect("fill", 4, 218, 312, 18)
    crayon.graphics.set_color(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.draw_rect("line", 4, 218, 312, 18)

    crayon.graphics.set_color(0.85, 0.9, 0.95, 1.0)
    local status = mouse_captured and "Mouse: LOCKED (FPS Look)" or "Mouse: UNLOCKED"
    crayon.graphics.draw_text(status .. " | [M]: Toggle Lock | [ESC]: Quit", 8, 222, 1.0)
end
