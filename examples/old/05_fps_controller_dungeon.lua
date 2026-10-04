-- ============================================================================
-- Example 05: 3D FPS Dungeon with Relative Mouse Look & Torch Light
-- Crayon Engine
-- ============================================================================

local cam = {
    x = 0.0,
    y = 0.8,
    z = 1.5,
    yaw = 0.0,      -- degrees
    pitch = 0.0,    -- degrees
    fov = 65.0
}

local mouse_captured = true
local textures = {}
local timer = 0
local score = 0

-- Player collision radius (in world units, map cells are 2x2)
local PLAYER_RADIUS = 0.3

-- Pickups placed in dungeon (first one directly in line of sight!)
local coins = {
    {x =  0, z = -1.5, active = true},
    {x = -3, z = -3.0, active = true},
    {x =  3, z = -3.0, active = true},
    {x = -3, z =  1.0, active = true},
    {x =  3, z =  1.0, active = true},
    {x =  0, z = -5.0, active = true}
}

-- Dungeon map layout (1 = wall, 0 = empty hall)
local map_w = 9
local map_h = 9
local map = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 1, 0, 0, 0, 1, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1
}

-- ---------------------------------------------------------------------------
-- World ↔ grid helpers. Walls are 2x2 cubes centered at (cx-1)*2-8.
-- So a world X in [-9, -7] maps to cell 1, [-7, -5] to cell 2, etc.
-- ---------------------------------------------------------------------------
local function cell_from_world(world_x, world_z)
    local cx = math.floor((world_x + 9) / 2) + 1
    local cz = math.floor((world_z + 9) / 2) + 1
    return cx, cz
end

local function is_wall_at(world_x, world_z)
    local cx, cz = cell_from_world(world_x, world_z)
    -- Treat anything outside the map as solid
    if cx < 1 or cx > map_w or cz < 1 or cz > map_h then
        return true
    end
    return map[(cz - 1) * map_w + cx] == 1
end

-- AABB check: sample the four corners of the player's footprint
local function can_stand_at(x, z)
    local r = PLAYER_RADIUS
    return not is_wall_at(x - r, z - r)
       and not is_wall_at(x + r, z - r)
       and not is_wall_at(x - r, z + r)
       and not is_wall_at(x + r, z + r)
end

-- ---------------------------------------------------------------------------
-- Lifecycle
-- ---------------------------------------------------------------------------
function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("05 - FPS Dungeon [WASD: Move, Mouse: Look, M: Mouse Lock]")

    textures.crate = crayon.graphics.loadTexture("game/assets/textures/crate.bmp")
    textures.brick = crayon.graphics.loadTexture("game/assets/textures/brick.bmp")
    textures.coin  = crayon.graphics.loadTexture("game/assets/textures/coin.bmp")

    crayon.graphics.setRetroEffects({
        jitterResolution = {160, 120},
        affine = 1.0,
        dither = true,
        fog = { startDist = 4.0, endDist = 18.0, color = {0.04, 0.04, 0.07} }
    })

    -- Low dark ambient light for moody dungeon atmosphere
    crayon.graphics.setLight(
        -0.2, -1.0, -0.2,
        0.2, 0.2, 0.25,      -- Dim directional
        0.1, 0.1, 0.15       -- Ambient
    )

    -- Enable torch point light
    crayon.graphics.setPointLightEnabled(0, true)

    -- Lock mouse cursor for FPS look
    crayon.window.setMouseRelative(true)
    crayon.window.showCursor(false)
    mouse_captured = true
end

function crayon.update(dt)
    timer = timer + dt

    -- -----------------------------------------------------------------------
    -- Toggle mouse capture with 'M'
    -- -----------------------------------------------------------------------
    if crayon.key.isPressed("m") then
        mouse_captured = not mouse_captured
        crayon.mouse.setRelativeMode(mouse_captured)
        crayon.mouse.setVisible(not mouse_captured)
    end

    -- -----------------------------------------------------------------------
    -- Mouse look
    -- -----------------------------------------------------------------------
    if mouse_captured then
        local mdx, mdy = crayon.mouse.getDelta()
        local sensitivity = 0.22
        cam.yaw = cam.yaw + mdx * sensitivity
        cam.pitch = math.max(-85.0, math.min(85.0, cam.pitch - mdy * sensitivity))
    end

    -- Arrow keys fallback look
    if crayon.key.isDown("left")  then cam.yaw   = cam.yaw - 90.0 * dt end
    if crayon.key.isDown("right") then cam.yaw   = cam.yaw + 90.0 * dt end
    if crayon.key.isDown("up")    then cam.pitch = math.min(85.0, cam.pitch + 70.0 * dt) end
    if crayon.key.isDown("down")  then cam.pitch = math.max(-85.0, cam.pitch - 70.0 * dt) end

    -- Wrap yaw to keep it bounded
    cam.yaw = cam.yaw % 360.0

    -- -----------------------------------------------------------------------
    -- WASD movement
    -- -----------------------------------------------------------------------
    local move_spd = 3.5 * dt
    local rad_yaw  = math.rad(cam.yaw)
    local fwd_x    = math.sin(rad_yaw)
    local fwd_z    = -math.cos(rad_yaw)
    local right_x  = math.cos(rad_yaw)
    local right_z  = math.sin(rad_yaw)

    local move_x, move_z = 0, 0
    if crayon.key.isDown("w") then
        move_x = move_x + fwd_x * move_spd
        move_z = move_z + fwd_z * move_spd
    end
    if crayon.key.isDown("s") then
        move_x = move_x - fwd_x * move_spd
        move_z = move_z - fwd_z * move_spd
    end
    if crayon.key.isDown("a") then
        move_x = move_x - right_x * move_spd
        move_z = move_z - right_z * move_spd
    end
    if crayon.key.isDown("d") then
        move_x = move_x + right_x * move_spd
        move_z = move_z + right_z * move_spd
    end

    -- -----------------------------------------------------------------------
    -- Per-axis swept collision (allows sliding along walls)
    -- -----------------------------------------------------------------------
    local try_x = cam.x + move_x
    if can_stand_at(try_x, cam.z) then
        cam.x = try_x
    end
    local try_z = cam.z + move_z
    if can_stand_at(cam.x, try_z) then
        cam.z = try_z
    end

    -- -----------------------------------------------------------------------
    -- Dynamic flickering torch light
    -- -----------------------------------------------------------------------
    local flicker = math.sin(timer * 15.0) * 0.05 + math.cos(timer * 23.0) * 0.04
    crayon.graphics.setPointLight(
        0,
        cam.x, cam.y, cam.z,
        1.0, 0.75 + flicker, 0.4,   -- Warm firelight
        6.5 + flicker * 2.0,         -- Radius
        3.2 + flicker                -- Intensity
    )

    -- -----------------------------------------------------------------------
    -- Pickup collection (squared distance — no sqrt per coin per frame)
    -- -----------------------------------------------------------------------
    local pickup_r2 = 0.8 * 0.8
    for _, c in ipairs(coins) do
        if c.active then
            local dx = cam.x - c.x
            local dz = cam.z - c.z
            if dx * dx + dz * dz < pickup_r2 then
                c.active = false
                score = score + 100
            end
        end
    end

    -- -----------------------------------------------------------------------
    -- Escape: release mouse first, then quit on second press
    -- -----------------------------------------------------------------------
    if crayon.key.isPressed("escape") then
        if mouse_captured then
            mouse_captured = false
            crayon.mouse.setRelativeMode(false)
            crayon.mouse.setVisible(true)
        else
            crayon.window.quit()
        end
    end
end

function crayon.draw()
    crayon.graphics.clear(0.04, 0.04, 0.07)

    -- -----------------------------------------------------------------------
    -- 3D world
    -- -----------------------------------------------------------------------
    local rad_yaw   = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local look_x = math.sin(rad_yaw) * math.cos(rad_pitch)
    local look_y = math.sin(rad_pitch)
    local look_z = -math.cos(rad_yaw) * math.cos(rad_pitch)

    crayon.graphics.setCamera3D({
        position = {cam.x, cam.y, cam.z},
        target   = {cam.x + look_x, cam.y + look_y, cam.z + look_z},
        up       = {0, 1, 0},
        fov      = cam.fov,
        near     = 0.1,
        far      = 40.0
    })

    -- Floor (up-facing) and Ceiling (rotated 180° around X so normal faces down)
    crayon.graphics.setColor(0.5, 0.5, 0.55, 1.0)
    crayon.graphics.drawPlane(0, 0.0, 0, 20, 20, textures.brick, 0, 0, 0)
    crayon.graphics.drawPlane(0, 2.0, 0, 20, 20, textures.brick, math.pi, 0, 0)

    -- Dungeon walls
    for cz = 1, map_h do
        for cx = 1, map_w do
            local idx = (cz - 1) * map_w + cx
            if map[idx] == 1 then
                local wx = (cx - 1) * 2 - 8
                local wz = (cz - 1) * 2 - 8
                crayon.graphics.setColor(0.8, 0.8, 0.85, 1.0)
                crayon.graphics.drawCube(wx, 1.0, wz, 2.0, 2.0, 2.0, textures.brick, 0, 0, 0)
            end
        end
    end

    -- Animated floating coin pickups (billboards)
    for _, c in ipairs(coins) do
        if c.active then
            local float_y = 0.7 + math.sin(timer * 4.0) * 0.1
            crayon.graphics.setColor(1.0, 0.95, 0.3, 1.0)
            crayon.graphics.drawBillboard(c.x, float_y, c.z, 0.8, 0.8, textures.coin, "spherical")
        end
    end

    -- -----------------------------------------------------------------------
    -- 2D HUD overlay — disable depth so HUD never gets occluded by walls
    -- -----------------------------------------------------------------------
    crayon.graphics.setDepthTest(false)

    -- Crosshair
    local hcx, hcy = 160, 120
    crayon.graphics.setColor(1.0, 1.0, 1.0, 0.7)
    crayon.graphics.drawLine(hcx - 5, hcy, hcx + 5, hcy, 1.0)
    crayon.graphics.drawLine(hcx, hcy - 5, hcx, hcy + 5, 1.0)

    -- Top header panel
    crayon.graphics.setColor(0.06, 0.08, 0.12, 0.85)
    crayon.graphics.drawRect("fill", 4, 4, 312, 22)
    crayon.graphics.setColor(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.drawRect("line", 4, 4, 312, 22)

    crayon.graphics.setColor(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.drawText("3D FPS DUNGEON CRAWLER", 8, 10, 1.0)

    crayon.graphics.setColor(0.4, 1.0, 0.5, 1.0)
    crayon.graphics.drawText("SCORE: " .. score, 235, 10, 1.0)

    -- Bottom controls & status
    crayon.graphics.setColor(0.06, 0.08, 0.12, 0.85)
    crayon.graphics.drawRect("fill", 4, 218, 312, 18)
    crayon.graphics.setColor(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.drawRect("line", 4, 218, 312, 18)

    crayon.graphics.setColor(0.85, 0.9, 0.95, 1.0)
    local status = mouse_captured and "Mouse: LOCKED (FPS Look)" or "Mouse: UNLOCKED"
    crayon.graphics.drawText(status .. " | [M]: Toggle | [ESC]: Quit", 8, 222, 1.0)

    -- Restore depth test for the next frame's 3D pass
    crayon.graphics.setDepthTest(true)
end