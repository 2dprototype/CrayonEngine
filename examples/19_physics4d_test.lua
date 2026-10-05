-- ============================================================================
-- Example 20: 4D Shapes Gallery (hv4d)  —  320x240
--
-- Every 4D primitive the engine can build, lined up along the x axis so you
-- can compare their 3D cross-sections side by side. Bodies are static — this
-- is a viewer, not a physics sim.
--
-- CONTROLS
--   Arrows ........ orbit camera
--   Z / X ......... zoom in / out
--   Q / E ......... slide the slice hyperplane along w
--   SPACE ......... toggle auto-scan of the slice
--   F ............. toggle filled vs. wireframe
--   P ............. toggle projection ghosts (off-slice bodies as faint outlines)
--   1-7 ........... focus each shape
--   8 ............. reset view to the whole gallery
--   R ............. rebuild the scene
-- ============================================================================


-- Forward declarations — crayon.physics4D etc. don't exist until after the
-- config phase has run, so we bind them in crayon.init().
local P, G, K


-- ---------------------------------------------------------------------------
-- What we're showing, in x-slot order.
-- ---------------------------------------------------------------------------
local SHAPES = {
    { name = "5cell",       scale = 1.30 },
    { name = "8cell",       scale = 1.30 },
    { name = "16cell",      scale = 1.30 },
    { name = "24cell",      scale = 1.30 },
    { name = "120cell",     scale = 2.40 },
    { name = "600cell",     scale = 2.40 },
    { name = "hypersphere", scale = 0.90 },
}


-- Tighter slot spacing for the low-res window.
local SLOT_SPACING = 3.6


-- ---------------------------------------------------------------------------
-- Viewer state
-- ---------------------------------------------------------------------------
local yaw, pitch, dist = 0.55, 0.32, 22.0
local sliceW           = 0.0
local autoScan         = true
local scanDir          = 1.0

local showFill         = true
local showProjection   = false


-- Reused tables — no per-frame allocation.
local camTbl = {
    position = { 0, 0, 0 },
    target   = { 0, 1.8, 0 },
    up       = { 0, 1, 0 },
    fov      = 60,
}

local dbgTbl = {
    shapes = true,
    planes = true,
    fill = true,
    projection = false,
    contacts = false,
    bounds = false,
    velocities = false,
    projectionRange = 1.6,
}


-- ---------------------------------------------------------------------------
-- Scene build
-- ---------------------------------------------------------------------------
local function buildScene()
    P.destroyAll()

    -- Compact arena for the smaller view.
    P.createArena(5.0, 0.35, 0.15)

    -- Lay each shape out at its own x-slot, raised above the floor, at w = 0.
    for i = 1, #SHAPES do
        local s = SHAPES[i]
        local x = (i - 1 - (#SHAPES - 1) * 0.5) * SLOT_SPACING
        local y = 1.8
        local z = 0.0
        local w = 0.0

        if s.name == "hypersphere" then
            P.createHypersphere(x, y, z, w, s.scale, { type = "static" })
        else
            P.createPolytope(s.name, x, y, z, w, s.scale, { type = "static" })
        end
    end
end


-- ---------------------------------------------------------------------------
-- Lifecycle
-- ---------------------------------------------------------------------------
function crayon.init()
    P = crayon.physics4D
    G = crayon.graphics
    K = crayon.key

    crayon.window.setTitle("20 - 4D Shapes Gallery")
    crayon.window.setResolution(320, 240)
    crayon.window.setMinSize(320, 240)

    buildScene()
end


-- ---------------------------------------------------------------------------
-- Update
-- ---------------------------------------------------------------------------
function crayon.update(dt)
    -- Camera orbit
    if K.isDown("left")  then yaw   = yaw   - 1.5 * dt end
    if K.isDown("right") then yaw   = yaw   + 1.5 * dt end
    if K.isDown("up")    then pitch = pitch + 1.0 * dt
                              if pitch > 1.45 then pitch = 1.45 end end
    if K.isDown("down")  then pitch = pitch - 1.0 * dt
                              if pitch < 0.05 then pitch = 0.05 end end
    if K.isDown("z")     then dist  = dist  - 12.0 * dt
                              if dist < 5.0 then dist = 5.0 end end
    if K.isDown("x")     then dist  = dist  + 12.0 * dt
                              if dist > 70.0 then dist = 70.0 end end

    -- Slice control
    if K.isPressed("space") then autoScan = not autoScan end

    if autoScan then
        sliceW = sliceW + scanDir * 2.5 * dt
        if sliceW >  4.0 then sliceW =  4.0; scanDir = -scanDir end
        if sliceW < -4.0 then sliceW = -4.0; scanDir = -scanDir end
    else
        if K.isDown("q") then sliceW = sliceW - 4.0 * dt
                              if sliceW < -4.0 then sliceW = -4.0 end end
        if K.isDown("e") then sliceW = sliceW + 4.0 * dt
                              if sliceW >  4.0 then sliceW =  4.0 end end
    end
    P.setSlice(sliceW)

    -- Toggles
    if K.isPressed("f") then
        showFill = not showFill
        dbgTbl.fill = showFill
    end
    if K.isPressed("p") then
        showProjection = not showProjection
        dbgTbl.projection = showProjection
    end
    if K.isPressed("r") then
        buildScene()
        sliceW = 0.0
    end

    -- 1-7: focus each shape head-on. 8: reset to the full gallery.
    for i = 1, #SHAPES do
        if K.isPressed(tostring(i)) then
            sliceW = 0.0
            autoScan = false
            local x = (i - 1 - (#SHAPES - 1) * 0.5) * SLOT_SPACING
            yaw   = 0.0
            pitch = 0.22
            dist  = 7.0
            camTbl.target[1] = x
            camTbl.target[2] = 1.8
            camTbl.target[3] = 0.0
        end
    end
    if K.isPressed("8") then
        sliceW = 0.0
        autoScan = false
        camTbl.target[1] = 0.0
        camTbl.target[2] = 1.8
        camTbl.target[3] = 0.0
        yaw, pitch, dist = 0.55, 0.32, 22.0
    end
end


-- ---------------------------------------------------------------------------
-- Draw
-- ---------------------------------------------------------------------------
function crayon.draw()
    G.clear(0.04, 0.05, 0.09, 1.0)

    -- Orbit camera around the current focus target (camTbl.target).
    local cp  = math.cos(pitch)
    local sp  = math.sin(pitch)
    local cy_ = math.cos(yaw)
    local sy_ = math.sin(yaw)

    camTbl.position[1] = camTbl.target[1] + cp * sy_ * dist
    camTbl.position[2] = camTbl.target[2] + sp * dist
    camTbl.position[3] = camTbl.target[3] + cp * cy_ * dist

    G.setCamera3D(camTbl)
    G.setLight(-0.4, -1.0, -0.6, 0.95, 0.95, 0.9, 0.35, 0.35, 0.4)

    -- Render the 4D world's 3D slice.
    P.drawDebug(dbgTbl)

    -- HUD (compact, sized for 320x240)
    G.setColor(1, 1, 1, 1)
    G.drawText("4D Shape Gallery", 6, 6, { scale = 1.0 })

    G.setColor(0.75, 0.88, 1.0, 1)
    G.drawText(string.format("w %+.2f  fill %s  proj %s",
        sliceW,
        showFill and "on" or "off",
        showProjection and "on" or "off"), 6, 20, { scale = 0.75 })

    G.setColor(0.6, 0.72, 0.86, 1)
    G.drawText("1-7 focus  8 reset  SPACE scan", 6, 32, { scale = 0.75 })

    G.setColor(0.5, 0.6, 0.72, 1)
    G.drawText("arrows orbit  Z/X zoom  Q/E slice  F fill  P proj  R rebuild",
        6, 224, { scale = 0.7 })
end