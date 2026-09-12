-- ============================================================================
-- Crayon Engine: Advanced Physics 3D Showcase & Verification Demo
-- Features Tested:
-- 1. Animated Ragdolls (Kinematic Hard Keying, Soft Keying, Motor Driving, Skeleton Mapping)
-- 2. Game Characters (Rigid Body Character vs Virtual Character)
-- 3. Vehicles (4-Wheeled Car, Dual-Track Tank, Leaning Motorcycle)
-- 4. New Graphics Helpers (draw_capsule_wires, draw_cylinder_wires, draw_skeleton, draw_ray_3d)
-- ============================================================================

local cam = {
    x = 0.0, y = 8.0, z = 18.0,
    yaw = -90.0, pitch = -20.0,
    fov = 60.0
}

local current_mode = 1 -- 1: Characters, 2: Vehicles, 3: Ragdolls
local mode_names = { "CHARACTERS (Rigid vs Virtual)", "VEHICLES (Car, Tank, Bike)", "ANIMATED RAGDOLL" }

-- Resources
local char_rigid = nil
local char_virtual = nil
local car_vehicle = nil
local tank_vehicle = nil
local bike_vehicle = nil
local ragdoll = nil
local skel_low = nil
local skel_high = nil
local pose_low = nil
local pose_high_local = nil
local pose_high_model = nil
local mapper = nil

local anim_time = 0.0

local function cleanup_all()
    crayon.physics3d.destroyAll()
    char_rigid = nil
    char_virtual = nil
    car_vehicle = nil
    tank_vehicle = nil
    bike_vehicle = nil
    ragdoll = nil
    skel_low = nil
    skel_high = nil
    pose_low = nil
    pose_high_local = nil
    pose_high_model = nil
    mapper = nil
end

-- ============================================================================
-- Scene 1: Characters (Rigid Body Character & Virtual Character)
-- ============================================================================
local function setup_characters()
    cleanup_all()
    crayon.physics3d.setGravity(0, -9.81, 0)

    -- Ground plane & Obstacles/Stairs
    crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 100.0)

    -- Steps / stairs for virtual character step-up testing
    for i = 1, 6 do
        crayon.physics3d.createBox(-5.0, i * 0.2, -5.0 + i * 0.6, 2.0, 0.2, 0.4, "static")
    end

    -- Slope
    for i = 1, 8 do
        crayon.physics3d.createBox(5.0, i * 0.15, -5.0 + i * 0.6, 2.0, 0.15, 0.5, "static")
    end

    -- 1. Rigid body character
    char_rigid = crayon.physics3d.createCharacter({
        pos = {-3.0, 2.0, 0.0},
        radius = 0.4,
        halfHeight = 0.6,
        mass = 80.0,
        friction = 0.2,
        maxSlopeAngleDeg = 45.0
    })

    -- 2. Virtual character
    char_virtual = crayon.physics3d.createCharacterVirtual({
        pos = {3.0, 2.0, 0.0},
        radius = 0.4,
        halfHeight = 0.6,
        mass = 70.0,
        maxSlopeAngleDeg = 45.0,
        maxStrength = 100.0,
        stepHeight = 0.4,
        predictiveContactDistance = 0.1,
        innerBody = true
    })
end

-- ============================================================================
-- Scene 2: Vehicles (Wheeled Car, Tracked Tank, Leaning Motorcycle)
-- ============================================================================
local function setup_vehicles()
    cleanup_all()
    crayon.physics3d.setGravity(0, -9.81, 0)
    crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 100.0)

    -- Ramp
    crayon.physics3d.createBox(0, 0.5, -12, 10.0, 0.5, 4.0, "static")

    -- 1. Wheeled Car
    local chassis_car = crayon.physics3d.createBox(-6.0, 1.5, 0.0, 1.0, 0.4, 2.0, "dynamic", 0.8, 0.2, 1200.0)
    local wheels = {
        { pos = {-1.1, -0.2,  1.3}, radius = 0.35, width = 0.2, isFront = true, isDrive = true },
        { pos = { 1.1, -0.2,  1.3}, radius = 0.35, width = 0.2, isFront = true, isDrive = true },
        { pos = {-1.1, -0.2, -1.3}, radius = 0.35, width = 0.2, isFront = false, isDrive = true },
        { pos = { 1.1, -0.2, -1.3}, radius = 0.35, width = 0.2, isFront = false, isDrive = true }
    }
    car_vehicle = crayon.physics3d.createWheeledVehicle({
        chassis = chassis_car,
        wheels = wheels,
        engineMaxTorque = 500.0,
        engineMinRpm = 1000.0,
        engineMaxRpm = 6500.0
    })

    -- 2. Tracked Vehicle (Tank)
    local chassis_tank = crayon.physics3d.createBox(0.0, 1.5, 0.0, 1.3, 0.5, 2.2, "dynamic", 0.9, 0.1, 3500.0)
    local left_t = {
        { pos = {-1.4, -0.2,  1.5}, radius = 0.4 },
        { pos = {-1.4, -0.2,  0.0}, radius = 0.4 },
        { pos = {-1.4, -0.2, -1.5}, radius = 0.4 }
    }
    local right_t = {
        { pos = { 1.4, -0.2,  1.5}, radius = 0.4 },
        { pos = { 1.4, -0.2,  0.0}, radius = 0.4 },
        { pos = { 1.4, -0.2, -1.5}, radius = 0.4 }
    }
    tank_vehicle = crayon.physics3d.createTrackedVehicle({
        chassis = chassis_tank,
        leftWheels = left_t,
        rightWheels = right_t,
        engineMaxTorque = 1500.0
    })

    -- 3. Motorcycle with Lean Controller
    local chassis_bike = crayon.physics3d.createBox(6.0, 1.5, 0.0, 0.3, 0.4, 1.2, "dynamic", 0.8, 0.2, 250.0)
    bike_vehicle = crayon.physics3d.createMotorcycle({
        chassis = chassis_bike,
        frontWheel = { pos = {0.0, -0.2,  1.0}, radius = 0.32, isFront = true },
        rearWheel  = { pos = {0.0, -0.2, -1.0}, radius = 0.32, isFront = false, isDrive = true },
        maxLeanAngleRad = 0.75,
        leanSpringConstant = 6000.0,
        leanSpringDamping = 1200.0,
        engineMaxTorque = 300.0
    })
    bike_vehicle:enableLeanController(true)
end

-- ============================================================================
-- Scene 3: Ragdoll & Skeleton Mapping
-- ============================================================================
local ragdoll_mode = "motors" -- "kinematic", "soft_keying", "motors", "free"

local function setup_ragdoll()
    cleanup_all()
    crayon.physics3d.setGravity(0, -9.81, 0)
    crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 100.0)

    -- 1. Create Skeletons (Low-detail ragdoll skeleton vs High-detail anim skeleton)
    -- Low: Pelvis(0), Spine(1, parent 0), Head(2, parent 1), LeftArm(3, parent 1), RightArm(4, parent 1)
    skel_low = crayon.physics3d.createSkeleton({
        { name = "Pelvis", parent = -1 },
        { name = "Spine", parent = 0 },
        { name = "Head", parent = 1 },
        { name = "LeftArm", parent = 1 },
        { name = "RightArm", parent = 1 }
    })

    -- High detail animation skeleton (with extra chest and wrist joints)
    skel_high = crayon.physics3d.createSkeleton({
        { name = "Root", parent = -1 },
        { name = "Pelvis", parent = 0 },
        { name = "Spine", parent = 1 },
        { name = "Chest", parent = 2 },
        { name = "Neck", parent = 3 },
        { name = "Head", parent = 4 },
        { name = "LeftClavicle", parent = 3 },
        { name = "LeftArm", parent = 6 },
        { name = "LeftHand", parent = 7 },
        { name = "RightClavicle", parent = 3 },
        { name = "RightArm", parent = 9 },
        { name = "RightHand", parent = 10 }
    })

    -- Create Poses
    pose_low = crayon.physics3d.createSkeletonPose(skel_low)
    pose_high_local = crayon.physics3d.createSkeletonPose(skel_high)
    pose_high_model = crayon.physics3d.createSkeletonPose(skel_high)

    -- Setup neutral low pose
    pose_low:setJoint(0, 0.0, 3.0, 0.0, 0, 0, 0, 1)   -- Pelvis
    pose_low:setJoint(1, 0.0, 0.8, 0.0, 0, 0, 0, 1)   -- Spine
    pose_low:setJoint(2, 0.0, 0.6, 0.0, 0, 0, 0, 1)   -- Head
    pose_low:setJoint(3, -0.8, 0.0, 0.0, 0, 0, 0, 1)  -- LeftArm
    pose_low:setJoint(4,  0.8, 0.0, 0.0, 0, 0, 0, 1)  -- RightArm
    pose_low:calculateMatrices()

    -- 2. Create Ragdoll from low parts
    ragdoll = crayon.physics3d.createRagdoll({
        parts = {
            { name = "Pelvis", parent = -1, pos = {0, 3.0, 0}, shape = "capsule", radius = 0.25, halfHeight = 0.3, mass = 15.0 },
            { name = "Spine",  parent = 0,  pos = {0, 3.8, 0}, shape = "capsule", radius = 0.22, halfHeight = 0.35, mass = 12.0, enableMotors = true },
            { name = "Head",   parent = 1,  pos = {0, 4.4, 0}, shape = "sphere",  radius = 0.25, mass = 5.0, enableMotors = true },
            { name = "LeftArm", parent = 1, pos = {-0.8, 3.8, 0}, shape = "capsule", radius = 0.15, halfHeight = 0.4, mass = 4.0, enableMotors = true },
            { name = "RightArm", parent = 1, pos = {0.8, 3.8, 0}, shape = "capsule", radius = 0.15, halfHeight = 0.4, mass = 4.0, enableMotors = true }
        }
    })

    -- 3. Create Skeleton Mapper between Low and High
    mapper = crayon.physics3d.createSkeletonMapper(skel_low, skel_high, pose_low, pose_high_local)
end

-- ============================================================================
-- Crayon Callbacks
-- ============================================================================

function crayon.init()
    crayon.graphics.set_light(0.5, -0.9, -0.3, 1.2, 1.1, 1.0, 0.2, 0.2, 0.25)
    setup_characters()
end

function crayon.update(dt)
    anim_time = anim_time + dt

    -- Camera Controls
    local cam_speed = 10.0 * dt
    local rad_yaw = math.rad(cam.yaw)
    local fwd_x, fwd_z = math.cos(rad_yaw), math.sin(rad_yaw)
    local right_x, right_z = -fwd_z, fwd_x

    if crayon.input.is_down("w") then cam.x = cam.x + fwd_x * cam_speed; cam.z = cam.z + fwd_z * cam_speed end
    if crayon.input.is_down("s") then cam.x = cam.x - fwd_x * cam_speed; cam.z = cam.z - fwd_z * cam_speed end
    if crayon.input.is_down("a") then cam.x = cam.x - right_x * cam_speed; cam.z = cam.z - right_z * cam_speed end
    if crayon.input.is_down("d") then cam.x = cam.x + right_x * cam_speed; cam.z = cam.z + right_z * cam_speed end
    if crayon.input.is_down("space") then cam.y = cam.y + cam_speed end
    if crayon.input.is_down("lshift") then cam.y = cam.y - cam_speed end

    if crayon.input.is_down("left") then cam.yaw = cam.yaw - 70.0 * dt end
    if crayon.input.is_down("right") then cam.yaw = cam.yaw + 70.0 * dt end
    if crayon.input.is_down("up") then cam.pitch = math.min(cam.pitch + 50.0 * dt, 80.0) end
    if crayon.input.is_down("down") then cam.pitch = math.max(cam.pitch - 50.0 * dt, -80.0) end

    -- Mode Switch
    if crayon.input.is_pressed("1") and current_mode ~= 1 then
        current_mode = 1
        setup_characters()
    elseif crayon.input.is_pressed("2") and current_mode ~= 2 then
        current_mode = 2
        setup_vehicles()
    elseif crayon.input.is_pressed("3") and current_mode ~= 3 then
        current_mode = 3
        setup_ragdoll()
    end

    -- ==================== Scene 1: Characters Update ====================
    if current_mode == 1 then
        -- Rigid character motion
        if char_rigid and char_rigid:isValid() then
            local rx, ry, rz = 0, 0, 0
            if crayon.input.is_down("i") then rz = -4.0 end
            if crayon.input.is_down("k") then rz =  4.0 end
            if crayon.input.is_down("j") then rx = -4.0 end
            if crayon.input.is_down("l") then rx =  4.0 end
            if crayon.input.is_pressed("u") and char_rigid:isSupported() then ry = 6.0 end
            local cur_vx, cur_vy, cur_vz = char_rigid:getLinearVelocity()
            char_rigid:setLinearVelocity(rx, (ry > 0 and ry) or cur_vy, rz)
        end

        -- Virtual character motion (updated outside physics update)
        if char_virtual and char_virtual:isValid() then
            local vx, vy, vz = 0, 0, 0
            if crayon.input.is_down("t") then vz = -4.0 end
            if crayon.input.is_down("g") then vz =  4.0 end
            if crayon.input.is_down("f") then vx = -4.0 end
            if crayon.input.is_down("h") then vx =  4.0 end
            if crayon.input.is_pressed("y") and char_virtual:isSupported() then vy = 6.0 end
            local cur_vx, cur_vy, cur_vz = char_virtual:getLinearVelocity()
            char_virtual:setLinearVelocity(vx, (vy > 0 and vy) or cur_vy, vz)
            char_virtual:update(dt)
        end
    end

    -- ==================== Scene 2: Vehicles Update ====================
    if current_mode == 2 then
        -- 4-Wheeled Car Input
        if car_vehicle and car_vehicle:isValid() then
            local throttle = (crayon.input.is_down("i") and 1.0) or (crayon.input.is_down("k") and -0.8) or 0.0
            local steer = (crayon.input.is_down("j") and -0.5) or (crayon.input.is_down("l") and 0.5) or 0.0
            local brake = crayon.input.is_down("m") and 1.0 or 0.0
            local handbrake = crayon.input.is_down("n")
            car_vehicle:setInputWheeled(throttle, steer, brake, handbrake)
        end

        -- Tracked Vehicle (Tank) Input
        if tank_vehicle and tank_vehicle:isValid() then
            local left = 0.0
            local right = 0.0
            if crayon.input.is_down("t") then left = left + 1.0; right = right + 1.0 end
            if crayon.input.is_down("g") then left = left - 0.8; right = right - 0.8 end
            if crayon.input.is_down("f") then left = left - 0.6; right = right + 0.6 end
            if crayon.input.is_down("h") then left = left + 0.6; right = right - 0.6 end
            tank_vehicle:setInputTracked(left, right, 0.0)
        end

        -- Motorcycle Input
        if bike_vehicle and bike_vehicle:isValid() then
            local forward = (crayon.input.is_down("up") and 1.0) or (crayon.input.is_down("down") and -0.5) or 0.0
            local steer = (crayon.input.is_down("left") and -0.4) or (crayon.input.is_down("right") and 0.4) or 0.0
            bike_vehicle:setInputMotorcycle(forward, steer, 0.0)
        end
    end

    -- ==================== Scene 3: Ragdoll Update ====================
    if current_mode == 3 and ragdoll and ragdoll:isValid() then
        -- Cycle keying mode: 4: Kinematic Hard, 5: Soft Keying, 6: Motor Driving, 7: Free Fall
        if crayon.input.is_pressed("4") then
            ragdoll_mode = "kinematic"
            ragdoll:setHardKeying(true)
        elseif crayon.input.is_pressed("5") then
            ragdoll_mode = "soft_keying"
            ragdoll:setHardKeying(false)
        elseif crayon.input.is_pressed("6") then
            ragdoll_mode = "motors"
            ragdoll:setHardKeying(false)
        elseif crayon.input.is_pressed("7") then
            ragdoll_mode = "free"
            ragdoll:setHardKeying(false)
            ragdoll:activate()
        end

        -- Generate dynamic sinusoidal wave animation on pose
        local arm_swing = math.sin(anim_time * 4.0) * 0.5
        local spine_tilt = math.cos(anim_time * 2.0) * 0.2

        pose_low:setJoint(0, 0.0, 3.0 + math.sin(anim_time * 2.0) * 0.3, 0.0, 0, 0, 0, 1)
        pose_low:setJoint(1, 0.0, 0.8, 0.0, spine_tilt, 0, 0, 1)
        pose_low:setJoint(2, 0.0, 0.6, 0.0, 0, 0, 0, 1)
        pose_low:setJoint(3, -0.8, 0.0, 0.0, arm_swing, 0, 0, 1)
        pose_low:setJoint(4,  0.8, 0.0, 0.0, -arm_swing, 0, 0, 1)
        pose_low:calculateMatrices()

        if ragdoll_mode == "kinematic" then
            ragdoll:setPose(pose_low)
        elseif ragdoll_mode == "soft_keying" then
            ragdoll:driveToPoseKinematics(pose_low, dt)
        elseif ragdoll_mode == "motors" then
            ragdoll:driveToPoseMotors(pose_low)
        end

        -- Map low pose to high pose
        if mapper and mapper:isValid() then
            mapper:map(pose_low, pose_high_local, pose_high_model)
        end
    end
end

function crayon.draw()
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local dir_x = math.cos(rad_pitch) * math.cos(rad_yaw)
    local dir_y = math.sin(rad_pitch)
    local dir_z = math.cos(rad_pitch) * math.sin(rad_yaw)
    local tx = cam.x + dir_x
    local ty = cam.y + dir_y
    local tz = cam.z + dir_z

    crayon.graphics.setCamera3d({
        position = {cam.x, cam.y, cam.z},
        target = {tx, ty, tz},
        up = {0, 1, 0},
        fov = cam.fov
    })

    -- Draw debug physics primitives (all rigid bodies, capsules, wheels, skeletons)
    crayon.physics3d.drawDebug()

    -- Draw Additional Graphics Helpers
    if current_mode == 1 then
        -- Draw ray pointing down under virtual character to show ground check
        if char_virtual and char_virtual:isValid() then
            local px, py, pz = char_virtual:getPosition()
            local nx, ny, nz = char_virtual:getGroundNormal()
            crayon.graphics.drawRay3d(px, py, pz, nx * 1.5, ny * 1.5, nz * 1.5, 0.0, 1.0, 0.2, 1.0)
        end
    end

    -- 2D Overlay UI
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("CRAYON ENGINE 3D PHYSICS DEMO", 10, 10, 1.2)
    crayon.graphics.drawText("MODE (Press 1/2/3): " .. mode_names[current_mode], 10, 30, 1.0)

    if current_mode == 1 then
        crayon.graphics.drawText("Controls: I/J/K/L + U = Rigid Character | T/F/G/H + Y = Virtual Character", 10, 50, 0.9)
        if char_rigid and char_rigid:isValid() then
            local rx, ry, rz = char_rigid:getPosition()
            crayon.graphics.drawText(string.format("Rigid Char: Pos=(%.2f, %.2f, %.2f) State=%s", rx, ry, rz, char_rigid:getGroundState()), 10, 70, 0.8)
        end
        if char_virtual and char_virtual:isValid() then
            local vx, vy, vz = char_virtual:getPosition()
            crayon.graphics.drawText(string.format("Virtual Char: Pos=(%.2f, %.2f, %.2f) State=%s", vx, vy, vz, char_virtual:getGroundState()), 10, 85, 0.8)
        end
    elseif current_mode == 2 then
        crayon.graphics.drawText("Controls: I/J/K/L = Car | T/F/G/H = Tank | Arrows = Bike", 10, 50, 0.9)
        if car_vehicle and car_vehicle:isValid() then
            crayon.graphics.drawText(string.format("Car Speed: %.1f km/h  RPM: %.0f", car_vehicle:getSpeedKmh(), car_vehicle:getEngineRpm()), 10, 70, 0.8)
        end
        if bike_vehicle and bike_vehicle:isValid() then
            crayon.graphics.drawText(string.format("Bike Speed: %.1f km/h  Lean: %.2f rad", bike_vehicle:getSpeedKmh(), bike_vehicle:getLeanAngle()), 10, 85, 0.8)
        end
    elseif current_mode == 3 then
        crayon.graphics.drawText("Ragdoll Keying (Press 4/5/6/7): " .. string.upper(ragdoll_mode), 10, 50, 0.9)
        crayon.graphics.drawText("4: Kinematic Hard | 5: Soft Velocities | 6: Motor Springs | 7: Free Fall", 10, 68, 0.8)
        if ragdoll and ragdoll:isValid() then
            crayon.graphics.drawText(string.format("Ragdoll Active: %s  Part Count: %d", tostring(ragdoll:isActive()), ragdoll:getPartCount()), 10, 85, 0.8)
        end
    end
end
