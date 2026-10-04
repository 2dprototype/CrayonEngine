-- Crayon Engine: Rich Text Markup Test
-- Demonstrates animated BBCode markup: [wave], [shake], [rainbow], [color], [b], [scale], and typewriter dialogue

function crayon.config(config)
    config.window.title = "Crayon Engine - Rich Text Markup Test"
    config.window.width = 320
    config.window.height = 240
    config.window.virtualWidth = 320
    config.window.virtualHeight = 240
    config.modules.physics3D = false
    config.modules.mesh3D = false
end

local dialogueLines = {
    "[color=yellow][b]Narrator:[/b][/color]\nWelcome to the [rainbow]dark asylum[/rainbow]...",
    "[color=red][shake amp=3]DANGER![/shake][/color] The facility core is [color=#ff3333]unstable[/color]!",
    "[color=#55ffff][wave amp=4]Can anyone hear me?[/wave][/color] The radio is fading away...",
    "[color=white]Normal text with [b]bold drop shadow[/b] and [color=green]nested colored words[/color]."
}

local currentLineIdx = 1
local visibleChars = 0
local charTimer = 0.0
local charsPerSec = 28.0
local lineDone = false

function crayon.init()
    visibleChars = 0
    charTimer = 0.0
    lineDone = false
end

function crayon.update(dt)
    local currentText = dialogueLines[currentLineIdx]
    local _, _, totalChars = crayon.graphics.measureTextMarkup(currentText, 1.0, 280)

    if visibleChars < totalChars then
        charTimer = charTimer + dt
        visibleChars = math.floor(charTimer * charsPerSec)
        if visibleChars >= totalChars then
            visibleChars = totalChars
            lineDone = true
        end
    else
        lineDone = true
    end

    -- Advance dialogue on Space or Left Click
    if crayon.key.isPressed("space") or crayon.mouse.isPressed(1) then
        if not lineDone then
            -- Skip to end of current line
            visibleChars = totalChars
            lineDone = true
        else
            -- Next line
            currentLineIdx = (currentLineIdx % #dialogueLines) + 1
            visibleChars = 0
            charTimer = 0.0
            lineDone = false
        end
    end
end

function crayon.draw()
    crayon.graphics.clear(0.05, 0.06, 0.08, 1.0)

    -- Showcase title
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawTextMarkup("[b][rainbow]CRAYON ENGINE RICH TEXT[/rainbow][/b]", 50, 15, 1.2)

    -- Demo various animated tags statically
    crayon.graphics.drawTextMarkup("[color=cyan][wave amp=3]Wavy text floating in space...[/wave][/color]", 20, 42, 1.0)
    crayon.graphics.drawTextMarkup("[color=red][shake amp=2]Terrifying violent tremor![/shake][/color]", 20, 62, 1.0)
    crayon.graphics.drawTextMarkup("[color=#ffa500]Custom Hex #FFA500[/color] & [b]Bold Drop Shadow[/b]", 20, 82, 1.0)
    crayon.graphics.drawTextMarkup("Inline scale: [scale=0.8]small[/scale] and [scale=1.5][color=yellow]BIG[/color][/scale] text", 20, 102, 1.0)

    -- Dialogue box container
    local boxX = 14
    local boxY = 135
    local boxW = 292
    local boxH = 90

    -- Background & border
    crayon.graphics.setColor(0.08, 0.1, 0.14, 0.95)
    crayon.graphics.drawRoundedRect("fill", boxX, boxY, boxW, boxH, 6)
    crayon.graphics.setColor(0.3, 0.45, 0.65, 0.8)
    crayon.graphics.drawRoundedRect("line", boxX, boxY, boxW, boxH, 6)

    -- Typewriter dialogue text with auto wrap
    local currentText = dialogueLines[currentLineIdx]
    crayon.graphics.drawTextMarkup(currentText, boxX + 10, boxY + 12, 1.0, boxW - 20, "left", visibleChars)

    -- Next prompt indicator
    if lineDone then
        local blink = (math.sin(crayon.time.getTime() * 8.0) > 0)
        if blink then
            crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
            crayon.graphics.drawText("v [SPACE]", boxX + boxW - 65, boxY + boxH - 14, 0.8)
        end
    else
        crayon.graphics.setColor(0.6, 0.6, 0.6, 0.8)
        crayon.graphics.drawText("[SPACE to skip]", boxX + boxW - 75, boxY + boxH - 14, 0.7)
    end
end