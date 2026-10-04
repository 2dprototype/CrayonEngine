-- ============================================================================
-- PENTAGATE — "THE WEEPING EYE"
-- 3D horror arena defence. Fight what comes through the eye.
-- Crayon Engine — 320x240 virtual, 3D world
--
-- Controls:
--   A / D or ← / →         Move
--   Mouse                  Aim (ray from camera)
--   Left mouse / Space / Z Fire
--   Space or R (on game over) Restart
-- ============================================================================

function crayon.config(c)
    c.window.title = "Pentagate"
    c.window.width  = 320
    c.window.height = 240
    c.window.virtualWidth  = 320
    c.window.virtualHeight = 240
    c.window.vsync   = true
    c.window.scaling = "integer"

    c.modules.physics   = false
    c.modules.physics2D = false
    c.modules.mesh3D    = true      -- REQUIRED: we render the world in 3D
    c.modules.audio     = false
    c.modules.particles = false

    c.graphics.clearColor = {0.0, 0.0, 0.0, 1.0}
end

-- Post-process effect names (rename here if the engine uses other ids)
local FX = {
    CHROMATIC = "chromatic",
    VIGNETTE  = "vignette",
    VHS       = "vhs",
    DISSOLVE  = "dissolve",
}

-- ---------------------------------------------------------------------------
-- World coordinates (metres)
-- ---------------------------------------------------------------------------
local WORLD = {
    PLAY_X       = 6.0,   -- play area half-width on X
    GROUND_Y     = 0.0,
    PLAYER_Z     = 3.0,   -- player sits near the camera
    PORTAL_X     = 0.0,
    PORTAL_Z     = -4.5,  -- portal sits far from the camera
    BULLET_Y     = 0.7,
}

local VW, VH = 320, 240
local WHITE  = nil       -- cached white texture id (for tinted billboards)

-- ---------------------------------------------------------------------------
-- Game state
-- ---------------------------------------------------------------------------
local player, portal, eye, maw
local demons, bullets, particles
local score, wave, waveTimer, gameOver, time, shake
local glitch, red_flash, white_flash, dissolveAmt
local camX, camYaw
local whispers, whisperTimer, dread

-- ---------------------------------------------------------------------------
-- Helpers
-- ---------------------------------------------------------------------------
local function spawn_particle(x, y, z, vx, vy, vz, life, r, g, b, size)
    particles[#particles + 1] = {
        x = x, y = y, z = z,
        vx = vx, vy = vy, vz = vz,
        life = life, maxLife = life,
        r = r, g = g, b = b, size = size or 0.2,
    }
end

local function spawn_burst(x, y, z, n, speed, life, r, g, b, size)
    for _ = 1, n do
        local theta = math.random() * math.pi * 2
        local phi   = math.acos(2 * math.random() - 1)
        local s = speed * (0.4 + math.random() * 0.6)
        spawn_particle(
            x, y, z,
            math.sin(phi) * math.cos(theta) * s,
            math.cos(phi) * s,
            math.sin(phi) * math.sin(theta) * s,
            life * (0.6 + math.random() * 0.6),
            r, g, b, size
        )
    end
end

local function spawn_bullet(x, y, z, dx, dz)
    local speed = 14
    bullets[#bullets + 1] = {
        x = x, y = y, z = z,
        vx = dx * speed,
        vz = dz * speed,
        life = 2.0,
        radius = 0.25,
    }
end

local function spawn_demon()
    -- Spawn from the maw's position
    local jitterX = (math.random() - 0.5) * 1.6
    local jitterZ = (math.random() - 0.5) * 0.4
    local x = WORLD.PORTAL_X + jitterX
    local z = WORLD.PORTAL_Z + jitterZ
    local y = maw.y

    local hp    = 1 + math.floor(wave * 0.5)
    local speed = 1.4 + wave * 0.15 + math.random() * 0.4

    demons[#demons + 1] = {
        x = x, y = y, z = z,
        hp = hp, maxHp = hp,
        speed = speed,
        radius = 0.45,
        spin = math.random() * math.pi * 2,
        spinSpeed = (math.random() - 0.5) * 3,
        hitFlash = 0,
        bounce = math.random() * math.pi * 2,
    }

    spawn_burst(x, y, z, 10, 4, 0.6, 1.0, 0.15, 0.15, 0.15)
    shake  = math.max(shake, 3.0)
    glitch = math.min(1.0, glitch + 0.15)
    maw.open = 1.0    -- maw snaps open on spawn
end

local function damage_player(amount)
    if player.iframe > 0 then return end
    player.hp = player.hp - amount
    player.iframe = 1.2
    shake       = 8.0
    glitch      = 1.0
    red_flash   = 1.0
    spawn_burst(player.x, 0.5, player.z, 22, 5, 0.9, 1.0, 0.35, 0.4, 0.2)
    if player.hp <= 0 then
        gameOver = true
        spawn_burst(player.x, 0.5, player.z, 60, 9, 1.4, 1.0, 0.3, 0.3, 0.3)
        shake       = 14.0
        glitch      = 1.0
        white_flash = 1.0
        crayon.graphics.pushEffect(FX.DISSOLVE, { amount = 0.0, scale = 3.0 })
    end
end

local function kill_demon(d)
    score  = score + 10
    glitch = math.min(1.0, glitch + 0.12)
    spawn_burst(d.x, d.y, d.z, 18, 5, 0.8, 1.0, 0.2, 0.15, 0.2)
    shake  = math.max(shake, 3.0)
end

local GLYPHS = { "#", "@", "%", "&", "$", "█", "▓", "▒", "░", "×", "†", "‡", "Ω", "ϟ" }
local function corrupt(s, chance)
    if math.random() > chance then return s end
    local i = math.random(1, #s)
    local g = GLYPHS[math.random(1, #GLYPHS)]
    return s:sub(1, i - 1) .. g .. s:sub(i + 1)
end

local WHISPER_TEXTS = {
    "we see you",         "come closer",        "the eye opens",
    "you cannot leave",   "feed us",            "one of us",
    "the gate hungers",   "behind you",         "it is awake",
    "let us in",          "you are already ours","open your eyes",
    "look up",            "we never blink",     "the flesh remembers",
    "the maw is patient", "you will join us",   "do not run",
}

local function spawn_whisper()
    local t = WHISPER_TEXTS[math.random(1, #WHISPER_TEXTS)]
    whispers[#whispers + 1] = {
        text = t,
        x = 20 + math.random() * (VW - 120),
        y = 30 + math.random() * (VH - 60),
        life = 2.5 + math.random() * 1.5,
        maxLife = 4.0,
    }
end

-- ---------------------------------------------------------------------------
-- Reset
-- ---------------------------------------------------------------------------
local function reset_game()
    player = {
        x = 0.0, z = WORLD.PLAYER_Z,
        speed = 6.0,
        hp = 5, maxHp = 5,
        fireCd = 0,
        fireRate = 0.16,
        iframe = 0,
        aim = { x = 0, z = -1 },
    }

    portal = {
        x = WORLD.PORTAL_X,
        z = WORLD.PORTAL_Z,
        spin = 0,
        breathe = 0,
        spawnCd = 1.5,
        spawnInterval = 2.0,
    }

    eye = { y = 4.6, lookX = 0, lookZ = 0, blink = 0, blinkTimer = 3 }

    maw = { y = 2.4, open = 0.15 }

    demons    = {}
    bullets   = {}
    particles = {}
    whispers  = {}
    score     = 0
    wave      = 1
    waveTimer = 0
    gameOver  = false
    time      = 0
    shake     = 0

    glitch      = 0
    red_flash   = 0
    white_flash = 0
    dissolveAmt = 0

    camX   = player.x * 0.5
    camYaw = 0
    dread  = 0

    whisperTimer = 3.0

    crayon.graphics.popEffect()
end

-- ---------------------------------------------------------------------------
-- Init
-- ---------------------------------------------------------------------------
function crayon.init()
    math.randomseed(os.time())
    WHITE = crayon.graphics.getWhiteTexture()

    -- 3D lighting: very dim ambient, blood-red point light at portal
    crayon.graphics.setLight(
        0.1, -1.0, 0.2,          -- faint key from above
        0.35, 0.05, 0.06,        -- dim warm-red tint
        0.04, 0.01, 0.02         -- almost-black ambient
    )
    crayon.graphics.setPointLightEnabled(0, true)
    crayon.graphics.setPointLightEnabled(1, true)

    -- PS1-era atmosphere
    crayon.graphics.setRetroEffects({
        jitterResolution = {320, 240},
        affine = 1.0,
        dither = true,
        colorDepth = 32,
        crt = { scanlines = 0.45, curvature = 0.10, vignette = 0.55 },
    })

    crayon.graphics.pushEffect(FX.CHROMATIC, { offset = 1.2, intensity = 0.55 })
    crayon.graphics.pushEffect(FX.VHS,       { strength = 0.14, jitter = 0.18, colorShift = 0.12 })
    crayon.graphics.pushEffect(FX.VIGNETTE,  { intensity = 0.6, softness = 0.7, radius = 0.72 })

    crayon.graphics.setDepthTest(true)
    crayon.graphics.setCullFace(false)

    reset_game()
end

-- ---------------------------------------------------------------------------
-- Update
-- ---------------------------------------------------------------------------
function crayon.update(dt)
    -- Always decay horror channels
    glitch      = math.max(0, glitch      - dt * 2.2)
    red_flash   = math.max(0, red_flash   - dt * 2.5)
    white_flash = math.max(0, white_flash - dt * 3.0)

    if gameOver then
        dissolveAmt = math.min(1.0, dissolveAmt + dt * 0.6)
        crayon.graphics.setEffectUniform(FX.DISSOLVE, "amount", dissolveAmt)

        if crayon.key.isPressed("space") or crayon.key.isPressed("r") then
            reset_game()
        end

        for i = #particles, 1, -1 do
            local p = particles[i]
            p.life = p.life - dt
            if p.life <= 0 then table.remove(particles, i)
            else
                p.x = p.x + p.vx * dt
                p.y = p.y + p.vy * dt
                p.z = p.z + p.vz * dt
                p.vx = p.vx * (1 - dt * 2)
                p.vy = p.vy * (1 - dt * 2)
                p.vz = p.vz * (1 - dt * 2)
            end
        end
        return
    end

    time = time + dt

    -- ---- Player movement (1D along X) ----
    local mv = 0
    if crayon.key.isDown("a") or crayon.key.isDown("left")  then mv = mv - 1 end
    if crayon.key.isDown("d") or crayon.key.isDown("right") then mv = mv + 1 end
    player.x = player.x + mv * player.speed * dt
    player.x = math.max(-WORLD.PLAY_X, math.min(WORLD.PLAY_X, player.x))

    -- ---- Aim: cast mouse ray onto horizontal plane at player Y ----
    local mx, my = crayon.mouse.getPosition()
    local ray = crayon.graphics.getCameraRay(mx, my)
    if ray and ray.origin and ray.direction then
        -- getCameraRay returns array-style tables: [1]=x, [2]=y, [3]=z
        local ox, oy, oz = ray.origin[1], ray.origin[2], ray.origin[3]
        local dx, dy, dz = ray.direction[1], ray.direction[2], ray.direction[3]

        if ox and oy and oz and dx and dy and dz then
            local yTarget = 0.5
            if math.abs(dy) > 0.0001 then
                local t = (yTarget - oy) / dy
                if t > 0 then
                    local wx = ox + dx * t
                    local wz = oz + dz * t
                    local rdx = wx - player.x
                    local rdz = wz - player.z
                    local rdl = math.sqrt(rdx * rdx + rdz * rdz)
                    if rdl > 0.001 then
                        player.aim.x = rdx / rdl
                        player.aim.z = rdz / rdl
                    end
                end
            end
        end
    end
    -- ---- Shooting ----
    player.fireCd = player.fireCd - dt
    local shooting = crayon.mouse.isDown(1)
                  or crayon.key.isDown("space")
                  or crayon.key.isDown("z")
    if shooting and player.fireCd <= 0 then
        player.fireCd = player.fireRate
        local spread = (math.random() - 0.5) * 0.06
        local ang = math.atan2(player.aim.z, player.aim.x) + spread
        local dx = math.cos(ang)
        local dz = math.sin(ang)
        spawn_bullet(
            player.x + dx * 0.5,
            WORLD.BULLET_Y,
            player.z + dz * 0.5,
            dx, dz
        )
        glitch = math.min(1.0, glitch + 0.03)
    end

    -- ---- Portal ----
    portal.spin    = portal.spin    + dt * 0.4
    portal.breathe = portal.breathe + dt

    -- Eye tracks the player with a delay
    local targetLookX = (player.x - portal.x) * 0.12
    local targetLookZ = (player.z - portal.z) * 0.02
    eye.lookX = eye.lookX + (targetLookX - eye.lookX) * math.min(1, dt * 3.0)
    eye.lookZ = eye.lookZ + (targetLookZ - eye.lookZ) * math.min(1, dt * 3.0)

    -- Eye blinks at irregular intervals; slower when calm, faster when near death
    eye.blinkTimer = eye.blinkTimer - dt
    if eye.blinkTimer <= 0 then
        eye.blinkTimer = 2.5 + math.random() * 4.0 - (1 - player.hp / player.maxHp) * 1.5
        eye.blink = 1.0
    end
    eye.blink = math.max(0, eye.blink - dt * 5)

    -- Maw opens wider as wave rises, snaps shut otherwise
    local restOpen = 0.15 + wave * 0.02
    maw.open = math.max(restOpen, maw.open - dt * 1.2)

    -- ---- Wave progression ----
    waveTimer = waveTimer + dt
    if waveTimer > 20 then
        waveTimer = 0
        wave = wave + 1
        shake     = math.max(shake, 5.0)
        glitch    = 1.0
        red_flash = 0.8
        maw.open  = 1.0
        spawn_burst(portal.x, maw.y, portal.z, 80, 8, 1.0, 1.0, 0.1, 0.1, 0.3)
        -- Portal "screams"
        spawn_whisper()
    end

    -- ---- Spawn demons ----
    portal.spawnInterval = math.max(0.4, 2.0 - wave * 0.12)
    portal.spawnCd = portal.spawnCd - dt
    if portal.spawnCd <= 0 then
        portal.spawnCd = portal.spawnInterval
        local n = 1
        if wave >= 3 and math.random() < 0.35 then n = 2 end
        for _ = 1, n do spawn_demon() end
    end

    -- ---- Ambient particles from portal ----
    if math.random() < 0.7 then
        local ang = math.random() * math.pi * 2
        local r = 0.6 + math.random() * 0.5
        spawn_particle(
            portal.x + math.cos(ang) * r,
            0.2 + math.random() * 4.0,
            portal.z + math.sin(ang) * r,
            (math.random() - 0.5) * 1.5,
            0.5 + math.random() * 1.5,
            (math.random() - 0.5) * 1.5,
            1.2, 1.0, 0.15, 0.15, 0.1
        )
    end

    -- ---- Whispers ----
    local newDread = (1 - player.hp / player.maxHp) + glitch * 0.5 + wave * 0.03
    dread = dread + (newDread - dread) * math.min(1, dt * 1.5)

    whisperTimer = whisperTimer - dt
    if whisperTimer <= 0 then
        whisperTimer = 4.0 - dread * 2.5 + math.random() * 2
        spawn_whisper()
    end
    for i = #whispers, 1, -1 do
        local w = whispers[i]
        w.life = w.life - dt
        if w.life <= 0 then table.remove(whispers, i) end
    end

    -- ---- Bullets ----
    for i = #bullets, 1, -1 do
        local b = bullets[i]
        b.x = b.x + b.vx * dt
        b.z = b.z + b.vz * dt
        b.life = b.life - dt

        if b.life <= 0 or math.abs(b.x) > 12 or b.z < -12 or b.z > 8 then
            table.remove(bullets, i)
        else
            if math.random() < 0.7 then
                spawn_particle(b.x, b.y, b.z, 0, 0, 0, 0.3, 1.0, 0.9, 0.4, 0.08)
            end

            for j = #demons, 1, -1 do
                local d = demons[j]
                local dx, dz = d.x - b.x, d.z - b.z
                local rr = d.radius + b.radius
                if dx * dx + dz * dz < rr * rr then
                    d.hp = d.hp - 1
                    d.hitFlash = 0.15
                    if d.hp <= 0 then
                        kill_demon(d)
                        table.remove(demons, j)
                    else
                        spawn_burst(b.x, b.y, b.z, 6, 3, 0.4, 1.0, 0.5, 0.3, 0.12)
                    end
                    table.remove(bullets, i)
                    break
                end
            end
        end
    end

    -- ---- Demons ----
    for i = #demons, 1, -1 do
        local d = demons[i]
        d.spin = d.spin + d.spinSpeed * dt
        d.bounce = d.bounce + dt * 6
        if d.hitFlash > 0 then d.hitFlash = d.hitFlash - dt end

        local dx = player.x - d.x
        local dz = player.z - d.z
        local dist = math.sqrt(dx * dx + dz * dz)
        if dist > 0.001 then
            d.x = d.x + (dx / dist) * d.speed * dt
            d.z = d.z + (dz / dist) * d.speed * dt
        end
        d.y = maw.y + math.sin(d.bounce) * 0.08

        if math.random() < 0.2 then
            spawn_particle(d.x, d.y, d.z,
                (math.random() - 0.5) * 0.8,
                (math.random() - 0.5) * 0.5,
                (math.random() - 0.5) * 0.8,
                0.5, 0.7, 0.1, 0.1, 0.1)
        end

        local pd = (player.x - d.x) ^ 2 + (player.z - d.z) ^ 2
        local pr = d.radius + 0.5
        if pd < pr * pr then
            damage_player(1)
            spawn_burst(d.x, d.y, d.z, 16, 4, 0.7, 1.0, 0.3, 0.3, 0.2)
            table.remove(demons, i)
        end
    end

    -- ---- Particles ----
    for i = #particles, 1, -1 do
        local p = particles[i]
        p.life = p.life - dt
        if p.life <= 0 then
            table.remove(particles, i)
        else
            p.x = p.x + p.vx * dt
            p.y = p.y + p.vy * dt
            p.z = p.z + p.vz * dt
            p.vx = p.vx * (1 - dt * 2)
            p.vy = p.vy * (1 - dt * 2) + (-1.5) * dt   -- slight gravity
            p.vz = p.vz * (1 - dt * 2)
        end
    end

    -- ---- Camera ----
    local targetCamX = player.x * 0.6
    camX = camX + (targetCamX - camX) * math.min(1, dt * 4.0)
    -- Slow menacing yaw drift
    camYaw = math.sin(time * 0.4) * 0.06 + glitch * (math.random() - 0.5) * 0.04

    -- ---- Timers ----
    if player.iframe > 0 then player.iframe = player.iframe - dt end
    if shake > 0 then shake = math.max(0, shake - dt * 20) end

    -- ---- Update lights ----
    local pulse = 1.0 + math.sin(portal.breathe * 2.4) * 0.15
    crayon.graphics.setPointLight(
        0,
        portal.x, eye.y, portal.z,
        1.0, 0.15, 0.18,
        10.0, 3.2 * pulse
    )
    crayon.graphics.setPointLight(
        1,
        player.x, 1.0, player.z,
        0.3, 0.6, 1.0,
        5.0, 0.8
    )
end

-- ---------------------------------------------------------------------------
-- 3D drawing primitives (all tint via setColor + WHITE billboard)
-- ---------------------------------------------------------------------------
local function billboard(x, y, z, w, h, r, g, b, a)
    crayon.graphics.setColor(r, g, b, a or 1.0)
    crayon.graphics.drawBillboard(x, y, z, w, h, WHITE, "spherical")
end

local function draw_floor()
    -- Dark cracked ground plane
    crayon.graphics.setColor(0.05, 0.03, 0.04, 1.0)
    crayon.graphics.drawPlane(0, WORLD.GROUND_Y - 0.01, 0,
        WORLD.PLAY_X * 3, 22, WHITE, 0, 0, 0)

    -- Blood pool under the portal
    for i = 1, 3 do
        local r = 3.5 + i * 0.6
        billboard(portal.x, WORLD.GROUND_Y + 0.02, portal.z + 1.5,
                  r * 2, r * 2,
                  0.25, 0.02, 0.03, 0.35 - i * 0.08)
    end
end

-- ---------------------------------------------------------------------------
-- Portal — redesigned as a weeping eye-tower
-- ---------------------------------------------------------------------------
local function draw_portal()
    local breathe = 1.0 + math.sin(portal.breathe * 2.2) * 0.06
    local px, pz  = portal.x, portal.z

    -- ==== Base / Fleshy stalk ====
    -- Wide base cylinder
    crayon.graphics.setColor(0.4, 0.15, 0.16, 1.0)
    crayon.graphics.drawCylinder(px, 0.3, pz, 1.6 * breathe, 0.6, WHITE, 0, 0, 0)
    -- Tapered mid section
    crayon.graphics.setColor(0.3, 0.08, 0.09, 1.0)
    crayon.graphics.drawCylinder(px, 1.4, pz, 1.1 * breathe, 1.6, WHITE, 0, 0, 0)
    -- Narrow neck
    crayon.graphics.setColor(0.35, 0.1, 0.1, 1.0)
    crayon.graphics.drawCylinder(px, 2.6, pz, 0.7 * breathe, 0.8, WHITE, 0, 0, 0)

    -- ==== The Maw ====
    local open = maw.open
    local mawY = 2.4

    -- Dark void (a black sphere flattened)
    billboard(px, mawY, pz + 0.55,
              1.4, 2.0 * (0.4 + open),
              0.02, 0.0, 0.0, 1.0)
    -- Inner red glow
    billboard(px, mawY, pz + 0.56,
              1.0, 1.4 * (0.3 + open),
              0.6, 0.05, 0.08, 0.7 * (0.4 + open * 0.6))

    -- Upper lip / teeth row
    local teethTopY = mawY + 0.9 + open * 0.3
    local teethBotY = mawY - 0.9 - open * 0.3
    for i = -3, 3 do
        local tx = px + i * 0.24
        -- upper fangs
        crayon.graphics.setColor(0.85, 0.82, 0.7, 1.0)
        crayon.graphics.drawCone(tx, teethTopY, pz + 0.5, 0.10, 0.35, WHITE,
            math.pi, 0, 0)   -- point downward
        crayon.graphics.drawCone(tx, teethBotY, pz + 0.5, 0.10, 0.35, WHITE,
            0, 0, 0)         -- point upward
    end

    -- ==== The Eye ====
    local eyeY = eye.y * (1.0 + math.sin(portal.breathe * 1.7) * 0.02)

    -- Sclera (white sphere, slightly bloodshot tint)
    crayon.graphics.setColor(0.9, 0.85, 0.82, 1.0)
    crayon.graphics.drawSphere(px, eyeY, pz, 0.9, WHITE, 0, 0, 0)

    -- Bloodshot veins — thin red lines around the eye
    for i = 1, 10 do
        local ang = (i / 10) * math.pi * 2 + portal.spin * 0.3
        local r0 = 0.35
        local r1 = 0.85
        crayon.graphics.setColor(0.7, 0.1, 0.1, 0.6)
        crayon.graphics.drawLine3D(
            px + math.cos(ang) * r0, eyeY + math.sin(ang) * r0, pz - 0.55,
            px + math.cos(ang) * r1, eyeY + math.sin(ang) * r1, pz - 0.55
        )
    end

    -- Iris (bright red ring, tracking the player)
    billboard(px + eye.lookX * 0.5, eyeY + eye.lookZ * 0.3, pz - 0.85,
              0.95, 0.95,
              1.0, 0.15, 0.12, 1.0)

    -- Pupil (black, tracks further)
    local pupilScale = 0.55 + glitch * 0.15
    billboard(px + eye.lookX * 0.7, eyeY + eye.lookZ * 0.4, pz - 0.9,
              0.55 * pupilScale, 0.55 * pupilScale,
              0.02, 0.0, 0.0, 1.0)

    -- Eyelid (drops when blinking)
    if eye.blink > 0.01 then
        local lidH = 1.8 * eye.blink
        billboard(px, eyeY + 0.9 - lidH * 0.5, pz - 1.05,
                  1.9, lidH,
                  0.2, 0.05, 0.05, 1.0)
    end

    -- ==== Tendrils reaching from the base ====
    for i = 1, 8 do
        local baseAng = (i / 8) * math.pi * 2
        local bx = px + math.cos(baseAng) * 1.4
        local bz = pz + math.sin(baseAng) * 1.4
        local px0, py0, pz0 = bx, 0.15, bz

        for s = 1, 5 do
            local reach = s * 0.9
            local wave  = math.sin(time * 1.7 + i * 1.3 + s * 0.6) * 0.35
            local lift  = math.sin(time * 2.3 + i + s * 0.4) * 0.2 + 0.15
            local nx = bx + math.cos(baseAng) * reach + wave * math.cos(baseAng + math.pi / 2)
            local ny = 0.15 + lift * (1 - s / 8)
            local nz = bz + math.sin(baseAng) * reach + wave * math.sin(baseAng + math.pi / 2)

            -- color fades redder as the tendril reaches out
            local t = s / 5
            crayon.graphics.setColor(0.7 - t * 0.3, 0.05, 0.08, 1.0 - t * 0.6)
            crayon.graphics.drawLine3D(px0, py0, pz0, nx, ny, nz)

            px0, py0, pz0 = nx, ny, nz
        end
    end

    -- ==== Ring of runes rotating around the eye ====
    for i = 1, 12 do
        local ang = (i / 12) * math.pi * 2 + portal.spin
        local rx = px + math.cos(ang) * 1.6
        local ry = eyeY + math.sin(ang) * 1.6
        -- billboard glyph stand-in
        crayon.graphics.setColor(1.0, 0.3, 0.15, 0.85)
        crayon.graphics.drawPoint(rx, ry, 1.5)
        -- also draw in 3D as a point via a tiny billboard
        billboard(rx, ry, pz, 0.15, 0.15, 1.0, 0.3, 0.15, 0.9)
    end
end

-- ---------------------------------------------------------------------------
-- Entities
-- ---------------------------------------------------------------------------
local function draw_player()
    local blink = player.iframe > 0 and (math.floor(time * 20) % 2 == 0)
    if blink then return end

    -- Ground shadow
    billboard(player.x, WORLD.GROUND_Y + 0.02, player.z,
              1.2, 0.8, 0.0, 0.0, 0.0, 0.55)

    -- Aura (cyan)
    billboard(player.x, 0.5, player.z,
              1.6, 1.6,
              0.2, 0.7, 1.0, 0.35 * (1 - dread * 0.5))

    -- Body (cone = wizard robe)
    crayon.graphics.setColor(0.25, 0.55, 0.9, 1.0)
    crayon.graphics.drawCone(player.x, 0.0, player.z, 0.5, 1.0, WHITE, 0, 0, 0)

    -- Head (small sphere)
    crayon.graphics.setColor(0.9, 0.95, 1.0, 1.0)
    crayon.graphics.drawSphere(player.x, 1.05, player.z, 0.28, WHITE, 0, 0, 0)

    -- Wand / aim indicator (line to aim direction)
    local ax = player.x + player.aim.x * 1.2
    local az = player.z + player.aim.z * 1.2
    crayon.graphics.setColor(0.6, 0.9, 1.0, 0.9)
    crayon.graphics.drawLine3D(player.x, 0.7, player.z, ax, 0.7, az)

    -- Muzzle glow at wand tip
    billboard(ax, 0.7, az, 0.4, 0.4, 1.0, 0.9, 0.4, 0.8)
end

local function draw_demons()
    for _, d in ipairs(demons) do
        -- Shadow
        billboard(d.x, WORLD.GROUND_Y + 0.02, d.z,
                  d.radius * 1.8, d.radius * 1.2,
                  0.0, 0.0, 0.0, 0.45)

        -- Body: small red sphere
        if d.hitFlash > 0 then
            crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
        else
            local flick = 0.85 + math.sin(time * 20 + d.spin * 3) * 0.15 * (0.4 + glitch)
            crayon.graphics.setColor(flick, 0.1, 0.15, 1.0)
        end
        crayon.graphics.drawSphere(d.x, d.y, d.z, d.radius, WHITE, 0, d.spin, 0)

        -- Glowing eye
        billboard(d.x, d.y + 0.05, d.z - d.radius - 0.02,
                  0.18, 0.18,
                  1.0, glitch > 0.4 and 0.0 or 0.85, 0.2, 1.0)

        -- HP pip
        if d.hp < d.maxHp then
            local w = d.radius * 2 * (d.hp / d.maxHp)
            billboard(d.x, d.y + d.radius + 0.3, d.z, w, 0.08,
                      1.0, 0.15, 0.15, 0.9)
            billboard(d.x, d.y + d.radius + 0.3, d.z, d.radius * 2, 0.08,
                      0.15, 0.02, 0.02, 0.9)
        end
    end
end

local function draw_bullets()
    for _, b in ipairs(bullets) do
        -- Glow
        billboard(b.x, b.y, b.z, 0.7, 0.7, 1.0, 0.85, 0.3, 0.35)
        -- Core
        billboard(b.x, b.y, b.z, 0.3, 0.3, 1.0, 1.0, 0.7, 1.0)
        -- Trail
        local tx = b.x - b.vx * 0.02
        local tz = b.z - b.vz * 0.02
        crayon.graphics.setColor(1.0, 0.6, 0.2, 0.6)
        crayon.graphics.drawLine3D(b.x, b.y, b.z, tx, b.y, tz)
    end
end

local function draw_particles()
    for _, p in ipairs(particles) do
        local t = p.life / p.maxLife
        billboard(p.x, p.y, p.z, p.size * 4, p.size * 4,
                  p.r, p.g, p.b, t * 0.9)
    end
end

-- ---------------------------------------------------------------------------
-- 2D overlays (HUD, whispers, glitch, game over)
-- ---------------------------------------------------------------------------
local function draw_hud()
    crayon.graphics.setColor(0.03, 0.01, 0.04, 0.85)
    crayon.graphics.drawRect("fill", 0, 0, VW, 12)
    crayon.graphics.setColor(0.4, 0.05, 0.1, 0.9)
    crayon.graphics.drawLine(0, 12, VW, 12, 1)

    crayon.graphics.setColor(1.0, 0.85, 0.3, 1.0)
    local sStr = "SCORE " .. score
    if glitch > 0.3 then sStr = corrupt(sStr, glitch * 0.4) end
    crayon.graphics.drawText(sStr, 4, 2, 1.0)

    crayon.graphics.setColor(1.0, 0.4, 0.4, 1.0)
    local wStr = "WAVE " .. wave
    if glitch > 0.3 then wStr = corrupt(wStr, glitch * 0.4) end
    crayon.graphics.drawText(wStr, VW - 52, 2, 1.0)

    local cx = VW * 0.5 - (player.maxHp * 5) + 5
    for i = 1, player.maxHp do
        local hx = cx + (i - 1) * 10
        if i <= player.hp then
            if player.hp <= 2 and math.random() < 0.2 then
                crayon.graphics.setColor(0.6, 0.1, 0.15, 1.0)
            else
                crayon.graphics.setColor(1.0, 0.2, 0.3, 1.0)
            end
            crayon.graphics.drawCircle("fill", hx, 6, 3, 8)
            crayon.graphics.setColor(1.0, 0.6, 0.6, 1.0)
            crayon.graphics.drawPoint(hx - 1, 5, 1.0)
        else
            crayon.graphics.setColor(0.2, 0.05, 0.08, 1.0)
            crayon.graphics.drawCircle("fill", hx, 6, 3, 8)
        end
    end

    crayon.graphics.setColor(0.6, 0.6, 0.7, 0.5)
    crayon.graphics.drawText("A/D: Move  |  Mouse/Space: Shoot", 6, VH - 9, 1.0)
end

local function draw_whispers()
    for _, w in ipairs(whispers) do
        local t = w.life / w.maxLife
        -- Fade in first 30%, out last 30%
        local a = math.min(t / 0.3, (1 - t) / 0.3, 1.0)
        a = math.max(0, a)
        a = a * (0.35 + dread * 0.6)

        -- Slight jitter for unstable reading
        local jx = (math.random() - 0.5) * (0.5 + glitch * 2)
        local jy = (math.random() - 0.5) * (0.5 + glitch * 2)

        -- Draw a faint dark backing so it reads on bright areas
        crayon.graphics.setColor(0.0, 0.0, 0.0, a * 0.5)
        crayon.graphics.drawRect("fill", w.x - 2 + jx, w.y - 1 + jy, #w.text * 6 + 4, 10)

        crayon.graphics.setColor(1.0, 0.15, 0.15, a)
        local txt = w.text
        if glitch > 0.2 then txt = corrupt(txt, glitch * 0.35) end
        crayon.graphics.drawText(txt, w.x + jx, w.y + jy, 1.0)
    end
end

local function draw_glitch_overlay()
    if glitch < 0.05 then return end

    local count = math.floor(glitch * 120)
    for _ = 1, count do
        local x = math.random() * VW
        local y = math.random() * VH
        local b = 0.4 + math.random() * 0.6
        crayon.graphics.setColor(b, b * 0.9, b * 0.9, 0.4)
        crayon.graphics.drawPoint(x, y, 1.0)
    end

    if glitch > 0.3 then
        local band_y = (time * 240) % VH
        crayon.graphics.setColor(1.0, 0.05, 0.1, 0.10 + glitch * 0.15)
        crayon.graphics.drawRect("fill", 0, band_y, VW, 3)
    end

    if glitch > 0.5 and math.random() < 0.4 then
        local y = math.random() * VH
        local h = 2 + math.random() * 6
        local off = (math.random() - 0.5) * glitch * 24
        crayon.graphics.setColor(1.0, 0.15, 0.2, 0.14)
        crayon.graphics.drawRect("fill", off, y, VW, h)
    end

    -- Occasional skull silhouette hint (just a rounded dark blob)
    if glitch > 0.65 and math.random() < 0.08 then
        local sx = math.random() * VW
        local sy = math.random() * VH
        crayon.graphics.setColor(0.0, 0.0, 0.0, 0.6)
        crayon.graphics.drawCircle("fill", sx, sy, 14, 12)
        crayon.graphics.drawCircle("fill", sx - 5, sy - 2, 3, 6)
        crayon.graphics.drawCircle("fill", sx + 5, sy - 2, 3, 6)
        crayon.graphics.setColor(1.0, 0.1, 0.1, 0.8)
        crayon.graphics.drawPoint(sx - 5, sy - 2, 2.0)
        crayon.graphics.drawPoint(sx + 5, sy - 2, 2.0)
    end
end

local function draw_game_over()
    crayon.graphics.setColor(0.4, 0.0, 0.05, 0.5 + dissolveAmt * 0.3)
    crayon.graphics.drawRect("fill", 0, 0, VW, VH)

    local msg = "THE EYE HAS TAKEN YOU"
    if dissolveAmt > 0.3 then msg = corrupt(msg, 0.3) end
    crayon.graphics.setColor(1.0, 0.15, 0.15, 1.0)
    crayon.graphics.drawText(msg, VW * 0.5 - 72, VH * 0.5 - 20, 1.6)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Final score: " .. score, VW * 0.5 - 42, VH * 0.5 + 4, 1.0)

    crayon.graphics.setColor(1.0, 0.85, 0.3, 1.0)
    crayon.graphics.drawText("Press SPACE or R to descend again",
        VW * 0.5 - 96, VH * 0.5 + 24, 1.0)
end

-- ---------------------------------------------------------------------------
-- Main draw
-- ---------------------------------------------------------------------------
function crayon.draw()
    -- Update post-process from glitch / dread
    crayon.graphics.setEffectUniform(FX.CHROMATIC, "offset",    1.0 + glitch * 7.0)
    crayon.graphics.setEffectUniform(FX.CHROMATIC, "intensity", 0.35 + glitch * 0.6)
    crayon.graphics.setEffectUniform(FX.VHS, "strength",   0.12 + glitch * 0.55)
    crayon.graphics.setEffectUniform(FX.VHS, "jitter",     0.14 + glitch * 0.65)
    crayon.graphics.setEffectUniform(FX.VHS, "colorShift", 0.10 + glitch * 0.4)
    crayon.graphics.setEffectUniform(FX.VIGNETTE, "intensity",
        0.6 + dread * 0.3 + glitch * 0.2)

    -- Occasional render skip for frame stutter
    if glitch > 0.85 and math.random() < 0.12 then return end

    crayon.graphics.clear(0.0, 0.0, 0.0, 1.0)

    -- -----------------------------------------------------------------------
    -- 3D WORLD
    -- -----------------------------------------------------------------------
    crayon.graphics.setDepthTest(true)

    local swayX = math.sin(time * 0.6) * 0.15
    local swayY = math.sin(time * 0.5 + 1.3) * 0.1
    local shakeX, shakeY = 0, 0
    if shake > 0 then
        shakeX = (math.random() - 0.5) * shake * 0.02
        shakeY = (math.random() - 0.5) * shake * 0.02
    end

    crayon.graphics.setCamera3D({
        position = {
            camX + swayX + shakeX,
            8.0 + swayY + shakeY,
            9.0,
        },
        target = {
            camX * 0.7 + camYaw * 2.0,
            0.5,
            -2.0,
        },
        up = {0, 1, 0},
        fov = 55.0,
        near = 0.1,
        far = 40.0,
    })

    draw_floor()
    draw_portal()

    -- Sort demons and particles back-to-front by Z for a rough draw order
    -- (the mesh renderer likely does depth testing, but sorting helps billboards)
    table.sort(demons, function(a, b) return a.z > b.z end)
    table.sort(particles, function(a, b) return a.z > b.z end)

    draw_demons()
    draw_bullets()
    draw_particles()
    draw_player()

    -- -----------------------------------------------------------------------
    -- 2D OVERLAYS
    -- -----------------------------------------------------------------------
    crayon.graphics.setDepthTest(false)

    draw_whispers()
    draw_glitch_overlay()

    if red_flash > 0 then
        crayon.graphics.setColor(1.0, 0.05, 0.05, red_flash * 0.5)
        crayon.graphics.drawRect("fill", 0, 0, VW, VH)
    end
    if white_flash > 0 then
        crayon.graphics.setColor(1.0, 1.0, 1.0, white_flash * 0.75)
        crayon.graphics.drawRect("fill", 0, 0, VW, VH)
    end

    draw_hud()
    if gameOver then draw_game_over() end
end