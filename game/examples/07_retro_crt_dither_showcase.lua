-- ============================================================================
-- Example 07: Comprehensive Retro Shader Controls (CRT, Dither, Jitter, Fog)
-- Crayon Engine
-- ============================================================================

local fx = {
    jitter = true,
    affine = 1.0,
    dither = true,
    dither_levels = 8,
    crt = true,
    crt_scanlines = 0.35,
    crt_curvature = 0.04,
    vignette = 0.3,
    fog = true
}

local textures = {}
local timer = 0
local cam_rot = 0

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("07 - Retro Shader Controls [1-7: Toggle Effects]")

    textures.crate = crayon.graphics.load_texture("game/assets/textures/crate.bmp")
    textures.brick = crayon.graphics.load_texture("game/assets/textures/brick.bmp")
    textures.grass = crayon.graphics.load_texture("game/assets/textures/grass.bmp")

    crayon.graphics.set_light(
        -0.5, -0.9, -0.4,
        1.0, 0.95, 0.85,
        0.25, 0.25, 0.3
    )

    apply_effects()
end

function apply_effects()
    crayon.graphics.set_retro_effects({
        jitter_resolution = fx.jitter and {160, 120} or nil,
        affine = fx.affine,
        dither = fx.dither,
        dither_levels = fx.dither_levels,
        crt = fx.crt,
        crt_scanline_strength = fx.crt_scanlines,
        crt_curvature = fx.crt_curvature,
        vignette = fx.vignette,
        fog = fx.fog and { start = 4.0, ["end"] = 15.0, color = {0.06, 0.08, 0.14} } or nil
    })
end

function crayon.update(dt)
    timer = timer + dt
    cam_rot = cam_rot + dt * 25.0

    -- Key toggles
    if crayon.input.is_pressed("1") then
        fx.jitter = not fx.jitter
        apply_effects()
    end
    if crayon.input.is_pressed("2") then
        fx.affine = (fx.affine > 0.5) and 0.0 or 1.0
        apply_effects()
    end
    if crayon.input.is_pressed("3") then
        fx.dither = not fx.dither
        apply_effects()
    end
    if crayon.input.is_pressed("4") then
        local levels = {2, 4, 8, 16, 32}
        local cur_idx = 3
        for idx, lvl in ipairs(levels) do
            if lvl == fx.dither_levels then cur_idx = idx end
        end
        cur_idx = (cur_idx % #levels) + 1
        fx.dither_levels = levels[cur_idx]
        apply_effects()
    end
    if crayon.input.is_pressed("5") then
        fx.crt = not fx.crt
        apply_effects()
    end
    if crayon.input.is_pressed("6") then
        fx.vignette = (fx.vignette > 0.0) and 0.0 or 0.45
        apply_effects()
    end
    if crayon.input.is_pressed("7") then
        fx.fog = not fx.fog
        apply_effects()
    end

    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.06, 0.08, 0.14)

    -- 3D Camera
    local rad = math.rad(cam_rot)
    local cx = math.cos(rad) * 6.5
    local cz = math.sin(rad) * 6.5

    crayon.graphics.set_camera3d({
        position = {cx, 3.2, cz},
        target = {0, 0.5, 0},
        up = {0, 1, 0},
        fov = 60.0
    })

    -- Ground
    crayon.graphics.set_color(0.8, 0.8, 0.85, 1.0)
    crayon.graphics.draw_plane(0, 0, 0, 0, 0, 0, 8, 1, 8, textures.grass)

    -- Central rotating textured cubes
    local rot_speed = timer * 40.0
    local rad_r = math.rad(rot_speed)
    crayon.graphics.draw_cube(0, 1.0, 0, rad_r * 0.5, rad_r, 0, 1.4, 1.4, 1.4, textures.crate)

    -- Satellite pillars & spheres
    for i = 1, 4 do
        local angle = rad + (i * (math.pi * 0.5))
        local px = math.cos(angle) * 3.0
        local pz = math.sin(angle) * 3.0
        crayon.graphics.draw_cylinder(px, 1.0, pz, 0, 0, 0, 0.6, 2.0, 0.6, textures.brick)
        crayon.graphics.draw_sphere(px, 2.4 + math.sin(timer * 3.0 + i) * 0.2, pz, 0, 0, 0, 0.5, 0.5, 0.5)
    end

    -- ========================================================================
    -- 2D HUD OVERLAY (Effect Dashboard)
    -- ========================================================================
    crayon.graphics.set_color(0.05, 0.07, 0.12, 0.88)
    crayon.graphics.draw_rect("fill", 4, 4, 312, 22)
    crayon.graphics.set_color(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.draw_rect("line", 4, 4, 312, 22)

    crayon.graphics.set_color(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.draw_text("RETRO SHADER DASHBOARD", 8, 10, 1.0)

    local fps = math.floor(crayon.window.get_fps() + 0.5)
    crayon.graphics.set_color(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.draw_text("FPS: " .. fps, 265, 10, 1.0)

    -- Effects status cards
    local cards = {
        {"1: Jitter", fx.jitter and "ON (160x120)" or "OFF", fx.jitter},
        {"2: Affine", (fx.affine > 0.5) and "ON (PS1 Warp)" or "OFF (Correct)", fx.affine > 0.5},
        {"3: Dither", fx.dither and "ON (Bayer 4x4)" or "OFF", fx.dither},
        {"4: Levels", fx.dither_levels .. " per channel", true},
        {"5: CRT", fx.crt and "ON (Scanlines)" or "OFF", fx.crt},
        {"6: Vignette", (fx.vignette > 0) and "ON" or "OFF", fx.vignette > 0},
        {"7: Fog", fx.fog and "ON (Linear)" or "OFF", fx.fog}
    }

    local by = 136
    crayon.graphics.set_color(0.04, 0.06, 0.1, 0.9)
    crayon.graphics.draw_rounded_rect("fill", 4, by, 312, 100, 4)
    crayon.graphics.set_color(0.3, 0.45, 0.7, 1.0)
    crayon.graphics.draw_rounded_rect("line", 4, by, 312, 100, 4)

    for i, c in ipairs(cards) do
        local cy = by + 6 + (i - 1) * 13
        crayon.graphics.set_color(0.85, 0.85, 0.9, 1.0)
        crayon.graphics.draw_text(c[1] .. ":", 10, cy, 1.0)

        if c[3] then
            crayon.graphics.set_color(0.4, 1.0, 0.5, 1.0)
        else
            crayon.graphics.set_color(0.65, 0.65, 0.7, 1.0)
        end
        crayon.graphics.draw_text(c[2], 110, cy, 1.0)
    end
end
