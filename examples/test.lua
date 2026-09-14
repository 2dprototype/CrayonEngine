local player = { x = 160, y = 120, speed = 100 }

function crayon.init()
    crayon.window.setTitle("2D Game")
    crayon.window.setResolution(320, 240)
    crayon.window.setScalingMode("integer")
end

function crayon.update(dt)
    local dx, dy = 0, 0
    if crayon.key.isDown("a", "left") then dx = dx - 1 end
    if crayon.key.isDown("d", "right") then dx = dx + 1 end
    if crayon.key.isDown("w", "up") then dy = dy - 1 end
    if crayon.key.isDown("s", "down") then dy = dy + 1 end
    player.x = player.x + dx * player.speed * dt
    player.y = player.y + dy * player.speed * dt
end

function crayon.mousemoved(x, y)
    player.x = x
    player.y = y
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.15)
    crayon.graphics.setColor(1, 0.8, 0.2)
    crayon.graphics.drawRect("fill", player.x - 8, player.y - 8, 16, 16)
    crayon.graphics.setColor(1, 1, 1)
    crayon.graphics.drawText("FPS: " .. math.floor(crayon.window.getFps()), 4, 4, 1)
end