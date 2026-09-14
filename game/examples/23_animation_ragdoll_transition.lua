-- ============================================================================
-- Example 23: Open-World Animation-Based Ragdoll System (FIXED)
-- ============================================================================

local cam = {
    x = 0.0, y = 5.0, z = 12.0,
    target_x = 0.0, target_y = 1.0, target_z = 0.0,
    yaw = -90.0, pitch = -15.0,
    dist = 8.0,
    fov = 60
}

local STATE_ANIMATED        = 1
local STATE_PHYSICAL_ANIM   = 2
local STATE_RAGDOLL         = 3
local STATE_GETTING_UP      = 4

local char_state = STATE_ANIMATED
local char_pos = { x = 0.0, y = 1.0, z = 0.0 }
local char_rot_y = 0.0

local char_controller = nil
local char_ragdoll = nil
local char_skeleton = nil
local char_pose = nil
local char_model = nil
local char_animator = nil

local cannonballs = {}

local settle_timer = 0.0
local get_up_duration = 1.2
local get_up_timer = 0.0
local get_up_clip = "Idle"
local ground_orientation = "unknown"

-- ----------------------------------------------------------------
-- Humanoid ragdoll — camelCase keys matching the C++ binding
-- ----------------------------------------------------------------
local function create_humanoid_ragdoll(x, y, z)
    local parts = {
        {   -- 0: Pelvis (root)
            name = "Pelvis",
            parentJointIndex = -1,
            position = { x, y + 1.0, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.18,
            halfHeight = 0.12,
            mass = 20.0,
            friction = 0.8,
            enableMotors = true,
            motorSpringK = 4000.0,
            motorDampingC = 150.0,
            motorMaxTorque = 500.0
        },
        {   -- 1: Spine
            name = "Spine",
            parentJointIndex = 0,
            position = { x, y + 1.35, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.19,
            halfHeight = 0.16,
            mass = 22.0,
            friction = 0.8,
            swingLimitY = math.rad(30),
            swingLimitZ = math.rad(25),
            twistMin = math.rad(-20),
            twistMax = math.rad(20),
            enableMotors = true,
            motorSpringK = 4000.0,
            motorDampingC = 150.0,
            motorMaxTorque = 600.0
        },
        {   -- 2: Head
            name = "Head",
            parentJointIndex = 1,
            position = { x, y + 1.75, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "sphere",
            radius = 0.15,
            mass = 5.0,
            friction = 0.6,
            swingLimitY = math.rad(35),
            swingLimitZ = math.rad(35),
            twistMin = math.rad(-40),
            twistMax = math.rad(40),
            enableMotors = true,
            motorSpringK = 2500.0,
            motorDampingC = 80.0,
            motorMaxTorque = 200.0
        },
        {   -- 3: UpperArm_L
            name = "UpperArm_L",
            parentJointIndex = 1,
            position = { x - 0.32, y + 1.35, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.08,
            halfHeight = 0.14,
            mass = 4.0,
            friction = 0.6,
            swingLimitY = math.rad(70),
            swingLimitZ = math.rad(60),
            twistMin = math.rad(-60),
            twistMax = math.rad(60),
            enableMotors = true,
            motorSpringK = 2500.0,
            motorDampingC = 80.0,
            motorMaxTorque = 250.0
        },
        {   -- 4: LowerArm_L
            name = "LowerArm_L",
            parentJointIndex = 3,
            position = { x - 0.32, y + 1.0, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.07,
            halfHeight = 0.14,
            mass = 3.0,
            friction = 0.6,
            swingLimitY = math.rad(10),
            swingLimitZ = math.rad(80),
            twistMin = math.rad(-15),
            twistMax = math.rad(15),
            enableMotors = true,
            motorSpringK = 2000.0,
            motorDampingC = 60.0,
            motorMaxTorque = 200.0
        },
        {   -- 5: UpperArm_R
            name = "UpperArm_R",
            parentJointIndex = 1,
            position = { x + 0.32, y + 1.35, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.08,
            halfHeight = 0.14,
            mass = 4.0,
            friction = 0.6,
            swingLimitY = math.rad(70),
            swingLimitZ = math.rad(60),
            twistMin = math.rad(-60),
            twistMax = math.rad(60),
            enableMotors = true,
            motorSpringK = 2500.0,
            motorDampingC = 80.0,
            motorMaxTorque = 250.0
        },
        {   -- 6: LowerArm_R
            name = "LowerArm_R",
            parentJointIndex = 5,
            position = { x + 0.32, y + 1.0, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.07,
            halfHeight = 0.14,
            mass = 3.0,
            friction = 0.6,
            swingLimitY = math.rad(10),
            swingLimitZ = math.rad(80),
            twistMin = math.rad(-15),
            twistMax = math.rad(15),
            enableMotors = true,
            motorSpringK = 2000.0,
            motorDampingC = 60.0,
            motorMaxTorque = 200.0
        },
        {   -- 7: Thigh_L
            name = "Thigh_L",
            parentJointIndex = 0,
            position = { x - 0.16, y + 0.68, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.11,
            halfHeight = 0.18,
            mass = 8.0,
            friction = 0.8,
            swingLimitY = math.rad(60),
            swingLimitZ = math.rad(45),
            twistMin = math.rad(-30),
            twistMax = math.rad(30),
            enableMotors = true,
            motorSpringK = 4500.0,
            motorDampingC = 180.0,
            motorMaxTorque = 600.0
        },
        {   -- 8: Calf_L
            name = "Calf_L",
            parentJointIndex = 7,
            position = { x - 0.16, y + 0.28, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.09,
            halfHeight = 0.18,
            mass = 5.0,
            friction = 0.8,
            swingLimitY = math.rad(10),
            swingLimitZ = math.rad(80),
            twistMin = math.rad(-15),
            twistMax = math.rad(15),
            enableMotors = true,
            motorSpringK = 4000.0,
            motorDampingC = 150.0,
            motorMaxTorque = 500.0
        },
        {   -- 9: Thigh_R
            name = "Thigh_R",
            parentJointIndex = 0,
            position = { x + 0.16, y + 0.68, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.11,
            halfHeight = 0.18,
            mass = 8.0,
            friction = 0.8,
            swingLimitY = math.rad(60),
            swingLimitZ = math.rad(45),
            twistMin = math.rad(-30),
            twistMax = math.rad(30),
            enableMotors = true,
            motorSpringK = 4500.0,
            motorDampingC = 180.0,
            motorMaxTorque = 600.0
        },
        {   -- 10: Calf_R
            name = "Calf_R",
            parentJointIndex = 9,
            position = { x + 0.16, y + 0.28, z },
            rotation = { 0, 0, 0, 1 },
            shapeType = "capsule",
            radius = 0.09,
            halfHeight = 0.18,
            mass = 5.0,
            friction = 0.8,
            swingLimitY = math.rad(10),
            swingLimitZ = math.rad(80),
            twistMin = math.rad(-15),
            twistMax = math.rad(15),
            enableMotors = true,
            motorSpringK = 4000.0,
            motorDampingC = 150.0,
            motorMaxTorque = 500.0
        }
    }

    return crayon.physics3d.createRagdoll({
        parts = parts,
        disableParentChildCollisions = true,
        stabilize = true
    })
end

-- ----------------------------------------------------------------
-- State transitions
-- ----------------------------------------------------------------
local function trigger_knockdown(ix, iy, iz)
    if char_state == STATE_RAGDOLL or char_state == STATE_GETTING_UP then return end
    if not char_ragdoll then return end

    print("[RagdollDemo] Knockdown! Transferring momentum...")
    char_state = STATE_RAGDOLL
    settle_timer = 0.0

    local cx, cy, cz = char_controller:getPosition()
    local vx, vy, vz = char_controller:getLinearVelocity()

    if char_animator and char_pose then
        char_animator:applyToPhysicsPose(char_pose, cx, cy - 0.55, cz)
        char_ragdoll:setPose(char_pose)
    end

    char_ragdoll:resetWarmStart()
    char_ragdoll:setHardKeying(false)
    char_ragdoll:setMotorsStiffness(0.0)
    char_ragdoll:setLinearVelocity(vx, vy, vz)
    char_ragdoll:addImpulse(ix or 0, iy or 40, iz or 0)
    -- Part 0 = Pelvis, Part 1 = Spine. Pick whichever you want to emphasize.
    char_ragdoll:addImpulseToPart(1,
        (ix or 0) * 0.8,
        (iy or 30),
        (iz or 0) * 0.8)
    char_ragdoll:activate()
end

local function trigger_recovery()
    if char_state ~= STATE_RAGDOLL then return end

    print("[RagdollDemo] Settled. Initiating get-up...")
    char_state = STATE_GETTING_UP
    get_up_timer = 0.0

    local rx, ry, rz = char_ragdoll:getRootTransform()
    ground_orientation = char_ragdoll:getGroundOrientation()
    print(string.format("[RagdollDemo] Root (%.2f,%.2f,%.2f) orientation=%s",
        rx, ry, rz, ground_orientation))

    -- Find ground below root
    local hit, hx, hy, hz = crayon.physics3d.raycast(rx, ry + 0.5, rz, 0, -1, 0, 3.0)
    local target_y = hit and hy or ry

    char_controller:setPosition(rx, target_y + 0.9, rz)
    char_controller:setLinearVelocity(0, 0, 0)
    char_pos.x, char_pos.y, char_pos.z = rx, target_y + 0.9, rz

    if char_animator then
        char_animator:crossFadeFromCurrentPose(get_up_clip, 0.35, false)
    end

    char_ragdoll:setHardKeying(true)
    char_ragdoll:setMotorsStiffness(4000.0, 150.0, 500.0)
end

-- ----------------------------------------------------------------
-- Init
-- ----------------------------------------------------------------
function crayon.init()
    crayon.window.setResolution(400, 240)
    crayon.window.setTitle("Crayon Engine - Animation & Ragdoll")
    crayon.window.setScalingMode("integer")

    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.setShadingMode("gouraud")
    crayon.graphics.setLight(-0.6, -1.0, 0.4, 1.0, 0.95, 0.9, 0.25, 0.25, 0.3)

    -- Ground plane (y = 0, up = +Y)
    crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 100)

    -- Static steps
    crayon.physics3d.createBox(3.0, 0.25, -2.0, 1.5, 0.25, 1.0, "static", 0.6, 0.1)
    crayon.physics3d.createBox(3.0, 0.60, -4.0, 1.5, 0.35, 1.0, "static", 0.6, 0.1)

    -- Character controller (virtual, kinematic)
    char_controller = crayon.physics3d.createCharacterVirtual({
        pos = { char_pos.x, char_pos.y, char_pos.z },
        radius = 0.35,
        halfHeight = 0.55,
        mass = 75.0,
        maxSlopeAngleDeg = 50.0,
        stepHeight = 0.35
    })

    -- Load model first so we can validate the ragdoll needs it
    local m = crayon.graphics.loadModel("models/Soldier.glb")
    if m and m:isValid() and m:isSkinned() then
        char_model = m
        char_animator = m:createAnimator()
        char_skeleton = m:createPhysicsSkeleton()

        if char_skeleton and char_skeleton:isValid() then
            char_pose = crayon.physics3d.createSkeletonPose(char_skeleton)
        end

        -- Pick a get-up clip if we can, otherwise Idle
        local anims = m:getAnimationNames()
        get_up_clip = anims[1] or "Idle"
        for _, name in ipairs(anims) do
            local n = name:lower()
            if n:find("getup") or n:find("get_up") or n:find("standup") then
                get_up_clip = name
                break
            end
        end

        if char_animator and anims[1] then
            char_animator:play(anims[1], true, 1.0)
        end
    end

    -- Ragdoll — drive from pose if we have a skinned model
    char_ragdoll = create_humanoid_ragdoll(char_pos.x, char_pos.y, char_pos.z)
    if char_ragdoll then
        char_ragdoll:setHardKeying(true)
    end
end

-- ----------------------------------------------------------------
-- Update
-- ----------------------------------------------------------------
function crayon.update(dt)
    -- Camera orbit
    if crayon.input.isMouseDown("right") then
        local dx, dy = crayon.input.getMouseDelta()
        cam.yaw = cam.yaw + dx * 0.3
        cam.pitch = math.max(-85.0, math.min(85.0, cam.pitch - dy * 0.3))
    end

    local _, scroll = crayon.input.getMouseWheel()
    if scroll ~= 0 then
        cam.dist = math.max(2.0, math.min(30.0, cam.dist - scroll))
    end

    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    cam.x = cam.target_x + cam.dist * math.cos(rad_pitch) * math.cos(rad_yaw)
    cam.y = cam.target_y + cam.dist * math.sin(rad_pitch)
    cam.z = cam.target_z + cam.dist * math.cos(rad_pitch) * math.sin(rad_yaw)

    -- Knockdown
    if crayon.input.isPressed("space") or crayon.input.isPressed("k") then
        if char_state ~= STATE_RAGDOLL and char_state ~= STATE_GETTING_UP then
            local fwd_x = -math.sin(char_rot_y) * 250.0
            local fwd_z = -math.cos(char_rot_y) * 250.0
            trigger_knockdown(fwd_x, 180.0, fwd_z)
        end
    end

    -- Motor mode toggle
    if crayon.input.isPressed("m") then
        if char_state == STATE_ANIMATED then
            char_state = STATE_PHYSICAL_ANIM
            char_ragdoll:setHardKeying(false)
            char_ragdoll:setMotorsStiffness(1500.0, 80.0, 350.0)
            print("[RagdollDemo] Active Physical Animation ON")
        elseif char_state == STATE_PHYSICAL_ANIM then
            char_state = STATE_ANIMATED
            char_ragdoll:setHardKeying(true)
            char_ragdoll:setMotorsStiffness(4500.0, 150.0, 600.0)
            print("[RagdollDemo] Standard animated locomotion")
        end
    end

    -- Force recovery
    if crayon.input.isPressed("r") and char_state == STATE_RAGDOLL then
        trigger_recovery()
    end

    -- Cannonball
    if crayon.input.isMousePressed("left") then
        local origin_x = char_pos.x
        local origin_y = char_pos.y + 0.6
        local origin_z = char_pos.z
        local target_x = cam.target_x - origin_x
        local target_y = cam.target_y - origin_y
        local target_z = cam.target_z - origin_z
        local len = math.sqrt(target_x*target_x + target_y*target_y + target_z*target_z)
        if len > 0.001 then
            target_x, target_y, target_z = target_x/len, target_y/len, target_z/len
            local ball = crayon.physics3d.createSphere(origin_x, origin_y, origin_z,
                                                       0.15, "dynamic", 0.5, 0.4, 1500.0)
            ball:setVelocity(target_x * 30.0, target_y * 30.0, target_z * 30.0)
            table.insert(cannonballs, { body = ball, r = 0.15 })
        end
    end

    -- State machine
    if char_state == STATE_ANIMATED or char_state == STATE_PHYSICAL_ANIM then
        local move_x, move_z = 0.0, 0.0
        if crayon.input.isDown("w") then move_z = move_z - 1.0 end
        if crayon.input.isDown("s") then move_z = move_z + 1.0 end
        if crayon.input.isDown("a") then move_x = move_x - 1.0 end
        if crayon.input.isDown("d") then move_x = move_x + 1.0 end

        local input_len = math.sqrt(move_x*move_x + move_z*move_z)
        local speed = 5.0

        if input_len > 0.001 then
            move_x = (move_x / input_len) * speed
            move_z = (move_z / input_len) * speed
            char_rot_y = math.atan2(-move_x, -move_z)
        else
            move_x, move_z = 0.0, 0.0
        end

        -- Preserve gravity accumulation from the controller
        local _, vy, _ = char_controller:getLinearVelocity()
        if char_controller:isSupported() and vy < 0 then vy = 0.0 end

        char_controller:setLinearVelocity(move_x, vy, move_z)
        char_controller:update(dt)

        local cx, cy, cz = char_controller:getPosition()
        char_pos.x, char_pos.y, char_pos.z = cx, cy, cz
        cam.target_x, cam.target_y, cam.target_z = cx, cy + 0.9, cz

        if char_animator then
            char_animator:update(dt)
            if char_pose then
                char_animator:applyToPhysicsPose(char_pose, cx, cy - 0.55, cz)
                if char_state == STATE_PHYSICAL_ANIM then
                    char_ragdoll:driveToPoseMotors(char_pose)
                else
                    char_ragdoll:setPose(char_pose)
                end
            end
        end

    elseif char_state == STATE_RAGDOLL then
        local rx, ry, rz = char_ragdoll:getRootTransform()
        cam.target_x, cam.target_y, cam.target_z = rx, ry + 0.5, rz

        local vx, vy, vz = char_ragdoll:getLinearVelocity()
        local speed = math.sqrt(vx*vx + vy*vy + vz*vz)

        if speed < 0.18 then
            settle_timer = settle_timer + dt
            if settle_timer > 0.8 then
                trigger_recovery()
            end
        else
            settle_timer = 0.0
        end

        if char_animator and char_pose then
            char_ragdoll:getPose(char_pose)
            char_animator:capturePhysicsPose(char_pose)
        end

    elseif char_state == STATE_GETTING_UP then
        get_up_timer = get_up_timer + dt
        if char_animator then
            char_animator:update(dt)
        end

        -- Snap ragdoll to the current animated get-up pose
        if char_animator and char_pose then
            local cx, cy, cz = char_controller:getPosition()
            char_animator:applyToPhysicsPose(char_pose, cx, cy - 0.55, cz)
            char_ragdoll:setPose(char_pose)
        end

        if get_up_timer >= get_up_duration then
            char_state = STATE_ANIMATED
            print("[RagdollDemo] Get-up complete. Locomotion resumed.")
        end
    end
end

-- ----------------------------------------------------------------
-- Draw
-- ----------------------------------------------------------------
function crayon.draw()
    crayon.graphics.clear(0.15, 0.15, 0.2)
    crayon.graphics.setCamera3d({
        position = { cam.x, cam.y, cam.z },
        target   = { cam.target_x, cam.target_y, cam.target_z },
        up       = { 0, 1, 0 },
        fov      = cam.fov
    })

    -- Ground grid (3D lines)
    crayon.graphics.setColor(0.35, 0.38, 0.42, 1.0)
    for x = -20, 20, 2 do
        crayon.graphics.drawLine3d(x, 0.01, -20, x, 0.01, 20)
        crayon.graphics.drawLine3d(-20, 0.01, x, 20, 0.01, x)
    end

    -- Character: prefer skinned draw when we have a model
    crayon.graphics.setColor(1, 1, 1, 1)
    if char_model and char_animator then
        if char_state == STATE_RAGDOLL and char_pose then
            crayon.graphics.drawModelSkinned(char_model, char_pose,
                char_pos.x, char_pos.y - 0.55, char_pos.z,
                0, char_rot_y, 0, 1, 1, 1)
        else
            crayon.graphics.drawModelSkinned(char_model, char_animator,
                char_pos.x, char_pos.y - 0.55, char_pos.z,
                0, char_rot_y, 0, 1, 1, 1)
        end
    else
        -- Fallback: procedural body visual
        if char_ragdoll and char_ragdoll:isValid() then
            local n = char_ragdoll:getPartCount()
            for i = 0, n - 1 do
                local px, py, pz = char_ragdoll:getPartPosition(i)
                if i == 0 then
                    crayon.graphics.setColor(0.2, 0.4, 0.8, 1)
                    crayon.graphics.drawCapsule(px, py, pz, 0.18, 0.24)
                elseif i == 1 then
                    crayon.graphics.setColor(0.3, 0.6, 0.9, 1)
                    crayon.graphics.drawCapsule(px, py, pz, 0.19, 0.32)
                elseif i == 2 then
                    crayon.graphics.setColor(0.9, 0.75, 0.6, 1)
                    crayon.graphics.drawSphere(px, py, pz, 0.15)
                else
                    crayon.graphics.setColor(0.7, 0.7, 0.7, 1)
                    crayon.graphics.drawCapsule(px, py, pz, 0.09, 0.28)
                end
            end
        end
    end

    -- Cannonballs
    crayon.graphics.setColor(0.85, 0.25, 0.2, 1.0)
    for _, cb in ipairs(cannonballs) do
        if cb.body:isValid() then
            local bx, by, bz = cb.body:getPosition()
            crayon.graphics.drawSphere(bx, by, bz, cb.r)
        end
    end

    -- HUD (screen space)
    crayon.graphics.resetCamera2d()

    local state_text = "ANIMATED LOCOMOTION"
    local r, g, b = 0.3, 1.0, 0.4
    if char_state == STATE_PHYSICAL_ANIM then
        state_text = "ACTIVE PHYSICAL ANIMATION"
        r, g, b = 0.4, 0.8, 1.0
    elseif char_state == STATE_RAGDOLL then
        state_text = string.format("RAGDOLL (settle %.1fs)", settle_timer)
        r, g, b = 1.0, 0.3, 0.3
    elseif char_state == STATE_GETTING_UP then
        state_text = "GETTING UP (" .. string.upper(ground_orientation) .. ")"
        r, g, b = 1.0, 0.8, 0.2
    end

    crayon.graphics.setColor(r, g, b, 1.0)
    crayon.graphics.drawText("State: " .. state_text, 10, 10, 1.0)

    crayon.graphics.setColor(1, 1, 1, 0.85)
    crayon.graphics.drawText("WASD: Move | SPACE/K: Knockdown | M: Physical Anim | R: Get-Up", 10, 24, 1.0)
    crayon.graphics.drawText("LMB: Shoot | RMB: Orbit | Wheel: Zoom", 10, 36, 1.0)
end