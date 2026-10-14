-- ============================================================================
-- Example 24: Selecting the renderer from crayon.config()
--
-- Priority (highest first):  --renderer=...  >  CRAYON_RENDERER env var  >  this config key.
--   "auto" : GPU when a real OpenGL context exists, otherwise the CPU rasterizer   (default)
--   "gl"   : force OpenGL (fails to start if no context)
--   "soft" : force the CPU rasterizer, even on a machine with a GPU
--
-- This file forces the CPU rasterizer so you can verify it on any machine, with no flags.
-- Change `renderer` below (or pass --renderer=gl on the command line) to compare.
-- ============================================================================
function crayon.config(t)
    t.window.title = "24 - renderer = soft (from config)"
    t.window.width = 320
    t.window.height = 240
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.graphics.renderer = "soft"        -- "auto" | "gl" | "soft"
    t.graphics.clearColor = { 0.06, 0.08, 0.16, 1 }
    t.modules.physics3D = false
    t.modules.audio = false
    t.modules.mesh3D = true
end

local G = crayon.graphics

function crayon.init()
    G = crayon.graphics
end

function crayon.update(dt)
    local t = crayon.time.getTime()
    G.setCamera3D({ position = { math.sin(t * 0.6) * 6, 3, math.cos(t * 0.6) * 6 }, target = { 0, 0.5, 0 }, up = { 0, 1, 0 }, fov = 60 })
end

function crayon.draw()
    local t = crayon.time.getTime()
    G.clear(0.06, 0.08, 0.16, 1)
    G.setDepthTest(true); G.setDepthWrite(true)
    G.setColor(0.5, 0.55, 0.7, 1); G.drawPlane(0, 0, 0, 20, 20)
    G.setColor(0.95, 0.4, 0.35, 1); G.drawCube(0, 0.6, 0, 1.2, 1.2, 1.2, nil, 0, t, 0)
    G.setColor(0.4, 0.9, 0.6, 1);   G.drawSphere(2.5, 0.6, 0, 0.6)
    G.setDepthTest(false); G.setDepthWrite(false)

    local soft = G.isSoftwareRenderer()
    G.setColor(1, 1, 1, 1)
    G.drawText("renderer: " .. G.getRendererName(), 6, 6)
    if soft then G.setColor(0.4, 1, 0.5, 1) else G.setColor(1, 0.7, 0.3, 1) end
    G.drawText(soft and "CPU rasterizer active (no GPU needed)" or "OpenGL active", 6, 18)
    G.setColor(0.7, 0.8, 0.9, 1)
    G.drawText(string.format("%d fps", math.floor(crayon.window.getFps())), 6, 30)
end

function crayon.keydown(k) if k == "escape" then crayon.window.quit() end end
