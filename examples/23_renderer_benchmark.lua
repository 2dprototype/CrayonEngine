-- ============================================================================
-- Example 23: Renderer benchmark (prints a table, then quits)
--
--   crayon --game examples/23_renderer_benchmark.lua --renderer=soft
--   crayon --game examples/23_renderer_benchmark.lua --renderer=gl
--
-- Runs four 2-second phases and prints the average frame time of each:
--   1. empty frame            (clear + present cost)
--   2. 3D: many cubes + monkey models
--   3. 2D: many alpha-blended sprites / rects
--   4. mixed: 3D + 2D together
-- Note: vsync caps GL results at the display refresh rate; set t.window.vsync=false below to
-- measure raw throughput. Run with both backends and compare.
-- ============================================================================
local G = crayon.graphics

function crayon.config(t)
    t.window.title = "23 - Renderer benchmark"
    t.window.width = 640
    t.window.height = 480
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.window.vsync = false
    t.modules.physics3D = false
    t.modules.audio = false
    t.modules.mesh3D = true
end

local PHASE_SECONDS = 2.0
local phases = { "empty", "3D scene", "2D sprites", "3D + 2D" }
local S = { phase = 1, time = 0, frames = 0, results = {}, assets = {} }

function crayon.init()
    G = crayon.graphics
    S.assets.tex = G.loadTexture("assets/models/cube_gltf/Cube_BaseColor.png")
    S.assets.monkey = G.loadModel("assets/models/monkey.obj")
    print(string.format("== benchmark | renderer: %s (software=%s) | virtual 320x240",
        G.getRendererName(), tostring(G.isSoftwareRenderer())))
end

local function scene3D(t)
    G.setDepthTest(true); G.setDepthWrite(true)
    G.setCamera3D({ position = { math.sin(t * 0.4) * 14, 6, math.cos(t * 0.4) * 14 }, target = { 0, 1, 0 }, up = { 0, 1, 0 }, fov = 55 })
    G.setLight(-0.5, -1, -0.7, 1, 0.95, 0.85, 0.25, 0.25, 0.35)
    G.setColor(0.5, 0.55, 0.65, 1); G.drawPlane(0, 0, 0, 60, 60, S.assets.tex)
    for i = 0, 63 do
        local x, z = (i % 8 - 3.5) * 2.5, (math.floor(i / 8) - 3.5) * 2.5
        G.setColor(0.5 + 0.5 * math.sin(i), 0.5 + 0.5 * math.cos(i * 1.3), 0.7, 1)
        G.drawCube(x, 0.5, z, 1, 1, 1, S.assets.tex, 0, t + i * 0.1, 0)
    end
    G.setColor(1, 1, 1, 1)
    for i = 0, 3 do G.drawModel(S.assets.monkey, (i - 1.5) * 3, 1.2, 0, 0, t + i, 0) end
    G.setDepthTest(false); G.setDepthWrite(false)
end

local function scene2D(t)
    for i = 0, 199 do
        local x = (i * 37 + t * 40) % 320
        local y = (i * 53 + math.sin(t + i) * 20) % 240
        G.setColor(0.5 + 0.5 * math.sin(i), 0.6, 1 - 0.5 * math.sin(i), 0.6)
        if i % 2 == 0 then G.drawSprite(S.assets.tex, x, y, 24, 24, t + i, 12, 12) else G.drawRect("fill", x, y, 20, 14) end
    end
    G.setColor(1, 1, 1, 1)
end

function crayon.update(dt)
    S.time = S.time + dt
    S.frames = S.frames + 1
    if S.time >= PHASE_SECONDS then
        local ms = S.time / S.frames * 1000
        S.results[#S.results + 1] = { name = phases[S.phase], ms = ms }
        print(string.format("   %-12s %8.3f ms/frame   %7.1f fps", phases[S.phase], ms, 1000 / ms))
        S.phase, S.time, S.frames = S.phase + 1, 0, 0
        if S.phase > #phases then
            print("== benchmark finished")
            crayon.window.quit()
        end
    end
end

function crayon.draw()
    local t = crayon.time.getTime()
    G.clear(0.08, 0.10, 0.20, 1)
    local p = S.phase
    if p == 2 or p == 4 then scene3D(t) end
    if p == 3 or p == 4 then scene2D(t) end
    G.setColor(1, 1, 1, 1)
    G.drawText("phase: " .. (phases[p] or "done"), 6, 6)
end

function crayon.keydown(k) if k == "escape" then crayon.window.quit() end end
