-- ============================================================================
-- Example 11: High-Performance 3D Jolt Physics Sandbox
-- Stacking, rigid bodies, cannonball shooting, impulses, raycast & debug draw
-- ============================================================================

local cam = {
    x = 0.0, y = 5.0, z = 14.0,
    yaw = -90.0, pitch = -15.0,
    fov = 60.0
}

local models = {}
local textures = {}
local bodies = {} -- list of { id = id, type = "cube"|"sphere"|"cylinder", size = {w,h,d}, color = {r,g,b} }

local show_debug = false
local auto_spawn_timer = 0.0
local floor_id = 0

local function spawn_cube(x, y, z, sx, sy, sz, motion, friction, restitution)
    motion = motion or "dynamic"
    friction = friction or 0.6
    restitution = restitution or 0.2
    local id = crayon.physics.create_box(x, y, z, sx * 0.5, sy * 0.5, sz * 0.5, motion, friction, restitution)
    table.insert(bodies, {
        id = id,
        type = "cube",
        size = {sx, sy, sz},
        color = {0.85 + math.random() * 0.15, 0.65 + math.random() * 0.2, 0.45}
    })
    return id
end

local function spawn_sphere(x, y, z, radius, motion, friction, restitution)
    motion = motion or "dynamic"
    friction = friction or 0.4
    restitution = restitution or 0.7
    local id = crayon.physics.create_sphere(x, y, z, radius, motion, friction, restitution)
    table.insert(bodies, {
        id = id,
        type = "sphere",
        radius = radius,
        color = {0.2 + math.random() * 0.8, 0.4 + math.random() * 0.6, 0.9}
    })
    return id
end

local function reset_scene()
    crayon.physics.destroy_all()
    bodies = {}

    -- Static Ground Plane
    floor_id = crayon.physics.create_plane(0, 0, 0, 0, 1, 0, 50.0)

    -- Static Boundary Walls
    crayon.physics.create_box(-20, 2.5, 0, 0.5, 2.5, 20, "static", 0.5, 0.1)
    crayon.physics.create_box( 20, 2.5, 0, 0.5, 2.5, 20, "static", 0.5, 0.1)
    crayon.physics.create_box(0, 2.5, -20, 20, 2.5, 0.5, "static", 0.5, 0.1)
    crayon.physics.create_box(0, 2.5,  20, 20, 2.5, 0.5, "static", 0.5, 0.1)

    -- Build a 5-layer Pyramid of crates
    local box_size = 1.2
    local layers = 5
    for layer = 0, layers - 1 do
        local count = layers - layer
        local y = 0.6 + layer * box_size
        local start_x = -((count - 1) * box_size) * 0.5
        for i = 0, count - 1 do
            local x = start_x + i * box_size
            spawn_cube(x, y, 0, box_size, box_size, box_size, "dynamic", 0.7, 0.1)
        end
    end

    -- Twin Jenga Tower on the left
    for h = 0, 7 do
        local y = 0.5 + h * 0.8
        spawn_cube(-6.0, y, 0, 0.8, 0.8, 0.8, "dynamic", 0.7, 0.1)
        spawn_cube( 6.0, y, 0, 0.8, 0.8, 0.8, "dynamic", 0.7, 0.1)
    end
end

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("Crayon Engine - High-Speed Jolt 3D Physics Sandbox")

    models.cube = crayon.graphics.load_model("cube")
    models.sphere = crayon.graphics.load_model("sphere")
    models.plane = crayon.graphics.load_model("plane")

    textures.crate = crayon.graphics.load_texture("game/assets/textures/crate.bmp")
    textures.grass = crayon.graphics.load_texture("game/assets/textures/grass.bmp")

    crayon.graphics.set_retro_effects({
        jitter_resolution = {160, 120},
        affine = 0.8,
        dither = true,
        fog = { start = 12, ["end"] = 40, color = {0.08, 0.10, 0.16} }
    })

    crayon.graphics.set_light(-0.4, -0.9, -0.6, 1.0, 0.95, 0.85, 0.35, 0.35, 0.42)
    reset_scene()
end

function crayon.update(dt)
    -- Camera Orbit & Movement
    local move_speed = 7.0 * dt
    local rad_yaw = math.rad(cam.yaw)
    local fwd_x, fwd_z = math.cos(rad_yaw), math.sin(rad_yaw)
    local right_x, right_z = -fwd_z, fwd_x

    if crayon.input.is_down("w") then cam.x = cam.x + fwd_x * move_speed; cam.z = cam.z + fwd_z * move_speed end
    if crayon.input.is_down("s") then cam.x = cam.x - fwd_x * move_speed; cam.z = cam.z - fwd_z * move_speed end
    if crayon.input.is_down("a") then cam.x = cam.x - right_x * move_speed; cam.z = cam.z - right_z * move_speed end
    if crayon.input.is_down("d") then cam.x = cam.x + right_x * move_speed; cam.z = cam.z + right_z * move_speed end
    if crayon.input.is_down("q") or crayon.input.is_down("space") then cam.y = cam.y + move_speed end
    if crayon.input.is_down("z") or crayon.input.is_down("lshift") then cam.y = cam.y - move_speed end

    if crayon.input.is_down("left") then cam.yaw = cam.yaw - 80.0 * dt end
    if crayon.input.is_down("right") then cam.yaw = cam.yaw + 80.0 * dt end
    if crayon.input.is_down("up") then cam.pitch = math.min(cam.pitch + 60.0 * dt, 80.0) end
    if crayon.input.is_down("down") then cam.pitch = math.max(cam.pitch - 60.0 * dt, -80.0) end

    -- Shoot Cannonball along Camera Look Direction (Left Mouse or 'F')
    local rad_pitch = math.rad(cam.pitch)
    local look_x = math.cos(rad_pitch) * math.cos(rad_yaw)
    local look_y = math.sin(rad_pitch)
    local look_z = math.cos(rad_pitch) * math.sin(rad_yaw)

    if crayon.input.is_mouse_down(1) or crayon.input.is_pressed("f") then
        local ball_id = spawn_sphere(cam.x + look_x * 1.5, cam.y + look_y * 1.5, cam.z + look_z * 1.5, 0.45, "dynamic", 0.4, 0.6)
        local shoot_speed = 35.0
        crayon.physics.set_velocity(ball_id, look_x * shoot_speed, look_y * shoot_speed, look_z * shoot_speed)
    end

    -- Rain random tumbling spheres on 'B'
    if crayon.input.is_pressed("b") then
        for _ = 1, 10 do
            local rx = (math.random() - 0.5) * 8.0
            local rz = (math.random() - 0.5) * 8.0
            local ry = 8.0 + math.random() * 6.0
            spawn_sphere(rx, ry, rz, 0.35 + math.random() * 0.25)
        end
    end

    -- Radial Blast / Explosion Impulse on 'E'
    if crayon.input.is_pressed("e") then
        for _, b in ipairs(bodies) do
            if crayon.physics.is_valid(b.id) then
                local bx, by, bz = crayon.physics.get_position(b.id)
                local dx, dy, dz = bx - 0.0, by - 0.5, bz - 0.0
                local dist_sq = dx * dx + dy * dy + dz * dz
                if dist_sq < 100.0 and dist_sq > 0.01 then
                    local dist = math.sqrt(dist_sq)
                    local force = (1.0 - dist / 10.0) * 80.0
                    crayon.physics.apply_impulse(b.id, (dx / dist) * force, (dy / dist + 0.6) * force, (dz / dist) * force)
                end
            end
        end
    end

    -- Toggle Debug Wireframe on 'TAB' or 'G'
    if crayon.input.is_pressed("tab") or crayon.input.is_pressed("g") then
        show_debug = not show_debug
    end

    -- Reset Scene on 'R'
    if crayon.input.is_pressed("r") then
        reset_scene()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.10, 0.16)

    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local target_x = cam.x + math.cos(rad_pitch) * math.cos(rad_yaw)
    local target_y = cam.y + math.sin(rad_pitch)
    local target_z = cam.z + math.cos(rad_pitch) * math.sin(rad_yaw)

    crayon.graphics.set_camera3d({
        position = {cam.x, cam.y, cam.z},
        target = {target_x, target_y, target_z},
        up = {0, 1, 0},
        fov = cam.fov
    })

    -- Render Ground
    crayon.graphics.draw_model(models.plane, 0, 0, 0, 0, 0, 0, 8.0, 1.0, 8.0, textures.grass)

    -- Render Dynamic Bodies
    for _, b in ipairs(bodies) do
        if crayon.physics.is_valid(b.id) then
            local px, py, pz = crayon.physics.get_position(b.id)
            local rx, ry, rz = crayon.physics.get_rotation(b.id)

            if b.type == "cube" then
                local sx, sy, sz = b.size[1], b.size[2], b.size[3]
                crayon.graphics.set_color(b.color[1], b.color[2], b.color[3], 1.0)
                crayon.graphics.draw_model(models.cube, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), sx, sy, sz, textures.crate)
            elseif b.type == "sphere" then
                local r = b.radius
                crayon.graphics.set_color(b.color[1], b.color[2], b.color[3], 1.0)
                crayon.graphics.draw_model(models.sphere, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), r * 2.0, r * 2.0, r * 2.0)
            end
        end
    end
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)

    -- Raycast Crosshair Aim Test
    local look_x = math.cos(rad_pitch) * math.cos(rad_yaw)
    local look_y = math.sin(rad_pitch)
    local look_z = math.cos(rad_pitch) * math.sin(rad_yaw)
    local hit, hx, hy, hz, nx, ny, nz, hdist, hbody = crayon.physics.raycast(cam.x, cam.y, cam.z, look_x, look_y, look_z, 50.0)

    -- Debug Wireframes (optional)
    if show_debug then
        crayon.physics.draw_debug(0.3, 1.0, 0.4, 1.0, 0.6, 0.6, 0.7, 0.8)
    end

    -- 2D HUD Overlays
    crayon.graphics.set_color(0.0, 0.0, 0.0, 0.6)
    crayon.graphics.draw_rect("fill", 4, 4, 185, 52)
    crayon.graphics.set_color(0.3, 0.6, 1.0, 1.0)
    crayon.graphics.draw_rect("line", 4, 4, 185, 52)

    local total_bodies, active_bodies = crayon.physics.get_body_count()
    local fps = math.floor(crayon.window.get_fps() + 0.5)

    crayon.graphics.set_color(1.0, 0.9, 0.2, 1.0)
    crayon.graphics.draw_text("JOLT 3D PHYSICS SANDBOX", 8, 8, 1.0)
    crayon.graphics.set_color(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.draw_text("FPS: " .. fps .. " | Active: " .. active_bodies .. "/" .. total_bodies, 8, 18, 1.0)
    crayon.graphics.set_color(0.8, 0.85, 1.0, 1.0)
    crayon.graphics.draw_text("Click/F: Shoot Cannonball | B: Ball Rain", 8, 28, 1.0)
    crayon.graphics.draw_text("E: Blast Impulse | TAB: Wireframe | R: Reset", 8, 38, 1.0)

    -- Reticle
    crayon.graphics.set_color(hit and 1.0 or 0.7, hit and 0.2 or 0.7, hit and 0.2 or 0.7, 0.9)
    crayon.graphics.draw_line(156, 120, 164, 120, 1.0)
    crayon.graphics.draw_line(160, 116, 160, 124, 1.0)
    if hit then
        crayon.graphics.draw_text("HIT (" .. math.floor(hdist * 10) / 10 .. "m)", 168, 116, 1.0)
    end
end
