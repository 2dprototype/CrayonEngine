-- ============================================================================
-- Example 03: Particle Systems
-- Demonstrates: crayon.particles.createEmitter, continuous emission, bursts,
--               custom particle colors, gravity, lifetime, and blend modes.
-- ============================================================================

local fireEmitter = nil
local sparkEmitter = nil
local mousePos = { x = 320, y = 240 }

function crayon.init()
    crayon.window.setTitle("03 - Particle Systems")
    crayon.window.setResolution(640, 480)

    -- 1. Continuous Fire/Smoke Emitter
    fireEmitter = crayon.particles.createEmitter({
        maxParticles = 500,
        emissionRate = 90,
        lifetimeMin = 0.6,
        lifetimeMax = 1.4,
        sizeStart = 14.0,
        sizeEnd = 2.0,
        gravity = -30.0, -- Upward convection
        blendMode = "additive"
    })
    fireEmitter:setPosition(320, 360)

    -- 2. Interactive Spark Explosion Emitter
    sparkEmitter = crayon.particles.createEmitter({
        maxParticles = 300,
        emissionRate = 0, -- Burst-only emitter
        lifetimeMin = 0.4,
        lifetimeMax = 0.9,
        sizeStart = 6.0,
        sizeEnd = 1.0,
        gravity = 120.0, -- Downward gravity
        blendMode = "additive"
    })
    sparkEmitter:setPosition(320, 240)
end

function crayon.mousemoved(x, y)
    mousePos.x = x
    mousePos.y = y
end

function crayon.mousedown(x, y, button)
    -- Trigger explosive burst of sparks at cursor position
    sparkEmitter:setPosition(x, y)
    sparkEmitter:burst(40)
end

function crayon.update(dt)
    -- Follow mouse or move in an arc if mouse is idle
    fireEmitter:update(dt)
    sparkEmitter:update(dt)

    -- Spacebar bursts sparks as well
    if crayon.input.isKeyPressed("space") then
        sparkEmitter:setPosition(mousePos.x, mousePos.y)
        sparkEmitter:burst(50)
    end
end

function crayon.draw()
    crayon.graphics.clear(0.04, 0.04, 0.07, 1.0)

    -- Draw subtle ground line for campfire
    crayon.graphics.setColor(0.2, 0.25, 0.35, 1.0)
    crayon.graphics.drawLine(180, 370, 460, 370, 2.0)

    -- Draw embers and fire particles
    fireEmitter:draw()
    sparkEmitter:draw()

    -- Instructions
    crayon.graphics.setColor(1.0, 1.0, 1.0, 0.9)
    crayon.graphics.drawText("Particle Emitter Demo", 20, 20, { scale = 1.5 })
    crayon.graphics.setColor(0.7, 0.75, 0.85, 1.0)
    crayon.graphics.drawText("Click mouse or press SPACE to trigger a spark burst", 20, 50, { scale = 1.0 })
    crayon.graphics.drawText("Active Fire Particles: " .. fireEmitter:getAliveCount(), 20, 75, { scale = 1.0 })
    crayon.graphics.drawText("Active Spark Particles: " .. sparkEmitter:getAliveCount(), 20, 95, { scale = 1.0 })
    crayon.graphics.drawText("FPS: " .. math.floor(crayon.window.getFps()), 560, 20, { scale = 1.0 })
end
