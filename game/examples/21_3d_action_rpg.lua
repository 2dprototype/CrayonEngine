-- ============================================================================
-- 3D Action RPG Sample Game
-- Crayon Engine
-- Features:
--  - Skinned glTF Humanoid Character (Soldier.glb) with Skeletal Animation
--  - Smooth Animation CrossFading (Idle <-> Walk <-> Run)
--  - Jolt Physics 3D Capsule Controller (Character Physics)
--  - Third-Person Follow Camera with Smooth Damping & Mouse Orbit / Zoom
--  - Interactive World: Physics Obstacles, Breakable Barrels, Dynamic Crates
--  - Combat System: Sword Slash / Attack action with Hit Detection & Impulses
--  - Dynamic Lighting, Torchlight, Distance Fog, and Retro Aesthetics
-- ============================================================================

-- Game State
local character_model = nil
local animator = nil
local is_skinned = false
local char_body = nil

-- Player Parameters
local player = {
    x = 0.0, y = 1.0, z = 0.0,
    rot_y = 0.0,
    target_rot_y = 0.0,
    vx = 0.0, vy = 0.0, vz = 0.0,
    speed = 0.0,
    walk_speed = 3.5,
    run_speed = 7.5,
    is_grounded = true,
    is_running = false,
    is_attacking = false,
    attack_timer = 0.0,
    health = 100,
    max_health = 100,
    score = 0
}

-- Third-Person Follow Camera
local cam = {
    target_x = 0.0, target_y = 1.5, target_z = 0.0,
    curr_x = 0.0, curr_y = 2.5, curr_z = 5.0,
    yaw = 180.0,
    pitch = 18.0,
    dist = 5.5,
    fov = 60.0
}

-- Environment & World Props
local ground_body = nil
local physics_props = {}
local enemies = {}
local textures = {}
local models = {}

-- Animation Names mapping
local anim_map = {
    idle = nil,
    walk = nil,
    run = nil,
    attack = nil
}
local current_anim = "idle"

-- Helper to find best matching animation name from model
local function detect_animations(names)
    print("[RPG] Inspecting character animations: " .. table.concat(names, ", "))
    for _, name in ipairs(names) do
        local lower = string.lower(name)
        if string.find(lower, "idle") and not anim_map.idle then
            anim_map.idle = name
        elseif string.find(lower, "walk") and not anim_map.walk then
            anim_map.walk = name
        elseif string.find(lower, "run") and not anim_map.run then
            anim_map.run = name
        elseif (string.find(lower, "attack") or string.find(lower, "slash") or string.find(lower, "hit") or string.find(lower, "shoot")) and not anim_map.attack then
            anim_map.attack = name
        end
    end

    -- Fallbacks if names don't match typical conventions
    if not anim_map.idle and #names >= 1 then anim_map.idle = names[1] end
    if not anim_map.walk and #names >= 2 then anim_map.walk = names[2] end
    if not anim_map.run and #names >= 3 then anim_map.run = names[3] end
    if not anim_map.attack and #names >= 4 then anim_map.attack = names[4] end
end

function crayon.init()
    crayon.window.setTitle("Crayon RPG - 3D Skeletal Character & Physics World")

    -- 1. Setup Graphics & Retro Aesthetics
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.setShadingMode("gouraud")
    crayon.graphics.setRetroEffects({
        jitterResolution = {320, 240},
        affine = 0.65,
        dither = true,
        colorDepth = 32,
        fog = {
            startDist = 12.0,
            endDist = 38.0,
            color = {0.14, 0.16, 0.22}
        }
    })

    -- Directional Sun Light
    crayon.graphics.setLight(-0.5, -1.0, 0.4, 1.0, 0.95, 0.88, 0.35, 0.35, 0.4)

    -- Point Light 0: Magic torch near fountain
    crayon.graphics.setPointLight(0, 0.0, 2.5, 0.0, 1.0, 0.65, 0.2, 8.0, 2.0)
    crayon.graphics.setPointLightEnabled(0, true)

    -- 2. Load Soldier.glb Character Model
    local model_paths = {
        "models/Soldier.glb",
        "game/assets/models/Soldier.glb",
        "game/assets/models/character.glb",
        "game/assets/models/box01.glb"
    }

    for _, p in ipairs(model_paths) do
        local m = crayon.graphics.loadModel(p)
        if m and m:isValid() then
            character_model = m
            print("[RPG] Successfully loaded model: " .. p)
            if m:isSkinned() then
                is_skinned = true
                animator = m:createAnimator()
                local anim_names = m:getAnimationNames()
                detect_animations(anim_names)
                if anim_map.idle then
                    animator:play(anim_map.idle, true, 1.0)
                    current_anim = "idle"
                end
                break
            end
        end
    end

    -- 3. Setup Physics World
    crayon.physics3d.setGravity(0, -18.0, 0)

    -- Static Ground Plane
    ground_body = crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 100.0)

    -- Player Character Physics Capsule: x, y, z, half_height, radius, motion, friction, restitution, density
    char_body = crayon.physics3d.createCapsule(player.x, player.y + 0.9, player.z, 0.5, 0.4, "dynamic", 0.8, 0.0, 200.0)
    if char_body and char_body:isValid() then
        -- Prevent player from toppling over like a bowling pin
        char_body:setAngularVelocity(0, 0, 0)
        char_body:setDamping(0.3, 1.0)
    end

    -- 4. Spawn RPG Town / Arena Props
    -- Arena Outer Pillars & Ruins
    for angle = 0, 315, 45 do
        local rad = math.rad(angle)
        local px = math.cos(rad) * 16.0
        local pz = math.sin(rad) * 16.0
        local pillar = crayon.physics3d.createCylinder(px, 3.0, pz, 3.0, 0.8, "static", 0.6, 0.1)
        table.insert(physics_props, { body = pillar, type = "pillar", x = px, y = 3.0, z = pz, r = 0.8, h = 6.0, col = {0.6, 0.65, 0.7} })
    end

    -- Dynamic Physics Crates & Chests to smash
    local crate_positions = {
        {-3, 1.0, -4}, {-2.2, 1.0, -4}, {-2.6, 2.0, -4},
        { 4, 1.0, -3}, { 4.8, 1.0, -3},
        {-5, 1.0,  3}, {-5, 2.0,  3},
        { 3, 1.0,  5}, { 3.8, 1.0,  5}
    }

    for i, cp in ipairs(crate_positions) do
        local box = crayon.physics3d.createBox(cp[1], cp[2], cp[3], 0.45, 0.45, 0.45, "dynamic", 0.7, 0.2, 50.0)
        table.insert(physics_props, {
            body = box,
            type = "box",
            size = {0.9, 0.9, 0.9},
            col = {0.78 + (i % 3) * 0.06, 0.55 + (i % 2) * 0.05, 0.35}
        })
    end

    -- Interactive Target Dummies / Slimes (Spheres)
    local target_spots = {
        {x = -6.0, z = -6.0},
        {x =  6.0, z = -7.0},
        {x = -7.0, z =  6.0},
        {x =  7.0, z =  7.0}
    }

    for idx, spot in ipairs(target_spots) do
        local enemy_body = crayon.physics3d.createSphere(spot.x, 1.0, spot.z, 0.65, "dynamic", 0.5, 0.4, 80.0)
        table.insert(enemies, {
            body = enemy_body,
            x = spot.x, y = 1.0, z = spot.z,
            hp = 3,
            max_hp = 3,
            radius = 0.65,
            hit_flash = 0.0
        })
    end
end

function crayon.update(dt)
    -- -------------------------------------------------------------
    -- 1. Camera Orbit & Mouse Input
    -- -------------------------------------------------------------
    if crayon.input.isDown("mouse_right") or crayon.input.isDown("mouse_middle") then
        local dx, dy = crayon.input.getMouseDelta()
        cam.yaw = cam.yaw - dx * 0.35
        cam.pitch = math.max(5.0, math.min(75.0, cam.pitch - dy * 0.25))
    end

    -- Mouse Scroll Zoom
    local wheel = crayon.input.getMouseWheel()
    if wheel ~= 0 then
        cam.dist = math.max(2.5, math.min(18.0, cam.dist - wheel * 0.75))
    end

    -- -------------------------------------------------------------
    -- 2. Player Input & Locomotion
    -- -------------------------------------------------------------
    local move_fwd = 0.0
    local move_right = 0.0

    if crayon.input.isDown("w") then move_fwd = move_fwd + 1.0 end
    if crayon.input.isDown("s") then move_fwd = move_fwd - 1.0 end
    if crayon.input.isDown("a") then move_right = move_right - 1.0 end
    if crayon.input.isDown("d") then move_right = move_right + 1.0 end

    player.is_running = crayon.input.isDown("lshift")
    local current_speed = player.is_running and player.run_speed or player.walk_speed

    -- Calculate movement direction relative to Camera Yaw
    local cam_rad = math.rad(cam.yaw)
    local cam_fwd_x = math.sin(cam_rad)
    local cam_fwd_z = math.cos(cam_rad)
    local cam_right_x = -cam_fwd_z
    local cam_right_z = cam_fwd_x

    local world_move_x = (cam_fwd_x * move_fwd + cam_right_x * move_right)
    local world_move_z = (cam_fwd_z * move_fwd + cam_right_z * move_right)
    local move_len = math.sqrt(world_move_x * world_move_x + world_move_z * world_move_z)

    local target_vx = 0.0
    local target_vz = 0.0

    if move_len > 0.001 then
        world_move_x = (world_move_x / move_len)
        world_move_z = (world_move_z / move_len)
        target_vx = world_move_x * current_speed
        target_vz = world_move_z * current_speed

        -- Smooth Character Rotation toward movement vector
        player.target_rot_y = math.deg(math.atan2(world_move_x, world_move_z))
    end

    -- Smooth angular turning
    local angle_diff = (player.target_rot_y - player.rot_y)
    while angle_diff > 180.0 do angle_diff = angle_diff - 360.0 end
    while angle_diff < -180.0 do angle_diff = angle_diff + 360.0 end
    player.rot_y = player.rot_y + angle_diff * math.min(1.0, dt * 14.0)

    -- -------------------------------------------------------------
    -- 3. Physics & Character Velocity Update
    -- -------------------------------------------------------------
    if char_body and char_body:isValid() then
        local px, py, pz = char_body:getPosition()
        local vx, vy, vz = char_body:getVelocity()

        -- Ground check raycast
        local hit, hx, hy, hz = crayon.physics3d.raycast(px, py + 0.1, pz, 0, -1, 0, 0.8)
        player.is_grounded = (hit == true)

        -- Jump (SPACE)
        if crayon.input.isPressed("space") and player.is_grounded then
            vy = 7.5
            char_body:applyImpulse(0, 350.0, 0)
        end

        -- Attack Action (Left Mouse or 'F' or 'E')
        if (crayon.input.isMouseDown(1) or crayon.input.isPressed("f") or crayon.input.isPressed("e")) and not player.is_attacking then
            player.is_attacking = true
            player.attack_timer = 0.65
            if animator and anim_map.attack then
                animator:crossFade(anim_map.attack, 0.15, false)
                current_anim = "attack"
            end

            -- Combat Hit Detection: Swing arc in front of character
            local attack_range = 2.4
            local rad = math.rad(player.rot_y)
            local fx = math.sin(rad)
            local fz = math.cos(rad)

            -- Hit nearby enemies
            for _, e in ipairs(enemies) do
                if e.hp > 0 and e.body and e.body:isValid() then
                    local ex, ey, ez = e.body:getPosition()
                    local dx = ex - px
                    local dz = ez - pz
                    local dist = math.sqrt(dx * dx + dz * dz)
                    if dist <= attack_range then
                        -- Check if target is roughly in front
                        local dot = (dx * fx + dz * fz) / (dist > 0 and dist or 1)
                        if dot > 0.3 then
                            e.hp = e.hp - 1
                            e.hit_flash = 0.25
                            player.score = player.score + 50
                            -- Pushback impulse
                            e.body:applyImpulse(fx * 300.0, 150.0, fz * 300.0)
                            print("[RPG] Hit Enemy! HP remaining: " .. e.hp)
                        end
                    end
                end
            end

            -- Hit and kick dynamic physics props
            for _, p in ipairs(physics_props) do
                if p.type == "box" and p.body and p.body:isValid() then
                    local bx, by, bz = p.body:getPosition()
                    local dx = bx - px
                    local dz = bz - pz
                    local dist = math.sqrt(dx * dx + dz * dz)
                    if dist <= attack_range + 0.5 then
                        p.body:applyImpulse(fx * 250.0, 120.0, fz * 250.0)
                    end
                end
            end
        end

        -- Attack duration timer
        if player.is_attacking then
            player.attack_timer = player.attack_timer - dt
            if player.attack_timer <= 0 then
                player.is_attacking = false
            end
        end

        -- Apply horizontal linear velocity with acceleration
        local accel = player.is_grounded and 16.0 or 6.0
        vx = vx + (target_vx - vx) * math.min(1.0, dt * accel)
        vz = vz + (target_vz - vz) * math.min(1.0, dt * accel)

        char_body:setVelocity(vx, vy, vz)
        char_body:setAngularVelocity(0, 0, 0) -- maintain vertical stance

        -- Sync player position
        player.x = px
        player.y = py - 0.5 -- feet position
        player.z = pz
        player.vx = vx
        player.vy = vy
        player.vz = vz
    end

    -- -------------------------------------------------------------
    -- 4. Animation State Machine (Idle <-> Walk <-> Run <-> Attack)
    -- -------------------------------------------------------------
    if animator and not player.is_attacking then
        local horiz_speed = math.sqrt(player.vx * player.vx + player.vz * player.vz)
        local target_anim = "idle"

        if horiz_speed > 4.5 and anim_map.run then
            target_anim = "run"
        elseif horiz_speed > 0.4 and anim_map.walk then
            target_anim = "walk"
        else
            target_anim = "idle"
        end

        if target_anim ~= current_anim then
            current_anim = target_anim
            local clip = anim_map[current_anim] or anim_map.idle
            if clip then
                animator:crossFade(clip, 0.2, true)
            end
        end

        -- Dynamic playback speed matching character velocity
        if current_anim == "walk" then
            animator:setSpeed(math.max(0.6, horiz_speed / player.walk_speed))
        elseif current_anim == "run" then
            animator:setSpeed(math.max(0.8, horiz_speed / player.run_speed))
        else
            animator:setSpeed(1.0)
        end
    end

    if animator then
        animator:update(dt)
    end

    -- -------------------------------------------------------------
    -- 5. Third-Person Follow Camera Tracking
    -- -------------------------------------------------------------
    -- Look target tracks slightly above player pelvis
    cam.target_x = cam.target_x + (player.x - cam.target_x) * math.min(1.0, dt * 10.0)
    cam.target_y = cam.target_y + (player.y + 1.4 - cam.target_y) * math.min(1.0, dt * 10.0)
    cam.target_z = cam.target_z + (player.z - cam.target_z) * math.min(1.0, dt * 10.0)

    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local desired_x = cam.target_x - cam.dist * math.cos(rad_pitch) * math.sin(rad_yaw)
    local desired_y = cam.target_y + cam.dist * math.sin(rad_pitch)
    local desired_z = cam.target_z - cam.dist * math.cos(rad_pitch) * math.cos(rad_yaw)

    -- Smooth camera damping
    cam.curr_x = cam.curr_x + (desired_x - cam.curr_x) * math.min(1.0, dt * 14.0)
    cam.curr_y = cam.curr_y + (desired_y - cam.curr_y) * math.min(1.0, dt * 14.0)
    cam.curr_z = cam.curr_z + (desired_z - cam.curr_z) * math.min(1.0, dt * 14.0)

    -- Update Enemy Flash Timers
    for _, e in ipairs(enemies) do
        if e.hit_flash > 0 then
            e.hit_flash = e.hit_flash - dt
        end
    end
end

function crayon.draw()
    crayon.graphics.clear(0.14, 0.16, 0.22, 1.0)

    -- 1. Setup 3D Camera
    crayon.graphics.setCamera3d({
        position = {cam.curr_x, cam.curr_y, cam.curr_z},
        target = {cam.target_x, cam.target_y, cam.target_z},
        up = {0, 1, 0},
        fov = cam.fov,
        near = 0.1,
        far = 150.0
    })

    -- 2. Draw Floor Arena Grid
    crayon.graphics.setColor(0.3, 0.35, 0.45, 1.0)
    crayon.graphics.drawGrid3d(40.0, 40, 0.0)

    -- Central Plaza Ring
    crayon.graphics.setColor(0.5, 0.55, 0.65, 1.0)
    crayon.graphics.drawCylinder(0, 0.05, 0, 8.0, 0.1, nil)

    -- 3. Draw Town Ruins / Pillars
    for _, prop in ipairs(physics_props) do
        if prop.type == "pillar" then
            crayon.graphics.setColor(prop.col[1], prop.col[2], prop.col[3], 1.0)
            crayon.graphics.drawCylinder(prop.x, prop.y, prop.z, prop.r, prop.h, nil)
            -- Capital atop pillar
            crayon.graphics.drawCube(prop.x, prop.y + prop.h * 0.5, prop.z, prop.r * 2.4, 0.4, prop.r * 2.4)
        elseif prop.type == "box" and prop.body and prop.body:isValid() then
            local bx, by, bz = prop.body:getPosition()
            local qx, qy, qz, qw = prop.body:getRotation()
            crayon.graphics.setColor(prop.col[1], prop.col[2], prop.col[3], 1.0)
            crayon.graphics.drawCube(bx, by, bz, prop.size[1], prop.size[2], prop.size[3])
        end
    end

    -- 4. Draw Interactive Target Dummies / Enemies
    for _, e in ipairs(enemies) do
        if e.body and e.body:isValid() then
            local ex, ey, ez = e.body:getPosition()
            if e.hit_flash > 0 then
                crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0) -- Flash white on hit
            elseif e.hp <= 0 then
                crayon.graphics.setColor(0.3, 0.3, 0.35, 1.0) -- Defeated
            else
                crayon.graphics.setColor(0.85, 0.25, 0.3, 1.0) -- Active enemy
            end
            crayon.graphics.drawSphere(ex, ey, ez, e.radius)

            -- HP Bar billboard
            if e.hp > 0 then
                crayon.graphics.setColor(0.1, 0.1, 0.1, 0.8)
                crayon.graphics.drawBillboard(ex, ey + 0.9, ez, 0.9, 0.15)
                crayon.graphics.setColor(0.2, 0.85, 0.3, 1.0)
                crayon.graphics.drawBillboard(ex - 0.45 * (1.0 - e.hp / e.max_hp), ey + 0.9, ez, 0.85 * (e.hp / e.max_hp), 0.1)
            end
        end
    end

    -- 5. Draw Player Character Model (Skinned glTF Mesh with Animator)
    if character_model and character_model:isValid() then
        crayon.graphics.setColor(1, 1, 1, 1)

        -- Soldier model glTF default orientation and scale adjustment
        local model_scale = 1.0
        local y_rot = player.rot_y

        if is_skinned and animator then
            character_model:drawSkinned(
                animator,
                player.x, player.y, player.z,
                0, y_rot, 0,
                model_scale, model_scale, model_scale
            )
        else
            character_model:draw(
                player.x, player.y, player.z,
                0, y_rot, 0,
                model_scale, model_scale, model_scale
            )
        end
    else
        -- Fallback Mannequin if model fails to load
        crayon.graphics.setColor(0.2, 0.5, 0.9, 1.0)
        crayon.graphics.drawCapsule(player.x, player.y + 0.9, player.z, 0.4, 0.5)
    end

    -- 6. HUD / UI Display
    crayon.graphics.resetCamera2d()

    -- Title & Stats Bar
    crayon.graphics.setColor(0, 0, 0, 0.55)
    crayon.graphics.drawRect("fill", 10, 10, 260, 105)

    crayon.graphics.setColor(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.drawText("=== CRAYON 3D ACTION RPG ===", 20, 18, 1)

    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("Action: " .. string.upper(current_anim), 20, 35, 1)
    crayon.graphics.drawText("Score:  " .. tostring(player.score), 20, 50, 1)
    crayon.graphics.drawText("FPS:    " .. tostring(crayon.window.getFps()), 20, 65, 1)

    -- Controls legend
    crayon.graphics.setColor(0.8, 0.85, 0.9, 1.0)
    crayon.graphics.drawText("WASD: Move  |  LSHIFT: Run  |  SPACE: Jump", 20, 80, 1)
    crayon.graphics.drawText("Left Mouse / F: Attack  |  Right Mouse: Orbit Cam", 20, 95, 1)

    -- Player Health Bar
    crayon.graphics.setColor(0.1, 0.1, 0.1, 0.8)
    crayon.graphics.drawRect("fill", 15, 122, 160, 16)
    crayon.graphics.setColor(0.9, 0.2, 0.25, 1.0)
    crayon.graphics.drawRect("fill", 17, 124, 156 * (player.health / player.max_health), 12)
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("HP: " .. player.health .. "/" .. player.max_health, 25, 125, 1)
    -- crayon.physics3d.drawDebug(0.3, 1.0, 0.4, 1.0, 0.6, 0.6, 0.8, 0.8)
end
