-- ============================================================================
-- Example 05: Audio Engine & 3D Spatial Sound
-- Demonstrates: crayon.audio.loadSound, playSound, stopSound, volume buses,
--               pitch, panning, and 3D spatial attenuation.
-- ============================================================================

local soundId = 0
local masterVol = 1.0
local sfxVol    = 1.0
local musicVol  = 0.8
local soundSourcePos = { x = -2.0, y = 0.0, z = 0.0 }
local audioLoaded = false

function crayon.init()
    crayon.window.setTitle("05 - Audio Engine & Spatial Sound")
    crayon.window.setResolution(640, 480)
    crayon.window.setScalingMode("aspect")

    -- Sync local UI mirror with engine values (so the HUD is truthful)
    masterVol = crayon.audio.getMasterVolume()
    sfxVol    = crayon.audio.getSfxVolume()
    musicVol  = crayon.audio.getMusicVolume()

    -- Attempt loading a sound effect if present in assets
    -- Supported formats: WAV via SDL3 audio loading
    soundId = crayon.audio.loadSound("assets/audio/laser.wav")
    if soundId > 0 then
        audioLoaded = true
    end

    -- Configure listener position in 3D audio space
    crayon.audio.setListenerPosition(0.0, 0.0, 0.0)
    crayon.audio.setListenerOrientation(0.0, 0.0, -1.0, 0.0, 1.0, 0.0)
end

function crayon.update(dt)
    -- Orbit 3D sound source position around the listener
    local t = crayon.time.getTime()
    soundSourcePos.x = math.cos(t) * 5.0
    soundSourcePos.z = math.sin(t) * 5.0

    -- Master volume keys
    if crayon.input.isKeyPressed("up") then
        masterVol = math.min(1.0, masterVol + 0.1)
        crayon.audio.setMasterVolume(masterVol)
    elseif crayon.input.isKeyPressed("down") then
        masterVol = math.max(0.0, masterVol - 0.1)
        crayon.audio.setMasterVolume(masterVol)
    end

    -- Trigger 2D SFX with slight random pitch variation
    if crayon.input.isKeyPressed("space") and soundId > 0 then
        crayon.audio.playSound(soundId, {
            volume = 0.9,
            pitch  = 0.9 + math.random() * 0.3,
            pan    = 0.0,
        })
    end

    -- Trigger 3D spatial sound attenuated by distance from listener.
    -- NOTE: playSound3d(id, x, y, z, options) — distances live in the table,
    --       not as a positional argument.
    if crayon.input.isKeyPressed("p") and soundId > 0 then
        crayon.audio.playSound3d(soundId, soundSourcePos.x, soundSourcePos.y, soundSourcePos.z, {
            volume  = 1.0,
            minDist = 1.0,
            maxDist = 15.0,
        })
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.08, 0.12, 1.0)

    crayon.graphics.setColor(0.3, 0.8, 1.0, 1.0)
    crayon.graphics.drawText("Crayon Audio Engine", 24, 24, { scale = 2.0 })

    crayon.graphics.setColor(0.8, 0.85, 0.9, 1.0)
    crayon.graphics.drawText("Volume Controls & Polyphonic Playback", 24, 56, { scale = 1.0 })

    -- Status card
    crayon.graphics.setColor(0.14, 0.16, 0.24, 1.0)
    crayon.graphics.drawRoundedRect("fill", 24, 90, 360, 200, 8)
    crayon.graphics.setColor(0.25, 0.35, 0.55, 1.0)
    crayon.graphics.drawRoundedRect("line", 24, 90, 360, 200, 8)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Master Volume: " .. string.format("%.0f%%", masterVol * 100), 40, 110)
    crayon.graphics.drawText("SFX Volume:    " .. string.format("%.0f%%", sfxVol    * 100), 40, 135)
    crayon.graphics.drawText("Music Volume:  " .. string.format("%.0f%%", musicVol  * 100), 40, 160)

    -- Audio file status
    if audioLoaded then
        crayon.graphics.setColor(0.4, 0.9, 0.4, 1.0)
        crayon.graphics.drawText("Sound Status: 'assets/audio/laser.wav' loaded (ID: " .. soundId .. ")", 40, 200)
    else
        crayon.graphics.setColor(1.0, 0.7, 0.2, 1.0)
        crayon.graphics.drawText("Sound Status: Place a WAV file at assets/audio/laser.wav", 40, 200)
    end

    crayon.graphics.setColor(0.7, 0.75, 0.85, 1.0)
    crayon.graphics.drawText("Controls:", 40, 230)
    crayon.graphics.drawText("[SPACE] Play 2D Sound   [P] Play 3D Spatial Sound", 40, 250)
    crayon.graphics.drawText("[UP / DOWN] Adjust Master Volume", 40, 268)

    -- Right side: 2D radar of the 3D audio space
    local radarX, radarY, radarR = 500, 190, 90
    crayon.graphics.setColor(0.1, 0.13, 0.2, 1.0)
    crayon.graphics.drawCircle("fill", radarX, radarY, radarR, 32)
    crayon.graphics.setColor(0.3, 0.4, 0.6, 1.0)
    crayon.graphics.drawCircle("line", radarX, radarY, radarR, 32)
    crayon.graphics.drawCircle("line", radarX, radarY, radarR * 0.5, 24)

    -- Listener at center (blue)
    crayon.graphics.setColor(0.2, 0.7, 1.0, 1.0)
    crayon.graphics.drawCircle("fill", radarX, radarY, 6, 16)
    crayon.graphics.drawText("Listener", radarX - 22, radarY + 10, { scale = 0.8 })

    -- Orbiting 3D audio emitter (red)
    local sx = radarX + (soundSourcePos.x / 5.0) * (radarR * 0.8)
    local sy = radarY + (soundSourcePos.z / 5.0) * (radarR * 0.8)
    crayon.graphics.setColor(1.0, 0.3, 0.3, 1.0)
    crayon.graphics.drawCircle("fill", sx, sy, 5, 16)
    crayon.graphics.drawText("3D Sound", sx - 24, sy - 16, { scale = 0.8 })
end