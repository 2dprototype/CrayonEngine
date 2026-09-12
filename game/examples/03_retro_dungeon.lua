-- ============================================================================
-- Example 03: Retro First-Person Dungeon Crawler & Mini-Map
-- Run with: ./crayon_engine --game game/examples/03_retro_dungeon.lua
-- ============================================================================

local MAP_W = 10
local MAP_H = 10
local map = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 1, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 0, 0, 0, 0, 1, 0, 1,
    1, 0, 1, 1, 1, 1, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 1, 0, 0, 0, 1,
    1, 1, 1, 0, 0, 1, 1, 1, 0, 1,
    1, 0, 0, 0, 1, 1, 0, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 0, 1, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1
}

local coins = {
    {x = 1.5, z = 3.5, collected = false},
    {x = 3.5, z = 1.5, collected = false},
    {x = 6.5, z = 1.5, collected = false},
    {x = 8.5, z = 5.5, collected = false},
    {x = 3.5, z = 5.5, collected = false},
    {x = 1.5, z = 8.5, collected = false},
    {x = 6.5, z = 8.5, collected = false}
}

local player = {
    x = 1.5,
    z = 1.5,
    angle = 0.0,
    score = 0
}

local cube_model = 0
local plane_model = 0
local tex_brick = 0
local tex_crate = 0
local tex_coin = 0
local timer = 0

function is_wall(x, z)
    local gx = math.floor(x)
    local gz = math.floor(z)
    if gx < 0 or gx >= MAP_W or gz < 0 or gz >= MAP_H then return true end
    return map[gz * MAP_W + gx + 1] == 1
end

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("Crayon Engine - Retro 3D Dungeon Crawler")

    cube_model  = crayon.graphics.loadModel("cube")
    plane_model = crayon.graphics.loadModel("plane")

    tex_brick = crayon.graphics.loadTexture("game/assets/textures/brick.bmp")
    tex_crate = crayon.graphics.loadTexture("game/assets/textures/crate.bmp")
    tex_coin  = crayon.graphics.loadTexture("game/assets/textures/coin.bmp")

    crayon.graphics.setRetroEffects({
        jitterResolution = {160, 120},
        affine = 1.0,
        dither = true,
        fog = { startDist = 3, endDist = 9, color = {0.04, 0.04, 0.08} }
    })

    crayon.graphics.setLight(0.3, -1.0, 0.5, 1.0, 0.9, 0.7, 0.35, 0.3, 0.4)
end

function crayon.update(dt)
    timer = timer + dt

    -- Turn Left / Right
    local turn_spd = 110.0 * dt
    if crayon.input.isDown("left") or crayon.input.isDown("a") then
        player.angle = player.angle - turn_spd
    end
    if crayon.input.isDown("right") or crayon.input.isDown("d") then
        player.angle = player.angle + turn_spd
    end

    -- Move Forward / Backward
    local move_spd = 2.8 * dt
    local rad = math.rad(player.angle)
    local fwd_x = math.cos(rad)
    local fwd_z = math.sin(rad)

    local move_x, move_z = 0, 0
    if crayon.input.isDown("up") or crayon.input.isDown("w") then
        move_x = move_x + fwd_x * move_spd
        move_z = move_z + fwd_z * move_spd
    end
    if crayon.input.isDown("down") or crayon.input.isDown("s") then
        move_x = move_x - fwd_x * move_spd
        move_z = move_z - fwd_z * move_spd
    end

    -- Simple collision detection
    local new_x = player.x + move_x
    local new_z = player.z + move_z
    if not is_wall(new_x, player.z) then player.x = new_x end
    if not is_wall(player.x, new_z) then player.z = new_z end

    -- Check Coin Collection
    for _, c in ipairs(coins) do
        if not c.collected then
            local dist = math.sqrt((player.x - c.x)^2 + (player.z - c.z)^2)
            if dist < 0.6 then
                c.collected = true
                player.score = player.score + 100
            end
        end
    end

    if crayon.input.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.04, 0.04, 0.08)

    -- 1. Setup Camera at Player Eye Level (Y = 0.5)
    local rad = math.rad(player.angle)
    local tx = player.x + math.cos(rad)
    local tz = player.z + math.sin(rad)

    crayon.graphics.setCamera3d({
        position = {player.x, 0.5, player.z},
        target = {tx, 0.5, tz},
        up = {0, 1, 0},
        fov = 65.0,
        near = 0.1,
        far = 20.0
    })

    -- 2. Draw Floor & Ceiling Planes
    -- drawPlane(x, y, z, w, d, tex, rx, ry, rz)
    crayon.graphics.drawPlane(5, 0, 5, 20, 20, tex_crate, 0, 0, 0)
    crayon.graphics.drawPlane(5, 1.0, 5, 20, 20, tex_crate, math.pi * 0.5, 0, 0)

    -- 3. Draw Dungeon Walls
    -- drawCube(x, y, z, sx, sy, sz, tex, rx, ry, rz)
    for gz = 0, MAP_H - 1 do
        for gx = 0, MAP_W - 1 do
            if map[gz * MAP_W + gx + 1] == 1 then
                crayon.graphics.drawCube(gx + 0.5, 0.5, gz + 0.5, 1.0, 1.0, 1.0, tex_brick, 0, 0, 0)
            end
        end
    end

    -- 4. Draw Collectible Coins (floating and rotating)
    for _, c in ipairs(coins) do
        if not c.collected then
            local bob = 0.4 + math.sin(timer * 4.0) * 0.08
            local coin_rot = timer * 3.0
            crayon.graphics.drawCube(c.x, bob, c.z, 0.25, 0.25, 0.05, tex_coin, 0, coin_rot, 0)
        end
    end

    -- 5. 2D HUD & Mini-Map
    -- Top Score Bar
    crayon.graphics.setColor(0.05, 0.05, 0.1, 0.8)
    crayon.graphics.drawRect("fill", 5, 5, 310, 20)
    crayon.graphics.setColor(0.4, 0.5, 0.8, 1.0)
    crayon.graphics.drawRect("line", 5, 5, 310, 20)

    crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.drawText("GOLD: " .. player.score, 12, 11, 1.0)

    crayon.graphics.setColor(0.8, 0.8, 0.8, 1.0)
    crayon.graphics.drawText("FPS: " .. math.floor(crayon.window.getFps() + 0.5), 260, 11, 1.0)

    -- Mini-map (Bottom Left)
    local mm_size = 5
    local mm_x = 10
    local mm_y = 175

    crayon.graphics.setColor(0, 0, 0, 0.7)
    crayon.graphics.drawRect("fill", mm_x - 2, mm_y - 2, MAP_W * mm_size + 4, MAP_H * mm_size + 4)

    for gz = 0, MAP_H - 1 do
        for gx = 0, MAP_W - 1 do
            local cell = map[gz * MAP_W + gx + 1]
            if cell == 1 then
                crayon.graphics.setColor(0.4, 0.4, 0.5, 0.9)
                crayon.graphics.drawRect("fill", mm_x + gx * mm_size, mm_y + gz * mm_size, mm_size, mm_size)
            end
        end
    end

    -- Coins on mini-map
    for _, c in ipairs(coins) do
        if not c.collected then
            crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
            crayon.graphics.drawRect("fill", mm_x + c.x * mm_size - 1, mm_y + c.z * mm_size - 1, 2, 2)
        end
    end

    -- Player dot & view line on mini-map
    crayon.graphics.setColor(1.0, 0.2, 0.2, 1.0)
    local px = mm_x + player.x * mm_size
    local pz = mm_y + player.z * mm_size
    crayon.graphics.drawCircle("fill", px, pz, 2)
    crayon.graphics.drawLine(px, pz, px + math.cos(rad) * 6, pz + math.sin(rad) * 6, 1.0)

    -- Bottom controls hint
    crayon.graphics.setColor(0.7, 0.8, 0.9, 1.0)
    crayon.graphics.drawText("Arrows/WASD: Move & Turn", 70, 220, 1.0)
end