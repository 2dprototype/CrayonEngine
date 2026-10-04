-- ============================================================================
-- Audio Test Rig — runs one feature at a time to isolate the crash
-- ============================================================================

local log_lines = {}
local function log(msg)
    local line = string.format("[%6.2f] %s", crayon.time.getTime(), msg)
    print(line)
    table.insert(log_lines, 1, line)
    while #log_lines > 14 do table.remove(log_lines) end
end

-- ---- Phase system -----------------------------------------------------------
local phase = 0
local phase_timer = 0
local PHASE_DURATION = 2.0
local phases = {}

local function add_phase(name, on_enter, on_update)
    table.insert(phases, {
        name = name,
        on_enter = on_enter or function() end,
        on_update = on_update or function() end,
    })
end

local function run_enter()
    if phase < 1 or phase > #phases then return end
    log("==== PHASE " .. phase .. ": " .. phases[phase].name .. " ====")
    local ok, err = pcall(phases[phase].on_enter)
    if not ok then log("PHASE ENTER ERROR: " .. tostring(err)) end
end

local function next_phase()
    phase = phase + 1
    phase_timer = 0
    if phase <= #phases then
        run_enter()
    else
        log("==== ALL PHASES COMPLETE ====")
    end
end

-- ---- Shared state -----------------------------------------------------------
local laser, coin, noise_snd
local loop_voice, orbit_voice, custom_voice = 0, 0, 0
local custom_ref = -1

-- ============================================================================
-- PHASES
-- ============================================================================

add_phase("0. baseline (no audio)", function() end)

add_phase("1. createSound(sine)", function()
    laser = crayon.audio.createSound({
        wave = "sine", freq = 440, duration = 0.3,
        envelope = { attack = 0.005, decay = 0.1, sustain = 0.5, release = 0.15 },
    })
    log("laser id = " .. tostring(laser))
end)

add_phase("2. playSound one-shot x3", function()
    for i = 1, 3 do
        log("  playSound #" .. i)
        crayon.audio.playSound(laser, { volume = 0.5 })
    end
end)

add_phase("3. playSound loop, volume=0", function()
    log("starting silent loop voice")
    loop_voice = crayon.audio.playSound(laser, { volume = 0.0, loop = true })
    log("loop_voice id = " .. tostring(loop_voice))
end)

add_phase("4. loop voice audible (0.4)", function()
    log("setVoiceVolume(loop_voice, 0.4)")
    crayon.audio.setVoiceVolume(loop_voice, 0.4)
end)

add_phase("5. live pitch sweep on loop", function()
    log("pitch sweep starts (update runs each frame)")
end, function(dt)
    if loop_voice > 0 then
        crayon.audio.setVoicePitch(loop_voice,
            0.5 + 0.5 * math.sin(crayon.time.getTime() * 3))
    end
end)

add_phase("6. mute loop, start 3D voice (vol=0)", function()
    if loop_voice > 0 then crayon.audio.setVoiceVolume(loop_voice, 0.0) end
    log("starting silent 3D voice")
    orbit_voice = crayon.audio.playSound3D(laser, 4, 0, 0,
        { volume = 0.0, maxDist = 10 })
    log("orbit_voice id = " .. tostring(orbit_voice))
end)

add_phase("7. 3D voice audible + moving", function()
    log("setVoiceVolume(orbit_voice, 0.6)")
    crayon.audio.setVoiceVolume(orbit_voice, 0.6)
end, function(dt)
    if orbit_voice > 0 then
        local t = crayon.time.getTime()
        crayon.audio.setVoicePosition(orbit_voice,
            math.cos(t) * 4, 0, math.sin(t) * 4)
    end
end)

add_phase("8. listener orientation rotating", function()
    log("rotating listener")
end, function(dt)
    local yaw = crayon.time.getTime()
    crayon.audio.setListenerOrientation(
        math.sin(yaw), 0, -math.cos(yaw), 0, 1, 0)
end)

add_phase("9. stopSound(orbit_voice)", function()
    log("stopSound")
    crayon.audio.stopSound(orbit_voice)
    orbit_voice = 0
end)

add_phase("10. createBuffer (baked noise)", function()
    log("creating baked buffer")
    noise_snd = crayon.audio.createBuffer(44100, function(t, i)
        return (math.random() * 2 - 1) * math.exp(-t * 6)
    end)
    log("noise_snd id = " .. tostring(noise_snd))
end)

add_phase("11. play baked buffer", function()
    log("playSound(noise_snd)")
    crayon.audio.playSound(noise_snd, { volume = 0.8 })
end)

add_phase("12. custom block generator (silent)", function()
    log("creating custom voice, vol=0, loop")
    custom_voice = crayon.audio.playSound(laser, { volume = 0.0, loop = true })
    log("custom_voice id = " .. tostring(custom_voice))
    log("setBlockGenerator(voice, 512, fn)")
    custom_ref = crayon.audio.setBlockGenerator(custom_voice, 512,
        function(t0, sr, n)
            local buf = {}
            for i = 1, n do
                buf[i] = math.sin((t0 + (i - 1) / sr) * 220 * 2 * math.pi) * 0.3
            end
            return buf
        end)
    log("custom_ref = " .. tostring(custom_ref))
end)

add_phase("13. custom block generator audible", function()
    log("setVoiceVolume(custom_voice, 0.4)")
    crayon.audio.setVoiceVolume(custom_voice, 0.4)
end)

add_phase("14. noise wave + filter", function()
    log("creating noise + lowpass sound")
    local n = crayon.audio.createSound({
        wave = "noise", duration = 0.5,
        envelope = { attack = 0.001, decay = 0.15, sustain = 0.0, release = 0.3 },
        filter = { cutoff = 900 },
    })
    log("noise id = " .. tostring(n))
    crayon.audio.playSound(n, { volume = 0.7 })
end)

add_phase("15. polyphony x10", function()
    log("playing 10 sounds at once")
    for i = 1, 10 do
        crayon.audio.playSound(laser, { volume = 0.3 })
    end
end)

add_phase("16. try music", function()
    log("playMusic('assets/audio/snare_4.wav')")
    local ok = crayon.audio.playMusic("assets/audio/snare_4.wav",
        { loop = true, fadeIn = 0.5 })
    log("playMusic returned " .. tostring(ok))
end)

add_phase("17. stopAllSounds", function()
    log("stopAllSounds")
    crayon.audio.stopAllSounds()
end)

add_phase("18. cleanup", function()
    log("clearBlockGenerator")
    if custom_ref >= 0 then
        crayon.audio.clearBlockGenerator(custom_voice, custom_ref)
    end
    log("DONE — no crash through all phases")
end)

-- ============================================================================
-- Callbacks
-- ============================================================================

function crayon.init()
    log("==== init start ====")
    crayon.window.setTitle("Audio Test Rig")
    crayon.window.setResolution(640, 360)

    log("setListenerPosition(0,0,0)")
    crayon.audio.setListenerPosition(0, 0, 0)
    log("setListenerOrientation(0,0,-1, 0,1,0)")
    crayon.audio.setListenerOrientation(0, 0, -1, 0, 1, 0)

    log("max voices = " .. crayon.audio.getMaxVoices())
    log("==== init done ====")

    phase = 0
    phase_timer = 0
    next_phase()   -- enters phase 1
end

function crayon.update(dt)
    if phase < 1 or phase > #phases then return end
    phase_timer = phase_timer + dt

    -- Skip to next phase with N
    if crayon.key.isPressed("n") then
        log("---- skipped phase " .. phase .. " ----")
        next_phase()
        return
    end

    -- Run this phase's update
    local ok, err = pcall(phases[phase].on_update, dt)
    if not ok then
        log("PHASE UPDATE ERROR: " .. tostring(err))
    end

    if phase_timer >= PHASE_DURATION then
        log("---- phase " .. phase .. " done ----")
        next_phase()
    end
end

function crayon.quit()
    log("quit callback")
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.12, 1)

    crayon.graphics.setColor(0.4, 0.85, 1.0)
    crayon.graphics.drawText("Audio Test Rig", 20, 14, { scale = 1.2 })

    crayon.graphics.setColor(1, 1, 1)
    if phase >= 1 and phase <= #phases then
        crayon.graphics.drawText(
            string.format("Phase %d/%d  (%.1fs)  %s",
                phase, #phases, phase_timer, phases[phase].name),
            20, 40)
    else
        crayon.graphics.drawText("All phases complete — no crash!", 20, 40)
    end

    crayon.graphics.setColor(0.7, 0.9, 0.7)
    crayon.graphics.drawText(
        "Voices: " .. crayon.audio.getActiveVoiceCount() ..
        " / " .. crayon.audio.getMaxVoices() ..
        "   (press N to skip phase)",
        20, 60)

    -- On-screen log
    crayon.graphics.setColor(0.05, 0.06, 0.09, 0.95)
    crayon.graphics.drawRoundedRect("fill", 20, 86, 600, 260, 6)
    crayon.graphics.setColor(0.25, 0.3, 0.4)
    crayon.graphics.drawRoundedRect("line", 20, 86, 600, 260, 6)

    local y = 96
    for _, line in ipairs(log_lines) do
        crayon.graphics.setColor(0.8, 0.85, 0.95)
        crayon.graphics.drawText(line, 30, y, { scale = 0.75 })
        y = y + 16
    end
end