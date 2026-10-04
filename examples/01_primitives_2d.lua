-- ============================================================================
-- Example 01: 2D Primitives, Colors, and Transformations
-- Demonstrates: drawRect, drawCircle, drawLine, drawPolygon, drawTriangle,
--               matrix stack (push, pop, translate, rotate, scale), and colors.
-- ============================================================================

local rotationAngle = 0.0
local pulseScale = 1.0

function crayon.init()
    crayon.window.setTitle("01 - 2D Primitives & Matrix Transforms")
    crayon.window.setResolution(640, 480)
    crayon.window.setScalingMode("aspect")
end

function crayon.update(dt)
    rotationAngle = rotationAngle + 45.0 * dt -- Rotate 45 degrees per second
    pulseScale = 1.0 + 0.2 * math.sin(rotationAngle * 0.05)
end

function crayon.draw()
    -- Clear background with a dark retro slate color
    crayon.graphics.clear(0.08, 0.09, 0.13, 1.0)

    -- 1. Drawing basic lines with thickness
    crayon.graphics.setColor(0.3, 0.35, 0.45, 1.0)
    for x = 20, 620, 40 do
        crayon.graphics.drawLine(x, 20, x, 460, 1.0)
    end
    for y = 20, 460, 40 do
        crayon.graphics.drawLine(20, y, 620, y, 1.0)
    end

    -- 2. Basic Filled & Outlined Rectangles
    crayon.graphics.setColor(0.9, 0.25, 0.3, 1.0)
    crayon.graphics.drawRect("fill", 40, 40, 80, 60)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 0.8)
    crayon.graphics.drawRect("line", 40, 40, 80, 60)

    -- 3. Rounded Rectangles (use drawRoundedRect, not drawRect)
    crayon.graphics.setColor(0.2, 0.7, 0.9, 0.85)
    crayon.graphics.drawRoundedRect("fill", 150, 40, 100, 60, 12, 12)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 0.9)
    crayon.graphics.drawRoundedRect("line", 150, 40, 100, 60, 12, 12)

    -- 4. Circles
    crayon.graphics.setColor(0.95, 0.75, 0.1, 0.9)
    crayon.graphics.drawCircle("fill", 320, 70, 30, 32)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawCircle("line", 320, 70, 30, 32)

    -- 5. Polygon & Triangle
    crayon.graphics.setColor(0.4, 0.85, 0.35, 1.0)
    crayon.graphics.drawTriangle("fill", 400, 100, 440, 40, 480, 100)

    crayon.graphics.setColor(0.85, 0.3, 0.9, 1.0)
    local hexPoints = {
        { 540, 70 }, { 560, 45 }, { 590, 45 },
        { 610, 70 }, { 590, 95 }, { 560, 95 }
    }
    crayon.graphics.drawPolygon("fill", hexPoints)

    -- 6. Matrix Transformation Stack: Rotating Star / Cross
    --    For 2D we must use the *2D variants.
    crayon.graphics.pushMatrix2D()
    crayon.graphics.translate2D(320, 280)
    crayon.graphics.rotate2D(rotationAngle)
    crayon.graphics.scale2D(pulseScale, pulseScale)

    -- Center core
    crayon.graphics.setColor(1.0, 0.4, 0.2, 0.9)
    crayon.graphics.drawRect("fill", -40, -40, 80, 80)

    -- 4 Petals
    for i = 0, 3 do
        crayon.graphics.pushMatrix2D()
        crayon.graphics.rotate2D(i * 90)
        crayon.graphics.setColor(0.2, 0.8, 1.0, 0.8)
        crayon.graphics.drawTriangle("fill", -20, -40, 0, -85, 20, -40)
        crayon.graphics.popMatrix2D()
    end

    crayon.graphics.popMatrix2D()

    -- 7. Informational Overlay
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Crayon Engine - 2D Primitives & Transforms", 24, 430, { scale = 1.0 })
    crayon.graphics.drawText("FPS: " .. math.floor(crayon.window.getFps()), 560, 430, { scale = 1.0 })
end