-- ============================================================================
-- Minimal Vehicle Example
-- Controls: I = accelerate, K = reverse, J/L = steer, M = brake, R = reset
-- ============================================================================

local car = nil

local function spawn_car()
    -- Chassis: a simple box
    local chassis = crayon.physics3d.createBox(0, 1.2, 0, 1.0, 0.4, 2.0, "dynamic", 0.8, 0.2, 1200.0)

    -- Four wheels: front pair steers, rear pair drives
    local wheels = {
        { pos = {-1.1, -0.4,  1.4}, radius = 0.38, width = 0.25, isFront = true,  isDrive = false },
        { pos = { 1.1, -0.4,  1.4}, radius = 0.38, width = 0.25, isFront = true,  isDrive = false },
        { pos = {-1.1, -0.4, -1.4}, radius = 0.38, width = 0.25, isFront = false, isDrive = true  },
        { pos = { 1.1, -0.4, -1.4}, radius = 0.38, width = 0.25, isFront = false, isDrive = true  },
    }

    return crayon.physics3d.createWheeledVehicle({
        chassis = chassis,
        wheels = wheels,
        engineMaxTorque = 700.0,
        engineMinRpm = 900.0,
        engineMaxRpm = 7200.0
    })
end

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("Minimal Vehicle")

    -- Ground
    crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 100.0)

    -- Lighting
    crayon.graphics.setLight(-0.5, -0.9, -0.4, 1.0, 0.95, 0.85, 0.35, 0.35, 0.4)

	print(1)
    car = spawn_car()
	print(2)
end

function crayon.update(dt)
    -- Gather input
    local throttle = 0.0
    if crayon.input.isDown("i") then throttle =  1.0 end
    if crayon.input.isDown("k") then throttle = -0.8 end

    local steer = 0.0
    if crayon.input.isDown("j") then steer = -1.0 end
    if crayon.input.isDown("l") then steer =  1.0 end

    local brake = crayon.input.isDown("m") and 1.0 or 0.0

    -- Send to the vehicle
	print(3)
    if car and car:isValid() then
        car:setInputWheeled(throttle, steer, brake, false)
    end
	print(4)

    -- Reset with R
    if crayon.input.isPressed("r") then
        crayon.physics3d.destroyAll()
        crayon.physics3d.createPlane(0, 0, 0, 0, 1, 0, 100.0)
        car = spawn_car()
    end

    if crayon.input.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.12, 0.18)

    -- Fixed camera looking at origin
    crayon.graphics.setCamera3d({
        position = {10, 6, 10},
        target   = {0, 0, 0},
        up       = {0, 1, 0},
        fov      = 60.0
    })

    -- Draw the physics world (shows the chassis and wheels as wireframes)
    crayon.physics3d.drawDebug()

    -- HUD
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    if car and car:isValid() then
        crayon.graphics.drawText(string.format("Speed: %.1f km/h", car:getSpeedKmh()), 8, 8, 1.0)
        crayon.graphics.drawText(string.format("RPM:   %.0f", car:getEngineRpm()), 8, 20, 1.0)
        crayon.graphics.drawText(string.format("Gear:  %d", car:getTransmissionGear()), 8, 32, 1.0)
    end
    crayon.graphics.drawText("I: Fwd  K: Back  J/L: Steer  M: Brake  R: Reset", 8, 222, 1.0)
end