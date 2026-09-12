-- ============================================================================
-- Example 10: 3D OBJ Model Loader & Model Inspector
-- Crayon Engine
-- ============================================================================

local model_files = {
    { name = "Suzanne (Monkey)", path = "game/assets/models/monkey.obj", scale = 1.0, y_offset = 0.0 },
    { name = "Cube (OBJ)",        path = "game/assets/models/cube.obj",   scale = 1.0, y_offset = 0.0 },
    { name = "Model",             path = "game/assets/models/model.obj",  scale = 1.0, y_offset = 0.0 }
}

local loaded_models = {}
local cur_model_idx = 1

local textures = {}
local cur_tex_idx = 1
local tex_list = {}

local shading_modes = {"flat", "gouraud", "unlit"}
local cur_shading_idx = 1

local auto_rotate = true
local rot_y = 0
local timer = 0

local cam = {
    yaw = 45.0,
    pitch = -20.0,
    dist = 4.5,
    target_y = 0.0
}

function crayon.init()
    crayon.window.setResolution(320, 240)
    -- crayon.window.setResolution(480, 360)
    crayon.window.setTitle("10 - 3D OBJ Model Inspector [TAB: Model, 1: Shading, 2: Texture]")

    -- Load textures
    textures.none  = 0
    textures.crate = crayon.graphics.loadTexture("game/assets/textures/crate.bmp")
    textures.brick = crayon.graphics.loadTexture("game/assets/textures/brick.bmp")
    textures.grass = crayon.graphics.loadTexture("game/assets/textures/grass.bmp")
    tex_list = {
        { name = "None (Color)", id = 0 },
        { name = "Crate", id = textures.crate },
        { name = "Brick", id = textures.brick },
        { name = "Grass", id = textures.grass }
    }

    -- Load first OBJ model
    load_current_model()

    crayon.graphics.setRetroEffects({
        jitterResolution = {240, 160},
        affine = 1.0,
        dither = true
    })

    -- Warm studio lighting
    crayon.graphics.setLight(
        -0.5, -0.8, -0.6,
        1.0, 0.95, 0.9,
        0.3, 0.3, 0.35
    )
end

function load_current_model()
    local item = model_files[cur_model_idx]
    if not loaded_models[cur_model_idx] then
        loaded_models[cur_model_idx] = crayon.graphics.loadModel(item.path)
    end
end

function crayon.update(dt)
    timer = timer + dt

    if auto_rotate then
        rot_y = rot_y + 40.0 * dt
    end

    -- Tab: Next Model
    if crayon.input.isPressed("tab") or crayon.input.isPressed("space") then
        cur_model_idx = (cur_model_idx % #model_files) + 1
        load_current_model()
    end

    -- 1: Cycle Shading Mode
    if crayon.input.isPressed("1") then
        cur_shading_idx = (cur_shading_idx % #shading_modes) + 1
        crayon.graphics.setShadingMode(shading_modes[cur_shading_idx])
    end

    -- 2: Cycle Texture
    if crayon.input.isPressed("2") then
        cur_tex_idx = (cur_tex_idx % #tex_list) + 1
    end

    -- R: Toggle Auto-Rotate
    if crayon.input.isPressed("r") then
        auto_rotate = not auto_rotate
    end

    -- Camera Orbit Controls
    if crayon.input.isDown("left") or crayon.input.isDown("a") then
        cam.yaw = cam.yaw - 60.0 * dt
    end
    if crayon.input.isDown("right") or crayon.input.isDown("d") then
        cam.yaw = cam.yaw + 60.0 * dt
    end
    if crayon.input.isDown("up") or crayon.input.isDown("w") then
        cam.pitch = math.min(80.0, cam.pitch + 45.0 * dt)
    end
    if crayon.input.isDown("down") or crayon.input.isDown("s") then
        cam.pitch = math.max(-80.0, cam.pitch - 45.0 * dt)
    end

    -- Zoom
    local wheel_x, wheel_y = crayon.input.getMouseWheel()
    if wheel_y and wheel_y ~= 0 then
        cam.dist = math.max(1.0, math.min(15.0, cam.dist - wheel_y * 0.6))
    end
    if crayon.input.isDown("q") then cam.dist = math.min(15.0, cam.dist + 3.0 * dt) end
    if crayon.input.isDown("e") then cam.dist = math.max(1.0, cam.dist - 3.0 * dt) end

    if crayon.input.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.14)

    -- 3D Camera Orbit
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local cx = math.cos(rad_pitch) * math.cos(rad_yaw) * cam.dist
    local cy = -math.sin(rad_pitch) * cam.dist + cam.target_y
    local cz = math.cos(rad_pitch) * math.sin(rad_yaw) * cam.dist

    crayon.graphics.setCamera3d({
        position = {cx, cy, cz},
        target = {0, cam.target_y, 0},
        up = {0, 1, 0},
        fov = 55.0,
        near = 0.1,
        far = 100.0
    })

    -- 1. Studio Turntable Grid
    crayon.graphics.setColor(0.25, 0.3, 0.45, 0.6)
    crayon.graphics.drawGrid3d(8, 8, -1.0)

    -- 2. Coordinate Axes
    crayon.graphics.setColor(0.9, 0.2, 0.2, 0.8)
    crayon.graphics.drawLine3d(-1.0, -1.0, 0, 1.0, -1.0, 0)
    crayon.graphics.setColor(0.2, 0.4, 0.9, 0.8)
    crayon.graphics.drawLine3d(0, -1.0, -1.0, 0, -1.0, 1.0)

    -- 3. Draw Loaded OBJ Model
    local item = model_files[cur_model_idx]
    local handle = loaded_models[cur_model_idx]
    if handle and handle:isValid() then
        local rad_r = math.rad(rot_y)
        local cur_tex = tex_list[cur_tex_idx].id
        crayon.graphics.setColor(0.95, 0.95, 0.98, 1.0)
        -- drawModel(model, x, y, z, rx, ry, rz, sx, sy, sz, tex)
        crayon.graphics.drawModel(
            handle,
            0, item.y_offset, 0,
            0, rad_r, 0,
            item.scale, item.scale, item.scale,
            cur_tex
        )
    end

    -- ========================================================================
    -- 2D HUD OVERLAY
    -- ========================================================================
    crayon.graphics.setColor(0.06, 0.08, 0.14, 0.85)
    crayon.graphics.drawRect("fill", 4, 4, 312, 22)
    crayon.graphics.setColor(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.drawRect("line", 4, 4, 312, 22)

    crayon.graphics.setColor(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.drawText("3D OBJ MODEL INSPECTOR", 8, 10, 1.0)

    local fps = math.floor(crayon.window.getFps() + 0.5)
    crayon.graphics.setColor(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.drawText("FPS: " .. fps, 265, 10, 1.0)

    -- Bottom Inspector Panel
    crayon.graphics.setColor(0.06, 0.08, 0.14, 0.9)
    crayon.graphics.drawRect("fill", 4, 196, 312, 40)
    crayon.graphics.setColor(0.25, 0.35, 0.6, 1.0)
    crayon.graphics.drawRect("line", 4, 196, 312, 40)

    crayon.graphics.setColor(0.95, 0.95, 1.0, 1.0)
    crayon.graphics.drawText("[TAB/SPACE] Model: " .. item.name, 10, 201, 1.0)

    crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.drawText("[1] Shading: " .. string.upper(shading_modes[cur_shading_idx]), 10, 213, 1.0)

    crayon.graphics.setColor(0.4, 0.85, 1.0, 1.0)
    crayon.graphics.drawText("[2] Tex: " .. tex_list[cur_tex_idx].name, 155, 213, 1.0)

    crayon.graphics.setColor(0.65, 0.7, 0.8, 1.0)
    crayon.graphics.drawText("Arrows/WASD: Orbit | Scroll/Q/E: Zoom | [R]: Spin", 10, 226, 1.0)
end