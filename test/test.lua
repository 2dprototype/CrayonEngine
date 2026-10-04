function crayon.config(t)

end

function crayon.init()
    tex = crayon.graphics.loadTexture("../examples/assets/textures/brick.bmp")
    
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("My Game")
end

function crayon.update(dt)
    if crayon.key.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)

    -- Shapes
    crayon.graphics.setColor(1, 0, 0, 1)
    crayon.graphics.drawRect("fill", 10, 10, 50, 50)
    crayon.graphics.drawCircle("fill", 100, 100, 30)

    -- Text
    crayon.graphics.drawText("Score: " .. 10, 10, 10, 2)

    -- 2D Camera
    crayon.graphics.setCamera2D({ x = 0, y = 0, zoom = 1.0, angle = 0 })
    
    crayon.graphics.drawSprite(tex, 10, 20, 32, 32, 2, 16, 16)

    -- Transforms
    crayon.graphics.pushMatrix2D()
    crayon.graphics.translate2D(100, 100)
    crayon.graphics.rotate2D(1.57)
    crayon.graphics.drawSprite(tex, 0, 0, 32, 32)
    crayon.graphics.popMatrix2D()
end