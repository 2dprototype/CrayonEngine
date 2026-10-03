
function crayon.config(t)
    -- t.window.transparent   = true
    -- t.window.borderless    = true
    -- t.window.alwaysOnTop   = true
    -- t.window.clickThrough  = false   -- clickable by default, toggle at runtime
    -- t.window.skipTaskbar   = true
    -- t.window.notFocusable  = true
    -- t.window.utilityWindow = true
    -- -- t.window.opacity       = 0.5
    t.console              = false
end

function crayon.init()
    local player = {
        name = "Alice",
        hp = 100,
        alive = true,
        pos = { x = 10, y = 20 },
        inventory = {
            { id = "sword", damage = 12 },
            { id = "shield", durability = 0.75 },
        },
        on_damage = function(hp) end,
    }

    crayon.print("player:", player)
end


function crayon.draw() 
    crayon.graphics.clear(0, 0, 0)
    crayon.graphics.setColor(0, 1, 0, 1)
    crayon.graphics.drawText("Hello", 10, 10, 1)
end