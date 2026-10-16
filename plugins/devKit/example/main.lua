-- Crayon DevKit Example Demonstration
-- Press [F3] or [ ` / ~ ] anytime during gameplay to toggle the DevKit live overlay!
-- Strictly adheres to canonical Crayon Engine camelCase conventions.

local balls = {}
local spawnCount = 24
local player = { x = 320, y = 180, speed = 250, radius = 18 }

function crayon.config(t)
    t.identity = "DevKitDemo"
    t.version = "1.0.0"

    t.window = {
        title = "Crayon DevKit - Press [F3] to Toggle Developer Overlay",
        width = 640,
        height = 360,
        virtualWidth = 640,
        virtualHeight = 360,
        resizable = true,
        vsync = true
    }

    t.graphics = {
        renderer = "gl",
        clearColor = {0.06, 0.08, 0.12, 1.0},
        crt = true,
        dither = true,
        vignette = true
    }

    -- Enable DevKit plugin
    t.plugins.devKit = true
end

function crayon.init()
    crayon.print("[DevKit Demo] Initialized! Press [F3] or [`] to open DevKit.")

    -- Seed bouncing entities
    for i = 1, spawnCount do
        table.insert(balls, {
            x = math.random(40, 600),
            y = math.random(40, 320),
            vx = (math.random() * 200 - 100) * 1.5,
            vy = (math.random() * 200 - 100) * 1.5,
            r = math.random(8, 16),
            color = { math.random() * 0.7 + 0.3, math.random() * 0.7 + 0.3, math.random() * 0.7 + 0.3 }
        })
    end

    -- Register a custom game-specific debug tab into the DevKit!
    if crayon.devKit then
        crayon.devKit.addTab("AirStrike Cheats", function()
            local ui = crayon.simpleUI
            -- If simpleUI is also loaded or using basic Lua inspection
            crayon.devKit.log("[CheatTab] Running custom debug hook")
        end)
    end
end

function crayon.update(dt)
    -- Respect DevKit time dilation and pause state
    local timeScale = crayon.devKit and crayon.devKit.getTimeScale() or 1.0
    local isPaused = crayon.devKit and crayon.devKit.isPaused() or false

    if isPaused then
        return
    end

    local simDt = dt * timeScale

    -- Player movement
    if not (crayon.devKit and crayon.devKit.wantCaptureKeyboard()) then
        if crayon.key.isDown("a", "left")  then player.x = player.x - player.speed * simDt end
        if crayon.key.isDown("d", "right") then player.x = player.x + player.speed * simDt end
        if crayon.key.isDown("w", "up")    then player.y = player.y - player.speed * simDt end
        if crayon.key.isDown("s", "down")  then player.y = player.y + player.speed * simDt end
    end

    -- Clamp player
    player.x = math.max(20, math.min(620, player.x))
    player.y = math.max(20, math.min(340, player.y))

    -- Update bouncing entities
    for _, b in ipairs(balls) do
        b.x = b.x + b.vx * simDt
        b.y = b.y + b.vy * simDt

        if b.x - b.r < 0 then b.x = b.r; b.vx = -b.vx end
        if b.x + b.r > 640 then b.x = 640 - b.r; b.vx = -b.vx end
        if b.y - b.r < 0 then b.y = b.r; b.vy = -b.vy end
        if b.y + b.r > 360 then b.y = 360 - b.r; b.vy = -b.vy end
    end
end

function crayon.draw()
    crayon.graphics.clear(0.06, 0.08, 0.12)

    -- Draw bouncing balls
    for _, b in ipairs(balls) do
        crayon.graphics.setColor(b.color[1], b.color[2], b.color[3])
        crayon.graphics.drawCircle("fill", b.x, b.y, b.r)
    end

    -- Draw player
    crayon.graphics.setColor(0.2, 0.85, 1.0)
    crayon.graphics.drawCircle("fill", player.x, player.y, player.radius)
    crayon.graphics.setColor(1.0, 1.0, 1.0)
    crayon.graphics.drawCircle("line", player.x, player.y, player.radius + 2)

    -- On-screen instructions
    crayon.graphics.setColor(1.0, 1.0, 1.0, 0.85)
    crayon.graphics.drawText("PRESS [F3] OR [ ` ] TO TOGGLE DEVKIT OVERLAY", 15, 15)
    crayon.graphics.setColor(0.5, 0.8, 1.0, 0.75)
    crayon.graphics.drawText("Controls: WASD to move player | Use DevKit profiler & slow-motion slider!", 15, 35)
end
