-- ============================================================================
-- Example 26: new 3D features (works on both renderers)
--
--   crayon --game examples/26_3d_features.lua [--renderer=soft]
--
--  * graphics.loadSkybox          procedural cubemap built with ImageData (no assets needed)
--  * graphics.createTerrain       heightmap terrain, getHeightAt / getNormalAt / raycast
--  * graphics.newStaticBatch      200 trees merged into a few chunked meshes
--  * transparent models           alpha < 1 draws are sorted back-to-front and blended
--  * graphics.drawModelLOD        detail levels by camera distance
--  * auto culling                 graphics.setAutoCull / setMaxDrawDistance / getCullStats
--
-- Keys: A/D orbit   W/S zoom   C toggle auto culling   B toggle static batch   ESC quit
-- ============================================================================
local G = crayon.graphics

function crayon.config(t)
    t.window.title = "26 - 3D features"
    t.window.width = 320
    t.window.height = 240
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.modules.physics3D = false
    t.modules.audio = false
    t.modules.mesh3D = true
    t.graphics.renderer = "soft"
end

local S = { angle = 30, dist = 38, cull = true, batch_on = true, t = 0 }

-- six cube faces: blue gradient sky with a sun on the +X horizon and a ground-coloured -Y face
local function make_sky()
    local N = 64
    local faces = {}
    for i = 1, 6 do faces[i] = G.newImageData(N, N) end
    for i = 1, 6 do
        local img = faces[i]
        for y = 0, N - 1 do
            for x = 0, N - 1 do
                local v = y / (N - 1)
                local r, g, b
                if i == 3 then r, g, b = 0.25, 0.45, 0.85              -- +Y zenith
                elseif i == 4 then r, g, b = 0.35, 0.4, 0.3            -- -Y ground
                else
                    local h = 1 - v                                      -- 1 at the top of a side face, 0 at the horizon bottom
                    r, g, b = 0.55 + (0.25 - 0.55) * h, 0.75 + (0.45 - 0.75) * h, 0.95 + (0.85 - 0.95) * h
                    if i == 1 and v > 0.7 then                           -- sun glow on the +X face near the horizon
                        local dx, dy = (x / N - 0.5), (v - 0.9)
                        local d = math.sqrt(dx * dx + dy * dy)
                        if d < 0.12 then r, g, b = 1, 0.95, 0.7 elseif d < 0.3 then local k = 1 - (d - 0.12) / 0.18; r, g, b = r + (1 - r) * k * 0.5, g + (0.9 - g) * k * 0.4, b end
                    end
                end
                img:setPixel(x, y, r, g, b, 1)
            end
        end
    end
    return G.loadSkybox(faces)
end

local function make_heights(n)
    local rows = {}
    for z = 0, n - 1 do
        local row = {}
        for x = 0, n - 1 do
            local h = 0.45 + 0.25 * math.sin(x * 0.11) * math.cos(z * 0.08) + 0.12 * math.sin(x * 0.31 + z * 0.17)
            row[x + 1] = math.max(0, math.min(1, h))
        end
        rows[z + 1] = row
        end
    return rows
end

function crayon.init()
    G = crayon.graphics
    S.sky = make_sky()
    S.terrain = G.createTerrain(make_heights(97), { width = 192, height = 14, depth = 192, x = -96, y = 0, z = -96,
                                                    uvRepeat = 24, chunkCells = 24,
                                                    colorLow = { 0.35, 0.55, 0.25 }, colorHigh = { 0.85, 0.85, 0.75 } })
    S.tex = G.loadTexture("assets/models/cube_gltf/Cube_BaseColor.png")
    S.cone, S.cube, S.sphere = G.loadModel("cone"), G.loadModel("cube"), G.loadModel("sphere")
    S.monkey = G.loadModel("assets/models/monkey.obj")
    S.low_sphere = G.loadModel("pyramid")

    -- 200 trees (trunk cube + cone) placed on the terrain surface, merged into chunked meshes
    S.batch = G.newStaticBatch(32)
    S.trees = {}
    local seed = 7
    local function rnd() seed = (seed * 1103515245 + 12345) % 2147483648; return seed / 2147483648 end
    for i = 1, 200 do
        local x, z = (rnd() - 0.5) * 170, (rnd() - 0.5) * 170
        local y = S.terrain:getHeightAt(x, z)
        local s = 0.8 + rnd() * 1.2
        S.trees[i] = { x = x, y = y, z = z, s = s }
        S.batch:add(S.cube, x, y + 0.6 * s, z, 0, 0, 0, 0.3 * s, 1.2 * s, 0.3 * s)
        S.batch:add(S.cone, x, y + 2.4 * s, z, 0, 0, 0, 1.6 * s, 2.6 * s, 1.6 * s)
    end
    S.batch:build()
    local inst, groups, verts, tris = S.batch:getStats()
    print(string.format("static batch: %d instances -> %d meshes (%d tris)", inst, groups, tris))
    print("renderer: " .. G.getRendererName())
end

function crayon.keydown(k, isRepeat)
    if isRepeat then return end
    if k == "c" then S.cull = not S.cull; G.setAutoCull(S.cull)
    elseif k == "b" then S.batch_on = not S.batch_on
    elseif k == "escape" then crayon.window.quit() end
end

function crayon.update(dt)
    S.t = S.t + dt
    if crayon.key.isDown("a") then S.angle = S.angle - 50 * dt end
    if crayon.key.isDown("d") then S.angle = S.angle + 50 * dt end
    if crayon.key.isDown("w") then S.dist = math.max(8, S.dist - 15 * dt) end
    if crayon.key.isDown("s") then S.dist = math.min(90, S.dist + 15 * dt) end
    S.angle = S.angle + 5 * dt
    local r = math.rad(S.angle)
    local cx, cz = math.sin(r) * S.dist, math.cos(r) * S.dist
    local cy = S.terrain:getHeightAt(cx, cz) + 7
    G.setCamera3D({ position = { cx, cy, cz }, target = { 0, S.terrain:getHeightAt(0, 0) + 2, 0 }, up = { 0, 1, 0 }, fov = 62 })
end

function crayon.draw()
    local t = S.t
    G.clear(0.5, 0.7, 0.95, 1)
    G.setDepthTest(true); G.setDepthWrite(true)
    G.setCullFace(false)
    G.setShadingMode("gouraud")
    G.setLight(-0.5, -1.0, -0.35, 1.0, 0.95, 0.85, 0.35, 0.38, 0.45)
    G.setRetroEffects({ jitterResolution = false, affine = 0.0, dither = true,
                        fog = { startDist = 45, endDist = 140, color = { 0.55, 0.72, 0.95 } } })
    G.setMaxDrawDistance(150)

    S.terrain:draw(S.tex)
    if S.batch_on then
        S.batch:draw()
    end
    G.setColor(0.3, 0.55, 0.2, 1)   -- (tint for the next opaque draws)

    -- LOD: a monkey that turns into a pyramid with distance (distances: high < 25, low beyond; culled > 120)
    G.setColor(1, 1, 1, 1)
    for i, p in ipairs({ { -10, 0 }, { 10, 6 }, { 30, -20 }, { -45, 30 } }) do
        local y = S.terrain:getHeightAt(p[1], p[2])
        G.drawModelLOD({ S.monkey, S.low_sphere }, { 25, 120 }, p[1], y + 1.6, p[2], 0, t + i, 0, 1.4, 1.4, 1.4)
    end

    -- transparent models, deliberately drawn nearest-first: the queue sorts them back-to-front
    local gy = S.terrain:getHeightAt(0, 0)
    G.setColor(0.2, 0.9, 0.4, 0.45); G.drawSphere(1.5, gy + 3, 0, 2.2)
    G.setColor(0.2, 0.5, 1.0, 0.45); G.drawSphere(-1.0, gy + 3, -1.5, 2.2)
    G.setColor(1.0, 0.4, 0.3, 0.45); G.drawCube(0, gy + 3, -3, 3, 3, 3, nil, 0, t, 0)
    G.setColor(1, 1, 1, 1)

    -- sky last: it only fills pixels nothing else covered
    G.drawSkybox(S.sky)

    G.setDepthTest(false); G.setDepthWrite(false)   -- also flushes the transparent queue before the HUD
    local culled3d, culled2d = G.getCullStats()
    local inst, groups = S.batch:getStats()
    G.setColor(0, 0, 0, 0.55); G.drawRect("fill", 2, 2, 316, 30)
    G.setColor(1, 1, 1, 1)
    G.drawText(string.format("%s | %d fps | culled draws: %d | batch: %d inst -> %d meshes", G.getRendererName(), math.floor(crayon.window.getFps()), culled3d, inst, groups), 6, 5)
    G.drawText(string.format("auto-cull: %s  batch: %s   A/D W/S  C cull  B batch", tostring(S.cull), tostring(S.batch_on)), 6, 17)
    local rx, ry, rz = S.terrain:raycast(0, 100, 0, 0, -1, 0)
    if rx then G.drawText(string.format("ground below origin: y=%.2f", ry), 6, 226) end
end
