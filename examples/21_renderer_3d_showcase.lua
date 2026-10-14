-- ============================================================================
-- Example 21: 3D showcase for both renderers (GPU and CPU rasterizer)
--
--   crayon --game examples/21_renderer_3d_showcase.lua --renderer=soft
--
-- Exercises everything the software backend reimplements: primitives, glTF/OBJ models,
-- directional + point + spot lights, gouraud/flat/unlit shading, fog, PS1 jitter + affine UVs,
-- billboards (spherical + cylindrical, alpha-blended), 3D lines/grid/axes, backface culling,
-- depth state and the 2D HUD on top.
--
-- Keys:  A / D orbit    W / S zoom     1 2 3 shading (gouraud / flat / unlit)
--        J jitter       T affine UVs   F fog           O dither     C backface cull
--        L lights on/off      ESC quit
-- ============================================================================
local G = crayon.graphics

function crayon.config(t)
    t.window.title = "21 - 3D showcase"
    t.window.width = 320
    t.window.height = 240
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.modules.physics3D = false
    t.modules.audio = false
    t.modules.mesh3D = true
end

local S = {
    angle = 20, dist = 11, shading = "gouraud",
    jitter = false, affine = false, fog = true, dither = true, cull = false, lights = true,
    assets = {},
}

function crayon.init()
    G = crayon.graphics
    S.assets.tex   = G.loadTexture("assets/models/cube_gltf/Cube_BaseColor.png")
    S.assets.white = G.getWhiteTexture()
    S.assets.monkey = G.loadModel("assets/models/monkey.obj")
    S.assets.car    = G.loadModel("14_car_game/assets/car_body.glb")
    S.assets.crate  = G.loadModel("assets/models/cube_gltf/Cube.gltf")
    print("renderer: " .. G.getRendererName() .. " (software=" .. tostring(G.isSoftwareRenderer()) .. ")")
end

local function applyState()
    G.setShadingMode(S.shading)
    G.setCullFace(S.cull)
    G.setRetroEffects({
        jitterResolution = S.jitter and { 160, 120 } or false,
        affine = S.affine and 1.0 or 0.0,
        dither = S.dither,
        fog = S.fog and { startDist = 10, endDist = 45, color = { 0.08, 0.10, 0.22 } } or false,
    })
end

function crayon.keydown(k, isRepeat)
    if isRepeat then return end
    if k == "1" then S.shading = "gouraud" elseif k == "2" then S.shading = "flat" elseif k == "3" then S.shading = "unlit"
    elseif k == "j" then S.jitter = not S.jitter
    elseif k == "t" then S.affine = not S.affine
    elseif k == "f" then S.fog = not S.fog
    elseif k == "o" then S.dither = not S.dither
    elseif k == "c" then S.cull = not S.cull
    elseif k == "l" then S.lights = not S.lights
    elseif k == "escape" then crayon.window.quit() end
    applyState()
end

function crayon.update(dt)
    if crayon.key.isDown("a") then S.angle = S.angle - 60 * dt end
    if crayon.key.isDown("d") then S.angle = S.angle + 60 * dt end
    if crayon.key.isDown("w") then S.dist = math.max(4, S.dist - 6 * dt) end
    if crayon.key.isDown("s") then S.dist = math.min(25, S.dist + 6 * dt) end
    S.angle = S.angle + 8 * dt
    local r = math.rad(S.angle)
    G.setCamera3D({ position = { math.sin(r) * S.dist, 4.5, math.cos(r) * S.dist },
                    target = { 0, 1, 0 }, up = { 0, 1, 0 }, fov = 55 })
end

function crayon.draw()
    local t = crayon.time.getTime()
    applyState()
    G.clear(0.08, 0.10, 0.22, 1)
    G.setDepthTest(true); G.setDepthWrite(true)

    G.setLight(-0.5, -1.0, -0.7, 1.0, 0.95, 0.85, 0.25, 0.25, 0.35)
    G.setPointLightEnabled(1, S.lights)
    G.setSpotLightEnabled(1, S.lights)
    G.setPointLight(1, math.cos(t) * 4, 2.2, math.sin(t) * 4, 1.0, 0.6, 0.25, 10, 2.0)
    G.setSpotLight(1, 0, 8, 0, 0, -1, 0, 14, 0.3, 0.9, 1.0, 3.0, 15, 30)

    -- floor + grid + axes
    G.setColor(0.55, 0.6, 0.7, 1); G.drawPlane(0, 0, 0, 40, 40, S.assets.tex)
    G.setColor(0.8, 0.8, 0.95, 1); G.drawGrid3D(30, 30, 0.02)
    G.drawAxes3D(0, 0.03, 0, 2)

    -- every procedural primitive
    G.setColor(0.9, 0.35, 0.35, 1); G.drawCube(-5, 0.75, 3, 1.5, 1.5, 1.5, S.assets.tex, 0, t, 0)
    G.setColor(0.35, 0.9, 0.5, 1);  G.drawSphere(-3, 0.7, 5, 0.7)
    G.setColor(0.3, 0.6, 0.95, 1);  G.drawCylinder(-1, 1.0, 5, 0.6, 2.0)
    G.setColor(0.95, 0.8, 0.3, 1);  G.drawCone(1, 0.8, 5, 0.7, 1.6)
    G.setColor(0.8, 0.4, 0.9, 1);   G.drawPyramid(3, 0.75, 5, 1.4, 1.5)
    G.setColor(0.4, 0.9, 0.9, 1);   G.drawTorus(5, 1.0, 3, 0.8, 0.25, nil, t, 0, 0)
    G.setColor(0.95, 0.55, 0.3, 1); G.drawCapsule(6, 0.8, 0, 0.4, 0.8)

    -- models: OBJ (welded), glTF crate, multi-part glTF car
    G.setColor(1, 1, 1, 1)
    G.drawModel(S.assets.monkey, 0, 1.1, 0, 0, t * 0.8, 0)
    G.drawModel(S.assets.crate, 4, 1, -2, 0, -t * 0.5, 0, 1.6, 1.6, 1.6)
    G.drawModel(S.assets.car, -5, 0, -3, 0, t * 0.3, 0)

    -- billboards: spherical (glowing) and cylindrical, alpha-blended over the scene
    G.setColor(1, 0.85, 0.3, 0.6); G.drawBillboard(0, 3.1, 0, 2.2, 2.2, S.assets.white)
    G.setColor(0.4, 0.8, 1.0, 0.5); G.drawBillboard(3, 1.5, -5, 1.5, 3, S.assets.white, "cylindrical")
    G.setColor(1, 1, 1, 1)

    -- lines
    G.drawLine3D(-6, 0.05, -6, 6, 0.05, 6)
    G.drawLine3D(-6, 0.05, 6, 6, 0.05, -6)

    -- HUD (2D on top of 3D, depth off)
    G.setDepthTest(false); G.setDepthWrite(false)
    G.setColor(0, 0, 0, 0.6); G.drawRect("fill", 2, 2, 316, 36)
    G.setColor(1, 1, 1, 1)
    G.drawText(G.getRendererName(), 6, 5)
    G.drawText(string.format("%s  jitter:%s affine:%s fog:%s dither:%s cull:%s lights:%s  %d fps",
        S.shading, tostring(S.jitter), tostring(S.affine), tostring(S.fog), tostring(S.dither),
        tostring(S.cull), tostring(S.lights), math.floor(crayon.window.getFps())), 6, 16)
    G.setColor(0.7, 0.8, 0.9, 1)
    G.drawText("A/D orbit  W/S zoom  1-3 shading  J T F O C L toggles", 6, 27)
end
