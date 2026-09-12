-- ============================================================================
-- Example 01: Complete 2D Drawing Primitives, Blending & Scissor Clipping
-- Crayon Engine
-- ============================================================================

local demo_mode = 1 -- 1: Basic Shapes, 2: Advanced Curves & Rings, 3: Blending Modes, 4: Scissor Clipping
local num_modes = 4
local mode_names = {
    "1: Basic Shapes (Point, Line, Tri, Quad, Poly)",
    "2: Curves (Rounded Rect, Ellipse, Arc, Ring)",
    "3: Blend Modes (Alpha, Additive, Multiply, None)",
    "4: Scissor Clipping (Scrollable Viewport)"
}

local timer = 0
local clip_scroll_y = 0

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("01 - 2D Primitives & Blending [TAB to Switch]")
end

function crayon.update(dt)
    timer = timer + dt

    if crayon.input.isPressed("tab") or crayon.input.isPressed("space") then
        demo_mode = (demo_mode % num_modes) + 1
    end
    if crayon.input.isPressed("1") then demo_mode = 1 end
    if crayon.input.isPressed("2") then demo_mode = 2 end
    if crayon.input.isPressed("3") then demo_mode = 3 end
    if crayon.input.isPressed("4") then demo_mode = 4 end

    -- Mouse scroll for scissor viewport
    local wheel_x, wheel_y = crayon.input.getMouseWheel()
    if wheel_y and wheel_y ~= 0 then
        clip_scroll_y = clip_scroll_y - wheel_y * 12
    end
    if crayon.input.isDown("up") then clip_scroll_y = clip_scroll_y - 40 * dt end
    if crayon.input.isDown("down") then clip_scroll_y = clip_scroll_y + 40 * dt end
    clip_scroll_y = math.max(0, math.min(120, clip_scroll_y))

    if crayon.input.isPressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.14)

    -- Top Status Bar
    crayon.graphics.setColor(0.12, 0.14, 0.22, 1.0)
    crayon.graphics.drawRect("fill", 0, 0, 320, 22)
    crayon.graphics.setColor(0.3, 0.5, 0.9, 1.0)
    crayon.graphics.drawLine(0, 22, 320, 22, 1.0)

    crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.drawText(mode_names[demo_mode], 6, 6, 1.0)

    if demo_mode == 1 then
        draw_basic_shapes()
    elseif demo_mode == 2 then
        draw_curves_and_rings()
    elseif demo_mode == 3 then
        draw_blending_modes()
    elseif demo_mode == 4 then
        draw_scissor_clipping()
    end

    -- Bottom Controls Hint
    crayon.graphics.setColor(0.12, 0.14, 0.22, 0.9)
    crayon.graphics.drawRect("fill", 0, 222, 320, 18)
    crayon.graphics.setColor(0.6, 0.7, 0.85, 1.0)
    crayon.graphics.drawText("[TAB/1-4]: Switch Page | [ESC]: Quit", 8, 226, 1.0)
end

function draw_basic_shapes()
    -- 1. Points
    crayon.graphics.setColor(0.9, 0.3, 0.4, 1.0)
    for i = 1, 8 do
        crayon.graphics.drawPoint(15 + i * 4, 45)
    end
    crayon.graphics.setColor(0.7, 0.8, 0.9, 1.0)
    crayon.graphics.drawText("Points", 15, 30, 1.0)

    -- 2. Lines (with thickness)
    crayon.graphics.setColor(0.3, 0.8, 0.9, 1.0)
    crayon.graphics.drawLine(70, 35, 115, 65, 1.0)
    crayon.graphics.setColor(0.9, 0.6, 0.2, 1.0)
    crayon.graphics.drawLine(70, 65, 115, 35, 3.0)
    crayon.graphics.drawText("Thick Lines", 65, 75, 1.0)

    -- 3. Rectangles (fill & line)
    crayon.graphics.setColor(0.2, 0.6, 0.4, 0.8)
    crayon.graphics.drawRect("fill", 140, 35, 45, 35)
    crayon.graphics.setColor(0.5, 1.0, 0.6, 1.0)
    crayon.graphics.drawRect("line", 140, 35, 45, 35)
    crayon.graphics.setColor(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.drawText("Rect", 150, 48, 1.0)

    -- 4. Triangles (fill & line)
    local tri_t = timer * 2.0
    local tx = 230 + math.cos(tri_t) * 8
    crayon.graphics.setColor(0.7, 0.2, 0.8, 0.7)
    crayon.graphics.drawTriangle("fill", tx, 35, 205, 70, 255, 70)
    crayon.graphics.setColor(1.0, 0.5, 1.0, 1.0)
    crayon.graphics.drawTriangle("line", tx, 35, 205, 70, 255, 70)
    crayon.graphics.drawText("Triangle", 212, 75, 1.0)

    -- 5. Quads
    crayon.graphics.setColor(0.2, 0.4, 0.8, 0.8)
    crayon.graphics.drawQuad("fill", 25, 110, 80, 100, 70, 150, 15, 140)
    crayon.graphics.setColor(0.4, 0.8, 1.0, 1.0)
    crayon.graphics.drawQuad("line", 25, 110, 80, 100, 70, 150, 15, 140)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Quad", 35, 122, 1.0)

    -- 6. Convex & Star Polygons (drawPolygon)
    -- Hexagon
    local hex_pts = {}
    local hcx, hcy, hr = 135, 130, 25
    for i = 0, 5 do
        local a = (i / 6) * math.pi * 2 + timer
        table.insert(hex_pts, {hcx + math.cos(a) * hr, hcy + math.sin(a) * hr})
    end
    crayon.graphics.setColor(0.8, 0.7, 0.1, 0.7)
    crayon.graphics.drawPolygon("fill", hex_pts)
    crayon.graphics.setColor(1.0, 0.95, 0.4, 1.0)
    crayon.graphics.drawPolygon("line", hex_pts)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Polygon", 116, 162, 1.0)

    -- 5-Point Star Polygon
    local star_pts = {}
    local scx, scy = 230, 130
    for i = 0, 9 do
        local a = (i / 10) * math.pi * 2 - math.pi / 2
        local r = (i % 2 == 0) and 28 or 13
        table.insert(star_pts, {scx + math.cos(a) * r, scy + math.sin(a) * r})
    end
    crayon.graphics.setColor(0.9, 0.2, 0.3, 0.7)
    crayon.graphics.drawPolygon("fill", star_pts)
    crayon.graphics.setColor(1.0, 0.6, 0.6, 1.0)
    crayon.graphics.drawPolygon("line", star_pts)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Star Polygon", 200, 165, 1.0)

    -- Text Metrics demo
    local test_msg = "Precise Text Width & Height"
    local tw = crayon.graphics.getTextWidth(test_msg, 1.0)
    local th = crayon.graphics.getTextHeight(test_msg, 1.0)
    crayon.graphics.setColor(0.15, 0.2, 0.3, 0.6)
    crayon.graphics.drawRect("fill", 15, 190, tw + 8, th + 6)
    crayon.graphics.setColor(0.4, 0.8, 0.9, 1.0)
    crayon.graphics.drawRect("line", 15, 190, tw + 8, th + 6)
    crayon.graphics.setColor(0.9, 0.95, 1.0, 1.0)
    crayon.graphics.drawText(test_msg, 19, 193, 1.0)
end

function draw_curves_and_rings()
    -- 1. Rounded Rectangles
    crayon.graphics.setColor(0.2, 0.35, 0.6, 0.85)
    crayon.graphics.drawRoundedRect("fill", 20, 35, 80, 50, 10, 8)
    crayon.graphics.setColor(0.5, 0.8, 1.0, 1.0)
    crayon.graphics.drawRoundedRect("line", 20, 35, 80, 50, 10, 8)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Rounded Rect", 26, 52, 1.0)

    -- 2. Circles & Ellipses
    local ecx, ecy = 160, 60
    crayon.graphics.setColor(0.8, 0.3, 0.4, 0.7)
    crayon.graphics.drawEllipse("fill", ecx, ecy, 36, 22, 24)
    crayon.graphics.setColor(1.0, 0.6, 0.7, 1.0)
    crayon.graphics.drawEllipse("line", ecx, ecy, 36, 22, 24)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Ellipse", 143, 56, 1.0)

    -- 3. Circular Rings (fill & line)
    local rcx, rcy = 260, 60
    crayon.graphics.setColor(0.3, 0.8, 0.5, 0.8)
    crayon.graphics.drawRing("fill", rcx, rcy, 16, 26, 24)
    crayon.graphics.setColor(0.6, 1.0, 0.8, 1.0)
    crayon.graphics.drawRing("line", rcx, rcy, 16, 26, 24)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Ring", 248, 56, 1.0)

    -- 4. Dynamic Gauge Arcs
    local start_a = -math.pi * 0.8
    local sweep = math.sin(timer * 2.0) * 0.5 + 0.5 -- 0 to 1
    local end_a = start_a + sweep * math.pi * 1.6

    -- Background Arc Track
    crayon.graphics.setColor(0.2, 0.25, 0.35, 0.7)
    crayon.graphics.drawArc("line", 80, 150, 40, start_a, start_a + math.pi * 1.6, 32)
    -- Active Arc Fill
    crayon.graphics.setColor(1.0, 0.7, 0.2, 1.0)
    crayon.graphics.drawArc("line", 80, 150, 40, start_a, end_a, 32)
    crayon.graphics.drawText("Arc Gauge: " .. math.floor(sweep * 100) .. "%", 45, 150, 1.0)

    -- 5. Animated Multi-Ring Target
    local tcx, tcy = 220, 145
    for r = 1, 4 do
        local osc = math.sin(timer * 3.0 + r) * 3
        local in_r = r * 8 + osc
        local out_r = in_r + 4
        crayon.graphics.setColor(0.4 + r * 0.15, 0.3, 0.8 - r * 0.15, 0.8)
        crayon.graphics.drawRing("fill", tcx, tcy, in_r, out_r, 20)
    end
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Concentric Rings", 175, 195, 1.0)
end

function draw_blending_modes()
    local modes = {"alpha", "additive", "multiply", "none"}
    local labels = {"Alpha (Default)", "Additive (Glow)", "Multiply (Shadow)", "None (Opaque)"}

    for col = 1, 4 do
        local bx = 20 + (col - 1) * 75
        local by = 45

        -- Background checkerboard or pattern to showcase blend transparency
        crayon.graphics.setBlendMode("alpha")
        crayon.graphics.setColor(0.2, 0.2, 0.25, 1.0)
        crayon.graphics.drawRect("fill", bx, by, 65, 90)
        crayon.graphics.setColor(0.9, 0.9, 0.95, 1.0)
        crayon.graphics.drawRect("fill", bx + 5, by + 5, 25, 25)
        crayon.graphics.drawRect("fill", bx + 35, by + 35, 25, 25)

        -- Title
        crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
        crayon.graphics.drawText(labels[col], bx - 5, by + 100, 1.0)

        -- Active Blend Mode
        crayon.graphics.setBlendMode(modes[col])

        -- Draw overlapping circles
        local t = timer * 2.0
        local ox1 = math.cos(t) * 6
        local oy1 = math.sin(t) * 6

        crayon.graphics.setColor(1.0, 0.2, 0.2, 0.75)
        crayon.graphics.drawCircle("fill", bx + 25 + ox1, by + 35 + oy1, 18)

        crayon.graphics.setColor(0.2, 0.9, 0.3, 0.75)
        crayon.graphics.drawCircle("fill", bx + 42 - ox1, by + 40 - oy1, 18)

        crayon.graphics.setColor(0.3, 0.4, 1.0, 0.75)
        crayon.graphics.drawCircle("fill", bx + 32, by + 55, 18)
    end

    crayon.graphics.setBlendMode("alpha")
    crayon.graphics.setColor(0.8, 0.85, 0.9, 1.0)
    crayon.graphics.drawText("Set blend modes on the fly with crayon.graphics.setBlendMode()", 10, 190, 1.0)
end

function draw_scissor_clipping()
    crayon.graphics.setColor(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.drawText("Scissor Clipping Viewport (Use UP/DOWN or Scroll)", 15, 30, 1.0)

    -- Container Border
    local vx, vy, vw, vh = 30, 50, 260, 130
    crayon.graphics.setColor(0.2, 0.25, 0.35, 1.0)
    crayon.graphics.drawRect("fill", vx - 2, vy - 2, vw + 4, vh + 4)
    crayon.graphics.setColor(0.5, 0.7, 1.0, 1.0)
    crayon.graphics.drawRect("line", vx - 2, vy - 2, vw + 4, vh + 4)

    -- Apply Scissor Rect!
    crayon.graphics.setScissor(vx, vy, vw, vh)

    -- Draw scrollable list inside scissor
    local content_y = vy + 10 - clip_scroll_y
    for i = 1, 15 do
        local item_y = content_y + (i - 1) * 22
        -- Alternating item backgrounds
        if i % 2 == 0 then
            crayon.graphics.setColor(0.12, 0.16, 0.25, 0.9)
        else
            crayon.graphics.setColor(0.16, 0.21, 0.32, 0.9)
        end
        crayon.graphics.drawRoundedRect("fill", vx + 5, item_y, vw - 10, 18, 4)

        crayon.graphics.setColor(0.4, 1.0, 0.6, 1.0)
        crayon.graphics.drawCircle("fill", vx + 16, item_y + 9, 4)

        crayon.graphics.setColor(0.95, 0.95, 0.95, 1.0)
        crayon.graphics.drawText("Inventory Item #" .. i .. " - Crystal Shard x" .. (i * 3), vx + 28, item_y + 4, 1.0)
    end

    -- Draw a big rotating star that clips through viewport edge
    local star_cx, star_cy = vx + vw - 20, vy + vh / 2
    local star_pts = {}
    for i = 0, 9 do
        local a = (i / 10) * math.pi * 2 + timer * 1.5
        local r = (i % 2 == 0) and 45 or 20
        table.insert(star_pts, {star_cx + math.cos(a) * r, star_cy + math.sin(a) * r})
    end
    crayon.graphics.setColor(1.0, 0.8, 0.2, 0.75)
    crayon.graphics.drawPolygon("fill", star_pts)
    crayon.graphics.setColor(1.0, 1.0, 0.6, 1.0)
    crayon.graphics.drawPolygon("line", star_pts)

    -- Reset Scissor
    crayon.graphics.resetScissor()

    -- Scrollbar indicator
    crayon.graphics.setColor(0.1, 0.12, 0.18, 0.8)
    crayon.graphics.drawRect("fill", vx + vw - 6, vy, 6, vh)
    local thumb_h = 30
    local thumb_y = vy + (clip_scroll_y / 120) * (vh - thumb_h)
    crayon.graphics.setColor(0.4, 0.7, 1.0, 0.9)
    crayon.graphics.drawRoundedRect("fill", vx + vw - 5, thumb_y, 4, thumb_h, 2)

    crayon.graphics.setColor(0.6, 0.7, 0.85, 1.0)
    crayon.graphics.drawText("Notice all rendering outside the box is clipped automatically!", 15, 195, 1.0)
end