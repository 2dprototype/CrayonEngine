-- Crayon Engine: Box2D 2D Physics System Test (crayon.physics2d)
-- Demonstrates static & dynamic bodies, circles, boxes, polygons, sensors, joints, raycasting, and debug drawing

function crayon.config(config)
    config.window.title = "Crayon Engine - Box2D Physics2D Test"
    config.window.width = 320
    config.window.height = 240
    config.window.virtualWidth = 320
    config.window.virtualHeight = 240
    config.modules.physics3d = false    -- 3D Jolt disabled for 2D optimization
    config.modules.physics2d = true   -- 2D Box2D enabled
    config.modules.mesh3D = false     -- NOTE: key is mesh3D (capital D), not mesh3d
end

local spawnedBodies = {}
local sensorTriggered = false
local lastCollisionInfo = "None"
local rayHitInfo = nil

function crayon.init()
    -- Set gravity (0, 18) — pixels per second squared in 2D pixel space
    crayon.physics2d.setGravity(0, 18)

    -- 1. Static Floor & Walls
    local floor = crayon.physics2d.createBody("static", 160, 235)
    floor:addBox(310, 10, 0, 0, 0, 0, 0.4, 0.2)

    local leftWall = crayon.physics2d.createBody("static", 5, 120)
    leftWall:addBox(10, 240, 0, 0, 0, 0, 0.2, 0.3)

    local rightWall = crayon.physics2d.createBody("static", 315, 120)
    rightWall:addBox(10, 240, 0, 0, 0, 0, 0.2, 0.3)

    -- 2. Angled Ramps
    local rampLeft = crayon.physics2d.createBody("static", 60, 130)
    rampLeft:addBox(90, 8, 0, 0, 0.35, 0, 0.2, 0.4)

    local rampRight = crayon.physics2d.createBody("static", 260, 160)
    rampRight:addBox(90, 8, 0, 0, -0.3, 0, 0.2, 0.4)

    -- 3. Trigger / Sensor Area (Yellow box)
    local sensor = crayon.physics2d.createBody("static", 160, 190)
    sensor:addBox(40, 30, 0, 0, 0, 1.0, 0.0, 0.0, true) -- isSensor = true

    -- 4. Revolute Joint Pendulum
    local pin = crayon.physics2d.createBody("static", 160, 60)
    pin:addCircle(4, 0, 0)

    local bob = crayon.physics2d.createBody("dynamic", 200, 60, { linearDamping = 0.05 })
    bob:addCircle(10, 0, 0, 2.0, 0.3, 0.8) -- Bouncy bob
    table.insert(spawnedBodies, bob)

    crayon.physics2d.createDistanceJoint(pin, bob, 160, 60, 200, 60, 40, 0.0, 0.0)

    -- 5. Spawn initial tumbling shapes
    for i = 1, 6 do
        local b = crayon.physics2d.createBody("dynamic", 40 + i * 20, 30 + (i % 2) * 15)
        if i % 2 == 0 then
            b:addBox(14, 14, 0, 0, 0.2, 1.0, 0.3, 0.4)
        else
            b:addCircle(7, 0, 0, 1.2, 0.2, 0.6)
        end
        table.insert(spawnedBodies, b)
    end
end

function crayon.update(dt)
    -- Left click to spawn dynamic body at mouse position.
    -- NOTE: isMouseButtonPressed does not exist; use isMousePressed (unified table)
    --       or crayon.mouse.isPressed (mouse table).
    if crayon.input.isMousePressed(1) then
        local mx, my = crayon.input.getMousePosition()
        local b = crayon.physics2d.createBody("dynamic", mx, my)
        if math.random() > 0.5 then
            b:addBox(12, 12, 0, 0, math.random() * 3.14, 1.5, 0.3, 0.5)
        else
            b:addCircle(6 + math.random(0, 4), 0, 0, 1.5, 0.2, 0.7)
        end
        -- Give a slight random impulse
        b:applyImpulse(math.random(-5, 5), math.random(-8, -2))
        table.insert(spawnedBodies, b)
    end

    -- Space to launch an upward explosion impulse on all dynamic bodies
    if crayon.input.isKeyPressed("space") then
        for _, b in ipairs(spawnedBodies) do
            if b:isValid() then
                b:applyImpulse(math.random(-15, 15), -35)
            end
        end
    end

    -- Raycast demo from mouse position down to floor
    local mx, my = crayon.input.getMousePosition()
    local hit, hx, hy, nx, ny, frac, bodyId = crayon.physics2d.raycast(mx, my, mx, 235)
    if hit then
        rayHitInfo = { x1 = mx, y1 = my, x2 = hx, y2 = hy, nx = nx, ny = ny, bodyId = bodyId }
    else
        rayHitInfo = nil
    end

    -- Clear off-screen or excessive bodies
    if #spawnedBodies > 40 then
        local old = table.remove(spawnedBodies, 1)
        if old and old:isValid() then
            old:destroy()
        end
    end
end

-- Collision & Sensor Event Callbacks (2D)
function crayon.collision2dEnter(bodyA, bodyB, nx, ny, impulse)
    lastCollisionInfo = "Hit: #" .. bodyA .. " <-> #" .. bodyB .. string.format(" (Impulse: %.1f)", impulse)
end

function crayon.collision2dExit(bodyA, bodyB)
    -- collision ended
end

function crayon.trigger2dEnter(sensorBody, otherBody)
    sensorTriggered = true
end

function crayon.trigger2dExit(sensorBody, otherBody)
    sensorTriggered = false
end

function crayon.draw()
    crayon.graphics.clear(0.07, 0.08, 0.11, 1.0)

    -- Draw wireframe Box2D physics bodies & joints
    crayon.physics2d.drawDebug()

    -- Raycast visualization
    if rayHitInfo then
        -- Ray line (red)
        crayon.graphics.setColor(1.0, 0.2, 0.2, 0.8)
        crayon.graphics.drawLine(rayHitInfo.x1, rayHitInfo.y1, rayHitInfo.x2, rayHitInfo.y2)

        -- Hit point (yellow circle)
        crayon.graphics.setColor(1.0, 0.9, 0.1, 1.0)
        -- FIX: drawCircle takes mode string first, then cx, cy, r [, segs]
        crayon.graphics.drawCircle("fill", rayHitInfo.x2, rayHitInfo.y2, 3)

        -- Normal vector (green)
        crayon.graphics.setColor(0.2, 1.0, 0.3, 1.0)
        crayon.graphics.drawLine(
            rayHitInfo.x2, rayHitInfo.y2,
            rayHitInfo.x2 + rayHitInfo.nx * 10,
            rayHitInfo.y2 + rayHitInfo.ny * 10
        )
    end

    -- HUD Overlay
    crayon.graphics.setColor(0.0, 0.0, 0.0, 0.7)
    -- FIX: drawRect takes mode string first, then x, y, w, h
    crayon.graphics.drawRect("fill", 4, 4, 312, 42)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Box2D 2D Physics - Click: Spawn | SPACE: Blast", 8, 8, 0.9)

    crayon.graphics.setColor(0.8, 0.8, 0.8, 0.9)
    crayon.graphics.drawText(lastCollisionInfo, 8, 20, 0.8)

    if sensorTriggered then
        crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
        crayon.graphics.drawText("[SENSOR TRIGGERED!]", 8, 31, 0.8)
    else
        crayon.graphics.setColor(0.5, 0.5, 0.5, 0.8)
        crayon.graphics.drawText("Sensor idle", 8, 31, 0.8)
    end
end