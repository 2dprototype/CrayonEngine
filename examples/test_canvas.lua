-- test_canvas_3.lua
local canvas
local frame = 0

function crayon.config(config)
    config.window.width = 640
    config.window.height = 480
    config.window.virtualWidth = 640
    config.window.virtualHeight = 480
end

function crayon.init()
    canvas = crayon.graphics.createCanvas(320, 240)
end

function crayon.draw()
    frame = frame + 1

    canvas:renderTo(function()
        canvas:clear(0.1, 0.1, 0.2, 1.0)

        -- Big bright shapes so we can see if anything draws
        crayon.graphics.setColor(1.0, 0.5, 0.0, 1.0)   -- orange
        crayon.graphics.drawRect("fill", 20, 20, 100, 100)

        crayon.graphics.setColor(0.3, 1.0, 0.3, 1.0)   -- green
        crayon.graphics.drawCircle("fill", 200, 120, 60, 24)

        crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
        crayon.graphics.drawText("canvas", 10, 200, { scale = 1.0 })
    end)

    crayon.graphics.clear(0.1, 0.1, 0.1, 1.0)
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawSprite(canvas, 0, 0, 640, 480)

    crayon.graphics.drawText("Frame: " .. frame, 16, 16, { scale = 2.0 })
end