-- ============================================================================
-- air_strike_2.lua — Kinetic Strike (Aircraft Bombsight Camera Edition)
-- * All 3D meshes render WHITE — IR/bombsight-consistent, no material tints
-- * No textures loaded or passed — drawModel tex arg is always 0
-- * Full "plane-mounted bombing camera" HUD + film-grain/scanline/noise stack
-- ============================================================================

local cam = {
    x = 0.0, y = 25.0, z = 15.0,
    yaw = -90.0, pitch = -60.0,
    fov = 55.0, shake = 0.0
}

local models = {}
local bodies = {}        -- { body=<Physics3D.Body>, type=, sx=, sy=, sz=, color=, is_target= }
local projectiles = {}   -- { body=<Physics3D.Body>, type=, last_y=, data= }
local explosions = {}
local particles = {}     -- Procedural shrapnel
local score = 0

-- Cinematic FX globals
local time_scale = 1.0
local flash_alpha = 0.0
local flash_color = { 1.0, 1.0, 1.0 }

-- Horror / glitch system
local glitch = {
    intensity     = 0.0,
    next_spike    = 4.0,
    dread         = 0.0,
    whisper_text  = "",
    whisper_timer = 0.0,
    sway_t        = 0.0,
}
local WHISPERS = {
    "DON'T LOOK AWAY", "IT SEES YOU", "BEHIND YOU", "STOP",
    "WHY ARE YOU HERE", "SIGNAL LOST", "WE ARE STILL HERE", "THE MONKEYS REMEMBER",
}

-- Bomber camera HUD state
local hud = {
    timecode     = 0.0,
    rec_blink    = 0.0,
    noise_band_y = {},
    frame        = 0,
    noise_burst  = 0.0,
}

local WEAPONS = {
    cannon = { speed = 120.0, radius = 6.0,  force = 80.0,  cooldown = 0.08, drop_height = 0,    color = {1.0, 0.9, 0.4}, hitstop = 0.0, flash = {1.0, 1.0, 0.8} },
    bomb   = { speed = 40.0,  radius = 25.0, force = 400.0, cooldown = 1.5,  drop_height = 40.0, color = {1.0, 0.2, 0.05}, hitstop = 0.1, flash = {1.0, 0.15, 0.1} }
}
local weapon_timers = { cannon = 0, bomb = 0 }

-- ============================================================================
-- Spawners
-- ============================================================================
local function spawn_box(x, y, z, w, h, d, motion, mass, color, is_target)
    local body = crayon.physics3D.createBox(x, y, z, w * 0.5, h * 0.5, d * 0.5, motion, 0.8, 0.4, mass)
    table.insert(bodies, { body = body, type = "cube", sx = w, sy = h, sz = d, color = color, is_target = is_target })
    return body
end

local function spawn_cylinder(x, y, z, r, h, motion, mass, color, is_target)
    local body = crayon.physics3D.createCylinder(x, y, z, h * 0.5, r, motion, 0.8, 0.4, mass)
    table.insert(bodies, { body = body, type = "cylinder", sx = r * 2, sy = h, sz = r * 2, color = color, is_target = is_target })
    return body
end

local function spawn_monkey(x, y, z, w, h, d, motion, mass, color, is_target)
    local body = crayon.physics3D.createBox(x, y, z, w * 0.5, h * 0.5, d * 0.5, motion, 0.8, 0.4, mass)
    table.insert(bodies, { body = body, type = "monkey", sx = w, sy = h, sz = d, color = color, is_target = is_target })
    return body
end

local function build_military_base()
    crayon.physics3D.destroyAll()
    bodies = {}
    crayon.physics3D.createPlane(0, 0, 0, 0, 1, 0, 100.0)

    -- Main hangar
    spawn_box( 10, 2, -10, 1, 4, 15, "static", 0, {1, 1, 1, 1}, false)
    spawn_box( 20, 2, -10, 1, 4, 15, "static", 0, {1, 1, 1, 1}, false)
    spawn_box( 15, 2, -17, 10, 4, 1, "static", 0, {1, 1, 1, 1}, false)
    for i = 0, 5 do
        spawn_box(10 + i * 2, 4.5, -10, 1.8, 0.5, 15, "dynamic", 80, {1, 1, 1, 1}, true)
    end

    -- Explosive fuel depot
    for fx = 0, 2 do
        for fz = 0, 2 do
            spawn_cylinder(-15 + fx * 2, 2, -5 + fz * 2, 0.8, 3.5, "dynamic", 30, {1, 1, 1, 1}, true)
        end
    end

    -- Watchtowers (monkeys on top)
    local tower_positions = { {-8, 8}, {12, 18}, {-22, -12} }
    for _, pos in ipairs(tower_positions) do
        for h = 0, 4 do
            spawn_box(pos[1], 1 + h * 2, pos[2], 2, 2, 2, "dynamic", 60, {1, 1, 1, 1}, true)
        end
        spawn_monkey(pos[1], 11, pos[2], 1, 1, 1, "dynamic", 10, {1, 1, 1, 1}, true)
    end
end

-- ============================================================================
-- Explosions / Shrapnel
-- ============================================================================
local function spawn_shrapnel(x, y, z, count, force)
    for _ = 1, count do
        table.insert(particles, {
            x = x, y = y, z = z,
            vx = (math.random() - 0.5) * force,
            vy = (math.random() * force) + (force * 0.5),
            vz = (math.random() - 0.5) * force,
            rx = math.random(0, 360), ry = math.random(0, 360), rz = math.random(0, 360),
            rvx = math.random(-300, 300), rvy = math.random(-300, 300),
            size = math.random() * 0.4 + 0.1,
            life = math.random() * 1.5 + 0.5
        })
    end
end

local function trigger_explosion(ex, ey, ez, radius, force, color, hitstop, flash_col)
    if hitstop > 0 then
        time_scale  = hitstop
        flash_alpha = 1.0
        flash_color = flash_col or { 1.0, 1.0, 1.0 }
    end
    cam.shake = cam.shake + (force * 0.01)

    if math.random() < 0.35 then
        glitch.intensity = math.max(glitch.intensity, 0.4 + math.random() * 0.4)
        hud.noise_burst  = math.max(hud.noise_burst, 0.6)
    end

    table.insert(explosions, { type = "sphere",    x = ex, y = ey,       z = ez, r = 0, max_r = radius,       life = 1.0, color = color })
    table.insert(explosions, { type = "shockwave", x = ex, y = ey + 0.5, z = ez, r = 0, max_r = radius * 1.5, life = 1.0, color = {1, 1, 1} })

    spawn_shrapnel(ex, ey, ez, radius * 3, force * 0.15)

    local nearby = crayon.physics3D.overlapSphere(ex, ey, ez, radius)
    for _, body in ipairs(nearby) do
        if body and body:isValid() then
            local bx, by, bz = body:getPosition()
            local dx, dy, dz = bx - ex, by - ey, bz - ez
            local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
            if dist < 0.1 then dist = 0.1 end
            if dist <= radius then
                local f_mult = math.pow(1.0 - (dist / radius), 2) * force
                local vy_bias = 1.5
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
-- Init
-- ============================================================================
function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("Air Strike 2")
    crayon.window.setScalingMode("integer")

    math.randomseed(os.time and os.time() or 12345)

    models.cube     = crayon.graphics.loadModel("cube")
    models.cylinder = crayon.graphics.loadModel("cylinder")
    models.sphere   = crayon.graphics.loadModel("sphere")
    models.plane    = crayon.graphics.loadModel("plane")
    models.monkey   = crayon.graphics.loadModel("../assets/models/monkey.obj")

    crayon.graphics.setRetroEffects({
        jitterResolution = {320, 240},
        affine           = 0.9,
        dither           = true,
        colorDepth       = 8,
        crt = {
            scanlines = 0.45,
            curvature = 0.06,
            vignette  = 0.45
        },
        fog = { startDist = 20, endDist = 80, color = {0.02, 0.02, 0.03} }
    })

    crayon.graphics.setLight(0.4, -0.9, -0.3, 0.9, 0.95, 0.85, 0.1, 0.05, 0.08)

    hud.noise_band_y = {}
    for i = 1, 12 do
        hud.noise_band_y[i] = math.random() * 240
    end

    build_military_base()
end

-- ============================================================================
-- Update
-- ============================================================================
function crayon.update(raw_dt)
    if time_scale < 1.0 then
        time_scale = math.min(1.0, time_scale + raw_dt * 1.5)
    end
    local dt = raw_dt * time_scale

    -- ===== Horror / glitch timing =====
    glitch.dread = math.min(1.0, glitch.dread + raw_dt * 0.004)

    glitch.intensity = math.max(0, glitch.intensity - raw_dt * 5.0)
    glitch.next_spike = glitch.next_spike - raw_dt
    if glitch.next_spike <= 0 then
        glitch.intensity = 0.6 + math.random() * 0.4
        local base = 3.5 - glitch.dread * 2.0
        glitch.next_spike = math.max(1.0, base + math.random() * 5.0)
        cam.shake = cam.shake + 0.8 + glitch.dread * 1.2
        hud.noise_burst = math.max(hud.noise_burst, 0.5)
    end

    glitch.whisper_timer = glitch.whisper_timer - raw_dt
    if glitch.whisper_timer <= 0 then
        glitch.whisper_text  = WHISPERS[math.random(1, #WHISPERS)]
        glitch.whisper_timer = 2.0 + math.random() * 4.0
    end

    glitch.sway_t = glitch.sway_t + raw_dt

    -- ===== Bomber-camera HUD updates =====
    hud.timecode    = hud.timecode + raw_dt
    hud.rec_blink   = hud.rec_blink + raw_dt
    hud.frame       = hud.frame + 1
    hud.noise_burst = math.max(0, hud.noise_burst - raw_dt * 2.5)

    for i = 1, #hud.noise_band_y do
        hud.noise_band_y[i] = hud.noise_band_y[i] + (30 + i * 5) * raw_dt
        if hud.noise_band_y[i] > 260 then
            hud.noise_band_y[i] = -20
        end
    end

    -- Altitude-linked turbulence
    local alt = math.max(0, cam.y)
    local alt_turb = math.min(0.35, alt * 0.006)
    local sway_x = math.sin(glitch.sway_t * 0.9) * (0.03 + glitch.dread * 0.12 + alt_turb)
    local sway_y = math.cos(glitch.sway_t * 1.3) * (0.02 + glitch.dread * 0.08 + alt_turb * 0.6)

    if flash_alpha > 0 then flash_alpha = math.max(0, flash_alpha - raw_dt * 2.0) end
    if cam.shake   > 0 then cam.shake   = math.max(0, cam.shake   - raw_dt * 6.0) end

    -- ===== Camera movement =====
    local move_speed = 8.0 * dt
    local rad_yaw = math.rad(cam.yaw)
    local fwd_x, fwd_z   = math.cos(rad_yaw), math.sin(rad_yaw)
    local right_x, right_z = -fwd_z, fwd_x

    if crayon.key.isDown("w") then cam.x = cam.x + fwd_x * move_speed;   cam.z = cam.z + fwd_z * move_speed   end
    if crayon.key.isDown("s") then cam.x = cam.x - fwd_x * move_speed;   cam.z = cam.z - fwd_z * move_speed   end
    if crayon.key.isDown("a") then cam.x = cam.x - right_x * move_speed; cam.z = cam.z - right_z * move_speed end
    if crayon.key.isDown("d") then cam.x = cam.x + right_x * move_speed; cam.z = cam.z + right_z * move_speed end
    if crayon.key.isDown("q") or crayon.key.isDown("space") then cam.y = cam.y + move_speed end
    if crayon.key.isDown("z") or crayon.key.isDown("shift") then cam.y = cam.y - move_speed end

    if crayon.key.isDown("left")  then cam.yaw   = cam.yaw   - 85.0 * dt end
    if crayon.key.isDown("right") then cam.yaw   = cam.yaw   + 85.0 * dt end
    if crayon.key.isDown("up")    then cam.pitch = math.min(cam.pitch + 60.0 * dt,  80.0) end
    if crayon.key.isDown("down")  then cam.pitch = math.max(cam.pitch - 60.0 * dt, -80.0) end

    -- ===== Mouse + targeting =====
    local mx, my = crayon.mouse.getPosition()
    local ox, oy, oz, dx, dy, dz = crayon.graphics.unproject(mx, my)
    local hit, hx, hy, hz = crayon.physics3D.raycast(ox, oy, oz, dx, dy, dz, 300.0)
    cam.target_hit = hit
    cam.tx, cam.ty, cam.tz = hx, hy, hz

    weapon_timers.cannon = math.max(0, weapon_timers.cannon - dt)
    weapon_timers.bomb   = math.max(0, weapon_timers.bomb   - dt)

    if hit then
        if crayon.mouse.isDown(1) and weapon_timers.cannon == 0 then
            weapon_timers.cannon = WEAPONS.cannon.cooldown
            local body = crayon.physics3D.createSphere(cam.x, cam.y - 2, cam.z, 0.3, "dynamic", 0.1, 0.1, 100.0)
            local ddx, ddy, ddz = hx - cam.x, hy - (cam.y - 2), hz - cam.z
            local len = math.sqrt(ddx * ddx + ddy * ddy + ddz * ddz)
            body:setVelocity(
                (ddx / len) * WEAPONS.cannon.speed,
                (ddy / len) * WEAPONS.cannon.speed,
                (ddz / len) * WEAPONS.cannon.speed
            )
            table.insert(projectiles, { body = body, type = "cannon", last_y = cam.y, data = WEAPONS.cannon })
        end

        if crayon.mouse.isDown(3) and weapon_timers.bomb == 0 then
            weapon_timers.bomb = WEAPONS.bomb.cooldown
            local body = crayon.physics3D.createSphere(hx, hy + WEAPONS.bomb.drop_height, hz, 1.2, "dynamic", 0.5, 0.1, 1000.0)
            body:setVelocity(0, -WEAPONS.bomb.speed, 0)
            table.insert(projectiles, { body = body, type = "bomb", last_y = hy + WEAPONS.bomb.drop_height, data = WEAPONS.bomb })
        end
    end

    -- ===== Projectiles =====
    for i = #projectiles, 1, -1 do
        local p = projectiles[i]
        if p.body and p.body:isValid() then
            local px, py, pz = p.body:getPosition()
            if py <= 0.5 then
                trigger_explosion(px, py, pz, p.data.radius, p.data.force, p.data.color, p.data.hitstop, p.data.flash)
                p.body:destroy()
                table.remove(projectiles, i)
            else
                p.last_y = py
            end
        else
            table.remove(projectiles, i)
        end
    end

    -- ===== Shrapnel =====
    for i = #particles, 1, -1 do
        local p = particles[i]
        p.vy = p.vy - (40.0 * dt)
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

    -- ===== Cleanup + score =====
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

    cam.sway_x = sway_x
    cam.sway_y = sway_y
end

-- ============================================================================
-- 2D HUD helpers
-- ============================================================================
local function draw_bombsight(cx, cy)
    crayon.graphics.setColor(0.85, 1.0, 0.85, 0.9)
    crayon.graphics.drawCircle("line", cx, cy, 42)
    crayon.graphics.drawCircle("line", cx, cy, 22)
    crayon.graphics.drawLine(cx - 70, cy, cx - 10, cy, 1.0)
    crayon.graphics.drawLine(cx + 10, cy, cx + 70, cy, 1.0)
    crayon.graphics.drawLine(cx, cy - 70, cx, cy - 10, 1.0)
    crayon.graphics.drawLine(cx, cy + 10, cx, cy + 70, 1.0)
    for i = 1, 8 do
        local a  = (i / 8) * math.pi * 2
        crayon.graphics.drawLine(
            cx + math.cos(a) * 44, cy + math.sin(a) * 44,
            cx + math.cos(a) * 50, cy + math.sin(a) * 50,
            1.0
        )
    end
    crayon.graphics.drawCircle("fill", cx, cy, 1)
end

local function draw_range_ladder(cx, cy, dist)
    crayon.graphics.setColor(0.7, 1.0, 0.7, 0.65)
    local top = cy + 80
    for i = 0, 8 do
        local y  = top + i * 6
        local hw = (i % 2 == 0) and 5 or 3
        crayon.graphics.drawLine(cx - hw, y, cx + hw, y, 1.0)
    end
    if dist then
        crayon.graphics.drawText(string.format("%4d m", math.floor(dist)), cx + 10, top + 20, 1.0)
    end
end

local function draw_frame_marks(vw, vh)
    crayon.graphics.setColor(0.75, 1.0, 0.75, 0.85)
    local m, s = 4, 10
    crayon.graphics.drawLine(m, m, m + s, m, 1.5)
    crayon.graphics.drawLine(m, m, m, m + s, 1.5)
    crayon.graphics.drawLine(vw - m - s, m, vw - m, m, 1.5)
    crayon.graphics.drawLine(vw - m, m, vw - m, m + s, 1.5)
    crayon.graphics.drawLine(m, vh - m, m + s, vh - m, 1.5)
    crayon.graphics.drawLine(m, vh - m - s, m, vh - m, 1.5)
    crayon.graphics.drawLine(vw - m - s, vh - m, vw - m, vh - m, 1.5)
    crayon.graphics.drawLine(vw - m, vh - m - s, vw - m, vh - m, 1.5)
end

local function draw_telemetry()
    local green = { 0.55, 1.0, 0.55 }
    crayon.graphics.setColor(green[1], green[2], green[3], 0.9)

    local hdg = (cam.yaw % 360)
    if hdg < 0 then hdg = hdg + 360 end

    local fps = crayon.time.getFps() or 0
    local tc  = hud.timecode
    local tc_str = string.format("%02d:%02d:%02d.%02d",
        math.floor(tc / 3600),
        math.floor((tc / 60) % 60),
        math.floor(tc % 60),
        math.floor((tc * 100) % 100))

    local lines = {
        string.format("ALT  %6.1f m", cam.y),
        string.format("HDG  %6.1f ", hdg),
        string.format("FOV  %6.1f ", cam.fov + cam.shake * 0.2 + glitch.intensity * 8.0),
        string.format("SPD  %6.1f ", 8.0),
        string.format("FPS  %6d ", math.floor(fps)),
        string.format("ORD  %6d ", score),
    }

    local y = 6
    for _, line in ipairs(lines) do
        crayon.graphics.drawText(line, 8, y, 1.0)
        y = y + 9
    end

    crayon.graphics.setColor(green[1], green[2], green[3], 0.85)
    crayon.graphics.drawText("TC " .. tc_str, 8, 232, 1.0)

    local x = 232
    crayon.graphics.drawText("WPN", x, 6,  1.0)
    crayon.graphics.drawText("CANNON", x, 16, 0.9)
    crayon.graphics.drawText("BOMB",   x, 26, 0.9)
    local function bar(y, t, total)
        local w = 60 * (1.0 - t / total)
        crayon.graphics.drawRect("line", x, y, 62, 4)
        if w > 0 then
            crayon.graphics.drawRect("fill", x + 1, y + 1, w, 2)
        end
    end
    bar(18, weapon_timers.cannon, WEAPONS.cannon.cooldown)
    bar(28, weapon_timers.bomb,   WEAPONS.bomb.cooldown)
end

local function draw_rec_indicator(vw)
    local blink = (math.floor(hud.rec_blink * 2) % 2 == 0)
    if blink then
        crayon.graphics.setColor(1.0, 0.15, 0.15, 0.95)
        crayon.graphics.drawCircle("fill", vw - 46, 10, 3)
    end
    crayon.graphics.setColor(0.85, 0.9, 0.85, 0.9)
    crayon.graphics.drawText("REC", vw - 40, 6, 1.0)

    crayon.graphics.setColor(0.55, 1.0, 0.55, 0.75)
    crayon.graphics.drawText("CAM-2", vw - 46, 20, 1.0)
    crayon.graphics.drawText("IR-MD", vw - 46, 30, 1.0)
end

local function draw_noise_bands(vw, vh, intensity)
    local count = math.floor(2 + intensity * 10 + hud.noise_burst * 8)
    for i = 1, count do
        local by = (hud.noise_band_y[i] or math.random() * vh)
        local bh = math.random(1, 3)
        local a  = 0.15 + intensity * 0.25 + hud.noise_burst * 0.35
        crayon.graphics.setColor(0.9, 1.0, 0.9, math.min(0.75, a))
        crayon.graphics.drawRect("fill", 0, by, vw, bh)
    end
end

local function draw_static(vw, vh, intensity)
    local dots = math.floor(20 + intensity * 120 + hud.noise_burst * 200)
    for _ = 1, dots do
        local x = math.random() * vw
        local y = math.random() * vh
        if math.random() < 0.5 then
            crayon.graphics.setColor(0.9, 1.0, 0.9, 0.5)
        else
            crayon.graphics.setColor(0.05, 0.05, 0.05, 0.6)
        end
        crayon.graphics.drawRect("fill", x, y, 1, 1)
    end
end

-- ============================================================================
-- Draw
-- ============================================================================
function crayon.draw()
    crayon.graphics.clear(0.02, 0.02, 0.03)

    local g     = glitch.intensity
    local dread = glitch.dread
    local vw, vh = crayon.window.getResolution()

    -- ===== Push post-processing for the 3D scene only =====
    crayon.graphics.pushEffect("vignette",  { intensity = 0.55 + dread * 0.3 })
    crayon.graphics.pushEffect("filmGrain", { intensity = 0.18 + g * 0.4 + hud.noise_burst * 0.3 })
    crayon.graphics.pushEffect("vhs",       { intensity = 0.35 + g * 0.55 })
    crayon.graphics.pushEffect("chromatic", { amount    = 0.006 + g * 0.02 })
    crayon.graphics.pushEffect("pixelate",  { size      = 2.0 })

    -- ===== Camera pose =====
    local magnitude = cam.shake * (cam.shake * 0.5)
    local sx = (math.random() - 0.5) * magnitude + (cam.sway_x or 0)
    local sz = (math.random() - 0.5) * magnitude
    local sy = (math.random() - 0.5) * magnitude * 0.5 + (cam.sway_y or 0)

    local glitch_yaw   = (math.random() - 0.5) * g * 4.0
    local glitch_pitch = (math.random() - 0.5) * g * 3.0

    local rad_yaw   = math.rad(cam.yaw   + glitch_yaw)
    local rad_pitch = math.rad(cam.pitch + glitch_pitch)
    local tx = (cam.x + sx) + math.cos(rad_pitch) * math.cos(rad_yaw)
    local ty = (cam.y + sy) + math.sin(rad_pitch)
    local tz = (cam.z + sz) + math.cos(rad_pitch) * math.sin(rad_yaw)

    crayon.graphics.setCamera3D({
        position = { cam.x + sx, cam.y + sy, cam.z + sz },
        target   = { tx, ty, tz },
        fov      = cam.fov + cam.shake * 0.2 + g * 8.0
    })

    -- ============================================================
    -- All 3D geometry is WHITE. One setColor is enough for the
    -- whole scene since we never change it between draws (except
    -- the explosion fire color).
    -- ============================================================
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)

    -- Ground
    crayon.graphics.drawModel(models.plane, 0, 0, 0, 0, 0, 0, 35.0, 1.0, 35.0, 0)

    -- Bodies (all white)
    for _, b in ipairs(bodies) do
        if b.body and b.body:isValid() then
            local px, py, pz = b.body:getPosition()
            local rx, ry, rz = b.body:getRotation()
            if b.type == "cube" then
                crayon.graphics.drawModel(models.cube, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), b.sx, b.sy, b.sz, 0)
            elseif b.type == "cylinder" then
                crayon.graphics.drawModel(models.cylinder, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), b.sx, b.sy, b.sz, 0)
            elseif b.type == "monkey" then
                crayon.graphics.drawModel(models.monkey, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), b.sx, b.sy, b.sz, 0)
            end
        end
    end

    -- Shrapnel (white)
    for _, p in ipairs(particles) do
        crayon.graphics.drawModel(models.cube, p.x, p.y, p.z, math.rad(p.rx), math.rad(p.ry), math.rad(p.rz), p.size, p.size, p.size, 0)
    end

    -- Projectiles (white)
    for _, p in ipairs(projectiles) do
        if p.body and p.body:isValid() then
            local px, py, pz = p.body:getPosition()
            local r = (p.type == "bomb") and 1.2 or 0.3
            crayon.graphics.drawModel(models.sphere, px, py, pz, 0, 0, 0, r * 2, r * 2, r * 2, 0)
        end
    end

    -- Explosions — ONLY place we tint, because it reads as fire/heat.
    -- If you want these white too, delete the setColor line below.
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
                crayon.graphics.drawModel(models.sphere, ex.x, ex.y, ex.z, 0, 0, 0, ex.r * 2, ex.r * 2, ex.r * 2, 0)
            elseif ex.type == "shockwave" then
                crayon.graphics.drawModel(models.cylinder, ex.x, ex.y, ex.z, 0, 0, 0, ex.r * 2, 0.2, ex.r * 2, 0)
            end
        end
    end

    -- Back to white for the in-world reticle cylinder
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)

    -- ===== Pop the post stack (reverse order) =====
    crayon.graphics.popEffect()  -- pixelate
    crayon.graphics.popEffect()  -- chromatic
    crayon.graphics.popEffect()  -- vhs
    crayon.graphics.popEffect()  -- filmGrain
    crayon.graphics.popEffect()  -- vignette

    -- ============ 2D BOMBSIGHT HUD ============
    local cx, cy = vw * 0.5, vh * 0.5

    draw_bombsight(cx, cy)

    local range = nil
    if cam.target_hit then
        local ddx = cam.tx - cam.x
        local ddy = cam.ty - cam.y
        local ddz = cam.tz - cam.z
        range = math.sqrt(ddx * ddx + ddy * ddy + ddz * ddz)
    end
    draw_range_ladder(cx, cy, range)

    draw_frame_marks(vw, vh)
    draw_telemetry()
    draw_rec_indicator(vw)

    if flash_alpha > 0 then
        crayon.graphics.setColor(flash_color[1], flash_color[2], flash_color[3], flash_alpha)
        crayon.graphics.drawRect("fill", 0, 0, vw, vh)
    end

    draw_noise_bands(vw, vh, g)
    draw_static(vw, vh, g)

    if g > 0.2 or hud.noise_burst > 0.3 then
        local tear_y = math.random() * vh
        local tear_h = math.random(2, 6)
        crayon.graphics.setColor(0.9, 1.0, 0.9, 0.35 + g * 0.3)
        crayon.graphics.drawRect("fill", 0, tear_y, vw, tear_h)
    end

    if glitch.whisper_timer > 0 and math.floor(glitch.whisper_timer * 8) % 2 == 0 then
        crayon.graphics.setColor(0.85, 0.1, 0.1, 0.65 + math.sin(glitch.sway_t * 30) * 0.2)
        local w = crayon.graphics.getTextWidth(glitch.whisper_text, 1.0)
        crayon.graphics.drawText(glitch.whisper_text, (vw - w) * 0.5, vh - 16, 1.0)
    end

    crayon.graphics.setColor(0.55, 1.0, 0.55, 0.45)
    crayon.graphics.drawText("WASD/QA:FLY  ARROWS:LOOK  LMB:CANNON  RMB:BOMB  R:RESET", 8, vh - 8, 0.85)
end