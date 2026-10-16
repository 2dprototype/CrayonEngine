-- Runs each example script for a few frames against a permissive mock of the engine API, with the real
-- prelude libraries loaded. Catches syntax errors, nil indexing, bad control flow and library misuse
-- (not engine-side API signature mismatches).
local examples = {
    "20_renderer_selftest.lua", "21_renderer_3d_showcase.lua", "22_renderer_2d_showcase.lua",
    "23_renderer_benchmark.lua", "24_renderer_forced_soft.lua", "25_2d_features.lua", "26_3d_features.lua",
    "27_features_selftest.lua", "28_collision.lua",
}
local OBJ_FUNCS = { loadModel = 1, loadTexture = 1, newTexture = 1, newImageData = 1, loadSkybox = 1, createTerrain = 1,
                    newStaticBatch = 1, getWhiteTexture = 1, loadImageData = 1, createCanvas = 1, loadFont = 1, loadShader = 1 }

local function make_env()
    local calls = {}
    local Obj = {}
    Obj.__index = function(t, k)
        if k == "getWidth" or k == "getHeight" then return function() return 64 end end
        if k == "getSize" then return function() return 64, 64 end end
        if k == "getStats" or k == "getInfo" then return function() return 10, 4, 100, 50 end end
        if k == "raycast" then return function() return 1, 2, 3 end end
        if k == "getNodes" or k == "getCollisionParts" then
            return function() return { { name = "spawn", properties = { team = "blue", kind = "player" }, wx = 0, wy = 1, wz = 2 } } end
        end
        return function(...) return 0, 0, 0, 0 end
    end
    local function new_obj() return setmetatable({}, Obj) end
    local G = setmetatable({}, { __index = function(t, k)
        if OBJ_FUNCS[k] then return function(...) calls[#calls + 1] = k; return new_obj() end end
        if k == "isSoftwareRenderer" then return function() return false end end
        if k == "getRendererName" then return function() return "mock" end end
        if k == "project" then return function() return 160, 120, true end end
        if k == "unproject" then return function() return 0, 0, 5, 0, 0, -1 end end
        return function(...) calls[#calls + 1] = k; return 0, 0, 0, 0 end
    end })
    local key_down = {}
    local env
    env = { crayon = {
        graphics = G,
        physics3D = setmetatable({}, { __index = function() return function() return new_obj() end end }),
        window = { getFps = function() return 60 end, getResolution = function() return 320, 240 end,
                   quit = function() env.quit_called = true end },
        time = { getTime = function() return env.now or 0 end },
        key = { isDown = function(k) return key_down[k] or false end },
        fs = { readText = function() return nil, "no files in smoke test" end },
        config = nil,
    }, _calls = calls, _keys = key_down }
    env.crayon.config = nil
    return env
end

local failures = 0
for _, name in ipairs(examples) do
    local env = make_env()
    _G.crayon = env.crayon
    for _, f in ipairs({ "10_json.lua", "20_sprite.lua", "30_tilemap.lua", "40_drawqueue.lua", "50_lights2d.lua" }) do
        dofile("src/scripting/prelude/" .. f)
    end
    local path = "examples/" .. name
    local fh = io.open(path, "r")
    if not fh then
        print(string.format("  skip  %-34s (not present)", name))
    else
        fh:close()
        local ok, err = pcall(function()
            dofile(path)
            local cfg = { window = {}, modules = {}, graphics = {} }
            if crayon.config then crayon.config(cfg) end
            if crayon.init then crayon.init() end
            for frame = 1, 40 do
                env.now = frame / 30
                env._keys["a"] = (frame % 7 == 0); env._keys["d"] = (frame % 5 == 0)
                env._keys["w"] = (frame % 11 == 0); env._keys["s"] = (frame % 13 == 0)
                if crayon.update then crayon.update(1 / 30) end
                if crayon.draw then crayon.draw() end
                if crayon.keydown and frame % 9 == 0 then
                    for _, k in ipairs({ "1", "2", "3", "j", "t", "f", "o", "c", "l", "m", "p", "b", "space" }) do crayon.keydown(k, false) end
                end
            end
        end)
        if ok then print(string.format("  ok    %-34s (%d graphics calls in 40 frames)", name, #env._calls))
        else failures = failures + 1; print(string.format("  FAIL  %-34s %s", name, tostring(err))) end
    end
end
print(failures == 0 and "\nSMOKE TEST PASSED" or ("\nSMOKE TEST FAILED (" .. failures .. ")"))
if failures > 0 then error("smoke test failed", 0) end
