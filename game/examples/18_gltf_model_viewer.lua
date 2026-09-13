-- =====================================================================
-- Example 18: 3D glTF / GLB Model Loader & Inspector
-- Crayon Engine
-- ====================================================================

local model_files = {
    { 
        name = "Box 01 (Binary GLB)", 
        path = "game/assets/models/box01.glb", 
        scale = 1.0, 
        y_offset = 0.0,
        desc = "Single-file binary glTF with embedded meshes & buffers"
    },
    { 
        name = "PBR Cube (glTF + Texture)", 
        path = "game/assets/models/cube_gltf/Cube.gltf", 
        scale = 1.5, 
        y_offset = 0.0,
        desc = "glTF separate file structure (.gltf + .bin + PNG textures)"
    }
}

local cur_model_idx = 1
local cur_model = nil
local model_info = {}
local textures = {}
local cur_tex_idx = 1
local tex_list = {}

local shading_modes = {"flat", "gouraud", "unlit"}
local cur_shading_idx = 2

local auto_rotate = true
local rot_y = 0.0
local timer = 0.0
local show_bounds = true
local show_nodes = true

local cam = {
    yaw = 45.0,
    pitch = -20.0,
    dist = 5.0,
    target_x = 0.0,
    target_y = 0.0,
    target_z = 0.0
}

-- Returns whichever value is a table; if the API returns 6 numbers,
-- wrap them into tables. Handles both calling conventions.
local function normalize_bounds(a, b, c, d, e, f)
    if type(a) == "table" and type(b) == "table" then
        return a, b
    end
    -- 6-number form: min.x, min.y, min.z, max.x, max.y, max.z
    if type(a) == "number" and type(d) == "number" then
        return {a, b, c}, {d, e, f}
    end
    return {0, 0, 0}, {0, 0, 0}
end

local function inspect_model(model)
    if not model or not model:isValid() then
        return { valid = false }
    end

    local a, b, c, d, e, f = model:getBounds()
    local bmin, bmax = normalize_bounds(a, b, c, d, e, f)
    local cx, cy, cz = model:getCenter()
    local sx, sy, sz = model:getSize()
    local node_count = model:getNodeCount()
    local part_count = model:getPartCount()

    local nodes = {}
    for i = 0, node_count - 1 do
        local n = model:getNode(i)
        if n then
            table.insert(nodes, {
                index = n.index,
                name = (n.name and n.name ~= "") and n.name or ("Node_" .. tostring(i)),
                pos = { n.x or 0, n.y or 0, n.z or 0 }
            })
        end
    end

    local parts = {}
    for i = 0, part_count - 1 do
        local pname, matname = model:getPartName(i)
        table.insert(parts, {
            index = i,
            name = (pname and pname ~= "") and pname or ("Part_" .. tostring(i)),
            material = matname or "",
            texture = model:getPartTexture(i)
        })
    end

    return {
        valid = true,
        node_count = node_count,
        part_count = part_count,
        bounds_min = bmin,
        bounds_max = bmax,
        center = { cx, cy, cz },
        size = { sx, sy, sz },
        nodes = nodes,
        parts = parts
    }
end

local function load_current_model()
    local info = model_files[cur_model_idx]
    cur_model = crayon.graphics.loadModel(info.path)
    model_info = inspect_model(cur_model)

    if model_info.valid then
        local max_dim = math.max(model_info.size[1], math.max(model_info.size[2], model_info.size[3]))
        cam.dist = math.max(3.5, max_dim * 2.8)
        cam.target_x = model_info.center[1]
        cam.target_y = model_info.center[2]
        cam.target_z = model_info.center[3]
    end
end

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("18 - 3D glTF/GLB Inspector [TAB: Model, 1: Shading, 2: Texture, B: Bounds]")

    textures.none  = 0
    textures.crate = crayon.graphics.loadTexture("game/assets/textures/crate.bmp")
    textures.brick = crayon.graphics.loadTexture("game/assets/textures/brick.bmp")
    textures.grass = crayon.graphics.loadTexture("game/assets/textures/grass.bmp")
    tex_list = {
        { name = "Default (glTF Mat)", id = 0 },
        { name = "Crate Override",    id = textures.crate },
        { name = "Brick Override",    id = textures.brick },
        { name = "Grass Override",    id = textures.grass }
    }

    load_current_model()

    crayon.graphics.setRetroEffects({
        jitterResolution = {240, 160},
        affine = 1.0,
        dither = true
    })

    crayon.graphics.setLight(
        -0.5, -0.8, -0.5,
        1.0, 0.95, 0.88,
        0.30, 0.30, 0.38
    )
end

function crayon.update(dt)
    timer = timer + dt

    -- Toggle model (TAB or Left/Right Arrow)
    if crayon.input.isPressed("tab") or crayon.input.isPressed("right") then
        cur_model_idx = (cur_model_idx % #model_files) + 1
        cur_tex_idx = 1
        load_current_model()
    elseif crayon.input.isPressed("left") then
        cur_model_idx = cur_model_idx - 1
        if cur_model_idx < 1 then cur_model_idx = #model_files end
        cur_tex_idx = 1
        load_current_model()
    end

    -- Shading mode toggle (1)
    if crayon.input.isPressed("1") then
        cur_shading_idx = (cur_shading_idx % #shading_modes) + 1
        crayon.graphics.setShadingMode(shading_modes[cur_shading_idx])
    end

    -- Texture override toggle (2)
    if crayon.input.isPressed("2") then
        cur_tex_idx = (cur_tex_idx % #tex_list) + 1
        if cur_model and cur_model:isValid() then
            local override_id = tex_list[cur_tex_idx].id
            for p = 0, cur_model:getPartCount() - 1 do
                cur_model:setPartTexture(p, override_id)
            end
        end
    end

    if crayon.input.isPressed("b") then
        show_bounds = not show_bounds
    end

    if crayon.input.isPressed("n") then
        show_nodes = not show_nodes
    end

    if crayon.input.isPressed("space") then
        auto_rotate = not auto_rotate
    end

    if auto_rotate then
        rot_y = rot_y + 40.0 * dt
    end

    -- Manual turntable drag (Mouse Drag)
    if crayon.input.isMouseDown("left") then
        local dx, dy = crayon.input.getMouseDelta()
        cam.yaw   = cam.yaw   + dx * 0.5
        cam.pitch = math.max(-85.0, math.min(85.0, cam.pitch - dy * 0.5))
    end

    -- Mouse wheel zoom
    local _, wheel_y = crayon.input.getMouseWheel()
    if wheel_y ~= 0 then
        cam.dist = math.max(1.5, math.min(25.0, cam.dist - wheel_y * 0.5))
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.12)

    local rad_yaw   = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local cx = cam.target_x + cam.dist * math.cos(rad_pitch) * math.sin(rad_yaw)
    local cy = cam.target_y - cam.dist * math.sin(rad_pitch)
    local cz = cam.target_z + cam.dist * math.cos(rad_pitch) * math.cos(rad_yaw)

    crayon.graphics.setCamera3d({
        position = {cx, cy, cz},
        target   = {cam.target_x, cam.target_y, cam.target_z},
        up       = {0, 1, 0},
        fov      = 60.0
    })

    -- 3D Coordinate Grid
    for i = -5, 5 do
        local col = (i == 0) and {0.4, 0.4, 0.5, 0.6} or {0.18, 0.20, 0.26, 0.4}
        crayon.graphics.setColor(col[1], col[2], col[3], col[4])
        crayon.graphics.drawLine3d(i, -0.01, -5, i, -0.01, 5)
        crayon.graphics.drawLine3d(-5, -0.01, i, 5, -0.01, i)
    end

    crayon.graphics.drawAxes3d(0, 0, 0, 1.0)

    -- Render the glTF / GLB Model
    if cur_model and cur_model:isValid() then
        crayon.graphics.pushMatrix()
        crayon.graphics.translate(cam.target_x, cam.target_y, cam.target_z)
        crayon.graphics.rotate(rot_y, 0, 1, 0)
        crayon.graphics.translate(-cam.target_x, -cam.target_y, -cam.target_z)

        local m_info = model_files[cur_model_idx]
        crayon.graphics.scale(m_info.scale, m_info.scale, m_info.scale)

        crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
        crayon.graphics.drawModel(cur_model)

        if show_bounds and model_info.valid then
            local bmin = model_info.bounds_min
            local bmax = model_info.bounds_max
            local w = bmax[1] - bmin[1]
            local h = bmax[2] - bmin[2]
            local d = bmax[3] - bmin[3]
            local bcx = (bmin[1] + bmax[1]) * 0.5
            local bcy = (bmin[2] + bmax[2]) * 0.5
            local bcz = (bmin[3] + bmax[3]) * 0.5

            crayon.graphics.setColor(0.2, 0.9, 0.4, 0.8)
            crayon.graphics.drawCubeWires(bcx, bcy, bcz, w, h, d)
        end

        crayon.graphics.popMatrix()
    else
        crayon.graphics.setColor(1.0, 0.2, 0.2, 1.0)
        crayon.graphics.drawCube(0, 0, 0, 1.0, 1.0, 1.0)
    end

    -- ==================================================================
    -- 2D Overlay HUD
    -- ==================================================================
    crayon.graphics.setColor(0.04, 0.05, 0.08, 0.85)
    crayon.graphics.drawRect("fill", 6, 6, 308, 38)
    crayon.graphics.setColor(0.3, 0.5, 0.8, 0.8)
    crayon.graphics.drawRect("line", 6, 6, 308, 38)

    local m_entry = model_files[cur_model_idx]
    crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.drawText(string.format("[%d/%d] %s", cur_model_idx, #model_files, m_entry.name), 12, 11)
    crayon.graphics.setColor(0.65, 0.70, 0.80, 1.0)
    crayon.graphics.drawText(m_entry.desc, 12, 23)

    crayon.graphics.setColor(0.04, 0.05, 0.08, 0.85)
    crayon.graphics.drawRect("fill", 6, 172, 308, 62)
    crayon.graphics.setColor(0.3, 0.4, 0.6, 0.7)
    crayon.graphics.drawRect("line", 6, 172, 308, 62)

    if model_info.valid then
        crayon.graphics.setColor(0.4, 0.9, 0.5, 1.0)
        crayon.graphics.drawText(string.format("Nodes: %d | Parts: %d | Shading: %s",
            model_info.node_count, model_info.part_count, shading_modes[cur_shading_idx]), 12, 176)

        crayon.graphics.setColor(0.75, 0.75, 0.85, 1.0)
        crayon.graphics.drawText(string.format("Size: [%.2f, %.2f, %.2f]",
            model_info.size[1], model_info.size[2], model_info.size[3]), 12, 188)

        crayon.graphics.drawText(string.format("Texture: %s (Key 2)", tex_list[cur_tex_idx].name), 12, 200)
    else
        crayon.graphics.setColor(1.0, 0.3, 0.3, 1.0)
        crayon.graphics.drawText("Failed to load model file!", 12, 176)
    end

    crayon.graphics.setColor(0.5, 0.6, 0.7, 0.9)
    crayon.graphics.drawText("TAB: Next | Drag: Orbit | Wheel: Zoom | Space: Spin", 12, 216)
end