-- ============================================================================
-- Example 25: new 2D features (works on both renderers)
--
--   crayon --game examples/25_2d_features.lua [--renderer=soft]
--
--  * crayon.sprite      sprite sheet + animation (art generated at startup with ImageData)
--  * crayon.tilemap     tile layers, tile properties, view culling, scrolling
--  * crayon.lights2d    2D point + spot lights multiplied over the scene
--  * graphics.beginMask / useMask / endMask   stencil-style masking (spotlight reveal)
--  * drawText options   outline + drop shadow
--  * crayon.drawqueue   z / y-sorted drawing
--  * texture:setFilter / setWrap, graphics.newImageData / newTexture
--  * graphics.screenshot, getCullStats (2D offscreen culling)
--
-- Keys: A/D move   L lights   F texture filter   M wrap mode   P screenshot   ESC quit
-- ============================================================================
local G = crayon.graphics

function crayon.config(t)
    t.window.title = "25 - 2D features"
    t.window.width = 320
    t.window.height = 240
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.modules.physics3D = false
    t.modules.audio = false
    t.modules.mesh3D = false
end

local S = { px = 40, py = 150, facing = 1, moving = false, lights_on = true, filter = "nearest", wrap_i = 1, shot = 0 }
local WRAPS = { "repeat", "clamp", "mirror" }

-- ---- procedural pixel art -------------------------------------------------------------------
local function rect(img, x, y, w, h, r, g, b)
    for yy = y, y + h - 1 do for xx = x, x + w - 1 do img:setPixel(xx, yy, r, g, b, 1) end end
end

local function make_character()      -- 4 walk frames, 16x16 each, 64x16 image
    local img = G.newImageData(64, 16, 0, 0, 0, 0)
    for f = 0, 3 do
        local ox = f * 16
        local bob = (f % 2 == 1) and 1 or 0
        rect(img, ox + 5, 2 + bob, 6, 5, 0.95, 0.78, 0.6)        -- head
        rect(img, ox + 5, 1 + bob, 6, 2, 0.35, 0.2, 0.1)         -- hair
        rect(img, ox + 4, 7 + bob, 8, 5, 0.2, 0.5, 0.9)          -- body
        local l = (f == 1) and -1 or (f == 3) and 1 or 0         -- legs swing
        rect(img, ox + 5 + l, 12, 2, 4, 0.15, 0.15, 0.4)
        rect(img, ox + 9 - l, 12, 2, 4, 0.15, 0.15, 0.4)
        img:setPixel(ox + 7, 4 + bob, 0, 0, 0, 1); img:setPixel(ox + 9, 4 + bob, 0, 0, 0, 1)
    end
    return img
end

local function make_tiles()           -- grass, dirt, water, stone: 64x16
    local img = G.newImageData(64, 16)
    local pal = { { 0.3, 0.65, 0.25 }, { 0.5, 0.35, 0.2 }, { 0.2, 0.4, 0.85 }, { 0.5, 0.5, 0.55 } }
    for i, c in ipairs(pal) do
        for y = 0, 15 do for x = 0, 15 do
            local n = ((x * 7 + y * 13 + i * 5) % 5) * 0.025
            img:setPixel((i - 1) * 16 + x, y, c[1] + n, c[2] + n, c[3] + n, 1)
        end end
    end
    for x = 0, 15 do img:setPixel(48 + x, 0, 0.7, 0.7, 0.75, 1); img:setPixel(48 + x, 15, 0.3, 0.3, 0.35, 1) end   -- stone edges
    return img
end

function crayon.init()
    G = crayon.graphics
    S.char_tex = G.newTexture(make_character(), { filter = "nearest" })
    S.tile_tex = G.newTexture(make_tiles(), { filter = "nearest" })

    local char_sheet = crayon.sprite.newSheet(S.char_tex, 16, 16)
    S.anim = crayon.sprite.newAnimation(char_sheet, { 1, 2, 3, 4 }, 8)
    S.char_sheet = char_sheet

    local tile_sheet = crayon.sprite.newSheet(S.tile_tex, 16, 16)
    S.map = crayon.tilemap.new(80, 15, 16, 16, tile_sheet)
    S.map:addLayer("ground")
    for x = 0, 79 do
        local top = 10 + math.floor(math.sin(x * 0.25) * 1.2 + 0.5)
        for y = top, 14 do S.map:setTile("ground", x, y, (y == top) and 1 or 2) end
    end
    for x = 30, 36 do S.map:setTile("ground", x, 12, 3); S.map:setTile("ground", x, 11, 3) end   -- a pond
    for x = 52, 56 do for y = 7, 9 do S.map:setTile("ground", x, y, 4) end end                    -- stone ruin
    S.map:setTileProperty(4, "solid", true)

    S.lights = crayon.lights2d.new({ resolution = 0.5 })
    S.lights:setAmbient(0.18, 0.18, 0.3)
    S.queue = crayon.drawqueue.new()
    print("renderer: " .. G.getRendererName() .. "  tilemap cells: " .. S.map.width * S.map.height)
end

function crayon.keydown(k, isRepeat)
    if isRepeat then return end
    if k == "l" then S.lights_on = not S.lights_on
    elseif k == "f" then
        S.filter = (S.filter == "nearest") and "linear" or "nearest"
        S.char_tex:setFilter(S.filter)
    elseif k == "m" then
        S.wrap_i = S.wrap_i % #WRAPS + 1
        S.char_tex:setWrap(WRAPS[S.wrap_i])
    elseif k == "p" then
        S.shot = S.shot + 1
        G.screenshot(string.format("screenshot_%02d.png", S.shot))
    elseif k == "escape" then crayon.window.quit() end
end

function crayon.update(dt)
    S.moving = false
    if crayon.key.isDown("a") then S.px = S.px - 60 * dt; S.facing = -1; S.moving = true end
    if crayon.key.isDown("d") then S.px = S.px + 60 * dt; S.facing = 1; S.moving = true end
    S.px = math.max(8, math.min(S.px, S.map.width * 16 - 8))
    if S.moving then S.anim:update(dt) else S.anim:seek(1) end
    S.map:update(dt)
    S.t = (S.t or 0) + dt
end

function crayon.draw()
    local t = S.t or 0
    local cam_x = math.max(0, math.min(S.px - 160, S.map.width * 16 - 320))
    G.clear(0.45, 0.7, 0.95, 1)
    G.setBlendMode("alpha")

    -- tilemap: only the cells inside the view are submitted
    G.setColor(1, 1, 1, 1)
    local drawn = S.map:draw({ x = -cam_x, y = 0, view = { x = 0, y = 0, w = 320, h = 240 } })

    -- y-sorted entities through the draw queue
    local q = S.queue
    local function crate(x, y) q:add(y, function() G.setColor(0.6, 0.4, 0.2, 1); G.drawRect("fill", x - cam_x - 6, y - 12, 12, 12) end) end
    crate(120, 168); crate(150, 160); crate(210, 170)
    q:add(S.py, function()
        G.setColor(1, 1, 1, 1)
        S.anim:draw(S.px - cam_x, S.py, { flipX = S.facing < 0, ox = 8, oy = 16, scale = 1 })
    end)
    q:flush()

    -- 2D lights multiplied over the scene (positions in screen space here)
    if S.lights_on then
        local L = S.lights
        L:clear()
        L:add({ x = S.px - cam_x, y = S.py - 8, radius = 70, color = { 1.0, 0.85, 0.55 }, intensity = 1.2 })
        L:add({ x = 160 + math.sin(t) * 90, y = 60, radius = 90, spot = true, direction = math.pi / 2, cone = 0.5,
                color = { 0.5, 0.7, 1.0 }, intensity = 1.0 })
        L:draw()
    end

    -- mask: a circular "window" onto a different background that follows a sine path
    local mx, my = 250 + math.sin(t * 1.3) * 20, 40 + math.cos(t * 1.7) * 10
    G.beginMask()
    G.drawCircle("fill", mx, my, 26)
    G.useMask()
    G.setColor(0.15, 0.1, 0.3, 1); G.drawRect("fill", 0, 0, 320, 240)
    for i = 0, 8 do G.setColor(1, 0.9, 0.4, 1); G.drawCircle("fill", mx - 24 + (i * 13 + t * 30) % 52, my - 14 + (i * 7) % 28, 2) end
    G.endMask()
    G.setColor(1, 1, 1, 1); G.drawCircle("line", mx, my, 26)

    -- text effects + HUD
    G.drawText("2D FEATURES", 8, 6, { outline = { color = { 0, 0, 0, 1 }, thickness = 1 }, shadow = { dx = 1, dy = 2, color = { 0, 0, 0, 0.5 } } })
    G.setColor(1, 1, 0.6, 1)
    G.drawText(string.format("tiles drawn: %d   filter: %s   wrap: %s   %d fps", drawn, S.filter, WRAPS[S.wrap_i], math.floor(crayon.window.getFps())),
               8, 224, { outline = { color = { 0, 0, 0, 1 }, thickness = 1 } })
    -- wrap demo strip: uv 0..3 across the strip shows repeat / clamp / mirror of the character sheet
    G.setColor(1, 1, 1, 1)
    G.drawSpritePart(S.char_tex, 8, 24, 0, 0, 0.25 * 3, 1, 96, 16)
    local culled3d, culled2d = G.getCullStats()
    G.drawText("A/D move  L lights  F filter  M wrap  P screenshot  (2D culled quads: " .. culled2d .. ")", 8, 214)
end
