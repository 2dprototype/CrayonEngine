-- ============================================================================
-- Example 17: 3D Cosserat Rod Constraints & Plant Simulation
-- Demonstrates:
-- - Cosserat rod constraints (stretch-shear & bend-twist)
-- - Bishop frame orientation tracking along a continuous deformable spine
-- - Orienting procedural leaves/geometry attached along the rod
-- - Dynamic wind breeze causing procedural swaying and twisting
-- - Batched physics debug visualization showing rod segments and frame axes
-- ============================================================================

local cam = {
    x = 0.0, y = 4.0, z = 10.0,
    yaw = -90.0, pitch = -12.0,
    fov = 60.0
}

local rod = nil
local floor_body = nil
local num_segments = 14
local rod_length = 5.0
local wind_active = true
local wind_time = 0.0
local show_debug = true

local function reset_scene()
    crayon.physics3d.destroyAll()

    -- Ground plane
    floor_body = crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 50.0)

    -- Static flower pot / base
    -- createCylinder(x, y, z, radius, halfHeight, tex, rx, ry, rz) is graphics-side;
    -- physics side is createCylinder(x, y, z, halfHeight, radius, ...)
    crayon.physics3d.createCylinder(0, 0.4, 0, 0.4, 0.8, "static", 0.6, 0.2)

    -- Build rod points along a vertical line, anchored at (0, 0.8, 0)
    -- createSoftBodyRod expects a table with:
    --   points            : array of {x, y, z}
    --   stretchCompliance : float
    --   bendTwistCompliance : float
    --   pinRoot           : bool
    local points = {}
    local segs = num_segments
    local total_len = rod_length
    local step = total_len / segs
    for i = 0, segs do
        points[#points + 1] = { 0.0, 0.8 + i * step, 0.0 }
    end

    rod = crayon.physics3d.createSoftBodyRod({
        points = points,
        stretchCompliance = 0.0001,
        bendTwistCompliance = 0.005,
        pinRoot = true,
    })
end

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("Crayon Engine - 3D Cosserat Rod Plant Simulation")
    crayon.graphics.setLight(-0.5, -1.0, -0.6, 1.0, 0.95, 0.9, 0.3, 0.3, 0.35)
    reset_scene()
end

function crayon.update(dt)
    -- Clamp dt so long frames don't destabilise the rod solver
    if dt > 1.0 / 30.0 then dt = 1.0 / 30.0 end

    -- Camera controls
    local move_speed = 7.0 * dt
    local rot_speed = 60.0 * dt

    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local fwd_x = math.cos(rad_pitch) * math.sin(rad_yaw)
    local fwd_y = math.sin(rad_pitch)
    local fwd_z = -math.cos(rad_pitch) * math.cos(rad_yaw)
    local rgt_x = math.cos(rad_yaw)
    local rgt_z = math.sin(rad_yaw)

    if crayon.input.isDown("w") then
        cam.x = cam.x + fwd_x * move_speed
        cam.y = cam.y + fwd_y * move_speed
        cam.z = cam.z + fwd_z * move_speed
    end
    if crayon.input.isDown("s") then
        cam.x = cam.x - fwd_x * move_speed
        cam.y = cam.y - fwd_y * move_speed
        cam.z = cam.z - fwd_z * move_speed
    end
    if crayon.input.isDown("a") then
        cam.x = cam.x - rgt_x * move_speed
        cam.z = cam.z - rgt_z * move_speed
    end
    if crayon.input.isDown("d") then
        cam.x = cam.x + rgt_x * move_speed
        cam.z = cam.z + rgt_z * move_speed
    end
    if crayon.input.isDown("space") then cam.y = cam.y + move_speed end
    if crayon.input.isDown("lshift") then cam.y = cam.y - move_speed end

    if crayon.input.isDown("left")  then cam.yaw   = cam.yaw   - rot_speed end
    if crayon.input.isDown("right") then cam.yaw   = cam.yaw   + rot_speed end
    if crayon.input.isDown("up")    then cam.pitch = math.min(85.0, cam.pitch + rot_speed) end
    if crayon.input.isDown("down")  then cam.pitch = math.max(-85.0, cam.pitch - rot_speed) end

    -- Toggle wind breeze
    if crayon.input.isPressed("g") then
        wind_active = not wind_active
    end

    -- Toggle debug lines
    if crayon.input.isPressed("tab") then
        show_debug = not show_debug
    end

    -- Reset scene
    if crayon.input.isPressed("r") then
        reset_scene()
    end

    -- Flick plant tip: soft bodies expose applyImpulse(i, x, y, z) with 1-based index
    if crayon.input.isPressed("f") and rod and rod:isValid() then
        local vc = rod:getVertexCount()
        rod:applyImpulse(vc, 2.0, 0.0, 1.0)   -- vc == last vertex (1-based)
    end

    -- Dynamic wind breeze applying sway along the rod
    if wind_active and rod and rod:isValid() then
        wind_time = wind_time + dt
        local sway_x = math.sin(wind_time * 2.2) * 1.5 + math.sin(wind_time * 4.7) * 0.5
        local sway_z = math.cos(wind_time * 1.8) * 1.2
        local vc = rod:getVertexCount()
        -- Skip vertex 1 (the pinned root), apply to the rest
        for i = 2, vc do
            local height_frac = (i - 1) / (vc - 1)
            local force_scale = height_frac * height_frac * 0.6 * dt
            rod:applyImpulse(i, sway_x * force_scale, 0.0, sway_z * force_scale)
        end
    end

    if crayon.input.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.1, 0.12)

    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local target_x = cam.x + math.cos(rad_pitch) * math.sin(rad_yaw)
    local target_y = cam.y + math.sin(rad_pitch)
    local target_z = cam.z - math.cos(rad_pitch) * math.cos(rad_yaw)

    crayon.graphics.setCamera3d({
        position = {cam.x, cam.y, cam.z},
        target = {target_x, target_y, target_z},
        up = {0, 1, 0},
        fov = cam.fov
    })

    -- Ground grid
    for i = -12, 12, 2 do
        crayon.graphics.drawLine3d(i, 0.0, -12, i, 0.0, 12, 0.2, 0.25, 0.25, 1.0)
        crayon.graphics.drawLine3d(-12, 0.0, i, 12, 0.0, i, 0.2, 0.25, 0.25, 1.0)
    end

    -- Pot cylinder (graphics: x,y,z, radius, height, tex, rx,ry,rz)
    crayon.graphics.setColor(0.65, 0.35, 0.2, 1.0)
    crayon.graphics.drawCylinder(0, 0.4, 0, 0.8, 0.8)

    -- Procedural stem visualization from rod vertex positions
    if rod and rod:isValid() then
        local vc = rod:getVertexCount()
        if vc >= 2 then
            -- getVertex(i) returns posX, posY, posZ, velX, velY, velZ, invMass — 1-based
            local px, py, pz = rod:getVertex(1)
            for i = 2, vc do
                local nx, ny, nz = rod:getVertex(i)
                if px and nx then
                    -- Stem segment
                    crayon.graphics.drawLine3d(px, py, pz, nx, ny, nz, 0.2, 0.75, 0.3, 1.0)

                    -- Node sphere
                    crayon.graphics.setColor(0.15, 0.85, 0.35, 1.0)
                    crayon.graphics.drawSphere(nx, ny, nz, 0.06)

                    -- Leaves on alternating nodes
                    if i % 2 == 0 and i < vc then
                        local tx, ty, tz = nx - px, ny - py, nz - pz
                        local len = math.sqrt(tx * tx + ty * ty + tz * tz)
                        if len > 0.0001 then
                            tx, ty, tz = tx / len, ty / len, tz / len
                            local sign = (i % 4 == 0) and 1.0 or -1.0
                            local lx = -tz * sign * 0.7
                            local ly = 0.2
                            local lz = tx * sign * 0.7

                            crayon.graphics.setColor(0.3, 0.9, 0.4, 1.0)
                            crayon.graphics.drawLine3d(nx, ny, nz, nx + lx, ny + ly, nz + lz, 0.3, 0.9, 0.4, 1.0)
                            crayon.graphics.drawSphere(nx + lx, ny + ly, nz + lz, 0.10)
                        end
                    end
                end
                px, py, pz = nx, ny, nz
            end

            -- Flower at tip
            crayon.graphics.setColor(1.0, 0.3, 0.5, 1.0)
            crayon.graphics.drawSphere(px, py, pz, 0.18)
        end
    end

    -- Batched physics debug visualization (rod stretch-shear + Bishop frames)
    if show_debug then
        crayon.physics3d.drawDebug({
            shapes              = true,
            softBodies          = true,
            softBodyConstraints = true,
            softBodyRods        = true,
            constraints         = true,
        })
    end

    -- HUD
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("3D COSSERAT ROD PLANT", 8, 8, 1.0)
    crayon.graphics.setColor(0.85, 0.9, 1.0, 1.0)
    crayon.graphics.drawText("G: Wind " .. (wind_active and "ON" or "OFF")
                          .. "  F: Flick  TAB: Debug " .. (show_debug and "ON" or "OFF"), 8, 22, 1.0)
    crayon.graphics.drawText("WASD+Space/Shift: Fly  Arrows: Look  R: Reset", 8, 34, 1.0)

    -- Tip elevation readout
    if rod and rod:isValid() then
        local vc = rod:getVertexCount()
        local tx, ty, tz = rod:getVertex(vc)
        if ty then
            crayon.graphics.setColor(0.5, 1.0, 0.6, 1.0)
            crayon.graphics.drawText(string.format("Tip height: %.2f m", ty), 8, 48, 1.0)
        end
    end
end