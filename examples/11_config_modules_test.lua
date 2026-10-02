-- ============================================================================
-- Example 11: Selective Module Configuration Test
-- Demonstrates: Selective module toggling where audio is enabled but physics
--               is disabled, using strict camelCase and crayon.config(t).
-- ============================================================================

function crayon.config(t)
    t.window.title = "Crayon Selective Modules Test"
    t.window.width = 320
    t.window.height = 240
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.window.vsync = true
    t.window.scaling = "integer"

    -- Audio enabled, physics disabled
    t.modules.physics = false
    t.modules.audio = true
    t.modules.mesh3D = false

    t.graphics.clearColor = {0.18, 0.08, 0.14, 1.0} -- Retro magenta-wine
    t.fpsLimit = 60
end

function crayon.init()
    print("[Selective Test] Initialized successfully with audio enabled and physics disabled!")
end

function crayon.draw()
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("Selective Modules Active!", 20, 30, 1.2)
    crayon.graphics.setColor(0.8, 0.8, 0.8, 1)
    crayon.graphics.drawText("Audio: ENABLED | Physics: DISABLED", 20, 60, 1.0)
    crayon.graphics.drawText("Virtual Canvas: 320x240", 20, 80, 1.0)
end
