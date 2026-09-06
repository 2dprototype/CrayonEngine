-- ============================================================================
-- Example 01: 2D Primitives, Sprites & Retro Text
-- Run with: ./crayon_engine --game game/examples/01_primitives_2d.lua
-- ============================================================================

local timer = 0
local tex_coin = 0
local particles = {}

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("Crayon Engine - 2D Primitives & Sprites")
    tex_coin = crayon.graphics.load_texture("game/assets/textures/coin.bmp")
end

function crayon.update(dt)
    timer = timer + dt

    -- Spawn particle on mouse click
    if crayon.input.is_mouse_pressed(1) then
        local mx, my = crayon.input.get_mouse_pos()
        for i = 1, 16 do
            local angle = math.random() * math.pi * 2
            local spd = 20 + math.random() * 60
            table.insert(particles, {
                x = mx, y = my,
                vx = math.cos(angle) * spd,
                vy = math.sin(angle) * spd,
                life = 1.0,
                r = math.random(), g = math.random(), b = 1.0
            })
        end
    end

    -- Update particles
    for i = #particles, 1, -1 do
        local p = particles[i]
        p.x = p.x + p.vx * dt
        p.y = p.y + p.vy * dt
        p.life = p.life - dt * 1.5
        if p.life <= 0 then
            table.remove(particles, i)
        end
    end

    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.08, 0.14)

    -- 1. Rectangles
    crayon.graphics.set_color(0.2, 0.3, 0.6, 0.8)
    crayon.graphics.draw_rect("fill", 15, 25, 80, 50)
    crayon.graphics.set_color(0.8, 0.9, 1.0, 1.0)
    crayon.graphics.draw_rect("line", 15, 25, 80, 50)
    crayon.graphics.draw_text("Filled Rect", 20, 45, 1.0)

    -- 2. Circles
    crayon.graphics.set_color(0.9, 0.4, 0.2, 0.8)
    crayon.graphics.draw_circle("fill", 160, 50, 24)
    crayon.graphics.set_color(1.0, 0.8, 0.4, 1.0)
    crayon.graphics.draw_circle("line", 160, 50, 24)
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_text("Circle", 140, 46, 1.0)

    -- 3. Rotating Sprites
    local rot = timer * 3.0
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_sprite(tex_coin, 260, 50, 32, 32, rot, 16, 16)
    crayon.graphics.draw_text("Sprite", 240, 75, 1.0)

    -- 4. Dynamic Lines
    for i = 0, 10 do
        local t = timer + (i * 0.2)
        local y1 = 120 + math.sin(t) * 20
        local y2 = 120 + math.cos(t) * 20
        crayon.graphics.set_color(0.3 + (i * 0.07), 0.7, 1.0 - (i * 0.07), 0.7)
        crayon.graphics.draw_line(20 + i * 28, y1, 30 + i * 28, y2, 2.0)
    end

    -- 5. Particle Bursts
    for _, p in ipairs(particles) do
        crayon.graphics.set_color(p.r, p.g, p.b, p.life)
        crayon.graphics.draw_circle("fill", p.x, p.y, p.life * 3.0)
    end

    -- 6. HUD / Instructions
    crayon.graphics.set_color(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.draw_text("Left Click anywhere to spawn particles!", 15, 180, 1.0)
    crayon.graphics.draw_text("FPS: " .. math.floor(crayon.window.get_fps() + 0.5), 15, 200, 1.0)

    -- Mouse cursor
    local mx, my = crayon.input.get_mouse_pos()
    crayon.graphics.set_color(1.0, 0.3, 0.3, 1.0)
    crayon.graphics.draw_line(mx - 3, my, mx + 3, my, 1.0)
    crayon.graphics.draw_line(mx, my - 3, mx, my + 3, 1.0)
end
