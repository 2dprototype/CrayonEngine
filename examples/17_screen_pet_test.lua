-- Small floating-window screenpet ball.
-- The window follows the pet's desktop-space position every frame.

function crayon.config(t)
    t.window.transparent   = true
    t.window.borderless    = true
    t.window.clickThrough  = true    -- start fully click-through
    t.window.resizable     = false
    t.window.alwaysOnTop   = true
    t.window.width         = 140
    t.window.height        = 140
    t.window.virtualWidth  = 140
    t.window.virtualHeight = 140
    t.graphics.clearColor  = {0, 0, 0, 0}
    t.modules.mesh3D       = false
    t.modules.audio        = false
    t.modules.particles    = false
    t.modules.physics3D    = false
    t.modules.physics2D    = false    -- hand-rolled integration below
    t.window.opacity       = 1
    t.window.notFocusable  = true
    t.window.utilityWindow = true
end

-- ---- Tunables ----------------------------------------------------------
local WIN_W, WIN_H   = 140, 140
local PET_R          = 30
local GRAVITY_Y      = 2400     -- px/s², +y is down
local JUMP_VEL       = -1050    -- px/s (negative = up)
local MAX_H_SPEED    = 700      -- px/s
local CHASE_BLEND    = 6.0      -- 1/s, higher = snappier chase
local RESTITUTION    = 0.5      -- bounciness off walls
local GROUND_VY_TOL  = 60       -- |vy| below this when touching floor = resting

-- ---- State -------------------------------------------------------------
local pet    = { x = 0, y = 0, vx = 0, vy = 0 }
local bounds = { x0 = 0, y0 = 0, x1 = 0, y1 = 0 }

local function sync_window()
    crayon.window.setPosition(
        math.floor(pet.x - WIN_W * 0.5),
        math.floor(pet.y - WIN_H * 0.5))
end

local function cursor_global()
    return crayon.mouse.getGlobalPosition()
end

local function cursor_over_pet()
    local gx, gy = cursor_global()
    local dx, dy = gx - pet.x, gy - pet.y
    local r = PET_R + 6
    return (dx * dx + dy * dy) <= r * r
end

local function grounded()
    return pet.y + PET_R >= bounds.y1 - 1.5 and math.abs(pet.vy) < GROUND_VY_TOL
end

-- ---- Integration -------------------------------------------------------
local function integrate(dt)
    -- Gravity
    pet.vy = pet.vy + GRAVITY_Y * dt

    -- Horizontal steering toward cursor (blended for smoothness)
    local gx = cursor_global()
    local dx = gx - pet.x
    local target_vx = 0
    if math.abs(dx) > 4 then
        target_vx = math.max(-MAX_H_SPEED, math.min(MAX_H_SPEED, dx * 4))
    end
    local blend = 1.0 - math.exp(-CHASE_BLEND * dt)
    pet.vx = pet.vx + (target_vx - pet.vx) * blend

    -- Integrate positions
    pet.x = pet.x + pet.vx * dt
    pet.y = pet.y + pet.vy * dt

    -- Wall collisions in desktop space
    if pet.x - PET_R < bounds.x0 then
        pet.x = bounds.x0 + PET_R
        pet.vx = -pet.vx * RESTITUTION
    end
    if pet.x + PET_R > bounds.x1 then
        pet.x = bounds.x1 - PET_R
        pet.vx = -pet.vx * RESTITUTION
    end
    if pet.y - PET_R < bounds.y0 then
        pet.y = bounds.y0 + PET_R
        pet.vy = -pet.vy * RESTITUTION
    end
    if pet.y + PET_R > bounds.y1 then
        pet.y = bounds.y1 - PET_R
        if math.abs(pet.vy) < GROUND_VY_TOL then
            pet.vy = 0
        else
            pet.vy = -pet.vy * RESTITUTION
        end
    end
end

-- ---- Lifecycle ---------------------------------------------------------
function crayon.init()
    -- Window stays at the 140x140 size from config. Do NOT resize it.

    -- Full desktop (includes taskbar area) -- only used for spawn point
    local vx, vy, vw, vh = crayon.window.getVirtualDesktopBounds()

    -- Usable area (excludes taskbar / dock / panels)
    local ux, uy, uw, uh = crayon.window.getUsableBounds()

    -- Physics bounds = usable area, not full desktop
    bounds.x0, bounds.y0 = ux, uy
    bounds.x1, bounds.y1 = ux + uw, uy + uh

    -- Start the pet horizontally centered, a third down the screen
    pet.x = ux + uw * 0.5
    pet.y = uy + uh * 0.33

    sync_window()
end

function crayon.update(dt)
    integrate(dt)
    sync_window()

    -- Click-through only when cursor is off the pet
    crayon.window.setClickThrough(not cursor_over_pet())
end

function crayon.mousedown(x, y, button)
    if button ~= 1 then return end

    -- Convert window-local to desktop coords for the hit test
    local wx, wy = crayon.window.getPosition()
    local dx = (wx + x) - pet.x
    local dy = (wy + y) - pet.y
    if (dx * dx + dy * dy) > (PET_R + 6) ^ 2 then return end

    if not grounded() then return end
    pet.vy = JUMP_VEL
end

function crayon.keydown(key, is_repeat)
    if key ~= "space" or is_repeat then return end
    if not grounded() then return end
    pet.vy = JUMP_VEL
end

-- ---- Rendering ---------------------------------------------------------
function crayon.draw()
    -- Always draw the pet at the window's center
    local cx = WIN_W * 0.5
    local cy = WIN_H * 0.5

    -- Soft outer glow
    crayon.graphics.setColor(1.0, 0.45, 0.15, 0.10)
    crayon.graphics.drawCircle("fill", cx, cy, PET_R + 14)
    crayon.graphics.setColor(1.0, 0.45, 0.15, 0.20)
    crayon.graphics.drawCircle("fill", cx, cy, PET_R + 6)

    -- Body
    crayon.graphics.setColor(1.0, 0.35, 0.15, 1.0)
    crayon.graphics.drawCircle("fill", cx, cy, PET_R)

    -- Inner disc for shading
    crayon.graphics.setColor(1.0, 0.55, 0.30, 1.0)
    crayon.graphics.drawCircle("fill", cx - 4, cy - 4, PET_R - 5)

    -- Specular highlight
    crayon.graphics.setColor(1.0, 0.95, 0.85, 0.9)
    crayon.graphics.drawCircle("fill", cx - 9, cy - 10, 5)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawCircle("fill", cx - 9, cy - 10, 2)

    -- Eyes track the global cursor
    local gx, gy = cursor_global()
    local edx, edy = gx - pet.x, gy - pet.y
    local elen = math.sqrt(edx * edx + edy * edy)
    local lx, ly = 0, 0
    if elen > 1 then
        lx = (edx / elen) * 3
        ly = (edy / elen) * 3
    end

    local eye_dx = 7
    local eye_y  = -1
    crayon.graphics.setColor(0.05, 0.05, 0.10, 1.0)
    crayon.graphics.drawCircle("fill", cx - eye_dx + lx, cy + eye_y + ly, 3)
    crayon.graphics.drawCircle("fill", cx + eye_dx + lx, cy + eye_y + ly, 3)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawCircle("fill", cx - eye_dx + lx - 1, cy + eye_y + ly - 1, 1)
    crayon.graphics.drawCircle("fill", cx + eye_dx + lx - 1, cy + eye_y + ly - 1, 1)
end