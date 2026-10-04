-- tint_test.lua
-- Exercises crayon.graphics.setModelColor / getModelColor,
-- model:setPartColor / getPartColor, and verifies no tint bleed.

local cam_yaw = 0.0
local cam_dist = 12.0

-- Loaded once in init
local char_model = nil
local part_count = 0

function crayon.config(t)
	t.window.width = 640
	t.window.height = 360
end

function crayon.init()
    crayon.window.setResolution(640, 360)
    crayon.window.setWindowSize(640, 360)
    crayon.window.setTitle("Tint Test - arrow keys to orbit, 1-9 to swap test")
    crayon.window.setScalingMode("integer")

    crayon.graphics.setRetroEffects({
        jitterResolution = { 320, 180 },
        affine = 0,
        fog = { startDist = 15, endDist = 60, color = { 0.08, 0.08, 0.12 } }
    })

    crayon.graphics.setLight(0.4, -1.0, 0.5,
                             1.0, 0.95, 0.9,
                             0.30, 0.30, 0.35)

    -- Try to load a skinned / multipart model if present; otherwise
    -- fall back to a plain cube so the test still runs.
    char_model = crayon.graphics.loadModel("assets/models/cube_gltf/Cube.glb")
    if not char_model:isValid() then
        char_model = crayon.graphics.loadModel("cube")
    end
    part_count = char_model:getPartCount()

    -- Seed each part with a distinct color so we can see per-part tinting.
    for i = 0, part_count - 1 do
        local r = ((i * 37) % 100) / 100
        local g = ((i * 63) % 100) / 100
        local b = ((i * 91) % 100) / 100
        char_model:setPartColor(i, r, g, b, 1.0)
    end

    print(("Loaded model: %d parts, %d nodes, skinned=%s")
        :format(part_count, char_model:getNodeCount(), tostring(char_model:isSkinned())))
end

function crayon.update(dt)
    if crayon.key.isDown("left")  then cam_yaw = cam_yaw - dt * 1.5 end
    if crayon.key.isDown("right") then cam_yaw = cam_yaw + dt * 1.5 end
    if crayon.key.isDown("up")    then cam_dist = math.max(3, cam_dist - dt * 4) end
    if crayon.key.isDown("down")  then cam_dist = math.min(30, cam_dist + dt * 4) end

    -- Space: pull the current part color and log it (proves getPartColor works)
    if crayon.key.isPressed("space") then
        for i = 0, part_count - 1 do
            local r, g, b, a = char_model:getPartColor(i)
            print(("part %d: %.2f %.2f %.2f %.2f"):format(i, r, g, b, a))
        end
    end

    -- Enter: log the current frame tint from the renderer
    if crayon.key.isPressed("enter") then
        local r, g, b, a = crayon.graphics.getModelColor()
        print(("modelColor: %.2f %.2f %.2f %.2f"):format(r, g, b, a))
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.08, 0.12)

    local cx = math.sin(cam_yaw) * cam_dist
    local cz = math.cos(cam_yaw) * cam_dist
    crayon.graphics.setCamera3D({
        position = { cx, 4.0, cz },
        target   = { 0.0, 1.0, 0.0 },
        fov      = 55.0
    })

    -- -------------------------------------------------------------------
    -- Row 1: procedural primitives, each with its own setModelColor.
    -- If tint is wired correctly, these are four distinct colors.
    -- -------------------------------------------------------------------
    crayon.graphics.setModelColor(1.0, 0.2, 0.2)     -- red
    crayon.graphics.drawCube(-3.0, 0.5, 0.0, 1.0, 1.0, 1.0)

    crayon.graphics.setModelColor(0.2, 1.0, 0.2)     -- green
    crayon.graphics.drawSphere(-1.0, 0.5, 0.0, 0.5)

    crayon.graphics.setModelColor(0.2, 0.4, 1.0)     -- blue
    crayon.graphics.drawCylinder(1.0, 0.5, 0.0, 0.3, 1.0)

    crayon.graphics.setModelColor(1.0, 1.0, 0.2)     -- yellow
    crayon.graphics.drawCone(3.0, 0.5, 0.0, 0.3, 1.0)

    -- Reset: subsequent primitives are white again.
    crayon.graphics.setModelColor(1.0, 1.0, 1.0)
    crayon.graphics.drawPlane(0.0, -0.01, 0.0, 12.0, 12.0)

    -- -------------------------------------------------------------------
    -- Row 2: billboard AFTER a tinted draw. If the reset obligation in
    -- draw_billboard is missing, this billboard comes out the wrong color.
    -- It must be pure white regardless of what setModelColor is above it.
    -- -------------------------------------------------------------------
    crayon.graphics.setModelColor(1.0, 0.2, 0.2)     -- deliberately tint red
    crayon.graphics.drawCube(-3.0, 2.5, 0.0, 0.8, 0.8, 0.8)

    -- No reset here on purpose: the engine must reset u_color internally
    -- for billboards. If the billboard below is reddish, the reset is missing.
    crayon.graphics.drawBillboard(0, 0.0, 2.5, 0.0, 1.0, 1.0)

    -- Texture-less line right after — also must be white
    crayon.graphics.drawLine3D(-4.0, 3.0, -4.0, 4.0, 3.0, 4.0)

    -- Reset for the rest of the frame
    crayon.graphics.setModelColor(1.0, 1.0, 1.0)

    -- -------------------------------------------------------------------
    -- Row 3: loaded model. Uses per-part colors set in init(), not the
    -- renderer's ambient tint. It must show its own colors regardless of
    -- what setModelColor was last set to.
    -- -------------------------------------------------------------------
    if char_model and char_model:isValid() then
        crayon.graphics.drawModel(char_model, 0.0, 0.5, -3.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    end

    -- -------------------------------------------------------------------
    -- Row 4: model drawn with an override texture still uses part.color.
    -- Swap part 0's color live to prove setPartColor propagates at draw time.
    -- -------------------------------------------------------------------
    if char_model and char_model:isValid() and part_count > 0 then
        local t = crayon.time.getTime()
        local pulse = 0.5 + 0.5 * math.sin(t * 3.0)      -- 0..1
        char_model:setPartColor(0, pulse, 1.0 - pulse, 0.5, 1.0)

        crayon.graphics.drawModel(char_model, 3.0, 0.5, -3.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
    end

    -- -------------------------------------------------------------------
    -- HUD
    -- -------------------------------------------------------------------
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("Row 1: red/green/blue/yellow cubes+spheres", 4, 4, 1)
    crayon.graphics.drawText("Row 2: white billboard + line (must NOT be red)", 4, 16, 1)
    crayon.graphics.drawText("Row 3: model using its own part colors", 4, 28, 1)
    crayon.graphics.drawText("Row 4: part 0 pulses every frame", 4, 40, 1)
    crayon.graphics.drawText("Arrows orbit | space dumps part colors | enter dumps tint", 4, 52, 1)
end

function crayon.quit()
    print("Tint test done.")
end