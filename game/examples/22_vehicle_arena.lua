-- ============================================================================
-- Vehicle Physics & Driving Arena
-- Controls:
--   W / Up     : Throttle / Accelerate
--   S / Down   : Reverse / Brake
--   A / Left   : Steer Left
--   D / Right  : Steer Right
--   SPACE      : Handbrake
--   R          : Reset Vehicle to origin
--   F1         : Toggle Debug Wireframe Draw
--   Right Drag : Orbit Camera around Vehicle
--   Scroll     : Zoom Camera
-- ============================================================================

local car = nil
local chassis_body = nil
local ground_body = nil
local arena_props = {}

local show_debug = true
local cam = {
    yaw = 180.0,
    pitch = 20.0,
    dist = 8.0,
    target_x = 0, target_y = 1, target_z = 0,
    curr_x = 0, curr_y = 3, curr_z = 8
}

local function spawn_vehicle(x, y, z)
    -- 1. Create Chassis Rigidbody: 2.0m wide, 0.8m high, 4.2m long
    chassis_body = crayon.physics3d.createBox(
        x, y, z,
        0.95, 0.35, 2.0,       -- half-extents (sx=1.9, sy=0.7, sz=4.0)
        "dynamic",
        0.6, 0.1, 1400.0       -- friction, restitution, density (mass ~1400kg)
    )

    -- 2. Define 4 Wheels with suspension & drive flags
    -- Local offsets relative to chassis center
    local half_w = 1.05
    local half_l = 1.45
    local wheel_y = -0.25
    local wheel_r = 0.40
    local wheel_w = 0.28

    local wheels = {
        -- Front Left (Steering)
        {
            pos = {-half_w, wheel_y, half_l},
            radius = wheel_r, width = wheel_w,
            suspensionMinLength = 0.15,
            suspensionMaxLength = 0.50,
            suspensionSpring = 32000.0,
            suspensionDamping = 3000.0,
            maxSteerAngleRad = 0.55,
            maxBrakeTorque = 2500.0,
            maxHandBrakeTorque = 0.0,
            isFront = true,
            isDrive = false
        },
        -- Front Right (Steering)
        {
            pos = {half_w, wheel_y, half_l},
            radius = wheel_r, width = wheel_w,
            suspensionMinLength = 0.15,
            suspensionMaxLength = 0.50,
            suspensionSpring = 32000.0,
            suspensionDamping = 3000.0,
            maxSteerAngleRad = 0.55,
            maxBrakeTorque = 2500.0,
            maxHandBrakeTorque = 0.0,
            isFront = true,
            isDrive = false
        },
        -- Rear Left (Drive + Handbrake)
        {
            pos = {-half_w, wheel_y, -half_l},
            radius = wheel_r, width = wheel_w,
            suspensionMinLength = 0.15,
            suspensionMaxLength = 0.50,
            suspensionSpring = 35000.0,
            suspensionDamping = 3200.0,
            maxSteerAngleRad = 0.0,
            maxBrakeTorque = 1800.0,
            maxHandBrakeTorque = 5000.0,
            isFront = false,
            isDrive = true
        },
        -- Rear Right (Drive + Handbrake)
        {
            pos = {half_w, wheel_y, -half_l},
            radius = wheel_r, width = wheel_w,
            suspensionMinLength = 0.15,
            suspensionMaxLength = 0.50,
            suspensionSpring = 35000.0,
            suspensionDamping = 3200.0,
            maxSteerAngleRad = 0.0,
            maxBrakeTorque = 1800.0,
            maxHandBrakeTorque = 5000.0,
            isFront = false,
            isDrive = true
        }
    }

    return crayon.physics3d.createWheeledVehicle({
        chassis = chassis_body,
        wheels = wheels,
        engineMaxTorque = 650.0,
        engineMinRpm = 800.0,
        engineMaxRpm = 6500.0,
        maxPitchRollAngle = 3.14159
    })
end

local function reset_arena()
    crayon.physics3d.destroyAll()
    arena_props = {}

    -- Static Ground Plane
    ground_body = crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 150.0)

    -- Stunt Jump Ramp (slanted box)
    local ramp = crayon.physics3d.createBox(0, 1.2, 25.0, 4.0, 0.4, 3.5, "static", 0.8, 0.1)
    ramp:setRotation(math.rad(-18.0), 0, 0, 1) -- slant ramp up
    table.insert(arena_props, { body = ramp, col = {0.8, 0.6, 0.2} })

    -- Obstacle traffic cones / cubes to knock over
    for i = -4, 4, 2 do
        local box = crayon.physics3d.createBox(i * 1.8, 0.5, 12.0, 0.4, 0.5, 0.4, "dynamic", 0.6, 0.3, 30.0)
        table.insert(arena_props, { body = box, col = {0.9, 0.3, 0.2} })
    end

    -- Spawn the vehicle at origin
    car = spawn_vehicle(0, 1.2, 0)
end

function crayon.init()
    crayon.window.setTitle("Crayon Engine - Vehicle Physics & Driving Arena")

    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.setShadingMode("gouraud")
    crayon.graphics.setLight(-0.4, -0.9, -0.5, 1.0, 0.95, 0.88, 0.35, 0.35, 0.4)

    reset_arena()
end

function crayon.update(dt)
    -- Advance the physics simulation (CRITICAL FIX)
    -- crayon.physics3d.update(dt)

    -- Gather Vehicle Controls
    local throttle = 0.0
    if crayon.input.isDown("w") or crayon.input.isDown("up") then
        throttle = 1.0
    elseif crayon.input.isDown("s") or crayon.input.isDown("down") then
        throttle = -0.8
    end

    local steer = 0.0
    if crayon.input.isDown("a") or crayon.input.isDown("left") then
        steer = -1.0
    elseif crayon.input.isDown("d") or crayon.input.isDown("right") then
        steer = 1.0
    end

    local brake = (throttle < 0 and 0.0) or (crayon.input.isDown("s") and 1.0 or 0.0)
    local handbrake = crayon.input.isDown("space")

    -- Send driver inputs to vehicle constraint
    if car and car:isValid() then
        car:setInputWheeled(throttle, steer, brake, handbrake)
    end

    -- Reset vehicle
    if crayon.input.isPressed("r") then
        reset_arena()
    end

    -- Toggle debug draw
    if crayon.input.isPressed("f1") then
        show_debug = not show_debug
    end

    -- Mouse orbit camera controls
    if crayon.input.isDown("mouse_right") or crayon.input.isDown("mouse_middle") then
        local dx, dy = crayon.input.getMouseDelta()
        cam.yaw = cam.yaw - dx * 0.35
        cam.pitch = math.max(5.0, math.min(80.0, cam.pitch - dy * 0.25))
    end

    local wheel = crayon.input.getMouseWheel()
    if wheel ~= 0 then
        cam.dist = math.max(3.0, math.min(25.0, cam.dist - wheel * 0.8))
    end

    -- Smooth Camera Follow
    if chassis_body and chassis_body:isValid() then
        local px, py, pz = chassis_body:getPosition()
        cam.target_x = cam.target_x + (px - cam.target_x) * math.min(1.0, dt * 10.0)
        cam.target_y = cam.target_y + (py + 0.8 - cam.target_y) * math.min(1.0, dt * 10.0)
        cam.target_z = cam.target_z + (pz - cam.target_z) * math.min(1.0, dt * 10.0)

        local rad_yaw = math.rad(cam.yaw)
        local rad_pitch = math.rad(cam.pitch)
        local desired_x = cam.target_x - cam.dist * math.cos(rad_pitch) * math.sin(rad_yaw)
        local desired_y = cam.target_y + cam.dist * math.sin(rad_pitch)
        local desired_z = cam.target_z - cam.dist * math.cos(rad_pitch) * math.cos(rad_yaw)

        cam.curr_x = cam.curr_x + (desired_x - cam.curr_x) * math.min(1.0, dt * 14.0)
        cam.curr_y = cam.curr_y + (desired_y - cam.curr_y) * math.min(1.0, dt * 14.0)
        cam.curr_z = cam.curr_z + (desired_z - cam.curr_z) * math.min(1.0, dt * 14.0)
    end
end

function crayon.draw()
    crayon.graphics.clear(0.12, 0.14, 0.2, 1.0)

    -- Setup 3D Camera
    crayon.graphics.setCamera3d({
        position = {cam.curr_x, cam.curr_y, cam.curr_z},
        target   = {cam.target_x, cam.target_y, cam.target_z},
        up       = {0, 1, 0},
        fov      = 60.0,
        near     = 0.1,
        far      = 200.0
    })

    -- Draw Ground Grid
    crayon.graphics.setColor(0.3, 0.35, 0.45, 1.0)
    crayon.graphics.drawGrid3d(100.0, 50, 0.0)

    -- Draw Solid Vehicle Car Body & Wheels
    if chassis_body and chassis_body:isValid() then
        local px, py, pz = chassis_body:getPosition()
        local rx, ry, rz = chassis_body:getRotation()

        -- Isolate transformations for the chassis
        crayon.graphics.pushMatrix()
        crayon.graphics.translate(px, py, pz)
        -- Apply Euler rotations
        crayon.graphics.rotate(ry, 0, 1, 0)
        crayon.graphics.rotate(rx, 1, 0, 0)
        crayon.graphics.rotate(rz, 0, 0, 1)

        -- Chassis main hull (Blue Sports Car)
        crayon.graphics.setColor(0.2, 0.55, 0.95, 1.0)
        crayon.graphics.drawCube(0, 0, 0, 1.9, 0.7, 4.0)

        -- Cabin / Cockpit Roof (offset relative to the center)
        crayon.graphics.setColor(0.15, 0.2, 0.3, 1.0)
        crayon.graphics.drawCube(0, 0.55, -0.2, 1.5, 0.5, 2.0)
        
        crayon.graphics.popMatrix()

        -- Draw Wheels at simulated suspension positions
        if car and car:isValid() then
            local wheel_count = car:getWheelCount()
            for i = 0, wheel_count - 1 do
                local wheel = car:getWheelTransform(i)
                if wheel then
                    local wx = wheel.position.x
                    local wy = wheel.position.y
                    local wz = wheel.position.z
                    
                    crayon.graphics.setColor(0.15, 0.15, 0.15, 1.0)
                    -- Convert 90 degrees to radians to properly orient the cylinder as a wheel
                    crayon.graphics.drawCylinder(wx, wy, wz, 0.40, 0.28, nil, 0, 0, math.rad(90))
                end
            end
        end
    end

    -- Draw Physics Debug Overlay (shows suspension springs & collision rays)
    if show_debug then
        crayon.physics3d.drawDebug()
    end

    -- Draw HUD
    crayon.graphics.resetCamera2d()
    crayon.graphics.setColor(0, 0, 0, 0.6)
    crayon.graphics.drawRect("fill", 10, 10, 270, 120)

    crayon.graphics.setColor(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.drawText("=== VEHICLE DRIVING ARENA ===", 20, 18, 1)

    crayon.graphics.setColor(1, 1, 1, 1)
    if car and car:isValid() then
        crayon.graphics.drawText(string.format("Speed: %.1f km/h", car:getSpeedKmh()), 20, 36, 1)
        crayon.graphics.drawText(string.format("RPM:   %.0f", car:getEngineRpm()), 20, 52, 1)
        crayon.graphics.drawText(string.format("Gear:  %d", car:getTransmissionGear()), 20, 68, 1)
    end
    crayon.graphics.drawText(string.format("FPS:   %d", crayon.window.getFps()), 20, 84, 1)

    crayon.graphics.setColor(0.75, 0.85, 0.95, 1.0)
    crayon.graphics.drawText("WASD: Drive  |  SPACE: Handbrake  |  R: Reset", 20, 102, 1)
    crayon.graphics.drawText("F1: Toggle Physics Wireframe  |  Right Drag: Cam", 20, 116, 1)
end