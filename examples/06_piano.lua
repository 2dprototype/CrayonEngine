-- ============================================================================
-- Crayon Synth Piano
-- ----------------------------------------------------------------------------
--   A S D F G H J K L ;   →  white keys (C4 .. E5)
--   W E   T Y U   O P     →  black keys (C#4 .. D#5)
--
--   Hold multiple keys      →  play chords
--   Hold SHIFT              →  sustain pedal (notes ring after release)
-- ============================================================================

-- Note table:  key → { freq_hz, name, is_black, white_index }
local KEYMAP = {
    -- white keys, in order
    a       = { 261.63, "C4",  false, 0 },
    s       = { 293.66, "D4",  false, 1 },
    d       = { 329.63, "E4",  false, 2 },
    f       = { 349.23, "F4",  false, 3 },
    g       = { 392.00, "G4",  false, 4 },
    h       = { 440.00, "A4",  false, 5 },
    j       = { 493.88, "B4",  false, 6 },
    k       = { 523.25, "C5",  false, 7 },
    l       = { 587.33, "D5",  false, 8 },
    [";"]   = { 659.25, "E5",  false, 9 },

    -- black keys, white_index = the white key they sit after
    w       = { 277.18, "C#4", true, 0 },
    e       = { 311.13, "D#4", true, 1 },
    t       = { 369.99, "F#4", true, 3 },
    y       = { 415.30, "G#4", true, 4 },
    u       = { 466.16, "A#4", true, 5 },
    o       = { 554.37, "C#5", true, 7 },
    p       = { 622.25, "D#5", true, 8 },
}

local N_WHITE = 10

-- Precomputed drawing lookups -----------------------------------------------
local WHITE_AT, BLACK_AT = {}, {}
for k, spec in pairs(KEYMAP) do
    if spec[3] then BLACK_AT[spec[4]] = k
    else              WHITE_AT[spec[4]] = k
    end
end

-- Runtime -------------------------------------------------------------------
local main_snd, harm_snd        -- two shared templates, reused for all notes
local active    = {}            -- key → { v1, v2 }
local sustained = {}            -- key → true (held by sustain pedal)

local function start_note(key)
    if active[key] then return end
    local spec = KEYMAP[key]
    if not spec then return end

    -- Pitch-ratio playback: same template, different speed.
    -- 440 Hz is the template's base freq, so ratio = freq / 440.
    local ratio = spec[1] / 440.0

    active[key] = {
        crayon.audio.playSound(main_snd, { pitch = ratio,         volume = 0.55 }),
        crayon.audio.playSound(harm_snd, { pitch = ratio * 2.0,   volume = 0.16 }),
    }
end

local function stop_note(key)
    local v = active[key]
    if not v then return end
    crayon.audio.stopSound(v[1])
    crayon.audio.stopSound(v[2])
    active[key] = nil
end

-- ============================================================================
function crayon.config(t)
    t.window.resizable = true         -- Window is resizable
    t.window.vsync = true             -- Vertical sync enabled

    -- 3. Module Optimization Settings (camelCase)
    -- Disable subsystems that are not needed to save memory and CPU cycles:
    t.modules.physics3D = false        -- Jolt 3D physics disabled (no RAM allocations or physics threads)
    t.modules.audio = true           -- MiniAudio disabled (no audio device opened)
    t.modules.mesh3D = false          -- 3D mesh renderer disabled
    t.modules.particles = false        -- 2D particle system remains active
    t.modules.input = true            -- Input handling active
    t.modules.fs = false               -- Filesystem active
    t.graphics.crt = false

    -- 5. Performance Limit (camelCase)
    t.fpsLimit = 60                   -- Cap frame rate at 60 FPS
end


function crayon.init()
    crayon.window.setTitle("Crayon Synth Piano")
    crayon.window.setResolution(320, 240)
    crayon.window.setScalingMode("integer")

    -- Fundamental: triangle wave, slow decay, low sustain.
    -- Long duration as a safety cap; the key-up triggers the release.
    main_snd = crayon.audio.createSound({
        wave     = "triangle",
        freq     = 440,
        duration = 5.0,
        envelope = {
            attack  = 0.005,
            decay   = 1.4,
            sustain = 0.08,
            release = 0.30,
        },
    })

    -- Harmonic: sine at 2×, dies quickly for a bright piano attack.
    harm_snd = crayon.audio.createSound({
        wave     = "sine",
        freq     = 880,
        duration = 1.0,
        envelope = {
            attack  = 0.003,
            decay   = 0.45,
            sustain = 0.0,
            release = 0.15,
        },
    })
end

function crayon.update(dt)
    local sustain = crayon.key.isShiftDown()

    for key in pairs(KEYMAP) do
        if crayon.key.isPressed(key) then
            start_note(key)
        end
        if crayon.key.isReleased(key) then
            if sustain then
                sustained[key] = true
            else
                stop_note(key)
            end
        end
    end

    -- Sustain pedal released: flush all held-pedal notes.
    if not sustain and next(sustained) then
        for key in pairs(sustained) do stop_note(key) end
        sustained = {}
    end
end

-- ============================================================================
-- Drawing
-- ============================================================================

local function draw_white(x, y, w, h, held)
    if held then crayon.graphics.setColor(0.42, 0.78, 1.00, 1.0)
    else         crayon.graphics.setColor(0.92, 0.92, 0.88, 1.0) end
    crayon.graphics.drawRect("fill", x, y, w, h)
    crayon.graphics.setColor(0.12, 0.12, 0.15, 1.0)
    crayon.graphics.drawRect("line", x, y, w, h, 1.0)
end

local function draw_black(x, y, w, h, held)
    if held then crayon.graphics.setColor(0.42, 0.78, 1.00, 1.0)
    else         crayon.graphics.setColor(0.07, 0.07, 0.09, 1.0) end
    crayon.graphics.drawRect("fill", x, y, w, h)
    crayon.graphics.setColor(0.30, 0.30, 0.38, 1.0)
    crayon.graphics.drawRect("line", x, y, w, h, 1.0)
end

function crayon.draw()
    crayon.graphics.clear(0.055, 0.062, 0.085, 1.0)

    -- Title
    crayon.graphics.setColor(0.42, 0.85, 1.00, 1.0)
    crayon.graphics.drawText("Crayon Synth Piano", 8, 6)

    -- Chord readout, sorted by pitch so "C4 E4 G4" reads left-to-right low→high.
    local entries = {}
    for key in pairs(active) do
        local spec = KEYMAP[key]
        if spec then entries[#entries + 1] = { spec[1], spec[2] } end
    end
    table.sort(entries, function(a, b) return a[1] < b[1] end)

    local names = {}
    for _, e in ipairs(entries) do names[#names + 1] = e[2] end

    crayon.graphics.setColor(0.95, 0.85, 0.55, 1.0)
    crayon.graphics.drawText(#names > 0 and table.concat(names, " ") or "—",
                             8, 22)

    -- Status
    crayon.graphics.setColor(0.5, 0.6, 0.72, 1.0)
    crayon.graphics.drawText(string.format("Voices %d/%d   SHIFT = sustain",
                                           crayon.audio.getActiveVoiceCount(),
                                           crayon.audio.getMaxVoices()),
                             8, 40, { scale = 0.75 })

    -- Piano geometry
    local kx, ky = 10, 130
    local ww, wh = 30, 90
    local bw, bh = 18, 56

    -- White keys
    for i = 0, N_WHITE - 1 do
        local k = WHITE_AT[i]
        local held = (k ~= nil) and (active[k] ~= nil)
        draw_white(kx + i * ww, ky, ww, wh, held)
    end

    -- Black keys
    for i = 0, N_WHITE - 1 do
        local k = BLACK_AT[i]
        if k then
            local bx = kx + (i + 1) * ww - bw * 0.5
            draw_black(bx, ky, bw, bh, active[k] ~= nil)
        end
    end

    -- White key labels
    for i = 0, N_WHITE - 1 do
        local k = WHITE_AT[i]
        if k then
            crayon.graphics.setColor(0.28, 0.30, 0.36, 1.0)
            crayon.graphics.drawText(string.upper(k),
                                     kx + i * ww + 10,
                                     ky + wh - 14,
                                     { scale = 0.75 })
        end
    end

    -- Black key labels
    for i = 0, N_WHITE - 1 do
        local k = BLACK_AT[i]
        if k then
            local bx = kx + (i + 1) * ww - bw * 0.5
            crayon.graphics.setColor(0.90, 0.85, 0.55, 1.0)
            crayon.graphics.drawText(string.upper(k),
                                     bx + 4,
                                     ky + bh - 12,
                                     { scale = 0.7 })
        end
    end
end