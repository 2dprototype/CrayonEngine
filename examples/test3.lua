-- ============================================================================
-- PENTAGATE — HORROR GLITCH EDITION
-- Defend against the demon horde pouring from the red pentagon portal.
-- Crayon Engine — 320x240 virtual resolution
--
-- Controls:
--   A / D or ← / →  Move
--   Mouse           Aim
--   Left mouse / Space / Z   Fire
--   Space or R (on game over) Restart
-- ============================================================================

function crayon.config(c)
    c.window.title = "Pentagate"
    c.window.width = 320
    c.window.height = 240
    c.window.virtualWidth  = 320
    c.window.virtualHeight = 240
    c.window.vsync   = true
    c.window.scaling = "integer"

    -- 2D-only game: disable expensive modules
    c.modules.physics   = false
    c.modules.physics2d = false
    c.modules.mesh3D    = false
    c.modules.audio     = false

    c.graphics.clearColor = {0.0, 0.0, 0.0, 1.0}
end

-- ---------------------------------------------------------------------------
-- Effect name constants (rename here if the engine uses different ids)
-- ---------------------------------------------------------------------------
local FX = {
    CHROMATIC = "chromatic",
    VIGNETTE  = "vignette",
    VHS       = "vhs",
    DISSOLVE  = "dissolve",
}

local VW, VH = 320, 240

-- ---------------------------------------------------------------------------
-- Game state
-- ---------------------------------------------------------------------------
local player, portal
local demons, bullets, particles, blood_drips
local score, wave, waveTimer, gameOver, time, shake

-- Horror / glitch state
local glitch        = 0    -- 0..1, drives nearly every horror effect
local red_flash     = 0    -- 0..1, full-screen red pulse
local white_flash   = 0    -- 0..1, full-screen white pulse
local dissolveAmt   = 0    -- 0..1, only used during game over

-- ---------------------------------------------------------------------------
-- Helpers
-- ---------------------------------------------------------------------------
local function pentagon(cx, cy, r, rot)
    local pts = {}
    for i = 0, 4 do
        local a = (i / 5) * math.pi * 2 - math.pi / 2 + rot
        pts[#pts + 1] = { cx + math.cos(a) * r, cy + math.sin(a) * r }
    end
    return pts
end

local function spawn_particle(x, y, vx, vy, life, r, g, b, size)
    particles[#particles + 1] = {
        x = x, y = y, vx = vx, vy = vy,
        life = life, maxLife = life,
        r = r, g = g, b = b, size = size or 1.5,
    }
end

local function spawn_burst(x, y, n, speed, life, r, g, b, size)
    for _ = 1, n do
        local a = math.random() * math.pi * 2
        local s = speed * (0.4 + math.random() * 0.6)
        spawn_particle(
            x, y,
            math.cos(a) * s, math.sin(a) * s,
            life * (0.6 + math.random() * 0.6),
            r, g, b, size
        )
    end
end

local function spawn_bullet(x, y, angle)
    local speed = 260
    bullets[#bullets + 1] = {
        x = x, y = y,
        vx = math.cos(angle) * speed,
        vy = math.sin(angle) * speed,
        life = 1.6,
        radius = 2,
    }
end

local function spawn_demon()
    local idx = math.random(0, 4)
    local a   = (idx / 5) * math.pi * 2 - math.pi / 2 + portal.spin
    local x   = portal.x + math.cos(a) * portal.radius
    local y   = portal.y + math.sin(a) * portal.radius

    local hp    = 1 + math.floor(wave * 0.5)
    local speed = 22 + wave * 3 + math.random() * 8

    demons[#demons + 1] = {
        x = x, y = y,
        hp = hp, maxHp = hp,
        speed = speed,
        radius = 6,
        spin = math.random() * math.pi * 2,
        spinSpeed = (math.random() - 0.5) * 4,
        hitFlash = 0,
    }

    spawn_burst(x, y, 8, 40, 0.5, 1.0, 0.2, 0.2, 1.6)
    shake  = math.max(shake, 2.0)
    glitch = math.min(1.0, glitch + 0.12)
end

local function damage_player(amount)
    if player.iframe > 0 then return end
    player.hp = player.hp - amount
    player.iframe = 1.2
    shake       = 6.0
    glitch      = 1.0
    red_flash   = 1.0
    spawn_burst(player.x, player.y, 16, 70, 0.8, 1.0, 0.4, 0.4, 2.0)
    if player.hp <= 0 then
        gameOver = true
        spawn_burst(player.x, player.y, 40, 120, 1.2, 1.0, 0.3, 0.3, 2.4)
        shake     = 12.0
        glitch    = 1.0
        white_flash = 1.0
        -- push a dissolving shader overlay that never gets popped until restart
        crayon.graphics.pushEffect(FX.DISSOLVE, { amount = 0.0, scale = 3.0 })
    end
end

local function kill_demon(d)
    score  = score + 10
    glitch = math.min(1.0, glitch + 0.10)
    spawn_burst(d.x, d.y, 14, 60, 0.7, 1.0, 0.25, 0.15, 1.8)
    shake  = math.max(shake, 2.5)
end

-- Corruption: replace a random char in a string with a demonic glyph
local GLYPHS = { "#", "@", "%", "&", "$", "█", "▓", "▒", "░", "×", "†", "‡" }
local function corrupt(s, chance)
    if math.random() > chance then return s end
    local i = math.random(1, #s)
    local g = GLYPHS[math.random(1, #GLYPHS)]
    return s:sub(1, i - 1) .. g .. s:sub(i + 1)
end

-- ---------------------------------------------------------------------------
-- Lifecycle
-- ---------------------------------------------------------------------------
local function reset_game()
    player = {
        x = VW * 0.5, y = 220,
        speed = 110,
        hp = 5, maxHp = 5,
        fireCd = 0,
        fireRate = 0.16,
        iframe = 0,
        aimAngle = -math.pi / 2,
    }

    portal = {
        x = VW * 0.5, y = 60,
        radius = 26,
        pulse = 0,
        spin = 0,
        spawnCd = 1.5,
        spawnInterval = 1.6,
    }

    demons      = {}
    bullets     = {}
    particles   = {}
    blood_drips = {}
    score       = 0
    wave        = 1
    waveTimer   = 0
    gameOver    = false
    time        = 0
    shake       = 0

    glitch      = 0
    red_flash   = 0
    white_flash = 0
    dissolveAmt = 0

    -- Pop the dissolve overlay that was left over from the last death
    crayon.graphics.popEffect()
end

function crayon.init()
    math.randomseed(os.time())

    -- Persistent CRT / pixel-grid atmosphere. Set once, never removed.
    crayon.graphics.setRetroEffects({
        jitterResolution = {320, 240},
        affine = 1.0,
        dither = true,
        colorDepth = 32,
        crt = {
            scanlines = 0.40,
            curvature = 0.08,
            vignette  = 0.45,
        },
    })

    -- Persistent post-process stack (bottom → top):
    --   chromatic aberration  (bleeds red/blue on every pixel)
    --   vhs                    (horizontal jitter + color shift)
    --   vignette               (dark tunnel edges)
    crayon.graphics.pushEffect(FX.CHROMATIC, {
        offset    = 1.2,
        intensity = 0.5,
        angle     = 0.0,
    })
    crayon.graphics.pushEffect(FX.VHS, {
        strength    = 0.12,
        jitter      = 0.15,
        colorShift  = 0.10,
    })
    crayon.graphics.pushEffect(FX.VIGNETTE, {
        intensity = 0.55,
        softness  = 0.65,
        radius    = 0.75,
    })

    reset_game()
end

function crayon.update(dt)
    -- Decay horror channels every frame — regardless of game state
    glitch      = math.max(0, glitch      - dt * 2.2)
    red_flash   = math.max(0, red_flash   - dt * 2.5)
    white_flash = math.max(0, white_flash - dt * 3.0)

    if gameOver then
        -- Dissolve out the whole screen over ~1.5s, then freeze
        dissolveAmt = math.min(1.0, dissolveAmt + dt * 0.7)
        crayon.graphics.setEffectUniform(FX.DISSOLVE, "amount", dissolveAmt)

        if crayon.key.isPressed("space") or crayon.key.isPressed("r") then
            reset_game()
        end

        -- Keep particles alive so the death burst fades
        for i = #particles, 1, -1 do
            local p = particles[i]
            p.life = p.life - dt
            if p.life <= 0 then
                table.remove(particles, i)
            else
                p.x = p.x + p.vx * dt
                p.y = p.y + p.vy * dt
                p.vx = p.vx * (1 - dt * 2)
                p.vy = p.vy * (1 - dt * 2)
            end
        end
        return
    end

    time = time + dt

    -- ---- Player movement ----
    local mv = 0
    if crayon.key.isDown("a") or crayon.key.isDown("left")  then mv = mv - 1 end
    if crayon.key.isDown("d") or crayon.key.isDown("right") then mv = mv + 1 end
    player.x = player.x + mv * player.speed * dt
    player.x = math.max(12, math.min(VW - 12, player.x))

    -- ---- Aim ----
    local mx, my = crayon.mouse.getPosition()
    player.aimAngle = math.atan2(my - player.y, mx - player.x)

    -- ---- Shooting ----
    player.fireCd = player.fireCd - dt
    local shooting = crayon.mouse.isDown(1)
                  or crayon.key.isDown("space")
                  or crayon.key.isDown("z")
    if shooting and player.fireCd <= 0 then
        player.fireCd = player.fireRate
        local spread = (math.random() - 0.5) * 0.06
        spawn_bullet(
            player.x + math.cos(player.aimAngle) * 6,
            player.y + math.sin(player.aimAngle) * 6,
            player.aimAngle + spread
        )
        glitch = math.min(1.0, glitch + 0.03)
    end

    -- ---- Portal ----
    portal.pulse = portal.pulse + dt
    portal.spin  = portal.spin  + dt * 0.35

    waveTimer = waveTimer + dt
    if waveTimer > 20 then
        waveTimer = 0
        wave = wave + 1
        shake       = math.max(shake, 4.0)
        glitch      = 1.0
        red_flash   = 0.75
        spawn_burst(portal.x, portal.y, 60, 90, 1.0, 1.0, 0.15, 0.15, 2.2)
    end

    portal.spawnInterval = math.max(0.35, 1.6 - wave * 0.1)
    portal.spawnCd = portal.spawnCd - dt
    if portal.spawnCd <= 0 then
        portal.spawnCd = portal.spawnInterval
        local n = 1
        if wave >= 3 and math.random() < 0.35 then n = 2 end
        for _ = 1, n do spawn_demon() end
    end

    -- Ambient embers drifting out of the portal
    if math.random() < 0.6 then
        local a = math.random() * math.pi * 2
        local r = portal.radius * (0.6 + math.random() * 0.6)
        spawn_particle(
            portal.x + math.cos(a) * r,
            portal.y + math.sin(a) * r,
            math.cos(a) * 10, math.sin(a) * 10 - 15,
            0.8, 1.0, 0.15, 0.15, 1.2
        )
    end

    -- Low HP: spawn blood drips running down the screen
    if player.hp <= 2 and math.random() < 0.35 then
        blood_drips[#blood_drips + 1] = {
            x = math.random() * VW,
            y = -4,
            vy = 20 + math.random() * 30,
            life = 6 + math.random() * 4,
            maxLife = 10,
            w = 1 + math.random() * 2,
        }
    end

    -- ---- Bullets ----
    for i = #bullets, 1, -1 do
        local b = bullets[i]
        b.x = b.x + b.vx * dt
        b.y = b.y + b.vy * dt
        b.life = b.life - dt

        if b.life <= 0 or b.x < -8 or b.x > VW + 8 or b.y < -8 or b.y > VH + 8 then
            table.remove(bullets, i)
        else
            if math.random() < 0.7 then
                spawn_particle(b.x, b.y, 0, 0, 0.25, 1.0, 0.9, 0.4, 1.2)
            end

            for j = #demons, 1, -1 do
                local d = demons[j]
                local dx, dy = d.x - b.x, d.y - b.y
                local rr = d.radius + b.radius
                if dx * dx + dy * dy < rr * rr then
                    d.hp = d.hp - 1
                    d.hitFlash = 0.15
                    if d.hp <= 0 then
                        kill_demon(d)
                        table.remove(demons, j)
                    else
                        spawn_burst(b.x, b.y, 5, 40, 0.35, 1.0, 0.5, 0.3, 1.4)
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
        if d.hitFlash > 0 then d.hitFlash = d.hitFlash - dt end

        local dx = player.x - d.x
        local dy = player.y - d.y
        local dist = math.sqrt(dx * dx + dy * dy)
        if dist > 0.001 then
            d.x = d.x + (dx / dist) * d.speed * dt
            d.y = d.y + (dy / dist) * d.speed * dt
        end

        if math.random() < 0.25 then
            spawn_particle(d.x, d.y,
                (math.random() - 0.5) * 10, (math.random() - 0.5) * 10,
                0.4, 0.7, 0.1, 0.1, 1.0)
        end

        local pd = (player.x - d.x) ^ 2 + (player.y - d.y) ^ 2
        local pr = d.radius + 8
        if pd < pr * pr then
            damage_player(1)
            spawn_burst(d.x, d.y, 12, 60, 0.6, 1.0, 0.3, 0.3, 1.8)
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
            p.vx = p.vx * (1 - dt * 2)
            p.vy = p.vy * (1 - dt * 2)
        end
    end

    -- ---- Blood drips ----
    for i = #blood_drips, 1, -1 do
        local bd = blood_drips[i]
        bd.y = bd.y + bd.vy * dt
        bd.life = bd.life - dt
        if bd.life <= 0 or bd.y > VH + 10 then
            table.remove(blood_drips, i)
        end
    end

    -- ---- Timers ----
    if player.iframe > 0 then player.iframe = player.iframe - dt end
    if shake > 0 then shake = math.max(0, shake - dt * 20) end
end

-- ---------------------------------------------------------------------------
-- Rendering
-- ---------------------------------------------------------------------------
local function draw_portal()
    local pr = portal.radius + math.sin(portal.pulse * 4.0) * 2

    for i = 4, 1, -1 do
        local a = 0.05 + (5 - i) * 0.05
        crayon.graphics.setColor(0.9, 0.05, 0.1, a)
        crayon.graphics.drawCircle("fill", portal.x, portal.y, pr + i * 8, 32)
    end

    crayon.graphics.setColor(0.85, 0.05, 0.08, 0.9)
    crayon.graphics.drawPolygon("fill", pentagon(portal.x, portal.y, pr, portal.spin))

    crayon.graphics.setColor(0.4, 0.02, 0.04, 1.0)
    crayon.graphics.drawPolygon("fill", pentagon(portal.x, portal.y, pr * 0.78, portal.spin + 0.6))

    crayon.graphics.setColor(0.03, 0.0, 0.0, 1.0)
    crayon.graphics.drawPolygon("fill", pentagon(portal.x, portal.y, pr * 0.5, -portal.spin))

    crayon.graphics.setColor(1.0, 0.25, 0.2, 1.0)
    crayon.graphics.drawPolygon("line", pentagon(portal.x, portal.y, pr, portal.spin))

    local verts = pentagon(portal.x, portal.y, pr * 0.72, portal.spin)
    crayon.graphics.setColor(1.0, 0.5, 0.3, 0.75)
    for i = 1, 5 do
        local j = ((i + 1) % 5) + 1
        crayon.graphics.drawLine(verts[i][1], verts[i][2], verts[j][1], verts[j][2], 1)
    end
end

local function draw_demons()
    for _, d in ipairs(demons) do
        crayon.graphics.setColor(0, 0, 0, 0.4)
        crayon.graphics.drawCircle("fill", d.x, d.y + 4, d.radius * 0.9, 10)

        if d.hitFlash > 0 then
            crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
        else
            -- Demons briefly flicker brighter as glitch rises
            local flick = 0.9 + math.sin(time * 30 + d.spin * 5) * 0.1 * (0.3 + glitch)
            crayon.graphics.setColor(flick, 0.1, 0.15, 1.0)
        end
        crayon.graphics.drawPolygon("fill", pentagon(d.x, d.y, d.radius, d.spin))

        crayon.graphics.setColor(0.05, 0.0, 0.0, 1.0)
        crayon.graphics.drawCircle("fill", d.x, d.y, d.radius * 0.45, 8)

        -- Eyes pulse red during glitch
        if glitch > 0.4 and math.random() < 0.3 then
            crayon.graphics.setColor(1.0, 0.0, 0.0, 1.0)
        else
            crayon.graphics.setColor(1.0, 0.9, 0.3, 1.0)
        end
        crayon.graphics.drawPoint(d.x - 2, d.y - 1, 1.5)
        crayon.graphics.drawPoint(d.x + 2, d.y - 1, 1.5)

        if d.hp < d.maxHp then
            crayon.graphics.setColor(0.2, 0.0, 0.0, 0.9)
            crayon.graphics.drawRect("fill", d.x - 6, d.y - d.radius - 5, 12, 2)
            crayon.graphics.setColor(1.0, 0.2, 0.2, 1.0)
            crayon.graphics.drawRect("fill", d.x - 6, d.y - d.radius - 5, 12 * (d.hp / d.maxHp), 2)
        end
    end
end

local function draw_bullets()
    for _, b in ipairs(bullets) do
        crayon.graphics.setColor(1.0, 0.9, 0.3, 0.35)
        crayon.graphics.drawCircle("fill", b.x, b.y, 5, 8)

        crayon.graphics.setColor(1.0, 0.6, 0.2, 0.6)
        local tx = b.x - b.vx * 0.02
        local ty = b.y - b.vy * 0.02
        crayon.graphics.drawLine(b.x, b.y, tx, ty, 2)

        crayon.graphics.setColor(1.0, 1.0, 0.7, 1.0)
        crayon.graphics.drawCircle("fill", b.x, b.y, 2, 8)
    end
end

local function draw_particles()
    for _, p in ipairs(particles) do
        local t = p.life / p.maxLife
        crayon.graphics.setColor(p.r, p.g, p.b, t)
        crayon.graphics.drawCircle("fill", p.x, p.y, p.size * t + 0.5, 6)
    end
end

local function draw_player()
    local blink = player.iframe > 0 and (math.floor(time * 20) % 2 == 0)
    if blink then return end

    crayon.graphics.setColor(0, 0, 0, 0.5)
    crayon.graphics.drawCircle("fill", player.x, player.y + 5, 7, 12)

    local fx = player.x + math.cos(player.aimAngle) * 12
    local fy = player.y + math.sin(player.aimAngle) * 12
    crayon.graphics.setColor(0.6, 0.8, 1.0, 0.9)
    crayon.graphics.drawLine(player.x, player.y, fx, fy, 2)

    crayon.graphics.setColor(1.0, 0.9, 0.5, 1.0)
    crayon.graphics.drawCircle("fill", fx, fy, 2, 8)

    -- Player flickers red on low HP
    if player.hp <= 2 and math.random() < 0.1 then
        crayon.graphics.setColor(1.0, 0.3, 0.3, 1.0)
    else
        crayon.graphics.setColor(0.4, 0.7, 1.0, 1.0)
    end
    crayon.graphics.drawCircle("fill", player.x, player.y, 8, 16)

    crayon.graphics.setColor(0.9, 0.95, 1.0, 1.0)
    crayon.graphics.drawCircle("fill", player.x, player.y, 4, 12)

    crayon.graphics.setColor(0.05, 0.1, 0.2, 1.0)
    crayon.graphics.drawCircle("fill",
        player.x + math.cos(player.aimAngle) * 3,
        player.y + math.sin(player.aimAngle) * 3,
        2, 8)
end

-- Glitch layers drawn in world space
local function draw_glitch_world()
    if glitch < 0.05 then return end

    -- Random static speckle
    local count = math.floor(glitch * 90)
    for _ = 1, count do
        local x = math.random() * VW
        local y = math.random() * VH
        local b = 0.6 + math.random() * 0.4
        crayon.graphics.setColor(b, b, b, 0.35)
        crayon.graphics.drawPoint(x, y, 1.0)
    end

    -- Horizontal scanline sweep band
    if glitch > 0.3 then
        local band_y = (time * 220) % VH
        crayon.graphics.setColor(1.0, 0.1, 0.15, 0.10 + glitch * 0.15)
        crayon.graphics.drawRect("fill", 0, band_y, VW, 3)
    end

    -- Slip: draw one horizontal slice offset sideways
    if glitch > 0.5 and math.random() < 0.35 then
        local y = math.random() * VH
        local h = 2 + math.random() * 6
        local off = (math.random() - 0.5) * glitch * 20
        crayon.graphics.setColor(1.0, 0.2, 0.2, 0.15)
        crayon.graphics.drawRect("fill", off, y, VW, h)
    end
end

local function draw_blood_drips()
    for _, bd in ipairs(blood_drips) do
        local alpha = math.min(0.85, bd.life / bd.maxLife)
        crayon.graphics.setColor(0.55, 0.02, 0.02, alpha)
        crayon.graphics.drawRect("fill", bd.x, bd.y, bd.w, 4 + bd.w * 2)
        crayon.graphics.setColor(0.85, 0.05, 0.05, alpha)
        crayon.graphics.drawCircle("fill", bd.x + bd.w * 0.5, bd.y + 4 + bd.w * 2, bd.w * 0.9, 8)
    end
end

local function draw_hud()
    crayon.graphics.setColor(0.03, 0.01, 0.04, 0.85)
    crayon.graphics.drawRect("fill", 0, 0, VW, 12)
    crayon.graphics.setColor(0.4, 0.05, 0.1, 0.9)
    crayon.graphics.drawLine(0, 12, VW, 12, 1)

    -- Score: corrupts digits under glitch
    crayon.graphics.setColor(1.0, 0.85, 0.3, 1.0)
    local scoreStr = "SCORE " .. score
    if glitch > 0.3 then scoreStr = corrupt(scoreStr, glitch * 0.4) end
    crayon.graphics.drawText(scoreStr, 4, 2, 1.0)

    -- Wave: corrupts under glitch
    crayon.graphics.setColor(1.0, 0.4, 0.4, 1.0)
    local waveStr = "WAVE " .. wave
    if glitch > 0.3 then waveStr = corrupt(waveStr, glitch * 0.4) end
    crayon.graphics.drawText(waveStr, VW - 52, 2, 1.0)

    -- HP pips
    local cx = VW * 0.5 - (player.maxHp * 5) + 5
    for i = 1, player.maxHp do
        local hx = cx + (i - 1) * 10
        if i <= player.hp then
            -- Low HP: pips flicker
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

    -- Bottom hint
    crayon.graphics.setColor(0.6, 0.6, 0.7, 0.5)
    crayon.graphics.drawText("A/D: Move  |  Mouse/Space: Shoot", 6, VH - 9, 1.0)
end

local function draw_game_over()
    crayon.graphics.setColor(0.4, 0.0, 0.05, 0.5 + dissolveAmt * 0.3)
    crayon.graphics.drawRect("fill", 0, 0, VW, VH)

    crayon.graphics.setColor(1.0, 0.15, 0.15, 1.0)
    crayon.graphics.drawText("THE PORTAL CLAIMS YOU", VW * 0.5 - 68, VH * 0.5 - 20, 1.6)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Final score: " .. score, VW * 0.5 - 42, VH * 0.5 + 4, 1.0)

    crayon.graphics.setColor(1.0, 0.85, 0.3, 1.0)
    crayon.graphics.drawText("Press SPACE or R to rise again", VW * 0.5 - 88, VH * 0.5 + 24, 1.0)
end

function crayon.draw()
    -- ---- Update post-process uniforms from glitch state ----
    -- Chromatic bleed widens hard on damage / wave-up
    crayon.graphics.setEffectUniform(FX.CHROMATIC, "offset",    1.0 + glitch * 6.0)
    crayon.graphics.setEffectUniform(FX.CHROMATIC, "intensity", 0.35 + glitch * 0.6)

    -- VHS jitter rises with glitch
    crayon.graphics.setEffectUniform(FX.VHS, "strength",   0.10 + glitch * 0.55)
    crayon.graphics.setEffectUniform(FX.VHS, "jitter",     0.12 + glitch * 0.65)
    crayon.graphics.setEffectUniform(FX.VHS, "colorShift", 0.08 + glitch * 0.35)

    -- Vignette deepens when HP is low, and pulses with glitch
    local hpVig = (player and player.hp <= 2) and 0.35 or 0.0
    crayon.graphics.setEffectUniform(FX.VIGNETTE, "intensity", 0.55 + hpVig + glitch * 0.25)

    -- ---- Fake frame stutter at maximum glitch ----
    if glitch > 0.85 and math.random() < 0.15 then
        -- Skip a frame; update already ran, so gameplay continues
        return
    end

    -- ---- Clear to black; the world draws on top ----
    crayon.graphics.clear(0.02, 0.02, 0.04, 1.0)

    local sx, sy = 0, 0
    if shake > 0 then
        sx = (math.random() - 0.5) * shake
        sy = (math.random() - 0.5) * shake
    end

    crayon.graphics.pushMatrix2d()
    crayon.graphics.translate2d(sx, sy)

    -- Background gradient
    crayon.graphics.drawGradientV(0, 0, VW, VH,
        {0.06, 0.0, 0.03, 1.0},
        {0.01, 0.01, 0.02, 1.0})

    -- Distant stars
    crayon.graphics.setColor(0.4, 0.3, 0.4, 0.3)
    for i = 1, 20 do
        local x = (i * 73) % 320
        local y = ((i * 41) % 200) + 10
        crayon.graphics.drawPoint(x, y, 1.0)
    end

    -- Ground reflection glow under portal
    crayon.graphics.setColor(0.5, 0.05, 0.1, 0.15)
    crayon.graphics.drawCircle("fill", portal.x, VH - 6, 90, 30)

    -- World entities
    draw_portal()
    draw_particles()
    draw_demons()
    draw_bullets()
    draw_player()

    -- Glitch overlays live in world space so they shake with the camera
    draw_glitch_world()

    crayon.graphics.popMatrix2d()

    -- Blood on the "lens" — screen space, unshaken
    draw_blood_drips()

    -- Full-screen flashes for damage & death
    if red_flash > 0 then
        crayon.graphics.setColor(1.0, 0.05, 0.05, red_flash * 0.45)
        crayon.graphics.drawRect("fill", 0, 0, VW, VH)
    end
    if white_flash > 0 then
        crayon.graphics.setColor(1.0, 1.0, 1.0, white_flash * 0.7)
        crayon.graphics.drawRect("fill", 0, 0, VW, VH)
    end

    -- HUD
    draw_hud()

    if gameOver then
        draw_game_over()
    end
end