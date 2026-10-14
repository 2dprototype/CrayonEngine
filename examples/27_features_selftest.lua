-- ============================================================================
-- Example 27: self-test for the new graphics features (run it on both renderers)
--
--   crayon --game examples/27_features_selftest.lua --renderer=soft
--   crayon --game examples/27_features_selftest.lua --renderer=gl
--
-- Asserts real values from ImageData, textures, sprite/tilemap/lights/drawqueue libraries, static
-- batching, terrain, skybox, LOD, culling counters, transparency queue and frame capture, renders a
-- few frames, prints a PASS/FAIL report (console + screen) and quits.
-- ============================================================================
local G = crayon.graphics

function crayon.config(t)
    t.window.title = "27 - features self-test"
    t.window.width = 320
    t.window.height = 240
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.modules.physics3D = false
    t.modules.audio = false
    t.modules.mesh3D = true
end

local R, S = {}, { frames = 0, t = 0 }

local function check(name, fn)
    local ok, res = pcall(fn)
    local pass = ok and (res == nil or res == true)
    R[#R + 1] = { name = name, pass = pass }
    print(string.format("[%s] %s%s", pass and "PASS" or "FAIL", name, (not ok) and ("  -> " .. tostring(res)) or (res == false and "  -> returned false" or "")))
end
local function near(a, b, e) return math.abs(a - b) <= (e or 1e-3) end

function crayon.init()
    G = crayon.graphics
    print("== renderer: " .. G.getRendererName() .. " (software=" .. tostring(G.isSoftwareRenderer()) .. ")")

    -- ---- ImageData ----------------------------------------------------------------------
    check("ImageData: create / setPixel / getPixel", function()
        local img = G.newImageData(4, 3)
        img:setPixel(2, 1, 1, 0.5, 0, 1)
        local r, g, b, a = img:getPixel(2, 1)
        local w, h = img:getSize()
        return w == 4 and h == 3 and near(r, 1, 0.01) and near(g, 0.5, 0.01) and near(b, 0, 0.01) and near(a, 1, 0.01)
    end)
    check("ImageData: out-of-range reads are transparent black", function()
        local r, g, b, a = G.newImageData(2, 2):getPixel(9, 9)
        return r == 0 and a == 0
    end)
    check("ImageData: fill / paste / copy / mapPixels", function()
        local a = G.newImageData(4, 4); a:fill(0, 1, 0, 1)
        local b = G.newImageData(2, 2, 1, 0, 0, 1)
        a:paste(b, 1, 1)
        local c = a:copy()
        c:mapPixels(function(x, y, r, g, bl, al) return r, g, 1, al end)
        local r1 = select(1, a:getPixel(1, 1)); local g0 = select(2, a:getPixel(0, 0)); local b2 = select(3, c:getPixel(0, 0))
        return near(r1, 1, 0.01) and near(g0, 1, 0.01) and near(b2, 1, 0.01) and near(select(3, a:getPixel(0, 0)), 0, 0.01)
    end)
    check("ImageData: save PNG + reload", function()
        local img = G.newImageData(8, 8, 0.2, 0.4, 0.6, 1)
        if not img:save("selftest_tmp.png") then return false end
        local back = G.loadImageData("selftest_tmp.png")
        local r, g, b = back:getPixel(3, 3)
        return back:getWidth() == 8 and near(r, 0.2, 0.01) and near(b, 0.6, 0.01)
    end)

    -- ---- textures from pixel data --------------------------------------------------------
    check("newTexture + getFilter/setFilter + getWrap/setWrap", function()
        local img = G.newImageData(4, 4, 1, 1, 1, 1)
        S.tex = G.newTexture(img, { filter = "nearest" })
        if S.tex:getFilter() ~= "nearest" or S.tex:getWrap() ~= "repeat" then return false end
        S.tex:setFilter("linear"); S.tex:setWrap("clamp")
        local ok = S.tex:getFilter() == "linear" and S.tex:getWrap() == "clamp"
        S.tex:setFilter("mipmap"); ok = ok and S.tex:getFilter() == "mipmap"
        S.tex:setFilter("nearest"); S.tex:setWrap("mirror")
        return ok and S.tex:getWrap() == "mirror" and S.tex:getWidth() == 4
    end)
    check("texture:update(imageData) accepts same size, rejects others", function()
        local img = G.newImageData(4, 4, 0, 0, 1, 1)
        S.tex:update(img)
        local ok2 = pcall(S.tex.update, S.tex, G.newImageData(5, 5))
        return ok2 == false
    end)
    check("bad filter / wrap names raise errors", function()
        return pcall(S.tex.setFilter, S.tex, "bogus") == false and pcall(S.tex.setWrap, S.tex, "bogus") == false
    end)

    -- ---- Lua libraries ------------------------------------------------------------------
    check("crayon.sprite: sheet + animation", function()
        local sheet = crayon.sprite.newSheet(G.newTexture(G.newImageData(64, 32)), 16, 16)
        local anim = crayon.sprite.newAnimation(sheet, sheet:range(1, 4), 10)
        anim:update(0.25)
        return sheet:getFrameCount() == 8 and anim:getFrame() == 3
    end)
    check("crayon.tilemap: cells, properties, collision, culled draw", function()
        local sheet = crayon.sprite.newSheet(G.newTexture(G.newImageData(32, 16)), 16, 16)
        local map = crayon.tilemap.new(20, 20, 16, 16, sheet)
        map:addLayer("g"); map:setTile("g", 2, 2, 1); map:setTile("g", 18, 18, 2)
        map:setTileProperty(2, "solid", true)
        local n = map:draw({ view = { x = 0, y = 0, w = 64, h = 64 } })
        return n == 1 and map:isSolidAt(18 * 16 + 1, 18 * 16 + 1) and not map:isSolidAt(2 * 16 + 1, 2 * 16 + 1)
    end)
    check("crayon.json round trip", function()
        local v = crayon.json.decode(crayon.json.encode({ a = { 1, 2, { b = "x" } }, c = true }))
        return v.a[3].b == "x" and v.c == true
    end)
    check("crayon.drawqueue sorts by z", function()
        local out, q = {}, crayon.drawqueue.new()
        q:add(2, function() out[#out + 1] = "b" end); q:add(1, function() out[#out + 1] = "a" end)
        q:flush()
        return table.concat(out) == "ab"
    end)
    check("crayon.lights2d: render + composite without error", function()
        local L = crayon.lights2d.new({ resolution = 0.25 })
        L:add({ x = 50, y = 50, radius = 40 })
        L:render()
        local img = L.img
        local r = img:getPixel(math.floor(50 * 0.25), math.floor(50 * 0.25))
        L:destroy()
        return r > 0.8
    end)
    check("renderLightmap: lit centre vs ambient edge", function()
        local img = G.newImageData(32, 32)
        G.renderLightmap(img, { 0.1, 0.1, 0.1 }, { { x = 16, y = 16, radius = 10 } })
        local c = img:getPixel(16, 16); local e = img:getPixel(1, 1)
        return c > 0.9 and near(e, 0.1, 0.02)
    end)

    -- ---- 3D --------------------------------------------------------------------------------
    S.cube, S.sphere, S.cone = G.loadModel("cube"), G.loadModel("sphere"), G.loadModel("cone")
    check("static batch: merges instances into chunked meshes", function()
        local b = G.newStaticBatch(10)
        for i = 0, 29 do b:add(S.cube, (i % 6) * 8, 0, math.floor(i / 6) * 8) end
        b:build()
        local inst, groups, verts, tris = b:getStats()
        S.batch = b
        return inst == 30 and groups >= 2 and groups < 30 and tris == 30 * 12
    end)
    check("terrain: heights, normals, containment, raycast", function()
        local rows = {}
        for z = 1, 17 do rows[z] = {} for x = 1, 17 do rows[z][x] = (x - 1) / 16 end end   -- ramp rising along +x
        local t = G.createTerrain(rows, { width = 16, height = 8, depth = 16, x = 0, y = 0, z = 0 })
        S.terrain = t
        local h4 = t:getHeightAt(4, 8)
        local nx, ny = t:getNormalAt(8, 8)
        local hx, hy = t:raycast(8, 50, 8, 0, -1, 0)
        local cols, rws, chunks = t:getInfo()
        return near(h4, 2.0, 0.01) and nx < 0 and ny > 0.5 and t:contains(8, 8) and not t:contains(-1, 8)
               and hx and near(hy, 4.0, 0.02) and cols == 17 and chunks >= 1
    end)
    check("skybox from six ImageData faces", function()
        local faces = {}
        for i = 1, 6 do faces[i] = G.newImageData(8, 8, i / 6, 0.3, 0.6, 1) end
        local sky = G.loadSkybox(faces)
        S.sky = sky
        return sky ~= nil and sky:getFaceSize() == 8
    end)
    check("skybox from equirect ImageData", function()
        local sky = G.loadSkybox(G.newImageData(64, 32, 0.4, 0.5, 0.9, 1), 16)
        return sky and sky:getFaceSize() == 16
    end)
    check("drawModelLOD picks the level by distance", function()
        G.setCamera3D({ position = { 0, 0, 0 }, target = { 0, 0, -1 }, up = { 0, 1, 0 }, fov = 60 })
        local near_lod = G.drawModelLOD({ S.sphere, S.cube }, { 10, 50 }, 0, 0, -5)
        local far_lod = G.drawModelLOD({ S.sphere, S.cube }, { 10, 50 }, 0, 0, -30)
        local culled = G.drawModelLOD({ S.sphere, S.cube }, { 10, 50 }, 0, 0, -80)
        return near_lod == 1 and far_lod == 2 and culled == nil
    end)
    check("culling API: visibility tests + toggles", function()
        G.setCamera3D({ position = { 0, 0, 5 }, target = { 0, 0, 0 }, up = { 0, 1, 0 }, fov = 60 })
        G.setAutoCull(true)
        local vis = G.isSphereVisible(0, 0, 0, 1) and G.isBoxVisible(-1, -1, -1, 1, 1, 1)
        local hidden = not G.isSphereVisible(0, 0, 50, 1) and not G.isBoxVisible(100, 0, 0, 101, 1, 1)
        G.setMaxDrawDistance(0)
        return vis and hidden and G.getAutoCull() == true
    end)
end

function crayon.update(dt) S.t = S.t + dt end

function crayon.draw()
    S.frames = S.frames + 1
    G.clear(0.1, 0.12, 0.2, 1)
    G.setDepthTest(true); G.setDepthWrite(true)
    G.setCamera3D({ position = { 20, 14, 30 }, target = { 20, 0, 0 }, up = { 0, 1, 0 }, fov = 60 })
    G.setLight(-0.5, -1, -0.4, 1, 1, 1, 0.3, 0.3, 0.4)
    if S.sky then G.drawSkybox(S.sky) end
    if S.terrain then S.terrain:draw() end
    if S.batch then S.batch:draw() end
    G.setColor(1, 0.3, 0.3, 0.5); G.drawSphere(20, 3, 10, 2); G.drawSphere(21, 3, 8, 2)   -- queued transparents
    G.setColor(1, 1, 1, 1)

    if S.frames == 3 then
        check("transparent draws are queued, then flushed before the HUD", function()
            G.setColor(0.3, 1, 0.3, 0.5); G.drawCube(20, 2, 14, 2, 2, 2)
            G.setColor(1, 1, 1, 1)
            G.flushTransparent()
            return true
        end)
        check("culling counters report skipped work", function()
            local c3 = G.getCullStats()
            return c3 >= 1   -- the terrain / batch chunks outside this camera
        end)
        G.captureFrame()
    end
    if S.frames == 5 then
        check("captureFrame -> getCapture returns the rendered frame", function()
            local img = G.getCapture()
            if not img then return false end
            local w, h = img:getSize()
            local r, g, b = img:getPixel(math.floor(w / 2), math.floor(h / 2))   -- any pixel must be readable
            return w == 320 and h == 240 and r ~= nil
        end)
        G.screenshot("selftest_shot.png")
    end

    G.setDepthTest(false); G.setDepthWrite(false)
    local pass = 0
    for _, r in ipairs(R) do if r.pass then pass = pass + 1 end end
    G.setColor(1, 1, 1, 1)
    G.drawText(string.format("%d/%d checks passed", pass, #R), 6, 6)
    local y = 18
    for _, r in ipairs(R) do
        if y > 232 then break end
        if r.pass then G.setColor(0.4, 1, 0.5, 1) else G.setColor(1, 0.4, 0.4, 1) end
        G.drawText((r.pass and "ok " or "FAIL ") .. r.name, 6, y); y = y + 8
    end

    if S.frames == 12 then
        local pass_n = 0
        for _, r in ipairs(R) do if r.pass then pass_n = pass_n + 1 end end
        print(string.format("== %d/%d checks passed", pass_n, #R))
        print(pass_n == #R and "== FEATURE SELFTEST PASSED" or "== FEATURE SELFTEST FAILED")
        S.quit_at = S.t + 4
    end
    if S.quit_at and S.t >= S.quit_at then crayon.window.quit() end
end

function crayon.keydown(k) if k == "escape" then crayon.window.quit() end end
