-- ============================================================================
-- Example 11: High-Performance 3D Jolt Physics Sandbox
-- Stacking, rigid bodies, cannonball shooting, impulses, raycast & debug draw
-- Refactored to Love2D-style userdata architecture (camelCase API)
-- ============================================================================

local cam = {
    x = 0.0, y = 5.0, z = 14.0,
    yaw = -90.0, pitch = -15.0,
    fov = 60.0
}

local models = {}
local textures = {}
local bodies = {} -- list of { body = body_userdata, type = "cube"|"sphere"|"cylinder", size = {w,h,d}, color = {r,g,b} }

local show_debug = false
local auto_spawn_timer = 0.0
local floor_body = nil

local function spawn_cube(x, y, z, sx, sy, sz, motion, friction, restitution)
    motion = motion or "dynamic"
    friction = friction or 0.6
    restitution = restitution or 0.2
    local body = crayon.physics3d.createBox(x, y, z, sx * 0.5, sy * 0.5, sz * 0.5, motion, friction, restitution)
    table.insert(bodies, {
        body = body,
        type = "cube",
        size = {sx, sy, sz},
        color = {0.85 + math.random() * 0.15, 0.65 + math.random() * 0.2, 0.45}
    })
    return body
end

local function spawn_sphere(x, y, z, radius, motion, friction, restitution)
    motion = motion or "dynamic"
    friction = friction or 0.4
    restitution = restitution or 0.7
    local body = crayon.physics3d.createSphere(x, y, z, radius, motion, friction, restitution)
    table.insert(bodies, {
        body = body,
        type = "sphere",
        radius = radius,
        color = {0.2 + math.random() * 0.8, 0.4 + math.random() * 0.6, 0.9}
    })
    return body
end

local function reset_scene()
    crayon.physics3d.destroyAll()
    bodies = {}

    -- Static Ground Plane
    floor_body = crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 50.0)

    -- Static Boundary Walls
    crayon.physics3d.createBox(-20, 2.5, 0, 0.5, 2.5, 20, "static", 0.5, 0.1)
    crayon.physics3d.createBox( 20, 2.5, 0, 0.5, 2.5, 20, "static", 0.5, 0.1)
    crayon.physics3d.createBox(0, 2.5, -20, 20, 2.5, 0.5, "static", 0.5, 0.1)
    crayon.physics3d.createBox(0, 2.5,  20, 20, 2.5, 0.5, "static", 0.5, 0.1)

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
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("Crayon Engine - High-Speed Jolt 3D Physics Sandbox")

    models.cube   = crayon.graphics.loadModel("cube")
    models.sphere = crayon.graphics.loadModel("sphere")
    models.plane  = crayon.graphics.loadModel("plane")

    textures.crate = crayon.graphics.loadTexture("game/assets/textures/crate.bmp")
    textures.grass = crayon.graphics.loadTexture("game/assets/textures/grass.bmp")

    -- Introspection test
    local cw, ch = textures.crate:getSize()
    print(string.format("[Init] Crate texture loaded: %dx%d", cw, ch))

    crayon.graphics.setRetroEffects({
        jitterResolution = {160, 120},
        affine = 0.8,
        dither = true,
        fog = { startDist = 12, endDist = 40, color = {0.08, 0.10, 0.16} }
    })

    crayon.graphics.setLight(-0.4, -0.9, -0.6, 1.0, 0.95, 0.85, 0.35, 0.35, 0.42)

    reset_scene()
end

function crayon.update(dt)
    -- Camera Orbit & Movement
    local move_speed = 7.0 * dt
    local rad_yaw = math.rad(cam.yaw)
    local fwd_x, fwd_z = math.cos(rad_yaw), math.sin(rad_yaw)
    local right_x, right_z = -fwd_z, fwd_x

    if crayon.input.isDown("w") then cam.x = cam.x + fwd_x * move_speed; cam.z = cam.z + fwd_z * move_speed end
    if crayon.input.isDown("s") then cam.x = cam.x - fwd_x * move_speed; cam.z = cam.z - fwd_z * move_speed end
    if crayon.input.isDown("a") then cam.x = cam.x - right_x * move_speed; cam.z = cam.z - right_z * move_speed end
    if crayon.input.isDown("d") then cam.x = cam.x + right_x * move_speed; cam.z = cam.z + right_z * move_speed end
    if crayon.input.isDown("q") or crayon.input.isDown("space") then cam.y = cam.y + move_speed end
    if crayon.input.isDown("z") or crayon.input.isDown("lshift") then cam.y = cam.y - move_speed end

    if crayon.input.isDown("left") then cam.yaw = cam.yaw - 80.0 * dt end
    if crayon.input.isDown("right") then cam.yaw = cam.yaw + 80.0 * dt end
    if crayon.input.isDown("up") then cam.pitch = math.min(cam.pitch + 60.0 * dt, 80.0) end
    if crayon.input.isDown("down") then cam.pitch = math.max(cam.pitch - 60.0 * dt, -80.0) end

    -- Shoot Cannonball along Camera Look Direction (Left Mouse or 'F')
    local rad_pitch = math.rad(cam.pitch)
    local look_x = math.cos(rad_pitch) * math.cos(rad_yaw)
    local look_y = math.sin(rad_pitch)
    local look_z = math.cos(rad_pitch) * math.sin(rad_yaw)

    if crayon.input.isMouseDown(1) or crayon.input.isPressed("f") then
        local ball = spawn_sphere(cam.x + look_x * 1.5, cam.y + look_y * 1.5, cam.z + look_z * 1.5, 0.45, "dynamic", 0.4, 0.6)
        local shoot_speed = 35.0
        ball:setVelocity(look_x * shoot_speed, look_y * shoot_speed, look_z * shoot_speed)
    end

    -- Rain random tumbling spheres on 'B'
    if crayon.input.isPressed("b") then
        for _ = 1, 10 do
            local rx = (math.random() - 0.5) * 8.0
            local rz = (math.random() - 0.5) * 8.0
            local ry = 8.0 + math.random() * 6.0
            spawn_sphere(rx, ry, rz, 0.35 + math.random() * 0.25)
        end
    end

    -- Radial Blast / Explosion Impulse on 'E'
    if crayon.input.isPressed("e") then
        for _, b in ipairs(bodies) do
            if b.body:isValid() then
                local bx, by, bz = b.body:getPosition()
                local dx, dy, dz = bx - 0.0, by - 0.5, bz - 0.0
                local dist_sq = dx * dx + dy * dy + dz * dz
                if dist_sq < 100.0 and dist_sq > 0.01 then
                    local dist = math.sqrt(dist_sq)
                    local force = (1.0 - dist / 10.0) * 80.0
                    b.body:applyImpulse((dx / dist) * force, (dy / dist + 0.6) * force, (dz / dist) * force)
                end
            end
        end
    end

    -- Toggle Debug Wireframe on 'TAB' or 'G'
    if crayon.input.isPressed("tab") or crayon.input.isPressed("g") then
        show_debug = not show_debug
    end

    -- Reset Scene on 'R'
    if crayon.input.isPressed("r") then
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

    crayon.graphics.setCamera3d({
        position = {cam.x, cam.y, cam.z},
        target = {target_x, target_y, target_z},
        up = {0, 1, 0},
        fov = cam.fov
    })

    -- Render Ground
    crayon.graphics.drawModel(models.plane, 0, 0, 0, 0, 0, 0, 8.0, 1.0, 8.0, textures.grass)

    -- Render Dynamic Bodies
    for _, b in ipairs(bodies) do
        if b.body:isValid() then
            local px, py, pz = b.body:getPosition()
            local rx, ry, rz = b.body:getRotation()

            if b.type == "cube" then
                local sx, sy, sz = b.size[1], b.size[2], b.size[3]
                crayon.graphics.setColor(b.color[1], b.color[2], b.color[3], 1.0)
                crayon.graphics.drawModel(models.cube, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), sx, sy, sz, textures.crate)
            elseif b.type == "sphere" then
                local r = b.radius
                crayon.graphics.setColor(b.color[1], b.color[2], b.color[3], 1.0)
                crayon.graphics.drawModel(models.sphere, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), r * 2.0, r * 2.0, r * 2.0)
            end
        end
    end
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)

    -- Raycast Crosshair Aim Test
    local look_x = math.cos(rad_pitch) * math.cos(rad_yaw)
    local look_y = math.sin(rad_pitch)
    local look_z = math.cos(rad_pitch) * math.sin(rad_yaw)
    local hit, hx, hy, hz, nx, ny, nz, hdist, hbody = crayon.physics3d.raycast(cam.x, cam.y, cam.z, look_x, look_y, look_z, 50.0)

    -- Debug Wireframes (optional)
    if show_debug then
        crayon.physics3d.drawDebug(0.3, 1.0, 0.4, 1.0, 0.6, 0.6, 0.7, 0.8)
    end

    -- 2D HUD Overlays
    crayon.graphics.setColor(0.0, 0.0, 0.0, 0.6)
    crayon.graphics.drawRect("fill", 4, 4, 185, 52)
    crayon.graphics.setColor(0.3, 0.6, 1.0, 1.0)
    crayon.graphics.drawRect("line", 4, 4, 185, 52)

    local total_bodies, active_bodies = crayon.physics3d.getBodyCount()
    local fps = math.floor(crayon.window.getFps() + 0.5)

    crayon.graphics.setColor(1.0, 0.9, 0.2, 1.0)
    crayon.graphics.drawText("JOLT 3D PHYSICS SANDBOX", 8, 8, 1.0)
    crayon.graphics.setColor(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.drawText("FPS: " .. fps .. " | Active: " .. active_bodies .. "/" .. total_bodies, 8, 18, 1.0)
    crayon.graphics.setColor(0.8, 0.85, 1.0, 1.0)
    crayon.graphics.drawText("Click/F: Shoot Cannonball | B: Ball Rain", 8, 28, 1.0)
    crayon.graphics.drawText("E: Blast Impulse | TAB: Wireframe | R: Reset", 8, 38, 1.0)

    -- Reticle
    crayon.graphics.setColor(hit and 1.0 or 0.7, hit and 0.2 or 0.7, hit and 0.2 or 0.7, 0.9)
    crayon.graphics.drawLine(156, 120, 164, 120, 1.0)
    crayon.graphics.drawLine(160, 116, 160, 124, 1.0)
    if hit then
        crayon.graphics.drawText("HIT (" .. math.floor(hdist * 10) / 10 .. "m)", 168, 116, 1.0)
    end
end