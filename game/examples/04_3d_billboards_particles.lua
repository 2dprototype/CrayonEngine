-- ============================================================================
-- Example 04: 3D Billboards & Particle System (Spherical vs Cylindrical)
-- Crayon Engine
-- ============================================================================

local cam = {
    yaw = 0.0,
    pitch = -15.0,
    dist = 9.0,
    target_y = 1.0
}

local tex_coin = 0
local tex_grass = 0
local tex_brick = 0
local timer = 0

-- 3D Particle system for fountain
local particles = {}
local max_particles = 60

-- Cylindrical billboard forest entities
local forest_trees = {}

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("04 - 3D Billboards & Particles [Arrows: Orbit, Space: Burst]")

    tex_coin  = crayon.graphics.load_texture("game/assets/textures/coin.bmp")
    tex_grass = crayon.graphics.load_texture("game/assets/textures/grass.bmp")
    tex_brick = crayon.graphics.load_texture("game/assets/textures/brick.bmp")

    crayon.graphics.set_retro_effects({
        jitter_resolution = {240, 160},
        affine = 1.0,
        dither = true,
        fog = { start = 8, ["end"] = 20, color = {0.08, 0.1, 0.16} }
    })

    crayon.graphics.set_light(
        -0.4, -0.9, -0.5,
        1.0, 0.95, 0.85,
        0.3, 0.3, 0.35
    )

    -- Generate random trees / pillars in a clearing
    math.randomseed(12345)
    for i = 1, 16 do
        local angle = (i / 16) * math.pi * 2 + (math.random() - 0.5) * 0.3
        local dist = 4.0 + math.random() * 4.0
        table.insert(forest_trees, {
            x = math.cos(angle) * dist,
            z = math.sin(angle) * dist,
            height = 1.8 + math.random() * 0.8,
            width = 1.0 + math.random() * 0.4
        })
    end
end

function spawn_particle()
    local angle = math.random() * math.pi * 2
    local spd = 0.8 + math.random() * 1.5
    return {
        x = (math.random() - 0.5) * 0.2,
        y = 0.5,
        z = (math.random() - 0.5) * 0.2,
        vx = math.cos(angle) * spd * 0.5,
        vy = 2.5 + math.random() * 2.0,
        vz = math.sin(angle) * spd * 0.5,
        size = 0.25 + math.random() * 0.2,
        life = 1.0,
        decay = 0.6 + math.random() * 0.5
    }
end

function crayon.update(dt)
    timer = timer + dt

    -- Camera Orbit Controls
    if crayon.input.is_down("left") or crayon.input.is_down("a") then
        cam.yaw = cam.yaw - 50.0 * dt
    end
    if crayon.input.is_down("right") or crayon.input.is_down("d") then
        cam.yaw = cam.yaw + 50.0 * dt
    end
    if crayon.input.is_down("up") or crayon.input.is_down("w") then
        cam.pitch = math.min(10.0, cam.pitch + 40.0 * dt)
    end
    if crayon.input.is_down("down") or crayon.input.is_down("s") then
        cam.pitch = math.max(-75.0, cam.pitch - 40.0 * dt)
    end

    -- Zoom
    local wheel_y = crayon.input.get_mouse_wheel() or 0
    if wheel_y ~= 0 then
        cam.dist = math.max(3.0, math.min(18.0, cam.dist - wheel_y * 1.0))
    end

    -- Continuous particle fountain emission
    if #particles < max_particles then
        table.insert(particles, spawn_particle())
    end

    -- Spacebar burst
    if crayon.input.is_pressed("space") then
        for i = 1, 25 do
            table.insert(particles, spawn_particle())
        end
    end

    -- Update particles
    for i = #particles, 1, -1 do
        local p = particles[i]
        p.x = p.x + p.vx * dt
        p.y = p.y + p.vy * dt
        p.z = p.z + p.vz * dt
        p.vy = p.vy - 4.5 * dt -- Gravity
        p.life = p.life - p.decay * dt
        if p.life <= 0 or p.y < 0 then
            table.remove(particles, i)
        end
    end

    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.1, 0.16)

    -- Setup 3D Camera Orbit
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local cx = math.cos(rad_pitch) * math.cos(rad_yaw) * cam.dist
    local cy = -math.sin(rad_pitch) * cam.dist + cam.target_y
    local cz = math.cos(rad_pitch) * math.sin(rad_yaw) * cam.dist

    crayon.graphics.set_camera3d({
        position = {cx, cy, cz},
        target = {0, cam.target_y, 0},
        up = {0, 1, 0},
        fov = 60.0
    })

    -- 1. Grassy Ground Terrain
    crayon.graphics.set_color(0.8, 0.9, 0.8, 1.0)
    crayon.graphics.draw_plane(0, 0, 0, 0, 0, 0, 10, 1, 10, tex_grass)

    -- 2. Central Stone Pedestal / Fountain Base
    crayon.graphics.set_color(0.6, 0.65, 0.7, 1.0)
    crayon.graphics.draw_cylinder(0, 0.25, 0, 0, 0, 0, 1.2, 0.5, 1.2, tex_brick)

    -- 3. Cylindrical Billboards (Retro 2.5D Sprite Trees/Pillars)
    -- They stay vertical and rotate only around the Y axis!
    crayon.graphics.set_color(0.9, 0.95, 0.85, 1.0)
    for _, t in ipairs(forest_trees) do
        -- Draw cylindrical billboard standing upright
        crayon.graphics.draw_billboard(
            t.x, t.height * 0.5, t.z,
            t.width, t.height,
            tex_brick,
            "cylindrical"
        )
    end

    -- 4. Animated Rotating 3D Coin Pickups (Spherical Billboards)
    for i = 1, 4 do
        local a = (i / 4) * math.pi * 2 + timer * 1.5
        local px = math.cos(a) * 2.2
        local pz = math.sin(a) * 2.2
        local py = 1.0 + math.sin(timer * 4.0 + i) * 0.2

        -- Shadow on the ground
        crayon.graphics.set_color(0, 0, 0, 0.4)
        crayon.graphics.draw_plane(px, 0.02, pz, 0, 0, 0, 0.3, 1, 0.3)

        -- Spherical billboard coin
        crayon.graphics.set_color(1.0, 0.9, 0.3, 1.0)
        crayon.graphics.draw_billboard(px, py, pz, 0.6, 0.6, tex_coin, "spherical")
    end

    -- 5. 3D Particle Fountain (Spherical Billboards with Unlit Glow)
    crayon.graphics.set_shading_mode("unlit")
    for _, p in ipairs(particles) do
        local sz = p.size * p.life
        crayon.graphics.set_color(1.0, 0.6 * p.life, 0.2, p.life)
        -- Spherical billboard faces camera completely in all 3 axes
        crayon.graphics.draw_billboard(p.x, p.y, p.z, sz, sz, 0, "spherical")
    end
    crayon.graphics.set_shading_mode("gouraud")

    -- ========================================================================
    -- 2D HUD OVERLAY
    -- ========================================================================
    crayon.graphics.set_color(0.08, 0.1, 0.16, 0.85)
    crayon.graphics.draw_rect("fill", 4, 4, 312, 22)
    crayon.graphics.set_color(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.draw_rect("line", 4, 4, 312, 22)

    crayon.graphics.set_color(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.draw_text("3D BILLBOARDS & PARTICLE FOUNTAIN", 8, 10, 1.0)

    crayon.graphics.set_color(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.draw_text("Particles: " .. #particles, 230, 10, 1.0)

    -- Bottom Explanation Box
    crayon.graphics.set_color(0.06, 0.08, 0.14, 0.85)
    crayon.graphics.draw_rect("fill", 4, 204, 312, 32)
    crayon.graphics.set_color(0.25, 0.35, 0.6, 1.0)
    crayon.graphics.draw_rect("line", 4, 204, 312, 32)

    crayon.graphics.set_color(0.95, 0.95, 0.95, 1.0)
    crayon.graphics.draw_text("Outer Pillars: Cylindrical (stays upright Y-axis)", 10, 208, 1.0)
    crayon.graphics.set_color(1.0, 0.7, 0.3, 1.0)
    crayon.graphics.draw_text("Particles/Coins: Spherical (full camera face) | [SPACE]: Burst", 10, 222, 1.0)
end
