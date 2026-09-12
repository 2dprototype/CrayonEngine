-- ============================================================================
-- Example 15: 3D Soft Body Cloth Simulation
-- Pinned vertices, wind simulation, LRA tethers, dihedral bend constraints,
-- rigid body collision, cannonball shooting, and batched debug drawing.
-- ============================================================================

local cam = {
    x = 0.0, y = 5.0, z = 12.0,
    yaw = -90.0, pitch = -15.0,
    fov = 60.0
}

local cloth = nil
local floor_body = nil
local balls = {}
local show_debug = true
local wind_active = true
local wind_time = 0.0

local function reset_scene()
    crayon.physics3d.destroyAll()
    balls = {}

    -- Ground plane
    floor_body = crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 50.0)

    -- Create hanging cloth
    -- 20x20 grid, size 6.0 x 6.0, centered at x=0, y=6.0, z=0
    cloth = crayon.physics3d.createSoftBodyCloth({
        x = 0.0, y = 6.0, z = 0.0,
        width = 6.0,
        height = 6.0,
        segmentsX = 20,
        segmentsY = 20,
        mass = 2.0,
        compliance = 0.0001,
        pinnedTop = true,      -- Pins top corners
        hasShear = true,
        hasBend = true,        -- Dihedral bend constraints
        hasLRA = true          -- Long range attachment constraints (tethers)
    })
end

function crayon.init()
    crayon.window.setTitle("Crayon Engine - 3D Soft Body Cloth Simulation")
    crayon.graphics.setLight(-0.5, -1.0, -0.7, 1.0, 0.95, 0.9, 0.25, 0.25, 0.3)
    reset_scene()
end

function crayon.update(dt)
    -- Camera controls
    local move_speed = 8.0 * dt
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
    if crayon.input.isDown("left_shift") then cam.y = cam.y - move_speed end

    if crayon.input.isDown("left")  then cam.yaw = cam.yaw - rot_speed end
    if crayon.input.isDown("right") then cam.yaw = cam.yaw + rot_speed end
    if crayon.input.isDown("up")    then cam.pitch = math.min(85.0, cam.pitch + rot_speed) end
    if crayon.input.isDown("down")  then cam.pitch = math.max(-85.0, cam.pitch - rot_speed) end

    -- Toggle debug lines
    if crayon.input.isPressed("tab") then
        show_debug = not show_debug
    end

    -- Toggle wind
    if crayon.input.isPressed("g") then
        wind_active = not wind_active
    end

    -- Reset scene
    if crayon.input.isPressed("r") then
        reset_scene()
    end

    -- Shoot cannonball on F or Left Click
    if crayon.input.isPressed("f") or crayon.input.isMousePressed(1) then
        local shoot_dir_x = fwd_x
        local shoot_dir_y = fwd_y
        local shoot_dir_z = fwd_z
        local spawn_dist = 1.5
        local bx = cam.x + shoot_dir_x * spawn_dist
        local by = cam.y + shoot_dir_y * spawn_dist
        local bz = cam.z + shoot_dir_z * spawn_dist

        local ball = crayon.physics3d.createSphere(bx, by, bz, 0.4, "dynamic", 0.5, 0.2)
        local speed = 35.0
        ball:setVelocity(shoot_dir_x * speed, shoot_dir_y * speed, shoot_dir_z * speed)
        table.insert(balls, ball)
    end

    -- Apply wind force to cloth vertices
    if wind_active and cloth then
        wind_time = wind_time + dt
        local gust = math.sin(wind_time * 3.0) * 0.5 + 0.5
        local wx = (math.sin(wind_time * 1.5) * 0.3 + 0.7) * 4.0 * gust
        local wz = (math.cos(wind_time * 2.0) * 0.2 - 0.8) * 6.0 * gust
        local vert_count = cloth:getVertexCount()
        for i = 0, vert_count - 1 do
            -- Add subtle wave variation per vertex index
            local fx = wx + math.sin(wind_time * 5.0 + i * 0.1) * 0.8
            local fz = wz + math.cos(wind_time * 4.0 + i * 0.1) * 0.8
            cloth:applyImpulse(i, fx * dt, 0.5 * dt, fz * dt)
        end
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.14)

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

    -- Draw ground grid
    crayon.graphics.setColor(0.2, 0.25, 0.3, 1.0)
    for i = -15, 15, 3 do
        crayon.graphics.drawLine3d(i, 0.0, -15, i, 0.0, 15, 0.2, 0.25, 0.3, 1.0)
        crayon.graphics.drawLine3d(-15, 0.0, i, 15, 0.0, i, 0.2, 0.25, 0.3, 1.0)
    end

    -- Draw rigid cannonballs
    for _, ball in ipairs(balls) do
        local x, y, z = ball:getPosition()
        crayon.graphics.setColor(0.9, 0.3, 0.2, 1.0)
        crayon.graphics.drawSphere(x, y, z, 0.4)
    end

    -- Draw fast batched debug wireframe for soft body and rigid bodies
    if show_debug then
        crayon.physics3d.drawDebug({
            shapes = true,
            softBodies = true,
            softBodyConstraints = true,
            constraints = true
        })
    end

    -- 2D Overlay UI
    crayon.graphics.resetCamera2d()
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("== 3D Soft Body Cloth Simulation ==", 20, 20)
    crayon.graphics.drawText("WASD + Space/Shift : Fly camera | Arrow keys : Look", 20, 40)
    crayon.graphics.drawText("Left Click / [F]    : Shoot cannonball at cloth", 20, 60)
    crayon.graphics.drawText("[G]                 : Toggle Wind Force (Current: " .. (wind_active and "ON" or "OFF") .. ")", 20, 80)
    crayon.graphics.drawText("[TAB]               : Toggle Batched Debug Lines (Current: " .. (show_debug and "ON" or "OFF") .. ")", 20, 100)
    crayon.graphics.drawText("[R]                 : Reset Cloth & Cannonballs", 20, 120)
    crayon.graphics.drawText("Cloth Specs: 20x20 Grid, LRA Tethers, Dihedral Bending, XPBD Solver", 20, 150)
end
