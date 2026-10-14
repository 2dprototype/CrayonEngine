-- ============================================================================
-- Example 20: Renderer self-test (works on BOTH backends)
--
--   crayon --game examples/20_renderer_selftest.lua                 (auto)
--   crayon --game examples/20_renderer_selftest.lua --renderer=soft (CPU rasterizer)
--   crayon --game examples/20_renderer_selftest.lua --renderer=gl   (GPU)
--
-- Runs a list of automated checks (asset loading, projection math, state setters, graceful
-- failure of GPU-only features in software mode), renders a full 3D + 2D frame for ~2 seconds
-- while measuring frame time, prints a PASS/FAIL report to the console and the screen, then quits.
-- Look at the screen too: a correct frame shows a lit textured floor, a spinning cube, the monkey
-- and the car, a glowing billboard, a gradient bar, a circle and a clipped magenta box.
-- ============================================================================
local G = crayon.graphics

function crayon.config(t)
    t.window.title = "20 - Renderer self-test"
    t.window.width = 320
    t.window.height = 240
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.modules.physics3D = false
    t.modules.audio = false
    t.modules.mesh3D = true
end

local S = {
    results = {},
    frames = 0,
    elapsed = 0,
    maxFrames = 120,
    done = false,
    assets = {},
}

local function check(name, fn)
    local ok, res = pcall(fn)
    local pass = ok and (res == nil or res == true)
    local detail = ""
    if not ok then detail = tostring(res) elseif res == false then detail = "returned false" end
    S.results[#S.results + 1] = { name = name, pass = pass, detail = detail }
    print(string.format("[%s] %s%s", pass and "PASS" or "FAIL", name, detail ~= "" and ("  -> " .. detail) or ""))
end

local function near(a, b, eps) return math.abs(a - b) <= eps end

function crayon.init()
    G = crayon.graphics
    local soft = G.isSoftwareRenderer()
    print("== Renderer: " .. tostring(G.getRendererName()) .. "  (software=" .. tostring(soft) .. ")")

    check("renderer name is a non-empty string", function()
        local n = G.getRendererName()
        return type(n) == "string" and #n > 0
    end)
    check("isSoftwareRenderer returns a boolean", function() return type(soft) == "boolean" end)

    -- ---- asset loading -------------------------------------------------------------------
    check("loadTexture (png)", function()
        S.assets.tex = G.loadTexture("assets/models/cube_gltf/Cube_BaseColor.png")
        return S.assets.tex:getWidth() > 0 and S.assets.tex:getHeight() > 0
    end)
    check("loadTexture: white texture", function() S.assets.white = G.getWhiteTexture(); return S.assets.white ~= nil end)
    check("loadModel (obj): monkey", function()
        S.assets.monkey = G.loadModel("assets/models/monkey.obj")
        return S.assets.monkey:isValid() and S.assets.monkey:getPartCount() >= 1
    end)
    check("loadModel (obj): welded mesh is not empty", function()
        local x0, y0, z0, x1, y1, z1 = S.assets.monkey:getBounds()
        return (x1 - x0) > 0.5 and (y1 - y0) > 0.5 and (z1 - z0) > 0.5
    end)
    check("loadModel (gltf): Cube.gltf", function()
        S.assets.cube = G.loadModel("assets/models/cube_gltf/Cube.gltf")
        return S.assets.cube:isValid() and S.assets.cube:getPartCount() >= 1
    end)
    check("loadModel (glb): car body", function()
        S.assets.car = G.loadModel("14_car_game/assets/car_body.glb")
        return S.assets.car:isValid() and S.assets.car:getPartCount() >= 2
    end)
    check("loadModel built-in 'cube'", function() S.assets.prim = G.loadModel("cube"); return S.assets.prim:isValid() end)

    -- ---- projection math (independent of the backend) -----------------------------------
    G.setCamera3D({ position = { 0, 0, 5 }, target = { 0, 0, 0 }, up = { 0, 1, 0 }, fov = 60 })
    S.needProjection = true

    -- ---- fixed-function state setters must never error ----------------------------------
    check("state setters (depth / cull / shading / retro / lights)", function()
        G.setDepthTest(true); G.setDepthWrite(true)
        G.setCullFace(false); G.setCullFace(true); G.setCullFace("front"); G.setCullFace("back"); G.setCullFace(false)
        for _, m in ipairs({ "gouraud", "flat", "unlit", "gouraud" }) do G.setShadingMode(m) end
        G.setRetroEffects({ jitterResolution = { 160, 120 }, affine = 0.0, dither = false,
                            fog = { startDist = 8, endDist = 40, color = { 0.1, 0.14, 0.3 } } })
        G.setLight(-0.5, -1.0, -0.7, 1.0, 0.95, 0.85, 0.25, 0.25, 0.35)
        G.setPointLight(1, 2, 3, 2, 1.0, 0.6, 0.3, 9.0, 1.2); G.setPointLightEnabled(1, true)
        G.setSpotLight(1, 0, 7, 0, 0, -1, 0, 12, 0.2, 0.9, 1.0, 3.0, 15, 30); G.setSpotLightEnabled(1, true)
        return true
    end)

    -- ---- features that need the GPU must fail *gracefully* in software mode --------------
    check("GPU-only: createCanvas behaves as documented", function()
        local ok = pcall(G.createCanvas, 64, 64)
        if soft then return ok == false end   -- software: a catchable Lua error, no crash
        return ok == true                     -- GL: canvas works
    end)
    check("GPU-only: loadShader behaves as documented", function()
        local ok = pcall(G.loadShader, "void main(){ gl_FragColor = vec4(1.0); }")
        if soft then return ok == false end
        return true                           -- on GL the result depends on the GLSL; just must not crash
    end)
    check("post-process pushEffect does not crash", function()
        pcall(G.pushEffect, "crt"); pcall(G.clearEffects)
        return true
    end)
end

local function projectionChecks()
    check("project(): origin lands at screen centre and is visible", function()
        local sx, sy, vis = G.project(0, 0, 0)
        return vis and near(sx, 160, 1.5) and near(sy, 120, 1.5)
    end)
    check("project(): point right of centre has larger x", function()
        local a = select(1, G.project(0, 0, 0)); local b = select(1, G.project(1, 0, 0))
        return b > a
    end)
    check("project(): point behind the camera is not visible", function()
        local _, _, vis = G.project(0, 0, 20)
        return vis == false
    end)
    check("unproject(project(p)) ray passes through p", function()
        local p = { 1.0, 0.5, -1.0 }
        local sx, sy = G.project(p[1], p[2], p[3])
        local ox, oy, oz, dx, dy, dz = G.unproject(sx, sy)
        local vx, vy, vz = p[1] - ox, p[2] - oy, p[3] - oz
        local t = vx * dx + vy * dy + vz * dz
        local cx, cy, cz = ox + dx * t - p[1], oy + dy * t - p[2], oz + dz * t - p[3]
        return math.sqrt(cx * cx + cy * cy + cz * cz) < 0.05
    end)
end

function crayon.update(dt)
    S.elapsed = S.elapsed + dt
    if S.needProjection then
        S.needProjection = false
        projectionChecks()
    end
    local t = S.elapsed
    G.setCamera3D({ position = { math.sin(t * 0.5) * 7, 2.5, math.cos(t * 0.5) * 7 + 1 },
                    target = { 0, 0.8, 0 }, up = { 0, 1, 0 }, fov = 60 })
end

local function drawScene(t)
    G.clear(0.10, 0.14, 0.30, 1.0)
    G.setDepthTest(true); G.setDepthWrite(true)
    G.setShadingMode("gouraud")
    G.setLight(-0.5, -1.0, -0.7, 1.0, 0.95, 0.85, 0.25, 0.25, 0.35)
    G.setPointLight(1, 2, 3, 2, 1.0, 0.6, 0.3, 9.0, 1.2); G.setPointLightEnabled(1, true)
    G.setRetroEffects({ jitterResolution = false, affine = 0.0, dither = true,
                        fog = { startDist = 8, endDist = 40, color = { 0.1, 0.14, 0.3 } } })

    G.setColor(1, 1, 1, 1)
    G.drawPlane(0, 0, 0, 40, 40, S.assets.tex)
    G.drawGrid3D(20, 20, 0.01)
    G.setColor(0.9, 0.4, 0.4, 1); G.drawCube(-3, 0.5, 0, 1, 1, 1, S.assets.tex, 0, t, 0)
    G.setColor(0.4, 0.9, 0.5, 1); G.drawSphere(3, 0.6, 0, 0.6)
    G.setColor(1, 1, 1, 1)
    if S.assets.monkey then G.drawModel(S.assets.monkey, 0, 0.9, 0, 0, t, 0) end
    if S.assets.car then G.drawModel(S.assets.car, 0, 0, -4, 0, -t, 0) end
    G.drawBillboard(0, 2.6, 1, 2, 2, S.assets.white)
    G.drawLine3D(-4, 0.02, -4, 4, 0.02, 4)

    G.setDepthTest(false); G.setDepthWrite(false)
    G.setColor(0, 0, 0, 0.55); G.drawRect("fill", 4, 4, 150, 22)
    G.setColor(1, 1, 1, 1); G.drawText("SELF-TEST  " .. string.upper(G.isSoftwareRenderer() and "software" or "opengl"), 8, 10)
    G.drawGradientRect(4, 206, 120, 24, { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { 1, 1, 0, 1 })
    G.setColor(1, 0.8, 0.2, 0.8); G.drawCircle("fill", 290, 30, 18)
    G.pushScissor(100, 100, 40, 30)
    G.setColor(1, 0, 1, 1); G.drawRect("fill", 90, 90, 70, 60)
    G.popScissor()
    G.setColor(1, 1, 1, 1)
end

local function drawReport()
    G.clear(0.05, 0.06, 0.1, 1)
    local pass, total = 0, #S.results
    for _, r in ipairs(S.results) do if r.pass then pass = pass + 1 end end
    G.setColor(1, 1, 1, 1)
    G.drawText(string.format("RESULT: %d/%d passed  %s", pass, total, pass == total and "OK" or "FAILED"), 6, 4)
    G.drawText(string.format("avg frame %.2f ms", (S.avgMs or 0)), 6, 14)
    local y = 26
    for _, r in ipairs(S.results) do
        if y > 232 then break end
        if r.pass then G.setColor(0.4, 1, 0.5, 1) else G.setColor(1, 0.4, 0.4, 1) end
        G.drawText((r.pass and "ok  " or "FAIL ") .. r.name, 6, y)
        y = y + 9
    end
end

function crayon.draw()
    if S.done then drawReport(); return end
    drawScene(S.elapsed)
    S.frames = S.frames + 1
    if S.frames >= S.maxFrames then
        S.avgMs = (S.elapsed / S.frames) * 1000.0
        S.done = true
        local pass = 0
        for _, r in ipairs(S.results) do if r.pass then pass = pass + 1 end end
        print(string.format("== %d/%d checks passed | %d frames | avg %.2f ms/frame (%.0f fps)",
              pass, #S.results, S.frames, S.avgMs, 1000.0 / S.avgMs))
        print(pass == #S.results and "== SELFTEST PASSED" or "== SELFTEST FAILED")
        S.quitAt = S.elapsed + 6.0   -- keep the report on screen for a few seconds
    end
end

function crayon.keydown(k)
    if k == "escape" then crayon.window.quit() end
end

local _update = crayon.update
function crayon.update(dt)
    _update(dt)
    if S.quitAt and S.elapsed >= S.quitAt then crayon.window.quit() end
end
