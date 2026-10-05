-- ============================================================================
-- Example 19: 4D Physics (hv4d)
--
-- You live in a 3D slice of a 4D world. Bodies are real 4D objects (tesseracts,
-- 16-cells, 24-cells, 600-cells, hyperspheres...). What you SEE is the 3D
-- cross-section where the world meets the slice hyperplane w = sliceW.
-- Slide the slice along w to scan through the objects, and watch things
-- appear / vanish / morph.  Bodies in other w-layers can still hit you!
--
--   Arrows ........ orbit camera        Z / X ......... zoom
--   Q / E ......... move slice (-w/+w)  R ............. reset scene
--   1-6 ........... spawn 8-cell, 5-cell, 16-cell, 24-cell, 600-cell, hypersphere
--   Left click .... shoot a ray through the slice and punch what it hits
--   SPACE ......... blast everything upward and sideways in w
--   F / P / B / V . toggle fill / projection (ghosts) / bounds / velocities
-- ============================================================================

-- NOTE: crayon.physics4D is only populated *after* the engine has finished
-- reading the config phase and registered its Lua modules. Do NOT resolve it
-- at file top-level (it will be nil). We forward-declare a local and bind it
-- inside crayon.init(), which runs once modules are live.
local P

local sliceW     = 0.0
local yaw, pitch, dist = 0.6, 0.45, 13.0
local flags = { shapes = true, fill = true, projection = true, contacts = true, bounds = false, velocities = false }

local bodies = {}
local spinner = nil
local hits, lastHit = 0, "-"
local spawnN = 0

local shapes = { "8cell", "5cell", "16cell", "24cell", "600cell" }

local function spawn(shape, x, y, z, w)
    local b
    if shape == "hypersphere" then
        b = P.createHypersphere(x, y, z, w, 0.55, { restitution = 0.35 })
    else
        b = P.createPolytope(shape, x, y, z, w, 0.9, {
            restitution = 0.15,
            -- tumble in all six rotation planes: xy xz xw yz yw zw
            angularVelocity = { 0.3, -0.2, 0.5, 0.1, -0.4, 0.25 },
        })
    end
    table.insert(bodies, b)
    return b
end

local function buildScene()
    P.destroyAll()
    bodies = {}

    -- floor at y=0 and walls at +-7 on x, z and w
    P.createArena(7.0, 0.5, 0.3)

    -- a stack of tesseracts
    for i = 0, 2 do
        local t = P.createTesseract(0, 0.5 + i * 1.05, 0, 0, 1.0, { restitution = 0.1 })
        table.insert(bodies, t)
    end

    -- a hypersphere on a collision course
    local s = P.createHypersphere(-4, 1.0, 0.2, 0.3, 0.6, { restitution = 0.2 })
    s:setLinearVelocity(6, 1, 0, 0)
    table.insert(bodies, s)

    -- bodies sitting in *other* w-layers: invisible in the w=0 slice until you move it
    spawn("16cell", 3, 2, 1, 2.5)
    spawn("24cell", -2, 2, -2, -2.5)

    -- a spinning static 24-cell: rotating in the xw / yw planes makes its 3D cross-section morph
    spinner = P.createPolytope("24cell", 0, 3.5, -4, 0, 1.2, { type = "static" })
    table.insert(bodies, spinner)
end

function crayon.init()
    -- Bind the module reference now that all Lua bindings have been registered.
    P = crayon.physics4D

    crayon.window.setTitle("19 - 4D Physics (hv4d)")
    crayon.window.setResolution(320, 240)
    buildScene()
end

function crayon.onCollision4DEnter(a, b, nx, ny, nz, nw, impulse)
    hits = hits + 1
    lastHit = string.format("%d vs %d  n=(%.1f %.1f %.1f %.1f)  i=%.1f", a, b, nx, ny, nz, nw, impulse)
end

function crayon.update(dt)
    -- Safety: in case update is ever called before init() for some reason.
    if not P then P = crayon.physics4D end
    if not P then return end

    -- camera orbit
    if crayon.key.isDown("left")  then yaw   = yaw   - 1.6 * dt end
    if crayon.key.isDown("right") then yaw   = yaw   + 1.6 * dt end
    if crayon.key.isDown("up")    then pitch = math.min(1.4, pitch + 1.2 * dt) end
    if crayon.key.isDown("down")  then pitch = math.max(0.05, pitch - 1.2 * dt) end
    if crayon.key.isDown("z") then dist = math.max(4, dist - 8 * dt) end
    if crayon.key.isDown("x") then dist = math.min(30, dist + 8 * dt) end

    -- slice control
    if crayon.key.isDown("q") then sliceW = sliceW - 2.0 * dt end
    if crayon.key.isDown("e") then sliceW = sliceW + 2.0 * dt end
    sliceW = math.max(-7, math.min(7, sliceW))
    P.setSlice(sliceW)

    -- spawning (always lands at the current slice so you can see it)
    for i = 1, 5 do
        if crayon.key.isPressed(tostring(i)) then
            spawnN = spawnN + 1
            spawn(shapes[i], (math.random() - 0.5) * 3, 6, (math.random() - 0.5) * 3, sliceW + (math.random() - 0.5))
        end
    end
    if crayon.key.isPressed("6") then
        spawn("hypersphere", (math.random() - 0.5) * 3, 6, (math.random() - 0.5) * 3, sliceW)
    end

    if crayon.key.isPressed("r") then buildScene() end

    -- toggles
    if crayon.key.isPressed("f") then flags.fill = not flags.fill end
    if crayon.key.isPressed("p") then flags.projection = not flags.projection end
    if crayon.key.isPressed("b") then flags.bounds = not flags.bounds end
    if crayon.key.isPressed("v") then flags.velocities = not flags.velocities end

    if crayon.key.isPressed("space") then
        for _, b in ipairs(bodies) do
            if b:isValid() and not b:isStatic() then
                b:applyImpulse((math.random() - 0.5) * 6, 7, (math.random() - 0.5) * 6, (math.random() - 0.5) * 8)
            end
        end
    end

    -- picking: a camera ray lives in slice space; convert it to a 4D ray and cast it
    if crayon.mouse.isPressed("left") then
        local mx, my = crayon.mouse.getPosition()
        local ray = crayon.graphics.getCameraRay(mx, my)
        if ray then
            local ox, oy, oz, ow = P.sliceToWorld(ray.origin[1], ray.origin[2], ray.origin[3])
            local dx, dy, dz, dw = P.sliceDirectionToWorld(ray.direction[1], ray.direction[2], ray.direction[3])
            local hit, px, py, pz, pw, nx, ny, nz, nw, d, id = P.raycast(ox, oy, oz, ow, dx, dy, dz, dw, 100)
            if hit then
                local body = P.getBody(id)
                if body and not body:isStatic() then
                    -- punch it along the ray, at the hit point (so it spins too)
                    body:applyImpulse(dx * 6, dy * 6, dz * 6, dw * 6, px, py, pz, pw)
                end
            end
        end
    end

    -- the spinner: rotate in xw and yw (impossible in 3D!) plus a little xz
    if spinner and spinner:isValid() then
        spinner:rotateBy(0.0, 0.004, 0.012, 0.0, 0.008, 0.0)
    end
end

function crayon.draw()
    if not P then P = crayon.physics4D end
    if not P then return end

    crayon.graphics.clear(0.05, 0.06, 0.10, 1.0)

    local cx = math.cos(pitch) * math.sin(yaw) * dist
    local cy = math.sin(pitch) * dist
    local cz = math.cos(pitch) * math.cos(yaw) * dist
    crayon.graphics.setCamera3D({
        position = { cx, cy + 1.5, cz },
        target   = { 0, 1.5, 0 },
        up       = { 0, 1, 0 },
        fov      = 50.0,
    })
    crayon.graphics.setLight(-0.4, -1.0, -0.6, 0.95, 0.95, 0.9, 0.35, 0.35, 0.4)

    -- bodies, half-space grids, contacts ... all from the 4D world's current slice
    P.drawDebug({
        shapes = flags.shapes, fill = flags.fill, projection = flags.projection,
        contacts = flags.contacts, bounds = flags.bounds, velocities = flags.velocities,
        projectionRange = 3.0,
    })

    -- HUD
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("4D Physics  (hv4d)", 16, 14, { scale = 1.4 })
    crayon.graphics.setColor(0.7, 0.85, 1.0, 1)
    crayon.graphics.drawText(string.format("slice w = %+.2f      bodies: %d      hits: %d", sliceW, P.getBodyCount(), hits), 16, 44)
    crayon.graphics.drawText("last: " .. lastHit, 16, 64)
    crayon.graphics.setColor(0.6, 0.7, 0.8, 1)
    crayon.graphics.drawText("[Q/E] slice  [1-6] spawn  [click] punch  [SPACE] blast  [F/P/B/V] view  [R] reset", 16, 576)
end