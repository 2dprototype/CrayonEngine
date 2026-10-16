-- Example: AirStrike Gunship Mission Loader
-- Demonstrates asynchronous mission loading with a smooth 60 FPS loading screen.

function crayon.config(t)
    t.window.title = "AirStrike Gunship - Asset Loader Demo"

    t.modules.mesh3D = true
    t.plugins.assets = true
end

local gameState = "loading"
local currentMission = "mission_01_desert"
local rotorAngle = 0.0

function crayon.init()
    -- 1. Define Common Assets
    crayon.assets.define("common", {
        models = {
            { id = "cube_mesh", path = "cube" }
        }
    })

    -- 2. Define Mission 1: Desert Strike
    crayon.assets.define("mission_01_desert", {
        models = {
            { id = "gunship", path = "cube" },
            { id = "target1", path = "pyramid" },
            { id = "target2", path = "cylinder" }
        }
    })

    -- 3. Define Mission 2: Ocean Base
    crayon.assets.define("mission_02_ocean", {
        models = {
            { id = "carrier", path = "cube" },
            { id = "jet",     path = "cone" },
            { id = "buoy",    path = "sphere" }
        }
    })

    -- Set loading budget: spend up to 8ms per frame on loading
    crayon.assets.setBudget(8.0)

    -- Start loading Mission 1
    crayon.assets.loadSync("common")
    crayon.assets.loadAsync("mission_01_desert")
    gameState = "loading"
end

function crayon.update(dt)
    rotorAngle = rotorAngle + dt * 10.0

    if gameState == "loading" then
        if crayon.assets.isDone() then
            gameState = "gameplay"
        end
    elseif gameState == "gameplay" then
        -- Press Space to switch to Mission 2 or back to Mission 1
        if crayon.key.isPressed("space") then
            if currentMission == "mission_01_desert" then
                crayon.assets.unload("mission_01_desert")
                currentMission = "mission_02_ocean"
                crayon.assets.loadAsync("mission_02_ocean")
                gameState = "loading"
            else
                crayon.assets.unload("mission_02_ocean")
                currentMission = "mission_01_desert"
                crayon.assets.loadAsync("mission_01_desert")
                gameState = "loading"
            end
        end
    end
end

function crayon.draw()
    if gameState == "loading" then
        -- Animated Loading Screen
        crayon.graphics.clear(0.08, 0.1, 0.15)
        
        local pct = crayon.assets.getPercent()
        local item = crayon.assets.getCurrentItem()
        local loaded, total = crayon.assets.getCount()

        crayon.graphics.setColor(1, 0.8, 0.2, 1)
        crayon.graphics.drawText("AIRSTRIKE GUNSHIP", 160, 80)

        crayon.graphics.setColor(0.9, 0.9, 0.9, 1)
        crayon.graphics.drawText("PREPARING MISSION: " .. currentMission, 100, 130)

        -- Progress bar outline
        crayon.graphics.setColor(0.3, 0.4, 0.5, 1)
        crayon.graphics.drawRect("line", 90, 180, 300, 24)

        -- Progress bar fill
        local barWidth = 296 * (pct / 100.0)
        if barWidth > 0 then
            crayon.graphics.setColor(0.2, 0.8, 0.4, 1)
            crayon.graphics.drawRect("fill", 92, 182, barWidth, 20)
        end

        -- Progress text
        crayon.graphics.setColor(1, 1, 1, 1)
        crayon.graphics.drawText(pct .. "%  (" .. loaded .. " / " .. total .. ")", 210, 186)

        if item ~= "" then
            crayon.graphics.setColor(0.6, 0.6, 0.7, 1)
            crayon.graphics.drawText("Loading: " .. item, 90, 220)
        end

        -- Animated loading icon (spinning rotor)
        crayon.graphics.setColor(0.3, 0.8, 1.0, 1)
        local cx, cy = 240, 280
        local rx = math.cos(rotorAngle) * 20
        local ry = math.sin(rotorAngle) * 20
        crayon.graphics.drawLine(cx - rx, cy - ry, cx + rx, cy + ry)
        crayon.graphics.drawLine(cx + ry, cy - rx, cx - ry, cy + rx)

    elseif gameState == "gameplay" then
        -- Gameplay View
        if currentMission == "mission_01_desert" then
            crayon.graphics.clear(0.18, 0.14, 0.08) -- Desert sand
        else
            crayon.graphics.clear(0.04, 0.12, 0.22) -- Ocean blue
        end

        crayon.graphics.setColor(1, 1, 1, 1)
        crayon.graphics.drawText("MISSION ACTIVE: " .. currentMission, 20, 20)
        crayon.graphics.drawText("Loaded Bundles: " .. table.concat(crayon.assets.getLoadedBundles(), ", "), 20, 40)
        crayon.graphics.setColor(0.2, 1.0, 0.4, 1)
        crayon.graphics.drawText("[PRESS SPACE] Switch Mission (Unloads previous & loads next)", 20, 320)

        -- Draw 3D scene using loaded models
        crayon.graphics.setCamera3D({
            position = {0, 4, 8},
            target   = {0, 0, 0},
            up       = {0, 1, 0},
            fov      = 55
        })

        if currentMission == "mission_01_desert" then
            local gunship = crayon.assets.getModel("gunship")
            if gunship then
                crayon.graphics.drawModel(gunship, 0, 0, 0, 1.5, 0, rotorAngle * 10, 0)
            end
        else
            local carrier = crayon.assets.getModel("carrier")
            if carrier then
                crayon.graphics.drawModel(carrier, 0, 0, 0, 2.0, 0, rotorAngle * 10, 0)
            end
        end
    end
end
