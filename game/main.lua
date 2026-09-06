-- ============================================================================
-- Crayon Engine - Master Demo Hub & Launcher
-- ============================================================================

local demo_list = {
    { key = "1", title = "01: 2D Primitives, Blending & Scissor", file = "game/examples/01_2d_primitives.lua", desc = "Points, lines, polys, arcs, rings, scissor, blend modes" },
    { key = "2", title = "02: 2D Sprites, 9-Slice & 2D Camera",   file = "game/examples/02_2d_sprites_animation.lua", desc = "Spritesheet animation, tiled map, 9-slice dialog, camera pan/zoom" },
    { key = "3", title = "03: 3D Primitives & Shading Gallery",   file = "game/examples/03_3d_primitives_showcase.lua", desc = "Cube, sphere, cylinder, cone, pyramid, torus, capsule, plane" },
    { key = "4", title = "04: 3D Billboards & Particle Fountain",  file = "game/examples/04_3d_billboards_particles.lua", desc = "Spherical vs cylindrical 2.5D billboards & 3D particle fountain" },
    { key = "5", title = "05: 3D FPS Dungeon & Torchlight",       file = "game/examples/05_fps_controller_dungeon.lua", desc = "First-person mouse-look, WASD movement, dynamic flickering light" },
    { key = "6", title = "06: Ortho 3D Isometric Builder",        file = "game/examples/06_isometric_camera.lua", desc = "Orthographic camera, isometric diorama, interactive block builder" },
    { key = "7", title = "07: Retro CRT & Dither Dashboard",      file = "game/examples/07_retro_crt_dither_showcase.lua", desc = "CRT scanlines, curvature, Bayer dither, PS1 jitter, fog" },
    { key = "8", title = "08: Window Management & Scaling",       file = "game/examples/08_window_and_scaling.lua", desc = "Window sizing, display bounds, integer/aspect/stretch scalers" },
    { key = "9", title = "09: Procedural Mesh & Waves",           file = "game/examples/09_custom_procedural_mesh.lua", desc = "Dynamic sinusoidal terrain generation & vertex colors" },
    { key = "0", title = "10: 3D OBJ Model Inspector",            file = "game/examples/10_obj_model_viewer.lua", desc = "Loads monkey.obj, girl.obj, cube.obj, turntable & lighting" },
    { key = "P", title = "Featured: Retro 3D World Showcase",     file = "showcase", desc = "Interactive 3D orbit world with matrix hierarchy & coin HUD" }
}

local selected_idx = 1
local in_menu = true
local timer = 0

-- ---------------- Showcase State (if option P chosen) ----------------
local showcase_cam = { x = 0, y = 2.5, z = 7.0, yaw = -90.0, pitch = -12.0, fov = 60.0 }
local showcase_fx = { jitter = true, affine = 1.0, dither = true, fog = true }
local showcase_models = {}
local showcase_textures = {}
local showcase_rot_y = 0

function run_showcase_init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("Crayon Engine - Retro 3D World Showcase")

    showcase_models.cube = crayon.graphics.load_model("cube")
    showcase_models.plane = crayon.graphics.load_model("plane")
    showcase_models.sphere = crayon.graphics.load_model("sphere")
    showcase_models.cylinder = crayon.graphics.load_model("cylinder")

    showcase_textures.crate = crayon.graphics.load_texture("game/assets/textures/crate.bmp")
    showcase_textures.brick = crayon.graphics.load_texture("game/assets/textures/brick.bmp")
    showcase_textures.grass = crayon.graphics.load_texture("game/assets/textures/grass.bmp")
    showcase_textures.coin  = crayon.graphics.load_texture("game/assets/textures/coin.bmp")

    crayon.graphics.set_retro_effects({
        jitter_resolution = showcase_fx.jitter and {160, 120} or nil,
        affine = showcase_fx.affine,
        dither = showcase_fx.dither,
        fog = showcase_fx.fog and { start = 6, ["end"] = 22, color = {0.06, 0.07, 0.12} } or nil
    })

    crayon.graphics.set_light(-0.4, -0.9, -0.6, 1.0, 0.92, 0.85, 0.28, 0.28, 0.35)
end

function launch_demo(idx)
    local item = demo_list[idx]
    if not item then return end

    if item.file == "showcase" then
        in_menu = false
        run_showcase_init()
    else
        -- Load example file directly into Lua environment
        in_menu = false
        dofile(item.file)
        if crayon.init then
            crayon.init()
        end
    end
end

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("Crayon Engine - Demo Launcher Hub [Up/Down: Select, Enter: Launch]")
end

function crayon.update(dt)
    timer = timer + dt

    if in_menu then
        -- Menu Navigation
        if crayon.input.is_pressed("up") or crayon.input.is_pressed("w") then
            selected_idx = selected_idx - 1
            if selected_idx < 1 then selected_idx = #demo_list end
        end
        if crayon.input.is_pressed("down") or crayon.input.is_pressed("s") then
            selected_idx = selected_idx + 1
            if selected_idx > #demo_list then selected_idx = 1 end
        end

        -- Direct Key Shortcuts
        for i = 1, 9 do
            if crayon.input.is_pressed(tostring(i)) then
                launch_demo(i)
                return
            end
        end
        if crayon.input.is_pressed("0") then
            launch_demo(10)
            return
        end
        if crayon.input.is_pressed("p") then
            launch_demo(11)
            return
        end

        -- Launch with Enter or Space
        if crayon.input.is_pressed("return") or crayon.input.is_pressed("space") then
            launch_demo(selected_idx)
            return
        end

        -- Quit
        if crayon.input.is_pressed("escape") then
            crayon.window.quit()
        end
    else
        -- Showcase Update (when running Option P)
        showcase_rot_y = showcase_rot_y + dt * 45.0
        local move_speed = 5.0 * dt
        local rad_yaw = math.rad(showcase_cam.yaw)
        local fwd_x, fwd_z = math.cos(rad_yaw), math.sin(rad_yaw)
        local right_x, right_z = -fwd_z, fwd_x

        if crayon.input.is_down("w") then showcase_cam.x = showcase_cam.x + fwd_x * move_speed; showcase_cam.z = showcase_cam.z + fwd_z * move_speed end
        if crayon.input.is_down("s") then showcase_cam.x = showcase_cam.x - fwd_x * move_speed; showcase_cam.z = showcase_cam.z - fwd_z * move_speed end
        if crayon.input.is_down("a") then showcase_cam.x = showcase_cam.x - right_x * move_speed; showcase_cam.z = showcase_cam.z - right_z * move_speed end
        if crayon.input.is_down("d") then showcase_cam.x = showcase_cam.x + right_x * move_speed; showcase_cam.z = showcase_cam.z + right_z * move_speed end
        if crayon.input.is_down("left") then showcase_cam.yaw = showcase_cam.yaw - 90.0 * dt end
        if crayon.input.is_down("right") then showcase_cam.yaw = showcase_cam.yaw + 90.0 * dt end

        if crayon.input.is_pressed("escape") then
            in_menu = true
            crayon.window.set_resolution(320, 240)
            crayon.window.set_title("Crayon Engine - Demo Launcher Hub")
        end
    end
end

function crayon.draw()
    if in_menu then
        draw_menu()
    else
        draw_showcase()
    end
end

function draw_menu()
    crayon.graphics.clear(0.06, 0.07, 0.12)

    -- Header Banner
    crayon.graphics.set_color(0.1, 0.14, 0.24, 0.95)
    crayon.graphics.draw_rect("fill", 0, 0, 320, 26)
    crayon.graphics.set_color(0.3, 0.5, 0.85, 1.0)
    crayon.graphics.draw_line(0, 26, 320, 26, 1.0)

    crayon.graphics.set_color(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.draw_text("CRAYON ENGINE - DEMO GALLERY", 12, 8, 1.0)

    local fps = math.floor(crayon.window.get_fps() + 0.5)
    crayon.graphics.set_color(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.draw_text("FPS: " .. fps, 265, 8, 1.0)

    -- Menu Item List
    local start_y = 32
    local item_h = 16
    for i, item in ipairs(demo_list) do
        local iy = start_y + (i - 1) * item_h
        local is_selected = (i == selected_idx)

        if is_selected then
            -- Highlight bar
            local pulse = math.sin(timer * 8.0) * 0.15 + 0.85
            crayon.graphics.set_color(0.2 * pulse, 0.35 * pulse, 0.7 * pulse, 0.85)
            crayon.graphics.draw_rounded_rect("fill", 6, iy - 1, 308, item_h - 1, 3)
            crayon.graphics.set_color(0.5, 0.75, 1.0, 1.0)
            crayon.graphics.draw_rounded_rect("line", 6, iy - 1, 308, item_h - 1, 3)

            -- Cursor arrow
            crayon.graphics.set_color(1.0, 0.9, 0.2, 1.0)
            crayon.graphics.draw_text(">", 10, iy + 2, 1.0)

            crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
            crayon.graphics.draw_text("[" .. item.key .. "] " .. item.title, 20, iy + 2, 1.0)
        else
            crayon.graphics.set_color(0.7, 0.75, 0.85, 0.9)
            crayon.graphics.draw_text("[" .. item.key .. "] " .. item.title, 20, iy + 2, 1.0)
        end
    end

    -- Bottom Description Box
    local cur_item = demo_list[selected_idx]
    crayon.graphics.set_color(0.08, 0.1, 0.18, 0.9)
    crayon.graphics.draw_rounded_rect("fill", 6, 210, 308, 26, 3)
    crayon.graphics.set_color(0.25, 0.4, 0.7, 1.0)
    crayon.graphics.draw_rounded_rect("line", 6, 210, 308, 26, 3)

    crayon.graphics.set_color(0.4, 0.85, 1.0, 1.0)
    crayon.graphics.draw_text(cur_item.desc, 12, 214, 1.0)

    crayon.graphics.set_color(0.65, 0.7, 0.8, 1.0)
    crayon.graphics.draw_text("[ENTER/SPACE]: Launch | [1-9,0,P]: Quick Jump | [ESC]: Quit", 12, 224, 1.0)
end

function draw_showcase()
    crayon.graphics.clear(0.06, 0.07, 0.12)

    local rad_yaw = math.rad(showcase_cam.yaw)
    local rad_pitch = math.rad(showcase_cam.pitch)
    local target_x = showcase_cam.x + math.cos(rad_pitch) * math.cos(rad_yaw)
    local target_y = showcase_cam.y + math.sin(rad_pitch)
    local target_z = showcase_cam.z + math.cos(rad_pitch) * math.sin(rad_yaw)

    crayon.graphics.set_camera3d({
        position = {showcase_cam.x, showcase_cam.y, showcase_cam.z},
        target = {target_x, target_y, target_z},
        up = {0, 1, 0},
        fov = showcase_cam.fov
    })

    -- Ground
    crayon.graphics.draw_model(showcase_models.plane, 0, 0, 0, 0, 0, 0, 2.5, 1, 2.5, showcase_textures.grass)

    -- Pillars
    local pillars = {{-5, -5}, {5, -5}, {-5, 5}, {5, 5}}
    for _, p in ipairs(pillars) do
        crayon.graphics.draw_model(showcase_models.cylinder, p[1], 1.5, p[2], 0, 0, 0, 0.8, 3.0, 0.8, showcase_textures.brick)
    end

    -- Rotating crates
    local rad_r = math.rad(showcase_rot_y)
    crayon.graphics.draw_model(showcase_models.cube, 0, 1.2, 0, rad_r * 0.7, rad_r, 0, 1.6, 1.6, 1.6, showcase_textures.crate)

    -- Matrix stack orbiting satellites
    for i = 1, 3 do
        local a = rad_r + (i * (math.pi * 2 / 3))
        crayon.graphics.push_matrix()
        crayon.graphics.translate(math.cos(a) * 3.5, 1.0, math.sin(a) * 3.5)
        crayon.graphics.rotate(rad_r * 2.0, 0, 1, 0)
        crayon.graphics.draw_model(showcase_models.cube, 0, 0, 0, 0, 0, 0, 0.8, 0.8, 0.8, showcase_textures.crate)
        crayon.graphics.pop_matrix()
    end

    -- HUD
    crayon.graphics.set_color(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.draw_text("FEATURED RETRO 3D SHOWCASE", 10, 10, 1.0)
    crayon.graphics.set_color(0.8, 0.85, 0.9, 1.0)
    crayon.graphics.draw_text("WASD/Arrows: Move/Look | [ESC]: Return to Menu", 10, 224, 1.0)
end
