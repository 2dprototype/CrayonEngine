-- crayon.tilemap: tile layers, tile properties/animations/collision helpers and Tiled (JSON) import.
--
--   local map = crayon.tilemap.loadTiled("assets/level1.tmj")      -- Tiled "JSON map" (CSV/array layers)
--   -- or build by hand:
--   local map = crayon.tilemap.new(40, 30, 16, 16, sheet)          -- cells, tile size, sprite sheet
--   local ground = map:addLayer("ground")
--   map:setTile("ground", 3, 5, 12)                                -- cell (3,5) = frame/gid 12
--   map:setTileProperty(12, "solid", true)
--
--   map:update(dt)                                                 -- tile animations
--   map:draw({ view = { x = camX, y = camY, w = 320, h = 240 } })  -- only visible cells are drawn
--   if map:isSolidAt(px, py) then ... end
--   for _, r in ipairs(map:solidRects(x, y, w, h)) do ... end      -- for AABB collision
--
-- Cells are 0-based (like pixel coordinates). Tile id 0 = empty; ids start at 1 (Tiled "gid").
local G = crayon.graphics
local tilemap = {}
local Map = {}
Map.__index = Map

local FLIP_H, FLIP_V, FLIP_D = 2147483648, 1073741824, 536870912
local FLAG_MASK_START = FLIP_D

-- ---------------------------------------------------------------------------------------------
-- Tiled flip flags -> (rotation quarter-turns, uv flipX). Built by search over the 8 dihedral
-- transforms so every H/V/D combination is covered exactly (no hand-derived special cases).
local function mat_mul(a, b)
    return { a[1] * b[1] + a[2] * b[3], a[1] * b[2] + a[2] * b[4],
             a[3] * b[1] + a[4] * b[3], a[3] * b[2] + a[4] * b[4] }
end
local function mat_eq(a, b) for i = 1, 4 do if a[i] ~= b[i] then return false end end return true end
local I2, Fx = { 1, 0, 0, 1 }, { -1, 0, 0, 1 }
local R90 = { 0, -1, 1, 0 }   -- clockwise quarter turn on screen (y down)
local FLIP_LOOKUP = {}
do
    local rots = { I2, R90, mat_mul(R90, R90), mat_mul(R90, mat_mul(R90, R90)) }
    for flags = 0, 7 do
        local h, v, d = flags >= 4, flags % 4 >= 2, flags % 2 >= 1
        local m = I2
        if d then m = mat_mul({ 0, 1, 1, 0 }, m) end
        if h then m = mat_mul(Fx, m) end
        if v then m = mat_mul({ 1, 0, 0, -1 }, m) end
        local found
        for r = 0, 3 do
            for f = 0, 1 do
                if not found and mat_eq(mat_mul(rots[r + 1], f == 1 and Fx or I2), m) then found = { r, f == 1 } end
            end
        end
        FLIP_LOOKUP[flags] = found
    end
end
tilemap._flip_lookup = FLIP_LOOKUP   -- exposed for tests

local function split_gid(gid)
    local flags = 0
    if gid >= FLIP_H then gid = gid - FLIP_H; flags = flags + 4 end
    if gid >= FLIP_V then gid = gid - FLIP_V; flags = flags + 2 end
    if gid >= FLIP_D then gid = gid - FLIP_D; flags = flags + 1 end
    return gid, flags
end

-- ---------------------------------------------------------------------------------------------
function tilemap.new(width, height, tile_w, tile_h, sheet)
    local map = setmetatable({ width = width, height = height, tile_w = tile_w, tile_h = tile_h,
                               layers = {}, layer_by_name = {}, tilesets = {}, props = {}, anims = {},
                               objects = {}, properties = {}, time = 0 }, Map)
    if sheet then map:addTileset(sheet, 1) end
    return map
end

-- A tileset maps gids [firstgid, firstgid + sheet.count) to sheet frames.
function Map:addTileset(sheet, firstgid)
    self.tilesets[#self.tilesets + 1] = { firstgid = firstgid or 1, sheet = sheet, count = sheet.count }
    table.sort(self.tilesets, function(a, b) return a.firstgid < b.firstgid end)
end

local function find_tileset(self, gid)
    local ts = self.tilesets
    for i = #ts, 1, -1 do
        if gid >= ts[i].firstgid then return ts[i] end
    end
end

-- opts: data (flat row-major array of gids), visible, opacity, parallaxx, parallaxy, offsetx, offsety, collision
function Map:addLayer(name, opts)
    opts = opts or {}
    local layer = { name = name, data = opts.data, visible = opts.visible ~= false, opacity = opts.opacity or 1,
                    parallaxx = opts.parallaxx or 1, parallaxy = opts.parallaxy or 1,
                    offsetx = opts.offsetx or 0, offsety = opts.offsety or 0, collision = opts.collision or false }
    if not layer.data then
        layer.data = {}
        for i = 1, self.width * self.height do layer.data[i] = 0 end
    end
    self.layers[#self.layers + 1] = layer
    self.layer_by_name[name] = layer
    return layer
end

function Map:getLayer(l)
    if type(l) == "table" then return l end
    local layer = type(l) == "number" and self.layers[l] or self.layer_by_name[l]
    if not layer then error("tilemap has no layer '" .. tostring(l) .. "'", 3) end
    return layer
end

local function cell_index(self, cx, cy)
    if cx < 0 or cy < 0 or cx >= self.width or cy >= self.height then return nil end
    return cy * self.width + cx + 1
end

function Map:setTile(layer, cx, cy, gid)
    local idx = cell_index(self, cx, cy)
    if idx then self:getLayer(layer).data[idx] = gid end
end

-- returns gid (flip bits removed), flags (0..7: H=4, V=2, D=1)
function Map:getTile(layer, cx, cy)
    local idx = cell_index(self, cx, cy)
    if not idx then return 0, 0 end
    local raw = self:getLayer(layer).data[idx] or 0
    if raw >= FLAG_MASK_START then return split_gid(raw) end
    return raw, 0
end

function Map:worldToCell(wx, wy) return math.floor(wx / self.tile_w), math.floor(wy / self.tile_h) end
function Map:cellToWorld(cx, cy) return cx * self.tile_w, cy * self.tile_h end

-- ---- tile properties / animation -------------------------------------------------------------
function Map:setTileProperty(gid, key, value)
    local p = self.props[gid]
    if not p then p = {}; self.props[gid] = p end
    p[key] = value
end
function Map:getTileProperty(gid, key)
    local p = self.props[gid]
    return p and p[key]
end
-- frames = { { gid = 5, duration = 0.2 }, { gid = 6, duration = 0.2 } }   (seconds)
function Map:setTileAnimation(gid, frames)
    local total = 0
    for _, f in ipairs(frames) do total = total + f.duration end
    self.anims[gid] = { frames = frames, total = total }
end
function Map:update(dt) self.time = self.time + dt end

local function resolve_animated(self, gid)
    local a = self.anims[gid]
    if not a or a.total <= 0 then return gid end
    local t = self.time % a.total
    for _, f in ipairs(a.frames) do
        if t < f.duration then return f.gid end
        t = t - f.duration
    end
    return gid
end

-- ---- collision helpers ---------------------------------------------------------------------
local function tile_solid(self, layer, gid)
    if gid == 0 then return false end
    if layer.collision then return true end
    return self.props[gid] ~= nil and self.props[gid].solid == true
end

-- layerName: restrict to one layer; default = every layer.
function Map:isSolidAt(wx, wy, layerName)
    local cx, cy = self:worldToCell(wx, wy)
    for _, layer in ipairs(self.layers) do
        if not layerName or layer.name == layerName then
            local gid = self:getTile(layer, cx, cy)
            if tile_solid(self, layer, gid) then return true end
        end
    end
    return false
end

-- Rects ({x,y,w,h} in world units) of every solid tile overlapping the box.
function Map:solidRects(x, y, w, h, layerName)
    local out = {}
    local cx0, cy0 = self:worldToCell(x, y)
    local cx1, cy1 = self:worldToCell(x + w - 1e-6, y + h - 1e-6)
    cx0, cy0 = math.max(cx0, 0), math.max(cy0, 0)
    cx1, cy1 = math.min(cx1, self.width - 1), math.min(cy1, self.height - 1)
    for cy = cy0, cy1 do
        for cx = cx0, cx1 do
            for _, layer in ipairs(self.layers) do
                if not layerName or layer.name == layerName then
                    local gid = self:getTile(layer, cx, cy)
                    if tile_solid(self, layer, gid) then
                        out[#out + 1] = { x = cx * self.tile_w, y = cy * self.tile_h, w = self.tile_w, h = self.tile_h }
                        break
                    end
                end
            end
        end
    end
    return out
end

function Map:getObjects(layerName) return self.objects[layerName] or {} end

-- ---- drawing ---------------------------------------------------------------------------------
-- opts: x, y (map origin, default 0,0), scale (default 1),
--       view = { x, y, w, h } visible world rect (default: whole map),
--       layers = { name = true, ... } (default: all visible layers)
-- When a layer has opacity < 1 the current colour is set to (1,1,1,opacity) while drawing it and
-- reset to opaque white afterwards.
function Map:draw(opts)
    opts = opts or {}
    local ox, oy = opts.x or 0, opts.y or 0
    local scale = opts.scale or 1
    local view = opts.view
    local tw, th = self.tile_w * scale, self.tile_h * scale
    local count = 0

    for _, layer in ipairs(self.layers) do
        if layer.visible and (not opts.layers or opts.layers[layer.name]) then
            local lox = ox + layer.offsetx * scale
            local loy = oy + layer.offsety * scale
            if view then   -- parallax: layers with factor < 1 scroll slower than the camera
                lox = lox + view.x * (1 - layer.parallaxx)
                loy = loy + view.y * (1 - layer.parallaxy)
            end
            local x0, y0, x1, y1 = 0, 0, self.width - 1, self.height - 1
            if view then
                x0 = math.max(0, math.floor((view.x - lox) / tw))
                y0 = math.max(0, math.floor((view.y - loy) / th))
                x1 = math.min(self.width - 1, math.floor((view.x + view.w - lox) / tw))
                y1 = math.min(self.height - 1, math.floor((view.y + view.h - loy) / th))
            end
            if layer.opacity < 1 then G.setColor(1, 1, 1, layer.opacity) end
            local data, width = layer.data, self.width
            for cy = y0, y1 do
                local row = cy * width + 1
                for cx = x0, x1 do
                    local raw = data[row + cx]
                    if raw and raw ~= 0 then
                        local gid, flags = raw, 0
                        if raw >= FLAG_MASK_START then gid, flags = split_gid(raw) end
                        if next(self.anims) then gid = resolve_animated(self, gid) end
                        local ts = find_tileset(self, gid)
                        local q = ts and ts.sheet.frames[gid - ts.firstgid + 1]
                        if q then
                            local w, h = q.w * scale, q.h * scale
                            local px = lox + cx * tw
                            local py = loy + (cy + 1) * th - h      -- tiles sit on the bottom edge of their cell (Tiled)
                            if flags == 0 then
                                G.drawSpritePart(ts.sheet.texture, px, py, q.u0, q.v0, q.u1, q.v1, w, h)
                            else
                                local f = FLIP_LOOKUP[flags]
                                local u0, u1 = q.u0, q.u1
                                if f[2] then u0, u1 = u1, u0 end
                                G.drawSpritePart(ts.sheet.texture, px + w * 0.5, py + h * 0.5, u0, q.v0, u1, q.v1, w, h,
                                                 f[1] * math.pi * 0.5, w * 0.5, h * 0.5)
                            end
                            count = count + 1
                        end
                    end
                end
            end
            if layer.opacity < 1 then G.setColor(1, 1, 1, 1) end
        end
    end
    return count
end

-- ---------------------------------------------------------------------------------------------
-- Tiled JSON import
local function dir_of(path)
    return path:match("^(.*)[/\\][^/\\]*$") or ""
end

local function join_path(dir, rel)
    if rel:match("^[/\\]") or rel:match("^%a:[/\\]") or dir == "" then return rel end
    local parts = {}
    for seg in (dir .. "/" .. rel):gmatch("[^/\\]+") do
        if seg == ".." and #parts > 0 and parts[#parts] ~= ".." then parts[#parts] = nil
        elseif seg ~= "." then parts[#parts + 1] = seg end
    end
    return table.concat(parts, "/")
end

local function read_json(path)
    local text, err = crayon.fs.readText(path)
    if not text then error("tilemap: cannot read '" .. path .. "'" .. (err and (": " .. tostring(err)) or ""), 3) end
    return crayon.json.decode(text)
end

local function props_to_table(list)
    local t = {}
    if type(list) == "table" then
        for _, p in ipairs(list) do t[p.name] = p.value end
    end
    return t
end

local function load_layers(map, layers, prefix)
    for _, l in ipairs(layers) do
        local name = prefix .. (l.name or "layer")
        local props = props_to_table(l.properties)
        if l.type == "tilelayer" then
            if l.chunks or map.infinite then
                error("tilemap: infinite Tiled maps are not supported (Map properties > uncheck 'Infinite')", 3)
            end
            if type(l.data) ~= "table" then
                error("tilemap: layer '" .. name .. "' uses '" .. tostring(l.encoding or "compressed") ..
                      "' data. In Tiled use Map properties > Tile Layer Format = CSV.", 3)
            end
            map:addLayer(name, { data = l.data, visible = l.visible ~= false, opacity = l.opacity or 1,
                                 parallaxx = l.parallaxx, parallaxy = l.parallaxy,
                                 offsetx = l.offsetx, offsety = l.offsety,
                                 collision = props.collision == true or props.solid == true })
        elseif l.type == "objectgroup" then
            local objs = {}
            for i, o in ipairs(l.objects or {}) do
                objs[i] = { id = o.id, name = o.name, type = o.type or o.class, x = o.x, y = o.y, width = o.width,
                            height = o.height, rotation = o.rotation, gid = o.gid, visible = o.visible,
                            point = o.point, polygon = o.polygon, polyline = o.polyline,
                            properties = props_to_table(o.properties) }
            end
            map.objects[name] = objs
        elseif l.type == "group" then
            load_layers(map, l.layers or {}, name .. "/")
        end
    end
end

function tilemap.loadTiled(path, opts)
    opts = opts or {}
    local data = read_json(path)
    if data.infinite then error("tilemap: infinite Tiled maps are not supported", 2) end
    local dir = dir_of(path)
    local map = tilemap.new(data.width, data.height, data.tilewidth, data.tileheight)
    map.properties = props_to_table(data.properties)
    map.orientation = data.orientation or "orthogonal"
    if map.orientation ~= "orthogonal" then
        error("tilemap: only orthogonal Tiled maps are supported (got '" .. tostring(map.orientation) .. "')", 2)
    end

    for _, ref in ipairs(data.tilesets or {}) do
        local ts, ts_dir = ref, dir
        if ref.source then
            local spath = join_path(dir, ref.source)
            ts = read_json(spath)
            ts_dir = dir_of(spath)
        end
        if not ts.image then
            error("tilemap: tileset '" .. tostring(ts.name) .. "' is an image collection; use a single-image tileset", 2)
        end
        local texture = (opts.textures and opts.textures[ts.name]) or G.loadTexture(join_path(ts_dir, ts.image), opts.textureOptions)
        local sheet = crayon.sprite.newSheet(texture, ts.tilewidth, ts.tileheight,
            { margin = ts.margin, spacing = ts.spacing, columns = ts.columns, count = ts.tilecount })
        map:addTileset(sheet, ref.firstgid)
        for _, t in ipairs(ts.tiles or {}) do
            local gid = ref.firstgid + t.id
            for k, v in pairs(props_to_table(t.properties)) do map:setTileProperty(gid, k, v) end
            if t.type == "solid" or t.class == "solid" then map:setTileProperty(gid, "solid", true) end
            if t.animation then
                local frames = {}
                for i, f in ipairs(t.animation) do frames[i] = { gid = ref.firstgid + f.tileid, duration = f.duration / 1000 } end
                map:setTileAnimation(gid, frames)
            end
        end
    end
    load_layers(map, data.layers or {}, "")
    return map
end

crayon.tilemap = tilemap
