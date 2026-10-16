dofile("test/lua/mock_crayon.lua")
local base = "src/scripting/prelude/"
for _, f in ipairs({ "10_json.lua", "20_sprite.lua", "30_tilemap.lua", "40_drawqueue.lua", "50_lights2d.lua" }) do dofile(base .. f) end
local G = crayon.graphics
local json = crayon.json

print("== json")
check(json.decode('{"a":[1,2,{"b":null}],"s":"x\\n\\u00e9\\ud83d\\ude00","t":true,"n":-1.5e2}').n == -150, "numbers incl. exponent")
local d = json.decode('{"a":[1,2,{"b":null}],"s":"x\\n\\u00e9\\ud83d\\ude00","t":true}')
check(d.a[3].b == json.null and d.t == true, "null / true")
check(d.s == "x\n\195\169\240\159\152\128", "escapes + unicode + surrogate pair")
check(not pcall(json.decode, '{"a":1,}'), "trailing comma rejected")
check(select(2, pcall(json.decode, '[1, 2')):find("line 1"), "errors report line/column")
local enc = json.encode({ list = { 1, 2.5, "q\"" }, flag = false, name = "n" })
check(enc == '{"flag":false,"list":[1,2.5,"q\\""],"name":"n"}', "compact encode is deterministic: " .. enc)
local rt = json.decode(json.encode({ x = { 1, { y = "z" } } }, "  "))
check(rt.x[2].y == "z", "pretty encode round-trips")
check(not pcall(json.encode, { f = function() end }), "functions cannot be encoded")

print("== sprite sheet")
local tex = G.loadTexture("sheet.png")   -- 128x64
local sheet = crayon.sprite.newSheet(tex, 32, 32)
check(sheet.columns == 4 and sheet.rows == 2 and sheet.count == 8, "grid 4x2 = 8 frames")
local q6 = sheet:getFrame(6)
check(approx(q6.u0, 32 / 128) and approx(q6.v0, 0.5) and approx(q6.u1, 64 / 128) and approx(q6.v1, 1.0), "frame 6 uv = second row, second column")
local padded = crayon.sprite.newSheet(G.loadTexture("p.png"), 28, 28, { margin = 2, spacing = 2 })   -- 128x64
check(padded.columns == 4 and padded.rows == 2 and approx(padded:getFrame(2).u0, (2 + 28 + 2) / 128), "margin + spacing respected")
crayon._reset()
sheet:draw(6, 100, 50, { scale = 2, rotation = 0.5, ox = 8, oy = 4 })
local c = crayon._last("drawSpritePart")
check(c.args[2] == 100 and c.args[3] == 50 and c.args[8] == 64 and c.args[9] == 64 and c.args[10] == 0.5 and c.args[11] == 16 and c.args[12] == 8,
      "draw passes position, scaled size, rotation and scaled origin")
crayon._reset()
sheet:draw(6, 0, 0, { flipX = true, ox = 8 })
c = crayon._last("drawSpritePart")
check(approx(c.args[4], q6.u1) and approx(c.args[6], q6.u0) and c.args[11] == 24, "flipX swaps u and mirrors the origin")
local atlas = crayon.sprite.newAtlas(tex, { idle = { 0, 0, 32, 32 }, jump = { 32, 0, 32, 40, 16, 40 } })
check(atlas:getFrame("jump").h == 40 and atlas:getFrame("jump").oy == 40, "atlas frames by name, with pivot")
check(not pcall(function() atlas:getFrame("nope") end), "unknown frame name is an error")
check(#sheet:range(3, 6) == 4 and sheet:range(6, 3)[1] == 6, "range ascending/descending")

print("== animation")
local fin = 0
local anim = crayon.sprite.newAnimation(sheet, { 1, 2, 3, 4 }, 10, { mode = "once", onFinish = function() fin = fin + 1 end })
check(anim:getFrame() == 1, "starts on first frame")
anim:update(0.11); check(anim:getFrame() == 2, "advances after 1/fps")
anim:update(0.25); check(anim:getFrame() == 4, "skips frames on large dt")
anim:update(1.0); check(anim:isFinished() and anim:getFrame() == 4 and fin == 1, "'once' stops on last frame and fires onFinish once")
anim:update(1.0); check(fin == 1, "finished animation stays finished")
local loop = crayon.sprite.newAnimation(sheet, { 1, 2, 3 }, 10)
loop:update(0.35); check(loop:getFrame() == 1 and loop.loops == 1, "'loop' wraps (frame " .. loop:getFrame() .. ")")
local pp = crayon.sprite.newAnimation(sheet, { 1, 2, 3, 4 }, 10, { mode = "pingpong" })
local seen = {}
for i = 1, 8 do seen[#seen + 1] = pp:getFrame(); pp:update(0.1) end
check(table.concat(seen, ",") == "1,2,3,4,3,2,1,2", "pingpong order " .. table.concat(seen, ","))
local dur = crayon.sprite.newAnimation(sheet, { 1, 2 }, 10, { durations = { 0.5, 0.1 } })
dur:update(0.55); check(dur:getFrame() == 2, "per-frame durations")
dur:pause(); dur:update(5); check(dur:getFrame() == 2, "pause freezes time")
local player = crayon.sprite.newAnimator(sheet, { idle = { frames = { 1, 2 }, fps = 2 }, run = { frames = { 3, 4 }, fps = 10 } })
player:play("idle"); player:update(0.6)
local before = player.current.index
player:play("idle")
check(player.current.index == before, "play() of the running animation does not restart it")
player:play("run"); check(player:getCurrent() == "run" and player.current.index == 1, "switching restarts")
crayon._reset(); player:draw(10, 10)
check(crayon._count("drawSpritePart") == 1, "animator draws current frame")

print("== tilemap")
local tsheet = crayon.sprite.newSheet(tex, 32, 32)   -- 8 frames
local map = crayon.tilemap.new(10, 6, 16, 16, tsheet)
map:addLayer("ground")
map:setTile("ground", 2, 3, 5)
map:setTile("ground", 9, 5, 7)
map:setTile("ground", 99, 99, 1)   -- out of range: ignored
check(map:getTile("ground", 2, 3) == 5 and map:getTile("ground", 99, 99) == 0, "set/get tile, out of range = 0")
local x, y = map:worldToCell(40, 52); check(x == 2 and y == 3, "worldToCell")
map:setTileProperty(5, "solid", true)
check(map:isSolidAt(2 * 16 + 4, 3 * 16 + 4) and not map:isSolidAt(0, 0) and not map:isSolidAt(9 * 16 + 1, 5 * 16 + 1), "isSolidAt uses the 'solid' property")
local rects = map:solidRects(20, 40, 30, 20)
check(#rects == 1 and rects[1].x == 32 and rects[1].y == 48, "solidRects returns overlapping solid tiles")
crayon._reset()
local n = map:draw()
check(n == 2 and crayon._count("drawSpritePart") == 2, "draw: two tiles")
crayon._reset()
n = map:draw({ view = { x = 0, y = 0, w = 64, h = 64 } })
check(n == 1, "draw culls tiles outside the view (drew " .. n .. ")")
local call = crayon._last("drawSpritePart")
check(call.args[2] == 32 and call.args[3] == 48 + 16 - 32, "tile positioned on its cell's bottom edge (tileset tile taller than the cell)")
-- animation
map:setTileAnimation(5, { { gid = 5, duration = 0.2 }, { gid = 6, duration = 0.2 } })
crayon._reset(); map:update(0.25); map:draw({ view = { x = 0, y = 0, w = 64, h = 64 } })
local q = tsheet:getFrame(6)
check(approx(crayon._last("drawSpritePart").args[4], q.u0), "animated tile shows frame 2 after 0.25s")
-- opacity + parallax
map.layers[1].opacity = 0.5; crayon._reset(); map:draw()
check(crayon._calls[1].name == "setColor" and crayon._calls[1].args[4] == 0.5, "layer opacity sets colour alpha")
map.layers[1].opacity = 1; map.layers[1].parallaxx = 0.5; crayon._reset()
map:draw({ view = { x = 100, y = 0, w = 320, h = 240 } })
local first_tile
for _, cc in ipairs(crayon._calls) do if cc.name == "drawSpritePart" then first_tile = cc; break end end
-- cell (2,3) would land at x=82, left of the view (x=100), so it is culled; cell (9,5) lands at 9*16 + 50
check(first_tile.args[2] == 9 * 16 + 100 * 0.5 and crayon._count("drawSpritePart") == 1, "parallax 0.5 shifts the layer by half the camera offset and culls against the shifted layer")

print("== tiled flip flags")
local L = crayon.tilemap._flip_lookup
local seen_t = {}
for flags = 0, 7 do seen_t[#seen_t + 1] = L[flags][1] .. (L[flags][2] and "f" or "n") end
local uniq = {}
for _, s in ipairs(seen_t) do uniq[s] = true end
local n_uniq = 0; for _ in pairs(uniq) do n_uniq = n_uniq + 1 end
check(n_uniq == 8, "all 8 dihedral transforms are distinct: " .. table.concat(seen_t, " "))
check(L[0][1] == 0 and L[0][2] == false, "no flags = identity")
check(L[4][1] == 0 and L[4][2] == true, "H flag = horizontal flip")
check(L[2][1] == 2 and L[2][2] == true, "V flag = rot180 + flipX")
check(L[6][1] == 2 and L[6][2] == false, "H+V = rotate 180")
local tm = crayon.tilemap.new(2, 1, 16, 16, tsheet); tm:addLayer("g")
tm.layers[1].data = { 2147483648 + 3, 1073741824 + 2 }   -- flipH gid3, flipV gid2
local g1, f1 = tm:getTile("g", 0, 0); check(g1 == 3 and f1 == 4, "getTile strips H flag")
local g2, f2 = tm:getTile("g", 1, 0); check(g2 == 2 and f2 == 2, "getTile strips V flag")
crayon._reset(); tm:draw()
check(crayon._count("drawSpritePart") == 2 and crayon._calls[1].args.n == nil, "flipped tiles drawn")
check(crayon._calls[1].n == 12, "flipped tile uses rotation+origin arguments")

print("== tiled import")
crayon._files["maps/level.tmj"] = json.encode({
    width = 4, height = 2, tilewidth = 16, tileheight = 16, orientation = "orthogonal", infinite = false,
    tilesets = { { firstgid = 1, source = "../tilesets/terrain.tsj" } },
    layers = {
        { type = "tilelayer", name = "ground", data = { 0, 1, 2, 3, 4, 0, 0, 5 }, width = 4, height = 2, visible = true, opacity = 1, properties = { { name = "collision", type = "bool", value = true } } },
        { type = "group", name = "fx", layers = { { type = "tilelayer", name = "clouds", data = { 0, 0, 0, 0, 0, 0, 0, 0 }, parallaxx = 0.5 } } },
        { type = "objectgroup", name = "spawns", objects = { { id = 1, name = "player", type = "spawn", x = 8, y = 24, width = 0, height = 0, properties = { { name = "hp", value = 3 } } } } },
    } })
crayon._files["tilesets/terrain.tsj"] = json.encode({
    name = "terrain", image = "terrain.png", imagewidth = 128, imageheight = 64, tilewidth = 32, tileheight = 32,
    columns = 4, tilecount = 8, margin = 0, spacing = 0,
    tiles = { { id = 1, properties = { { name = "solid", type = "bool", value = true } } },
              { id = 2, animation = { { tileid = 2, duration = 100 }, { tileid = 3, duration = 100 } } } } })
local lm = crayon.tilemap.loadTiled("maps/level.tmj")
check(lm.width == 4 and lm.height == 2 and #lm.layers == 2 and lm.layer_by_name["fx/clouds"] ~= nil, "layers loaded, groups flattened ('fx/clouds')")
check(lm.tilesets[1].sheet.texture._path == "tilesets/terrain.png", "external tileset image path resolved relative to the .tsj ('" .. tostring(lm.tilesets[1].sheet.texture._path) .. "')")
check(lm:getTileProperty(2, "solid") == true, "tileset tile properties -> gid properties")
check(lm.anims[3] and lm.anims[3].total > 0.199 and lm.anims[3].total < 0.201, "tileset animation durations converted to seconds")
check(lm.layer_by_name.ground.collision == true and lm:isSolidAt(1 * 16 + 1, 0 * 16 + 1), "layer 'collision' property makes every tile solid")
check(lm:getObjects("spawns")[1].name == "player" and lm:getObjects("spawns")[1].properties.hp == 3, "objects + properties")
check(lm.layer_by_name["fx/clouds"].parallaxx == 0.5, "parallax kept")
check(not pcall(crayon.tilemap.loadTiled, "missing.tmj"), "missing file is an error")
crayon._files["inf.tmj"] = json.encode({ width = 1, height = 1, tilewidth = 8, tileheight = 8, infinite = true, tilesets = {}, layers = {} })
check(select(2, pcall(crayon.tilemap.loadTiled, "inf.tmj")):find("infinite"), "infinite maps give a clear error")
crayon._files["b64.tmj"] = json.encode({ width = 1, height = 1, tilewidth = 8, tileheight = 8, tilesets = {}, layers = { { type = "tilelayer", name = "a", encoding = "base64", data = "AAAA" } } })
check(select(2, pcall(crayon.tilemap.loadTiled, "b64.tmj")):find("CSV"), "base64 layers explain how to export CSV")

print("== draw queue")
local order = {}
local q = crayon.drawqueue.new()
q:add(5, function(a, b) order[#order + 1] = "e" .. a .. tostring(b) end, 1, nil)
q:add(1, function() order[#order + 1] = "a" end)
q:add(3, function() order[#order + 1] = "c1" end)
q:add(3, function() order[#order + 1] = "c2" end)
q:add(-2, function() order[#order + 1] = "first" end)
check(q:count() == 5, "count")
q:flush()
check(table.concat(order, ",") == "first,a,c1,c2,e1nil", "sorted by z, stable for ties, args (incl. nil) forwarded: " .. table.concat(order, ","))
check(q:count() == 0, "flush clears")

print("== lights2d")
local lights = crayon.lights2d.new({ resolution = 0.5 })
check(lights.img._w == 160 and lights.img._h == 120, "light map at half resolution")
check(lights.tex._opts.filter == "linear" and lights.tex._opts.wrap == "clamp", "light texture is smooth + clamped")
lights:setAmbient(0.2, 0.2, 0.3); lights:setView(50, 20, 2)
lights:add({ x = 10, y = 10, radius = 30 }); lights:add({ x = 0, y = 0, radius = 5, spot = true })
check(lights:count() == 2, "add / count")
crayon._reset(); lights:draw()
local rl = crayon._last("renderLightmap")
check(rl and rl.args[4].offsetX == 50 and rl.args[4].scaleX == 1.0 and #rl.args[3] == 2, "view mapping: scale = resolution * zoom")
check(lights.tex._updated == 1, "texture updated once per draw")
check(crayon._calls[2].name == "setBlendMode" and crayon._calls[2].args[1] == "multiply" and crayon._last("setBlendMode").args[1] == "alpha", "multiply blend, then restored to alpha")
check(crayon._last("drawSprite").args[4] == 320 and crayon._last("drawSprite").args[5] == 240, "composited over the full virtual screen")
lights:destroy(); check(lights.tex == nil, "destroy releases the texture")

finish("PRELUDE LUA TESTS")
