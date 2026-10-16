-- ============================================================================
-- Example 28: level collision from a model (Blender workflow), terrain colliders, convex props
--
--   crayon --game examples/28_collision.lua [--renderer=soft]
--
--  * physics3D.createModelBody(level)      collision comes from the model's own collision nodes:
--        Ground_col / Ramp_col  (trimesh)   UBX_Crate (box)   UCX_Pillar (convex hull)
--    The collision nodes are hidden when drawing; press C to show them.
--  * model:getNodes()                      spawn points and their custom properties (Blender custom props)
--  * physics3D.createConvexBody(...)       dynamic props from a visual model (hull of the monkey)
--  * physics3D.createCompoundBody(...)     a dynamic "T" made of two boxes
--  * terrain:createBody()                  heightfield collider for a terrain
--
-- Keys: SPACE drop a ball   B drop a box   M drop a monkey (convex hull)   T drop a T-shape
--       C show/hide collision geometry   A/D orbit   ESC quit
--
-- In Blender: name collision objects UBX_ / USP_ / UCP_ / UCX_ (box / sphere / capsule / convex hull) or
-- with "_col" ("col_") for a triangle mesh; add Custom Properties on empties for spawn points; export
-- glTF 2.0 (+Y up, Custom Properties on). tools/make_collision_demo_gltf.py writes this demo level.
-- ============================================================================
local G = crayon.graphics
local P = crayon.physics3D

function crayon.config(t)
    t.window.title = "28 - collision"
    t.window.width = 320
    t.window.height = 240
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.modules.physics3D = true
    t.modules.audio = false
    t.modules.mesh3D = true
    -- t.graphics.renderer = "gl"
end

local S = { bodies = {}, angle = 25, show_col = false, spawn = { x = 0, y = 1, z = 8 }, t = 0 }

local function add(kind, body, model, sx, sy, sz)
    S.bodies[#S.bodies + 1] = { kind = kind, body = body, model = model, sx = sx or 1, sy = sy or 1, sz = sz or 1 }
    if #S.bodies > 60 then            -- keep the scene light: drop the oldest
        local old = table.remove(S.bodies, 1)
        old.body:destroy()
    end
end

function crayon.init()
    G = crayon.graphics
    P = crayon.physics3D
    S.level = G.loadModel("assets/models/collision_demo.gltf")
    S.monkey = G.loadModel("assets/models/monkey.obj")
    S.tex = G.loadTexture("assets/models/cube_gltf/Cube_BaseColor.png")

    print("collision parts in the level:")
    for _, part in ipairs(S.level:getCollisionParts()) do
        print(string.format("  %-14s %-7s hidden=%s", part.name, part.kind, tostring(part.hidden)))
    end

    -- the whole level collider in one call (mesh + box + hull children as one static body)
    S.level_body = P.createModelBody(S.level, { friction = 0.6, restitution = 0.1 })

    -- spawn points are plain nodes with custom properties
    for _, n in ipairs(S.level:getNodes()) do
        if n.properties.team then
            print(string.format("spawn '%s' team=%s at (%.1f, %.1f, %.1f)", n.name, n.properties.team, n.wx, n.wy, n.wz))
            if n.properties.kind == "player" then S.spawn = { x = n.wx, y = n.wy + 6, z = n.wz } end
        end
    end

    -- a small terrain next to the level with its own heightfield collider
    local rows = {}
    for z = 0, 32 do
        rows[z + 1] = {}
        for x = 0, 32 do rows[z + 1][x + 1] = 0.35 + 0.3 * math.sin(x * 0.3) * math.cos(z * 0.25) end
    end
    S.terrain = G.createTerrain(rows, { width = 24, height = 5, depth = 24, x = 22, y = 0, z = -12, uvRepeat = 8,
                                        colorLow = { 0.35, 0.55, 0.25 }, colorHigh = { 0.85, 0.85, 0.7 } })
    S.terrain_body = S.terrain:createBody(0.7, 0.0)
    print("renderer: " .. G.getRendererName())
end

local function drop(kind)
    local jitter = (math.random() - 0.5) * 3
    local x, y, z = S.spawn.x + jitter, S.spawn.y, S.spawn.z + (math.random() - 0.5) * 2
    if kind == "ball" then
        add("sphere", P.createSphere(x, y, z, 0.5, "dynamic", 0.5, 0.35, 800))
    elseif kind == "box" then
        add("box", P.createBox(x, y, z, 0.5, 0.5, 0.5, "dynamic", 0.5, 0.2, 800))
    elseif kind == "monkey" then
        add("monkey", P.createConvexBody(x, y, z, S.monkey, { friction = 0.5, restitution = 0.2, density = 500 }), S.monkey, 1, 1, 1)
    elseif kind == "t" then
        local b = P.createCompoundBody(x, y, z, {
            { shape = "box", hx = 1.0, hy = 0.25, hz = 0.25, y = 0.75 },
            { shape = "box", hx = 0.25, hy = 0.75, hz = 0.25, y = 0 },
        }, { motion = "dynamic", friction = 0.5, density = 700 })
        add("t", b)
    end
end

function crayon.keydown(k, isRepeat)
    if isRepeat then return end
    if k == "space" then drop("ball")
    elseif k == "b" then drop("box")
    elseif k == "m" then drop("monkey")
    elseif k == "t" then drop("t")
    elseif k == "c" then S.show_col = not S.show_col; S.level:setCollisionVisible(S.show_col)
    elseif k == "escape" then crayon.window.quit() end
end

function crayon.update(dt)
    S.t = S.t + dt
    if crayon.key.isDown("a") then S.angle = S.angle - 60 * dt end
    if crayon.key.isDown("d") then S.angle = S.angle + 60 * dt end
    S.angle = S.angle + 6 * dt
    local r = math.rad(S.angle)
    G.setCamera3D({ position = { math.sin(r) * 26, 12, math.cos(r) * 26 }, target = { 6, 1, 0 }, up = { 0, 1, 0 }, fov = 60 })
    if math.floor(S.t * 1.5) ~= math.floor((S.t - dt) * 1.5) and #S.bodies < 24 then drop(({ "ball", "box", "monkey", "t" })[math.random(4)]) end
end

function crayon.draw()
    G.clear(0.45, 0.65, 0.9, 1)
    G.setDepthTest(true); G.setDepthWrite(true)
    G.setLight(-0.5, -1, -0.4, 1, 0.95, 0.85, 0.35, 0.38, 0.45)
    G.setRetroEffects({ jitterResolution = false, affine = 0.0, dither = true,
                        fog = { startDist = 40, endDist = 120, color = { 0.45, 0.65, 0.9 } } })

    G.setColor(0.8, 0.8, 0.85, 1)
    G.drawModel(S.level, 0, 0, 0)
    G.setColor(1, 1, 1, 1)
    S.terrain:draw(S.tex)

    for _, b in ipairs(S.bodies) do
        local x, y, z = b.body:getPosition()
        local rx, ry, rz = b.body:getRotation()
        if b.kind == "sphere" then G.setColor(0.9, 0.4, 0.3, 1); G.drawSphere(x, y, z, 0.5)
        elseif b.kind == "box" then G.setColor(0.4, 0.7, 0.95, 1); G.drawCube(x, y, z, 1, 1, 1, nil, rx, ry, rz)
        elseif b.kind == "monkey" then G.setColor(1, 1, 1, 1); G.drawModel(b.model, x, y, z, rx, ry, rz)
        elseif b.kind == "t" then
            G.setColor(0.9, 0.8, 0.3, 1)
            -- the compound's origin is its centre of mass handle; draw the two boxes in body space
            local m = { { 0, 0.75, 0, 2, 0.5, 0.5 }, { 0, 0, 0, 0.5, 1.5, 0.5 } }
            G.pushMatrix(); G.translate(x, y, z); G.rotate(rz, 0, 0, 1); G.rotate(ry, 0, 1, 0); G.rotate(rx, 1, 0, 0)   -- same order as drawModel
            for _, p in ipairs(m) do G.drawCube(p[1], p[2], p[3], p[4], p[5], p[6]) end
            G.popMatrix()
        end
    end
    G.setColor(1, 1, 1, 1)

    G.setDepthTest(false); G.setDepthWrite(false)
    G.setColor(0, 0, 0, 0.55); G.drawRect("fill", 2, 2, 316, 28)
    G.setColor(1, 1, 1, 1)
    G.drawText(string.format("%s | %d bodies | collision shown: %s", G.getRendererName(), #S.bodies, tostring(S.show_col)), 6, 5)
    G.drawText("SPACE ball  B box  M monkey hull  T compound  C collision  A/D orbit", 6, 17)
end
