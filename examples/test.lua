
function crayon.config(t)
    t.window.transparent   = true
    t.window.borderless    = true
    t.window.alwaysOnTop   = true
    t.window.clickThrough  = false   -- clickable by default, toggle at runtime
    t.window.skipTaskbar   = true
    t.window.notFocusable  = true
    t.window.utilityWindow = true
    t.window.opacity       = 0.5
end


function crayon.draw() 
    crayon.graphics.clear(0, 0, 0)
    crayon.graphics.setColor(0, 1, 0, 1)
    crayon.graphics.drawText("Hello", 10, 10, 1)
end