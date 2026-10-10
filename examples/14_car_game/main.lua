-- ============================================================================
-- CRAYON CAR GAME  (compact sandbox, infinite ground)
--
--   W / UP      throttle            S / DOWN   brake / reverse
--   A,D / LEFT,RIGHT  steer         SPACE      handbrake
--   MOUSE       orbit camera        WHEEL      zoom in/out
--   LSHIFT      nitro               R          reset upright (Shift+R: back to start)
--   C           camera mode         T          auto / manual transmission
--   Q / E       gear down / up (manual)
--   1 / 2 / 3   RWD / FWD / AWD     B          toggle anti-roll bars
--   F1          wheel telemetry     F2         physics debug draw
--   H           help                ESC        quit
-- ============================================================================

-- Config phase: disable modules this demo doesn't use.
function crayon.config(t)
    t.modules.physics2D = false
    t.modules.physics4D = false
    t.modules.audio     = false
    t.modules.particles = false
    t.modules.fs        = false
end

local P = crayon.physics3D
local G = crayon.graphics
local K = crayon.key
local M = crayon.mouse

local sin, cos, sqrt, atan2, acos, abs, min, max, pi, exp =
    math.sin, math.cos, math.sqrt, math.atan2 or math.atan, math.acos, math.abs,
    math.min, math.max, math.pi, math.exp
local format = string.format

-- ---------------------------------------------------------------- helpers ---
local function clamp(v, lo, hi) return v < lo and lo or (v > hi and hi or v) end
local function lerp(a, b, t) return a + (b - a) * t end
local function angle_lerp(a, b, t)
    local d = (b - a + pi) % (2 * pi) - pi
    return a + d * t
end

local function axis_angle(x, y, z, w)
    if w < 0 then x, y, z, w = -x, -y, -z, -w end
    local s = sqrt(max(0, 1 - w * w))
    if s < 1e-4 then return 0, 1, 0, 0 end
    return x / s, y / s, z / s, 2 * acos(clamp(w, -1, 1))
end

local function quat_forward(x, y, z, w)   -- local +Z in world space
    return 2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y)
end
local function quat_up(x, y, z, w)        -- local +Y in world space
    return 2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)
end

local function draw_with_pose(px, py, pz, qx, qy, qz, qw, fn)
    local ax, ay, az, ang = axis_angle(qx, qy, qz, qw)
    G.pushMatrix()
    G.translate(px, py, pz)
    if ang ~= 0 then G.rotate(ang, ax, ay, az) end
    fn()
    G.popMatrix()
end

-- ------------------------------------------------------------------ state ---
local models   = {}
local textures = {}
local car, chassis = nil, nil
local layout = "RWD"
local manual = false
local anti_roll_on = true
local show_help, show_telemetry, show_debug = true, false, false
local cam_mode = 1
local steer_input = 0.0
local nitro = 100.0
local err_msg = nil

local boxes = {}
local skids = {}
local skid_last = {}
local started = false

local START = { x = 0, y = 1.2, z = 0 }

-- LOW-END TUNING ------------------------------------------------------------
local SKID_MAX   = 60
local SKID_STEP  = 0.30
local FOG_END    = 90

local GROUND_HALF = 200

local cached_dt = 1 / 60
local cached_w  = 320
local cached_h  = 240

-- CAMERA (GTA-SA style) -----------------------------------------------------
--   cam.heading       smoothed car heading (avoids camera jitter on hard turns)
--   orbit.yaw         mouse-controlled yaw offset from car heading (radians)
--   orbit.pitch       camera elevation angle (radians, positive = above car)
--   orbit.dist        distance from car
--   orbit.idle        seconds since last mouse movement (for auto-recenter)
local cam = { heading = 0 }
local orbit = {
    yaw    = 0.0,
    pitch  = 0.24,        -- default elevation ~14 degrees
    dist   = 5.6,
    sens   = 0.0032,      -- radians per pixel of mouse delta
    pitch_min = -0.15,    -- look up (camera drops below car briefly)
    pitch_max =  1.15,    -- look down (top-down-ish)
    default_pitch = 0.24,
    idle = 0.0,
    -- Auto-recenter tunables:
    recenter_delay_yaw   = 0.35,   -- s of no mouse before yaw starts recentering
    recenter_delay_pitch = 1.20,   -- s of no mouse before pitch starts recentering
    recenter_base_rate   = 0.8,    -- baseline exponential rate (per second)
    recenter_max_rate    = 3.5,    -- at high speed
}

local cam_tbl = { position = { 0, 0, 0 }, target = { 0, 0, 0 }, up = { 0, 1, 0 }, fov = 62 }

-- --------------------------------------------------------------- the world ---
local function build_world()
    P.createPlane(0, 0, 0, 0, 1, 0, 500.0)

    local spots = {
        {  -5, 0.75,  8 },
        {   5, 0.75,  8 },
        {  -3, 0.75, 16 },
        {   3, 0.75, 16 },
    }
    for _, s in ipairs(spots) do
        local b = P.createBox(s[1], s[2], s[3], 0.75, 0.75, 0.75, "dynamic", 0.7, 0.25, 220)
        boxes[#boxes + 1] = { body = b, size = 1.5 }
    end
end

-- ------------------------------------------------------------------ the car ---
local function wheel_set(lay)
    local base = {
        radius = 0.36, width = 0.26,
        suspensionMinLength = 0.08, suspensionMaxLength = 0.32,
        suspensionFrequency = 1.7, suspensionDampingRatio = 0.55,
        maxSteerAngleRad = 0.55,
    }
    local function w(x, z, front, drive)
        local t = {}
        for k, v in pairs(base) do t[k] = v end
        t.pos = { x, -0.10, z }
        t.isFront = front
        t.isDrive = drive
        t.maxBrakeTorque = front and 3200 or 2400
        t.lateralGrip = front and 1.05 or 1.0
        return t
    end
    local fd = (lay == "FWD" or lay == "AWD")
    local rd = (lay == "RWD" or lay == "AWD")
    return {
        w( 1.00,  1.30, true,  fd),
        w(-1.00,  1.30, true,  fd),
        w( 1.00, -1.28, false, rd),
        w(-1.00, -1.28, false, rd),
    }
end

local function create_vehicle()
    local ok, v = pcall(P.createWheeledVehicle, {
        chassis = chassis,
        wheels = wheel_set(layout),
        maxPitchRollAngle = math.rad(75),

        engineMaxTorque = 360, engineMinRpm = 1000, engineMaxRpm = 6800,
        engineInertia = 0.4,
        torqueCurve = { { 0.0, 0.55 }, { 0.15, 0.80 }, { 0.45, 1.0 }, { 0.75, 0.95 }, { 1.0, 0.70 } },

        transmission = manual and "manual" or "auto",
        gearRatios = { 2.9, 1.95, 1.45, 1.12, 0.9, 0.75 },
        reverseGearRatios = { 3.2 },
        shiftUpRpm = 5600, shiftDownRpm = 2400, clutchStrength = 15,

        differentialRatio = 4.0, limitedSlipRatio = 1.6,
        centerLimitedSlipRatio = 1.8, frontTorqueSplit = 0.4,

        antiRollFront = anti_roll_on and 7000 or 0,
        antiRollRear  = anti_roll_on and 5000 or 0,
        collisionTester = "ray",
    })
    if not ok then err_msg = tostring(v); return nil end
    if manual then v:setGear(1, 1.0) end
    return v
end

local function spawn_car(x, y, z)
    chassis = P.createBox(x, y, z, 0.92, 0.30, 2.1, "dynamic", 0.3, 0.1, 320)
    chassis:setDamping(0.01, 0.35)
    chassis:setMotionQuality(true)
    car = create_vehicle()
    skid_last = {}
end

local function rebuild_vehicle()
    if car and car:isValid() then car:destroy() end
    car = create_vehicle()
end

local function reset_car(to_start)
    if not chassis then return end
    local x, y, z = chassis:getPosition()
    local heading = 0
    local qx, qy, qz, qw = chassis:getQuaternion()
    local fx, _, fz = quat_forward(qx, qy, qz, qw)
    heading = atan2(fx, fz)
    if to_start then
        x, y, z, heading = START.x, START.y, START.z, 0
        started = false
        nitro = 100
        orbit.yaw = 0
        orbit.pitch = orbit.default_pitch
        orbit.idle = 0
        cam.heading = 0
    else
        y = y + 1.2
    end
    chassis:setPosition(x, y, z, true)
    chassis:setRotation(0, math.deg(heading), 0, true)
    chassis:setVelocity(0, 0, 0)
    chassis:setAngularVelocity(0, 0, 0)
    skid_last = {}
end

-- --------------------------------------------------------------- lifecycle ---
function crayon.init()
    P = crayon.physics3D
    G = crayon.graphics
    K = crayon.key
    M = crayon.mouse
    crayon.window.setTitle("Crayon Car Game")
    crayon.window.setResolution(320, 240)

    cached_w, cached_h = crayon.window.getResolution()

    -- GTA-SA style mouse look: hide and lock the cursor, capture deltas.
    crayon.window.setMouseRelative(true)
    crayon.window.showCursor(false)

    models.car   = G.loadModel("assets/car_body.glb")
    models.wheel = G.loadModel("assets/wheel.glb")

    textures.ground = G.loadTexture("assets/grass.bmp")
    textures.crate  = G.loadTexture("assets/crate.bmp")

    P.setGravity(0, -9.81, 0)
    build_world()
    spawn_car(START.x, START.y, START.z)

    G.setLight(-0.45, -1.0, -0.35, 1.0, 0.95, 0.85, 0.38, 0.38, 0.45)
    G.setRetroEffects({ fog = { startDist = 40, endDist = FOG_END, color = { 0.55, 0.72, 0.92 } } })
end

function crayon.windowResized(w, h)
    cached_w, cached_h = w, h
end

local function update_driver(dt)
    if not (car and car:isValid()) then return end

    local up_k, down_k = K.isDown("w") or K.isDown("up"), K.isDown("s") or K.isDown("down")
    local left_k, right_k = K.isDown("a") or K.isDown("left"), K.isDown("d") or K.isDown("right")
    local handbrake = K.isDown("space") and 1.0 or 0.0

    local fwd_speed = car:getForwardSpeed()
    local speed_kmh = car:getSpeedKmh()

    local throttle, brake = 0.0, 0.0
    if manual then
        throttle = up_k and 1.0 or 0.0
        brake = down_k and 1.0 or 0.0
    else
        if up_k then
            if fwd_speed < -1.5 then brake = 1.0 else throttle = 1.0 end
        end
        if down_k then
            if fwd_speed > 1.5 then brake = 1.0; throttle = 0.0
            else throttle = -1.0 end
        end
    end

    local target = (right_k and 1 or 0) - (left_k and 1 or 0)
    local speed_factor = 1.0 / (1.0 + (speed_kmh / 90.0) ^ 2)
    local rate = (target == 0) and 7.0 or 4.5
    steer_input = steer_input + (target * speed_factor - steer_input) * min(1.0, rate * dt)

    car:setInputWheeled(throttle, steer_input, brake, handbrake)

    if manual then
        local gear = car:getTransmissionGear()
        if K.isPressed("e") then car:setGear(min(gear + 1, 6), 1.0) end
        if K.isPressed("q") then car:setGear(max(gear - 1, -1), 1.0) end
    end

    local vx, vy, vz = chassis:getVelocity()
    local speed = sqrt(vx * vx + vy * vy + vz * vz)
    local drag = 0.8 * speed * dt
    chassis:applyImpulse(-vx * drag, -vy * drag * 0.2, -vz * drag)
    chassis:applyImpulse(0, -0.35 * speed * speed * dt, 0)

    if K.isDown("lshift") and nitro > 0 and throttle > 0 then
        local qx, qy, qz, qw = chassis:getQuaternion()
        local fx, fy, fz = quat_forward(qx, qy, qz, qw)
        local f = 7500 * dt
        chassis:applyImpulse(fx * f, fy * f, fz * f)
        nitro = max(0, nitro - 28 * dt)
    else
        nitro = min(100, nitro + 7 * dt)
    end

    if not started and (throttle ~= 0 or brake ~= 0) then started = true end
end

-- ---------------------------------------------------------- mouse look (GTA) ---
-- Reads the per-frame mouse delta and applies it to the orbit.  When no
-- mouse input has been seen for a while, the yaw and pitch drift back to
-- their defaults, proportionally to how fast the car is moving (so a
-- stationary car keeps whatever the player aimed at, like GTA-SA on foot,
-- and a fast car snaps back behind it within a second or so).
local function update_mouse_look(dt)
    local dx, dy = M.getDelta()

    -- Wheel zoom.
    local _, wdy = M.getWheel()
    if wdy and abs(wdy) > 0.001 then
        orbit.dist = clamp(orbit.dist - wdy * 0.35, 3.0, 16.0)
    end

    if dx and dy and (abs(dx) > 0.001 or abs(dy) > 0.001) then
        orbit.yaw   = orbit.yaw - dx * orbit.sens
        -- mouse down (dy > 0) lowers the camera => looking up at the car
        orbit.pitch = clamp(orbit.pitch - dy * orbit.sens, orbit.pitch_min, orbit.pitch_max)
        orbit.idle  = 0
    else
        orbit.idle = orbit.idle + dt
    end

    -- Yaw auto-recenter (only while the car is actually moving).
    if orbit.idle > orbit.recenter_delay_yaw then
        local kmh = car and car:getSpeedKmh() or 0
        if kmh > 4 then
            -- wrap yaw into [-pi, pi] so we take the short way around
            if orbit.yaw >  pi then orbit.yaw = orbit.yaw - 2 * pi end
            if orbit.yaw < -pi then orbit.yaw = orbit.yaw + 2 * pi end
            local rate = orbit.recenter_base_rate
                + (orbit.recenter_max_rate - orbit.recenter_base_rate) * min(kmh / 60.0, 1.0)
            orbit.yaw = orbit.yaw * exp(-rate * dt)
        end
    end

    -- Pitch auto-recenter (returns to the default elevation).
    if orbit.idle > orbit.recenter_delay_pitch then
        local k = min(1.0, dt * 1.6)
        orbit.pitch = orbit.pitch + (orbit.default_pitch - orbit.pitch) * k
    end
end

local skid_parity = 0
local function update_skids()
    if not (car and car:isValid()) then return end

    skid_parity = 1 - skid_parity
    if skid_parity ~= 0 then return end

    for i = 1, 4 do
        local info = car:getWheelInfo(i - 1)
        if info and info.contact and info.skid > 0.55 then
            local p = info.contactPos
            local last = skid_last[i]
            local cur = { p.x, p.y + 0.02, p.z }
            if last then
                local dx, dz = cur[1] - last[1], cur[3] - last[3]
                local d = sqrt(dx * dx + dz * dz)
                if d > SKID_STEP and d < 4.0 then
                    local lx, lz = -dz / d * 0.11, dx / d * 0.11
                    skids[#skids + 1] = {
                        { last[1] + lx, last[2], last[3] + lz }, { last[1] - lx, last[2], last[3] - lz },
                        { cur[1] - lx, cur[2], cur[3] - lz },   { cur[1] + lx, cur[2], cur[3] + lz },
                    }
                    if #skids > SKID_MAX then table.remove(skids, 1) end
                    skid_last[i] = cur
                elseif d >= 4.0 then
                    skid_last[i] = cur
                end
            else
                skid_last[i] = cur
            end
        else
            skid_last[i] = nil
        end
    end
end

function crayon.update(dt)
    dt = min(dt, 0.05)
    cached_dt = dt

    if K.isPressed("escape") then crayon.window.quit() end

    if K.isPressed("1") then layout = "RWD"; rebuild_vehicle() end
    if K.isPressed("2") then layout = "FWD"; rebuild_vehicle() end
    if K.isPressed("3") then layout = "AWD"; rebuild_vehicle() end
    if K.isPressed("t") then
        manual = not manual
        if car then
            car:setManualTransmission(manual)
            if manual then car:setGear(1, 1.0) end
        end
    end
    if K.isPressed("b") then
        anti_roll_on = not anti_roll_on
        if car then
            car:setAntiRoll(0, anti_roll_on and 7000 or 0)
            car:setAntiRoll(1, anti_roll_on and 5000 or 0)
        end
    end
    if K.isPressed("c") then
        cam_mode = cam_mode % 4 + 1
        -- per-mode default distance
        if cam_mode == 1 then orbit.dist = 5.6
        elseif cam_mode == 2 then orbit.dist = 10.0
        elseif cam_mode == 4 then orbit.dist = 14.0 end
    end
    if K.isPressed("h") then show_help = not show_help end
    if K.isPressed("f1") then show_telemetry = not show_telemetry end
    if K.isPressed("f2") then show_debug = not show_debug end
    if K.isPressed("r") then reset_car(K.isDown("lshift")) end

    update_driver(dt)
    update_mouse_look(dt)
    update_skids()
end

-- ------------------------------------------------------------------ camera ---
local function update_camera()
    if not chassis then return end
    local px, py, pz = chassis:getPosition()
    local qx, qy, qz, qw = chassis:getQuaternion()
    local fx, fy, fz = quat_forward(qx, qy, qz, qw)
    local ux, uy, uz = quat_up(qx, qy, qz, qw)
    local car_heading = atan2(fx, fz)
    local dt = cached_dt
    local speed = car and car:getSpeedKmh() or 0

    -- Smoothed car heading for the camera's "base" — avoids camera jitter
    -- when the car snaps around during hard turns or landings.
    cam.heading = angle_lerp(cam.heading, car_heading, min(1, dt * 6.0))

    -- FOV widens slightly with speed.
    local fov_target = 62 + clamp(speed * 0.12, 0, 22)
    cam.fov = lerp(cam.fov or 62, fov_target, min(1, dt * 3))

    local pos, target = cam_tbl.position, cam_tbl.target

    if cam_mode == 3 then
        -- Hood cam: full car orientation, mouse doesn't apply (it's a cockpit).
        pos[1] = px + fx * 0.9 + ux * 0.62
        pos[2] = py + fy * 0.9 + uy * 0.62
        pos[3] = pz + fz * 0.9 + uz * 0.62
        target[1] = pos[1] + fx * 10
        target[2] = pos[2] + fy * 10
        target[3] = pos[3] + fz * 10
        cam_tbl.fov = 78
    else
        -- Orbit modes 1 / 2 / 4: heading = smoothed car heading + mouse yaw.
        local h = cam.heading + orbit.yaw
        local hx, hz = sin(h), cos(h)
        local cos_p, sin_p = cos(orbit.pitch), sin(orbit.pitch)

        -- Horizontal distance shrinks with elevation so the car stays framed.
        local horiz = orbit.dist * cos_p
        local vert  = orbit.dist * sin_p

        pos[1] = px - hx * horiz
        pos[2] = max(py + 0.8 + vert, 0.6)   -- never dip below the ground
        pos[3] = pz - hz * horiz

        -- Aim a little bit above the car's center so we see the road ahead.
        target[1] = px + hx * 2.5
        target[2] = py + 0.9
        target[3] = pz + hz * 2.5

        cam_tbl.fov = cam_mode == 4 and 55 or cam.fov
    end

    G.setCamera3D(cam_tbl)
end

-- -------------------------------------------------------------------- draw ---
local function draw_world()
    G.setColor(1, 1, 1, 1)
    G.drawPlane(0, 0.0, 0, GROUND_HALF * 2, GROUND_HALF * 2, textures.ground)

    for _, d in ipairs(boxes) do
        if d.body:isValid() then
            local x, y, z = d.body:getPosition()
            local qx, qy, qz, qw = d.body:getQuaternion()
            draw_with_pose(x, y, z, qx, qy, qz, qw, function()
                G.drawCube(0, 0, 0, d.size, d.size, d.size, textures.crate)
            end)
        end
    end

    G.setColor(0.04, 0.04, 0.05, 1)
    for _, q in ipairs(skids) do
        G.drawQuad3D(q[1], q[2], q[3], q[4])
        G.drawQuad3D(q[4], q[3], q[2], q[1])
    end
end

local function draw_car()
    if not (chassis and chassis:isValid()) then return end
    local px, py, pz = chassis:getPosition()
    local qx, qy, qz, qw = chassis:getQuaternion()
    G.setColor(1, 1, 1, 1)
    draw_with_pose(px, py, pz, qx, qy, qz, qw, function() G.drawModel(models.car, 0, 0, 0, 0, 0, 0, 1, 1, 1) end)

    if car and car:isValid() then
        for i = 0, car:getWheelCount() - 1 do
            local t = car:getWheelTransform(i)
            if t then
                local r = t.rotation
                draw_with_pose(t.position.x, t.position.y, t.position.z, r.x, r.y, r.z, r.w,
                    function() G.drawModel(models.wheel, 0, 0, 0, 0, 0, 0, 1, 1, 1) end)
            end
        end
    end
end

local function bar(x, y, w, h, frac, r, g, b)
    G.setColor(0, 0, 0, 0.55); G.drawRect("fill", x - 2, y - 2, w + 4, h + 4)
    G.setColor(r, g, b, 1);    G.drawRect("fill", x, y, w * clamp(frac, 0, 1), h)
end

local function draw_hud()
    local W, H = cached_w, cached_h

    if not (car and car:isValid()) then
        G.setColor(1, 0.3, 0.3, 1)
        G.drawText("Vehicle failed to build: " .. tostring(err_msg), 20, 20)
        return
    end

    local speed = car:getSpeedKmh()
    local rpm = car:getEngineRpm()
    local gear = car:getTransmissionGear()
    local gtxt = gear < 0 and "R" or (gear == 0 and "N" or tostring(gear))

    G.setColor(1, 1, 1, 1)
    G.drawText(format("%3d", speed), 24, H - 90, { scale = 2.4 })
    G.drawText("km/h", 116, H - 56, { scale = 1.1 })
    G.drawText(gtxt, 160, H - 96, { scale = 2.4 })

    bar(24, H - 24, 200, 8, rpm / 6800, rpm > 6000 and 1 or 0.2, rpm > 6000 and 0.2 or 0.85, 0.2)
    bar(24, H - 38, 100, 5, nitro / 100, 0.2, 0.6, 1.0)

    G.setColor(0.9, 0.9, 1, 1)
    G.drawText(format("%s | %s | AR %s | CAM %d", layout, manual and "MAN" or "AUTO", anti_roll_on and "ON" or "OFF", cam_mode), 24, 20)
    if car:isSkidding() then
        G.setColor(1, 0.5, 0.2, 1); G.drawText("SKID", 24, 36)
    end

    if show_telemetry then
        G.setColor(0, 0, 0, 0.55); G.drawRect("fill", W - 330, 70, 316, 130)
        G.setColor(1, 1, 1, 1)
        local names = { "FL", "FR", "RL", "RR" }
        for i = 1, 4 do
            local w = car:getWheelInfo(i - 1)
            if w then
                G.drawText(format("%s %s susp %.2f slip %+.2f/%+.2f %s", names[i], w.contact and "GND" or "AIR",
                    w.suspension, w.longSlip, w.latSlip, w.skidding and "SKID" or ""), W - 322, 78 + (i - 1) * 16)
            end
        end
        G.drawText(format("fwd speed %.1f m/s  rpm %d", car:getForwardSpeed(), rpm), W - 322, 148)
    end

    if show_help then
        G.setColor(0, 0, 0, 0.5); G.drawRect("fill", 14, 56, 290, 128)
        G.setColor(0.95, 0.95, 0.95, 1)
        local lines = {
            "W/S throttle+brake  A/D steer  SPACE handbrake",
            "MOUSE look  WHEEL zoom  LSHIFT nitro",
            "R reset  Shift+R restart",
            "C camera  T auto/manual (Q/E shift)",
            "1/2/3 RWD/FWD/AWD  B anti-roll",
            "F1 telemetry  F2 debug  H hide",
        }
        for i, l in ipairs(lines) do G.drawText(l, 22, 64 + (i - 1) * 16) end
    end
end

function crayon.draw()
    G.clear(0.55, 0.72, 0.92, 1)
    update_camera()
    if not show_debug then 
        draw_world()
        draw_car() 
    end
    if show_debug then P.drawDebug({ vehicles = true, shapes = true, constraints = false }) end
    draw_hud()
end