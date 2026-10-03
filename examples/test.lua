-- Draggable box screenpet with free rotation.
-- Grab anywhere on the box; drag and the box rotates so the grab point
-- tracks the cursor exactly. Release to throw.

function crayon.config(t)
    t.window.transparent   = true
    t.window.borderless    = true
    t.window.clickThrough  = true
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
    t.modules.physics3d    = false
    t.modules.physics2d    = false
end

-- Window / drawing constants
local WIN_W, WIN_H = 140, 140
local CX, CY       = WIN_W * 0.5, WIN_H * 0.5

-- Pet physics
local BOX_HALF    = 26       -- half-size of the square
local GRAVITY     = 2200
local RESTITUTION = 0.28
local LIN_DAMP    = 0.6      -- linear velocity decay (1/s)
local ANG_DAMP    = 1.8      -- angular velocity decay (1/s)
local MASS        = 1.0
-- Moment of inertia for a uniform square of half-side h: I = m*(2h)²/6
local INERTIA     = MASS * (2*BOX_HALF) * (2*BOX_HALF) / 6.0

-- State
local pet    = { x = 0, y = 0, vx = 0, vy = 0, angle = 0, omega = 0 }
local bounds = { x0 = 0, y0 = 0, x1 = 0, y1 = 0 }

local drag = {
    active = false,
    local_x = 0, local_y = 0, local_angle = 0,
    target_x = 0, target_y = 0,
    prev_x = 0, prev_y = 0, prev_angle = 0,
    smooth_vx = 0, smooth_vy = 0, smooth_omega = 0,
}

local CORNERS = {
    {-BOX_HALF, -BOX_HALF},
    { BOX_HALF, -BOX_HALF},
    { BOX_HALF,  BOX_HALF},
    {-BOX_HALF,  BOX_HALF},
}

local function atan2(y, x)
    if math.atan2 then return math.atan2(y, x) end
    return math.atan(y, x)
end

local function sync_window()
    crayon.window.setPosition(
        math.floor(pet.x - WIN_W * 0.5),
        math.floor(pet.y - WIN_H * 0.5))
end

local function cursor_global()
    return crayon.mouse.getGlobalPosition()
end

local function point_in_box(gx, gy, padding)
    padding = padding or 0
    local dx, dy = gx - pet.x, gy - pet.y
    local c, s = math.cos(pet.angle), math.sin(pet.angle)
    local lx = dx * c + dy * s
    local ly = -dx * s + dy * c
    return math.abs(lx) <= BOX_HALF + padding and math.abs(ly) <= BOX_HALF + padding
end

-- ============================================================================
-- Collision — one contact point, impulse-based
-- ============================================================================

local function resolve_contact(wx, wy, nx, ny, penetration)
    -- Push body out along the normal
    pet.x = pet.x + nx * penetration
    pet.y = pet.y + ny * penetration

    local rx = wx - pet.x
    local ry = wy - pet.y

    -- Velocity at the contact point: v + ω × r
    local vcx = pet.vx - pet.omega * ry
    local vcy = pet.vy + pet.omega * rx

    -- Normal velocity. Negative means still moving into the wall.
    local vn = vcx * nx + vcy * ny
    if vn >= 0 then return end

    -- Effective inverse mass at the contact along the normal
    local rxn = rx * ny - ry * nx                       -- r × n (scalar)
    local inv_eff = 1.0 / MASS + (rxn * rxn) / INERTIA

    local j = -(1 + RESTITUTION) * vn / inv_eff

    -- Linear impulse J = j * n
    pet.vx = pet.vx + j * nx / MASS
    pet.vy = pet.vy + j * ny / MASS

    -- Angular impulse: torque = r × J
    pet.omega = pet.omega + j * rxn / INERTIA
end

local function collide()
    -- Iterate so opposite corners settle in the same frame
    for _ = 1, 4 do
        local c, s = math.cos(pet.angle), math.sin(pet.angle)
        local hit = false
        for _, corner in ipairs(CORNERS) do
            local lx, ly = corner[1], corner[2]
            local wx = pet.x + lx * c - ly * s
            local wy = pet.y + lx * s + ly * c

            if wy > bounds.y1 then
                resolve_contact(wx, wy, 0, -1, wy - bounds.y1); hit = true
            end
            if wy < bounds.y0 then
                resolve_contact(wx, wy, 0, 1, bounds.y0 - wy); hit = true
            end
            if wx < bounds.x0 then
                resolve_contact(wx, wy, 1, 0, bounds.x0 - wx); hit = true
            end
            if wx > bounds.x1 then
                resolve_contact(wx, wy, -1, 0, wx - bounds.x1); hit = true
            end
        end
        if not hit then break end
    end
end

-- ============================================================================
-- Drag — grab point glued to cursor
-- ============================================================================

local function update_drag(dt)
    if not drag.active then return end

    local dx = drag.target_x - pet.x
    local dy = drag.target_y - pet.y
    local dist = math.sqrt(dx * dx + dy * dy)

    -- If the cursor is essentially on the box center, keep the current angle
    local new_angle
    if dist < 0.5 then
        new_angle = pet.angle
    else
        -- We want: angle_of(world_grab_point) == angle_of(cursor - center)
        -- angle_of(world_grab_point) = angle + angle_of(local_grab_point)
        -- So: angle = cursor_angle - local_angle
        new_angle = atan2(dy, dx) - drag.local_angle
    end

    -- Rotate the local grab offset by the new angle to find the world offset
    local c, s = math.cos(new_angle), math.sin(new_angle)
    local gpx = drag.local_x * c - drag.local_y * s
    local gpy = drag.local_x * s + drag.local_y * c

    -- Place the center so the grab point lands on the cursor
    local new_x = drag.target_x - gpx
    local new_y = drag.target_y - gpy

    -- Measure instantaneous velocity for throw-on-release (smoothed)
    if dt > 0 then
        local alpha = 1 - math.exp(-25 * dt)
        local ivx = (new_x - drag.prev_x) / dt
        local ivy = (new_y - drag.prev_y) / dt
        local ivo = (new_angle - drag.prev_angle) / dt
        drag.smooth_vx    = drag.smooth_vx    * (1 - alpha) + ivx * alpha
        drag.smooth_vy    = drag.smooth_vy    * (1 - alpha) + ivy * alpha
        drag.smooth_omega = drag.smooth_omega * (1 - alpha) + ivo * alpha
    end

    drag.prev_x, drag.prev_y, drag.prev_angle = new_x, new_y, new_angle
    pet.x, pet.y, pet.angle = new_x, new_y, new_angle
end

-- ============================================================================
-- Lifecycle
-- ============================================================================

function crayon.init()
    local ux, uy, uw, uh = crayon.window.getUsableBounds()
    bounds.x0, bounds.y0 = ux, uy
    bounds.x1, bounds.y1 = ux + uw, uy + uh

    pet.x = ux + uw * 0.5
    pet.y = uy + uh * 0.35
    pet.angle = 0.35  -- a slight tilt so it looks alive on spawn

    sync_window()
end

function crayon.update(dt)
    if drag.active then
        -- Poll global cursor each frame (window follows, so window-local drifts)
        drag.target_x, drag.target_y = cursor_global()
        update_drag(dt)
    else
        -- Gravity + integration
        pet.vy = pet.vy + GRAVITY * dt
        pet.x = pet.x + pet.vx * dt
        pet.y = pet.y + pet.vy * dt
        pet.angle = pet.angle + pet.omega * dt

        -- Damping
        local ld = math.exp(-LIN_DAMP * dt)
        pet.vx = pet.vx * ld
        pet.vy = pet.vy * ld
        pet.omega = pet.omega * math.exp(-ANG_DAMP * dt)
    end

    collide()
    sync_window()

    -- Click-through: OFF while dragging (we need the events),
    -- otherwise ON unless the cursor is on the box.
    if drag.active then
        crayon.window.setClickThrough(false)
    else
        local gx, gy = cursor_global()
        crayon.window.setClickThrough(not point_in_box(gx, gy, 2))
    end
end

function crayon.mousedown(x, y, button)
    if button ~= 1 then return end

    local wx, wy = crayon.window.getPosition()
    local gx, gy = wx + x, wy + y
    if not point_in_box(gx, gy, 2) then return end

    -- Convert cursor into box-local space
    local dx, dy = gx - pet.x, gy - pet.y
    local c, s = math.cos(pet.angle), math.sin(pet.angle)
    local lx = dx * c + dy * s
    local ly = -dx * s + dy * c

    drag.active = true
    drag.local_x = lx
    drag.local_y = ly
    drag.local_angle = atan2(ly, lx)
    drag.target_x, drag.target_y = gx, gy
    drag.prev_x, drag.prev_y, drag.prev_angle = pet.x, pet.y, pet.angle
    drag.smooth_vx, drag.smooth_vy, drag.smooth_omega = 0, 0, 0
end

function crayon.mouseup(x, y, button)
    if button ~= 1 then return end
    if not drag.active then return end

    drag.active = false

    -- Throw with the last smoothed velocity, clamped so it stays sane
    local max_v = 1800
    local max_w = 8
    pet.vx    = math.max(-max_v, math.min(max_v, drag.smooth_vx))
    pet.vy    = math.max(-max_v, math.min(max_v, drag.smooth_vy))
    pet.omega = math.max(-max_w, math.min(max_w, drag.smooth_omega))
end

-- ============================================================================
-- Rendering — manual rotation (no matrix stack dependency)
-- ============================================================================

-- Box-local → window-local
local function w(lx, ly)
    local c, s = math.cos(pet.angle), math.sin(pet.angle)
    return CX + lx * c - ly * s, CY + lx * s + ly * c
end

function crayon.draw()
    local h = BOX_HALF

    -- Shadow corners (offset in box-local space then rotated with body)
    local s1x, s1y = w(-h + 3, -h + 5)
    local s2x, s2y = w( h + 3, -h + 5)
    local s3x, s3y = w( h + 3,  h + 5)
    local s4x, s4y = w(-h + 3,  h + 5)
    crayon.graphics.setColor(0, 0, 0, 0.22)
    crayon.graphics.drawQuad("fill", s1x, s1y, s2x, s2y, s3x, s3y, s4x, s4y)

    -- Body
    local c1x, c1y = w(-h, -h)
    local c2x, c2y = w( h, -h)
    local c3x, c3y = w( h,  h)
    local c4x, c4y = w(-h,  h)
    crayon.graphics.setColor(1.0, 0.48, 0.22, 1.0)
    crayon.graphics.drawQuad("fill", c1x, c1y, c2x, c2y, c3x, c3y, c4x, c4y)

    -- Top highlight strip
    local h1x, h1y = w(-h + 3, -h + 3)
    local h2x, h2y = w( h - 3, -h + 3)
    local h3x, h3y = w( h - 3, -h + 9)
    local h4x, h4y = w(-h + 3, -h + 9)
    crayon.graphics.setColor(1.0, 0.68, 0.42, 1.0)
    crayon.graphics.drawQuad("fill", h1x, h1y, h2x, h2y, h3x, h3y, h4x, h4y)

    -- Outline
    crayon.graphics.setColor(0.55, 0.15, 0.05, 1.0)
    crayon.graphics.drawLine(c1x, c1y, c2x, c2y, 2)
    crayon.graphics.drawLine(c2x, c2y, c3x, c3y, 2)
    crayon.graphics.drawLine(c3x, c3y, c4x, c4y, 2)
    crayon.graphics.drawLine(c4x, c4y, c1x, c1y, 2)

    -- Eyes track the cursor (in box-local space so they rotate with the body)
    local gx, gy = cursor_global()
    local edx, edy = gx - pet.x, gy - pet.y
    local ec, es = math.cos(-pet.angle), math.sin(-pet.angle)
    local ldx = edx * ec - edy * es
    local ldy = edx * es + edy * ec
    local elen = math.sqrt(ldx * ldx + ldy * ldy)
    local ox, oy = 0, 0
    if elen > 1 then
        ox = (ldx / elen) * 2.5
        oy = (ldy / elen) * 2.5
    end

    local ex1, ey1 = w(-9 + ox, -3 + oy)
    local ex2, ey2 = w( 9 + ox, -3 + oy)
    crayon.graphics.setColor(0.05, 0.05, 0.10, 1.0)
    crayon.graphics.drawCircle("fill", ex1, ey1, 4)
    crayon.graphics.drawCircle("fill", ex2, ey2, 4)

    -- Glints
    local g1x, g1y = w(-10 + ox, -4 + oy)
    local g2x, g2y = w( 8 + ox, -4 + oy)
    crayon.graphics.setColor(1, 1, 1, 0.9)
    crayon.graphics.drawCircle("fill", g1x, g1y, 1.5)
    crayon.graphics.drawCircle("fill", g2x, g2y, 1.5)

    -- Mouth
    local m1x, m1y = w(-5, 8)
    local m2x, m2y = w( 0, 10)
    local m3x, m3y = w( 5, 8)
    crayon.graphics.setColor(0.55, 0.15, 0.05, 1.0)
    crayon.graphics.drawLine(m1x, m1y, m2x, m2y, 1.5)
    crayon.graphics.drawLine(m2x, m2y, m3x, m3y, 1.5)
end