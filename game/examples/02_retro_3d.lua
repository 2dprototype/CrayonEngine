-- ============================================================================
-- Example 02: 3D Retro Geometry & Hierarchical Matrix Stack
-- Run with: ./crayon_engine --game game/examples/02_retro_3d.lua
-- ============================================================================

local cam_dist = 8.0
local cam_angle = 0
local cube_model = 0
local sphere_model = 0
local cyl_model = 0
local crate_tex = 0
local brick_tex = 0

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("Crayon Engine - 3D Hierarchical Transforms")

    cube_model = crayon.graphics.load_model("cube")
    sphere_model = crayon.graphics.load_model("sphere")
    cyl_model = crayon.graphics.load_model("cylinder")

    crate_tex = crayon.graphics.load_texture("game/assets/textures/crate.bmp")
    brick_tex = crayon.graphics.load_texture("game/assets/textures/brick.bmp")

    crayon.graphics.set_retro_effects({
        jitter_resolution = {160, 120},
        affine = 1.0,
        dither = true,
        fog = { start = 6, ["end"] = 18, color = {0.1, 0.12, 0.18} }
    })

    crayon.graphics.set_light(-0.5, -1.0, -0.3, 1.0, 0.95, 0.85, 0.25, 0.25, 0.3)
end

function crayon.update(dt)
    cam_angle = cam_angle + dt * 30.0

    if crayon.input.is_down("up") then cam_dist = math.max(3.0, cam_dist - dt * 5.0) end
    if crayon.input.is_down("down") then cam_dist = math.min(15.0, cam_dist + dt * 5.0) end

    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.12, 0.18)

    local rad = math.rad(cam_angle)
    local cx = math.cos(rad) * cam_dist
    local cz = math.sin(rad) * cam_dist

    crayon.graphics.set_camera3d({
        position = {cx, 4.0, cz},
        target = {0, 0, 0},
        up = {0, 1, 0},
        fov = 55.0
    })

    -- Central Pedestal
    crayon.graphics.draw_model(cyl_model, 0, -1.0, 0, 0, 0, 0, 1.5, 2.0, 1.5, brick_tex)

    -- Primary Central Spinning Box
    crayon.graphics.push_matrix()
    crayon.graphics.translate(0, 0.8, 0)
    crayon.graphics.rotate(rad * 1.5, 0, 1, 0)
    crayon.graphics.draw_model(cube_model, 0, 0, 0, 0, 0, 0, 1.2, 1.2, 1.2, crate_tex)

    -- Satellite Orbit 1
    crayon.graphics.push_matrix()
    crayon.graphics.rotate(rad * 2.0, 0, 1, 0)
    crayon.graphics.translate(2.5, 0, 0)
    crayon.graphics.draw_model(sphere_model, 0, 0, 0, 0, 0, 0, 0.6, 0.6, 0.6, brick_tex)

    -- Moon Orbit around Satellite 1
    crayon.graphics.rotate(rad * 4.0, 1, 0, 0)
    crayon.graphics.translate(0, 0.9, 0)
    crayon.graphics.draw_model(cube_model, 0, 0, 0, 0, 0, 0, 0.3, 0.3, 0.3, crate_tex)
    crayon.graphics.pop_matrix()

    -- Satellite Orbit 2
    crayon.graphics.push_matrix()
    crayon.graphics.rotate(-rad * 1.8, 0, 0, 1)
    crayon.graphics.translate(0, 2.8, 0)
    crayon.graphics.draw_model(cube_model, 0, 0, 0, 0, 0, 0, 0.5, 0.5, 0.5, crate_tex)
    crayon.graphics.pop_matrix()

    crayon.graphics.pop_matrix()

    -- HUD Overlay
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_text("3D Transform Hierarchy & Matrix Stack", 10, 10, 1.0)
    crayon.graphics.draw_text("Up/Down: Zoom Camera", 10, 24, 1.0)
    crayon.graphics.draw_text("FPS: " .. math.floor(crayon.window.get_fps() + 0.5), 260, 10, 1.0)
end
