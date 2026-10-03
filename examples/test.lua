function crayon.config(t)
    t.window.transparent = true
    t.window.borderless  = true   -- see note #3
    t.graphics.clearColor = {0, 0, 0, 0}   -- ← alpha 0, not 1
end

function crayon.draw()
    crayon.graphics.setColor(0, 1, 0)
    crayon.graphics.drawText("[SPACE to skip]", 10, 10, 2)
end