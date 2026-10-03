-- Spider screenpet, rendered as a flat black shadow. Eight legs, tetrapod
-- gait, chases the cursor and keeps a comfort distance. Click to startle.
--
-- Rendered as a single flat black silhouette: no eyes, no highlights, no
-- internal markings, no drop shadow. Only the outline reads.
--
-- Realism rework:
--   * Body shrunk relative to the legs. A real spider's leg span is 3-4x
--     its body length; earlier versions had near-equal proportions and
--     read as a cartoon.
--   * Body assembled from three shapes: rounded cephalothorax, tiny
--     pedicel disc, pointed oval abdomen.
--   * Legs taper from 2.2 px at the femur to 0.4 px at the tarsus tip,
--     drawn as tapered quads so the silhouette narrows continuously.
--   * Two visible joint kinks per leg. The femur-patella joint carries
--     most of the bend, the tibia-metatarsus joint adds a subtle second
--     bend. This double-kink is what reads as "spider" from above.
--   * Legs sprawl in a wide radial fan from ~30 degrees (front pair) to
--     ~120 degrees (rear pair) off the body axis.

function crayon.config(t)
    t.window.transparent   = true
    t.window.borderless    = true
    t.window.clickThrough  = true
    t.window.resizable     = false
    t.window.alwaysOnTop   = true
    t.window.width         = 200
    t.window.height        = 200
    t.window.virtualWidth  = 200
    t.window.virtualHeight = 200
    t.graphics.clearColor  = {0, 0, 0, 0}
    t.modules.mesh3D       = false
    t.modules.audio        = false
    t.modules.particles    = false
    t.modules.physics3d    = false
    t.modules.physics2d    = false
    t.window.opacity       = 1
end

-- ---------------------------------------------------------------------------
-- Tunables
-- ---------------------------------------------------------------------------
local WIN_W, WIN_H = 200, 200
local CX, CY       = WIN_W * 0.5, WIN_H * 0.5

local MAX_SPEED     = 480
local ACCEL_BLEND   = 6.5
local TURN_BLEND    = 9.0
local APPROACH_DIST = 80
local RETREAT_DIST  = 55
local EDGE_MARGIN   = 50
local CLICK_RADIUS  = 22

-- Gait
local STEP_DUR         = 0.12
local STEP_THRESHOLD   = 14
local DUR_SCALE_MIN    = 0.35
local THRESH_SCALE_MIN = 0.55
local URGENT_MULT      = 2.5
local MAX_PRED_LEAD    = 60
local MAX_STEP_TIME    = 0.45

-- Leg segment tuning. Knee at 40% along the anchor-foot line, offset out
-- by 9% of leg length. Second bend at 72% along, offset 3%.
local KNEE_T   = 0.40
local KNEE_OUT = 0.09
local BEND_T   = 0.72
local BEND_OUT = 0.03

-- ---------------------------------------------------------------------------
-- Body geometry (body-local: +X forward, +Y to the spider's right)
-- ---------------------------------------------------------------------------
local CEPH_POLY = {
    { 6.0,  0.0 },
    { 5.5,  3.0 },
    { 4.0,  5.0 },
    { 1.5,  6.5 },
    {-1.5,  6.5 },
    {-4.0,  5.0 },
    {-5.0,  2.5 },
    {-5.0,  0.0 },
    {-5.0, -2.5 },
    {-4.0, -5.0 },
    {-1.5, -6.5 },
    { 1.5, -6.5 },
    { 4.0, -5.0 },
    { 5.5, -3.0 },
}

local ABD_POLY = {
    {-5.0,  1.0 },
    {-5.5,  4.0 },
    {-7.0,  7.0 },
    {-10.0, 8.5 },
    {-13.0, 9.0 },
    {-16.5, 8.5 },
    {-20.0, 7.0 },
    {-23.0, 4.5 },
    {-26.0, 0.0 },
    {-23.0,-4.5 },
    {-20.0,-7.0 },
    {-16.5,-8.5 },
    {-13.0,-9.0 },
    {-10.0,-8.5 },
    {-7.0, -7.0 },
    {-5.5, -4.0 },
    {-5.0, -1.0 },
}

local PEDICEL_X, PEDICEL_R = -5.0, 2.5

-- Small nubs at the front of the cephalothorax (fang bases).
local CHELICERA_R = { 7.0,  1.3, 1.4 }
local CHELICERA_L = { 7.0, -1.3, 1.4 }

-- Pedipalps: short thin appendages pointing forward-out.
local PEDIPALP_R_ANCHOR = { 5.5,  2.5 }
local PEDIPALP_R_REST   = { 16.0, 7.0 }
local PEDIPALP_L_ANCHOR = { 5.5, -2.5 }
local PEDIPALP_L_REST   = { 16.0,-7.0 }

-- ---------------------------------------------------------------------------
-- Legs — anchors sit just inside the cephalothorax rim; rests fan widely.
-- ---------------------------------------------------------------------------
local LEG_DEFS = {
    { anchor = { 3.5,  3.5 }, rest = { 38,  22 }, group = "a" }, -- R1 front
    { anchor = { 0.5,  5.0 }, rest = { 34,  40 }, group = "b" }, -- R2
    { anchor = {-2.0,  5.0 }, rest = { 10,  48 }, group = "a" }, -- R3
    { anchor = {-4.5,  3.5 }, rest = {-24,  38 }, group = "b" }, -- R4 rear
    { anchor = { 3.5, -3.5 }, rest = { 38, -22 }, group = "b" }, -- L1
    { anchor = { 0.5, -5.0 }, rest = { 34, -40 }, group = "a" }, -- L2
    { anchor = {-2.0, -5.0 }, rest = { 10, -48 }, group = "b" }, -- L3
    { anchor = {-4.5, -3.5 }, rest = {-24, -38 }, group = "a" }, -- L4
}

-- ---------------------------------------------------------------------------
-- State
-- ---------------------------------------------------------------------------
local legs = {}
local pet    = { x = 0, y = 0, vx = 0, vy = 0, angle = 0, wobble = 0 }
local bounds = { x0 = 0, y0 = 0, x1 = 0, y1 = 0 }
local time   = 0
local startle = 0

local last_cx, last_cy = nil, nil

-- ---------------------------------------------------------------------------
-- Helpers
-- ---------------------------------------------------------------------------
local function atan2(y, x)
    if math.atan2 then return math.atan2(y, x) end
    return math.atan(y, x)
end

local function smootherstep(t)
    if t <= 0 then return 0 end
    if t >= 1 then return 1 end
    return t * t * t * (t * (t * 6 - 15) + 10)
end

local function is_bad_number(v)
    return v == nil or v ~= v or v == math.huge or v == -math.huge
end

local function cursor_global()
    local gx, gy = crayon.mouse.getGlobalPosition()
    if is_bad_number(gx) or is_bad_number(gy) then
        return last_cx or pet.x, last_cy or pet.y
    end
    if math.abs(gx) > 200000 or math.abs(gy) > 200000 then
        return last_cx or pet.x, last_cy or pet.y
    end
    if gx == 0 and gy == 0
       and last_cx and (math.abs(last_cx) > 2 or math.abs(last_cy) > 2) then
        return last_cx, last_cy
    end
    last_cx, last_cy = gx, gy
    return gx, gy
end

local function to_screen(wx, wy)
    return CX + (wx - pet.x), CY + (wy - pet.y)
end

local function body_angle()
    return pet.angle + pet.wobble
end

local function place_feet()
    local a = body_angle()
    local c, s = math.cos(a), math.sin(a)
    for _, leg in ipairs(legs) do
        local rx, ry = leg.rest[1], leg.rest[2]
        leg.foot_x = pet.x + rx * c - ry * s
        leg.foot_y = pet.y + rx * s + ry * c
        leg.stepping = false
        leg.step_t   = 0
        leg.step_elapsed = 0
        leg.lift     = 0
    end
end

-- ---------------------------------------------------------------------------
-- Gait
-- ---------------------------------------------------------------------------
local function update_legs(dt)
    local a = body_angle()
    local c, s = math.cos(a), math.sin(a)

    local speed = math.sqrt(pet.vx * pet.vx + pet.vy * pet.vy)
    local speed_frac = math.min(1, speed / MAX_SPEED)
    local dur_scale    = 1.0 - (1.0 - DUR_SCALE_MIN) * speed_frac
    local thresh_scale = 1.0 - (1.0 - THRESH_SCALE_MIN) * speed_frac

    local group_busy = { a = false, b = false }
    for _, leg in ipairs(legs) do
        if leg.stepping then group_busy[leg.group] = true end
    end

    for _, leg in ipairs(legs) do
        if is_bad_number(leg.foot_x) or is_bad_number(leg.foot_y) then
            local rr_x, rr_y = leg.rest[1], leg.rest[2]
            leg.foot_x = pet.x + rr_x * c - rr_y * s
            leg.foot_y = pet.y + rr_x * s + rr_y * c
            leg.stepping = false
            leg.step_t = 0
            leg.step_elapsed = 0
            leg.lift = 0
        end

        local rl_x, rl_y = leg.rest[1], leg.rest[2]
        local tx = pet.x + rl_x * c - rl_y * s
        local ty = pet.y + rl_x * s + rl_y * c

        if leg.stepping then
            leg.step_t       = leg.step_t + dt / leg.step_dur
            leg.step_elapsed = leg.step_elapsed + dt

            if leg.step_t >= 1 or leg.step_elapsed > MAX_STEP_TIME then
                leg.stepping = false
                leg.step_t   = 1
                leg.step_elapsed = 0
                leg.foot_x   = leg.target_x
                leg.foot_y   = leg.target_y
                leg.lift     = 0
            else
                local t  = leg.step_t
                local et = smootherstep(t)
                leg.foot_x = leg.from_x + (leg.target_x - leg.from_x) * et
                leg.foot_y = leg.from_y + (leg.target_y - leg.from_y) * et
                leg.lift   = math.sin(t * math.pi)
            end
        else
            local dx = tx - leg.foot_x
            local dy = ty - leg.foot_y
            local drift = math.sqrt(dx * dx + dy * dy)
            local threshold = leg.base_threshold * thresh_scale

            if drift > threshold then
                local other = (leg.group == "a") and "b" or "a"
                local urgent = drift > leg.base_threshold * URGENT_MULT

                if not group_busy[other] or urgent then
                    leg.stepping = true
                    leg.step_t   = 0
                    leg.step_elapsed = 0
                    leg.from_x   = leg.foot_x
                    leg.from_y   = leg.foot_y

                    local dur = leg.base_step_dur * dur_scale
                    if dur < 0.040 then dur = 0.040 end
                    if dur > 0.200 then dur = 0.200 end
                    leg.step_dur = dur

                    local pred_x = pet.vx * leg.step_dur
                    local pred_y = pet.vy * leg.step_dur
                    local plen = math.sqrt(pred_x * pred_x + pred_y * pred_y)
                    if plen > MAX_PRED_LEAD then
                        local k = MAX_PRED_LEAD / plen
                        pred_x = pred_x * k
                        pred_y = pred_y * k
                    end
                    leg.target_x = tx + pred_x
                    leg.target_y = ty + pred_y

                    group_busy[leg.group] = true
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------
-- Body motion
-- ---------------------------------------------------------------------------
local function update_body(dt)
    local gx, gy = cursor_global()
    local dx, dy = gx - pet.x, gy - pet.y
    local dist   = math.sqrt(dx * dx + dy * dy)

    if startle > 0 then
        startle = startle - dt
        local damp = math.exp(-3.2 * dt)
        pet.vx = pet.vx * damp
        pet.vy = pet.vy * damp
    else
        local tvx, tvy = 0, 0
        if dist > 0.001 then
            local nx, ny = dx / dist, dy / dist
            if dist > APPROACH_DIST + 6 then
                local k = math.min(1, (dist - APPROACH_DIST) / 140)
                tvx = nx * MAX_SPEED * k
                tvy = ny * MAX_SPEED * k
            elseif dist < RETREAT_DIST - 6 then
                local k = math.min(1, (RETREAT_DIST - dist) / 40)
                tvx = -nx * MAX_SPEED * 0.75 * k
                tvy = -ny * MAX_SPEED * 0.75 * k
            end
        end
        local blend = 1 - math.exp(-ACCEL_BLEND * dt)
        pet.vx = pet.vx + (tvx - pet.vx) * blend
        pet.vy = pet.vy + (tvy - pet.vy) * blend
    end

    pet.x = pet.x + pet.vx * dt
    pet.y = pet.y + pet.vy * dt

    local target_angle = pet.angle
    if dist > 4 then
        target_angle = atan2(dy, dx)
    else
        local sp = math.sqrt(pet.vx * pet.vx + pet.vy * pet.vy)
        if sp > 8 then target_angle = atan2(pet.vy, pet.vx) end
    end
    local da = target_angle - pet.angle
    while da >  math.pi do da = da - 2 * math.pi end
    while da < -math.pi do da = da + 2 * math.pi end
    pet.angle = pet.angle + da * (1 - math.exp(-TURN_BLEND * dt))

    local speed = math.sqrt(pet.vx * pet.vx + pet.vy * pet.vy)
    local wf = math.min(1, speed / 320)
    local wobble_target = math.sin(time * 13) * 0.055 * wf
    pet.wobble = pet.wobble + (wobble_target - pet.wobble)
                              * (1 - math.exp(-16 * dt))

    local clamped_x = math.max(bounds.x0 + EDGE_MARGIN,
                       math.min(bounds.x1 - EDGE_MARGIN, pet.x))
    if clamped_x ~= pet.x then pet.vx = 0 end
    pet.x = clamped_x

    local clamped_y = math.max(bounds.y0 + EDGE_MARGIN,
                       math.min(bounds.y1 - EDGE_MARGIN, pet.y))
    if clamped_y ~= pet.y then pet.vy = 0 end
    pet.y = clamped_y
end

-- ---------------------------------------------------------------------------
-- Lifecycle
-- ---------------------------------------------------------------------------
function crayon.init()
    local ux, uy, uw, uh = crayon.window.getUsableBounds()
    bounds.x0, bounds.y0 = ux, uy
    bounds.x1, bounds.y1 = ux + uw, uy + uh

    pet.x     = ux + uw * 0.5
    pet.y     = uy + uh * 0.4
    pet.angle = -math.pi * 0.5
    pet.vx, pet.vy = 0, 0
    pet.wobble = 0

    last_cx, last_cy = pet.x, pet.y

    legs = {}
    for i, def in ipairs(LEG_DEFS) do
        local base_dur = STEP_DUR + ((i % 3) - 1) * 0.010
        local base_thr = STEP_THRESHOLD + (i % 4) * 0.8
        legs[i] = {
            anchor = def.anchor,
            rest   = def.rest,
            group  = def.group,
            foot_x = 0, foot_y = 0,
            stepping = false, step_t = 0, step_elapsed = 0,
            from_x = 0, from_y = 0,
            target_x = 0, target_y = 0,
            lift = 0,
            base_step_dur  = base_dur,
            base_threshold = base_thr,
            step_dur       = base_dur,
            step_threshold = base_thr,
        }
    end
    place_feet()

    crayon.window.setPosition(
        math.floor(pet.x - WIN_W * 0.5),
        math.floor(pet.y - WIN_H * 0.5))
end

function crayon.update(dt)
    if dt > 0.05 then dt = 0.05 end
    if dt < 0 then dt = 0 end
    time = time + dt

    update_body(dt)
    update_legs(dt)

    crayon.window.setPosition(
        math.floor(pet.x - WIN_W * 0.5),
        math.floor(pet.y - WIN_H * 0.5))

    local gx, gy = cursor_global()
    local dx, dy = gx - pet.x, gy - pet.y
    local over = (dx * dx + dy * dy) <= CLICK_RADIUS * CLICK_RADIUS
    crayon.window.setClickThrough(not over)
end

function crayon.mousedown(x, y, button)
    if button ~= 1 then return end

    local wx, wy = crayon.window.getPosition()
    local gx, gy = wx + x, wy + y
    local dx, dy = gx - pet.x, gy - pet.y
    if (dx * dx + dy * dy) > CLICK_RADIUS * CLICK_RADIUS then return end

    startle = 0.85
    local d = math.sqrt(dx * dx + dy * dy)
    local nx, ny
    if d > 0.5 then
        nx, ny = dx / d, dy / d
    else
        nx, ny = math.cos(pet.angle), math.sin(pet.angle)
    end
    pet.vx = -nx * MAX_SPEED * 1.9
    pet.vy = -ny * MAX_SPEED * 1.9
end

-- ---------------------------------------------------------------------------
-- Rendering
-- ---------------------------------------------------------------------------

-- Draw a segment as a tapered quadrilateral, so the leg silhouette narrows
-- continuously rather than stepping at joint boundaries.
local function draw_tapered_segment(x1, y1, x2, y2, w1, w2)
    local dx = x2 - x1
    local dy = y2 - y1
    local len = math.sqrt(dx * dx + dy * dy)
    if len < 0.001 then return end
    local px = -dy / len
    local py =  dx / len
    local h1 = w1 * 0.5
    local h2 = w2 * 0.5
    crayon.graphics.drawPolygon("fill", {
        { x1 + px * h1, y1 + py * h1 },
        { x2 + px * h2, y2 + py * h2 },
        { x2 - px * h2, y2 - py * h2 },
        { x1 - px * h1, y1 - py * h1 },
    })
end

local function body_poly_to_window(poly, breathe, c, s)
    local pts = {}
    for i = 1, #poly do
        local lx = poly[i][1] * breathe
        local ly = poly[i][2] * breathe
        pts[i] = { CX + lx * c - ly * s, CY + lx * s + ly * c }
    end
    return pts
end

-- ---------------------------------------------------------------------------
-- Legs: two visible kinks per limb.
--   A  = anchor on the body
--   J1 = femur-patella joint (the big outward knee)
--   J2 = tibia-metatarsus joint (a much subtler second bend)
--   F  = foot tip
-- J1 and J2 are found by taking a point along A->F and pushing it outward,
-- perpendicular to the leg direction. The sign of the push is chosen so it
-- points away from the body centre, which gives each leg its outward bow.
-- ---------------------------------------------------------------------------
local function draw_leg(leg)
    local a = body_angle()
    local c, s = math.cos(a), math.sin(a)

    local ax = pet.x + leg.anchor[1] * c - leg.anchor[2] * s
    local ay = pet.y + leg.anchor[1] * s + leg.anchor[2] * c

    local pull = leg.lift * 0.30
    local fx = leg.foot_x + (ax - leg.foot_x) * pull
    local fy = leg.foot_y + (ay - leg.foot_y) * pull

    local dx = fx - ax
    local dy = fy - ay
    local L = math.sqrt(dx * dx + dy * dy)
    if L < 0.001 then return end
    local ux, uy = dx / L, dy / L

    -- Perpendicular to leg direction, signed outward
    local pxp, pyp = -uy, ux
    local mx = (ax + fx) * 0.5
    local my = (ay + fy) * 0.5
    local ox, oy = mx - pet.x, my - pet.y
    if pxp * ox + pyp * oy < 0 then
        pxp, pyp = -pxp, -pyp
    end

    -- Joint 1: the knee
    local j1x = ax + ux * L * KNEE_T + pxp * L * KNEE_OUT
    local j1y = ay + uy * L * KNEE_T + pyp * L * KNEE_OUT

    -- Joint 2: subtle second bend along J1->F
    local j1dx = fx - j1x
    local j1dy = fy - j1y
    local j2x = j1x + j1dx * ((BEND_T - KNEE_T) / (1 - KNEE_T))
                     + pxp * L * BEND_OUT
    local j2y = j1y + j1dy * ((BEND_T - KNEE_T) / (1 - KNEE_T))
                     + pyp * L * BEND_OUT

    local sax, say   = to_screen(ax,  ay)
    local sj1x, sj1y = to_screen(j1x, j1y)
    local sj2x, sj2y = to_screen(j2x, j2y)
    local sfx, sfy   = to_screen(fx,  fy)

    -- Three tapered segments: femur, tibia, metatarsus+tarsus. Widths fall
    -- from 2.2 at the base to 0.4 at the tip, matching a real spider's
    -- gradual taper down the limb.
    draw_tapered_segment(sax,  say,  sj1x, sj1y, 2.2, 1.5)
    draw_tapered_segment(sj1x, sj1y, sj2x, sj2y, 1.5, 0.9)
    draw_tapered_segment(sj2x, sj2y, sfx,  sfy,  0.9, 0.4)

    -- Tiny joint dots, just enough to smooth the transitions.
    crayon.graphics.drawCircle("fill", sj1x, sj1y, 0.8, 8)
    crayon.graphics.drawCircle("fill", sj2x, sj2y, 0.5, 6)
end

local function draw_pedipalp(anchor, rest, phase)
    local a = body_angle()
    local c, s = math.cos(a), math.sin(a)

    local ax = pet.x + anchor[1] * c - anchor[2] * s
    local ay = pet.y + anchor[1] * s + anchor[2] * c

    local bob = math.sin(time * 6 + phase) * 0.8
    local fx = pet.x + rest[1] * c - rest[2] * s + bob * c
    local fy = pet.y + rest[1] * s + rest[2] * c + bob * s

    local mx = (ax + fx) * 0.5
    local my = (ay + fy) * 0.5

    local sax, say = to_screen(ax, ay)
    local smx, smy = to_screen(mx, my)
    local sfx, sfy = to_screen(fx, fy)

    draw_tapered_segment(sax, say, smx, smy, 1.3, 0.9)
    draw_tapered_segment(smx, smy, sfx, sfy, 0.9, 0.4)
    crayon.graphics.drawCircle("fill", smx, smy, 0.5, 6)
end

local function draw_body()
    local breathe = 1 + math.sin(time * 2.4) * 0.02
    local a = body_angle()
    local c, s = math.cos(a), math.sin(a)

    -- Pedicel first: it sits behind the two big body shapes.
    local pcx = CX + PEDICEL_X * c
    local pcy = CY + PEDICEL_X * s
    crayon.graphics.drawCircle("fill", pcx, pcy, PEDICEL_R, 12)

    -- Abdomen
    crayon.graphics.drawPolygon("fill",
        body_poly_to_window(ABD_POLY, breathe, c, s))

    -- Cephalothorax
    crayon.graphics.drawPolygon("fill",
        body_poly_to_window(CEPH_POLY, breathe, c, s))

    -- Chelicerae (fang bases) at the front
    for _, ch in ipairs({CHELICERA_R, CHELICERA_L}) do
        local lx = ch[1] * breathe
        local ly = ch[2] * breathe
        local chx = CX + lx * c - ly * s
        local chy = CY + lx * s + ly * c
        crayon.graphics.drawCircle("fill", chx, chy, ch[3], 10)
    end
end

function crayon.draw()
    crayon.graphics.setColor(0, 0, 0, 1)
    crayon.graphics.pushScissor(0, 0, WIN_W, WIN_H)

    for _, leg in ipairs(legs) do
        draw_leg(leg)
    end
    draw_pedipalp(PEDIPALP_R_ANCHOR, PEDIPALP_R_REST, 0.0)
    draw_pedipalp(PEDIPALP_L_ANCHOR, PEDIPALP_L_REST, 1.7)
    draw_body()

    crayon.graphics.popScissor()
end