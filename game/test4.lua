-- ============================================================================
-- Crayon Engine: 4-Player Mini Ludo
-- Features: 320x240, 1 Token/Player, Safe Zones, Capturing, 3D Dice
-- ============================================================================

local config = {
    cell_size = 18
}

local state = {
    mode = "waiting", -- "rolling", "moving", "pass_turn", "scoreboard"
    dice_value = 1,
    roll_timer = 0,
    current_turn = 1,
    message = "P1 TURN\nSPACE\nTO ROLL",
    blink_timer = 0,
    dice_rx = 0, dice_ry = 0, dice_rz = 0,
    finish_order = {},
    pending_scoreboard = false,
    scoreboard_text = ""
}

local players = {}
local player_colors = {
    {1.0, 0.2, 0.2, 1.0}, -- P1: Red
    {0.2, 0.9, 0.2, 1.0}, -- P2: Green
    {1.0, 0.9, 0.1, 1.0}, -- P3: Yellow
    {0.2, 0.5, 1.0, 1.0}  -- P4: Blue
}

-- ============================================================================
-- Board Coordinate Generation
-- ============================================================================
local track_coords = {}
local home_coords = { {}, {}, {}, {} }
local base_coords = { {60, 60}, {180, 60}, {180, 180}, {60, 180} }

local function generate_board()
    -- Perimeter track (40 cells total, 0 to 39)
    -- Left edge going up
    for i = 0, 4 do track_coords[i] = {20, 120 - i * 20} end
    track_coords[5] = {20, 20}
    -- Top edge going right
    for i = 1, 9 do track_coords[5 + i] = {20 + i * 20, 20} end
    track_coords[15] = {220, 20}
    -- Right edge going down
    for i = 1, 9 do track_coords[15 + i] = {220, 20 + i * 20} end
    track_coords[25] = {220, 220}
    -- Bottom edge going left
    for i = 1, 9 do track_coords[25 + i] = {220 - i * 20, 220} end
    track_coords[35] = {20, 220}
    -- Left edge going up to close loop
    for i = 1, 4 do track_coords[35 + i] = {20, 220 - i * 20} end

    -- Home stretches (5 cells each towards center)
    for i = 1, 5 do
        home_coords[1][i] = {20 + i * 20, 120}  -- P1 moves Right
        home_coords[2][i] = {120, 20 + i * 20}  -- P2 moves Down
        home_coords[3][i] = {220 - i * 20, 120} -- P3 moves Left
        home_coords[4][i] = {120, 220 - i * 20} -- P4 moves Up
    end
end

local function get_cell_pos(player_id, cell)
    if cell == 0 then
        return base_coords[player_id][1], base_coords[player_id][2]
    elseif cell <= 40 then
        -- Map logical 1-40 to absolute 0-39 based on player offset
        local offset = (player_id - 1) * 10
        local abs_index = (offset + cell - 1) % 40
        return track_coords[abs_index][1], track_coords[abs_index][2]
    elseif cell <= 45 then
        return home_coords[player_id][cell - 40][1], home_coords[player_id][cell - 40][2]
    end
    return 120, 120 -- Center (Win)
end

-- ============================================================================
-- Math Helpers
-- ============================================================================
local function rotate_3d(x, y, z, ax, ay, az)
    local y1 = y * math.cos(ax) - z * math.sin(ax)
    local z1 = y * math.sin(ax) + z * math.cos(ax)
    local x2 = x * math.cos(ay) + z1 * math.sin(ay)
    local z2 = -x * math.sin(ay) + z1 * math.cos(ay)
    local x3 = x2 * math.cos(az) - y1 * math.sin(az)
    local y3 = x2 * math.sin(az) + y1 * math.cos(az)
    return x3, y3, z2
end

-- ============================================================================
-- Game Flow Helpers
-- ============================================================================
local PLACE_NAME = { "1ST", "2ND", "3RD", "4TH" }

local function advance_turn()
    local n = #players
    for _ = 1, n do
        state.current_turn = (state.current_turn % n) + 1
        if not players[state.current_turn].finished then break end
    end
    state.mode = "waiting"
    state.message = "P" .. state.current_turn .. " TURN\nSPACE\nTO ROLL"
end

local function finish_player(p)
    p.finished = true
    table.insert(state.finish_order, p.id)
    local place = #state.finish_order
    state.message = "P" .. p.id .. " FINISHES!\n" .. (PLACE_NAME[place] or (place .. "TH"))
    state.mode = "pass_turn"
    state.roll_timer = 1.3
    
    if place >= 3 then
        state.pending_scoreboard = true
    end
end

local function resolve_landing(p)
    if p.cell == 45 then
        finish_player(p)
        return
    end

    -- Capture Logic
    if p.cell >= 1 and p.cell <= 40 then
        local my_abs = ((p.id - 1) * 10 + p.cell - 1) % 40
        local is_safe = (my_abs == 0 or my_abs == 10 or my_abs == 20 or my_abs == 30)

        if not is_safe then
            for _, other in ipairs(players) do
                if other.id ~= p.id and other.cell >= 1 and other.cell <= 40 then
                    local other_abs = ((other.id - 1) * 10 + other.cell - 1) % 40
                    if my_abs == other_abs then
                        other.cell = 0
                        other.target_cell = 0
                        other.x, other.y = get_cell_pos(other.id, 0)
                        state.message = "P" .. p.id .. " CAPTURED\nP" .. other.id .. "!"
                        state.mode = "pass_turn"
                        state.roll_timer = 1.2
                        return
                    end
                end
            end
        end
    end

    state.mode = "pass_turn"
    state.roll_timer = 0.5
end

-- ============================================================================
-- Engine Loops
-- ============================================================================
function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("Ludo - Animated")
    math.randomseed(math.floor(crayon.time.get_time() * 100000))

    generate_board()

    for i = 1, 4 do
        local px, py = get_cell_pos(i, 0)
        table.insert(players, {
            id = i, cell = 0, target_cell = 0,
            x = px, y = py, move_timer = 0,
            color = player_colors[i], finished = false
        })
    end
end

function crayon.update(dt)
    if crayon.input.is_pressed("escape") then crayon.window.quit() end
    state.blink_timer = state.blink_timer + dt

    local p = players[state.current_turn]

    if state.mode == "waiting" then
        if crayon.input.is_pressed("space") then
            state.mode = "rolling"
            state.roll_timer = 1.0
            state.message = "ROLLING..."
        end

    elseif state.mode == "rolling" then
        state.roll_timer = state.roll_timer - dt
        state.dice_rx = state.dice_rx + 8 * dt
        state.dice_ry = state.dice_ry + 10 * dt
        state.dice_rz = state.dice_rz + 6 * dt

        if state.roll_timer <= 0 then
            state.dice_value = math.random(1, 6)

            if p.cell == 0 then
                if state.dice_value == 6 then
                    p.target_cell = 1
                    state.mode = "moving"
                    state.message = "P" .. p.id .. " ROLLED 6!\nLEAVING BASE"
                else
                    state.mode = "pass_turn"
                    state.roll_timer = 0.9
                    state.message = "P" .. p.id .. " ROLLED: " .. state.dice_value .. "\nNEED 6\nTO START"
                end
            else
                local dest = p.cell + state.dice_value
                if dest > 45 then
                    state.mode = "pass_turn"
                    state.roll_timer = 0.9
                    state.message = "P" .. p.id .. " ROLLED: " .. state.dice_value .. "\nNEED EXACT\nNO MOVE"
                else
                    p.target_cell = dest
                    state.mode = "moving"
                    state.message = "P" .. p.id .. " ROLLED: " .. state.dice_value
                end
            end
        end

    elseif state.mode == "moving" then
        p.move_timer = p.move_timer + dt
        if p.move_timer > 0.15 then
            p.move_timer = 0
            if p.cell < p.target_cell then
                p.cell = p.cell + 1
                p.x, p.y = get_cell_pos(p.id, p.cell)
                if p.cell == p.target_cell then
                    resolve_landing(p)
                end
            end
        end

    elseif state.mode == "pass_turn" then
        state.roll_timer = state.roll_timer - dt
        if state.roll_timer <= 0 then
            if state.pending_scoreboard then
                state.scoreboard_text = "SCOREBOARD"
                for i, pid in ipairs(state.finish_order) do
                    state.scoreboard_text = state.scoreboard_text .. "\n" .. (PLACE_NAME[i] or (i .. "TH")) .. ": P" .. pid
                end
                state.mode = "scoreboard"
            else
                -- Ludo Rule: Rolling a 6 grants an extra turn
                if state.dice_value == 6 and not p.finished then
                    state.mode = "waiting"
                    state.message = "P" .. p.id .. " TURN\nROLLED 6!\nROLL AGAIN"
                else
                    advance_turn()
                end
            end
        end

    elseif state.mode == "scoreboard" then
        if crayon.input.is_pressed("space") then
            for i = 1, #players do
                players[i].cell = 0
                players[i].target_cell = 0
                players[i].finished = false
                players[i].x, players[i].y = get_cell_pos(i, 0)
            end
            state.finish_order = {}
            state.pending_scoreboard = false
            state.current_turn = 1
            state.mode = "waiting"
            state.message = "P1 TURN\nSPACE\nTO ROLL"
        end
    end
end

-- ============================================================================
-- Rendering
-- ============================================================================
local function draw_3d_dice(cx, cy, size)
    local verts = { {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1}, {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1} }
    local edges = { {1,2}, {2,3}, {3,4}, {4,1}, {5,6}, {6,7}, {7,8}, {8,5}, {1,5}, {2,6}, {3,7}, {4,8} }
    local proj = {}
    for _, v in ipairs(verts) do
        local rx, ry, rz = rotate_3d(v[1], v[2], v[3], state.dice_rx, state.dice_ry, state.dice_rz)
        table.insert(proj, {cx + rx * size, cy + ry * size})
    end
    crayon.graphics.set_color(1, 1, 1, 1)
    for _, e in ipairs(edges) do
        crayon.graphics.draw_line(proj[e[1]][1], proj[e[1]][2], proj[e[2]][1], proj[e[2]][2], 1.5)
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.15, 1)

    -- Draw Bases
    for i, c in ipairs(player_colors) do
        crayon.graphics.set_color(c[1], c[2], c[3], 0.3)
        crayon.graphics.draw_rect("fill", base_coords[i][1] - 25, base_coords[i][2] - 25, 50, 50)
        crayon.graphics.set_color(c[1], c[2], c[3], 1.0)
        crayon.graphics.draw_rect("line", base_coords[i][1] - 25, base_coords[i][2] - 25, 50, 50)
    end

    local half = config.cell_size / 2

    -- Draw Track
    for i = 0, 39 do
        local cx, cy = track_coords[i][1], track_coords[i][2]
        
        -- Identify safe zones (starts)
        if i == 0 then crayon.graphics.set_color(player_colors[1][1], player_colors[1][2], player_colors[1][3], 0.4)
        elseif i == 10 then crayon.graphics.set_color(player_colors[2][1], player_colors[2][2], player_colors[2][3], 0.4)
        elseif i == 20 then crayon.graphics.set_color(player_colors[3][1], player_colors[3][2], player_colors[3][3], 0.4)
        elseif i == 30 then crayon.graphics.set_color(player_colors[4][1], player_colors[4][2], player_colors[4][3], 0.4)
        else crayon.graphics.set_color(0.2, 0.2, 0.2, 1) end

        crayon.graphics.draw_rect("fill", cx - half, cy - half, config.cell_size, config.cell_size)
        crayon.graphics.set_color(0.5, 0.5, 0.5, 1)
        crayon.graphics.draw_rect("line", cx - half, cy - half, config.cell_size, config.cell_size)
    end

    -- Draw Home Stretches
    for i = 1, 4 do
        crayon.graphics.set_color(player_colors[i][1], player_colors[i][2], player_colors[i][3], 0.4)
        for j = 1, 5 do
            local cx, cy = home_coords[i][j][1], home_coords[i][j][2]
            crayon.graphics.draw_rect("fill", cx - half, cy - half, config.cell_size, config.cell_size)
            crayon.graphics.set_color(player_colors[i][1], player_colors[i][2], player_colors[i][3], 0.8)
            crayon.graphics.draw_rect("line", cx - half, cy - half, config.cell_size, config.cell_size)
        end
    end

    -- Draw Center (Win Zone)
    crayon.graphics.set_color(1, 1, 1, 0.8)
    crayon.graphics.draw_circle("fill", 120, 120, config.cell_size)

    -- UI Panel
    crayon.graphics.set_color(1, 1, 1, 1)
    crayon.graphics.draw_line(230, 0, 230, 240, 2.0)
    crayon.graphics.draw_text("MINI", 240, 10, 1.0)
    crayon.graphics.draw_text("LUDO", 240, 24, 1.0)

    if state.mode == "rolling" then
        draw_3d_dice(270, 90, 16)
    else
        local c = players[state.current_turn].color
        crayon.graphics.set_color(c[1], c[2], c[3], 1.0)
        crayon.graphics.draw_rect("line", 250, 70, 40, 40)
        crayon.graphics.draw_text(tostring(state.dice_value), 262, 80, 3.0)
    end

    crayon.graphics.set_color(1, 1, 1, 1)
    local msg_y = 130
    for line in string.gmatch(state.message, "[^\n]+") do
        crayon.graphics.draw_text(line, 238, msg_y, 1.0)
        msg_y = msg_y + 12
    end

    -- Draw Tokens
    for i, p in ipairs(players) do
        -- Visual offset if they stack (mostly for bases or safe zones)
        local ox = (i % 2 == 0) and 3 or -3
        local oy = (i > 2) and 3 or -3

        if i == state.current_turn and state.mode ~= "scoreboard" and math.floor(state.blink_timer * 6) % 2 == 0 then
            crayon.graphics.set_color(1, 1, 1, 1)
            crayon.graphics.draw_circle("fill", p.x + ox, p.y + oy, 8)
        end
        crayon.graphics.set_color(p.color[1], p.color[2], p.color[3], 1.0)
        crayon.graphics.draw_circle("fill", p.x + ox, p.y + oy, 6)

        if p.finished then
            crayon.graphics.set_color(1, 1, 1, 1)
            crayon.graphics.draw_circle("line", p.x + ox, p.y + oy, 8)
        end
    end

    -- Scoreboard Overlay
    if state.mode == "scoreboard" then
        crayon.graphics.set_color(0, 0, 0, 0.85)
        crayon.graphics.draw_rect("fill", 40, 40, 240, 160)
        crayon.graphics.set_color(1, 1, 1, 1)
        crayon.graphics.draw_rect("line", 40, 40, 240, 160)
        
        local y = 50
        for line in string.gmatch(state.scoreboard_text, "[^\n]+") do
            crayon.graphics.draw_text(line, 80, y, 1.5)
            y = y + 25
        end
        crayon.graphics.set_color(0.7, 0.7, 0.7, 1)
        crayon.graphics.draw_text("SPACE TO PLAY AGAIN", 78, 175, 1.0)
    end
end