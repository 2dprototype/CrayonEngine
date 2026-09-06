-- ============================================================================
-- Example 02: 2D Sprites, Animated Spritesheet, 9-Slice UI & 2D Camera Pan/Zoom
-- Crayon Engine
-- ============================================================================

local tex_coin = 0
local tex_brick = 0
local tex_crate = 0
local tex_grass = 0

-- 2D Camera state
local cam2d = {
    x = 160,
    y = 120,
    zoom = 1.0,
    rotation = 0.0
}

-- Player character
local player = {
    x = 160,
    y = 120,
    speed = 90,
    anim_frame = 0,
    anim_timer = 0
}

local timer = 0
local show_dialog = true

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("02 - 2D Sprites, 9-Slice & 2D Camera [WASD: Move, Q/E: Rotate, Scroll: Zoom]")

    tex_coin  = crayon.graphics.load_texture("game/assets/textures/coin.bmp")
    tex_brick = crayon.graphics.load_texture("game/assets/textures/brick.bmp")
    tex_crate = crayon.graphics.load_texture("game/assets/textures/crate.bmp")
    tex_grass = crayon.graphics.load_texture("game/assets/textures/grass.bmp")
end

function crayon.update(dt)
    timer = timer + dt

    -- Player movement in world
    local dx, dy = 0, 0
    if crayon.input.is_down("w") or crayon.input.is_down("up") then dy = dy - 1 end
    if crayon.input.is_down("s") or crayon.input.is_down("down") then dy = dy + 1 end
    if crayon.input.is_down("a") or crayon.input.is_down("left") then dx = dx - 1 end
    if crayon.input.is_down("d") or crayon.input.is_down("right") then dx = dx + 1 end

    if dx ~= 0 or dy ~= 0 then
        local len = math.sqrt(dx * dx + dy * dy)
        player.x = player.x + (dx / len) * player.speed * dt
        player.y = player.y + (dy / len) * player.speed * dt
    end

    -- Camera rotation controls
    if crayon.input.is_down("q") then cam2d.rotation = cam2d.rotation - 1.5 * dt end
    if crayon.input.is_down("e") then cam2d.rotation = cam2d.rotation + 1.5 * dt end

    -- Camera zoom controls (Keys or Wheel)
    local wheel_y = crayon.input.get_mouse_wheel() or 0
    if wheel_y ~= 0 then
        cam2d.zoom = math.max(0.4, math.min(3.0, cam2d.zoom + wheel_y * 0.15))
    end
    if crayon.input.is_down("r") then cam2d.zoom = math.min(3.0, cam2d.zoom + 0.8 * dt) end
    if crayon.input.is_down("f") then cam2d.zoom = math.max(0.4, cam2d.zoom - 0.8 * dt) end

    -- Smooth Camera follow player
    cam2d.x = cam2d.x + (player.x - cam2d.x) * 5.0 * dt
    cam2d.y = cam2d.y + (player.y - cam2d.y) * 5.0 * dt

    -- Spritesheet animation (coin has 8 frames of 16x16 in 128x16 texture)
    player.anim_timer = player.anim_timer + dt
    if player.anim_timer >= 0.08 then
        player.anim_timer = 0
        player.anim_frame = (player.anim_frame + 1) % 8
    end

    -- Toggle UI
    if crayon.input.is_pressed("space") then
        show_dialog = not show_dialog
    end

    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.08, 0.12)

    -- ========================================================================
    -- 1. WORLD SPACE (Rendered with 2D Camera Transform)
    -- ========================================================================
    crayon.graphics.set_camera2d({
        x = cam2d.x,
        y = cam2d.y,
        zoom = cam2d.zoom,
        rotation = cam2d.rotation
    })

    -- A. Tiled Grass Ground (Seamless repeat across 1200x1200 world)
    crayon.graphics.set_color(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.draw_sprite_tiled(tex_grass, -400, -400, 1120, 1040, 1.0, 1.0)

    -- B. World Grid/Border Lines
    crayon.graphics.set_color(0.2, 0.4, 0.3, 0.6)
    crayon.graphics.draw_rect("line", -400, -400, 1120, 1040)

    -- C. Tiled Brick Wall / Pathway
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_sprite_tiled(tex_brick, 60, -300, 200, 48, 1.0, 1.0)
    crayon.graphics.draw_sprite_tiled(tex_brick, 140, -300, 40, 700, 1.0, 1.0)

    -- D. Crate Obstacles with drop shadow
    local crate_positions = {
        {90, 80}, {230, 80}, {90, 180}, {230, 180},
        {50, -150}, {260, -150}, {30, 300}, {290, 300}
    }
    for _, c in ipairs(crate_positions) do
        -- Drop shadow
        crayon.graphics.set_color(0.0, 0.0, 0.0, 0.3)
        crayon.graphics.draw_ellipse("fill", c[1] + 16, c[2] + 30, 18, 7)
        -- Crate
        crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
        crayon.graphics.draw_sprite(tex_crate, c[1], c[2], 32, 32)
    end

    -- E. Animated Spinning Pickups around the map using draw_sprite_part
    for i = 1, 6 do
        local angle = (i / 6) * math.pi * 2 + timer * 0.5
        local cx = 160 + math.cos(angle) * 90
        local cy = 120 + math.sin(angle) * 70
        local frame = (player.anim_frame + i) % 8
        local sx = frame * 16

        -- Shadow
        crayon.graphics.set_color(0, 0, 0, 0.35)
        crayon.graphics.draw_ellipse("fill", cx, cy + 12, 10, 4)

        -- Animated coin sprite slice (16x16 region)
        crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
        crayon.graphics.draw_sprite_part(tex_coin, sx, 0, 16, 16, cx - 12, cy - 12, 24, 24)
    end

    -- F. Player Character
    -- Shadow
    crayon.graphics.set_color(0.0, 0.0, 0.0, 0.4)
    crayon.graphics.draw_ellipse("fill", player.x, player.y + 14, 14, 6)
    -- Animated coin avatar
    crayon.graphics.set_color(1.0, 0.9, 0.4, 1.0)
    local cur_sx = player.anim_frame * 16
    crayon.graphics.draw_sprite_part(tex_coin, cur_sx, 0, 16, 16, player.x - 16, player.y - 16, 32, 32)

    -- World Origin Marker
    crayon.graphics.set_color(1.0, 0.2, 0.2, 0.8)
    crayon.graphics.draw_line(160, 110, 160, 130, 1.0)
    crayon.graphics.draw_line(150, 120, 170, 120, 1.0)

    -- Reset 2D Camera back to screen coordinates
    crayon.graphics.reset_camera2d()

    -- ========================================================================
    -- 2. SCREEN SPACE (UI, HUD & 9-Slice Dialog Box)
    -- ========================================================================

    -- Top HUD Panel
    crayon.graphics.set_color(0.08, 0.1, 0.16, 0.85)
    crayon.graphics.draw_rect("fill", 4, 4, 312, 22)
    crayon.graphics.set_color(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.draw_rect("line", 4, 4, 312, 22)

    crayon.graphics.set_color(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.draw_text("2D CAMERA & SPRITE SHOWCASE", 10, 10, 1.0)

    crayon.graphics.set_color(0.5, 1.0, 0.6, 1.0)
    local zoom_pct = math.floor(cam2d.zoom * 100)
    local rot_deg = math.floor(math.deg(cam2d.rotation) % 360)
    crayon.graphics.draw_text("Zoom: " .. zoom_pct .. "% | Rot: " .. rot_deg .. "d", 185, 10, 1.0)

    -- 9-Slice Interactive Dialog Box
    if show_dialog then
        local dw, dh = 240, 76
        local dx = (320 - dw) / 2
        local dy = 145

        -- 9-Slice rendered using brick texture with 8px borders
        crayon.graphics.set_color(0.9, 0.9, 1.0, 0.95)
        crayon.graphics.draw_sprite_9slice(tex_brick, dx, dy, dw, dh, 8, 8, 8, 8)

        -- Inner backdrop for clean text legibility
        crayon.graphics.set_color(0.05, 0.07, 0.12, 0.9)
        crayon.graphics.draw_rounded_rect("fill", dx + 8, dy + 8, dw - 16, dh - 16, 4)

        -- Dialog Title & Message
        crayon.graphics.set_color(1.0, 0.85, 0.2, 1.0)
        crayon.graphics.draw_text("9-Slice Scalable Dialog Box", dx + 14, dy + 12, 1.0)

        crayon.graphics.set_color(0.85, 0.9, 0.95, 1.0)
        crayon.graphics.draw_text("Notice how corners stay crisp while", dx + 14, dy + 28, 1.0)
        crayon.graphics.draw_text("the edges and center stretch seamlessly!", dx + 14, dy + 40, 1.0)

        crayon.graphics.set_color(0.5, 0.8, 1.0, 1.0)
        crayon.graphics.draw_text("Press [SPACE] to toggle this dialog.", dx + 14, dy + 54, 1.0)
    end

    -- Bottom Controls HUD
    crayon.graphics.set_color(0.06, 0.08, 0.12, 0.8)
    crayon.graphics.draw_rect("fill", 4, 222, 312, 16)
    crayon.graphics.set_color(0.7, 0.75, 0.85, 1.0)
    crayon.graphics.draw_text("WASD: Move | Q/E: Rotate | Scroll/R/F: Zoom", 8, 225, 1.0)
end
