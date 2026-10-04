-- ============================================================================
-- Example 05: Audio Engine Test
-- Keys 1-8 play sounds. Arrow keys change volumes. M toggles music.
-- ============================================================================

local laser, coin, explosion, blip, noise_buf
local loop_voice, orbit_voice
local orbit_angle = 0
local custom_ref = -1

function crayon.config(t)
	t.window.width = 640
	t.window.height = 360
end

function crayon.init()
    crayon.window.setTitle("05 - Audio Test")
    crayon.window.setResolution(640, 360)

    -- Procedural sounds (no files needed)
    laser = crayon.audio.createSound({
        wave = "square", freq = 880, duration = 0.2,
        envelope   = { attack = 0.001, decay = 0.05, sustain = 0.25, release = 0.08 },
        pitchSweep = { from = 1400, to = 180, time = 0.18 },
    })
    coin = crayon.audio.createSound({
        wave = "square", freq = 988, duration = 0.1,
        envelope = { attack = 0.001, decay = 0.02, sustain = 0.6, release = 0.05 },
    })
    explosion = crayon.audio.createSound({
        wave = "noise", duration = 0.5,
        envelope = { attack = 0.001, decay = 0.15, sustain = 0.0, release = 0.3 },
        filter   = { cutoff = 900 },
    })
    blip = crayon.audio.createSound({
        wave = "sine", freq = 1320, duration = 0.05,
        envelope = { attack = 0.001, decay = 0.01, sustain = 0.0, release = 0.03 },
    })

    -- Baked procedural buffer (one-time cost at init)
    noise_buf = crayon.audio.createBuffer(44100, function(t, i)
        return (math.random() * 2 - 1) * math.exp(-t * 6)
    end)

    -- Listener at origin looking down -Z
    crayon.audio.setListenerPosition(0, 0, 0)
    crayon.audio.setListenerOrientation(0, 0, -1, 0, 1, 0)

    -- Two persistent voices
    loop_voice  = crayon.audio.playSound(laser, { volume = 0.0, loop = true })
    orbit_voice = crayon.audio.playSound3D(coin, 4, 0, 0, { volume = 0.6, maxDist = 10 })

    -- Custom Lua block generator on a third voice
    local cv = crayon.audio.playSound(blip, { volume = 0.4, loop = true })
    custom_ref = crayon.audio.setBlockGenerator(cv, 512, function(t0, sr, n)
        local buf = {}
        for i = 1, n do
            local t = t0 + (i - 1) / sr
            buf[i] = math.sin(t * 220 * 2 * math.pi) * 0.4
        end
        return buf
    end)

    print("Max voices:", crayon.audio.getMaxVoices())
end

function crayon.update(dt)
    -- Orbit the 3D voice around the listener
    orbit_angle = orbit_angle + dt
    crayon.audio.setVoicePosition(orbit_voice,
        math.cos(orbit_angle) * 4, 0, math.sin(orbit_angle) * 4)

    -- One-shot sounds
    if crayon.key.isPressed("1") then crayon.audio.playSound(laser)      end
    if crayon.key.isPressed("2") then crayon.audio.playSound(coin)       end
    if crayon.key.isPressed("3") then crayon.audio.playSound(explosion)  end
    if crayon.key.isPressed("4") then crayon.audio.playSound(blip, { pan = -0.5 }) end
    if crayon.key.isPressed("5") then crayon.audio.playSound(noise_buf)  end
    if crayon.key.isPressed("6") then
        crayon.audio.playSound3D(coin, 6, 0, 0, { maxDist = 12 })
    end
    if crayon.key.isPressed("7") then
        crayon.audio.playSound(coin, { volume = 1.0 })   -- polyphony test
    end
    if crayon.key.isPressed("8") then
        crayon.audio.setVoiceVolume(loop_voice, 0.4)     -- enable loop
    end

    -- Music toggle
    if crayon.key.isPressed("m") then
        if crayon.audio.isMusicPlaying() then
            crayon.audio.stopMusic(0.5)
        else
            crayon.audio.playMusic("assets/audio/snare_4.wav", { loop = true, fadeIn = 0.5 })
        end
    end

    -- Volume
    if crayon.key.isDown("up")    then crayon.audio.setMasterVolume(crayon.audio.getMasterVolume() + dt) end
    if crayon.key.isDown("down")  then crayon.audio.setMasterVolume(crayon.audio.getMasterVolume() - dt) end
    if crayon.key.isDown("right") then crayon.audio.setSfxVolume(crayon.audio.getSfxVolume() + dt)       end
    if crayon.key.isDown("left")  then crayon.audio.setSfxVolume(crayon.audio.getSfxVolume() - dt)       end

    -- Live pitch + pan sweep on the loop voice
    crayon.audio.setVoicePitch(loop_voice,
        0.5 + 0.5 * (1 + math.sin(crayon.time.getTime() * 2)))
end

function crayon.quit()
    if custom_ref >= 0 then
        crayon.audio.clearBlockGenerator(0, custom_ref)   -- voice id 0 = ignore, just free ref
    end
end

function crayon.draw()
	
    crayon.graphics.clear(0.08, 0.09, 0.12, 1)
    crayon.graphics.setColor(0.4, 0.85, 1.0)
    crayon.graphics.drawText("Audio Engine Test", 20, 20, { scale = 1.5 })

    crayon.graphics.setColor(1, 1, 1)
    crayon.graphics.drawText("[1] Laser (procedural square + sweep)", 20, 70)
    crayon.graphics.drawText("[2] Coin (procedural square)",           20, 90)
    crayon.graphics.drawText("[3] Explosion (noise + filter)",         20, 110)
    crayon.graphics.drawText("[4] Blip (sine, panned left)",           20, 130)
    crayon.graphics.drawText("[5] Baked noise buffer",                 20, 150)
    crayon.graphics.drawText("[6] 3D coin at (6,0,0)",                 20, 170)
    crayon.graphics.drawText("[7] Polyphony test",                     20, 190)
    crayon.graphics.drawText("[8] Enable looping laser",               20, 210)
    crayon.graphics.drawText("[M] Music toggle",                       20, 230)
    crayon.graphics.drawText("[ARROWS] Volumes",                       20, 250)

    crayon.graphics.setColor(0.7, 0.9, 0.7)
    crayon.graphics.drawText(
        "Voices: " .. crayon.audio.getActiveVoiceCount() ..
        " / " .. crayon.audio.getMaxVoices(), 20, 300)

    -- Simple radar for the 3D voice
    local rx, ry, rr = 500, 200, 90
    crayon.graphics.setColor(0.15, 0.18, 0.25)
    crayon.graphics.drawCircle("fill", rx, ry, rr)
    crayon.graphics.setColor(0.35, 0.45, 0.65)
    crayon.graphics.drawCircle("line", rx, ry, rr)
    crayon.graphics.setColor(0.2, 0.7, 1.0)
    crayon.graphics.drawCircle("fill", rx, ry, 5)       -- listener
    local sx = rx + (math.cos(orbit_angle) * 4 / 5) * rr
    local sy = ry + (math.sin(orbit_angle) * 4 / 5) * rr
    crayon.graphics.setColor(1.0, 0.4, 0.4)
    crayon.graphics.drawCircle("fill", sx, sy, 5)       -- 3D source
end