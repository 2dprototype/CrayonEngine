-- ============================================================================
-- Example 08: Window Management, Display Bounds & Scaling Modes
-- Crayon Engine
-- ============================================================================

local scaling_modes = {"integer", "aspect", "stretch", "center"}
local cur_mode_idx = 1
local resolutions = {
    {320, 240, "320x240 (Retro 4:3)"},
    {640, 480, "640x480 (VGA 4:3)"},
    {426, 240, "426x240 (16:9 Retro Widescreen)"}
}
local cur_res_idx = 1
local window_sizes = {
    {640, 480},
    {960, 720},
    {1280, 720}
}
local cur_win_idx = 2

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_scaling_mode("integer")
    crayon.window.set_title("08 - Window & Scaling Management")
end

function crayon.update(dt)
    -- 1: Cycle Scaling Mode
    if crayon.input.is_pressed("1") or crayon.input.is_pressed("tab") then
        cur_mode_idx = (cur_mode_idx % #scaling_modes) + 1
        crayon.window.set_scaling_mode(scaling_modes[cur_mode_idx])
    end

    -- 2: Cycle Internal Resolution
    if crayon.input.is_pressed("2") then
        cur_res_idx = (cur_res_idx % #resolutions) + 1
        local r = resolutions[cur_res_idx]
        crayon.window.set_resolution(r[1], r[2])
    end

    -- 3: Cycle Window Size
    if crayon.input.is_pressed("3") then
        cur_win_idx = (cur_win_idx % #window_sizes) + 1
        local ws = window_sizes[cur_win_idx]
        crayon.window.set_window_size(ws[1], ws[2])
        crayon.window.center()
    end

    -- F: Toggle Fullscreen
    if crayon.input.is_pressed("f") then
        crayon.window.set_fullscreen(not crayon.window.is_fullscreen())
    end

    -- V: Toggle VSync
    if crayon.input.is_pressed("v") then
        crayon.window.set_vsync(not crayon.window.get_vsync())
    end

    -- B: Toggle Bordered
    if crayon.input.is_pressed("b") then
        crayon.window.set_bordered(not crayon.window.is_bordered())
    end

    -- C: Center Window
    if crayon.input.is_pressed("c") then
        crayon.window.center()
    end

    -- M: Maximize / Restore
    if crayon.input.is_pressed("m") then
        if crayon.window.is_maximized() then
            crayon.window.restore()
        else
            crayon.window.maximize()
        end
    end

    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    local rw, rh = crayon.window.get_resolution()
    local ww, wh = crayon.window.get_window_size()
    local dw, dh = crayon.window.get_display_size()

    crayon.graphics.clear(0.1, 0.12, 0.18)

    -- Draw border grid pattern matching virtual canvas edges
    crayon.graphics.set_color(0.2, 0.35, 0.5, 0.8)
    crayon.graphics.draw_rect("line", 2, 2, rw - 4, rh - 4)

    -- Diagonal test lines across canvas
    crayon.graphics.set_color(0.2, 0.25, 0.35, 0.5)
    crayon.graphics.draw_line(0, 0, rw, rh, 1.0)
    crayon.graphics.draw_line(0, rh, rw, 0, 1.0)

    -- Central Target
    local cx, cy = rw / 2, rh / 2
    crayon.graphics.set_color(0.8, 0.3, 0.4, 0.7)
    crayon.graphics.draw_circle("line", cx, cy, 50)
    crayon.graphics.draw_circle("line", cx, cy, 30)
    crayon.graphics.draw_circle("fill", cx, cy, 4)

    -- Header
    crayon.graphics.set_color(0.06, 0.08, 0.14, 0.9)
    crayon.graphics.draw_rect("fill", 10, 10, rw - 20, 24)
    crayon.graphics.set_color(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.draw_rect("line", 10, 10, rw - 20, 24)

    crayon.graphics.set_color(1.0, 0.9, 0.3, 1.0)
    crayon.graphics.draw_text("WINDOW & SCALING CONTROLS", 16, 16, 1.0)

    -- Info Card
    local iy = 42
    crayon.graphics.set_color(0.06, 0.08, 0.14, 0.9)
    crayon.graphics.draw_rounded_rect("fill", 10, iy, rw - 20, rh - 54, 4)
    crayon.graphics.set_color(0.3, 0.5, 0.8, 1.0)
    crayon.graphics.draw_rounded_rect("line", 10, iy, rw - 20, rh - 54, 4)

    local info = {
        {"Scaling Mode [1/TAB]:", string.upper(crayon.window.get_scaling_mode()), {1.0, 0.85, 0.2}},
        {"Virtual Canvas [2]:", rw .. " x " .. rh .. " (" .. resolutions[cur_res_idx][3] .. ")", {0.4, 1.0, 0.5}},
        {"Window Size [3]:", ww .. " x " .. wh, {0.5, 0.8, 1.0}},
        {"Display Bounds:", dw .. " x " .. dh, {0.8, 0.8, 0.9}},
        {"Fullscreen [F]:", crayon.window.is_fullscreen() and "YES" or "NO", {0.9, 0.9, 0.9}},
        {"VSync [V]:", crayon.window.get_vsync() and "ENABLED" or "DISABLED", {0.9, 0.9, 0.9}},
        {"Bordered [B]:", crayon.window.is_bordered() and "YES" or "NO", {0.9, 0.9, 0.9}},
        {"Maximized [M]:", crayon.window.is_maximized() and "YES" or "NO", {0.9, 0.9, 0.9}},
        {"Center Window [C]:", "Press C to re-center", {0.7, 0.7, 0.8}}
    }

    for i, item in ipairs(info) do
        local line_y = iy + 8 + (i - 1) * 17
        crayon.graphics.set_color(0.8, 0.85, 0.9, 1.0)
        crayon.graphics.draw_text(item[1], 18, line_y, 1.0)
        crayon.graphics.set_color(item[3][1], item[3][2], item[3][3], 1.0)
        crayon.graphics.draw_text(item[2], 140, line_y, 1.0)
    end
end
