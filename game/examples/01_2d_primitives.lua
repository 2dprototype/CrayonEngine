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
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("01 - 2D Primitives & Blending [TAB to Switch]")
end

function crayon.update(dt)
    timer = timer + dt

    if crayon.input.is_pressed("tab") or crayon.input.is_pressed("space") then
        demo_mode = (demo_mode % num_modes) + 1
    end
    if crayon.input.is_pressed("1") then demo_mode = 1 end
    if crayon.input.is_pressed("2") then demo_mode = 2 end
    if crayon.input.is_pressed("3") then demo_mode = 3 end
    if crayon.input.is_pressed("4") then demo_mode = 4 end

    -- Mouse scroll for scissor viewport
    local wheel_y = crayon.input.get_mouse_wheel() or 0
    if wheel_y ~= 0 then
        clip_scroll_y = clip_scroll_y - wheel_y * 12
    end
    if crayon.input.is_down("up") then clip_scroll_y = clip_scroll_y - 40 * dt end
    if crayon.input.is_down("down") then clip_scroll_y = clip_scroll_y + 40 * dt end
    clip_scroll_y = math.max(0, math.min(120, clip_scroll_y))

    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.14)

    -- Top Status Bar
    crayon.graphics.set_color(0.12, 0.14, 0.22, 1.0)
    crayon.graphics.draw_rect("fill", 0, 0, 320, 22)
    crayon.graphics.set_color(0.3, 0.5, 0.9, 1.0)
    crayon.graphics.draw_line(0, 22, 320, 22, 1.0)

    crayon.graphics.set_color(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.draw_text(mode_names[demo_mode], 6, 6, 1.0)

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
    crayon.graphics.set_color(0.12, 0.14, 0.22, 0.9)
    crayon.graphics.draw_rect("fill", 0, 222, 320, 18)
    crayon.graphics.set_color(0.6, 0.7, 0.85, 1.0)
    crayon.graphics.draw_text("[TAB/1-4]: Switch Page | [ESC]: Quit", 8, 226, 1.0)
end

function draw_basic_shapes()
    -- 1. Points
    crayon.graphics.set_color(0.9, 0.3, 0.4, 1.0)
    for i = 1, 8 do
        crayon.graphics.draw_point(15 + i * 4, 45)
    end
    crayon.graphics.set_color(0.7, 0.8, 0.9, 1.0)
    crayon.graphics.draw_text("Points", 15, 30, 1.0)

    -- 2. Lines (with thickness)
    crayon.graphics.set_color(0.3, 0.8, 0.9, 1.0)
    crayon.graphics.draw_line(70, 35, 115, 65, 1.0)
    crayon.graphics.set_color(0.9, 0.6, 0.2, 1.0)
    crayon.graphics.draw_line(70, 65, 115, 35, 3.0)
    crayon.graphics.draw_text("Thick Lines", 65, 75, 1.0)

    -- 3. Rectangles (fill & line)
    crayon.graphics.set_color(0.2, 0.6, 0.4, 0.8)
    crayon.graphics.draw_rect("fill", 140, 35, 45, 35)
    crayon.graphics.set_color(0.5, 1.0, 0.6, 1.0)
    crayon.graphics.draw_rect("line", 140, 35, 45, 35)
    crayon.graphics.set_color(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.draw_text("Rect", 150, 48, 1.0)

    -- 4. Triangles (fill & line)
    local tri_t = timer * 2.0
    local tx = 230 + math.cos(tri_t) * 8
    crayon.graphics.set_color(0.7, 0.2, 0.8, 0.7)
    crayon.graphics.draw_triangle("fill", tx, 35, 205, 70, 255, 70)
    crayon.graphics.set_color(1.0, 0.5, 1.0, 1.0)
    crayon.graphics.draw_triangle("line", tx, 35, 205, 70, 255, 70)
    crayon.graphics.draw_text("Triangle", 212, 75, 1.0)

    -- 5. Quads
    crayon.graphics.set_color(0.2, 0.4, 0.8, 0.8)
    crayon.graphics.draw_quad("fill", 25, 110, 80, 100, 70, 150, 15, 140)
    crayon.graphics.set_color(0.4, 0.8, 1.0, 1.0)
    crayon.graphics.draw_quad("line", 25, 110, 80, 100, 70, 150, 15, 140)
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_text("Quad", 35, 122, 1.0)

    -- 6. Convex & Star Polygons (draw_polygon)
    -- Hexagon
    local hex_pts = {}
    local hcx, hcy, hr = 135, 130, 25
    for i = 0, 5 do
        local a = (i / 6) * math.pi * 2 + timer
        table.insert(hex_pts, {hcx + math.cos(a) * hr, hcy + math.sin(a) * hr})
    end
    crayon.graphics.set_color(0.8, 0.7, 0.1, 0.7)
    crayon.graphics.draw_polygon("fill", hex_pts)
    crayon.graphics.set_color(1.0, 0.95, 0.4, 1.0)
    crayon.graphics.draw_polygon("line", hex_pts)
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_text("Polygon", 116, 162, 1.0)

    -- 5-Point Star Polygon
    local star_pts = {}
    local scx, scy = 230, 130
    for i = 0, 9 do
        local a = (i / 10) * math.pi * 2 - math.pi / 2
        local r = (i % 2 == 0) and 28 or 13
        table.insert(star_pts, {scx + math.cos(a) * r, scy + math.sin(a) * r})
    end
    crayon.graphics.set_color(0.9, 0.2, 0.3, 0.7)
    crayon.graphics.draw_polygon("fill", star_pts)
    crayon.graphics.set_color(1.0, 0.6, 0.6, 1.0)
    crayon.graphics.draw_polygon("line", star_pts)
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_text("Star Polygon", 200, 165, 1.0)

    -- Text Metrics demo
    local test_msg = "Precise Text Width & Height"
    local tw = crayon.graphics.get_text_width(test_msg, 1.0)
    local th = crayon.graphics.get_text_height(test_msg, 1.0)
    crayon.graphics.set_color(0.15, 0.2, 0.3, 0.6)
    crayon.graphics.draw_rect("fill", 15, 190, tw + 8, th + 6)
    crayon.graphics.set_color(0.4, 0.8, 0.9, 1.0)
    crayon.graphics.draw_rect("line", 15, 190, tw + 8, th + 6)
    crayon.graphics.set_color(0.9, 0.95, 1.0, 1.0)
    crayon.graphics.draw_text(test_msg, 19, 193, 1.0)
end

function draw_curves_and_rings()
    -- 1. Rounded Rectangles
    crayon.graphics.set_color(0.2, 0.35, 0.6, 0.85)
    crayon.graphics.draw_rounded_rect("fill", 20, 35, 80, 50, 10, 8)
    crayon.graphics.set_color(0.5, 0.8, 1.0, 1.0)
    crayon.graphics.draw_rounded_rect("line", 20, 35, 80, 50, 10, 8)
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_text("Rounded Rect", 26, 52, 1.0)

    -- 2. Circles & Ellipses
    local ecx, ecy = 160, 60
    crayon.graphics.set_color(0.8, 0.3, 0.4, 0.7)
    crayon.graphics.draw_ellipse("fill", ecx, ecy, 36, 22, 24)
    crayon.graphics.set_color(1.0, 0.6, 0.7, 1.0)
    crayon.graphics.draw_ellipse("line", ecx, ecy, 36, 22, 24)
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_text("Ellipse", 143, 56, 1.0)

    -- 3. Circular Rings (fill & line)
    local rcx, rcy = 260, 60
    crayon.graphics.set_color(0.3, 0.8, 0.5, 0.8)
    crayon.graphics.draw_ring("fill", rcx, rcy, 16, 26, 24)
    crayon.graphics.set_color(0.6, 1.0, 0.8, 1.0)
    crayon.graphics.draw_ring("line", rcx, rcy, 16, 26, 24)
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_text("Ring", 248, 56, 1.0)

    -- 4. Dynamic Gauge Arcs
    local start_a = -math.pi * 0.8
    local sweep = math.sin(timer * 2.0) * 0.5 + 0.5 -- 0 to 1
    local end_a = start_a + sweep * math.pi * 1.6

    -- Background Arc Track
    crayon.graphics.set_color(0.2, 0.25, 0.35, 0.7)
    crayon.graphics.draw_arc("line", 80, 150, 40, start_a, start_a + math.pi * 1.6, 32)
    -- Active Arc Fill
    crayon.graphics.set_color(1.0, 0.7, 0.2, 1.0)
    crayon.graphics.draw_arc("line", 80, 150, 40, start_a, end_a, 32)
    crayon.graphics.draw_text("Arc Gauge: " .. math.floor(sweep * 100) .. "%", 45, 150, 1.0)

    -- 5. Animated Multi-Ring Target
    local tcx, tcy = 220, 145
    for r = 1, 4 do
        local osc = math.sin(timer * 3.0 + r) * 3
        local in_r = r * 8 + osc
        local out_r = in_r + 4
        crayon.graphics.set_color(0.4 + r * 0.15, 0.3, 0.8 - r * 0.15, 0.8)
        crayon.graphics.draw_ring("fill", tcx, tcy, in_r, out_r, 20)
    end
    crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.draw_text("Concentric Rings", 175, 195, 1.0)
end

function draw_blending_modes()
    local modes = {"alpha", "additive", "multiply", "none"}
    local labels = {"Alpha (Default)", "Additive (Glow)", "Multiply (Shadow)", "None (Opaque)"}

    for col = 1, 4 do
        local bx = 20 + (col - 1) * 75
        local by = 45

        -- Background checkerboard or pattern to showcase blend transparency
        crayon.graphics.set_blend_mode("alpha")
        crayon.graphics.set_color(0.2, 0.2, 0.25, 1.0)
        crayon.graphics.draw_rect("fill", bx, by, 65, 90)
        crayon.graphics.set_color(0.9, 0.9, 0.95, 1.0)
        crayon.graphics.draw_rect("fill", bx + 5, by + 5, 25, 25)
        crayon.graphics.draw_rect("fill", bx + 35, by + 35, 25, 25)

        -- Title
        crayon.graphics.set_color(1.0, 1.0, 1.0, 1.0)
        crayon.graphics.draw_text(labels[col], bx - 5, by + 100, 1.0)

        -- Active Blend Mode
        crayon.graphics.set_blend_mode(modes[col])

        -- Draw overlapping circles
        local t = timer * 2.0
        local ox1 = math.cos(t) * 6
        local oy1 = math.sin(t) * 6

        crayon.graphics.set_color(1.0, 0.2, 0.2, 0.75)
        crayon.graphics.draw_circle("fill", bx + 25 + ox1, by + 35 + oy1, 18)

        crayon.graphics.set_color(0.2, 0.9, 0.3, 0.75)
        crayon.graphics.draw_circle("fill", bx + 42 - ox1, by + 40 - oy1, 18)

        crayon.graphics.set_color(0.3, 0.4, 1.0, 0.75)
        crayon.graphics.draw_circle("fill", bx + 32, by + 55, 18)
    end

    crayon.graphics.set_blend_mode("alpha")
    crayon.graphics.set_color(0.8, 0.85, 0.9, 1.0)
    crayon.graphics.draw_text("Set blend modes on the fly with crayon.graphics.set_blend_mode()", 10, 190, 1.0)
end

function draw_scissor_clipping()
    crayon.graphics.set_color(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.draw_text("Scissor Clipping Viewport (Use UP/DOWN or Scroll)", 15, 30, 1.0)

    -- Container Border
    local vx, vy, vw, vh = 30, 50, 260, 130
    crayon.graphics.set_color(0.2, 0.25, 0.35, 1.0)
    crayon.graphics.draw_rect("fill", vx - 2, vy - 2, vw + 4, vh + 4)
    crayon.graphics.set_color(0.5, 0.7, 1.0, 1.0)
    crayon.graphics.draw_rect("line", vx - 2, vy - 2, vw + 4, vh + 4)

    -- Apply Scissor Rect!
    crayon.graphics.set_scissor(vx, vy, vw, vh)

    -- Draw scrollable list inside scissor
    local content_y = vy + 10 - clip_scroll_y
    for i = 1, 15 do
        local item_y = content_y + (i - 1) * 22
        -- Alternating item backgrounds
        if i % 2 == 0 then
            crayon.graphics.set_color(0.12, 0.16, 0.25, 0.9)
        else
            crayon.graphics.set_color(0.16, 0.21, 0.32, 0.9)
        end
        crayon.graphics.draw_rounded_rect("fill", vx + 5, item_y, vw - 10, 18, 4)

        crayon.graphics.set_color(0.4, 1.0, 0.6, 1.0)
        crayon.graphics.draw_circle("fill", vx + 16, item_y + 9, 4)

        crayon.graphics.set_color(0.95, 0.95, 0.95, 1.0)
        crayon.graphics.draw_text("Inventory Item #" .. i .. " - Crystal Shard x" .. (i * 3), vx + 28, item_y + 4, 1.0)
    end

    -- Draw a big rotating star that clips through viewport edge
    local star_cx, star_cy = vx + vw - 20, vy + vh / 2
    local star_pts = {}
    for i = 0, 9 do
        local a = (i / 10) * math.pi * 2 + timer * 1.5
        local r = (i % 2 == 0) and 45 or 20
        table.insert(star_pts, {star_cx + math.cos(a) * r, star_cy + math.sin(a) * r})
    end
    crayon.graphics.set_color(1.0, 0.8, 0.2, 0.75)
    crayon.graphics.draw_polygon("fill", star_pts)
    crayon.graphics.set_color(1.0, 1.0, 0.6, 1.0)
    crayon.graphics.draw_polygon("line", star_pts)

    -- Reset Scissor
    crayon.graphics.reset_scissor()

    -- Scrollbar indicator
    crayon.graphics.set_color(0.1, 0.12, 0.18, 0.8)
    crayon.graphics.draw_rect("fill", vx + vw - 6, vy, 6, vh)
    local thumb_h = 30
    local thumb_y = vy + (clip_scroll_y / 120) * (vh - thumb_h)
    crayon.graphics.set_color(0.4, 0.7, 1.0, 0.9)
    crayon.graphics.draw_rounded_rect("fill", vx + vw - 5, thumb_y, 4, thumb_h, 2)

    crayon.graphics.set_color(0.6, 0.7, 0.85, 1.0)
    crayon.graphics.draw_text("Notice all rendering outside the box is clipped automatically!", 15, 195, 1.0)
end
