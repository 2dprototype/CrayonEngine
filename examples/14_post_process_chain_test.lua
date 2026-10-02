-- Crayon Engine: 2D Post-Process Chain Test
-- Demonstrates stackable post-process effects: chromatic, vignette, vhs, bloom2d, pixelate, radialBlur, filmGrain

function crayon.config(config)
    config.window.title = "Crayon Engine - Post-Process Chain Test"
    config.window.width = 960
    config.window.height = 720
    config.window.virtualWidth = 320
    config.window.virtualHeight = 240
    config.modules.physics = false
    config.modules.mesh3d = false
end

local effectsList = {
    { name = "chromatic", label = "1: Chromatic Aberration", active = false },
    { name = "vignette", label = "2: Vignette (Pulsing)", active = false },
    { name = "vhs", label = "3: VHS Tape Glitch", active = false },
    { name = "bloom2d", label = "4: Bloom 2D", active = false },
    { name = "pixelate", label = "5: Pixelate", active = false },
    { name = "radialBlur", label = "6: Radial Blur", active = false },
    { name = "filmGrain", label = "7: Film Grain", active = false },
}

local function rebuildChain()
    crayon.graphics.clearEffects()
    for _, eff in ipairs(effectsList) do
        if eff.active then
            crayon.graphics.pushEffect(eff.name)
        end
    end
end

function crayon.init()
    -- Enable chromatic and vignette by default for horror atmosphere
    effectsList[1].active = true
    effectsList[2].active = true
    rebuildChain()
end

function crayon.update(dt)
    -- Toggle effects with keys 1 through 7
    local keys = { "1", "2", "3", "4", "5", "6", "7" }
    for i, k in ipairs(keys) do
        if crayon.input.isKeyPressed(k) then
            effectsList[i].active = not effectsList[i].active
            rebuildChain()
        end
    end

    -- Clear all effects with C
    if crayon.input.isKeyPressed("c") then
        for _, eff in ipairs(effectsList) do
            eff.active = false
        end
        rebuildChain()
    end

    -- Animate uniforms dynamically
    local t = crayon.time.getTime()
    if effectsList[1].active then
        local amt = 0.004 + math.sin(t * 3.0) * 0.003
        crayon.graphics.setEffectUniform("chromatic", "u_offset", amt, amt * 0.6)
    end
    if effectsList[2].active then
        local radius = 0.7 + math.sin(t * 2.0) * 0.1
        crayon.graphics.setEffectUniform("vignette", "u_radius", radius)
        crayon.graphics.setEffectUniform("vignette", "u_intensity", 0.6)
    end
    if effectsList[6].active then
        local blur = 0.02 + (math.sin(t * 4.0) + 1.0) * 0.015
        crayon.graphics.setEffectUniform("radialBlur", "u_intensity", blur)
    end
end

function crayon.draw()
    crayon.graphics.clear(0.06, 0.07, 0.1, 1.0)

    local t = crayon.time.getTime()

    -- Draw colorful background elements to demonstrate filters
    for i = 1, 8 do
        local angle = t * 0.5 + i * (math.pi / 4)
        local cx = 160 + math.cos(angle) * 70
        local cy = 120 + math.sin(angle) * 50
        local r = 0.5 + 0.5 * math.sin(t + i)
        local g = 0.5 + 0.5 * math.sin(t + i + 2)
        local b = 0.5 + 0.5 * math.sin(t + i + 4)
        crayon.graphics.setColor(r, g, b, 0.85)
        crayon.graphics.drawCircle(cx, cy, 22, true)
    end

    -- Center emblem
    crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.drawRoundedRect(120, 85, 80, 70, 8, true)
    crayon.graphics.setColor(0.1, 0.1, 0.15, 1.0)
    crayon.graphics.drawText("CRAYON", 132, 110, 1.2)

    -- Instructions HUD
    crayon.graphics.setColor(0.0, 0.0, 0.0, 0.75)
    crayon.graphics.drawRect(4, 4, 312, 60, true)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Press 1-7 to toggle stackable effects:", 8, 8, 1.0)

    for i, eff in ipairs(effectsList) do
        local col = eff.active and {0.2, 1.0, 0.4} or {0.6, 0.6, 0.6}
        crayon.graphics.setColor(col[1], col[2], col[3], 1.0)
        local x = 8 + ((i - 1) % 4) * 76
        local y = 22 + math.floor((i - 1) / 4) * 14
        crayon.graphics.drawText(string.sub(eff.label, 1, 8) .. (eff.active and " [ON]" or " [-]"), x, y, 0.8)
    end

    crayon.graphics.setColor(0.9, 0.9, 0.9, 0.8)
    crayon.graphics.drawText("Press 'C' to clear effects stack", 8, 52, 0.8)
end
