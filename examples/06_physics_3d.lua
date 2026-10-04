-- ============================================================================
-- Example 06: Jolt 3D Physics (simplified)
-- Rigid bodies, a static floor, spawn-on-key, blast impulse, collision events.
-- ============================================================================

local floor  = nil
local bodies = {}
local collisions = 0
local lastHit    = "none"

function crayon.init()
    crayon.window.setTitle("06 - 3D Physics")
    crayon.window.setResolution(640, 480)

    crayon.graphics.setCamera3D({
        position = { 0.0, 8.0, 14.0 },
        target   = { 0.0, 1.0, 0.0 },
        up       = { 0.0, 1.0, 0.0 },
        fov      = 45.0,
    })

    -- Static floor: half-extents (15, 0.5, 15) centered at y = -0.5
    floor = crayon.physics3D.createBox(0, -0.5, 0, 15, 0.5, 15, "static", 0.6, 0.2)

    -- A few starter bodies
    spawn( 0.0, 5.0,  0.0)
    spawn( 1.0, 8.0,  0.5)
    spawn(-1.5, 6.0,  0.0)
end

function spawn(x, y, z)
    local b = crayon.physics3D.createBox(x, y, z, 0.5, 0.5, 0.5, "dynamic", 0.5, 0.3, 800.0)
    if b then table.insert(bodies, b) end
end

function crayon.onCollisionEnter(a, b, nx, ny, nz, impulse)
    collisions = collisions + 1
    lastHit = string.format("#%d: %d vs %d  (i=%.1f)", collisions, a, b, impulse)
end

function crayon.update(dt)
    if crayon.key.isPressed("space") then
        spawn((math.random() - 0.5) * 3.0, 8.0, (math.random() - 0.5) * 3.0)
    end

    if crayon.key.isPressed("e") then
        for _, b in ipairs(bodies) do
            if b:isValid() then
                local x, _, z = b:getPosition()
                b:applyImpulse(x * 2.0, 8.0, z * 2.0)
            end
        end
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.10, 0.15, 1.0)

    crayon.graphics.setLight(-0.4, -1.0, -0.6, 0.9, 0.9, 0.85, 0.3, 0.3, 0.35)

    -- Floor
    crayon.graphics.setColor(0.3, 0.35, 0.45, 1.0)
    crayon.graphics.drawPlane(0, 0, 0, 20, 20)

    -- Dynamic boxes
    for _, b in ipairs(bodies) do
        if b:isValid() then
            local x, y, z    = b:getPosition()
            local rx, ry, rz = b:getRotation()
            crayon.graphics.setColor(0.9, 0.35, 0.2, 1.0)
            crayon.graphics.drawCube(x, y, z, 1, 1, 1, nil, rx, ry, rz)
        end
    end

    -- HUD
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("3D Physics", 20, 20, { scale = 1.5 })

    crayon.graphics.setColor(0.75, 0.8, 0.9, 1)
    crayon.graphics.drawText("[SPACE] Spawn   [E] Blast", 20, 50)
    crayon.graphics.drawText("Bodies:     " .. #bodies,        20, 74)
    crayon.graphics.drawText("Collisions: " .. collisions,     20, 94)
    crayon.graphics.drawText("Last:       " .. lastHit,        20, 114)
    crayon.graphics.drawText("FPS: " .. math.floor(crayon.window.getFps()), 560, 20)
end