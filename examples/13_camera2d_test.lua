-- Crayon Engine: Camera2D Object Test
-- Demonstrates target follow, deadzone, lookahead, lerp smoothing, bounds clamping, shake, and moveTo room transitions

function crayon.config(config)
    config.window.title = "Crayon Engine - Camera2D Test"
    config.window.width = 320
    config.window.height = 240
    config.window.virtualWidth = 320
    config.window.virtualHeight = 240
    config.modules.physics3d = false
    config.modules.mesh3D = false
end

local cam = nil
local player = {
    x = 160,
    y = 120,
    w = 12,
    h = 12,
    speed = 120
}

local worldWidth = 800
local worldHeight = 600

function crayon.init()
    cam = crayon.graphics.newCamera2D(player.x, player.y, 1.0)
    cam:setTarget(player.x, player.y)
    cam:setBounds(0, 0, worldWidth, worldHeight)
    cam:setDeadzone(40, 30)
    cam:setFollowLerp(6.0)
    cam:setOffset(160, 120) -- Center on screen (virtual resolution 320x240)
end

function crayon.update(dt)
    -- Movement controls
    local moveX = 0
    local moveY = 0
    if crayon.input.isKeyDown("d") or crayon.input.isKeyDown("right") then moveX = moveX + 1 end
    if crayon.input.isKeyDown("a") or crayon.input.isKeyDown("left")  then moveX = moveX - 1 end
    if crayon.input.isKeyDown("s") or crayon.input.isKeyDown("down")  then moveY = moveY + 1 end
    if crayon.input.isKeyDown("w") or crayon.input.isKeyDown("up")    then moveY = moveY - 1 end

    player.x = player.x + moveX * player.speed * dt
    player.y = player.y + moveY * player.speed * dt

    -- Clamp player to world
    if player.x < 10 then player.x = 10 end
    if player.x > worldWidth - 10 then player.x = worldWidth - 10 end
    if player.y < 10 then player.y = 10 end
    if player.y > worldHeight - 10 then player.y = worldHeight - 10 end

    -- Update camera lookahead based on moving direction
    cam:setLookahead(moveX * 30, moveY * 20)
    cam:setTarget(player.x, player.y)

    -- Screen shake trigger
    if crayon.input.isKeyPressed("space") then
        cam:shake(10.0, 0.4, 35.0)
    end

    -- Room transition test (Move to room 2 or back)
    if crayon.input.isKeyPressed("m") then
        if player.x < 400 then
            cam:moveTo(600, 450, 1.2, "easeInOut")
        else
            cam:moveTo(160, 120, 1.2, "easeInOut")
        end
    end

    -- Zoom controls
    if crayon.input.isKeyDown("q") then
        cam:setZoom(cam:getZoom() - 0.5 * dt)
    elseif crayon.input.isKeyDown("e") then
        cam:setZoom(cam:getZoom() + 0.5 * dt)
    end

    cam:update(dt)
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.12, 1.0)

    -- Apply camera transform to 2D batch
    cam:apply()

    -- 1. Draw World Grid
    crayon.graphics.setColor(0.15, 0.17, 0.22, 1.0)
    for x = 0, worldWidth, 40 do
        crayon.graphics.drawLine(x, 0, x, worldHeight)
    end
    for y = 0, worldHeight, 40 do
        crayon.graphics.drawLine(0, y, worldWidth, y)
    end

    -- 2. Draw World Boundaries (outline)
    crayon.graphics.setColor(0.8, 0.2, 0.2, 0.7)
    crayon.graphics.drawRect("line", 0, 0, worldWidth, worldHeight)

    -- 3. Draw Room partitions
    crayon.graphics.setColor(0.3, 0.5, 0.8, 0.5)
    crayon.graphics.drawRect("fill", 400, 0, 2, worldHeight)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("ROOM 1", 50, 50, 1.5)
    crayon.graphics.drawText("ROOM 2", 450, 50, 1.5)

    -- 4. Draw Player (filled)
    crayon.graphics.setColor(0.2, 0.85, 0.4, 1.0)
    crayon.graphics.drawRect("fill", player.x - player.w * 0.5, player.y - player.h * 0.5, player.w, player.h)

    -- Reset camera transform for UI overlay
    crayon.graphics.resetCamera2d()

    -- HUD Overlay
    local cx, cy = cam:getPosition()
    crayon.graphics.setColor(0.0, 0.0, 0.0, 0.6)
    crayon.graphics.drawRect("fill", 4, 4, 312, 54)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText(
        "Camera2D: (" .. string.format("%.1f", cx) .. ", " .. string.format("%.1f", cy) ..
        ") Zoom: " .. string.format("%.2f", cam:getZoom()),
        8, 8, 1.0
    )
    crayon.graphics.setColor(0.8, 0.8, 0.8, 0.9)
    crayon.graphics.drawText("WASD/Arrows: Move | SPACE: Shake", 8, 22, 1.0)
    crayon.graphics.drawText("M: MoveTo Transition | Q/E: Zoom", 8, 36, 1.0)
end