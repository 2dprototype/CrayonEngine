-- Simple UI Demonstration: AirStrike Gunship Tactical Avionics & Mission Control
-- Demonstrates full-featured native UI (Tabs, Combos, Tooltips, Modals, Tables, Transparent HUD)
-- Immune to retro CRT scanlines and post-process shaders. Strictly camelCase.

local showMissionControl = true
local showScrambleModal = false

local playerName = "Ghost-1"
local weaponList = { "AGM-114 Hellfire", "Hydra 70 Rockets", "AIM-9X Sidewinder", "GAU-19 .50 Cal Gatling" }
local selectedWeapon = 1
local flareCount = 30
local flaresDeployed = 0
local masterArm = true

local throttle = 0.85
local altitude = 4250
local airspeed = 185.0
local fov = 75.0
local soundVolume = 80
local hudTint = { 0.2, 0.8, 1.0 }

local targets = {
    { callsign = "BOGEY-ALPHA",  dist = "12.4 km", status = "HOSTILE",  threat = "HIGH" },
    { callsign = "CONVOY-DELTA", dist = "6.1 km",  status = "TARGET",   threat = "MEDIUM" },
    { callsign = "OUTPOST-ZULU", dist = "18.9 km", status = "TARGET",   threat = "LOW" },
    { callsign = "VIPER-LEAD",   dist = "4.2 km",  status = "FRIENDLY", threat = "NONE" },
}

function crayon.config(t)
    t.identity = "AirStrikeGunship"
    t.version = "1.0.0"

    t.window = {
        title = "AirStrike Gunship - Simple UI Native Avionics",
        width = 640,
        height = 360,
        virtualWidth = 640,
        virtualHeight = 360,
        resizable = true,
        vsync = true
    }

    t.graphics = {
        renderer = "gl",
        clearColor = {0.05, 0.08, 0.12, 1.0},
        crt = true,
        dither = true,
        vignette = true
    }

    t.plugins.simpleUI = true
end

function crayon.init()
    crayon.print("[AirStrike] Gunship Tactical HUD & Mission Control Initialized.")
end

function crayon.update(dt)
    local ui = crayon.simpleUI
    if not ui then return end

    -- Flight telemetry simulation
    altitude = altitude + (math.sin(crayon.time.getTime() * 0.5) * 2.0)
    airspeed = 180.0 + (throttle * 30.0)

    -- -------------------------------------------------------------------------
    -- 1. Main Mission Control Window (Tabs, Combos, Tooltips, Tables)
    -- -------------------------------------------------------------------------
    ui.setNextWindowPos(30, 30, 2)
    ui.setNextWindowSize(540, 560, 2)

    local isOpen, shouldDraw = ui.beginWindow("AH-64 Gunship :: Tactical Computer", showMissionControl)
    showMissionControl = isOpen

    if shouldDraw then
        ui.textColored(hudTint[1], hudTint[2], hudTint[3], 1.0, "SYSTEM OPERATIONAL // RETRO FILTER IMMUNITY: ACTIVE")
        ui.separator()

        if ui.beginTabBar("AvionicsTabs") then
            -- Tab 1: Weaponry & Armament
            if ui.beginTabItem("Armament & Loadout") then
                ui.spacing()
                ui.text("Master Armament Switch:")
                masterArm, _ = ui.checkbox("MASTER ARM", masterArm)
                if ui.isItemHovered() then
                    ui.setTooltip("Enables active firing circuits for all wing pylons")
                end

                ui.spacing()
                ui.text("Active Weapon Pylon:")
                selectedWeapon, _ = ui.combo("Payload", selectedWeapon, weaponList)
                if ui.isItemHovered() then
                    ui.setTooltip("Selected: " .. weaponList[selectedWeapon])
                end

                ui.spacing()
                ui.separatorText("Countermeasures")
                if ui.button("DEPLOY FLARE POD", 180, 32) then
                    if flareCount > 0 then
                        flareCount = flareCount - 1
                        flaresDeployed = flaresDeployed + 1
                    end
                end
                if ui.isItemHovered() then
                    ui.setTooltip("Dispenses IR decoy flare against heat-seeking missiles")
                end

                ui.sameLine()
                ui.text(string.format("Flares Remaining: %d | Deployed: %d", flareCount, flaresDeployed))

                ui.spacing()
                ui.separatorText("Combat Authorization")
                if ui.button("REQUEST SCRAMBLE AIR-STRIKE", 240, 34) then
                    ui.openPopup("Confirm Scramble")
                end

                ui.endTabItem()
            end

            -- Tab 2: Radar & Target Table
            if ui.beginTabItem("Radar & Targets") then
                ui.spacing()
                ui.textColored(0.4, 0.9, 0.4, 1.0, "Longbow Radar Track Table (4 Contacts)")
                ui.spacing()

                local tableFlags = ui.TableFlags.Borders + ui.TableFlags.RowBg + ui.TableFlags.Resizable
                if ui.beginTable("TargetRoster", 4, tableFlags) then
                    ui.tableSetupColumn("Callsign")
                    ui.tableSetupColumn("Distance")
                    ui.tableSetupColumn("IFF Status")
                    ui.tableSetupColumn("Threat Level")
                    ui.tableHeadersRow()

                    for _, tgt in ipairs(targets) do
                        ui.tableNextRow()
                        ui.tableNextColumn()
                        ui.text(tgt.callsign)

                        ui.tableNextColumn()
                        ui.text(tgt.dist)

                        ui.tableNextColumn()
                        if tgt.status == "HOSTILE" then
                            ui.textColored(1.0, 0.3, 0.3, 1.0, tgt.status)
                        elseif tgt.status == "FRIENDLY" then
                            ui.textColored(0.3, 1.0, 0.3, 1.0, tgt.status)
                        else
                            ui.textColored(1.0, 0.8, 0.2, 1.0, tgt.status)
                        end

                        ui.tableNextColumn()
                        ui.text(tgt.threat)
                    end
                    ui.endTable()
                end

                ui.endTabItem()
            end

            -- Tab 3: System & Configuration
            if ui.beginTabItem("Pilot & Settings") then
                ui.spacing()
                playerName, _ = ui.inputText("Pilot Callsign", playerName, 32)

                ui.spacing()
                ui.separatorText("Display & Audio")
                fov, _ = ui.sliderFloat("Field of View", fov, 60.0, 110.0, "%.1f deg")
                soundVolume, _ = ui.sliderInt("Turbine Audio Volume", soundVolume, 0, 100, "%d%%")
                hudTint[1], hudTint[2], hudTint[3], _ = ui.colorEdit3("Avionics Tint", hudTint[1], hudTint[2], hudTint[3])

                ui.endTabItem()
            end

            ui.endTabBar()
        end

        -- Modal Dialog for Combat Authorization
        local modalFlags = ui.WindowFlags.AlwaysAutoResize
        local mOpen, mDraw = ui.beginPopupModal("Confirm Scramble", true, modalFlags)
        if mDraw then
            ui.textColored(1.0, 0.3, 0.3, 1.0, "WARNING: AUTHORIZE IMMEDIATE AIR STRIKE?")
            ui.text("This action will commit all ready attack squadrons.")
            ui.separator()
            if ui.button("CONFIRM AUTHORIZATION", 180, 30) then
                crayon.print("[AirStrike] Scramble Authorized by " .. playerName .. "!")
                ui.closeCurrentPopup()
            end
            ui.sameLine()
            if ui.button("Cancel", 100, 30) then
                ui.closeCurrentPopup()
            end
            ui.endPopup()
        end
    end
    ui.endWindow()

    -- -------------------------------------------------------------------------
    -- 2. Transparent Combat Flight HUD (Frameless, unmovable overlay in corner)
    -- -------------------------------------------------------------------------
    local dispW, dispH = ui.getDisplaySize()
    ui.setNextWindowPos(dispW - 280, 30, 2)
    ui.setNextWindowSize(250, 160, 2)
    ui.setNextWindowBgAlpha(0.35)

    local hudFlags = ui.WindowFlags.NoTitleBar + ui.WindowFlags.NoResize + ui.WindowFlags.NoMove
    if ui.beginWindow("HUDOverlay", true, hudFlags) then
        ui.textColored(hudTint[1], hudTint[2], hudTint[3], 1.0, "-- FLIGHT TELEMETRY --")
        ui.text(string.format("ALTITUDE: %d FT", math.floor(altitude)))
        ui.text(string.format("AIRSPEED: %.1f KTS", airspeed))
        ui.spacing()
        ui.text("THROTTLE:")
        ui.progressBar(throttle, -1, 14, string.format("%d%%", math.floor(throttle * 100)))
    end
    ui.endWindow()
end

function crayon.draw()
    -- Draw retro pixel game canvas in background (scanlines and CRT filters applied)
    crayon.graphics.clear(0.04, 0.07, 0.10)

    -- Terrain wireframe / grid in retro canvas
    crayon.graphics.setColor(0.15, 0.35, 0.22)
    crayon.graphics.drawRect("fill", 0, 220, 640, 140)

    crayon.graphics.setColor(1.0, 0.8, 0.2)
    crayon.graphics.drawText("RETRO GAME CANVAS (CRT Scanlines & Dithering Applied)", 20, 240)
    crayon.graphics.drawText("Notice how the Simple UI overlays above are 100% crisp & razor-sharp!", 20, 260)
end
