-- ============================================================================
-- CRAYON TRUCK SIMULATOR  (small grass map + crates, like the car game)
--
--   W/UP throttle      S/DOWN brake / reverse      A,D / LEFT,RIGHT steer
--   SPACE  parking brake (hold)      P  toggle parking brake
--   H   couple / uncouple trailer
--   T   container twist-locks
--   E   crane (inside a depot square)
--   M   auto / manual gearbox (Z / X shift)       F  refuel
--   C   camera           R  recover upright (Shift+R: back to depot)
--   F1  telemetry        F2  physics debug        F3  help       ESC quit
--
-- Models: tools/gen_truck_models.py.   Forward = +Z, left = +X, steering +1 = right.
-- ============================================================================
local P, G, K = crayon.physics3D, crayon.graphics, crayon.key
local sin, cos, sqrt, acos, abs, min, max, pi =
    math.sin, math.cos, math.sqrt, math.acos, math.abs, math.min, math.max, math.pi
local atan2 = math.atan2 or math.atan

local function clamp(v, lo, hi) return v < lo and lo or (v > hi and hi or v) end
local function lerp(a, b, t) return a + (b - a) * t end
local function angle_lerp(a, b, t) local d = (b - a + pi) % (2 * pi) - pi; return a + d * t end
local function axis_angle(x, y, z, w)
    if w < 0 then x, y, z, w = -x, -y, -z, -w end
    local s = sqrt(max(0, 1 - w * w))
    if s < 1e-4 then return 0, 1, 0, 0 end
    return x / s, y / s, z / s, 2 * acos(clamp(w, -1, 1))
end
local function qrot(q, vx, vy, vz)
    local x, y, z, w = q[1], q[2], q[3], q[4]
    local tx, ty, tz = 2 * (y * vz - z * vy), 2 * (z * vx - x * vz), 2 * (x * vy - y * vx)
    return vx + w * tx + (y * tz - z * ty), vy + w * ty + (z * tx - x * tz), vz + w * tz + (x * ty - y * tx)
end
local function quat_of(body) local x, y, z, w = body:getQuaternion(); return { x, y, z, w } end
local function yaw_of(q) local fx, _, fz = qrot(q, 0, 0, 1); return atan2(fx, fz) end

local function draw_pose(px, py, pz, q, fn)
    local ax, ay, az, ang = axis_angle(q[1], q[2], q[3], q[4])
    G.pushMatrix(); G.translate(px, py, pz)
    if ang ~= 0 then G.rotate(ang, ax, ay, az) end
    fn(); G.popMatrix()
end

-- ------------------------------------------------------------- constants ---
local TRACTOR_Y, TRAILER_Y = 1.15, 1.50
local PLATE   = { 0, 0.45, -2.2 }         -- fifth wheel, tractor local
local KINGPIN = { 0, -0.20, 5.9 }         -- kingpin, trailer local
local LEG_Z, LEG_X = 4.0, 1.15
local CONT_Y = 1.45
local MAX_KMH = 90

-- Two depots, close together so the small grass map covers the whole course.
local DEPOTS = {
    A = { x = 0, z = 0,  name = "Harbor Depot" },
    B = { x = 0, z = 80, name = "Inland Terminal" },
}
local FUEL = { x = 60, z = 10 }

-- Ground: an infinite Jolt half-space for physics, and one grass-textured
-- plane for visuals -- same approach as the car game (grass.bmp).
local GROUND_HALF = 150

-- ----------------------------------------------------------------- state ---
local models, textures = {}, {}
local colors = { "red", "blue", "green", "orange" }
local tractor, tvehicle
local trailer, trvehicle
local legs = {}
local hitch = nil
local container = nil
local crates = {}                          -- dynamic wooden crates (car-game style)
local manual, parking = false, false
local steer_in, fuel, money, damage = 0, 100.0, 0, 0.0
local job = nil
local crane = { state = "idle", t = 0, color = "red", from = nil }
local msg, msg_t = "", 0
local cam_mode, cam = 1, { heading = 0, fov = 62 }
local show_help, show_tel, show_dbg = true, false, false
local last_cv = { 0, 0, 0 }

local function say(t, d) msg = t; msg_t = d or 3.0 end

-- ----------------------------------------------------------------- world ---
-- Physics ground is an infinite half-space; the only visual world content is
-- the grass plane, the 4 crates, the two depot squares, and the fuel pad.
local function build_world()
    P.createPlane(0, 0, 0, 0, 1, 0, 2000)

    -- 4 crates, same positions / size / density as the car game.
    local spots = {
        { -40, 0.75, 10 },
        { -40, 0.75, 18 },
        { -35, 0.75, 14 },
        { -45, 0.75, 14 },
    }
    for _, s in ipairs(spots) do
        local b = P.createBox(s[1], s[2], s[3], 0.75, 0.75, 0.75, "dynamic", 0.7, 0.25, 220)
        crates[#crates + 1] = b
    end
end

-- ----------------------------------------------------------------- units ---
-- Wheel tuning notes (Jolt):
--   * Wheel Y is relative to chassis center of mass.
--   * Anti-roll bars in the 4000-10000 Nm range (real truck values).
--   * Hitch uses a HINGE (yaw free, pitch/roll locked) -- a PointConstraint
--     would let the trailer pitch/roll freely and feed spin into the tractor.
local function wheel_list(front_z, axles_z, base_x, drive, is_trailer)
    local list = {}
    local wy = is_trailer and -0.45 or -0.25
    local smin = 0.20
    local smax = is_trailer and 0.55 or 0.62
    local sf = is_trailer and 1.05 or 1.15
    local function mk(x, z, front, drv)
        return {
            pos = { x, wy, z },
            radius = 0.5, width = 0.46,
            suspensionMinLength = smin,
            suspensionMaxLength = smax,
            suspensionFrequency = sf,
            suspensionDampingRatio = 0.7,
            maxSteerAngleRad = 0.45,
            maxBrakeTorque = is_trailer and 9000 or 12000,
            maxHandBrakeTorque = front and 0 or 16000,
            isFront = front, isDrive = drv,
            steer = front and not is_trailer or false,
            inertia = 2.0, angularDamping = 0.15,
            longitudinalGrip = is_trailer and 1.0 or 1.1,
            lateralGrip = is_trailer and 1.0 or 1.1,
        }
    end
    if front_z then
        list[#list + 1] = mk(base_x, front_z, true, false)
        list[#list + 1] = mk(-base_x, front_z, true, false)
    end
    for _, z in ipairs(axles_z) do
        list[#list + 1] = mk(base_x, z, false, drive)
        list[#list + 1] = mk(-base_x, z, false, drive)
    end
    return list
end

local function create_tractor_vehicle()
    local ok, v = pcall(P.createWheeledVehicle, {
        chassis = tractor,
        wheels = wheel_list(2.4, { -1.7, -3.0 }, 1.0, true, false),
        maxPitchRollAngle = math.rad(60),
        engineMaxTorque = 1500, engineMinRpm = 600, engineMaxRpm = 2200, engineInertia = 2.0,
        torqueCurve = { { 0.0, 0.50 }, { 0.20, 0.85 }, { 0.50, 1.0 }, { 0.80, 0.90 }, { 1.0, 0.55 } },
        transmission = manual and "manual" or "auto",
        gearRatios = { 6.4, 4.2, 3.0, 2.2, 1.6, 1.2, 1.0, 0.8 },
        reverseGearRatios = { 6.0 },
        shiftUpRpm = 1850, shiftDownRpm = 1000, clutchStrength = 40, switchTime = 0.4,
        differentialRatio = 3.9, limitedSlipRatio = 1.5,
        antiRollFront = 8000, antiRollRear = 8000,
    })
    if not ok then say("Tractor vehicle failed: " .. tostring(v), 10); return nil end
    if manual then v:setGear(1, 1.0) end
    return v
end

local function create_trailer_vehicle()
    local ok, v = pcall(P.createWheeledVehicle, {
        chassis = trailer,
        wheels = wheel_list(nil, { -4.4, -5.7 }, 1.0, false, true),
        maxPitchRollAngle = math.rad(70),
        engineMaxTorque = 1, transmission = "manual",
        antiRollFront = 5000, antiRollRear = 5000,
    })
    if not ok then say("Trailer vehicle failed: " .. tostring(v), 10); return nil end
    v:setGear(0, 0.0)
    return v
end

local function legs_deploy()
    if #legs > 0 then return end
    local tp = { trailer:getPosition() }
    local q = quat_of(trailer)
    local yaw = math.deg(yaw_of(q))
    for _, sx in ipairs({ 1, -1 }) do
        local wx, wy, wz = qrot(q, sx * LEG_X, -1.0, LEG_Z)
        local b = P.createBox(tp[1] + wx, tp[2] + wy, tp[3] + wz, 0.1, 0.85, 0.15, "dynamic", 0.9, 0.0, 300)
        b:setRotation(0, yaw, 0, true)
        local c = P.createFixedConstraint(trailer, b)
        legs[#legs + 1] = { body = b, con = c }
    end
end
local function legs_retract()
    for _, l in ipairs(legs) do
        if l.con then P.destroyConstraint(l.con) end
        if l.body:isValid() then l.body:destroy() end
    end
    legs = {}
end

-- ------------------------------------------------------------- container ---
local function container_place(color)
    local q = quat_of(trailer)
    local tx, ty, tz = trailer:getPosition()
    local ox, oy, oz = qrot(q, 0, CONT_Y + 0.02, -0.05)
    local b = P.createBox(tx + ox, ty + oy, tz + oz, 1.2, 1.3, 6.0, "dynamic", 0.7, 0.05, 90)
    b:setRotation(0, math.deg(yaw_of(q)), 0, true)
    b:setVelocity(0, 0, 0)
    container = { body = b, color = color, lock = nil }
    container.lock = P.createFixedConstraint(trailer, b)
    last_cv = { 0, 0, 0 }
end
local function container_remove()
    if not container then return end
    if container.lock then P.destroyConstraint(container.lock) end
    if container.body:isValid() then container.body:destroy() end
    container = nil
end

local function toggle_lock()
    if not container or not trailer then return end
    if container.lock then
        P.destroyConstraint(container.lock); container.lock = nil
        say("Twist-locks OPEN - container is free. Do not drive!", 3)
    else
        local q = quat_of(trailer)
        local tx, ty, tz = trailer:getPosition()
        local cx, cy, cz = container.body:getPosition()
        local lx, ly, lz = qrot({ -q[1], -q[2], -q[3], q[4] }, cx - tx, cy - ty, cz - tz)
        if abs(lx) < 0.5 and abs(ly - CONT_Y) < 0.35 and abs(lz) < 1.2 then
            container.lock = P.createFixedConstraint(trailer, container.body)
            say("Twist-locks LOCKED", 2)
        else
            say("Container is not seated on the deck", 2.5)
        end
    end
end

-- ----------------------------------------------------------------- hitch ---
local function kingpin_world()
    local q = quat_of(trailer); local x, y, z = trailer:getPosition()
    local ox, oy, oz = qrot(q, KINGPIN[1], KINGPIN[2], KINGPIN[3])
    return x + ox, y + oy, z + oz
end
local function plate_world()
    local q = quat_of(tractor); local x, y, z = tractor:getPosition()
    local ox, oy, oz = qrot(q, PLATE[1], PLATE[2], PLATE[3])
    return x + ox, y + oy, z + oz
end
local function hitch_status()
    local kx, ky, kz = kingpin_world(); local px, py, pz = plate_world()
    local dh = sqrt((kx - px) ^ 2 + (kz - pz) ^ 2)
    local dv = ky - py
    local ya = abs((yaw_of(quat_of(trailer)) - yaw_of(quat_of(tractor)) + pi) % (2 * pi) - pi)
    return dh, dv, ya
end

local function toggle_hitch()
    if hitch then
        P.destroyConstraint(hitch); hitch = nil
        legs_deploy()
        say("Trailer uncoupled - landing legs down", 3)
        return
    end
    local dh, dv, ya = hitch_status()
    if dh > 0.9 or abs(dv) > 0.55 then say(string.format("Fifth wheel not under kingpin (%.1f m off)", dh), 3); return end
    if ya > math.rad(25) then say("Tractor and trailer are not aligned", 3); return end
    local kx, ky, kz = kingpin_world(); local px, py, pz = plate_world()
    legs_retract()
    -- Hinge around world-vertical: yaw free, pitch/roll locked (real fifth wheel).
    hitch = P.createHingeConstraint(tractor, trailer, (kx + px) / 2, (ky + py) / 2, (kz + pz) / 2, 0, 1, 0)
    say("Trailer coupled", 3)
end

-- ------------------------------------------------------------- spawning ---
local function spawn_all()
    tractor = P.createBox(0, TRACTOR_Y, 30, 0.9, 0.4, 3.6, "dynamic", 0.4, 0.1, 770)
    tractor:setDamping(0.01, 0.3); tractor:setMotionQuality(true)
    tvehicle = create_tractor_vehicle()

    trailer = P.createBox(0, TRAILER_Y, 8, 1.2, 0.15, 6.5, "dynamic", 0.4, 0.1, 640)
    trailer:setDamping(0.01, 0.3)
    trvehicle = create_trailer_vehicle()
    legs_deploy()
    container_place("red")
    job = { from = "A", to = "B", color = "red", pay = 2400 }
end

local function recover(to_depot)
    local x, y, z = tractor:getPosition()
    local yaw = yaw_of(quat_of(tractor))
    if to_depot then x, y, z, yaw = 0, TRACTOR_Y, 30, 0 else y = y + 1.5 end
    tractor:setPosition(x, y, z, true); tractor:setRotation(0, math.deg(yaw), 0, true)
    tractor:setVelocity(0, 0, 0); tractor:setAngularVelocity(0, 0, 0)
    if hitch and not to_depot then
        local q = quat_of(tractor)
        local ox, oy, oz = qrot(q, 0, TRAILER_Y - TRACTOR_Y + 0.1, -9.0)
        P.destroyConstraint(hitch); hitch = nil
        trailer:setPosition(x + ox, y + oy, z + oz, true); trailer:setRotation(0, math.deg(yaw), 0, true)
        trailer:setVelocity(0, 0, 0); trailer:setAngularVelocity(0, 0, 0)
        legs_deploy()
        if container and container.lock then P.destroyConstraint(container.lock); container.lock = nil end
        say("Recovered - re-couple with H, re-lock container with T", 4)
    end
end

-- --------------------------------------------------------- crane / jobs ---
local function in_depot(id)
    local d = DEPOTS[id]
    local x, _, z = trailer:getPosition()
    return abs(x - d.x) < 9 and abs(z - (d.z + 24)) < 12
end
local function current_depot()
    for id, _ in pairs(DEPOTS) do if in_depot(id) then return id end end
end
local function trailer_stopped()
    local vx, vy, vz = trailer:getVelocity()
    return sqrt(vx * vx + vz * vz) < 1.0
end

local function crane_action()
    if crane.state ~= "idle" then return end
    local id = current_depot()
    if not id then say("Park the TRAILER inside a yellow depot square", 3); return end
    if not trailer_stopped() then say("Stop the vehicle first", 2); return end
    if container then
        if not container.lock then say("Lock the container first (T)", 3); return end
        if not job or job.to ~= id then say("This container is not for this depot", 3); return end
        crane.state = "unload"; crane.t = 0; crane.color = container.color
        container_remove()
    else
        local to = (id == "A") and "B" or "A"
        crane.state = "load"; crane.t = 0
        crane.color = colors[math.random(#colors)]
        job = { from = id, to = to, color = crane.color, pay = 2000 + math.random(0, 1500) }
    end
end

local function update_crane(dt)
    if crane.state == "idle" then return end
    crane.t = crane.t + dt
    local dur = 3.0
    if crane.state == "unload" then
        if crane.t >= dur then
            local bonus = max(0.2, 1 - damage / 100)
            local pay = math.floor(job.pay * bonus)
            money = money + pay
            say(string.format("Delivered! +$%d", pay), 5)
            job = nil; damage = 0; crane.state = "idle"
        end
    else
        if crane.t >= dur then
            container_place(crane.color)
            say(string.format("Cargo loaded - deliver to %s", DEPOTS[job.to].name), 5)
            crane.state = "idle"
        end
    end
end

local function track_damage(dt)
    if not container then return end
    local vx, vy, vz = container.body:getVelocity()
    local dv = sqrt((vx - last_cv[1]) ^ 2 + (vy - last_cv[2]) ^ 2 + (vz - last_cv[3]) ^ 2)
    last_cv = { vx, vy, vz }
    local acc = dv / max(dt, 1e-3)
    if acc > 14 then damage = min(100, damage + (acc - 14) * dt * 0.8) end
    if not container.lock then damage = min(100, damage + dt * 2) end
end

-- ------------------------------------------------------------- lifecycle ---
function crayon.init()
    P, G, K = crayon.physics3D, crayon.graphics, crayon.key

    crayon.window.setTitle("Crayon Truck Simulator")
    crayon.window.setResolution(320*2, 240*2)

    models.tractor = G.loadModel("assets/tractor.glb")
    models.trailer = G.loadModel("assets/trailer.glb")
    models.legs    = G.loadModel("assets/trailer_legs.glb")
    models.wheel   = G.loadModel("assets/truck_wheel.glb")
    for _, c in ipairs(colors) do models["cont_" .. c] = G.loadModel("assets/container_" .. c .. ".glb") end

    -- Same textures as the car game.
    textures.ground = G.loadTexture("assets/grass.bmp")
    textures.crate  = G.loadTexture("assets/crate.bmp")

    P.setGravity(0, -9.81, 0)
    build_world()
    spawn_all()
    G.setRetroEffects({ fog = { startDist = 80, endDist = GROUND_HALF * 2 - 20, color = { 0.62, 0.74, 0.88 } } })
end

local function drive(dt)
    if not (tvehicle and tvehicle:isValid()) then return end
    local fwd = tvehicle:getForwardSpeed()
    local kmh = tvehicle:getSpeedKmh()
    local up_k, dn_k = K.isDown("w") or K.isDown("up"), K.isDown("s") or K.isDown("down")
    local lf, rt = K.isDown("a") or K.isDown("left"), K.isDown("d") or K.isDown("right")
    local hand = (K.isDown("space") or parking) and 1.0 or 0.0

    local thr, brk = 0.0, 0.0
    if manual then
        thr = up_k and 1.0 or 0.0; brk = dn_k and 1.0 or 0.0
    else
        if up_k then if fwd < -1.0 then brk = 1.0 else thr = 1.0 end end
        if dn_k then if fwd > 1.0 then brk = 1.0; thr = 0.0 else thr = -1.0 end end
    end
    if fuel <= 0 then thr = 0 end
    if kmh > MAX_KMH and thr > 0 then thr = 0 end
    brk = brk * 0.9 + (brk > 0 and 0.1 or 0)

    local target = (rt and 1 or 0) - (lf and 1 or 0)
    local limit = 1.0 / (1.0 + (kmh / 45.0) ^ 2)
    steer_in = steer_in + (target * limit - steer_in) * min(1.0, ((target == 0) and 6.0 or 1.8) * dt)

    tvehicle:setInputWheeled(thr, steer_in, brk, hand)
    if trvehicle and trvehicle:isValid() then
        local tb = max(brk, hand)
        if not hitch then tb = 1.0 end
        trvehicle:setInputWheeled(0, 0, tb, hand)
    end

    if manual then
        local g = tvehicle:getTransmissionGear()
        if K.isPressed("x") then tvehicle:setGear(min(g + 1, 8), 1.0) end
        if K.isPressed("z") then tvehicle:setGear(max(g - 1, -1), 1.0) end
    end

    for _, b in ipairs({ tractor, trailer }) do
        local vx, vy, vz = b:getVelocity()
        local s = sqrt(vx * vx + vz * vz)
        local k = 2.2 * s * dt
        b:applyImpulse(-vx * k, 0, -vz * k)
    end

    if thr > 0 then fuel = max(0, fuel - (0.004 + 0.006 * tvehicle:getEngineRpm() / 2200) * dt * 4) end
end

function crayon.update(dt)
    dt = min(dt, 0.05)
    msg_t = max(0, msg_t - dt)
    if K.isPressed("escape") then crayon.window.quit() end
    if K.isPressed("h") then toggle_hitch() end
    if K.isPressed("t") then toggle_lock() end
    if K.isPressed("e") then crane_action() end
    if K.isPressed("p") then parking = not parking end
    if K.isPressed("m") then
        manual = not manual
        tvehicle:setManualTransmission(manual); if manual then tvehicle:setGear(1, 1.0) end
    end
    if K.isPressed("c") then cam_mode = cam_mode % 4 + 1 end
    if K.isPressed("f1") then show_tel = not show_tel end
    if K.isPressed("f2") then show_dbg = not show_dbg end
    if K.isPressed("f3") then show_help = not show_help end
    if K.isPressed("r") then recover(K.isDown("lshift")) end
    if K.isPressed("f") then
        local x, _, z = tractor:getPosition()
        if sqrt((x - FUEL.x) ^ 2 + (z - FUEL.z) ^ 2) < 10 and tvehicle:getSpeedKmh() < 3 then
            local cost = math.floor((100 - fuel) * 3); fuel = 100; money = max(0, money - cost)
            say(string.format("Refueled  -$%d", cost), 3)
        else say("Drive to the fuel station and stop", 2.5) end
    end
    drive(dt)
    update_crane(dt)
    track_damage(dt)
end

-- ----------------------------------------------------------------- camera ---
local function update_camera()
    local px, py, pz = tractor:getPosition()
    local q = quat_of(tractor)
    local fx, fy, fz = qrot(q, 0, 0, 1)
    local ux, uy, uz = qrot(q, 0, 1, 0)
    local dt = crayon.time.getDt()
    local speed = tvehicle and tvehicle:getSpeedKmh() or 0
    cam.heading = angle_lerp(cam.heading, yaw_of(q), min(1, dt * 3))
    local hx, hz = sin(cam.heading), cos(cam.heading)
    cam.fov = lerp(cam.fov, 60 + clamp(speed * 0.1, 0, 14), min(1, dt * 2))
    local pos, tgt, fov = nil, nil, cam.fov
    if cam_mode == 1 then
        pos = { px - hx * 17, max(py + 6.5, 2.0), pz - hz * 17 }; tgt = { px + hx * 4, py + 1.8, pz + hz * 4 }
    elseif cam_mode == 2 then
        pos = { px + ux * 1.7 + fx * 1.9 + fz * 0.5, py + uy * 1.7 + fy * 1.9, pz + uz * 1.7 + fz * 1.9 - fx * 0.5 }
        tgt = { pos[1] + fx * 10, pos[2] + fy * 10, pos[3] + fz * 10 }; fov = 75
    elseif cam_mode == 3 then
        pos = { px - hz * 22 - hx * 8, py + 7, pz + hx * 22 - hz * 8 }; tgt = { px, py + 1.5, pz }
    else
        pos = { px, py + 60, pz - 10 }; tgt = { px, py, pz }
    end
    G.setCamera3D({ position = pos, target = tgt, up = { 0, 1, 0 }, fov = fov })
end

-- ------------------------------------------------------------------ draw ---
local function draw_vehicle_wheels(v, dual_from)
    for i = 0, v:getWheelCount() - 1 do
        local t = v:getWheelTransform(i)
        if t then
            local r = t.rotation
            draw_pose(t.position.x, t.position.y, t.position.z, { r.x, r.y, r.z, r.w }, function()
                if dual_from and i >= dual_from then
                    G.pushMatrix(); G.translate(0.20, 0, 0); G.drawModel(models.wheel, 0, 0, 0, 0, 0, 0, 1, 1, 1); G.popMatrix()
                    G.pushMatrix(); G.translate(-0.20, 0, 0); G.drawModel(models.wheel, 0, 0, 0, 0, 0, 0, 1, 1, 1); G.popMatrix()
                else
                    G.drawModel(models.wheel, 0, 0, 0, 0, 0, 0, 1, 1, 1)
                end
            end)
        end
    end
end

local function draw_depot(d, id)
    local active = job and (job.to == id or (not container and not job))
    G.setColor(0.2, 0.2, 0.22, 1); G.drawPlane(d.x, 0.02, d.z + 24, 22, 34)
    if active or in_depot(id) then G.setColor(1.0, 0.85, 0.1, 1) else G.setColor(0.8, 0.8, 0.3, 1) end
    G.drawCube(d.x - 9.2, 0.04, d.z + 24, 0.3, 0.06, 28); G.drawCube(d.x + 9.2, 0.04, d.z + 24, 0.3, 0.06, 28)
    G.drawCube(d.x, 0.04, d.z + 36, 18.4, 0.06, 0.3); G.drawCube(d.x, 0.04, d.z + 12, 18.4, 0.06, 0.3)
    G.setColor(0.85, 0.35, 0.1, 1)
    for _, sx in ipairs({ -1, 1 }) do
        G.drawCube(d.x + sx * 11, 7, d.z + 24, 0.8, 14, 0.8)
        G.drawCube(d.x + sx * 11, 14.2, d.z + 24, 1.0, 0.5, 18)
    end
    G.drawCube(d.x, 14.5, d.z + 24, 23, 0.8, 1.2)
    G.setColor(0.2, 0.2, 0.2, 1)
    G.drawCube(d.x, 14.0, d.z + 24, 1.5, 0.6, 1.5)
end

local function draw_crane_container()
    if crane.state == "idle" then return end
    local id = current_depot() or "A"
    local d = DEPOTS[id]
    local tx, ty, tz = trailer:getPosition()
    local q = quat_of(trailer)
    local t = clamp(crane.t / 3.0, 0, 1)
    local s = t * t * (3 - 2 * t)
    local lift = (crane.state == "unload") and s or (1 - s)
    local ox, oy, oz = qrot(q, 0, CONT_Y, 0)
    local hang = lift * 9.0
    G.setColor(1, 1, 1, 1)
    draw_pose(tx + ox, ty + oy + hang, tz + oz, q, function() G.drawModel(models["cont_" .. crane.color], 0, 0, 0, 0, 0, 0, 1, 1, 1) end)
    G.setColor(0.1, 0.1, 0.1, 1)
    G.drawCube(tx + ox, ty + oy + hang + 2.0 + (14 - (ty + oy + hang + 2.0)) / 2, tz + oz, 0.1, 14 - (ty + oy + hang + 2.0), 0.1)
end

local function draw_world()
    -- Grass ground: one textured plane, exactly like the car game.
    G.setColor(1, 1, 1, 1)
    G.drawPlane(0, 0, 0, GROUND_HALF * 2, GROUND_HALF * 2, textures.ground)

    -- Wooden crates (car-game style).
    for _, b in ipairs(crates) do
        if b:isValid() then
            local x, y, z = b:getPosition()
            G.setColor(1, 1, 1, 1)
            draw_pose(x, y, z, quat_of(b), function()
                G.drawCube(0, 0, 0, 1.5, 1.5, 1.5, textures.crate)
            end)
        end
    end

    -- Depots (yellow squares + gantry crane) -- needed for the crane mechanic.
    for id, d in pairs(DEPOTS) do draw_depot(d, id) end

    -- Fuel station (simplified pad + pump).
    G.setColor(0.3, 0.3, 0.35, 1); G.drawPlane(FUEL.x, 0.02, FUEL.z, 20, 20)
    G.setColor(0.9, 0.15, 0.1, 1); G.drawCube(FUEL.x + 7, 1.3, FUEL.z, 1.0, 2.6, 2.0)
    G.setColor(0.95, 0.95, 0.95, 1); G.drawCube(FUEL.x + 7, 3.6, FUEL.z, 5, 0.3, 6)
end

local function draw_units()
    G.setColor(1, 1, 1, 1)
    local tp = { tractor:getPosition() }
    draw_pose(tp[1], tp[2], tp[3], quat_of(tractor), function() G.drawModel(models.tractor, 0, 0, 0, 0, 0, 0, 1, 1, 1) end)
    draw_vehicle_wheels(tvehicle, 2)
    local rp = { trailer:getPosition() }
    draw_pose(rp[1], rp[2], rp[3], quat_of(trailer), function()
        G.drawModel(models.trailer, 0, 0, 0, 0, 0, 0, 1, 1, 1)
        if not hitch then G.drawModel(models.legs, 0, 0, 0, 0, 0, 0, 1, 1, 1) end
    end)
    draw_vehicle_wheels(trvehicle, 0)
    if container then
        local cp = { container.body:getPosition() }
        draw_pose(cp[1], cp[2], cp[3], quat_of(container.body), function()
            G.drawModel(models["cont_" .. container.color], 0, 0, 0, 0, 0, 0, 1, 1, 1)
        end)
    end
    draw_crane_container()
end

local function bar(x, y, w, h, f, r, g, b)
    G.setColor(0, 0, 0, 0.55); G.drawRect("fill", x - 1, y - 1, w + 2, h + 2)
    G.setColor(r, g, b, 1); G.drawRect("fill", x, y, w * clamp(f, 0, 1), h)
end

-- HUD scaled for 320x240.
local function draw_hud()
    local W, H = crayon.window.getResolution()
    if not (tvehicle and tvehicle:isValid()) then G.setColor(1, 0.3, 0.3, 1); G.drawText("Vehicle failed to build", 8, 8); return end
    local kmh, rpm, gear = tvehicle:getSpeedKmh(), tvehicle:getEngineRpm(), tvehicle:getTransmissionGear()
    local gtxt = gear < 0 and "R" or (gear == 0 and "N" or tostring(gear))

    G.setColor(1, 1, 1, 1)
    G.drawText(string.format("%3d", kmh), 10, H - 44, { scale = 1.6 })
    G.drawText("km/h", 52, H - 28, { scale = 0.6 })
    G.drawText(gtxt .. (manual and " M" or " A"), 78, H - 44, { scale = 1.3 })

    bar(10, H - 10, 90, 4, rpm / 2200, rpm > 1900 and 1 or 0.2, rpm > 1900 and 0.2 or 0.85, 0.2)
    bar(10, H - 18, 48, 3, fuel / 100, 0.95, 0.75, 0.1)
    G.setColor(0.8, 0.8, 0.8, 1); G.drawText("FUEL", 62, H - 20, { scale = 0.55 })
    if kmh > MAX_KMH - 3 then G.setColor(1, 0.8, 0.2, 1); G.drawText("LIMITER", 106, H - 14, { scale = 0.6 }) end
    if parking or K.isDown("space") then G.setColor(1, 0.2, 0.2, 1); G.drawText("(P) PARKING BRAKE", 106, H - 24, { scale = 0.6 }) end

    G.setColor(1, 0.95, 0.4, 1)
    G.drawText(string.format("$%d", money), W - 44, 8, { scale = 0.9 })

    G.setColor(0.9, 0.9, 1, 1)
    local hs = hitch and "COUPLED" or "UNCOUPLED"
    local ls = container and (container.lock and "LOCKED" or "UNLOCKED") or "NO CARGO"
    G.drawText(string.format("Trailer: %s  Container: %s", hs, ls), 8, 8, { scale = 0.6 })
    if job then
        G.drawText(string.format("Job: %s -> %s  ($%d)", job.color, DEPOTS[job.to].name, job.pay), 8, 18, { scale = 0.55 })
        G.setColor(1, 0.6, 0.3, 1)
        G.drawText(string.format("Cargo dmg: %d%%", math.floor(damage)), 8, 26, { scale = 0.55 })
    else
        G.drawText("No job - park trailer in depot + E", 8, 18, { scale = 0.55 })
    end

    if not hitch then
        local dh, dv, ya = hitch_status()
        if dh < 6 then
            local ok = dh < 0.9 and abs(dv) < 0.55 and ya < math.rad(25)
            if ok then G.setColor(0.3, 1, 0.4, 1) else G.setColor(1, 0.7, 0.2, 1) end
            G.drawText(string.format("Hitch %.1f m %s", dh, ok and "press H" or ""), W / 2 - 40, H - 60, { scale = 0.6 })
        end
    else
        local ang = math.deg((yaw_of(quat_of(trailer)) - yaw_of(quat_of(tractor)) + pi) % (2 * pi) - pi)
        if abs(ang) > 55 then G.setColor(1, 0.2, 0.2, 1); G.drawText(string.format("JACKKNIFE %ddeg", ang), W / 2 - 34, H - 60, { scale = 0.65 }) end
    end

    local dp = current_depot()
    if dp and crane.state == "idle" then
        G.setColor(0.4, 1, 0.5, 1)
        G.drawText(container and "[E] crane unload" or "[E] crane load", W / 2 - 30, H - 74, { scale = 0.55 })
    end
    if crane.state ~= "idle" then G.setColor(1, 0.9, 0.3, 1); G.drawText("CRANE...", W / 2 - 16, H - 74, { scale = 0.6 }) end
    if msg_t > 0 then G.setColor(1, 1, 1, 1); G.drawText(msg, 10, 38, { scale = 0.6 }) end

    if show_tel then
        G.setColor(0, 0, 0, 0.55); G.drawRect("fill", W - 120, 40, 116, 130)
        G.setColor(1, 1, 1, 1)
        for i = 0, tvehicle:getWheelCount() - 1 do
            local w = tvehicle:getWheelInfo(i)
            if w then G.drawText(string.format("T%d %s %.2f %+.2f/%+.2f",
                i + 1, w.contact and "G" or "A", w.suspension, w.longSlip, w.latSlip), W - 116, 44 + i * 11, { scale = 0.5 }) end
        end
        for i = 0, trvehicle:getWheelCount() - 1 do
            local w = trvehicle:getWheelInfo(i)
            if w then G.drawText(string.format("R%d %s %.2f",
                i + 1, w.contact and "G" or "A", w.suspension), W - 116, 100 + i * 11, { scale = 0.5 }) end
        end
    end

    if show_help then
        G.setColor(0, 0, 0, 0.55); G.drawRect("fill", 6, 40, 220, 96)
        G.setColor(0.95, 0.95, 0.95, 1)
        local lines = {
            "W/S drive  A/D steer  SPACE/P brake",
            "H hitch  T twist-lock  E crane",
            "M gearbox  Z/X shift  F refuel",
            "C camera  R recover  Shift+R depot",
            "F1 telemetry  F2 debug  F3 help",
        }
        for i, l in ipairs(lines) do G.drawText(l, 10, 46 + (i - 1) * 16, { scale = 0.55 }) end
    end
end

function crayon.draw()
    G.clear(0.55, 0.72, 0.92, 1)
    update_camera()
    G.setLight(-0.4, -1.0, -0.3, 1.0, 0.95, 0.85, 0.40, 0.40, 0.46)
    if not show_dbg then
        draw_world()
        draw_units()
    end
    if show_dbg then P.drawDebug({ vehicles = true, shapes = true, constraints = true }) end
    draw_hud()
end