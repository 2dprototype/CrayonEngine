-- ============================================================================
-- Example 09: Custom Procedural 3D Mesh Generation & Vertex Colors
-- Crayon Engine
-- ============================================================================

local terrain_mesh = 0
local tex_grass = 0
local timer = 0
local wireframe = false
local use_texture = false

local cam = {
    yaw = 45.0,
    pitch = -30.0,
    dist = 12.0
}

-- Generate dynamic sinusoidal terrain grid
function build_sine_terrain(time_val)
    local grid_res = 16
    local size = 8.0
    local step = size / grid_res
    local half_size = size * 0.5

    local vertices = {}
    local indices = {}

    -- Generate vertices
    for z = 0, grid_res do
        for x = 0, grid_res do
            local wx = x * step - half_size
            local wz = z * step - half_size
            -- Double sine wave height
            local wy = math.sin(wx * 0.9 + time_val * 2.0) * 0.5
                     + math.cos(wz * 0.9 + time_val * 1.5) * 0.5

            -- Approximate normal via partial derivatives
            local dX = 0.9 * math.cos(wx * 0.9 + time_val * 2.0) * 0.5
            local dZ = -0.9 * math.sin(wz * 0.9 + time_val * 1.5) * 0.5
            local nx = -dX
            local ny = 1.0
            local nz = -dZ
            local nlen = math.sqrt(nx * nx + ny * ny + nz * nz)

            -- Vertex color gradient based on elevation
            local height_factor = (wy + 1.0) * 0.5 -- 0 to 1
            local cr = 0.2 + height_factor * 0.7
            local cg = 0.8 - height_factor * 0.3
            local cb = 0.3 + (1.0 - height_factor) * 0.6

            table.insert(vertices, {
                pos = {wx, wy, wz},
                norm = {nx / nlen, ny / nlen, nz / nlen},
                uv = {x / grid_res * 2.0, z / grid_res * 2.0},
                color = {cr, cg, cb, 1.0}
            })
        end
    end

    -- Generate quad triangle indices (1-based for Lua; engine converts to 0-based)
    for z = 0, grid_res - 1 do
        for x = 0, grid_res - 1 do
            local row1 = z * (grid_res + 1)
            local row2 = (z + 1) * (grid_res + 1)

            local p1 = row1 + x + 1
            local p2 = row1 + x + 2
            local p3 = row2 + x + 1
            local p4 = row2 + x + 2

            -- Triangle 1 (winding matches terrain up-normal)
            table.insert(indices, p1)
            table.insert(indices, p3)
            table.insert(indices, p2)

            -- Triangle 2
            table.insert(indices, p2)
            table.insert(indices, p3)
            table.insert(indices, p4)
        end
    end

    return crayon.graphics.createMesh({
        vertices = vertices,
        indices = indices
    })
end

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("09 - Custom Procedural Mesh [SPACE: Animate, 1: Tex]")

    tex_grass = crayon.graphics.loadTexture("../assets/textures/grass.bmp")

    crayon.graphics.setRetroEffects({
        jitterResolution = {240, 160},
        affine = 1.0,
        dither = true
    })

    crayon.graphics.setLight(
        -0.5, -0.9, -0.4,
        1.0, 0.95, 0.85,
        0.3, 0.3, 0.35
    )

    terrain_mesh = build_sine_terrain(0)
end

function crayon.update(dt)
    timer = timer + dt

    -- Camera Controls
    -- FIX: crayon.input.isDown doesn't exist; use crayon.key.isDown
    if crayon.key.isDown("left")  or crayon.key.isDown("a") then cam.yaw   = cam.yaw   - 45.0 * dt end
    if crayon.key.isDown("right") or crayon.key.isDown("d") then cam.yaw   = cam.yaw   + 45.0 * dt end
    if crayon.key.isDown("up")    or crayon.key.isDown("w") then cam.pitch = math.min(-10.0, cam.pitch + 35.0 * dt) end
    if crayon.key.isDown("down")  or crayon.key.isDown("s") then cam.pitch = math.max(-80.0, cam.pitch - 35.0 * dt) end

    -- Zoom (getMouseWheel DOES exist on crayon.input — keep it)
    local wheel_x, wheel_y = crayon.mouse.getWheel()
    if wheel_y and wheel_y ~= 0 then
        cam.dist = math.max(4.0, math.min(25.0, cam.dist - wheel_y * 1.0))
    end

    -- Rebuild animated mesh while SPACE is held
    -- FIX: crayon.input.isDown doesn't exist; use crayon.key.isDown
    if crayon.key.isDown("space") then
        terrain_mesh = build_sine_terrain(timer)
    end

    -- FIX: crayon.input.isPressed doesn't exist; use crayon.key.isPressed
    if crayon.key.isPressed("1") then
        use_texture = not use_texture
    end

    if crayon.key.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.14)

    -- Camera Orbit
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local cx = math.cos(rad_pitch) * math.cos(rad_yaw) * cam.dist
    local cy = -math.sin(rad_pitch) * cam.dist
    local cz = math.cos(rad_pitch) * math.sin(rad_yaw) * cam.dist

    crayon.graphics.setCamera3D({
        position = {cx, cy, cz},
        target = {0, 0, 0},
        up = {0, 1, 0},
        fov = 55.0
    })

    -- 1. Reference Grid below
    crayon.graphics.setColor(0.2, 0.25, 0.35, 0.4)
    crayon.graphics.drawGrid3D(12, 12, -1.2)

    -- 2. Draw Custom Procedural Mesh
    -- drawModel(model, x, y, z, rx, ry, rz, sx, sy, sz, tex)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawModel(terrain_mesh, 0, 0, 0, 0, 0, 0, 1, 1, 1,
                              use_texture and tex_grass or 0)

    -- ========================================================================
    -- 2D HUD OVERLAY
    -- ========================================================================
    crayon.graphics.setColor(0.06, 0.08, 0.14, 0.85)
    crayon.graphics.drawRect("fill", 4, 4, 312, 22)
    crayon.graphics.setColor(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.drawRect("line", 4, 4, 312, 22)

    crayon.graphics.setColor(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.drawText("PROCEDURAL MESH GENERATION", 8, 10, 1.0)

    local fps = math.floor(crayon.time.getFps() + 0.5)
    crayon.graphics.setColor(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.drawText("FPS: " .. fps, 265, 10, 1.0)

    -- Bottom Info
    crayon.graphics.setColor(0.06, 0.08, 0.14, 0.85)
    crayon.graphics.drawRect("fill", 4, 204, 312, 32)
    crayon.graphics.setColor(0.25, 0.35, 0.6, 1.0)
    crayon.graphics.drawRect("line", 4, 204, 312, 32)

    crayon.graphics.setColor(0.9, 0.95, 1.0, 1.0)
    crayon.graphics.drawText("Hold [SPACE]: Real-time Dynamic Wave Generation", 10, 208, 1.0)
    crayon.graphics.setColor(0.6, 0.75, 0.9, 1.0)
    crayon.graphics.drawText("[1] Texture: " .. (use_texture and "ON" or "OFF (Vertex Color)") .. " | Arrows: Orbit", 10, 222, 1.0)
end