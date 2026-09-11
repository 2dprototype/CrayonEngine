-- ============================================================================
-- Crayon Engine: 100-Cell Snakes & Ladders (Animation Edition)
-- Features: 320x240, 4-Player, Path Interpolation, Tapering Snake Geometry
-- ============================================================================

local config = {
    cell_size = 22,
    offset_x = 4,
    offset_y = 10
}

local state = {
    mode = "waiting", -- Added: "climbing", "falling"
    dice_value = 1,
    roll_timer = 0,
    current_turn = 1,
    message = "P1 TURN\nSPACE\nTO ROLL",
    blink_timer = 0,
    dice_rx = 0, dice_ry = 0, dice_rz = 0
}

local players = {}
local player_colors = {
    {1.0, 0.2, 0.2, 1.0}, {0.2, 0.5, 1.0, 1.0}, 
    {1.0, 0.9, 0.1, 1.0}, {0.8, 0.2, 0.8, 1.0} 
}

local board = {
    links = {
        [4] = 14, [9] = 31, [20] = 38, [28] = 84, [40] = 59, [51] = 67, [63] = 81, [71] = 91,
        [17] = 7, [62] = 19, [87] = 24, [54] = 34, [64] = 60, [93] = 73, [95] = 75, [99] = 78
    }
}

-- ============================================================================
-- Math & Pathing Helpers
-- ============================================================================
local function lerp(a, b, t) return a + (b - a) * t end

local function get_cell_pos(cell)
    cell = math.max(1, math.min(100, cell))
    local index = cell - 1
    local row = math.floor(index / 10)
    local col = index % 10
    
    if row % 2 == 1 then col = 9 - col end
    
    local px = config.offset_x + col * config.cell_size + (config.cell_size / 2)
    local py = config.offset_y + (9 - row) * config.cell_size + (config.cell_size / 2)
    return px, py
end

-- Calculates the exact X,Y point along the snake's slithering body at time t (0 to 1)
local function get_snake_path_pos(sx, sy, ex, ey, t)
    local dx, dy = ex - sx, ey - sy
    local dist = math.sqrt(dx*dx + dy*dy)
    local nx, ny = -dy / dist, dx / dist
    local cx, cy = sx + dx * t, sy + dy * t
    
    local amp = 5
    local freq = 3.5
    local offset = math.sin(t * math.pi * 2 * freq) * amp
    return cx + nx * offset, cy + ny * offset
end

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
-- Core Engine Loops
-- ============================================================================
function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("Snakes & Ladders - Animated")
    
    for i = 1, 4 do
        local px, py = get_cell_pos(1)
        table.insert(players, {
            id = i, cell = 1, target_cell = 1,
            x = px, y = py, move_timer = 0, anim_t = 0,
            color = player_colors[i]
        })
    end
end

local function next_turn()
    state.current_turn = (state.current_turn % 4) + 1
    state.mode = "waiting"
    state.message = "P" .. state.current_turn .. " TURN\nSPACE\nTO ROLL"
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
            state.mode = "moving"
            p.target_cell = math.min(100, p.cell + state.dice_value)
            state.message = "P" .. p.id .. " ROLLED:\n  " .. state.dice_value
        end
        
    elseif state.mode == "moving" then
        p.move_timer = p.move_timer + dt
        if p.move_timer > 0.15 then
            p.move_timer = 0
            if p.cell < p.target_cell then
                p.cell = p.cell + 1
                p.x, p.y = get_cell_pos(p.cell)
            else
                local link = board.links[p.cell]
                if link then
                    p.target_cell = link
                    p.anim_t = 0
                    if link > p.cell then
                        state.mode = "climbing"
                        state.message = "CLIMBING!"
                    else
                        state.mode = "falling"
                        state.message = "SLIDING DOWN!"
                    end
                else
                    if p.cell == 100 then
                        state.mode = "gameover"
                        state.message = "P" .. p.id .. " WINS!\nSPACE\nTO RESTART"
                    else
                        next_turn()
                    end
                end
            end
        end
        
    elseif state.mode == "climbing" then
        p.anim_t = p.anim_t + dt * 1.2 -- Climbing speed
        if p.anim_t >= 1 then
            p.cell = p.target_cell
            p.x, p.y = get_cell_pos(p.cell)
            state.mode = "pass_turn"
            state.roll_timer = 0.5 
        else
            local sx, sy = get_cell_pos(p.cell)
            local ex, ey = get_cell_pos(p.target_cell)
            p.x = lerp(sx, ex, p.anim_t)
            p.y = lerp(sy, ey, p.anim_t)
        end
        
    elseif state.mode == "falling" then
        p.anim_t = p.anim_t + dt * 0.8 -- Falling speed (follows spiral)
        if p.anim_t >= 1 then
            p.cell = p.target_cell
            p.x, p.y = get_cell_pos(p.cell)
            state.mode = "pass_turn"
            state.roll_timer = 0.5
        else
            local sx, sy = get_cell_pos(p.cell)
            local ex, ey = get_cell_pos(p.target_cell)
            p.x, p.y = get_snake_path_pos(sx, sy, ex, ey, p.anim_t)
        end
        
    elseif state.mode == "pass_turn" then
        state.roll_timer = state.roll_timer - dt
        if state.roll_timer <= 0 then next_turn() end

    elseif state.mode == "gameover" then
        if crayon.input.is_pressed("space") then
            for i = 1, 4 do
                players[i].cell = 1
                players[i].target_cell = 1
                players[i].x, players[i].y = get_cell_pos(1)
            end
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
    for i, v in ipairs(verts) do
        local rx, ry, rz = rotate_3d(v[1], v[2], v[3], state.dice_rx, state.dice_ry, state.dice_rz)
        table.insert(proj, {cx + rx * size, cy + ry * size})
    end
    crayon.graphics.set_color(1, 1, 1, 1)
    for _, e in ipairs(edges) do
        crayon.graphics.draw_line(proj[e[1]][1], proj[e[1]][2], proj[e[2]][1], proj[e[2]][2], 1.5)
    end
end

local function draw_ladder(sx, sy, ex, ey)
    local dx, dy = ex - sx, ey - sy
    local dist = math.sqrt(dx*dx + dy*dy)
    local nx, ny = -dy / dist, dx / dist
    local hw = 4

    crayon.graphics.set_color(0.8, 0.8, 0.8, 1) 
    crayon.graphics.draw_line(sx + nx*hw, sy + ny*hw, ex + nx*hw, ey + ny*hw, 1.0)
    crayon.graphics.draw_line(sx - nx*hw, sy - ny*hw, ex - nx*hw, ey - ny*hw, 1.0)

    local steps = math.floor(dist / 7)
    for i = 1, steps do
        local t = i / (steps + 1)
        local rx, ry = sx + dx * t, sy + dy * t
        crayon.graphics.draw_line(rx + nx*hw, ry + ny*hw, rx - nx*hw, ry - ny*hw, 1.0)
    end
end

local function draw_snake(sx, sy, ex, ey)
    local dx, dy = ex - sx, ey - sy
    local dist = math.sqrt(dx*dx + dy*dy)
    local nx, ny = -dy / dist, dx / dist

    local points = {}
    local segments = math.floor(dist / 3)
    for i = 0, segments do
        local t = i / segments
        local cx, cy = sx + dx * t, sy + dy * t
        
        local amp = 4
        local freq = 3.5  
        local offset = math.sin(t * math.pi * 2 * freq) * amp
        table.insert(points, {cx + nx * offset, cy + ny * offset})
    end

    -- Mono Green Snake Styling
    crayon.graphics.set_color(0.1, 0.9, 0.2, 1.0)
    crayon.graphics.draw_polyline(points, 2.0, false)

    -- crayon.graphics.set_color(0, 0, 0, 1)
    -- crayon.graphics.draw_circle("fill", sx, sy, 4)
    -- crayon.graphics.set_color(0.1, 0.9, 0.2, 1.0)
    -- crayon.graphics.draw_circle("line", sx, sy, 4)
    
    local tx, ty = sx - (dx/dist)*6, sy - (dy/dist)*6
    crayon.graphics.draw_line(sx, sy, tx, ty, 1.0)
end

function crayon.draw()
    crayon.graphics.clear(0.05, 0.05, 0.05, 1) 
    
    -- Draw Board & Connections
    for i = 1, 100 do
        local cx, cy = get_cell_pos(i)
        local half = config.cell_size / 2
        crayon.graphics.set_color(0.3, 0.3, 0.3, 1)
        crayon.graphics.draw_rect("line", cx - half, cy - half, config.cell_size, config.cell_size)
        crayon.graphics.set_color(0.6, 0.6, 0.6, 1)
        crayon.graphics.draw_text(tostring(i), cx - half + 2, cy - half + 2, 1.0)
    end
    
    for start_c, end_c in pairs(board.links) do
        local sx, sy = get_cell_pos(start_c)
        local ex, ey = get_cell_pos(end_c)
        if start_c < end_c then draw_ladder(sx, sy, ex, ey) else draw_snake(sx, sy, ex, ey) end
    end
    
    -- UI Panel
    crayon.graphics.set_color(1, 1, 1, 1)
    crayon.graphics.draw_line(230, 0, 230, 240, 2.0)
    crayon.graphics.draw_text("SNAKES", 240, 10, 1.0)
    crayon.graphics.draw_text("  &", 240, 22, 1.0)
    crayon.graphics.draw_text("LADDERS", 240, 34, 1.0)
    
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
    
    -- Draw Player Tokens
    for i, p in ipairs(players) do
        local ox = (i % 2 == 0) and 4 or -4
        local oy = (i > 2) and 4 or -4
        
        if i == state.current_turn and math.floor(state.blink_timer * 6) % 2 == 0 then
            crayon.graphics.set_color(1, 1, 1, 1)
            crayon.graphics.draw_rect("fill", p.x + ox - 5, p.y + oy - 5, 10, 10)
        end
        crayon.graphics.set_color(p.color[1], p.color[2], p.color[3], p.color[4])
        crayon.graphics.draw_rect("fill", p.x + ox - 3, p.y + oy - 3, 6, 6)
    end
end