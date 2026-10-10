-- ============================================================================
-- air_strike_2.lua — Kinetic Strike (Aircraft Bombsight Camera Edition)
-- * Colors: primitives use setModelColor(r,g,b); the C-17 uses setPartColor
-- * Full "plane-mounted bombing camera" HUD + film-grain/scanline/noise stack
-- * C-17 planes have accurate box colliders and sit on the ground
-- * Military airbase map (runway, taxiway, apron, hangars, tower, radar,
--   fuel depot, ammo bunkers, barracks, perimeter towers)
-- * [Tab] toggles physics debug draw
-- * [F1]  toggles all retro/post-processing effects
-- ============================================================================

local cam = {
    x = 0.0, y = 25.0, z = 15.0,
    yaw = -90.0, pitch = -60.0,
    fov = 55.0, shake = 0.0
}

local models = {}
local tex_white = nil
local bodies = {}        -- { body, type, sx, sy, sz, color={r,g,b,a}, is_target }
local projectiles = {}
local explosions = {}
local particles = {}
local score = 0

-- Cinematic FX globals
local time_scale = 1.0
local flash_alpha = 0.0
local flash_color = { 1.0, 1.0, 1.0 }

-- Toggle state
local effects_enabled    = true
local debug_draw_enabled = false

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

-- ============================================================================
-- C-17 MODEL DIMENSIONS
-- ============================================================================
local PLANE_W = 10.35
local PLANE_H = 3.35
local PLANE_L = 10.61

-- ============================================================================
-- COLOR PALETTE  (r,g,b,a)
-- Primitives: setModelColor(r,g,b) ignores alpha (uses setColor's alpha).
-- Models:     model:setPartColor(i, r,g,b,a) uses all four.
-- ============================================================================
local COL = {
    ground        = {0.26, 0.34, 0.22, 1.0},
    asphalt       = {0.14, 0.14, 0.16, 1.0},
    marking_white = {0.94, 0.94, 0.90, 1.0},
    marking_yellow= {0.95, 0.85, 0.15, 1.0},
    edge_light    = {1.00, 0.85, 0.20, 1.0},
    hangar        = {0.34, 0.42, 0.30, 1.0},
    hangar_door   = {0.26, 0.32, 0.22, 1.0},
    hangar_roof   = {0.30, 0.36, 0.26, 1.0},
    tower_shaft   = {0.72, 0.75, 0.70, 1.0},
    tower_cabin   = {0.62, 0.68, 0.72, 1.0},
    tower_glass   = {0.30, 0.45, 0.55, 1.0},
    tower_dome    = {0.85, 0.85, 0.88, 1.0},
    radar_base    = {0.55, 0.58, 0.55, 1.0},
    radar_dish    = {0.78, 0.80, 0.82, 1.0},
    fuel_tank     = {0.90, 0.55, 0.15, 1.0},
    fuel_tank_band= {0.90, 0.15, 0.10, 1.0},
    berm          = {0.50, 0.44, 0.30, 1.0},
    bunker        = {0.42, 0.38, 0.28, 1.0},
    bunker_cover  = {0.34, 0.32, 0.22, 1.0},
    barracks      = {0.72, 0.64, 0.50, 1.0},
    watchtower    = {0.45, 0.35, 0.25, 1.0},
    watchcabin    = {0.55, 0.45, 0.32, 1.0},
    guard         = {0.85, 0.82, 0.70, 1.0},
    plane         = {0.72, 0.75, 0.78, 1.0},
    shrapnel      = {1.00, 0.55, 0.15, 1.0},
    cannon        = {1.00, 0.95, 0.40, 1.0},
    bomb          = {0.28, 0.28, 0.30, 1.0},
    white         = {1.00, 1.00, 1.00, 1.0},
}

local WEAPONS = {
    cannon = { speed = 120.0, radius = 6.0,  force = 80.0,  cooldown = 0.08, drop_height = 0,    color = {1.0, 0.9, 0.4}, hitstop = 0.0, flash = {1.0, 1.0, 0.8} },
    bomb   = { speed = 40.0,  radius = 25.0, force = 400.0, cooldown = 1.5,  drop_height = 40.0, color = {1.0, 0.2, 0.05}, hitstop = 0.1, flash = {1.0, 0.15, 0.1} }
}
local weapon_timers = { cannon = 0, bomb = 0 }

-- ============================================================================
-- Retro-effect presets
-- ============================================================================
local RETRO_ON = {
    jitterResolution = {320, 240},
    affine           = 0.9,
    dither           = true,
    colorDepth       = 8,
    crt = { scanlines = 0.45, curvature = 0.06, vignette = 0.45 },
    fog = { startDist = 20, endDist = 80, color = {0.02, 0.02, 0.03} }
}
local RETRO_OFF = {
    jitterResolution = false,
    affine           = 0.0,
    dither           = false,
    colorDepth       = 32,
    crt              = { scanlines = 0.0, curvature = 0.0, vignette = 0.0 },
    fog              = { startDist = 10000, endDist = 10001, color = {0.0, 0.0, 0.0} }
}

local function apply_effects(on)
    crayon.graphics.setRetroEffects(on and RETRO_ON or RETRO_OFF)
end

-- ============================================================================
-- Color helpers
-- ============================================================================
-- Primitives (drawCube/drawSphere/...): setModelColor takes only (r,g,b)
local function tint(c)
    crayon.graphics.setModelColor(c[1], c[2], c[3])
end

-- Loaded models: setPartColor takes (idx, r,g,b,a) and is baked into the mesh
local function tint_model(model, c)
    if not model or not model:isValid() then return end
    local n = model:getPartCount()
    for i = 0, n - 1 do
        model:setPartColor(i, c[1], c[2], c[3], c[4] or 1.0)
    end
end

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

local function spawn_plane(x, belly_y, z, motion, mass, yaw)
    belly_y = belly_y or 0.0
    yaw     = yaw or 0.0
    local cy = belly_y + PLANE_H * 0.5
    local body = crayon.physics3D.createBox(
        x, cy, z,
        PLANE_W * 0.5, PLANE_H * 0.5, PLANE_L * 0.5,
        motion, 0.9, 0.5, mass or 400
    )
    table.insert(bodies, {
        body      = body,
        type      = "plane",
        sx        = PLANE_W,
        sy        = PLANE_H,
        sz        = PLANE_L,
        color     = COL.plane,
        is_target = true,
        yaw       = yaw,
    })
    return body
end

-- ============================================================================
-- MILITARY AIRBASE LAYOUT
-- ============================================================================
local function build_military_base()
    crayon.physics3D.destroyAll()
    bodies = {}

    crayon.physics3D.createPlane(0, 0, 0, 0, 1, 0, 200.0)

    -- ==================================================================
    -- RUNWAY (X axis, at Z = -45)
    -- ==================================================================
    local RY_Z    = -45
    local RY_LEN  = 150
    local RY_HALF = RY_LEN * 0.5

    spawn_box(0, 0.02, RY_Z, RY_LEN, 0.05, 20, "static", 0, COL.asphalt, false)

    for i = -7, 7 do
        spawn_box(i * 10, 0.08, RY_Z, 4.0, 0.05, 0.9, "static", 0, COL.marking_white, false)
    end

    for i = 0, 5 do
        local off = 4 + i * 1.8
        spawn_box(-RY_HALF + off, 0.08, RY_Z - 5, 0.8, 0.05, 8, "static", 0, COL.marking_white, false)
        spawn_box(-RY_HALF + off, 0.08, RY_Z + 5, 0.8, 0.05, 8, "static", 0, COL.marking_white, false)
        spawn_box( RY_HALF - off, 0.08, RY_Z - 5, 0.8, 0.05, 8, "static", 0, COL.marking_white, false)
        spawn_box( RY_HALF - off, 0.08, RY_Z + 5, 0.8, 0.05, 8, "static", 0, COL.marking_white, false)
    end

    for i = -9, 9 do
        spawn_box(i * 8, 0.25, RY_Z - 9, 0.35, 0.5, 0.35, "static", 0, COL.edge_light, false)
        spawn_box(i * 8, 0.25, RY_Z + 9, 0.35, 0.5, 0.35, "static", 0, COL.edge_light, false)
    end

    -- ==================================================================
    -- TAXIWAY
    -- ==================================================================
    spawn_box(0, 0.02, -28, RY_LEN, 0.05, 10, "static", 0, COL.asphalt, false)
    for i = -7, 7 do
        spawn_box(i * 8, 0.08, -28, 4.5, 0.05, 1.0, "static", 0, COL.marking_yellow, false)
    end

    -- ==================================================================
    -- HANGARS
    -- ==================================================================
    local hangar_xs = { -35, 0, 35 }
    for _, hx in ipairs(hangar_xs) do
        local hz = 20
        local hw, hd, hh = 26, 22, 14

        spawn_box(hx, hh * 0.5, hz + hd * 0.5, hw, hh, 1.0, "static", 0, COL.hangar, false)
        spawn_box(hx - hw * 0.5, hh * 0.5, hz, 1.0, hh, hd, "static", 0, COL.hangar, false)
        spawn_box(hx + hw * 0.5, hh * 0.5, hz, 1.0, hh, hd, "static", 0, COL.hangar, false)
        spawn_box(hx, hh + 0.5, hz, hw + 2, 1.0, hd + 2, "static", 0, COL.hangar_roof, false)
        spawn_box(hx - hw * 0.25, hh * 0.5, hz - hd * 0.5, hw * 0.5 - 1, hh, 0.6, "dynamic", 500, COL.hangar_door, true)
        spawn_box(hx + hw * 0.25, hh * 0.5, hz - hd * 0.5, hw * 0.5 - 1, hh, 0.6, "dynamic", 500, COL.hangar_door, true)
    end

    -- ==================================================================
    -- CONTROL TOWER
    -- ==================================================================
    do
        local tx, tz = 55, 0
        spawn_box(tx, 10, tz, 4, 20, 4, "static", 0, COL.tower_shaft, false)
        spawn_box(tx, 22, tz, 8, 4, 8,  "static", 0, COL.tower_cabin, false)
        spawn_box(tx, 22, tz - 4.1, 8, 3.2, 0.3, "static", 0, COL.tower_glass, false)
        spawn_box(tx, 26, tz, 0.5, 4, 0.5, "static", 0, COL.tower_shaft, false)
        spawn_cylinder(tx, 29.5, tz, 1.8, 3, "static", 0, COL.tower_dome, false)
    end

    -- ==================================================================
    -- RADAR ARRAY
    -- ==================================================================
    do
        local rx, rz = -55, 0
        spawn_box(rx, 3, rz, 5, 6, 5, "static", 0, COL.radar_base, false)
        spawn_box(rx, 7, rz, 7, 2, 7, "static", 0, COL.radar_base, false)
        spawn_box(rx, 10, rz, 5.5, 0.4, 5.5, "static", 0, COL.radar_dish, false)
    end

    -- ==================================================================
    -- FUEL DEPOT
    -- ==================================================================
    do
        local fx, fz = -50, 38
        local tank_offsets = { {-5, -5}, {5, -5}, {-5, 5}, {5, 5} }
        for _, off in ipairs(tank_offsets) do
            spawn_cylinder(fx + off[1], 5, fz + off[2], 3.5, 10, "dynamic", 200, COL.fuel_tank, true)
            spawn_cylinder(fx + off[1], 8, fz + off[2], 3.55, 0.4, "dynamic", 5, COL.fuel_tank_band, true)
        end
        local bw = 14
        spawn_box(fx, 1.5, fz - 10, bw, 3, 1.5, "static", 0, COL.berm, false)
        spawn_box(fx, 1.5, fz + 10, bw, 3, 1.5, "static", 0, COL.berm, false)
        spawn_box(fx - 10, 1.5, fz, 1.5, 3, bw, "static", 0, COL.berm, false)
        spawn_box(fx + 10, 1.5, fz, 1.5, 3, bw, "static", 0, COL.berm, false)
    end

    -- ==================================================================
    -- AMMO BUNKERS
    -- ==================================================================
    for i = 0, 3 do
        local bx = 22 + i * 10
        local bz = 42
        spawn_box(bx, 2, bz, 4, 4, 8, "dynamic", 1000, COL.bunker, true)
        spawn_box(bx, 4.5, bz, 5, 1, 9, "static", 0, COL.bunker_cover, false)
    end

    -- ==================================================================
    -- BARRACKS
    -- ==================================================================
    for i = 0, 4 do
        spawn_box(-32 + i * 6, 2.5, 48, 3, 5, 12, "dynamic", 150, COL.barracks, true)
        spawn_box(-32 + i * 6, 2.5, 60, 3, 5, 12, "dynamic", 150, COL.barracks, true)
    end

    -- ==================================================================
    -- PERIMETER WATCHTOWERS
    -- ==================================================================
    local corner_positions = { {-70, -70}, {70, -70}, {-70, 70}, {70, 70} }
    for _, pos in ipairs(corner_positions) do
        for h = 0, 4 do
            spawn_box(pos[1], 1 + h * 2.5, pos[2], 2, 2.5, 2, "static", 0, COL.watchtower, true)
        end
        spawn_box(pos[1], 14, pos[2], 3.5, 3, 3.5, "static", 0, COL.watchcabin, true)
        spawn_box(pos[1], 16, pos[2], 1, 1, 1, "dynamic", 10, COL.guard, true)
    end

    -- ==================================================================
    -- PARKED C-17s
    -- ==================================================================
    spawn_plane(-15, 0, -12, "dynamic", 400)
    spawn_plane( 15, 0, -12, "dynamic", 400)
    spawn_plane(  0, 0,   4, "dynamic", 400)
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
            life = math.random() * 1.5 + 0.5,
            color = {
                COL.shrapnel[1] * (0.85 + math.random() * 0.15),
                COL.shrapnel[2] * (0.70 + math.random() * 0.30),
                COL.shrapnel[3] * (0.40 + math.random() * 0.40),
                1.0,
            }
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
    models.c17      = crayon.graphics.loadModel("top_c17.obj")

    tex_white = crayon.graphics.getWhiteTexture()

    -- Bake the C-17's color into the mesh itself — setModelColor does NOT
    -- affect drawModel, so loaded models must use setPartColor.
    tint_model(models.c17, COL.plane)

    apply_effects(effects_enabled)

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

    if crayon.key.isPressed("tab") then
        debug_draw_enabled = not debug_draw_enabled
    end
    if crayon.key.isPressed("f1") then
        effects_enabled = not effects_enabled
        apply_effects(effects_enabled)
    end

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

    local alt = math.max(0, cam.y)
    local alt_turb = math.min(0.35, alt * 0.006)
    local sway_x = math.sin(glitch.sway_t * 0.9) * (0.03 + glitch.dread * 0.12 + alt_turb)
    local sway_y = math.cos(glitch.sway_t * 1.3) * (0.02 + glitch.dread * 0.08 + alt_turb * 0.6)

    if flash_alpha > 0 then flash_alpha = math.max(0, flash_alpha - raw_dt * 2.0) end
    if cam.shake   > 0 then cam.shake   = math.max(0, cam.shake   - raw_dt * 6.0) end

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

    for i = #bodies, 1, -1 do
        local b = bodies[i]
        if b.body and b.body:isValid() then
            local _, by, _ = b.body:getPosition()
            if by < -10.0 then
                if b.is_target then score = score + (b.type == "plane" and 500 or 150) end
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

local function draw_toggles()
    crayon.graphics.setColor(0.55, 1.0, 0.55, 0.9)
    local dbg = debug_draw_enabled and "[DBG: ON] " or "[DBG: --] "
    local fxs = effects_enabled    and "[FX: ON] "  or "[FX: --] "
    crayon.graphics.drawText(dbg .. fxs, 8, 220, 0.85)
end

-- ============================================================================
-- Draw
-- ============================================================================
function crayon.draw()
    crayon.graphics.clear(0.02, 0.02, 0.03)

    local g     = glitch.intensity
    local dread = glitch.dread
    local vw, vh = crayon.window.getResolution()

    if effects_enabled then
        crayon.graphics.pushEffect("vignette",  { intensity = 0.55 + dread * 0.3 })
        crayon.graphics.pushEffect("filmGrain", { intensity = 0.18 + g * 0.4 + hud.noise_burst * 0.3 })
        crayon.graphics.pushEffect("vhs",       { intensity = 0.35 + g * 0.55 })
        crayon.graphics.pushEffect("chromatic", { amount    = 0.006 + g * 0.02 })
        crayon.graphics.pushEffect("pixelate",  { size      = 2.0 })
    end

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

    -- ==================================================================
    -- 3D SCENE
    -- Primitives use setModelColor(r,g,b); the C-17 uses baked part colors
    -- ==================================================================

    -- Ground
    tint(COL.ground)
    crayon.graphics.drawPlane(0, 0, 0, 400, 400, tex_white, 0, 0, 0)

    -- Bodies
    for _, b in ipairs(bodies) do
        if b.body and b.body:isValid() then
            local px, py, pz = b.body:getPosition()
            local rx, ry, rz = b.body:getRotation()
            local rrx, rry, rrz = math.rad(rx), math.rad(ry), math.rad(rz)
            local c = b.color or COL.white

            if b.type == "cube" then
                tint(c)
                crayon.graphics.drawCube(px, py, pz, b.sx, b.sy, b.sz, tex_white, rrx, rry, rrz)

            elseif b.type == "cylinder" then
                tint(c)
                local radius = b.sx * 0.5
                local height = b.sy
                crayon.graphics.drawCylinder(px, py, pz, radius, height, tex_white, rrx, rry, rrz)

            elseif b.type == "plane" then
                -- Color is baked into the model via setPartColor at init.
                crayon.graphics.drawModel(models.c17, px, py, pz,
                    rrx, rry, rrz, 1.0, 1.0, 1.0, tex_white)
            end
        end
    end

    -- Shrapnel
    for _, p in ipairs(particles) do
        local c = p.color or COL.shrapnel
        tint(c)
        crayon.graphics.drawCube(p.x, p.y, p.z, p.size, p.size, p.size, tex_white,
            math.rad(p.rx), math.rad(p.ry), math.rad(p.rz))
    end

    -- Projectiles
    for _, p in ipairs(projectiles) do
        if p.body and p.body:isValid() then
            local px, py, pz = p.body:getPosition()
            local r = (p.type == "bomb") and 1.2 or 0.3
            if p.type == "bomb" then tint(COL.bomb) else tint(COL.cannon) end
            crayon.graphics.drawSphere(px, py, pz, r, tex_white, 0, 0, 0)
        end
    end

    -- Explosions
    local raw_dt = crayon.time.getDt()
    for i = #explosions, 1, -1 do
        local ex = explosions[i]
        ex.life = ex.life - (raw_dt * 3.0)
        ex.r    = ex.r + ((ex.max_r - ex.r) * raw_dt * 10.0)

        if ex.life <= 0 then
            table.remove(explosions, i)
        else
            crayon.graphics.setModelColor(ex.color[1], ex.color[2], ex.color[3])
            if ex.type == "sphere" then
                crayon.graphics.drawSphere(ex.x, ex.y, ex.z, ex.r, tex_white, 0, 0, 0)
            elseif ex.type == "shockwave" then
                crayon.graphics.drawCylinder(ex.x, ex.y, ex.z, ex.r, 0.2, tex_white, 0, 0, 0)
            end
        end
    end

    -- Reset tint so nothing after this inherits the last color
    crayon.graphics.setModelColor(1.0, 1.0, 1.0)

    -- Debug overlay
    if debug_draw_enabled then
        crayon.physics3D.drawDebug({
            shapes     = true,
            bounds     = false,
            velocities = true,
            color      = {1.0, 0.35, 0.35, 0.9},
        })
    end

    if effects_enabled then
        crayon.graphics.popEffect()
        crayon.graphics.popEffect()
        crayon.graphics.popEffect()
        crayon.graphics.popEffect()
        crayon.graphics.popEffect()
    end

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
    draw_toggles()

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
    crayon.graphics.drawText(
        "WASD/QA:FLY  ARROWS:LOOK  LMB:CANNON  RMB:BOMB  TAB:DBG  F1:FX  R:RESET",
        8, vh - 8, 0.85)
end