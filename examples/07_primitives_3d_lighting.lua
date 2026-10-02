-- ============================================================================
-- Example 07: 3D Primitives, Lighting, Spotlights & Shading Modes
-- Demonstrates: setCamera3d, drawPlane, drawCube, drawSphere, drawCylinder,
--               setLight (directional), setPointLight, setSpotLight,
--               setShadingMode (flat/gouraud/unlit), depth-tested 3D pass.
-- ============================================================================

function crayon.config(config)
    config.window.title = "07 - 3D Primitives & Lighting"
    config.window.width = 640
    config.window.height = 480
    config.window.virtualWidth = 640
    config.window.virtualHeight = 480
    config.modules.physics3d = false
    config.modules.mesh3D = true
end

-- ----------------------------------------------------------------------------
-- Persistent demo state. Stored on _G so it cannot be lost to file-scope
-- local-scope issues (e.g. a partial paste dropping the `local` keyword, or
-- a reload discarding the previous chunk's upvalues).
-- ----------------------------------------------------------------------------
Demo07 = Demo07 or {
    camAngle      = 0.0,
    camDist       = 12.0,
    shadingModes  = { "gouraud", "flat", "unlit" },
    shadingIndex  = 1,
}

function crayon.init()
    crayon.graphics.setShadingMode(Demo07.shadingModes[Demo07.shadingIndex])
end

function crayon.update(dt)
    Demo07.camAngle = Demo07.camAngle + 25.0 * dt
    local rad = math.rad(Demo07.camAngle)
    local camX = math.sin(rad) * Demo07.camDist
    local camZ = math.cos(rad) * Demo07.camDist

    crayon.graphics.setCamera3d({
        position = { camX, 6.0, camZ },
        target   = { 0.0, 1.0, 0.0 },
        up       = { 0.0, 1.0, 0.0 },
        fov      = 50.0
    })

    if crayon.input.isKeyPressed("m") then
        Demo07.shadingIndex = (Demo07.shadingIndex % #Demo07.shadingModes) + 1
        crayon.graphics.setShadingMode(Demo07.shadingModes[Demo07.shadingIndex])
    end
end

function crayon.draw()
    local t = crayon.time.getTime()

    -- 1. Clear colour + depth
    crayon.graphics.clear(0.10, 0.13, 0.22, 1.0)

    -- 2. Depth state for the 3D pass
    crayon.graphics.setDepthTest(true)
    crayon.graphics.setDepthWrite(true)

    -- 3. Directional sunlight
    crayon.graphics.setLight(
        -0.5, -1.0, -0.7,
        1.0, 0.95, 0.85,
        0.25, 0.25, 0.35
    )

    -- 4. Orbiting point light
    local lampX = math.cos(t * 0.9) * 3.5
    local lampZ = math.sin(t * 0.9) * 3.5
    crayon.graphics.setPointLight(
        1,
        lampX, 2.5, lampZ,
        1.0, 0.7, 0.2,
        10.0,
        2.0
    )
    crayon.graphics.setPointLightEnabled(1, true)

    -- 5. Cyan spotlight from above
    --    Signature: (idx, px,py,pz, dx,dy,dz, radius, r,g,b, intensity, innerDeg, outerDeg)
    crayon.graphics.setSpotLight(
        1,
        0.0, 7.0, 0.0,
        0.0, -1.0, 0.0,
        12.0,
        0.2, 0.9, 1.0,
        3.0,
        15.0,
        30.0
    )
    crayon.graphics.setSpotLightEnabled(1, true)

    -- 6. Ground plane
    crayon.graphics.setColor(0.35, 0.4, 0.5, 1.0)
    crayon.graphics.drawPlane(0.0, 0.0, 0.0, 20.0, 20.0)

    -- 7. Showcase primitives
    local rotY = t * 1.2
    crayon.graphics.setColor(0.9, 0.3, 0.35, 1.0)
    crayon.graphics.drawCube(0.0, 1.0, 0.0, 1.5, 1.5, 1.5, nil, 0.0, rotY, 0.0)

    crayon.graphics.setColor(0.2, 0.85, 0.45, 1.0)
    crayon.graphics.drawSphere(-3.5, 1.0, 0.0, 0.9)

    -- Cylinder lifted a hair above the plane to avoid z-fighting
    crayon.graphics.setColor(0.25, 0.6, 0.95, 1.0)
    crayon.graphics.drawCylinder(3.5, 1.01, 0.0, 0.8, 2.0)

    -- Light-bulb marker
    crayon.graphics.setColor(1.0, 0.8, 0.2, 1.0)
    crayon.graphics.drawSphere(lampX, 2.5, lampZ, 0.2)

    -- 8. HUD pass
    crayon.graphics.setDepthTest(false)
    crayon.graphics.setDepthWrite(false)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("3D Primitives, Lighting & Spotlights", 20, 20, { scale = 1.5 })

    crayon.graphics.setColor(0.7, 0.8, 0.9, 1.0)
    crayon.graphics.drawText(
        "Press [M] to cycle shading mode: "
            .. string.upper(Demo07.shadingModes[Demo07.shadingIndex]),
        20, 50
    )
    crayon.graphics.drawText(
        "Point Light (Gold) orbiting | Spotlight (Cyan) casting from top",
        20, 72
    )
    crayon.graphics.drawText("FPS: " .. math.floor(crayon.window.getFps()), 560, 20)
end