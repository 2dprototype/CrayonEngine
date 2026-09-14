-- ============================================================================
-- Example 14: Transparent Desktop Screenpet, Advanced Window & Input Showcase
-- Demonstrates:
-- 1. Transparent window / opacity for desktop screenpets & widgets
-- 2. Click-and-drag window movement across the desktop
-- 3. Always-on-top & borderless window toggling
-- 4. Full mouse events (virtual & physical window pos, delta, wheel x/y)
-- 5. Live text input typing buffer, clipboard copy/paste, and key modifiers
-- 6. Advanced 2D drawing (multi-color gradients, bezier curves, radial pie)
-- 7. 3D-to-2D projection for floating nameplates
-- ============================================================================

local pet = {
    x = 160, y = 145,
    w = 54, h = 48,
    vx = 30.0,
    dir = 1,
    state = "idle",
    state_timer = 0,
    blink = false,
    blink_timer = 0,
    hearts = {},
    happiness = 0.85
}

-- Window dragging state
local dragging_window = false
local drag_anchor_mouse_x = 0
local drag_anchor_mouse_y = 0

-- Window settings (Manual toggle via [SPACE], [O]/[P], [T], [B])
local is_transparent_mode = true
local window_opacity = 1.0
local always_on_top = false
local is_bordered = true

-- Text Input Box
local text_buffer = "Type here... [Ctrl+V to Paste]"
local text_active = false
local cursor_timer = 0

-- 3D projection test entity
local cube_model = nil
local rot_3d = 0.0

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("Crayon Engine - Desktop Screenpet & Input Showcase")

    -- Enable transparent window mode and always-on-top
    crayon.window.setTransparent(is_transparent_mode)
    crayon.window.setOpacity(window_opacity)
    crayon.window.setAlwaysOnTop(always_on_top)

    -- Start accepting text input
    crayon.key.setTextInput(true)
    text_active = true

    -- Load procedural 3D model for floating projection tag
    cube_model = crayon.graphics.loadModel("cube")
end

function crayon.update(dt)
    cursor_timer = cursor_timer + dt
    rot_3d = rot_3d + dt * 60.0

    local virt_mouse_x, virt_mouse_y = crayon.mouse.getPosition()
    local win_mouse_x, win_mouse_y = crayon.mouse.getWindowPosition()

    -- 1. Window Dragging with Mouse (Hold Left Click in top header bar or on pet to drag entire window!)
    if crayon.mouse.isPressed("left") then
        local in_drag_zone = (virt_mouse_y < 24) or
                             (math.abs(virt_mouse_x - pet.x) < 32 and math.abs(virt_mouse_y - pet.y) < 30)
        if in_drag_zone then
            dragging_window = true
            drag_anchor_mouse_x = win_mouse_x
            drag_anchor_mouse_y = win_mouse_y
        end
    end

    if dragging_window then
        if crayon.mouse.isDown("left") then
            local dx = win_mouse_x - drag_anchor_mouse_x
            local dy = win_mouse_y - drag_anchor_mouse_y
            if dx ~= 0 or dy ~= 0 then
                local wx, wy = crayon.window.getPosition()
                crayon.window.setPosition(math.floor(wx + dx), math.floor(wy + dy))
            end
        else
            dragging_window = false
        end
    end

    -- 2. Window Controls via Keyboard
    -- [O] Decrease opacity, [P] Increase opacity
    if crayon.key.isDown("o") then
        window_opacity = math.max(0.2, window_opacity - dt * 0.8)
        crayon.window.setOpacity(window_opacity)
    end
    if crayon.key.isDown("p") then
        window_opacity = math.min(1.0, window_opacity + dt * 0.8)
        crayon.window.setOpacity(window_opacity)
    end

    -- [T] Toggle Always-on-top
    if crayon.key.isPressed("t") then
        always_on_top = not always_on_top
        crayon.window.setAlwaysOnTop(always_on_top)
    end

    -- [B] Toggle Window Border
    if crayon.key.isPressed("b") then
        is_bordered = not is_bordered
        crayon.window.setBordered(is_bordered)
    end

    -- [SPACE] Toggle Transparent Desktop vs Solid Canvas
    if crayon.key.isPressed("space") then
        is_transparent_mode = not is_transparent_mode
        crayon.window.setTransparent(is_transparent_mode)
    end

    -- [ESCAPE] Return to Menu
    if crayon.key.isPressed("escape") then
        crayon.window.setTransparent(false)
        crayon.window.setOpacity(1.0)
        crayon.window.setAlwaysOnTop(false)
        crayon.window.setBordered(true)
        crayon.key.setTextInput(false)
        in_menu = true
        crayon.window.setResolution(320, 240)
        crayon.window.setTitle("Crayon Engine - Demo Launcher Hub")
        return
    end

    -- 3. Screenpet Behavior & AI
    pet.blink_timer = pet.blink_timer + dt
    if pet.blink_timer > 3.5 then
        pet.blink = true
        if pet.blink_timer > 3.7 then
            pet.blink = false
            pet.blink_timer = 0
        end
    end

    -- Petting interaction
    local dist_to_mouse = math.sqrt((virt_mouse_x - pet.x)^2 + (virt_mouse_y - pet.y)^2)
    if dist_to_mouse < 35 and crayon.mouse.isPressed("right") then
        -- Spawn heart particle
        table.insert(pet.hearts, { x = pet.x, y = pet.y - 20, vy = -40, life = 1.0 })
        pet.happiness = math.min(1.0, pet.happiness + 0.15)
        pet.state = "happy"
        pet.state_timer = 1.2
    end

    -- Update hearts
    for i = #pet.hearts, 1, -1 do
        local h = pet.hearts[i]
        h.y = h.y + h.vy * dt
        h.life = h.life - dt
        if h.life <= 0 then
            table.remove(pet.hearts, i)
        end
    end

    -- Passive wander
    if pet.state == "idle" then
        pet.x = pet.x + pet.vx * pet.dir * dt
        if pet.x > 260 then pet.dir = -1 end
        if pet.x < 60  then pet.dir = 1 end
    elseif pet.state == "happy" then
        pet.state_timer = pet.state_timer - dt
        if pet.state_timer <= 0 then
            pet.state = "idle"
        end
    end

    -- Slowly decay happiness
    pet.happiness = math.max(0.1, pet.happiness - dt * 0.02)

    -- 4. Text Input Handling
    local chars = crayon.key.getTextInput()
    if #chars > 0 then
        if text_buffer == "Type here... [Ctrl+V to Paste]" then
            text_buffer = ""
        end
        text_buffer = text_buffer .. chars
    end

    -- Backspace deletes character
    if crayon.key.isPressed("backspace") and #text_buffer > 0 then
        text_buffer = string.sub(text_buffer, 1, -2)
    end

    -- Ctrl+V Paste from system clipboard
    if crayon.key.isCtrlDown() and crayon.key.isPressed("v") then
        local clip = crayon.key.getClipboard()
        if clip and #clip > 0 then
            if text_buffer == "Type here... [Ctrl+V to Paste]" then
                text_buffer = ""
            end
            text_buffer = text_buffer .. clip
        end
    end
end

function crayon.draw()
    -- 1. Clear Screen: When transparent mode is enabled, alpha 0.0 makes desktop show through!
    if is_transparent_mode then
        crayon.graphics.clear(0.0, 0.0, 0.0, 0.0)
    else
        crayon.graphics.clear(0.08, 0.1, 0.16, 1.0)
    end

    -- 2. Top Drag Header Bar (Multi-color gradient!)
    crayon.graphics.drawGradientH(4, 4, 312, 20, {0.15, 0.25, 0.55, 0.88}, {0.45, 0.18, 0.55, 0.88})
    crayon.graphics.setColor(0.4, 0.8, 1.0, 1.0)
    crayon.graphics.drawRect("line", 4, 4, 312, 20, 1.0)
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("SCREENPET OVERLAY | DRAG BAR TO MOVE", 12, 10, 1.0)

    -- 3. Bezier Trajectory Rope connecting Pet to Mouse
    local mx, my = crayon.input.getMousePos()
    local mid_x = (pet.x + mx) * 0.5
    local mid_y = math.max(pet.y, my) + 25.0 -- droop curve
    crayon.graphics.setColor(0.9, 0.6, 0.2, 0.75)
    crayon.graphics.drawBezier(pet.x, pet.y - 12, mid_x, mid_y, mx, my, 1.5, 18)

    -- 4. Draw Animated Screenpet
    local bob = math.sin(cursor_timer * 6.0) * 2.0
    local px, py = pet.x, pet.y + bob

    -- Pet Body (Asymmetric rounded rect)
    crayon.graphics.drawRoundedRectEx("fill", px - 22, py - 20, 44, 36, 16, 16, 8, 8, 8)
    crayon.graphics.drawRoundedRectEx("line", px - 22, py - 20, 44, 36, 16, 16, 8, 8, 8)

    -- Ears
    crayon.graphics.setColor(1.0, 0.85, 0.3, 1.0)
    crayon.graphics.drawTriangle("fill", px - 18, py - 18, px - 10, py - 18, px - 15, py - 28)
    crayon.graphics.drawTriangle("fill", px + 10, py - 18, px + 18, py - 18, px + 15, py - 28)

    -- Eyes
    crayon.graphics.setColor(0.1, 0.1, 0.15, 1.0)
    if pet.blink then
        crayon.graphics.drawLine(px - 10, py - 8, px - 4, py - 8, 2.0)
        crayon.graphics.drawLine(px + 4, py - 8, px + 10, py - 8, 2.0)
    else
        crayon.graphics.drawCircle("fill", px - 7, py - 8, 3.5, 12)
        crayon.graphics.drawCircle("fill", px + 7, py - 8, 3.5, 12)
        -- Eye glint
        crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
        crayon.graphics.drawCircle("fill", px - 8, py - 9, 1.0, 8)
        crayon.graphics.drawCircle("fill", px + 6, py - 9, 1.0, 8)
    end

    -- Blush cheeks
    crayon.graphics.setColor(1.0, 0.4, 0.5, 0.6)
    crayon.graphics.drawCircle("fill", px - 12, py - 2, 2.5, 8)
    crayon.graphics.drawCircle("fill", px + 12, py - 2, 2.5, 8)

    -- Mouth
    crayon.graphics.setColor(0.3, 0.15, 0.1, 1.0)
    if pet.state == "happy" then
        crayon.graphics.drawArc("line", px, py - 4, 4.0, 0.0, math.pi, 8)
    else
        crayon.graphics.drawArc("line", px, py - 3, 2.5, 0.2, math.pi - 0.2, 8)
    end

    -- Happiness Radial Pie Chart above Pet
    crayon.graphics.drawPie("fill", px + 24, py - 22, 6.0, -math.pi * 0.5, -math.pi * 0.5 + (pet.happiness * math.pi * 2.0), 16)
    crayon.graphics.setColor(0.2, 0.2, 0.2, 1.0)
    crayon.graphics.drawCircle("line", px + 24, py - 22, 6.0, 16)

    -- Hearts animation
    crayon.graphics.setColor(1.0, 0.25, 0.45, 1.0)
    for _, h in ipairs(pet.hearts) do
        crayon.graphics.drawCircle("fill", h.x - 3, h.y, 3, 8)
        crayon.graphics.drawCircle("fill", h.x + 3, h.y, 3, 8)
        crayon.graphics.drawTriangle("fill", h.x - 6, h.y, h.x + 6, h.y, h.x, h.y + 6)
    end

    -- 5. 3D Coordinate Entity & Floating 3D-to-2D Projection Tag
    local cam_pos = {0, 2, 6}
    crayon.graphics.setCamera3d({
        position = cam_pos,
        target = {0, 0, 0},
        up = {0, 1, 0},
        fov = 60.0
    })

    -- Render small rotating 3D wireframe cube and coordinate axes at world (2.4, -0.2, 0)
    local obj_pos = {2.4, -0.2, 0.0}
    crayon.graphics.drawAxes3d(obj_pos[1], obj_pos[2], obj_pos[3], 0.8)
    crayon.graphics.drawCubeWires(obj_pos[1], obj_pos[2], obj_pos[3], 0.7, 0.7, 0.7, {0.2, 0.9, 0.8, 0.85}, 0, math.rad(rot_3d), 0)

    -- Project 3D coordinate to 2D virtual screen!
    local sx, sy, is_vis = crayon.graphics.project(obj_pos[1], obj_pos[2] + 0.65, obj_pos[3])
    if is_vis then
        -- Draw 2D floating HUD tag over the 3D entity!
        crayon.graphics.setColor(0.08, 0.12, 0.22, 0.85)
        crayon.graphics.drawRoundedRect("fill", sx - 32, sy - 10, 64, 18, 4)
        crayon.graphics.setColor(0.3, 0.8, 1.0, 1.0)
        crayon.graphics.drawRoundedRect("line", sx - 32, sy - 10, 64, 18, 4)
        crayon.graphics.drawText("3D TARGET", sx - 24, sy - 5, 0.9)
    end

    -- 6. Interactive Text Input Card (Bottom Left)
    crayon.graphics.drawGradientV(8, 175, 210, 36, {0.12, 0.16, 0.28, 0.9}, {0.06, 0.08, 0.16, 0.95})
    crayon.graphics.setColor(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.drawRoundedRect("line", 8, 175, 210, 36, 4)

    crayon.graphics.setColor(0.9, 0.9, 1.0, 1.0)
    local display_text = text_buffer
    if (cursor_timer % 0.8) < 0.4 then
        display_text = display_text .. "_"
    end
    crayon.graphics.drawText(display_text, 14, 182, 1.0)

    -- Modifiers HUD
    local shift_on = crayon.input.isShiftDown()
    local ctrl_on  = crayon.input.isCtrlDown()
    local alt_on   = crayon.input.isAltDown()
    local caps_on  = crayon.input.isCapsLock()
    crayon.graphics.setColor(0.6, 0.7, 0.85, 1.0)
    crayon.graphics.drawText(string.format("MODS: [Shift:%s] [Ctrl:%s] [Alt:%s] [Caps:%s]",
        shift_on and "ON" or "-", ctrl_on and "ON" or "-", alt_on and "ON" or "-", caps_on and "ON" or "-"),
        14, 198, 0.75)

    -- 7. Bottom Controls Help Card
    crayon.graphics.setColor(0.08, 0.10, 0.18, 0.92)
    crayon.graphics.drawRect("fill", 4, 215, 312, 22)
    crayon.graphics.setColor(0.2, 0.4, 0.7, 1.0)
    crayon.graphics.drawRect("line", 4, 215, 312, 22)

    crayon.graphics.setColor(0.5, 0.9, 1.0, 1.0)
    local op_pct = math.floor(window_opacity * 100)
    crayon.graphics.drawText(string.format("[O/P] Opacity: %d%% | [T] Top: %s | [SPACE] Trans: %s",
        op_pct, always_on_top and "ON" or "OFF", is_transparent_mode and "ON" or "OFF"), 8, 219, 0.9)
    crayon.graphics.drawText("Right-Click Pet to Pat | Drag bar to move", 8, 229, 0.8)
end