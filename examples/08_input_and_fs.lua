-- ============================================================================
-- Example 08: Unified Input, Lifecycle Callbacks, & File System (FS)
-- Demonstrates: crayon.input, window callbacks (resized, focusChanged, quit),
--               and persistent file storage with crayon.fs.
-- ============================================================================

local lastKey = "None"
local mouseState = { x = 0, y = 0, down = false }
local savePath = "save_data.txt"
local highscore = 0
local focusStatus = "Focused"
local windowSizeInfo = "640 x 480"
local logMessages = {}

function addLog(msg)
    table.insert(logMessages, 1, msg)
    if #logMessages > 6 then table.remove(logMessages) end
end

function crayon.config(config)
    config.window.width = 640
    config.window.height = 480
    config.window.virtualWidth = 640
    config.window.virtualHeight = 480
end

function crayon.init()
    crayon.window.setTitle("08 - Input & File System")
    -- crayon.window.setResolution(640, 480)

    -- Attempt reading saved high score using crayon.fs
    if crayon.fs.exists(savePath) then
        local content = crayon.fs.readText(savePath)
        highscore = tonumber(content) or 0
        addLog("Loaded persistent save: " .. highscore)
    else
        addLog("No existing save file. Creating new.")
        crayon.fs.writeText(savePath, tostring(highscore))
    end
end

-- Window Lifecycle Callbacks
function crayon.windowResized(w, h)
    windowSizeInfo = w .. " x " .. h
    addLog("Window resized to: " .. windowSizeInfo)
end

function crayon.focusChanged(focused)
    focusStatus = focused and "Focused" or "Unfocused"
    addLog("Window focus changed: " .. focusStatus)
end

function crayon.quit()
    addLog("Engine quit requested! Saving state...")
    crayon.fs.writeText(savePath, tostring(highscore))
    return false -- Allow standard engine shutdown
end

function crayon.update(dt)
    -- Unified input polling
    mouseState.x, mouseState.y = crayon.input.getMousePosition()
    mouseState.down = crayon.input.isMouseDown(1)

    -- Detect any key pressed
    local pressed = crayon.key.getPressedKeys()
    if #pressed > 0 then
        lastKey = table.concat(pressed, ", ")
    end

    -- Increase score with [S] key and persist to file
    if crayon.key.isPressed("s") then
        highscore = highscore + 100
        crayon.fs.writeText(savePath, tostring(highscore))
        addLog("Saved high score: " .. highscore)
    end

    -- Reset save with [R] key
    if crayon.key.isPressed("r") then
        highscore = 0
        crayon.fs.writeText(savePath, "0")
        addLog("Reset high score to 0")
    end
end

function crayon.draw()
    crayon.graphics.clear(0.09, 0.1, 0.15, 1.0)

    crayon.graphics.setColor(0.3, 0.85, 0.9, 1.0)
    crayon.graphics.drawText("Unified Input & File System", 24, 24, { scale = 2.0 })

    -- Left Card: Input State
    crayon.graphics.setColor(0.14, 0.17, 0.25, 1.0)
    crayon.graphics.drawRect("fill", 24, 80, 280, 200, 8, 8)
    crayon.graphics.setColor(0.3, 0.4, 0.6, 1.0)
    crayon.graphics.drawRect("line", 24, 80, 280, 200, 8, 8)

    crayon.graphics.setColor(1.0, 0.85, 0.2, 1.0)
    crayon.graphics.drawText("Input State", 40, 95, { scale = 1.2 })
    crayon.graphics.setColor(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.drawText("Mouse Pos:   " .. math.floor(mouseState.x) .. ", " .. math.floor(mouseState.y), 40, 125)
    crayon.graphics.drawText("Mouse Down:  " .. tostring(mouseState.down), 40, 150)
    crayon.graphics.drawText("Last Key:    " .. lastKey, 40, 175)
    crayon.graphics.drawText("Focus:       " .. focusStatus, 40, 200)
    crayon.graphics.drawText("Resolution:  " .. windowSizeInfo, 40, 225)

    -- Right Card: File System & Save Data
    crayon.graphics.setColor(0.14, 0.17, 0.25, 1.0)
    crayon.graphics.drawRect("fill", 336, 80, 280, 200, 8, 8)
    crayon.graphics.setColor(0.3, 0.4, 0.6, 1.0)
    crayon.graphics.drawRect("line", 336, 80, 280, 200, 8, 8)

    crayon.graphics.setColor(0.4, 0.9, 0.5, 1.0)
    crayon.graphics.drawText("Persistent FS State", 352, 95, { scale = 1.2 })
    crayon.graphics.setColor(0.9, 0.9, 0.9, 1.0)
    crayon.graphics.drawText("Save File:   " .. savePath, 352, 125)
    crayon.graphics.drawText("High Score:  " .. highscore, 352, 150)
    crayon.graphics.setColor(0.7, 0.75, 0.85, 1.0)
    crayon.graphics.drawText("[S] Add +100 & Save", 352, 185)
    crayon.graphics.drawText("[R] Reset High Score", 352, 210)

    -- Bottom Card: Event Log
    crayon.graphics.setColor(0.12, 0.14, 0.2, 1.0)
    crayon.graphics.drawRect("fill", 24, 300, 592, 150, 8, 8)
    crayon.graphics.setColor(0.25, 0.35, 0.5, 1.0)
    crayon.graphics.drawRect("line", 24, 300, 592, 150, 8, 8)

    crayon.graphics.setColor(0.9, 0.7, 0.3, 1.0)
    crayon.graphics.drawText("Event & Activity Log", 40, 312, { scale = 1.1 })
    crayon.graphics.setColor(0.75, 0.8, 0.9, 1.0)
    for i, log in ipairs(logMessages) do
        crayon.graphics.drawText("> " .. log, 40, 328 + i * 18)
    end
end
