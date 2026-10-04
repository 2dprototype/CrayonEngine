-- ============================================================
-- Crayon Engine — Complete Physics 3D Ragdoll Sandbox
-- Controls:
--   WASD           : Walk / Move
--   Shift          : Walk toggle
--   Space          : Jump
--   T              : Toggle Ragdoll (Passive Limp)
--   M              : Toggle Active Motor Ragdoll (Pose Drive)
--   G              : Recover standing from Ragdoll
--   F / Mouse Left : Launch Physical Cannonball
--   P              : Trigger Physics Blast Shockwave
--   B              : Spawn Stack of Dynamic Crates
--   Tab            : Toggle Physics 3D Debug Visualizer
--   Arrows / Q / E : Camera rotation and zoom
-- ============================================================

local atan2 = math.atan2 or function(y, x) return math.atan(y, x) end

local MODEL_YAW_OFFSET = math.pi
local FEET_OFFSET = 1.05

-- Camera & Player State
local cam = { yaw = 0, pitch = -20, distance = 6, fov = 60 }
local player = { x = 0, y = 0, z = 0, yaw = 0, anim_state = "Idle" }

-- Core Handles
local human, anim
local skeleton, pose
local ragdoll = nil
local character = nil

-- Sandbox Physics Objects
local boxes = {}
local cannonballs = {}

-- Modes
local debug_mode = true
local active_motor_mode = false

-- ============================================================
-- Helper Utilities
-- ============================================================
local function is_major_bone(name)
    if not name then return false end
    local n = name:lower()
    return not (n:find("finger") or n:find("thumb") or
                n:find("index")  or n:find("middle") or
                n:find("ring")   or n:find("pinky") or
                n:find("pinkie") or n:find("toe"))
end

local function pick_shape(name)
    if not name then return { type = "capsule", radius = 0.09, halfHeight = 0.09, mass = 2.5 } end
    local n = name:lower()
    if n:find("head") then
        return { type = "sphere",  radius = 0.15, mass = 4.0 }
    elseif n:find("hips") or n:find("pelvis") then
        return { type = "capsule", radius = 0.14, halfHeight = 0.10, mass = 8.0 }
    elseif n:find("spine") or n:find("chest") or n:find("torso") then
        return { type = "capsule", radius = 0.15, halfHeight = 0.12, mass = 7.0 }
    elseif n:find("shoulder") then
        return { type = "capsule", radius = 0.09, halfHeight = 0.06, mass = 2.0 }
    elseif n:find("forearm") or n:find("arm") then
        return { type = "capsule", radius = 0.08, halfHeight = 0.13, mass = 2.5 }
    elseif n:find("hand") then
        return { type = "sphere",  radius = 0.08, mass = 1.0 }
    elseif n:find("upleg") or n:find("thigh") then
        return { type = "capsule", radius = 0.11, halfHeight = 0.17, mass = 6.0 }
    elseif n:find("leg") or n:find("shin") or n:find("calf") then
        return { type = "capsule", radius = 0.09, halfHeight = 0.15, mass = 4.0 }
    elseif n:find("foot") then
        return { type = "capsule", radius = 0.08, halfHeight = 0.06, mass = 2.0 }
    elseif n:find("neck") then
        return { type = "capsule", radius = 0.07, halfHeight = 0.05, mass = 1.5 }
    end
    return { type = "capsule", radius = 0.09, halfHeight = 0.09, mass = 2.5 }
end

-- ============================================================
-- Ragdoll Generator
-- ============================================================
local function build_ragdoll_config()
    local joint_names = human:getJointNames() or {}
    local parts = {}
    local joint_to_part = {}

    -- First Pass: Create parts for major bones
    for li, jname in ipairs(joint_names) do
        if is_major_bone(jname) then
            local s = pick_shape(jname)
            local matrix = pose:getJointMatrix(li - 1)
            
            local px, py, pz = player.x, player.y + FEET_OFFSET, player.z
            if matrix then
                px, py, pz = matrix[13], matrix[14], matrix[15]
            end

            joint_to_part[li] = #parts + 1

            table.insert(parts, {
                name = jname,
                parentJointIndex = -1,
                position = { px, py, pz },
                rotation = { 0, 0, 0, 1 },
                shapeType    = s.type,
                radius       = s.radius,
                halfHeight   = s.halfHeight,
                halfExtent   = { s.radius, s.halfHeight or s.radius, s.radius },
                mass         = s.mass,
                friction     = 0.6,
                restitution  = 0.2,
                swingLimitY  = 0.8,
                swingLimitZ  = 0.8,
                twistMin     = -0.5,
                twistMax     =  0.5,
                enableMotors = active_motor_mode,
                motorSpringK = 200.0,
                motorDampingC = 20.0,
                motorMaxTorque = 150.0
            })
        end
    end

    -- Second Pass: Resolve parent joint relationships mapped to ragdoll parts
    for li, jname in ipairs(joint_names) do
        local my_part = joint_to_part[li]
        if my_part then
            local parent_part = -1
            local node = human:getNode(jname)
            if node and node.parent then
                local p = node.parent
                while p and p >= 0 do
                    local pnode = human:getNode(p)
                    if not pnode then break end
                    for lj, parent_name in ipairs(joint_names) do
                        if parent_name == pnode.name and joint_to_part[lj] then
                            parent_part = joint_to_part[lj] - 1
                            break
                        end
                    end
                    if parent_part >= 0 then break end
                    p = pnode.parent
                end
            end
            parts[my_part].parentJointIndex = parent_part
        end
    end

    return parts
end

-- ============================================================
-- Sandbox Props Spawner
-- ============================================================
local function spawn_crate_stack()
    for x = -2, 2 do
        for y = 1, 4 do
            local bx = x * 1.1
            local by = y * 1.05
            local bz = -5.0
            local b = crayon.physics3d.createBox(bx, by, bz, 0.5, 0.5, 0.5, "dynamic", 0.5, 0.3)
            table.insert(boxes, { body = b, size = 1.0 })
        end
    end
end

local function fire_cannonball()
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)

    local dir_x = -math.sin(rad_yaw) * math.cos(rad_pitch)
    local dir_y = math.sin(rad_pitch)
    local dir_z = -math.cos(rad_yaw) * math.cos(rad_pitch)

    local spawn_x = player.x - dir_x * 1.5
    local spawn_y = player.y + 1.8 - dir_y * 1.5
    local spawn_z = player.z - dir_z * 1.5

    local ball = crayon.physics3d.createSphere(spawn_x, spawn_y, spawn_z, 0.45, "dynamic", 0.4, 0.6, 25.0)
    ball:setVelocity(dir_x * 35.0, dir_y * 35.0, dir_z * 35.0)
    table.insert(cannonballs, ball)
end

local function blast_shockwave()
    local cx, cy, cz = player.x, player.y + 1.0, player.z

    for _, box in ipairs(boxes) do
        if box.body and box.body:isValid() then
            local bx, by, bz = box.body:getPosition()
            local dx, dy, dz = bx - cx, by - cy, bz - cz
            local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
            if dist < 8.0 and dist > 0.01 then
                local force = (8.0 - dist) * 15.0
                box.body:applyImpulse((dx / dist) * force, (dy / dist) * force + 4.0, (dz / dist) * force)
            end
        end
    end

    for _, ball in ipairs(cannonballs) do
        if ball:isValid() then
            local bx, by, bz = ball:getPosition()
            local dx, dy, dz = bx - cx, by - cy, bz - cz
            local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
            if dist < 8.0 and dist > 0.01 then
                local force = (8.0 - dist) * 20.0
                ball:applyImpulse((dx / dist) * force, (dy / dist) * force + 5.0, (dz / dist) * force)
            end
        end
    end
end

-- ============================================================
-- Ragdoll Lifecycle
-- ============================================================
local function spawn_ragdoll(enable_motors)
    if ragdoll ~= nil then return end
    active_motor_mode = enable_motors or false

    pose:setRootOffset(player.x, player.y + FEET_OFFSET, player.z)
    anim:applyToPhysicsPose(pose)
    pose:calculateMatrices()

    local parts = build_ragdoll_config()
    ragdoll = crayon.physics3d.createRagdoll({
        disableParentChildCollisions = true,
        stabilize = true,
        parts = parts
    })

    if not ragdoll or not ragdoll:isValid() then
        ragdoll = nil
        return
    end

    ragdoll:setPose(pose)
    ragdoll:activate()

    if character and character:isValid() then
        character:setLinearVelocity(0, 0, 0)
        character:setPosition(0, -500, 0)
    end
end

local function recover_from_ragdoll()
    if ragdoll == nil then return end

    ragdoll:getPose(pose)

    local joint_names = human:getJointNames() or {}
    for li, jname in ipairs(joint_names) do
        local n = jname:lower()
        if n:find("hips") or n:find("pelvis") then
            local m = pose:getJointMatrix(li - 1)
            if m then
                player.x = m[13]
                player.y = m[14] - FEET_OFFSET
                player.z = m[15]
            end
            break
        end
    end

    ragdoll:destroy()
    ragdoll = nil

    if character and character:isValid() then
        character:setPosition(player.x, player.y + FEET_OFFSET, player.z)
        character:setLinearVelocity(0, 0, 0)
    end

    player.anim_state = "Idle"
    anim:play("Idle")
end

-- ============================================================
-- Engine Init
-- ============================================================
function crayon.init()
    crayon.window.setTitle("Crayon Engine - 3D Physics & Ragdoll Sandbox")
    crayon.window.setScalingMode("aspect")
    crayon.graphics.setRetroEffects({ affine = 0.0 })

    -- Static Ground Plane
    crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 200)

    -- Static Scenery & Initial Crates
    spawn_crate_stack()

    -- Load Model & Skeletal Data
    human = crayon.graphics.loadModel("models/Soldier.glb")
    anim  = crayon.graphics.createAnimator(human)

    skeleton = human:createPhysicsSkeleton()
    pose     = crayon.physics3d.createSkeletonPose(skeleton)

    character = crayon.physics3d.createCharacter({
        pos = { 0, FEET_OFFSET, 0 },
        radius = 0.35,
        halfHeight = 0.7,
        mass = 80.0,
        friction = 0.5,
        maxSlopeAngleDeg = 50.0
    })

    anim:play("Idle")
    player.anim_state = "Idle"
end

-- ============================================================
-- Update Loop
-- ============================================================
function crayon.update(dt)
	crayon.physics3d.step(dt)
	
    -- Debug visualizer toggle
    if crayon.input.isPressed("tab") then
        debug_mode = not debug_mode
    end

    -- Camera Orbit & Zoom
    local rot_speed = 90.0 * dt
    if crayon.input.isDown("left")  then cam.yaw = cam.yaw + rot_speed end
    if crayon.input.isDown("right") then cam.yaw = cam.yaw - rot_speed end
    if crayon.input.isDown("up")    then cam.pitch = math.min(cam.pitch + 50 * dt,  75) end
    if crayon.input.isDown("down")  then cam.pitch = math.max(cam.pitch - 50 * dt, -80) end
    if crayon.input.isDown("q")     then cam.distance = math.max(cam.distance - 10 * dt, 2.5) end
    if crayon.input.isDown("e")     then cam.distance = math.min(cam.distance + 10 * dt, 35.0) end

    -- Sandbox Controls
    if crayon.input.isPressed("t") then
        if ragdoll == nil then spawn_ragdoll(false) else recover_from_ragdoll() end
    end
    if crayon.input.isPressed("m") then
        if ragdoll == nil then spawn_ragdoll(true) else recover_from_ragdoll() end
    end
    if crayon.input.isPressed("g") then
        recover_from_ragdoll()
    end
    if crayon.input.isPressed("f") or crayon.input.isMousePressed(1) then
        fire_cannonball()
    end
    if crayon.input.isPressed("p") then
        blast_shockwave()
    end
    if crayon.input.isPressed("b") then
        spawn_crate_stack()
    end

    -- Update Active Ragdoll Pose Tracking
    if ragdoll ~= nil then
        anim:update(dt)
        if active_motor_mode then
            anim:applyToPhysicsPose(pose)
            ragdoll:driveToPoseMotors(pose)
        end

        ragdoll:getPose(pose)
        local joint_names = human:getJointNames() or {}
        for li, jname in ipairs(joint_names) do
            local n = jname:lower()
            if n:find("hips") or n:find("pelvis") or n:find("spine") then
                local m = pose:getJointMatrix(li - 1)
                if m then
                    player.x = m[13]
                    player.y = m[14] - FEET_OFFSET
                    player.z = m[15]
                end
                break
            end
        end
    -- Character Controller Logic
    elseif character and character:isValid() then
        local rad_yaw = math.rad(cam.yaw)
        local fwd_x = -math.sin(rad_yaw)
        local fwd_z = -math.cos(rad_yaw)
        local rgt_x = -fwd_z
        local rgt_z =  fwd_x

        local mx, mz = 0, 0
        if crayon.input.isDown("w") then mx = mx + fwd_x; mz = mz + fwd_z end
        if crayon.input.isDown("s") then mx = mx - fwd_x; mz = mz - fwd_z end
        if crayon.input.isDown("a") then mx = mx - rgt_x; mz = mz - rgt_z end
        if crayon.input.isDown("d") then mx = mx + rgt_x; mz = mz + rgt_z end

        local mag = math.sqrt(mx * mx + mz * mz)
        local speed = 0
        if mag > 0.001 then
            mx, mz = mx / mag, mz / mag
            local walk = crayon.input.isDown("lshift") or crayon.input.isDown("shift")
            speed = walk and 2.5 or 6.5
            player.yaw = atan2(mx, mz)
            local target_anim = walk and "Walk" or "Run"
            if player.anim_state ~= target_anim then
                anim:crossFade(target_anim, 0.2, true)
                player.anim_state = target_anim
            end
        else
            if player.anim_state ~= "Idle" then
                anim:crossFade("Idle", 0.2, true)
                player.anim_state = "Idle"
            end
        end

        local _, vy, _ = character:getLinearVelocity()
        if crayon.input.isPressed("space") and character:isSupported() then
            vy = 7.5
        end

        character:setLinearVelocity(mx * speed, vy, mz * speed)

        local cx, cy, cz = character:getPosition()
        player.x = cx
        player.y = cy - FEET_OFFSET
        player.z = cz

        anim:update(dt)
    end

    if crayon.input.isPressed("escape") then
        crayon.window.quit()
    end
end

-- ============================================================
-- Render Loop
-- ============================================================
function crayon.draw()
    crayon.graphics.clear(0.45, 0.65, 0.88)

    -- Dynamic 3D Camera Setup
    local rad_yaw   = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local tx = player.x
    local ty = player.y + 1.2
    local tz = player.z
    local cx = tx + math.sin(rad_yaw) * math.cos(rad_pitch) * cam.distance
    local cy = ty - math.sin(rad_pitch) * cam.distance
    local cz = tz + math.cos(rad_yaw) * math.cos(rad_pitch) * cam.distance

    crayon.graphics.setCamera3d({
        position = { cx, cy, cz },
        target   = { tx, ty, tz },
        up       = { 0, 1, 0 },
        fov      = cam.fov
    })

    -- Physics 3D Debug Visualizer Layer
    if debug_mode then
        crayon.physics3d.drawDebug(0.3, 1.0, 0.4, 1.0, 0.6, 0.6, 0.8, 0.8)
	else
		crayon.graphics.setLight(-0.4, -1.0, -0.6, 1.0, 0.95, 0.85, 0.35, 0.35, 0.45)

		-- Ground Plane
		crayon.graphics.setColor(0.30, 0.50, 0.25, 1)
		crayon.graphics.drawPlane(0, 0, 0, 250, 250, 0, 0, 0, 0)

		-- Draw Physics Sandbox Objects
		crayon.graphics.setColor(0.80, 0.55, 0.35, 1)
		for _, box in ipairs(boxes) do
			if box.body and box.body:isValid() then
				local x, y, z    = box.body:getPosition()
				local rx, ry, rz = box.body:getRotation()
				crayon.graphics.drawCube(x, y, z, box.size, box.size, box.size, 0, rx, ry, rz)
			end
		end

		-- Draw Physical Cannonballs
		crayon.graphics.setColor(0.2, 0.2, 0.25, 1)
		for _, ball in ipairs(cannonballs) do
			if ball:isValid() then
				local x, y, z = ball:getPosition()
				crayon.graphics.drawSphere(x, y, z, 0.45)
			end
		end

		-- Draw Character / Ragdoll Skinned Mesh
		crayon.graphics.setColor(1, 1, 1, 1)
		if ragdoll ~= nil then
			ragdoll:getPose(pose)
			crayon.graphics.drawModelSkinned(human, pose, 0, 0, 0, 0, 0, 0, 1, 1, 1)
		else
			crayon.graphics.drawModelSkinned(human, anim, player.x, player.y, player.z, 0, player.yaw + MODEL_YAW_OFFSET, 0, 1, 1, 1)
		end

		-- HUD Information
		crayon.graphics.setColor(1, 1, 1, 1)
		crayon.graphics.drawText("Ragdoll Physics 3D Sandbox", 8, 8, 1.0)
		crayon.graphics.drawText("WASD: Move | Shift: Walk | Space: Jump", 8, 22, 1.0)
		crayon.graphics.drawText("T: Passive Ragdoll | M: Motor Ragdoll | G: Recover", 8, 36, 1.0)
		crayon.graphics.drawText("F / Mouse 1: Cannonball | P: Shockwave Blast | B: Spawn Crates", 8, 50, 1.0)
		crayon.graphics.drawText("Tab: Debug Draw | Q/E/Arrows: Camera Controls", 8, 64, 1.0)

		if ragdoll ~= nil then
			local mode_str = active_motor_mode and "ACTIVE MOTOR" or "PASSIVE LIMP"
			crayon.graphics.drawText("RAGDOLL STATE: " .. mode_str, 8, 80, 1.0)
		end
	end
end