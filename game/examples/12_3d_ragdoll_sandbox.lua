-- ============================================================================
-- Example 12: High-Performance 3D Ragdoll Physics Showcase
-- Articulated skeletal ragdolls, stairs tumbling, cannonball blast & zero-G
-- Refactored to Love2D-style userdata architecture (camelCase API)
-- ============================================================================

local cam = {
    x = 0.0, y = 7.0, z = 16.0,
    yaw = -90.0, pitch = -18.0,
    fov = 60.0
}

local models = {}
local textures = {}
local ragdolls = {}     -- list of ragdoll instances
local cannonballs = {}  -- list of launched spheres: { body = body, r = radius }
local show_debug = false
local zero_g = false

-- Helper to spawn an articulated humanoid ragdoll
local function spawn_ragdoll(x, y, z, scale)
    scale = scale or 1.0
    local rd = { parts = {}, constraints = {} }

    local friction = 0.6
    local restitution = 0.1
    local density = 900.0

    -- Colors for retro character outfit
    local col_skin  = {1.0, 0.85, 0.7}
    local col_torso = {0.2, 0.5, 0.9}   -- Blue shirt
    local col_legs  = {0.15, 0.18, 0.3} -- Dark jeans
    local col_shoes = {0.1, 0.1, 0.1}

    -- 1. Pelvis (Hips)
    local pelvis_w, pelvis_h, pelvis_d = 0.5 * scale, 0.3 * scale, 0.35 * scale
    local pelvis = crayon.physics3d.createBox(x, y + 1.8 * scale, z, pelvis_w * 0.5, pelvis_h * 0.5, pelvis_d * 0.5, "dynamic", friction, restitution, density)
    table.insert(rd.parts, { body = pelvis, type = "box", sx = pelvis_w, sy = pelvis_h, sz = pelvis_d, col = col_legs })

    -- 2. Torso / Chest
    local torso_w, torso_h, torso_d = 0.6 * scale, 0.6 * scale, 0.4 * scale
    local torso = crayon.physics3d.createBox(x, y + 2.3 * scale, z, torso_w * 0.5, torso_h * 0.5, torso_d * 0.5, "dynamic", friction, restitution, density)
    table.insert(rd.parts, { body = torso, type = "box", sx = torso_w, sy = torso_h, sz = torso_d, col = col_torso })

    -- Spine Joint (Pelvis <-> Torso)
    local spine_c = crayon.physics3d.createPointConstraint(pelvis, torso, x, y + 2.0 * scale, z)
    table.insert(rd.constraints, spine_c)

    -- 3. Head
    local head_r = 0.25 * scale
    local head = crayon.physics3d.createSphere(x, y + 2.85 * scale, z, head_r, "dynamic", friction, restitution, 700.0)
    table.insert(rd.parts, { body = head, type = "sphere", r = head_r, col = col_skin })

    -- Neck Joint (Torso <-> Head)
    local neck_c = crayon.physics3d.createPointConstraint(torso, head, x, y + 2.6 * scale, z)
    table.insert(rd.constraints, neck_c)

    -- 4. Arms (Left & Right)
    local function create_arm(side)
        local arm_x = x + side * (torso_w * 0.5 + 0.2 * scale)
        local arm_r = 0.12 * scale
        local arm_len = 0.35 * scale

        -- Upper Arm
        local uarm = crayon.physics3d.createCapsule(arm_x, y + 2.2 * scale, z, arm_len * 0.5, arm_r, "dynamic", friction, restitution, density)
        table.insert(rd.parts, { body = uarm, type = "capsule", len = arm_len, r = arm_r, col = col_torso })

        -- Shoulder Joint (Torso <-> Upper Arm)
        local shoulder_c = crayon.physics3d.createPointConstraint(torso, uarm, x + side * torso_w * 0.5, y + 2.45 * scale, z)
        table.insert(rd.constraints, shoulder_c)

        -- Lower Arm / Forearm
        local larm = crayon.physics3d.createCapsule(arm_x, y + 1.7 * scale, z, arm_len * 0.5, arm_r * 0.9, "dynamic", friction, restitution, density)
        table.insert(rd.parts, { body = larm, type = "capsule", len = arm_len, r = arm_r * 0.9, col = col_skin })

        -- Elbow Hinge Joint (Upper Arm <-> Lower Arm)
        local elbow_c = crayon.physics3d.createHingeConstraint(uarm, larm, arm_x, y + 1.95 * scale, z, 1.0, 0.0, 0.0, 0.0, math.rad(130))
        table.insert(rd.constraints, elbow_c)
    end

    create_arm(-1.0) -- Left Arm
    create_arm( 1.0) -- Right Arm

    -- 5. Legs (Left & Right)
    local function create_leg(side)
        local leg_x = x + side * 0.22 * scale
        local leg_r = 0.14 * scale
        local thigh_len = 0.45 * scale
        local calf_len = 0.45 * scale

        -- Thigh (Upper Leg)
        local thigh = crayon.physics3d.createCapsule(leg_x, y + 1.35 * scale, z, thigh_len * 0.5, leg_r, "dynamic", friction, restitution, density)
        table.insert(rd.parts, { body = thigh, type = "capsule", len = thigh_len, r = leg_r, col = col_legs })

        -- Hip Joint (Pelvis <-> Thigh)
        local hip_c = crayon.physics3d.createPointConstraint(pelvis, thigh, leg_x, y + 1.65 * scale, z)
        table.insert(rd.constraints, hip_c)

        -- Calf (Lower Leg)
        local calf = crayon.physics3d.createCapsule(leg_x, y + 0.75 * scale, z, calf_len * 0.5, leg_r * 0.85, "dynamic", friction, restitution, density)
        table.insert(rd.parts, { body = calf, type = "capsule", len = calf_len, r = leg_r * 0.85, col = col_shoes })

        -- Knee Hinge Joint (Thigh <-> Calf, bends backwards)
        local knee_c = crayon.physics3d.createHingeConstraint(thigh, calf, leg_x, y + 1.05 * scale, z, 1.0, 0.0, 0.0, math.rad(-130), 0.0)
        table.insert(rd.constraints, knee_c)
    end

    create_leg(-1.0) -- Left Leg
    create_leg( 1.0) -- Right Leg

    table.insert(ragdolls, rd)
    return rd
end

local function reset_scene()
    crayon.physics3d.destroyAll()
    ragdolls = {}
    cannonballs = {}

    -- Ground Floor
    crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 60.0)

    -- Giant Tumbling Stairs on the Left
    local num_steps = 14
    local step_h = 0.65
    local step_d = 1.0
    local step_w = 9.0
    for i = 1, num_steps do
        local sx = -4.0
        local sy = (i - 1) * step_h + step_h * 0.5
        local sz = -(i - 1) * step_d + 3.0
        crayon.physics3d.createBox(sx, sy, sz, step_w * 0.5, step_h * 0.5, step_d * 0.5, "static", 0.6, 0.2)
    end

    -- Top platform
    local top_y = num_steps * step_h
    local top_z = -(num_steps - 1) * step_d + 3.0 - 2.0
    crayon.physics3d.createBox(-4.0, top_y - 0.2, top_z, 4.5, 0.4, 3.0, "static", 0.6, 0.2)

    -- Obstacle / Bowling Pins on the Right
    for px = -1, 1 do
        for pz = -1, 1 do
            local pin = crayon.physics3d.createCapsule(5.0 + px * 1.5, 1.0, -2.0 + pz * 1.5, 0.6, 0.25, "dynamic", 0.5, 0.3, 500.0)
        end
    end

    -- Spawn Initial Ragdolls
    spawn_ragdoll(-4.0, top_y + 1.0, top_z + 1.0, 1.0)
    spawn_ragdoll(0.0, 3.0, 0.0, 1.1)
end

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("Crayon Engine - 3D Jolt Ragdoll Sandbox")

    models.cube = crayon.graphics.loadModel("cube")
    models.sphere = crayon.graphics.loadModel("sphere")
    models.cylinder = crayon.graphics.loadModel("cylinder")
    models.capsule = crayon.graphics.loadModel("capsule")
    models.plane = crayon.graphics.loadModel("plane")

    textures.grass = crayon.graphics.loadTexture("game/assets/textures/grass.bmp")
    textures.brick = crayon.graphics.loadTexture("game/assets/textures/brick.bmp")

    crayon.graphics.setRetroEffects({
        pixelation = 1,
        depth_fog = true,
        lighting = true,
        affine = 0.8,
        dither = true,
        fog = { start = 14, ["end"] = 45, color = {0.07, 0.09, 0.14} }
    })

    crayon.graphics.setLight(-0.5, -0.8, -0.6, 1.0, 0.95, 0.9, 0.3, 0.3, 0.38)

    reset_scene()
end

function crayon.update(dt)
    -- Camera Orbit & Movement
    local move_speed = 8.0 * dt
    local rad_yaw = math.rad(cam.yaw)
    local fwd_x, fwd_z = math.cos(rad_yaw), math.sin(rad_yaw)
    local right_x, right_z = -fwd_z, fwd_x

    if crayon.input.isDown("w") then cam.x = cam.x + fwd_x * move_speed; cam.z = cam.z + fwd_z * move_speed end
    if crayon.input.isDown("s") then cam.x = cam.x - fwd_x * move_speed; cam.z = cam.z - fwd_z * move_speed end
    if crayon.input.isDown("a") then cam.x = cam.x - right_x * move_speed; cam.z = cam.z - right_z * move_speed end
    if crayon.input.isDown("d") then cam.x = cam.x + right_x * move_speed; cam.z = cam.z + right_z * move_speed end
    if crayon.input.isDown("q") or crayon.input.isDown("space") then cam.y = cam.y + move_speed end
    if crayon.input.isDown("z") or crayon.input.isDown("lshift") then cam.y = cam.y - move_speed end

    if crayon.input.isDown("left") then cam.yaw = cam.yaw - 80.0 * dt end
    if crayon.input.isDown("right") then cam.yaw = cam.yaw + 80.0 * dt end
    if crayon.input.isDown("up") then cam.pitch = math.min(cam.pitch + 60.0 * dt, 80.0) end
    if crayon.input.isDown("down") then cam.pitch = math.max(cam.pitch - 60.0 * dt, -80.0) end

    local rad_pitch = math.rad(cam.pitch)
    local look_x = math.cos(rad_pitch) * math.cos(rad_yaw)
    local look_y = math.sin(rad_pitch)
    local look_z = math.cos(rad_pitch) * math.sin(rad_yaw)

    -- Shoot Heavy Cannonball on Click or 'F'
    if crayon.input.isMouseDown(1) or crayon.input.isPressed("f") then
        local r = 0.5
        local ball = crayon.physics3d.createSphere(cam.x + look_x * 1.5, cam.y + look_y * 1.5, cam.z + look_z * 1.5, r, "dynamic", 0.5, 0.4, 2500.0)
        local v = 45.0
        ball:setVelocity(look_x * v, look_y * v, look_z * v)
        table.insert(cannonballs, { body = ball, r = r })
    end

    -- Spawn a new Ragdoll in front of camera on 'B'
    if crayon.input.isPressed("b") then
        local rx = cam.x + look_x * 6.0
        local ry = math.max(cam.y + look_y * 6.0, 1.5)
        local rz = cam.z + look_z * 6.0
        local new_rd = spawn_ragdoll(rx, ry, rz, 0.85 + math.random() * 0.3)
        -- Random tumble impulse
        new_rd.parts[1].body:applyImpulse((math.random() - 0.5) * 20.0, 10.0, (math.random() - 0.5) * 20.0)
    end

    -- Radial Blast / Explosion on 'E' using Overlap Sphere Query!
    if crayon.input.isPressed("e") then
        local blast_pos = {cam.x + look_x * 8.0, cam.y + look_y * 8.0, cam.z + look_z * 8.0}
        local blast_radius = 12.0
        local nearby_bodies = crayon.physics3d.overlapSphere(blast_pos[1], blast_pos[2], blast_pos[3], blast_radius)

        for _, b in ipairs(nearby_bodies) do
            if b:isValid() then
                local bx, by, bz = b:getPosition()
                local dx, dy, dz = bx - blast_pos[1], by - blast_pos[2], bz - blast_pos[3]
                local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
                if dist > 0.01 then
                    local force = (1.0 - dist / blast_radius) * 120.0
                    b:applyImpulse((dx / dist) * force, (dy / dist + 0.5) * force, (dz / dist) * force)
                end
            end
        end
    end

    -- Zero-G toggle on 'G'
    if crayon.input.isPressed("g") then
        zero_g = not zero_g
        if zero_g then
            crayon.physics3d.setGravity(0.0, 0.0, 0.0)
        else
            crayon.physics3d.setGravity(0.0, -9.81, 0.0)
        end
    end

    -- Wireframe Debug toggle on 'TAB'
    if crayon.input.isPressed("tab") then
        show_debug = not show_debug
    end

    -- Reset Scene on 'R'
    if crayon.input.isPressed("r") then
        reset_scene()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.09, 0.10, 0.17)

    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local target_x = cam.x + math.cos(rad_pitch) * math.cos(rad_yaw)
    local target_y = cam.y + math.sin(rad_pitch)
    local target_z = cam.z + math.cos(rad_pitch) * math.sin(rad_yaw)

    crayon.graphics.setCamera3d({
        position = {cam.x, cam.y, cam.z},
        target = {target_x, target_y, target_z},
        up = {0, 1, 0},
        fov = cam.fov
    })

    -- Render Ground
    crayon.graphics.drawModel(models.plane, 0, 0, 0, 0, 0, 0, 9.0, 1.0, 9.0, textures.grass)

    -- Render Tumbling Stairs
    local num_steps = 14
    local step_h = 0.65
    local step_d = 1.0
    local step_w = 9.0
    for i = 1, num_steps do
        local sx = -4.0
        local sy = (i - 1) * step_h + step_h * 0.5
        local sz = -(i - 1) * step_d + 3.0
        crayon.graphics.drawModel(models.cube, sx, sy, sz, 0, 0, 0, step_w, step_h, step_d, textures.brick)
    end
    local top_y = num_steps * step_h
    local top_z = -(num_steps - 1) * step_d + 3.0 - 2.0
    crayon.graphics.drawModel(models.cube, -4.0, top_y - 0.2, top_z, 0, 0, 0, 9.0, 0.4, 6.0, textures.brick)

    -- Render Ragdolls
    for _, rd in ipairs(ragdolls) do
        for _, part in ipairs(rd.parts) do
            if part.body:isValid() then
                local px, py, pz = part.body:getPosition()
                local rx, ry, rz = part.body:getRotation()
                crayon.graphics.setColor(part.col[1], part.col[2], part.col[3], 1.0)

                if part.type == "box" then
                    crayon.graphics.drawModel(models.cube, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), part.sx, part.sy, part.sz)
                elseif part.type == "sphere" then
                    crayon.graphics.drawModel(models.sphere, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), part.r * 2.0, part.r * 2.0, part.r * 2.0)
                elseif part.type == "capsule" then
                    crayon.graphics.drawModel(models.cylinder, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), part.r * 2.0, part.len, part.r * 2.0)
                end
            end
        end
    end

    -- Render Cannonballs
    crayon.graphics.setColor(0.3, 0.3, 0.35, 1.0)
    for _, cb in ipairs(cannonballs) do
        if cb.body:isValid() then
            local px, py, pz = cb.body:getPosition()
            crayon.graphics.drawModel(models.sphere, px, py, pz, 0, 0, 0, cb.r * 2.0, cb.r * 2.0, cb.r * 2.0)
        end
    end
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)

    -- Debug Wireframes (optional)
    if show_debug then
        crayon.physics3d.drawDebug(0.3, 1.0, 0.4, 1.0, 0.6, 0.6, 0.8, 0.8)
    end

    -- 2D HUD Overlays
    crayon.graphics.setColor(0.0, 0.0, 0.0, 0.65)
    crayon.graphics.drawRect("fill", 4, 4, 205, 54)
    crayon.graphics.setColor(0.3, 0.6, 1.0, 1.0)
    crayon.graphics.drawRect("line", 4, 4, 205, 54)

    local total_bodies, active_bodies = crayon.physics3d.getBodyCount()
    local fps = math.floor(crayon.window.get_fps() + 0.5)

    crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.drawText("3D JOLT RAGDOLL SHOWCASE", 8, 8, 1.0)
    crayon.graphics.setColor(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.drawText("FPS: " .. fps .. " | Ragdolls: " .. #ragdolls .. " | Bodies: " .. active_bodies .. "/" .. total_bodies, 8, 18, 1.0)
    crayon.graphics.setColor(0.8, 0.85, 1.0, 1.0)
    crayon.graphics.drawText("Click/F: Heavy Cannonball | B: Spawn Ragdoll", 8, 28, 1.0)
    crayon.graphics.drawText("E: AOE Blast | G: Zero-G [" .. (zero_g and "ON" or "OFF") .. "] | TAB: Wire | R: Reset", 8, 38, 1.0)

    -- Aiming Reticle
    crayon.graphics.setColor(1.0, 1.0, 1.0, 0.8)
    crayon.graphics.drawLine(156, 120, 164, 120, 1.0)
    crayon.graphics.drawLine(160, 116, 160, 124, 1.0)
end
