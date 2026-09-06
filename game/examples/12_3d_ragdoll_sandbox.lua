-- ============================================================================
-- Example 12: High-Performance 3D Ragdoll Physics Showcase
-- Articulated skeletal ragdolls, stairs tumbling, cannonball blast & zero-G
-- ============================================================================

local cam = {
    x = 0.0, y = 7.0, z = 16.0,
    yaw = -90.0, pitch = -18.0,
    fov = 60.0
}

local models = {}
local textures = {}
local ragdolls = {}     -- list of ragdoll instances
local cannonballs = {}  -- list of launched spheres
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
    local pelvis_id = crayon.physics.create_box(x, y + 1.8 * scale, z, pelvis_w * 0.5, pelvis_h * 0.5, pelvis_d * 0.5, "dynamic", friction, restitution, density)
    table.insert(rd.parts, { id = pelvis_id, type = "box", sx = pelvis_w, sy = pelvis_h, sz = pelvis_d, col = col_legs })

    -- 2. Torso / Chest
    local torso_w, torso_h, torso_d = 0.6 * scale, 0.6 * scale, 0.4 * scale
    local torso_id = crayon.physics.create_box(x, y + 2.3 * scale, z, torso_w * 0.5, torso_h * 0.5, torso_d * 0.5, "dynamic", friction, restitution, density)
    table.insert(rd.parts, { id = torso_id, type = "box", sx = torso_w, sy = torso_h, sz = torso_d, col = col_torso })

    -- Spine Joint (Pelvis <-> Torso)
    local spine_c = crayon.physics.create_point_constraint(pelvis_id, torso_id, x, y + 2.0 * scale, z)
    table.insert(rd.constraints, spine_c)

    -- 3. Head
    local head_r = 0.25 * scale
    local head_id = crayon.physics.create_sphere(x, y + 2.85 * scale, z, head_r, "dynamic", friction, restitution, 700.0)
    table.insert(rd.parts, { id = head_id, type = "sphere", r = head_r, col = col_skin })

    -- Neck Joint (Torso <-> Head)
    local neck_c = crayon.physics.create_point_constraint(torso_id, head_id, x, y + 2.6 * scale, z)
    table.insert(rd.constraints, neck_c)

    -- 4. Arms (Left & Right)
    local function create_arm(side)
        local arm_x = x + side * (torso_w * 0.5 + 0.2 * scale)
        local arm_r = 0.12 * scale
        local arm_len = 0.35 * scale

        -- Upper Arm
        local uarm_id = crayon.physics.create_capsule(arm_x, y + 2.2 * scale, z, arm_len * 0.5, arm_r, "dynamic", friction, restitution, density)
        table.insert(rd.parts, { id = uarm_id, type = "capsule", len = arm_len, r = arm_r, col = col_torso })

        -- Shoulder Joint (Torso <-> Upper Arm)
        local shoulder_c = crayon.physics.create_point_constraint(torso_id, uarm_id, x + side * torso_w * 0.5, y + 2.45 * scale, z)
        table.insert(rd.constraints, shoulder_c)

        -- Lower Arm / Forearm
        local larm_id = crayon.physics.create_capsule(arm_x, y + 1.7 * scale, z, arm_len * 0.5, arm_r * 0.9, "dynamic", friction, restitution, density)
        table.insert(rd.parts, { id = larm_id, type = "capsule", len = arm_len, r = arm_r * 0.9, col = col_skin })

        -- Elbow Hinge Joint (Upper Arm <-> Lower Arm)
        local elbow_c = crayon.physics.create_hinge_constraint(uarm_id, larm_id, arm_x, y + 1.95 * scale, z, 1.0, 0.0, 0.0, 0.0, math.rad(130))
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
        local thigh_id = crayon.physics.create_capsule(leg_x, y + 1.35 * scale, z, thigh_len * 0.5, leg_r, "dynamic", friction, restitution, density)
        table.insert(rd.parts, { id = thigh_id, type = "capsule", len = thigh_len, r = leg_r, col = col_legs })

        -- Hip Joint (Pelvis <-> Thigh)
        local hip_c = crayon.physics.create_point_constraint(pelvis_id, thigh_id, leg_x, y + 1.65 * scale, z)
        table.insert(rd.constraints, hip_c)

        -- Calf (Lower Leg)
        local calf_id = crayon.physics.create_capsule(leg_x, y + 0.75 * scale, z, calf_len * 0.5, leg_r * 0.85, "dynamic", friction, restitution, density)
        table.insert(rd.parts, { id = calf_id, type = "capsule", len = calf_len, r = leg_r * 0.85, col = col_shoes })

        -- Knee Hinge Joint (Thigh <-> Calf, bends backwards)
        local knee_c = crayon.physics.create_hinge_constraint(thigh_id, calf_id, leg_x, y + 1.05 * scale, z, 1.0, 0.0, 0.0, math.rad(-130), 0.0)
        table.insert(rd.constraints, knee_c)
    end

    create_leg(-1.0) -- Left Leg
    create_leg( 1.0) -- Right Leg

    table.insert(ragdolls, rd)
    return rd
end

local function reset_scene()
    crayon.physics.destroy_all()
    ragdolls = {}
    cannonballs = {}

    -- Ground Floor
    crayon.physics.create_plane(0, 0, 0, 0, 1, 0, 60.0)

    -- Giant Tumbling Stairs on the Left
    local num_steps = 14
    local step_h = 0.65
    local step_d = 1.0
    local step_w = 9.0
    for i = 1, num_steps do
        local sx = -4.0
        local sy = (i - 1) * step_h + step_h * 0.5
        local sz = -(i - 1) * step_d + 3.0
        crayon.physics.create_box(sx, sy, sz, step_w * 0.5, step_h * 0.5, step_d * 0.5, "static", 0.6, 0.2)
    end

    -- Top platform
    local top_y = num_steps * step_h
    local top_z = -(num_steps - 1) * step_d + 3.0 - 2.0
    crayon.physics.create_box(-4.0, top_y - 0.2, top_z, 4.5, 0.4, 3.0, "static", 0.6, 0.2)

    -- Obstacle / Bowling Pins on the Right
    for px = -1, 1 do
        for pz = -1, 1 do
            local bx = 6.0 + px * 1.5
            local bz = pz * 1.5
            crayon.physics.create_cylinder(bx, 1.2, bz, 1.0, 0.35, "dynamic", 0.6, 0.2)
        end
    end

    -- Spawn Ragdoll #1 at the top of the stairs
    local rd1 = spawn_ragdoll(-4.0, top_y + 0.2, top_z, 1.1)
    -- Give Ragdoll #1 a forward push to start tumbling down the stairs!
    crayon.physics.apply_impulse(rd1.parts[1].id, 0.0, 5.0, 18.0)

    -- Spawn Ragdoll #2 on the ground platform
    spawn_ragdoll(2.0, 0.2, 2.0, 1.0)
    -- Spawn Ragdoll #3 near the pins
    spawn_ragdoll(6.0, 0.2, -4.0, 0.95)
end

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("Crayon Engine - Jolt 3D Physics Ragdoll Simulator")

    models.cube = crayon.graphics.load_model("cube")
    models.sphere = crayon.graphics.load_model("sphere")
    models.cylinder = crayon.graphics.load_model("cylinder")
    models.capsule = crayon.graphics.load_model("capsule")
    models.plane = crayon.graphics.load_model("plane")

    textures.grass = crayon.graphics.load_texture("game/assets/textures/grass.bmp")
    textures.brick = crayon.graphics.load_texture("game/assets/textures/brick.bmp")

    crayon.graphics.set_retro_effects({
        jitter_resolution = {160, 120},
        affine = 0.75,
        dither = true,
        fog = { start = 14, ["end"] = 45, color = {0.09, 0.10, 0.17} }
    })

    crayon.graphics.set_light(-0.5, -0.9, -0.6, 1.0, 0.95, 0.88, 0.35, 0.35, 0.42)
    reset_scene()
end

function crayon.update(dt)
    -- Camera Orbit & Movement
    local move_speed = 8.0 * dt
    local rad_yaw = math.rad(cam.yaw)
    local fwd_x, fwd_z = math.cos(rad_yaw), math.sin(rad_yaw)
    local right_x, right_z = -fwd_z, fwd_x

    if crayon.input.is_down("w") then cam.x = cam.x + fwd_x * move_speed; cam.z = cam.z + fwd_z * move_speed end
    if crayon.input.is_down("s") then cam.x = cam.x - fwd_x * move_speed; cam.z = cam.z - fwd_z * move_speed end
    if crayon.input.is_down("a") then cam.x = cam.x - right_x * move_speed; cam.z = cam.z - right_z * move_speed end
    if crayon.input.is_down("d") then cam.x = cam.x + right_x * move_speed; cam.z = cam.z + right_z * move_speed end
    if crayon.input.is_down("q") or crayon.input.is_down("space") then cam.y = cam.y + move_speed end
    if crayon.input.is_down("z") or crayon.input.is_down("lshift") then cam.y = cam.y - move_speed end

    if crayon.input.is_down("left") then cam.yaw = cam.yaw - 85.0 * dt end
    if crayon.input.is_down("right") then cam.yaw = cam.yaw + 85.0 * dt end
    if crayon.input.is_down("up") then cam.pitch = math.min(cam.pitch + 60.0 * dt, 80.0) end
    if crayon.input.is_down("down") then cam.pitch = math.max(cam.pitch - 60.0 * dt, -80.0) end

    local rad_pitch = math.rad(cam.pitch)
    local look_x = math.cos(rad_pitch) * math.cos(rad_yaw)
    local look_y = math.sin(rad_pitch)
    local look_z = math.cos(rad_pitch) * math.sin(rad_yaw)

    -- Shoot Heavy Cannonball on Click or 'F'
    if crayon.input.is_mouse_down(1) or crayon.input.is_pressed("f") then
        local r = 0.5
        local bid = crayon.physics.create_sphere(cam.x + look_x * 1.5, cam.y + look_y * 1.5, cam.z + look_z * 1.5, r, "dynamic", 0.5, 0.4, 2500.0)
        local v = 45.0
        crayon.physics.set_velocity(bid, look_x * v, look_y * v, look_z * v)
        table.insert(cannonballs, { id = bid, r = r })
    end

    -- Spawn a new Ragdoll in front of camera on 'B'
    if crayon.input.is_pressed("b") then
        local rx = cam.x + look_x * 6.0
        local ry = math.max(cam.y + look_y * 6.0, 1.5)
        local rz = cam.z + look_z * 6.0
        local new_rd = spawn_ragdoll(rx, ry, rz, 0.85 + math.random() * 0.3)
        -- Random tumble impulse
        crayon.physics.apply_impulse(new_rd.parts[1].id, (math.random() - 0.5) * 20.0, 10.0, (math.random() - 0.5) * 20.0)
    end

    -- Radial Blast / Explosion on 'E' using Overlap Sphere Query!
    if crayon.input.is_pressed("e") then
        local blast_pos = {cam.x + look_x * 8.0, cam.y + look_y * 8.0, cam.z + look_z * 8.0}
        local blast_radius = 12.0
        local nearby_bodies = crayon.physics.overlap_sphere(blast_pos[1], blast_pos[2], blast_pos[3], blast_radius)

        for _, bid in ipairs(nearby_bodies) do
            local bx, by, bz = crayon.physics.get_position(bid)
            local dx, dy, dz = bx - blast_pos[1], by - blast_pos[2], bz - blast_pos[3]
            local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
            if dist > 0.01 then
                local force = (1.0 - dist / blast_radius) * 120.0
                crayon.physics.apply_impulse(bid, (dx / dist) * force, (dy / dist + 0.5) * force, (dz / dist) * force)
            end
        end
    end

    -- Zero-G toggle on 'G'
    if crayon.input.is_pressed("g") then
        zero_g = not zero_g
        if zero_g then
            crayon.physics.set_gravity(0.0, 0.0, 0.0)
        else
            crayon.physics.set_gravity(0.0, -9.81, 0.0)
        end
    end

    -- Wireframe Debug toggle on 'TAB'
    if crayon.input.is_pressed("tab") then
        show_debug = not show_debug
    end

    -- Reset Scene on 'R'
    if crayon.input.is_pressed("r") then
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

    crayon.graphics.set_camera3d({
        position = {cam.x, cam.y, cam.z},
        target = {target_x, target_y, target_z},
        up = {0, 1, 0},
        fov = cam.fov
    })

    -- Render Ground
    crayon.graphics.draw_model(models.plane, 0, 0, 0, 0, 0, 0, 9.0, 1.0, 9.0, textures.grass)

    -- Render Tumbling Stairs
    local num_steps = 14
    local step_h = 0.65
    local step_d = 1.0
    local step_w = 9.0
    for i = 1, num_steps do
        local sx = -4.0
        local sy = (i - 1) * step_h + step_h * 0.5
        local sz = -(i - 1) * step_d + 3.0
        crayon.graphics.draw_model(models.cube, sx, sy, sz, 0, 0, 0, step_w, step_h, step_d, textures.brick)
    end
    local top_y = num_steps * step_h
    local top_z = -(num_steps - 1) * step_d + 3.0 - 2.0
    crayon.graphics.draw_model(models.cube, -4.0, top_y - 0.2, top_z, 0, 0, 0, 9.0, 0.4, 6.0, textures.brick)

    -- Render Ragdolls
    for _, rd in ipairs(ragdolls) do
        for _, part in ipairs(rd.parts) do
            if crayon.physics.is_valid(part.id) then
                local px, py, pz = crayon.physics.get_position(part.id)
                local rx, ry, rz = crayon.physics.get_rotation(part.id)
                crayon.graphics.set_color(part.col[1], part.col[2], part.col[3], 1.0)

                if part.type == "box" then
                    crayon.graphics.draw_model(models.cube, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), part.sx, part.sy, part.sz)
                elseif part.type == "sphere" then
                    crayon.graphics.draw_model(models.sphere, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), part.r * 2.0, part.r * 2.0, part.r * 2.0)
                elseif part.type == "capsule" then
                    crayon.graphics.draw_model(models.cylinder, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), part.r * 2.0, part.len, part.r * 2.0)
                end
            end
        end
    end

    -- Render Cannonballs
    crayon.graphics.set_color(0.3, 0.3, 0.35, 1.0)
    for _, cb in ipairs(cannonballs) do
        if crayon.physics.is_valid(cb.id) then
            local px, py, pz = crayon.physics.get_position(cb.id)
            crayon.graphics.draw_model(models.sphere, px, py, pz, 0, 0, 0, cb.r * 2.0, cb.r * 2.0, cb.r * 2.0)
        end
    end
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)

    -- Debug Wireframes (optional)
    if show_debug then
        crayon.physics.draw_debug(0.3, 1.0, 0.4, 1.0, 0.6, 0.6, 0.8, 0.8)
    end

    -- 2D HUD Overlays
    crayon.graphics.set_color(0.0, 0.0, 0.0, 0.65)
    crayon.graphics.draw_rect("fill", 4, 4, 205, 54)
    crayon.graphics.set_color(0.3, 0.6, 1.0, 1.0)
    crayon.graphics.draw_rect("line", 4, 4, 205, 54)

    local total_bodies, active_bodies = crayon.physics.get_body_count()
    local fps = math.floor(crayon.window.get_fps() + 0.5)

    crayon.graphics.set_color(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.draw_text("3D JOLT RAGDOLL SHOWCASE", 8, 8, 1.0)
    crayon.graphics.set_color(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.draw_text("FPS: " .. fps .. " | Ragdolls: " .. #ragdolls .. " | Bodies: " .. active_bodies .. "/" .. total_bodies, 8, 18, 1.0)
    crayon.graphics.set_color(0.8, 0.85, 1.0, 1.0)
    crayon.graphics.draw_text("Click/F: Heavy Cannonball | B: Spawn Ragdoll", 8, 28, 1.0)
    crayon.graphics.draw_text("E: AOE Blast | G: Zero-G [" .. (zero_g and "ON" or "OFF") .. "] | TAB: Wire | R: Reset", 8, 38, 1.0)

    -- Aiming Reticle
    crayon.graphics.set_color(1.0, 1.0, 1.0, 0.8)
    crayon.graphics.draw_line(156, 120, 164, 120, 1.0)
    crayon.graphics.draw_line(160, 116, 160, 124, 1.0)
end
