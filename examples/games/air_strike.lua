-- ============================================================================
-- Air Strike: Kinetic Art Revision
-- Fixed for the current Crayon API:
--   * crayon.physics.*       -> crayon.physics3D.*   (alias removed)
--   * module-level get_position(id)/is_valid(id)/set_velocity(id,...)/
--     apply_impulse(id,...)/destroy_body(id) -> Body methods
--   * crayon.input.*          -> crayon.key / crayon.mouse
--   * snake_case graphics/window/time functions -> strict camelCase
--   * retro fog keys start/end -> startDist/endDist
-- ============================================================================

local cam = {
    x = 0.0, y = 25.0, z = 15.0,
    yaw = -90.0, pitch = -60.0,
    fov = 55.0, shake = 0.0
}

local models = {}
local bodies = {}          -- { body = <Physics3D.Body>, type=, sx=, sy=, sz=, color=, is_target= }
local projectiles = {}     -- { body = <Physics3D.Body>, type=, last_y=, data= }
local explosions = {}
local particles = {}       -- Procedural shrapnel
local score = 0
local tex_white = nil

-- Cinematic FX globals
local time_scale = 1.0
local flash_alpha = 0.0

local WEAPONS = {
    cannon = { speed = 120.0, radius = 6.0,  force = 80.0,  cooldown = 0.08, drop_height = 0,    color = {1.0, 0.9, 0.4}, hitstop = 0.0 },
    bomb   = { speed = 40.0,  radius = 25.0, force = 400.0, cooldown = 1.5,  drop_height = 40.0, color = {1.0, 0.2, 0.05}, hitstop = 0.1 }
}
local weapon_timers = { cannon = 0, bomb = 0 }

-- ============================================================================
-- Spawners
-- ============================================================================
local function spawn_box(x, y, z, w, h, d, motion, mass, color, is_target)
    local body = crayon.physics3D.createBox(x, y, z, w * 0.5, h * 0.5, d * 0.5, motion, 0.8, 0.4, mass)
    table.insert(bodies, {
        body = body, type = "cube",
        sx = w, sy = h, sz = d,
        color = color, is_target = is_target
    })
    return body
end

local function spawn_cylinder(x, y, z, r, h, motion, mass, color, is_target)
    local body = crayon.physics3D.createCylinder(x, y, z, h * 0.5, r, motion, 0.8, 0.4, mass)
    table.insert(bodies, {
        body = body, type = "cylinder",
        sx = r * 2, sy = h, sz = r * 2,
        color = color, is_target = is_target
    })
    return body
end

local function build_military_base()
    crayon.physics3D.destroyAll()
    bodies = {}
    crayon.physics3D.createPlane(0, 0, 0, 0, 1, 0, 100.0)

    -- Main hangar
    spawn_box( 10, 2, -10, 1, 4, 15, "static", 0, {0.2, 0.25, 0.2}, false)
    spawn_box( 20, 2, -10, 1, 4, 15, "static", 0, {0.2, 0.25, 0.2}, false)
    spawn_box( 15, 2, -17, 10, 4, 1, "static", 0, {0.2, 0.25, 0.2}, false)
    for i = 0, 5 do
        spawn_box(10 + i * 2, 4.5, -10, 1.8, 0.5, 15, "dynamic", 80, {0.15, 0.2, 0.15}, true)
    end

    -- Explosive fuel depot cluster
    for fx = 0, 2 do
        for fz = 0, 2 do
            spawn_cylinder(-15 + fx * 2, 2, -5 + fz * 2, 0.8, 3.5, "dynamic", 30, {0.9, 0.1, 0.1}, true)
        end
    end

    -- Dense watchtowers
    local tower_positions = { {-8, 8}, {12, 18}, {-22, -12} }
    for _, pos in ipairs(tower_positions) do
        for h = 0, 4 do
            spawn_box(pos[1], 1 + h * 2, pos[2], 2, 2, 2, "dynamic", 60, {0.3, 0.3, 0.35}, true)
        end
        spawn_cylinder(pos[1], 11, pos[2], 1.0, 1.5, "dynamic", 20, {0.1, 0.1, 0.1}, true)
    end
end

-- ============================================================================
-- Explosions / Shrapnel
-- ============================================================================
local function spawn_shrapnel(x, y, z, count, force)
    for _ = 1, count do
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
    -- Cinematic triggers
    if hitstop > 0 then
        time_scale  = hitstop
        flash_alpha = 1.0
    end
    cam.shake = cam.shake + (force * 0.01)

    -- Visual layers
    table.insert(explosions, { type = "sphere",    x = ex, y = ey,       z = ez, r = 0, max_r = radius,       life = 1.0, color = color })
    table.insert(explosions, { type = "shockwave", x = ex, y = ey + 0.5, z = ez, r = 0, max_r = radius * 1.5, life = 1.0, color = {1, 1, 1} })

    spawn_shrapnel(ex, ey, ez, radius * 3, force * 0.15)

    -- Violent physics displacement
    local nearby = crayon.physics3D.overlapSphere(ex, ey, ez, radius)
    for _, body in ipairs(nearby) do
        if body and body:isValid() then
            local bx, by, bz = body:getPosition()
            local dx, dy, dz = bx - ex, by - ey, bz - ez
            local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
            if dist < 0.1 then dist = 0.1 end

            if dist <= radius then
                -- Exponential falloff for closer, more violent impact
                local f_mult = math.pow(1.0 - (dist / radius), 2) * force
                local vy_bias = 1.5 -- Upward bias (launch things into the air)
                body:applyImpulse(
                    (dx / dist) * f_mult,
                    ((dy / dist) + vy_bias) * f_mult,
                    (dz / dist) * f_mult
                )
            end
        end
    end
end

-- ============================================================================
-- Engine callbacks
-- ============================================================================
function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("Air Strike")
    crayon.window.setScalingMode("integer")

    math.randomseed(os.time and os.time() or 12345)

    models.cube     = crayon.graphics.loadModel("cube")
    models.cylinder = crayon.graphics.loadModel("cylinder")
    models.sphere   = crayon.graphics.loadModel("sphere")
    models.plane    = crayon.graphics.loadModel("plane")
    tex_white       = crayon.graphics.getWhiteTexture()

    crayon.graphics.setRetroEffects({
        jitterResolution = {400, 225},
        affine = 0.9,
        dither = true,
        fog = { startDist = 30, endDist = 100, color = {0.05, 0.05, 0.06} }
    })
    crayon.graphics.setLight(0.5, -0.9, -0.3, 1.2, 1.1, 1.0, 0.1, 0.1, 0.15)
    build_military_base()
end

function crayon.update(raw_dt)
    -- Time dilation recovery (hit-stop)
    if time_scale < 1.0 then
        time_scale = math.min(1.0, time_scale + raw_dt * 1.5)
    end
    local dt = raw_dt * time_scale

    if flash_alpha > 0 then flash_alpha = math.max(0, flash_alpha - raw_dt * 2.0) end
    if cam.shake   > 0 then cam.shake   = math.max(0, cam.shake   - raw_dt * 6.0) end

    -- Camera movement
    local move_spd = 25.0 * dt
    if crayon.key.isDown("w") then cam.z = cam.z - move_spd end
    if crayon.key.isDown("s") then cam.z = cam.z + move_spd end
    if crayon.key.isDown("a") then cam.x = cam.x - move_spd end
    if crayon.key.isDown("d") then cam.x = cam.x + move_spd end

    -- Mouse reticle via unproject -> raycast
    local mx, my = crayon.mouse.getPosition()
    local ox, oy, oz, dx, dy, dz = crayon.graphics.unproject(mx, my)
    local hit, hx, hy, hz, _, _, _, _ = crayon.physics3D.raycast(ox, oy, oz, dx, dy, dz, 300.0)
    cam.target_hit = hit
    cam.tx, cam.ty, cam.tz = hx, hy, hz

    weapon_timers.cannon = math.max(0, weapon_timers.cannon - dt)
    weapon_timers.bomb   = math.max(0, weapon_timers.bomb   - dt)

    if hit then
        -- Cannon (LMB)
        if crayon.mouse.isDown(1) and weapon_timers.cannon == 0 then
            weapon_timers.cannon = WEAPONS.cannon.cooldown
            local body = crayon.physics3D.createSphere(cam.x, cam.y - 2, cam.z, 0.3, "dynamic", 0.1, 0.1, 100.0)
            local ddx, ddy, ddz = hx - cam.x, hy - (cam.y - 2), hz - cam.z
            local dir_len = math.sqrt(ddx * ddx + ddy * ddy + ddz * ddz)
            body:setVelocity(
                (ddx / dir_len) * WEAPONS.cannon.speed,
                (ddy / dir_len) * WEAPONS.cannon.speed,
                (ddz / dir_len) * WEAPONS.cannon.speed
            )
            table.insert(projectiles, { body = body, type = "cannon", last_y = cam.y, data = WEAPONS.cannon })
        end

        -- Bomb (RMB / button 3)
        if crayon.mouse.isDown(3) and weapon_timers.bomb == 0 then
            weapon_timers.bomb = WEAPONS.bomb.cooldown
            local body = crayon.physics3D.createSphere(hx, hy + WEAPONS.bomb.drop_height, hz, 1.2, "dynamic", 0.5, 0.1, 1000.0)
            body:setVelocity(0, -WEAPONS.bomb.speed, 0)
            table.insert(projectiles, { body = body, type = "bomb", last_y = hy + WEAPONS.bomb.drop_height, data = WEAPONS.bomb })
        end
    end

    -- Process projectiles
    for i = #projectiles, 1, -1 do
        local p = projectiles[i]
        if p.body and p.body:isValid() then
            local px, py, pz = p.body:getPosition()
            if py <= 0.5 then
                trigger_explosion(px, py, pz, p.data.radius, p.data.force, p.data.color, p.data.hitstop)
                p.body:destroy()
                table.remove(projectiles, i)
            else
                p.last_y = py
            end
        else
            table.remove(projectiles, i)
        end
    end

    -- Procedural shrapnel (custom lightweight loop)
    for i = #particles, 1, -1 do
        local p = particles[i]
        p.vy = p.vy - (40.0 * dt) -- Gravity
        p.x  = p.x + p.vx * dt
        p.y  = p.y + p.vy * dt
        p.z  = p.z + p.vz * dt
        p.rx = p.rx + p.rvx * dt
        p.ry = p.ry + p.rvy * dt

        if p.y < 0.2 then
            p.y  = 0.2
            p.vy = -p.vy * 0.5
            p.vx = p.vx * 0.8
            p.vz = p.vz * 0.8
        end

        p.life = p.life - dt
        if p.life <= 0 then table.remove(particles, i) end
    end

    -- Cleanup map (fall-off = score)
    for i = #bodies, 1, -1 do
        local b = bodies[i]
        if b.body and b.body:isValid() then
            local _, by, _ = b.body:getPosition()
            if by < -10.0 then
                if b.is_target then score = score + 150 end
                b.body:destroy()
                table.remove(bodies, i)
            end
        else
            table.remove(bodies, i)
        end
    end

    if crayon.key.isPressed("r") then build_military_base() end
end

function crayon.draw()
    crayon.graphics.clear(0.05, 0.05, 0.06)

    -- Cinematic shake (non-linear curve)
    local magnitude = cam.shake * (cam.shake * 0.5)
    local sx = (math.random() - 0.5) * magnitude
    local sz = (math.random() - 0.5) * magnitude
    local sy = (math.random() - 0.5) * magnitude * 0.5

    local rad_yaw   = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local tx = (cam.x + sx) + math.cos(rad_pitch) * math.cos(rad_yaw)
    local ty = (cam.y + sy) + math.sin(rad_pitch)
    local tz = (cam.z + sz) + math.cos(rad_pitch) * math.sin(rad_yaw)

    crayon.graphics.setCamera3D({
        position = { cam.x + sx, cam.y + sy, cam.z + sz },
        target   = { tx, ty, tz },
        fov      = cam.fov + cam.shake * 0.2
    })

    -- Ground
    crayon.graphics.setColor(0.1, 0.1, 0.12, 1.0)
    crayon.graphics.drawModel(models.plane, 0, 0, 0, 0, 0, 0, 35.0, 1.0, 35.0, tex_white)

    -- Solid bodies
    for _, b in ipairs(bodies) do
        if b.body and b.body:isValid() then
            local px, py, pz = b.body:getPosition()
            local rx, ry, rz = b.body:getRotation()
            crayon.graphics.setColor(b.color[1], b.color[2], b.color[3], 1.0)
            local mdl = (b.type == "cube") and models.cube or models.cylinder
            crayon.graphics.drawModel(mdl, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), b.sx, b.sy, b.sz, tex_white)
        end
    end

    -- Shrapnel
    crayon.graphics.setColor(0.2, 0.2, 0.2, 1.0)
    for _, p in ipairs(particles) do
        crayon.graphics.drawModel(models.cube, p.x, p.y, p.z, math.rad(p.rx), math.rad(p.ry), math.rad(p.rz), p.size, p.size, p.size, tex_white)
    end

    -- Projectiles
    crayon.graphics.setColor(0.0, 0.0, 0.0, 1.0)
    for _, p in ipairs(projectiles) do
        if p.body and p.body:isValid() then
            local px, py, pz = p.body:getPosition()
            local r = (p.type == "bomb") and 1.2 or 0.3
            crayon.graphics.drawModel(models.sphere, px, py, pz, 0, 0, 0, r * 2, r * 2, r * 2, tex_white)
        end
    end

    -- Layered explosions
    local raw_dt = crayon.time.getDt()
    for i = #explosions, 1, -1 do
        local ex = explosions[i]
        ex.life = ex.life - (raw_dt * 3.0)
        ex.r    = ex.r + ((ex.max_r - ex.r) * raw_dt * 10.0)

        if ex.life <= 0 then
            table.remove(explosions, i)
        else
            crayon.graphics.setColor(ex.color[1], ex.color[2], ex.color[3], ex.life)
            if ex.type == "sphere" then
                crayon.graphics.drawModel(models.sphere, ex.x, ex.y, ex.z, 0, 0, 0, ex.r * 2, ex.r * 2, ex.r * 2, tex_white)
            elseif ex.type == "shockwave" then
                crayon.graphics.drawModel(models.cylinder, ex.x, ex.y, ex.z, 0, 0, 0, ex.r * 2, 0.2, ex.r * 2, tex_white)
            end
        end
    end
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)

    -- Reticle
    if cam.target_hit then
        crayon.graphics.setColor(1.0, 0.1, 0.0, 0.8)
        crayon.graphics.drawModel(models.cylinder, cam.tx, cam.ty + 0.1, cam.tz, 0, 0, 0, 2.0, 0.1, 2.0, tex_white)
    end

    -- Full-screen flash (additive overlay), sized from virtual resolution
    if flash_alpha > 0 then
        local vw, vh = crayon.window.getResolution()
        crayon.graphics.setColor(1.0, 1.0, 1.0, flash_alpha)
        crayon.graphics.drawRect("fill", 0, 0, vw, vh)
    end

    -- Minimalist UI
    crayon.graphics.setColor(1.0, 0.5, 0.0, 1.0)
    if time_scale < 1.0 then
        crayon.graphics.drawText("TIME DILATION ACTIVE", 10, 10, 1.5)
    end
end