-- ============================================================================
-- Example 02: 2D Sprites, Animated Spritesheet, 9-Slice UI & 2D Camera Pan/Zoom
-- Crayon Engine
-- ============================================================================

local tex_coin = 0
local tex_brick = 0
local tex_crate = 0
local tex_grass = 0

-- Coin spritesheet layout: 8 frames of 16x16 in a 128x16 texture
local COIN_SHEET_W  = 128
local COIN_SHEET_H  = 16
local COIN_FRAME_W  = 16
local COIN_FRAME_H  = 16
local COIN_FRAME_N  = 8

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
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("02 - 2D Sprites, 9-Slice & 2D Camera [WASD: Move, Q/E: Rotate, Scroll: Zoom]")

    tex_coin  = crayon.graphics.loadTexture("game/assets/textures/coin.bmp")
    tex_brick = crayon.graphics.loadTexture("game/assets/textures/brick.bmp")
    tex_crate = crayon.graphics.loadTexture("game/assets/textures/crate.bmp")
    tex_grass = crayon.graphics.loadTexture("game/assets/textures/grass.bmp")
end

function crayon.update(dt)
    timer = timer + dt

    -- Player movement in world
    local dx, dy = 0, 0
    if crayon.key.isDown("w") or crayon.key.isDown("up")    then dy = dy - 1 end
    if crayon.key.isDown("s") or crayon.key.isDown("down")  then dy = dy + 1 end
    if crayon.key.isDown("a") or crayon.key.isDown("left")  then dx = dx - 1 end
    if crayon.key.isDown("d") or crayon.key.isDown("right") then dx = dx + 1 end

    if dx ~= 0 or dy ~= 0 then
        local len = math.sqrt(dx * dx + dy * dy)
        player.x = player.x + (dx / len) * player.speed * dt
        player.y = player.y + (dy / len) * player.speed * dt
    end

    -- Camera rotation controls
    if crayon.key.isDown("q") then cam2d.rotation = cam2d.rotation - 1.5 * dt end
    if crayon.key.isDown("e") then cam2d.rotation = cam2d.rotation + 1.5 * dt end

    -- Camera zoom controls (Keys or Wheel)
    local wheel_x, wheel_y = crayon.input.getMouseWheel()
    if wheel_y and wheel_y ~= 0 then
        cam2d.zoom = math.max(0.4, math.min(3.0, cam2d.zoom + wheel_y * 0.15))
    end
    if crayon.key.isDown("r") then cam2d.zoom = math.min(3.0, cam2d.zoom + 0.8 * dt) end
    if crayon.key.isDown("f") then cam2d.zoom = math.max(0.4, cam2d.zoom - 0.8 * dt) end

    -- Framerate-independent smooth camera follow
    cam2d.x = crayon.math.damp(cam2d.x, player.x, 8.0, dt)
    cam2d.y = crayon.math.damp(cam2d.y, player.y, 8.0, dt)

    -- Spritesheet animation
    player.anim_timer = player.anim_timer + dt
    if player.anim_timer >= 0.08 then
        player.anim_timer = player.anim_timer - 0.08
        player.anim_frame = (player.anim_frame + 1) % COIN_FRAME_N
    end

    -- Toggle UI
    if crayon.key.isPressed("space") then
        show_dialog = not show_dialog
    end

    if crayon.key.isPressed("escape") then
        crayon.window.quit()
    end
end

-- Helper: draw a frame from a spritesheet using pixel coordinates.
-- Uses drawSpritePart with normalized UVs derived from the sheet size.
local function draw_sheet_frame(tex, sheet_w, sheet_h, frame_x, frame_y, frame_w, frame_h,
                                dx, dy, dw, dh, rot, ox, oy)
    local u0 = frame_x / sheet_w
    local v0 = frame_y / sheet_h
    local u1 = (frame_x + frame_w) / sheet_w
    local v1 = (frame_y + frame_h) / sheet_h
    crayon.graphics.drawSpritePart(tex, dx, dy, u0, v0, u1, v1, dw, dh, rot or 0.0, ox or 0.0, oy or 0.0)
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.08, 0.12)

    -- ========================================================================
    -- 1. WORLD SPACE (Rendered with 2D Camera Transform)
    -- ========================================================================
    crayon.graphics.setCamera2d({
        x = cam2d.x,
        y = cam2d.y,
        zoom = cam2d.zoom,
        angle = cam2d.rotation
    })

    -- A. Tiled Grass Ground (16x16 tiles across a 1120x1040 world)
    crayon.graphics.setColor(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.drawSpriteTiled(tex_grass, -400, -400, 1120, 1040, 16, 16)

    -- B. World Grid/Border Lines
    crayon.graphics.setColor(0.2, 0.4, 0.3, 0.6)
    crayon.graphics.drawRect("line", -400, -400, 1120, 1040)

    -- C. Tiled Brick Wall / Pathway (16x16 tiles)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawSpriteTiled(tex_brick,  60, -300, 200, 48, 16, 16)
    crayon.graphics.drawSpriteTiled(tex_brick, 140, -300,  40, 700, 16, 16)

    -- D. Crate Obstacles with drop shadow
    local crate_positions = {
        {90, 80}, {230, 80}, {90, 180}, {230, 180},
        {50, -150}, {260, -150}, {30, 300}, {290, 300}
    }
    for _, c in ipairs(crate_positions) do
        -- Drop shadow
        crayon.graphics.setColor(0.0, 0.0, 0.0, 0.3)
        crayon.graphics.drawEllipse("fill", c[1] + 16, c[2] + 30, 18, 7)
        -- Crate
        crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
        crayon.graphics.drawSprite(tex_crate, c[1], c[2], 32, 32)
    end

    -- E. Animated Spinning Pickups around the map
    for i = 1, 6 do
        local angle = (i / 6) * math.pi * 2 + timer * 0.5
        local cx = 160 + math.cos(angle) * 90
        local cy = 120 + math.sin(angle) * 70
        local frame = (player.anim_frame + i) % COIN_FRAME_N

        -- Shadow
        crayon.graphics.setColor(0, 0, 0, 0.35)
        crayon.graphics.drawEllipse("fill", cx, cy + 12, 10, 4)

        -- Animated coin frame (24x24 on screen)
        crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
        draw_sheet_frame(tex_coin, COIN_SHEET_W, COIN_SHEET_H,
                         frame * COIN_FRAME_W, 0, COIN_FRAME_W, COIN_FRAME_H,
                         cx - 12, cy - 12, 24, 24)
    end

    -- F. Player Character
    -- Shadow
    crayon.graphics.setColor(0.0, 0.0, 0.0, 0.4)
    crayon.graphics.drawEllipse("fill", player.x, player.y + 14, 14, 6)

    -- Animated coin avatar (32x32 on screen)
    crayon.graphics.setColor(1.0, 0.9, 0.4, 1.0)
    draw_sheet_frame(tex_coin, COIN_SHEET_W, COIN_SHEET_H,
                     player.anim_frame * COIN_FRAME_W, 0, COIN_FRAME_W, COIN_FRAME_H,
                     player.x - 16, player.y - 16, 32, 32)

    -- World Origin Marker
    crayon.graphics.setColor(1.0, 0.2, 0.2, 0.8)
    crayon.graphics.drawLine(160, 110, 160, 130, 1.0)
    crayon.graphics.drawLine(150, 120, 170, 120, 1.0)

    -- Reset 2D Camera back to screen coordinates
    crayon.graphics.resetCamera2d()

    -- ========================================================================
    -- 2. SCREEN SPACE (UI, HUD & 9-Slice Dialog Box)
    -- ========================================================================

    -- Top HUD Panel
    crayon.graphics.setColor(0.08, 0.1, 0.16, 0.85)
    crayon.graphics.drawRect("fill", 4, 4, 312, 22)
    crayon.graphics.setColor(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.drawRect("line", 4, 4, 312, 22)

    crayon.graphics.setColor(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.drawText("2D CAMERA & SPRITE SHOWCASE", 10, 10, 1.0)

    crayon.graphics.setColor(0.5, 1.0, 0.6, 1.0)
    local zoom_pct = math.floor(cam2d.zoom * 100)
    local rot_deg  = math.floor(math.deg(cam2d.rotation) % 360)
    crayon.graphics.drawText("Zoom: " .. zoom_pct .. "% | Rot: " .. rot_deg .. "d", 185, 10, 1.0)

    -- 9-Slice Interactive Dialog Box
    if show_dialog then
        local dw, dh = 240, 76
        local dx = (320 - dw) / 2
        local dy = 145

        -- 9-Slice rendered using brick texture with 8px borders
        crayon.graphics.setColor(0.9, 0.9, 1.0, 0.95)
        crayon.graphics.drawSprite9slice(tex_brick, dx, dy, dw, dh, 8, 8, 8, 8)

        -- Inner backdrop for clean text legibility
        crayon.graphics.setColor(0.05, 0.07, 0.12, 0.9)
        crayon.graphics.drawRoundedRect("fill", dx + 8, dy + 8, dw - 16, dh - 16, 4)

        -- Dialog Title & Message
        crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
        crayon.graphics.drawText("9-Slice Scalable Dialog Box", dx + 14, dy + 12, 1.0)

        crayon.graphics.setColor(0.85, 0.9, 0.95, 1.0)
        crayon.graphics.drawText("Notice how corners stay crisp while", dx + 14, dy + 28, 1.0)
        crayon.graphics.drawText("the edges and center stretch seamlessly!", dx + 14, dy + 40, 1.0)

        crayon.graphics.setColor(0.5, 0.8, 1.0, 1.0)
        crayon.graphics.drawText("Press [SPACE] to toggle this dialog.", dx + 14, dy + 54, 1.0)
    end

    -- Bottom Controls HUD
    crayon.graphics.setColor(0.06, 0.08, 0.12, 0.8)
    crayon.graphics.drawRect("fill", 4, 222, 312, 16)
    crayon.graphics.setColor(0.7, 0.75, 0.85, 1.0)
    crayon.graphics.drawText("WASD: Move | Q/E: Rotate | Scroll/R/F: Zoom", 8, 225, 1.0)
end