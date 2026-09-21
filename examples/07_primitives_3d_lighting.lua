-- ============================================================================
-- Example 07: 3D Primitives, Lighting, Spotlights & Shading Modes
-- Demonstrates: setCamera3d, drawPlane, drawCube, drawSphere, drawCylinder,
--               setLight (directional), setPointLight, setSpotLight,
--               setShadingMode (flat/gouraud/unlit), and drawSkyGradient.
-- ============================================================================

local camAngle = 0.0
local camDist = 12.0
local shadingModes = { "gouraud", "flat", "unlit" }
local currentShadingIndex = 1

function crayon.init()
    crayon.window.setTitle("07 - 3D Primitives & Lighting")
    crayon.window.setResolution(640, 480)
end

function crayon.update(dt)
    -- Orbit camera around center
    camAngle = camAngle + 25.0 * dt
    local rad = math.rad(camAngle)
    local camX = math.sin(rad) * camDist
    local camZ = math.cos(rad) * camDist

    crayon.graphics.setCamera3d({
        position = { camX, 6.0, camZ },
        target = { 0.0, 1.0, 0.0 },
        up = { 0.0, 1.0, 0.0 },
        fov = 50.0
    })

    -- Cycle shading modes with 'M' key
    if crayon.input.isKeyPressed("m") then
        currentShadingIndex = (currentShadingIndex % #shadingModes) + 1
        crayon.graphics.setShadingMode(shadingModes[currentShadingIndex])
    end
end

function crayon.draw()
    -- Clear and draw atmospheric sky gradient
    crayon.graphics.clear(0.05, 0.06, 0.1, 1.0)
    crayon.graphics.drawSkyGradient(0.12, 0.18, 0.32, 0.03, 0.04, 0.08)

    -- 1. Configure Directional Sunlight
    crayon.graphics.setLight(
        -0.5, -1.0, -0.7,   -- Direction vector
        1.0, 0.95, 0.85,    -- Sun color
        0.25, 0.25, 0.35    -- Ambient color
    )

    -- 2. Configure Point Light (Orbiting golden lamp)
    local lampX = math.cos(crayon.window.getTime() * 1.5) * 3.5
    local lampZ = math.sin(crayon.window.getTime() * 1.5) * 3.5
    crayon.graphics.setPointLight(
        1,                  -- Light index 1
        lampX, 2.5, lampZ,  -- Position (x, y, z)
        1.0, 0.7, 0.2,      -- Color (r, g, b)
        10.0,               -- Radius
        2.0                 -- Intensity
    )
    crayon.graphics.setPointLightEnabled(1, true)

    -- 3. Configure Spotlight (Cyan flashlight shining down at center)
    crayon.graphics.setSpotLight(
        1,                  -- Spot index 1
        0.0, 7.0, 0.0,      -- Position
        0.0, -1.0, 0.0,     -- Direction (downward)
        0.2, 0.9, 1.0,      -- Color
        12.0,               -- Radius
        3.0,                -- Intensity
        15.0,               -- Inner cutoff angle (degrees)
        30.0                -- Outer cutoff angle (degrees)
    )
    crayon.graphics.setSpotLightEnabled(1, true)

    -- 4. Draw Ground Plane
    crayon.graphics.setColor(0.35, 0.4, 0.5, 1.0)
    crayon.graphics.drawPlane(0.0, 0.0, 0.0, 20.0, 20.0)

    -- 5. Draw 3D Primitives Showcase
    -- Central rotating Cube
    local rotY = crayon.window.getTime() * 40.0
    crayon.graphics.setColor(0.9, 0.3, 0.35, 1.0)
    crayon.graphics.drawCube(0.0, 1.0, 0.0, 1.5, 1.5, 1.5, nil, 0.0, rotY, 0.0)

    -- Smooth Sphere
    crayon.graphics.setColor(0.2, 0.85, 0.45, 1.0)
    crayon.graphics.drawSphere(-3.5, 1.0, 0.0, 0.9)

    -- Cylinder Column
    crayon.graphics.setColor(0.25, 0.6, 0.95, 1.0)
    crayon.graphics.drawCylinder(3.5, 1.0, 0.0, 0.8, 2.0)

    -- Visual marker for orbiting point light
    crayon.graphics.setColor(1.0, 0.8, 0.2, 1.0)
    crayon.graphics.drawSphere(lampX, 2.5, lampZ, 0.2)

    -- 6. HUD Instructions
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("3D Primitives, Lighting & Spotlights", 20, 20, { scale = 1.5 })
    crayon.graphics.setColor(0.7, 0.8, 0.9, 1.0)
    crayon.graphics.drawText("Press [M] to cycle shading mode: " .. string.upper(shadingModes[currentShadingIndex]), 20, 50)
    crayon.graphics.drawText("Point Light (Gold) orbiting | Spotlight (Cyan) casting from top", 20, 72)
    crayon.graphics.drawText("FPS: " .. math.floor(crayon.window.getFps()), 560, 20)
end
