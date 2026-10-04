-- ============================================================================
-- Example 10: crayon.config() Configuration Demonstration
-- Demonstrates: Configuring window, virtual resolution, graphics defaults,
--               and disabling heavy modules (physics, audio) for optimization
--               before the engine and subsystems initialize.
-- ============================================================================

-- This function runs BEFORE the engine initializes window, OpenGL, physics, or audio!
function crayon.config(t)
    -- 1. App identity & version
    t.identity = "config_demo"
    t.version = "1.0.0"

    -- 2. Window & Virtual Resolution Settings (camelCase)
    t.window.title = "Crayon Config Demo (Optimized 2D Mode)"
    t.window.width = 400              -- Physical window width
    t.window.height = 300             -- Physical window height
    t.window.virtualWidth = 400       -- Internal virtual canvas width
    t.window.virtualHeight = 300      -- Internal virtual canvas height
    t.window.resizable = true         -- Window is resizable
    t.window.vsync = true             -- Vertical sync enabled
    t.window.scaling = "aspect"       -- Aspect ratio scaling ("integer", "aspect", "stretch", "center")

    -- 3. Module Optimization Settings (camelCase)
    -- Disable subsystems that are not needed to save memory and CPU cycles:
    t.modules.physics3D = false        -- Jolt 3D physics disabled (no RAM allocations or physics threads)
    t.modules.audio = false           -- MiniAudio disabled (no audio device opened)
    t.modules.mesh3D = false          -- 3D mesh renderer disabled
    t.modules.particles = true        -- 2D particle system remains active
    t.modules.input = true            -- Input handling active
    t.modules.fs = true               -- Filesystem active

    -- 4. Initial Graphics & Post-Processing (camelCase)
    t.graphics.clearColor = {0.07, 0.12, 0.18, 1.0} -- Dark retro navy blue
    t.graphics.crt = false

    -- 5. Performance Limit (camelCase)
    t.fpsLimit = 60                   -- Cap frame rate at 60 FPS
end

local timer = 0.0
local physics_test_status = "Not tested"
local audio_test_status = "Not tested"

function crayon.init()
    print("[Config Test] crayon.init() invoked successfully!")
    print("[Config Test] Testing disabled module safety stubs...")

    -- Test that calling a disabled module raises a clean Lua error rather than crashing
    local ok_phys, err_phys = pcall(function()
        crayon.physics.createBody()
    end)
    if not ok_phys then
        physics_test_status = "Safely intercepted: " .. tostring(err_phys):match("Module 'physics' is disabled[^\n]*")
        print("[Config Test] Physics check passed: " .. physics_test_status)
    else
        physics_test_status = "ERROR: Physics was accessible when it should be disabled"
    end

    local ok_aud, err_aud = pcall(function()
        crayon.audio.playSound("dummy.wav")
    end)
    if not ok_aud then
        audio_test_status = "Safely intercepted: " .. tostring(err_aud):match("Module 'audio' is disabled[^\n]*")
        print("[Config Test] Audio check passed: " .. audio_test_status)
    else
        audio_test_status = "ERROR: Audio was accessible when it should be disabled"
    end
end

function crayon.update(dt)
    timer = timer + dt
end

function crayon.draw()
    -- Background
    crayon.graphics.setColor(0.12, 0.18, 0.26, 1.0)
    crayon.graphics.drawRect("fill", 10, 10, 380, 280)

    crayon.graphics.setColor(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.drawRect("line", 10, 10, 380, 280)

    -- Header
    crayon.graphics.setColor(1.0, 0.9, 0.2, 1.0)
    crayon.graphics.drawText("CRAYON.CONFIG() SYSTEM ACTIVE", 20, 22, 1.4)

    -- Subtitle
    crayon.graphics.setColor(0.7, 0.85, 1.0, 1.0)
    crayon.graphics.drawText("Subsystems optimized before window launch:", 20, 48, 1.0)

    -- Status items
    local y = 72
    local function draw_status(label, val, col)
        crayon.graphics.setColor(0.8, 0.8, 0.8, 1.0)
        crayon.graphics.drawText(label, 25, y, 1.0)
        crayon.graphics.setColor(col[1], col[2], col[3], 1.0)
        crayon.graphics.drawText(val, 200, y, 1.0)
        y = y + 20
    end

    draw_status("Physics (Jolt 3D):", "DISABLED (Memory Saved)", {0.3, 1.0, 0.4})
    draw_status("Audio (MiniAudio):", "DISABLED (Device Skipped)", {0.3, 1.0, 0.4})
    draw_status("3D Mesh Renderer:", "DISABLED (Shaders Skipped)", {0.3, 1.0, 0.4})
    draw_status("Virtual Resolution:", "400 x 300", {1.0, 0.8, 0.3})
    draw_status("Physical Window:", "800 x 600", {1.0, 0.8, 0.3})
    draw_status("FPS Limit:", "60 FPS", {1.0, 0.8, 0.3})

    -- Safety check section
    y = y + 6
    crayon.graphics.setColor(0.5, 0.7, 0.9, 1.0)
    crayon.graphics.drawText("Safety Stub Interception Checks:", 25, y, 1.0)
    y = y + 18

    crayon.graphics.setColor(0.4, 0.9, 0.5, 1.0)
    crayon.graphics.drawText("Physics error: " .. string.sub(physics_test_status, 1, 40) .. "...", 30, y, 0.85)
    y = y + 16
    crayon.graphics.drawText("Audio error:   " .. string.sub(audio_test_status, 1, 40) .. "...", 30, y, 0.85)

    -- Animated indicator
    local pulse = (math.sin(timer * 4.0) + 1.0) * 0.5
    crayon.graphics.setColor(0.2 + 0.8 * pulse, 0.6, 1.0, 1.0)
    crayon.graphics.drawCircle("fill", 370, 270, 6 + 2 * pulse)
end
