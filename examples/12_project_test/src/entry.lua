-- ============================================================================
-- Project Test Entry Script
-- Launched via .crayonproj manifest!
-- Demonstrates: .crayonproj resolution, custom directory context, and
--               camelCase crayon.config(t).
-- ============================================================================

function crayon.config(t)
    t.window.title = "Crayon Project Manifest Test (.crayonproj)"
    t.window.width = 720
    t.window.height = 540
    t.window.virtualWidth = 360
    t.window.virtualHeight = 270
    t.window.scaling = "aspect"
    t.window.resizable = true

    t.modules.physics = false
    t.modules.audio = false
    t.modules.mesh3D = false

    t.graphics.clearColor = {0.14, 0.08, 0.22, 1.0}
    t.fpsLimit = 60
end

local timer = 0.0

function crayon.init()
    print("[Project Test] Initialized successfully via .crayonproj manifest!")
end

function crayon.update(dt)
    timer = timer + dt
end

function crayon.draw()
    crayon.graphics.setColor(0.2, 0.12, 0.32, 1.0)
    crayon.graphics.drawRect("fill", 15, 15, 330, 240)

    crayon.graphics.setColor(0.6, 0.3, 0.9, 1.0)
    crayon.graphics.drawRect("line", 15, 15, 330, 240)

    crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.drawText(".CRAYONPROJ PROJECT LOADED", 30, 30, 1.2)

    crayon.graphics.setColor(0.8, 0.9, 1.0, 1.0)
    crayon.graphics.drawText("Project entry: src/entry.lua", 30, 60, 0.9)
    crayon.graphics.drawText("Virtual Canvas: 360x270", 30, 80, 0.9)
    crayon.graphics.drawText("Window: 720x540 (Aspect)", 30, 100, 0.9)
    crayon.graphics.drawText("Physics & Audio: Disabled", 30, 120, 0.9)

    local offset = math.sin(timer * 3.0) * 10
    crayon.graphics.setColor(0.4, 0.8, 1.0, 1.0)
    crayon.graphics.drawCircle("fill", 180 + offset, 190, 15)
end
