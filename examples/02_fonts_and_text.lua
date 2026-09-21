-- ============================================================================
-- Example 02: Typography, Formatting, Alignment, and Word Wrap
-- Demonstrates: drawText, text scaling, multi-line wrapping, horizontal
--               alignment (left, center, right), and font loading/metrics.
-- ============================================================================

local sampleStory = "Crayon Engine is a lightweight 2D/3D retro engine powered by C++20 and LuaJIT. " ..
                    "Text wrapping automatically breaks long paragraphs into tidy lines based on wrap width. " ..
                    "You can align text to the left, center, or right with accurate typography metrics!"

local customFont = nil
local wrapBoxWidth = 340
local boxExpandDir = 1

function crayon.init()
    crayon.window.setTitle("02 - Fonts & Typography")
    crayon.window.setResolution(640, 480)

    -- Attempt to load an optional TTF font if present in assets, or fall back cleanly
    -- local font = crayon.graphics.loadFont("assets/fonts/retro.ttf", 16)
end

function crayon.update(dt)
    -- Gently oscillate the text box boundary width to demonstrate dynamic word wrapping
    wrapBoxWidth = wrapBoxWidth + boxExpandDir * 40 * dt
    if wrapBoxWidth > 420 then
        wrapBoxWidth = 420
        boxExpandDir = -1
    elseif wrapBoxWidth < 260 then
        wrapBoxWidth = 260
        boxExpandDir = 1
    end
end

function crayon.draw()
    crayon.graphics.clear(0.06, 0.08, 0.12, 1.0)

    -- Title header
    crayon.graphics.setColor(0.9, 0.8, 0.3, 1.0)
    crayon.graphics.drawText("Typography & Text Rendering", 24, 20, { scale = 2.0 })

    crayon.graphics.setColor(0.6, 0.65, 0.75, 1.0)
    crayon.graphics.drawText("Interactive word-wrap and multi-line formatting demonstration", 24, 52, { scale = 1.0 })

    -- Left panel: Dynamic word-wrapped paragraph inside a visible boundary box
    local boxX, boxY, boxH = 24, 85, 230
    crayon.graphics.setColor(0.12, 0.15, 0.22, 0.9)
    crayon.graphics.drawRect("fill", boxX, boxY, wrapBoxWidth, boxH, 8, 8)
    crayon.graphics.setColor(0.35, 0.5, 0.8, 0.8)
    crayon.graphics.drawRect("line", boxX, boxY, wrapBoxWidth, boxH, 8, 8)

    -- Word wrapped paragraph inside the box
    crayon.graphics.setColor(0.95, 0.95, 0.95, 1.0)
    crayon.graphics.drawText(sampleStory, boxX + 12, boxY + 14, {
        scale = 1.0,
        wrap = wrapBoxWidth - 24,
        align = "left"
    })

    -- Right panel: Alignment Comparisons
    local rightX = 460
    crayon.graphics.setColor(0.3, 0.8, 0.5, 1.0)
    crayon.graphics.drawText("Alignment Options:", rightX - 100, 85, { scale = 1.2 })

    -- Center-aligned column
    crayon.graphics.setColor(0.4, 0.4, 0.5, 0.5)
    crayon.graphics.drawLine(rightX, 115, rightX, 320, 1.0)

    crayon.graphics.setColor(0.3, 0.7, 1.0, 1.0)
    crayon.graphics.drawText("Left Aligned", rightX, 125, { scale = 1.0, align = "left" })

    crayon.graphics.setColor(1.0, 0.8, 0.2, 1.0)
    crayon.graphics.drawText("Centered Text", rightX, 155, { scale = 1.0, align = "center" })

    crayon.graphics.setColor(1.0, 0.4, 0.4, 1.0)
    crayon.graphics.drawText("Right Aligned", rightX, 185, { scale = 1.0, align = "right" })

    -- Text scaling samples
    crayon.graphics.setColor(0.8, 0.6, 1.0, 1.0)
    crayon.graphics.drawText("Scale 1.0x (Regular)", 24, 340, { scale = 1.0 })
    crayon.graphics.drawText("Scale 1.5x (Medium)", 24, 365, { scale = 1.5 })
    crayon.graphics.drawText("Scale 2.0x (Large)", 24, 400, { scale = 2.0 })

    -- Footer
    crayon.graphics.setColor(0.5, 0.55, 0.65, 1.0)
    crayon.graphics.drawText("FPS: " .. math.floor(crayon.window.getFps()), 560, 440, { scale = 1.0 })
end
