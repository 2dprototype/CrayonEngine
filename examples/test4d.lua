-- ============================================================================
-- Example 21: 4D Wireframe Sandbox (hv4d)  —  320x240
--
-- A handful of 4D bodies drop into a 4D arena and bounce around. Everything
-- is drawn as pure wireframe (no fills, no projection ghosts) so you see the
-- raw slice geometry of each shape.
--
-- CONTROLS
--   Arrows ...... orbit camera
--   Z / X ....... zoom
--   Q / E ....... slide the slice along w
--   SPACE ....... blast everything upward
--   R ........... reset the scene
-- ============================================================================


-- Forward declarations: crayon.physics4D doesn't exist until after the
-- config phase, so we bind it in crayon.init().
local P, G, K


-- ---------------------------------------------------------------------------
-- Viewer state
-- ---------------------------------------------------------------------------
local yaw, pitch, dist = 0.6, 0.35, 16.0
local sliceW           = 0.0


-- Reused tables — no per-frame allocation.
local camTbl = {
    position = { 0, 0, 0 },
    target   = { 0, 1.5, 0 },
    up       = { 0, 1, 0 },
    fov      = 60,
}

-- Pure wireframe: fill off, no projection ghosts, no debug extras.
local dbgTbl = {
    shapes          = true,
    fill            = false,
    planes          = true,
    projection      = false,
    contacts        = false,
    bounds          = false,
    velocities      = false,
    projectionRange = 0,
}


-- ---------------------------------------------------------------------------
-- Scene
-- ---------------------------------------------------------------------------
local bodies = {}

local function buildScene()
    P.destroyAll()
    bodies = {}

    -- Arena: floor at y=0, walls at ±4 on x, z, w.
    P.createArena(4.0, 0.6, 0.5)

    -- A small stack of tesseracts at the center.
    for i = 0, 2 do
        local t = P.createTesseract(0, 0.6 + i * 1.1, 0, 0, 0.9, {
            restitution    = 0.3,
            angularVelocity = { 0.4, -0.3, 0.6, 0.2, -0.5, 0.3 },
        })
        bodies[#bodies + 1] = t
    end

    -- A couple of 16-cells off to the sides.
    local a = P.createPolytope("16cell", -2.4, 2.5, 0.5, 0, 0.8, {
        restitution    = 0.4,
        angularVelocity = { 0.3, 0.4, -0.2, 0.5, 0.1, -0.3 },
    })
    local b = P.createPolytope("16cell",  2.4, 2.5, -0.5, 0, 0.8, {
        restitution    = 0.4,
        angularVelocity = { -0.3, 0.2, 0.5, -0.1, 0.4, 0.2 },
    })
    bodies[#bodies + 1] = a
    bodies[#bodies + 1] = b

    -- A hypersphere rolling around.
    local s = P.createHypersphere(-3.0, 1.2, 0, 0.4, 0.7, {
        restitution = 0.7,
    })
    s:setLinearVelocity(4, 0.5, 0.5, 0)
    bodies[#bodies + 1] = s
end


-- ---------------------------------------------------------------------------
-- Lifecycle
-- ---------------------------------------------------------------------------
function crayon.init()
    P = crayon.physics4D
    G = crayon.graphics
    K = crayon.key

    crayon.window.setTitle("21 - 4D Wireframe")
    crayon.window.setResolution(320, 240)
    crayon.window.setMinSize(320, 240)

    buildScene()
end


-- ---------------------------------------------------------------------------
-- Update
-- ---------------------------------------------------------------------------
function crayon.update(dt)
    -- Camera
    if K.isDown("left")  then yaw   = yaw   - 1.5 * dt end
    if K.isDown("right") then yaw   = yaw   + 1.5 * dt end
    if K.isDown("up")    then pitch = pitch + 1.0 * dt
                              if pitch > 1.45 then pitch = 1.45 end end
    if K.isDown("down")  then pitch = pitch - 1.0 * dt
                              if pitch < 0.05 then pitch = 0.05 end end
    if K.isDown("z")     then dist  = dist  - 8.0 * dt
                              if dist < 4.0 then dist = 4.0 end end
    if K.isDown("x")     then dist  = dist  + 8.0 * dt
                              if dist > 40.0 then dist = 40.0 end end

    -- Slice
    if K.isDown("q") then sliceW = sliceW - 3.0 * dt
                          if sliceW < -4.0 then sliceW = -4.0 end end
    if K.isDown("e") then sliceW = sliceW + 3.0 * dt
                          if sliceW >  4.0 then sliceW =  4.0 end end
    P.setSlice(sliceW)

    -- Blast everything upward
    if K.isPressed("space") then
        for i = 1, #bodies do
            local b = bodies[i]
            if b:isValid() and not b:isStatic() then
                b:applyImpulse(
                    (math.random() - 0.5) * 4,
                    8,
                    (math.random() - 0.5) * 4,
                    (math.random() - 0.5) * 4)
            end
        end
    end

    if K.isPressed("r") then
        buildScene()
        sliceW = 0.0
    end
end


-- ---------------------------------------------------------------------------
-- Draw
-- ---------------------------------------------------------------------------
function crayon.draw()
    G.clear(0.04, 0.05, 0.09, 1.0)

    -- Orbit camera
    local cp  = math.cos(pitch)
    local sp  = math.sin(pitch)
    local cy_ = math.cos(yaw)
    local sy_ = math.sin(yaw)

    camTbl.position[1] = cp * sy_ * dist
    camTbl.position[2] = sp * dist + 1.5
    camTbl.position[3] = cp * cy_ * dist

    G.setCamera3D(camTbl)
    G.setLight(-0.4, -1.0, -0.6, 0.95, 0.95, 0.9, 0.35, 0.35, 0.4)

    -- Wireframe-only render of the 4D slice.
    P.drawDebug(dbgTbl)

    -- HUD
    G.setColor(1, 1, 1, 1)
    G.drawText("4D Wireframe", 6, 6, { scale = 1.0 })

    G.setColor(0.75, 0.88, 1.0, 1)
    G.drawText(string.format("w %+.2f   bodies %d", sliceW, P.getBodyCount()),
        6, 20, { scale = 0.75 })

    G.setColor(0.5, 0.6, 0.72, 1)
    G.drawText("arrows orbit  Z/X zoom  Q/E slice",  6, 216, { scale = 0.7 })
    G.drawText("SPACE blast   R reset",              6, 226, { scale = 0.7 })
end