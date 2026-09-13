-- =====================================================================
-- Example 19: 3D glTF Level Game Development
-- Crayon Engine
-- Demonstrates:
--  - Loading glTF models as 3D level environments
--  - Extracting node positions & markers for spawn points and props
--  - Generating accurate static trimesh physics bodies using createMeshBody
--  - Player ball physics controller rolling on glTF level terrain/platforms
--  - Dynamic lighting, collectible items, and retro post-processing
-- ====================================================================

local cam = {
    x = 0.0, y = 6.0, z = 12.0,
    target_x = 0.0, target_y = 1.0, target_z = 0.0,
    yaw = -90.0, pitch = -25.0,
    dist = 9.0
}

-- Level & Models
local level_model = nil
local prop_model = nil
local level_body = nil

-- Player State
local player = {
    body = nil,
    radius = 0.5,
    speed = 18.0,
    jump_force = 6.0,
    score = 0
}

-- Dynamic Objects & Collectibles
local collectibles = {}
local dynamic_cubes = {}
local timer = 0.0
local message_timer = 3.0

local function spawn_player(x, y, z)
    if player.body and player.body:isValid() then
        player.body:destroy()
    end
    -- Create dynamic sphere physics body
    player.body = crayon.physics3d.createSphere(x, y, z, player.radius, "dynamic", 0.7, 0.4, 1200.0)
    player.body:setDamping(0.2, 0.3)
end

local function reset_game()
    crayon.physics3d.destroyAll()
    collectibles = {}
    dynamic_cubes = {}

    -- 1. Load glTF Level Model
    level_model = crayon.graphics.loadModel("game/assets/models/cube_gltf/Cube.gltf")
    prop_model  = crayon.graphics.loadModel("game/assets/models/box01.glb")

    -- 2. Build Level from glTF using createMeshBody
    -- Platform 1: Main Arena (scaled glTF mesh collider)
    level_body = crayon.physics3d.createMeshBody(0, 0, 0, level_model, 0.6, 0.1)

    -- Platform 2: Upper ledge
    crayon.physics3d.createBox(4.0, 1.5, -3.0, 2.0, 0.25, 2.0, "static", 0.6, 0.1)

    -- Platform 3: Floating ramp / bridge
    crayon.physics3d.createBox(-4.0, 1.0, 2.0, 2.5, 0.2, 1.5, "static", 0.6, 0.1)

    -- Static boundary fences
    crayon.physics3d.createBox(0, 1.0, -9.0, 10.0, 1.0, 0.3, "static", 0.5, 0.2)
    crayon.physics3d.createBox(0, 1.0,  9.0, 10.0, 1.0, 0.3, "static", 0.5, 0.2)
    crayon.physics3d.createBox(-9.0, 1.0, 0, 0.3, 1.0, 10.0, "static", 0.5, 0.2)
    crayon.physics3d.createBox( 9.0, 1.0, 0, 0.3, 1.0, 10.0, "static", 0.5, 0.2)

    -- 3. Spawn Player
    spawn_player(0.0, 3.5, 0.0)

    -- 4. Spawn Dynamic Physics Boxes
    for i = 1, 5 do
        local bx = -2.0 + math.random() * 4.0
        local bz = -2.0 + math.random() * 4.0
        local by = 4.0 + i * 1.2
        local b = crayon.physics3d.createBox(bx, by, bz, 0.4, 0.4, 0.4, "dynamic", 0.5, 0.3)
        table.insert(dynamic_cubes, {
            body = b,
            size = {0.8, 0.8, 0.8},
            color = {0.9, 0.6 + i * 0.06, 0.2}
        })
    end

    -- 5. Spawn Collectible Energy Orbs
    local orb_spots = {
        { x = 0, y = 1.2, z = -5 },
        { x = 4, y = 2.4, z = -3 },
        { x = -4, y = 1.8, z = 2 },
        { x = -5, y = 1.2, z = -4 },
        { x = 5, y = 1.2, z = 4 },
        { x = 0, y = 1.2, z = 5 }
    }
    for _, s in ipairs(orb_spots) do
        table.insert(collectibles, {
            x = s.x, y = s.y, z = s.z,
            collected = false,
            rot = math.random() * 360
        })
    end

    player.score = 0
end

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("19 - 3D glTF Level Game [WASD: Roll, SPACE: Jump, R: Reset]")

    reset_game()

    crayon.graphics.setRetroEffects({
        jitterResolution = {240, 160},
        affine = 1.0,
        dither = true,
        fog = { startDist = 12, endDist = 35, color = {0.08, 0.09, 0.14} }
    })

    -- Sunlight directional light
    crayon.graphics.setLight(
        -0.4, -0.9, -0.5,
        1.0, 0.94, 0.85,
        0.28, 0.28, 0.35
    )
end

function crayon.update(dt)
    timer = timer + dt
    if message_timer > 0 then message_timer = message_timer - dt end

    if crayon.input.isPressed("r") then
        reset_game()
        return
    end

    -- Player Ball Movement Controls
    if player.body and player.body:isValid() then
        local move_x = 0.0
        local move_z = 0.0

        if crayon.input.isDown("w") or crayon.input.isDown("up") then
            move_z = move_z - 1.0
        end
        if crayon.input.isDown("s") or crayon.input.isDown("down") then
            move_z = move_z + 1.0
        end
        if crayon.input.isDown("a") or crayon.input.isDown("left") then
            move_x = move_x - 1.0
        end
        if crayon.input.isDown("d") or crayon.input.isDown("right") then
            move_x = move_x + 1.0
        end

        if move_x ~= 0 or move_z ~= 0 then
            local len = math.sqrt(move_x * move_x + move_z * move_z)
            move_x = (move_x / len) * player.speed
            move_z = (move_z / len) * player.speed
            -- Apply torque to roll realistically
            player.body:applyTorque(move_z * 2.5, 0.0, -move_x * 2.5)
            player.body:applyForce(move_x * 3.0, 0.0, move_z * 3.0)
        end

        -- Jump
        if crayon.input.isPressed("space") then
            local px, py, pz = player.body:getPosition()
            -- Simple raycast down to check if grounded
            local hit = crayon.physics3d.raycast(px, py, pz, 0, -1, 0, player.radius + 0.25)
            if hit then
                player.body:applyImpulse(0, player.jump_force * 100.0, 0)
            end
        end

        -- Respawn if fallen off platform
        local px, py, pz = player.body:getPosition()
        if py < -6.0 then
            spawn_player(0, 3.5, 0)
        end

        -- Camera follow player smoothly
        cam.target_x = cam.target_x + (px - cam.target_x) * 6.0 * dt
        cam.target_y = cam.target_y + (py + 0.5 - cam.target_y) * 6.0 * dt
        cam.target_z = cam.target_z + (pz - cam.target_z) * 6.0 * dt

        -- Collectible pickup check
        for _, c in ipairs(collectibles) do
            if not c.collected then
                c.rot = c.rot + 90.0 * dt
                local dx = px - c.x
                local dy = py - c.y
                local dz = pz - c.z
                local dist_sq = dx*dx + dy*dy + dz*dz
                if dist_sq < 1.0 then
                    c.collected = true
                    player.score = player.score + 100
                end
            end
        end
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.14)

    -- Orbit / Follow 3D Camera
    local rad_yaw   = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local cx = cam.target_x + cam.dist * math.cos(rad_pitch) * math.cos(rad_yaw)
    local cy = cam.target_y - cam.dist * math.sin(rad_pitch)
    local cz = cam.target_z + cam.dist * math.cos(rad_pitch) * math.sin(rad_yaw)

    crayon.graphics.setCamera3d({
        position = {cx, cy, cz},
        target   = {cam.target_x, cam.target_y, cam.target_z},
        up       = {0, 1, 0},
        fov      = 60.0
    })

    -- 1. Draw Main glTF Level Platform
    if level_model and level_model:isValid() then
        crayon.graphics.pushMatrix()
        crayon.graphics.translate(0, 0, 0)
        crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
        crayon.graphics.drawModel(level_model)
        crayon.graphics.popMatrix()
    end

    -- 2. Draw Decorative Level Platforms & Structures
    -- Ledge platform
    crayon.graphics.setColor(0.35, 0.45, 0.65, 1.0)
    crayon.graphics.drawCube(4.0, 1.5, -3.0, 4.0, 0.5, 4.0)

    -- Ramp / bridge
    crayon.graphics.setColor(0.55, 0.40, 0.35, 1.0)
    crayon.graphics.drawCube(-4.0, 1.0, 2.0, 5.0, 0.4, 3.0)

    -- Boundary fences
    crayon.graphics.setColor(0.25, 0.28, 0.35, 0.8)
    crayon.graphics.drawCubeWires(0, 1.0, -9.0, 20.0, 2.0, 0.6)
    crayon.graphics.drawCubeWires(0, 1.0,  9.0, 20.0, 2.0, 0.6)
    crayon.graphics.drawCubeWires(-9.0, 1.0, 0, 0.6, 2.0, 20.0)
    crayon.graphics.drawCubeWires( 9.0, 1.0, 0, 0.6, 2.0, 20.0)

    -- 3. Draw glTF Prop Models (box01.glb) as level decoration
    if prop_model and prop_model:isValid() then
        crayon.graphics.pushMatrix()
        crayon.graphics.translate(4.0, 2.0, -3.0)
        crayon.graphics.scale(0.8, 0.8, 0.8)
        crayon.graphics.setColor(1.0, 0.9, 0.8, 1.0)
        crayon.graphics.drawModel(prop_model)
        crayon.graphics.popMatrix()

        crayon.graphics.pushMatrix()
        crayon.graphics.translate(-4.0, 1.5, 2.0)
        crayon.graphics.scale(0.6, 0.6, 0.6)
        crayon.graphics.setColor(0.8, 1.0, 0.9, 1.0)
        crayon.graphics.drawModel(prop_model)
        crayon.graphics.popMatrix()
    end

    -- 4. Draw Dynamic Physics Cubes
    for _, item in ipairs(dynamic_cubes) do
        if item.body and item.body:isValid() then
            local bx, by, bz = item.body:getPosition()
            local rx, ry, rz, rw = item.body:getRotation()
            crayon.graphics.pushMatrix()
            crayon.graphics.translate(bx, by, bz)
            crayon.graphics.setColor(item.color[1], item.color[2], item.color[3], 1.0)
            crayon.graphics.drawCube(0, 0, 0, item.size[1], item.size[2], item.size[3])
            crayon.graphics.popMatrix()
        end
    end

    -- 5. Draw Collectibles (Floating Rotating Octahedrons/Pyramids)
    for _, c in ipairs(collectibles) do
        if not c.collected then
            crayon.graphics.pushMatrix()
            crayon.graphics.translate(c.x, c.y + math.sin(timer * 3.0 + c.x) * 0.15, c.z)
            crayon.graphics.rotate(c.rot, 0, 1, 0)
            crayon.graphics.setColor(1.0, 0.85, 0.15, 1.0)
            crayon.graphics.drawPyramid(0, 0, 0, 0.4, 0.6)
            crayon.graphics.popMatrix()
        end
    end

    -- 6. Draw Player Ball
    if player.body and player.body:isValid() then
        local px, py, pz = player.body:getPosition()
        local rx, ry, rz, rw = player.body:getRotation()
        crayon.graphics.pushMatrix()
        crayon.graphics.translate(px, py, pz)
        crayon.graphics.setColor(0.2, 0.7, 1.0, 1.0)
        crayon.graphics.drawSphere(0, 0, 0, player.radius)
        -- Wire overlay to show rotation
        crayon.graphics.setColor(1.0, 1.0, 1.0, 0.5)
        crayon.graphics.drawCubeWires(0, 0, 0, player.radius * 1.5, player.radius * 1.5, player.radius * 1.5)
        crayon.graphics.popMatrix()
    end

    -- ===================================================================
    -- 2D HUD
    -- =================================================================
    -- Top Banner
    crayon.graphics.setColor(0.04, 0.05, 0.08, 0.8)
    crayon.graphics.drawRect("fill", 6, 6, 308, 28)
    crayon.graphics.setColor(0.2, 0.5, 0.8, 0.8)
    crayon.graphics.drawRect("line", 6, 6, 308, 28)

    crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.drawText(string.format("SCORE: %04d", player.score), 12, 14)

    local remaining = 0
    for _, c in ipairs(collectibles) do
        if not c.collected then remaining = remaining + 1 end
    end
    crayon.graphics.setColor(0.7, 0.8, 0.9, 1.0)
    crayon.graphics.drawText(string.format("ORBS LEFT: %d", remaining), 120, 14)

    if remaining == 0 then
        crayon.graphics.setColor(0.3, 1.0, 0.4, 1.0)
        crayon.graphics.drawText("STAGE CLEAR! Press R", 200, 14)
    end

    -- Controls Help
    crayon.graphics.setColor(0.04, 0.05, 0.08, 0.7)
    crayon.graphics.drawRect("fill", 6, 218, 308, 16)
    crayon.graphics.setColor(0.5, 0.6, 0.7, 0.9)
    crayon.graphics.drawText("WASD: Roll Ball | SPACE: Jump | R: Reset Level", 12, 222)
end