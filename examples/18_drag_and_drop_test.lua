-- ============================================================================
-- Drag & Drop demo for Crayon Engine
-- ============================================================================
-- Drag any file from your OS file manager onto this window. The engine will
-- fire crayon.dropbegin / dropposition / dropfile / droptext / dropcomplete,
-- and this script draws a log of what happened.
--
-- Requires the five engine-side additions:
--   1. lua_runtime.hpp  — five call_drop_* declarations
--   2. lua_runtime.cpp  — five call_drop_* implementations
--   3. engine.cpp       — five SDL_EVENT_DROP_* dispatch branches
-- ============================================================================

function crayon.config(t)
    t.window.title         = "Drag & Drop Demo"
    t.window.width         = 480
    t.window.height        = 360
    t.window.virtualWidth  = 480
    t.window.virtualHeight = 360
    t.window.resizable     = true
    t.window.vsync         = true
    t.window.scaling       = "integer"

    t.modules.mesh3D       = false
    t.modules.audio        = false
    t.modules.particles    = false
    t.modules.physics3D    = false
    t.modules.physics2D    = false

    t.graphics.clearColor  = {0.06, 0.07, 0.10, 1.0}
end

-- ----------------------------------------------------------------------------
-- State
-- ----------------------------------------------------------------------------
local VW, VH = 480, 360

local drag = {
    active       = false,   -- true between dropbegin and dropcomplete
    x            = 0,       -- latest window-local drag position
    y            = 0,
    hover_count  = 0,       -- how many position updates this drag
    started_at   = 0,
}

-- Rolling event log, newest at the top
local log = {}
local LOG_MAX = 12

local stats = {
    total_files = 0,
    total_texts = 0,
    last_file   = nil,
    last_text   = nil,
    last_file_size_hint = nil,
    file_counts_by_ext  = {},
}

-- ----------------------------------------------------------------------------
-- Helpers
-- ----------------------------------------------------------------------------
local function push_log(kind, message, r, g, b)
    table.insert(log, 1, {
        kind    = kind,
        message = message,
        r = r or 1, g = g or 1, b = b or 1,
        t       = crayon.time.getTime(),
    })
    while #log > LOG_MAX do
        table.remove(log)
    end
end

-- Extract the trailing component of a path, handling both / and \ separators.
local function basename(path)
    if not path or path == "" then return "" end
    return (path:match("([^/\\]+)[/\\]*$")) or path
end

-- Split off the last extension. "photo.png" -> "photo", "png"
local function split_ext(name)
    local stem, ext = name:match("^(.*)%.([^%.]+)$")
    if stem then return stem, ext end
    return name, ""
end

-- Basic size sanity check just so the log has something interesting to say.
local function describe_path(path)
    local name = basename(path)
    local stem, ext = split_ext(name)
    if ext == "" then ext = "(no ext)" end
    return stem, ext, name
end

-- ----------------------------------------------------------------------------
-- Engine drop callbacks
-- ----------------------------------------------------------------------------

-- Fired once when a drag operation enters the window.
function crayon.dropbegin(x, y)
    drag.active      = true
    drag.x           = x
    drag.y           = y
    drag.hover_count = 0
    drag.started_at  = crayon.time.getTime()

    push_log("begin", string.format("dropbegin at (%.0f, %.0f)", x, y),
             0.55, 0.85, 1.0)
end

-- Fired repeatedly while the drag moves over the window.
function crayon.dropPosition(x, y)
    drag.x = x
    drag.y = y
    drag.hover_count = drag.hover_count + 1
    -- Not logged individually; too noisy. See hover_count in HUD.
end

-- Fired once per file that was part of the drop payload.
-- If the user drags 5 files at once, this fires 5 times back to back.
function crayon.dropFile(path, x, y)
    stats.total_files = stats.total_files + 1

    local stem, ext, name = describe_path(path)
    stats.last_file = path

    stats.file_counts_by_ext[ext] =
        (stats.file_counts_by_ext[ext] or 0) + 1

    -- A short summary line for the log: name (ext)
    local summary = string.format("%s  [%s]", name, ext)
    push_log("file", summary, 1.0, 0.85, 0.35)

    -- And one detail line with the full path, truncated for the log.
    local shown = path
    if #shown > 56 then
        shown = "…" .. shown:sub(#shown - 54)
    end
    push_log("path", shown, 0.6, 0.6, 0.65)
end

-- Fired once per text payload in a drop. Most OS file managers send files,
-- but text editors and browsers often send text/uri-list or plain text.
function crayon.dropText(text, x, y)
    stats.total_texts = stats.total_texts + 1
    stats.last_text = text

    local preview = text:gsub("%s+", " ")
    if #preview > 60 then
        preview = preview:sub(1, 59) .. "…"
    end
    push_log("text", preview, 0.7, 1.0, 0.7)
end

-- Fired once when the whole drag operation finishes (after all dropfiles
-- and droptexts). Also fires if the user cancels the drag by dropping
-- outside the window, or by pressing Escape.
function crayon.dropComplete()
    if drag.active then
        local elapsed = crayon.time.getTime() - drag.started_at
        push_log("complete",
            string.format("dropcomplete  (%d hover updates, %.2fs)",
                drag.hover_count, elapsed),
            0.85, 0.85, 0.95)
    end
    drag.active = false
    drag.hover_count = 0
end

-- ----------------------------------------------------------------------------
-- Init
-- ----------------------------------------------------------------------------
function crayon.init()
    push_log("info", "Drag a file from your OS onto this window.", 0.6, 0.6, 0.7)
end

-- ----------------------------------------------------------------------------
-- Update
-- ----------------------------------------------------------------------------
function crayon.update(dt)
    -- No simulation here, but we do want to keep an eye on the cursor when
    -- a drag is active. Reading global mouse position works even while the
    -- OS has grabbed the cursor for the drag operation, which is useful for
    -- showing a live pointer readout.
    if drag.active then
        local mx, my = crayon.mouse.getPosition()
        drag.x, drag.y = mx, my
    end
end

-- ----------------------------------------------------------------------------
-- Draw
-- ----------------------------------------------------------------------------
local function draw_background()
    crayon.graphics.setColor(0.06, 0.07, 0.10, 1.0)
    crayon.graphics.drawRect("fill", 0, 0, VW, VH)

    -- Subtle scanline pattern so the drop zone reads as interactive.
    crayon.graphics.setColor(1, 1, 1, 0.015)
    for y = 0, VH, 3 do
        crayon.graphics.drawRect("fill", 0, y, VW, 1)
    end
end

local function draw_header()
    crayon.graphics.setColor(0.85, 0.85, 0.92, 1.0)
    crayon.graphics.drawText("Drag & Drop Demo", 10, 8, 1.2)

    crayon.graphics.setColor(0.55, 0.55, 0.62, 1.0)
    crayon.graphics.drawText(
        "Drop any file from Explorer / Finder / your file manager.",
        10, 26, 0.8)
end

local function draw_drop_zone()
    local x, y = 10, 44
    local w, h = VW - 20, 66

    -- Frame colour shifts while a drag is in progress.
    local r, g, b, a
    if drag.active then
        -- Pulse gently to signal "release to drop"
        local pulse = 0.5 + 0.5 * math.sin(crayon.time.getTime() * 8)
        r = 0.3 + pulse * 0.5
        g = 0.7 + pulse * 0.25
        b = 1.0
        a = 1.0
    else
        r, g, b, a = 0.35, 0.4, 0.5, 0.8
    end

    -- Dashed border via four rectangles (top, bottom, left, right segments).
    crayon.graphics.setColor(r, g, b, a)
    local dash = 10
    for dx = x, x + w - dash, dash * 2 do
        crayon.graphics.drawRect("fill", dx,        y,         dash, 1)
        crayon.graphics.drawRect("fill", dx,        y + h - 1, dash, 1)
    end
    for dy = y, y + h - dash, dash * 2 do
        crayon.graphics.drawRect("fill", x,         dy, 1, dash)
        crayon.graphics.drawRect("fill", x + w - 1, dy, 1, dash)
    end

    -- Centre message
    crayon.graphics.setColor(r, g, b, 1.0)
    local msg = drag.active and "RELEASE TO DROP" or "DROP ZONE"
    local msg_w = crayon.graphics.getTextWidth(msg, 1.6)
    crayon.graphics.drawText(msg, x + (w - msg_w) * 0.5, y + 14, 1.6)

    crayon.graphics.setColor(0.75, 0.8, 0.85, 0.85)
    local sub = drag.active
        and string.format("cursor at (%.0f, %.0f)  ·  %d updates",
                          drag.x, drag.y, drag.hover_count)
        or  string.format("files: %d   texts: %d",
                          stats.total_files, stats.total_texts)
    local sub_w = crayon.graphics.getTextWidth(sub, 0.8)
    crayon.graphics.drawText(sub, x + (w - sub_w) * 0.5, y + 42, 0.8)
end

local function draw_last_payload()
    local y = 120
    crayon.graphics.setColor(0.6, 0.6, 0.68, 1.0)
    crayon.graphics.drawText("MOST RECENT PAYLOAD", 10, y, 0.7)

    y = y + 14
    crayon.graphics.setColor(0.95, 0.95, 0.95, 1.0)
    if stats.last_file then
        local name = basename(stats.last_file)
        crayon.graphics.drawText("file: " .. name, 10, y, 0.9)
        y = y + 13

        crayon.graphics.setColor(0.55, 0.55, 0.6, 1.0)
        local shown = stats.last_file
        -- wrap the path at ~60 chars
        while #shown > 60 do
            crayon.graphics.drawText(shown:sub(1, 60), 10, y, 0.75)
            shown = shown:sub(61)
            y = y + 11
        end
        if #shown > 0 then
            crayon.graphics.drawText(shown, 10, y, 0.75)
            y = y + 11
        end
    elseif stats.last_text then
        crayon.graphics.setColor(0.95, 0.95, 0.95, 1.0)
        crayon.graphics.drawText("text: " .. stats.last_text:sub(1, 60),
                                 10, y, 0.9)
        y = y + 13
    else
        crayon.graphics.setColor(0.4, 0.4, 0.45, 1.0)
        crayon.graphics.drawText("(nothing dropped yet)", 10, y, 0.9)
        y = y + 13
    end
end

local function draw_log()
    local x = 10
    local y = 210
    crayon.graphics.setColor(0.6, 0.6, 0.68, 1.0)
    crayon.graphics.drawText("EVENT LOG", x, y, 0.7)
    y = y + 14

    local now = crayon.time.getTime()
    for i, entry in ipairs(log) do
        -- Fade older entries slightly
        local age = now - entry.t
        local a = math.max(0.25, 1.0 - age * 0.15)
        crayon.graphics.setColor(entry.r, entry.g, entry.b, a)
        crayon.graphics.drawText(
            string.format("[%-8s] %s", entry.kind, entry.message),
            x, y, 0.75)
        y = y + 11
        if y > VH - 20 then break end
    end
end

local function draw_extension_counts()
    -- Small right-aligned histogram of file extensions seen.
    local keys = {}
    for k in pairs(stats.file_counts_by_ext) do
        keys[#keys + 1] = k
    end
    if #keys == 0 then return end
    table.sort(keys)

    local x = VW - 130
    local y = 210
    crayon.graphics.setColor(0.6, 0.6, 0.68, 1.0)
    crayon.graphics.drawText("EXTENSIONS", x, y, 0.7)
    y = y + 14

    for _, ext in ipairs(keys) do
        crayon.graphics.setColor(0.9, 0.9, 0.95, 0.9)
        crayon.graphics.drawText(
            string.format("%-8s %d", ext, stats.file_counts_by_ext[ext]),
            x, y, 0.75)
        y = y + 11
    end
end

function crayon.draw()
    draw_background()
    draw_header()
    draw_drop_zone()
    draw_last_payload()
    draw_log()
    draw_extension_counts()
end