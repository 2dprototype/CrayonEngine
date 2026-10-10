-- Android smoke test: shows every finger as a circle and prints the platform.
-- Replace this folder's contents with your own game (keep main.lua or a .crayonproj).

local platform = "?"

function crayon.config(t)
    t.window = { virtualWidth = 480, virtualHeight = 270, scaling = "aspect" }
end

function crayon.init()
    platform = crayon.window.getPlatform()
    print("platform:", platform, "touch available:", crayon.touch.isAvailable())
end

function crayon.touchdown(id, x, y)  print("touch down", id, x, y) end
function crayon.touchup(id, x, y)    print("touch up",   id, x, y) end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.08, 0.14)
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("Crayon on " .. platform, 10, 10, 2)
    crayon.graphics.drawText("touches: " .. crayon.touch.getCount(), 10, 30, 1)

    for _, t in ipairs(crayon.touch.getTouches()) do
        crayon.graphics.setColor(1, 0.6, 0.2, 1)
        crayon.graphics.drawCircle("fill", t.x, t.y, 24)
    end
end

function crayon.update(dt)
    -- The Android Back button arrives as "escape".
    if crayon.key.isPressed("escape") then crayon.window.quit() end
end
