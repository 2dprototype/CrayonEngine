-- Smooth click-to-jump screenpet ball
-- Motion model: velocity steering (not forces). Feels snappy, no jitter.

function crayon.config(t)
    -- t.window.transparent  = true
    -- t.window.borderless   = true
    t.window.clickThrough = false
    t.window.resizable    = false
    t.graphics.clearColor = {0, 0, 0, 0}
    t.modules.mesh3D      = false
    t.modules.physics3d   = false
    t.modules.audio       = false
    t.modules.particles   = false
end

-- Tunables
local PET_RADIUS   = 26
local GRAVITY_Y    = 100      -- m/s^2 (window y is down)
local JUMP_VEL     = -1800    -- px/s upward
local MAX_H_SPEED  = 700      -- px/s
local CHASE_BLEND  = 8.0      -- how quickly velocity approaches target (1/s)
local DEAD_ZONE    = 4        -- px -- don't jitter when nearly aligned
local GROUND_TOL   = 250      -- |vy| below this = considered grounded

local pet
local W, H

function crayon.init()
    local vx, vy, vw, vh = crayon.window.getVirtualDesktopBounds()
    crayon.window.setPosition(vx, vy)
    crayon.window.setWindowSize(vw, vh)
    crayon.window.setResolution(vw, vh)
    crayon.window.setScalingMode("center")
    W, H = vw, vh

    crayon.physics2d.setMeterScale(50)
    crayon.physics2d.setGravity(0, GRAVITY_Y)

    -- Static walls -- thicker than the ball so fast jumps can't tunnel
    local t = 24
    local walls = {
        { -t, -t, vw + 2*t, t  },   -- top
        { -t, vh, vw + 2*t, t  },   -- bottom
        { -t, -t, t, vh + 2*t  },   -- left
        { vw, -t, t, vh + 2*t  },   -- right
    }
    for _, w in ipairs(walls) do
        local b = crayon.physics2d.createBody("static", w[1] + w[3]/2, w[2] + w[4]/2)
        b:addBox(w[3], w[4])
    end

    -- The ball
    pet = crayon.physics2d.createBody("dynamic", vw/2, vh/2, {
        fixedRotation = true,
        linearDamping = 0.0,   -- we control velocity directly
        gravityScale  = 1.0,
        bullet        = true,  -- CCD, prevents tunneling on jumps
        allowSleep    = false, -- always active so chase stays responsive
    })
    pet:addCircle(PET_RADIUS, 0, 0, 1.0, 0.0, 0.1)

    crayon.window.setClickThrough(true)
end

-- Cursor in window-local coords
local function cursor_local()
    local wx, wy = crayon.window.getPosition()
    local gx, gy = crayon.mouse.getGlobalPosition()
    return gx - wx, gy - wy
end

local function is_over_pet()
    local px, py = pet:getPosition()
    local tx, ty = cursor_local()
    local dx, dy = tx - px, ty - py
    local r = PET_RADIUS + 6
    return (dx*dx + dy*dy) <= r*r
end

local function is_grounded()
    local _, vy = pet:getLinearVelocity()
    return math.abs(vy) < GROUND_TOL
end

function crayon.update(dt)
    local px = pet:getPosition()
    local vx, vy = pet:getLinearVelocity()
    local tx = cursor_local()

    -- Horizontal steering: blend velocity toward target
    local dx = tx - px
    local target_vx = 0
    if math.abs(dx) > DEAD_ZONE then
        target_vx = math.max(-MAX_H_SPEED, math.min(MAX_H_SPEED, dx * 5))
    end

    -- frame-rate-independent blend
    local blend = 1.0 - math.exp(-CHASE_BLEND * dt)
    local nvx = vx + (target_vx - vx) * blend

    -- Vertical velocity is untouched here -- gravity owns it,
    -- and jumps are set once in mousedown. We pass current vy back.
    pet:setLinearVelocity(nvx, vy)

    -- Click-through management
    crayon.window.setClickThrough(not is_over_pet())
end

function crayon.mousedown(x, y, button)
    if button ~= 1 then return end

    -- Was the click on the ball?
    local px, py = pet:getPosition()
    local dx, dy = x - px, y - py
    local r = PET_RADIUS + 6
    if (dx*dx + dy*dy) > r*r then return end

    -- Only jump when grounded
    if not is_grounded() then return end

    local vx = pet:getLinearVelocity()
    pet:setLinearVelocity(vx, JUMP_VEL)
end

-- Bonus: space also jumps
function crayon.keydown(key, is_repeat)
    if key ~= "space" or is_repeat then return end
    if not is_grounded() then return end
    local vx = pet:getLinearVelocity()
    pet:setLinearVelocity(vx, JUMP_VEL)
end

function crayon.draw()
    local px, py = pet:getPosition()

    -- Outer soft glow
    crayon.graphics.setColor(1.0, 0.45, 0.15, 0.12)
    crayon.graphics.drawCircle("fill", px, py, PET_RADIUS + 12)
    crayon.graphics.setColor(1.0, 0.45, 0.15, 0.22)
    crayon.graphics.drawCircle("fill", px, py, PET_RADIUS + 5)

    -- Body
    crayon.graphics.setColor(1.0, 0.35, 0.15, 1.0)
    crayon.graphics.drawCircle("fill", px, py, PET_RADIUS)

    -- Lighter inner disc for volume
    crayon.graphics.setColor(1.0, 0.55, 0.30, 1.0)
    crayon.graphics.drawCircle("fill", px - 4, py - 4, PET_RADIUS - 5)

    -- Specular highlight
    crayon.graphics.setColor(1.0, 0.95, 0.85, 0.9)
    crayon.graphics.drawCircle("fill", px - 9, py - 10, 5)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawCircle("fill", px - 9, py - 10, 2)

    -- Eyes track the cursor
    local tx, ty = cursor_local()
    local edx, edy = tx - px, ty - py
    local elen = math.sqrt(edx*edx + edy*edy)
    local lx, ly = 0, 0
    if elen > 1 then
        lx = (edx / elen) * 3
        ly = (edy / elen) * 3
    end

    local eye_dx = 7
    local eye_y  = -1
    crayon.graphics.setColor(0.05, 0.05, 0.10, 1.0)
    crayon.graphics.drawCircle("fill", px - eye_dx + lx, py + eye_y + ly, 3)
    crayon.graphics.drawCircle("fill", px + eye_dx + lx, py + eye_y + ly, 3)

    -- Pupils catch light
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawCircle("fill", px - eye_dx + lx - 1, py + eye_y + ly - 1, 1)
    crayon.graphics.drawCircle("fill", px + eye_dx + lx - 1, py + eye_y + ly - 1, 1)
end