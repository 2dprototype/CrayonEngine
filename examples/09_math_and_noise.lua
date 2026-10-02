-- ============================================================================
-- Example 09: Math Library, Vectors, Damping & Procedural Noise
-- Demonstrates: crayon.math.lerp, smoothstep, damp, remap, noise,
--               and vector functions (length, normalize, dot, distance).
-- ============================================================================

function crayon.config(config)
    config.window.title = "09 - Math Library & Procedural Noise"
    config.window.width = 640
    config.window.height = 480
    config.window.virtualWidth = 640
    config.window.virtualHeight = 480
    config.modules.physics = false
    config.modules.mesh3D = false
end

local followPos = { x = 320, y = 240 }
local targetPos = { x = 320, y = 240 }
local gridCols, gridRows = 32, 20
local cellW, cellH = 10, 10
local animOffset = 0.0

function crayon.init()
    -- Nothing window-related belongs here.
end

function crayon.update(dt)
    animOffset = animOffset + dt * 0.5

    -- Target follows mouse
    local mx, my = crayon.input.getMousePosition()
    targetPos.x = mx
    targetPos.y = my

    -- Smoothly damp followPos towards targetPos
    followPos.x = crayon.math.damp(followPos.x, targetPos.x, 8.0, dt)
    followPos.y = crayon.math.damp(followPos.y, targetPos.y, 8.0, dt)
end

function crayon.draw()
    local t = crayon.time.getTime()

    crayon.graphics.clear(0.06, 0.07, 0.11, 1.0)

    -- Header
    crayon.graphics.setColor(0.4, 0.8, 1.0, 1.0)
    crayon.graphics.drawText("Math & Procedural Noise Toolkit", 24, 20, { scale = 1.8 })

    -- 1. 2D Procedural Noise Grid
    local gridStartX, gridStartY = 24, 65
    crayon.graphics.setColor(0.9, 0.8, 0.2, 1.0)
    crayon.graphics.drawText("2D Noise Surface (crayon.math.noise):", gridStartX, gridStartY - 5, { scale = 1.0 })

    for cy = 0, gridRows - 1 do
        for cx = 0, gridCols - 1 do
            local nx = cx * 0.1 + animOffset
            local ny = cy * 0.1 + animOffset * 0.5

            -- crayon.math.noise() returns approximately [-1, 1].
            -- Remap to [0, 1] so the color ramp below stays in range.
            local nRaw = crayon.math.noise(nx, ny)
            local nVal = (nRaw + 1.0) * 0.5

            local r = crayon.math.lerp(0.05, 0.2, nVal)
            local g = crayon.math.lerp(0.15, 0.65, nVal)
            local b = crayon.math.lerp(0.3, 0.9, nVal)

            crayon.graphics.setColor(r, g, b, 1.0)
            crayon.graphics.drawRect("fill",
                gridStartX + cx * cellW,
                gridStartY + cy * cellH + 15,
                cellW - 1, cellH - 1)
        end
    end

    -- 2. Right card: Vector Math
    local cardX, cardY = 380, 75
    crayon.graphics.setColor(0.13, 0.16, 0.23, 1.0)
    crayon.graphics.drawRoundedRect("fill", cardX, cardY, 236, 190, 8)
    crayon.graphics.setColor(0.25, 0.35, 0.55, 1.0)
    crayon.graphics.drawRoundedRect("line", cardX, cardY, 236, 190, 8)

    local dist = crayon.math.vec2Distance(followPos.x, followPos.y, targetPos.x, targetPos.y)
    local nx, ny = crayon.math.vec2Normalize(targetPos.x - followPos.x, targetPos.y - followPos.y)
    local dotVal = crayon.math.vec2Dot(nx, ny, 1.0, 0.0)

    crayon.graphics.setColor(1.0, 0.75, 0.3, 1.0)
    crayon.graphics.drawText("Live Vector Calculations", cardX + 16, cardY + 14, { scale = 1.1 })

    crayon.graphics.setColor(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.drawText(string.format("Distance:     %.1f px", dist), cardX + 16, cardY + 45)
    crayon.graphics.drawText(string.format("Dir Normal:   (%.2f, %.2f)", nx, ny), cardX + 16, cardY + 70)
    crayon.graphics.drawText(string.format("Dot (X-axis): %.2f", dotVal), cardX + 16, cardY + 95)

    local tNorm = (math.sin(t * 2.0) + 1.0) * 0.5
    local smoothVal = crayon.math.smoothstep(0.0, 1.0, tNorm)
    crayon.graphics.drawText(string.format("Linear T:     %.2f", tNorm), cardX + 16, cardY + 125)
    crayon.graphics.drawText(string.format("Smoothstep:   %.2f", smoothVal), cardX + 16, cardY + 148)

    -- 3. Interactive damped follower
    crayon.graphics.setColor(0.1, 0.12, 0.18, 0.8)
    crayon.graphics.drawRoundedRect("fill", 24, 290, 592, 160, 8)
    crayon.graphics.setColor(0.25, 0.35, 0.5, 0.8)
    crayon.graphics.drawRoundedRect("line", 24, 290, 592, 160, 8)

    crayon.graphics.setColor(0.6, 0.65, 0.75, 1.0)
    crayon.graphics.drawText("Move mouse anywhere - spring damping follower (crayon.math.damp):", 36, 302)

    -- Target crosshair
    crayon.graphics.setColor(1.0, 0.3, 0.3, 0.8)
    crayon.graphics.drawCircle("line", targetPos.x, targetPos.y, 8, 16)
    crayon.graphics.drawLine(targetPos.x - 12, targetPos.y, targetPos.x + 12, targetPos.y, 1.0)
    crayon.graphics.drawLine(targetPos.x, targetPos.y - 12, targetPos.x, targetPos.y + 12, 1.0)

    -- Damped follower
    crayon.graphics.setColor(0.2, 0.8, 1.0, 0.9)
    crayon.graphics.drawCircle("fill", followPos.x, followPos.y, 14, 24)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawCircle("line", followPos.x, followPos.y, 14, 24)

    -- Elastic connector line
    crayon.graphics.setColor(0.3, 0.7, 1.0, 0.5)
    crayon.graphics.drawLine(followPos.x, followPos.y, targetPos.x, targetPos.y, 2.0)
end