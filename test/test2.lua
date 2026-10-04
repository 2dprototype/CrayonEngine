local cam = {
    x = 0.0, y = 25.0, z = 15.0,
    yaw = -90.0, pitch = -60.0,
    fov = 55.0, shake = 0.0
}

local models = {}
local bodies = {}
local projectiles = {}
local explosions = {}
local particles = {} -- Procedural shrapnel
local score = 0
local tex_white = 0

-- Cinematic FX Globals
local time_scale = 1.0
local flash_alpha = 0.0

local WEAPONS = {
    cannon = { speed = 120.0, radius = 6.0, force = 80.0, cooldown = 0.08, drop_height = 0, color = {1.0, 0.9, 0.4}, hitstop = 0.0 },
    bomb   = { speed = 40.0, radius = 25.0, force = 400.0, cooldown = 1.5, drop_height = 40.0, color = {1.0, 0.2, 0.05}, hitstop = 0.1 }
}
local weapon_timers = { cannon = 0, bomb = 0 }

-- Helpers
local function spawn_box(x, y, z, w, h, d, motion, mass, color, is_target)
    local id = crayon.physics.create_box(x, y, z, w*0.5, h*0.5, d*0.5, motion, 0.8, 0.4, mass)
    table.insert(bodies, { id = id, type = "cube", sx = w, sy = h, sz = d, color = color, is_target = is_target })
    return id
end

local function spawn_cylinder(x, y, z, r, h, motion, mass, color, is_target)
    local id = crayon.physics.create_cylinder(x, y, z, h*0.5, r, motion, 0.8, 0.4, mass)
    table.insert(bodies, { id = id, type = "cylinder", sx = r*2, sy = h, sz = r*2, color = color, is_target = is_target })
    return id
end

local function spawn_monkey(x, y, z, w, h, d, motion, mass, color, is_target)
    local id = crayon.physics.create_box(x, y, z, w*0.5, h*0.5, d*0.5, motion, 0.8, 0.4, mass)
    table.insert(bodies, { id = id, type = "monkey", sx = w, sy = h, sz = d, color = color, is_target = is_target })
    return id
end

local function build_military_base()
    crayon.physics.destroy_all()
    bodies = {}
    crayon.physics.create_plane(0, 0, 0, 0, 1, 0, 100.0)
    
    -- Main Hangar
    spawn_box( 10, 2, -10, 1, 4, 15, "static", 0, {0.2, 0.25, 0.2}, false)
    spawn_box( 20, 2, -10, 1, 4, 15, "static", 0, {0.2, 0.25, 0.2}, false)
    spawn_box( 15, 2, -17, 10, 4, 1, "static", 0, {0.2, 0.25, 0.2}, false)
    for i = 0, 5 do
        spawn_box(10 + i*2, 4.5, -10, 1.8, 0.5, 15, "dynamic", 80, {0.15, 0.2, 0.15}, true)
    end

    -- Explosive Fuel Depot Cluster
    for fx = 0, 2 do
        for fz = 0, 2 do
            spawn_cylinder(-15 + fx*2, 2, -5 + fz*2, 0.8, 3.5, "dynamic", 30, {0.9, 0.1, 0.1}, true)
        end
    end

    -- Dense Watchtowers
    local tower_positions = { {-8, 8}, {12, 18}, {-22, -12} }
    for _, pos in ipairs(tower_positions) do
        for h = 0, 4 do
            spawn_box(pos[1], 1 + h*2, pos[2], 2, 2, 2, "dynamic", 60, {0.3, 0.3, 0.35}, true)
        end
        -- spawn_cylinder(pos[1], 11, pos[2], 1.0, 1.5, "dynamic", 20, {0.1, 0.1, 0.1}, true)
		spawn_monkey(pos[1], 11, pos[2], 1, 1, 1, "dynamic", 10, {0.3, 0.3, 0.35}, true)
    end
	
	-- spawn_monkey(-8, 8, 4, 2, 2, 2, "dynamic", 10, {0.3, 0.3, 0.35}, true)
end

local function spawn_shrapnel(x, y, z, count, force)
    for i = 1, count do
        local vx = (math.random() - 0.5) * force
        local vy = (math.random() * force) + (force * 0.5) -- Force upwards
        local vz = (math.random() - 0.5) * force
        table.insert(particles, {
            x = x, y = y, z = z,
            vx = vx, vy = vy, vz = vz,
            rx = math.random(0, 360), ry = math.random(0, 360), rz = math.random(0, 360),
            rvx = math.random(-300, 300), rvy = math.random(-300, 300),
            size = math.random() * 0.4 + 0.1,
            life = math.random() * 1.5 + 0.5
        })
    end
end

local function trigger_explosion(ex, ey, ez, radius, force, color, hitstop)
    -- Cinematic Triggers
    if hitstop > 0 then
        time_scale = hitstop
        flash_alpha = 1.0 -- Screen flash for heavy impacts
    end
    cam.shake = cam.shake + (force * 0.01)

    -- Visual Layers
    table.insert(explosions, {type="sphere", x=ex, y=ey, z=ez, r=0, max_r=radius, life=1.0, color=color})
    table.insert(explosions, {type="shockwave", x=ex, y=ey+0.5, z=ez, r=0, max_r=radius*1.5, life=1.0, color={1, 1, 1}})
    
    spawn_shrapnel(ex, ey, ez, radius * 3, force * 0.15)

    -- Violent Physics Displacement
    local nearby = crayon.physics.overlap_sphere(ex, ey, ez, radius)
    for _, bid in ipairs(nearby) do
        local bx, by, bz = crayon.physics.get_position(bid)
        local dx, dy, dz = bx - ex, by - ey, bz - ez
        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
        if dist < 0.1 then dist = 0.1 end
        
        if dist <= radius then
            -- Exponential falloff for closer, more violent impact
            local f_mult = math.pow(1.0 - (dist / radius), 2) * force
            
            -- Upward Bias (Launch things into the air)
            local vy_bias = 1.5 
            
            -- Apply unhinged directional force
            crayon.physics.apply_impulse(bid, (dx/dist)*f_mult, ((dy/dist) + vy_bias)*f_mult, (dz/dist)*f_mult)
            
            -- Add some rotational chaos (simulated by applying tiny offset impulses if engine supports it, 
            -- otherwise the raw force sheer will usually tumble the physics boxes)
        end
    end
end

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("Kinetic Art: Strike Protocol")
    
    models.cube = crayon.graphics.load_model("cube")
    models.cylinder = crayon.graphics.load_model("cylinder")
    models.sphere = crayon.graphics.load_model("sphere")
    models.plane = crayon.graphics.load_model("plane")
    models.monkey = crayon.graphics.load_model("game/assets/models/monkey.obj")
	
    tex_white = crayon.graphics.get_white_texture()
    tex_grass = crayon.graphics.load_texture("game/assets/textures/grass.bmp")

    crayon.graphics.set_retro_effects({
        jitter_resolution = {400, 225}, affine = 0.9, dither = true,
        fog = { start = 30, ["end"] = 100, color = {0.05, 0.05, 0.06} }
    })
    crayon.graphics.set_light(0.5, -0.9, -0.3, 1.2, 1.1, 1.0, 0.1, 0.1, 0.15)
    build_military_base()
end

function crayon.update(raw_dt)
    -- Time Dilation Recovery (Hit-Stop effect)
    if time_scale < 1.0 then
        time_scale = math.min(1.0, time_scale + raw_dt * 1.5)
    end
    local dt = raw_dt * time_scale

    if flash_alpha > 0 then flash_alpha = math.max(0, flash_alpha - raw_dt * 2.0) end
    if cam.shake > 0 then cam.shake = math.max(0, cam.shake - raw_dt * 6.0) end

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
    
    local mx, my = crayon.input.get_mouse_pos()
    local ox, oy, oz, dx, dy, dz = crayon.graphics.unproject(mx, my)
    local hit, hx, hy, hz = crayon.physics.raycast(ox, oy, oz, dx, dy, dz, 300.0)
    cam.target_hit = hit
    cam.tx, cam.ty, cam.tz = hx, hy, hz

    weapon_timers.cannon = math.max(0, weapon_timers.cannon - dt)
    weapon_timers.bomb = math.max(0, weapon_timers.bomb - dt)

    if hit then
        if crayon.input.is_mouse_down(1) and weapon_timers.cannon == 0 then
            weapon_timers.cannon = WEAPONS.cannon.cooldown
            local bid = crayon.physics.create_sphere(cam.x, cam.y - 2, cam.z, 0.3, "dynamic", 0.1, 0.1, 100.0)
            local dir_len = math.sqrt((hx-cam.x)^2 + (hy-(cam.y-2))^2 + (hz-cam.z)^2)
            crayon.physics.set_velocity(bid, ((hx-cam.x)/dir_len)*WEAPONS.cannon.speed, ((hy-(cam.y-2))/dir_len)*WEAPONS.cannon.speed, ((hz-cam.z)/dir_len)*WEAPONS.cannon.speed)
            table.insert(projectiles, {id = bid, type = "cannon", last_y = cam.y, data = WEAPONS.cannon})
        end

        if crayon.input.is_mouse_down(3) and weapon_timers.bomb == 0 then
            weapon_timers.bomb = WEAPONS.bomb.cooldown
            local bid = crayon.physics.create_sphere(hx, hy + WEAPONS.bomb.drop_height, hz, 1.2, "dynamic", 0.5, 0.1, 1000.0)
            crayon.physics.set_velocity(bid, 0, -WEAPONS.bomb.speed, 0)
            table.insert(projectiles, {id = bid, type = "bomb", last_y = hy + WEAPONS.bomb.drop_height, data = WEAPONS.bomb})
        end
    end

    -- Process Projectiles
    for i = #projectiles, 1, -1 do
        local p = projectiles[i]
        if crayon.physics.is_valid(p.id) then
            local px, py, pz = crayon.physics.get_position(p.id)
            if py <= 0.5 or (py > p.last_y and py < p.last_y + 0.1) then
                trigger_explosion(px, py, pz, p.data.radius, p.data.force, p.data.color, p.data.hitstop)
                crayon.physics.destroy_body(p.id)
                table.remove(projectiles, i)
            else
                p.last_y = py
            end
        else
            table.remove(projectiles, i)
        end
    end

    -- Update Procedural Shrapnel (Custom lightweight physics loop)
    for i = #particles, 1, -1 do
        local p = particles[i]
        p.vy = p.vy - (40.0 * dt) -- Gravity
        p.x = p.x + p.vx * dt
        p.y = p.y + p.vy * dt
        p.z = p.z + p.vz * dt
        p.rx = p.rx + p.rvx * dt
        p.ry = p.ry + p.rvy * dt
        
        -- Ground bounce
        if p.y < 0.2 then
            p.y = 0.2
            p.vy = -p.vy * 0.5
            p.vx = p.vx * 0.8
            p.vz = p.vz * 0.8
        end
        
        p.life = p.life - dt
        if p.life <= 0 then table.remove(particles, i) end
    end

    -- Cleanup Map
    for i = #bodies, 1, -1 do
        local b = bodies[i]
        if crayon.physics.is_valid(b.id) then
            local bx, by, bz = crayon.physics.get_position(b.id)
            if by < -10.0 then
                if b.is_target then score = score + 150 end
                crayon.physics.destroy_body(b.id)
                table.remove(bodies, i)
            end
        else
            table.remove(bodies, i)
        end
    end

    if crayon.input.is_pressed("r") then build_military_base() end
end

function crayon.draw()
    crayon.graphics.clear(0.05, 0.05, 0.06)
    
	-- crayon.physics.draw_debug(0.3, 1.0, 0.4, 1.0, 0.6, 0.6, 0.8, 0.8)
    -- Cinematic Shake calculation
    local magnitude = cam.shake * (cam.shake * 0.5) -- Non-linear shake curve
    local sx = (math.random() - 0.5) * magnitude
    local sz = (math.random() - 0.5) * magnitude
    local sy = (math.random() - 0.5) * magnitude * 0.5

    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local tx = (cam.x + sx) + math.cos(rad_pitch) * math.cos(rad_yaw)
    local ty = (cam.y + sy) + math.sin(rad_pitch)
    local tz = (cam.z + sz) + math.cos(rad_pitch) * math.sin(rad_yaw)

    crayon.graphics.set_camera3d({
        position = {cam.x + sx, cam.y + sy, cam.z + sz},
        target = {tx, ty, tz}, fov = cam.fov + cam.shake * 0.2 -- FOV pumping on blasts
    })

    -- Ground
    crayon.graphics.set_color(0.1, 0.1, 0.12, 1.0)
    crayon.graphics.draw_model(models.plane, 0, 0, 0, 0, 0, 0, 35.0, 1.0, 35.0, tex_grass)

    -- Solid Bodies
    for _, b in ipairs(bodies) do
        if crayon.physics.is_valid(b.id) then
            local px, py, pz = crayon.physics.get_position(b.id)
            local rx, ry, rz = crayon.physics.get_rotation(b.id)
            crayon.graphics.set_color(b.color[1], b.color[2], b.color[3], 1.0)
            if b.type == "cube" then
                crayon.graphics.draw_model(models.cube, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), b.sx, b.sy, b.sz, tex_white)
            elseif b.type == "cylinder" then
                crayon.graphics.draw_model(models.cylinder, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), b.sx, b.sy, b.sz, tex_white)  
			elseif b.type == "monkey" then
                crayon.graphics.draw_model(models.monkey, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), b.sx, b.sy, b.sz, tex_white)
            end
        end
    end

    -- Shrapnel
    crayon.graphics.set_color(0.2, 0.2, 0.2, 1.0)
    for _, p in ipairs(particles) do
        crayon.graphics.draw_model(models.cube, p.x, p.y, p.z, math.rad(p.rx), math.rad(p.ry), math.rad(p.rz), p.size, p.size, p.size, tex_white)
    end

    -- Projectiles
    crayon.graphics.set_color(0.0, 0.0, 0.0, 1.0)
    for _, p in ipairs(projectiles) do
        if crayon.physics.is_valid(p.id) then
            local px, py, pz = crayon.physics.get_position(p.id)
            local r = p.type == "bomb" and 1.2 or 0.3
            crayon.graphics.draw_model(models.sphere, px, py, pz, 0, 0, 0, r*2, r*2, r*2, tex_white)
        end
    end

    -- Render Layered Explosions
    local raw_dt = crayon.time.get_dt()
    for i = #explosions, 1, -1 do
        local ex = explosions[i]
        ex.life = ex.life - (raw_dt * 3.0)
        ex.r = ex.r + ((ex.max_r - ex.r) * raw_dt * 10.0) -- Smooth damp expansion
        
        if ex.life <= 0 then
            table.remove(explosions, i)
        else
            crayon.graphics.set_color(ex.color[1], ex.color[2], ex.color[3], ex.life)
            if ex.type == "sphere" then
                crayon.graphics.draw_model(models.sphere, ex.x, ex.y, ex.z, 0, 0, 0, ex.r*2, ex.r*2, ex.r*2, tex_white)
            elseif ex.type == "shockwave" then
                -- Flat expanding cylinder
                crayon.graphics.draw_model(models.cylinder, ex.x, ex.y, ex.z, 0, 0, 0, ex.r*2, 0.2, ex.r*2, tex_white)
            end
        end
    end
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)

    -- Reticle
    if cam.target_hit then
        crayon.graphics.set_color(1.0, 0.1, 0.0, 0.8)
        crayon.graphics.draw_model(models.cylinder, cam.tx, cam.ty + 0.1, cam.tz, 0, 0, 0, 2.0, 0.1, 2.0, tex_white)
    end

    -- Full Screen Flash (Additive Overlay)
    if flash_alpha > 0 then
        crayon.graphics.set_color(1.0, 1.0, 1.0, flash_alpha)
        crayon.graphics.draw_rect("fill", 0, 0, 800, 450)
    end

    -- Minimalist UI
    crayon.graphics.set_color(1.0, 0.5, 0.0, 1.0)
    if time_scale < 1.0 then crayon.graphics.draw_text("TIME DILATION ACTIVE", 10, 10, 1.5) end
end