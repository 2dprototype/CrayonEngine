-- Minimal mock of the engine API used by the prelude libraries. Records every graphics call.
crayon = {}
local calls = {}
crayon._calls = calls
local function rec(name)
    return function(...)
        calls[#calls + 1] = { name = name, args = { ... }, n = select("#", ...) }
    end
end

local function make_texture(w, h)
    local t = { _w = w, _h = h }
    function t:getWidth() return self._w end
    function t:getHeight() return self._h end
    function t:update(img) self._updated = (self._updated or 0) + 1; self._last_img = img end
    function t:destroy() self._destroyed = true end
    return t
end

local files = {}
crayon._files = files
crayon.fs = { readText = function(p) if files[p] then return files[p] end return nil, "could not open file for reading" end }
crayon.window = { getResolution = function() return 320, 240 end }

local G = {}
crayon.graphics = G
for _, n in ipairs({ "drawSpritePart", "setColor", "setBlendMode", "drawSprite", "drawRect" }) do G[n] = rec(n) end
G.loadTexture = function(path) local t = make_texture(128, 64); t._path = path; return t end
G.getTextureSize = function(t) return t._w, t._h end
G.newImageData = function(w, h) return { _w = w, _h = h } end
G.newTexture = function(img, opts) local t = make_texture(img._w, img._h); t._opts = opts; return t end
G.renderLightmap = function(img, amb, lights, view) calls[#calls + 1] = { name = "renderLightmap", args = { img, amb, lights, view }, n = 4 } end

function crayon._reset() for i = #calls, 1, -1 do calls[i] = nil end end
function crayon._last(name) for i = #calls, 1, -1 do if calls[i].name == name then return calls[i] end end end
function crayon._count(name) local n = 0 for _, c in ipairs(calls) do if c.name == name then n = n + 1 end end return n end

local failures, checks = 0, 0
function check(cond, msg)
    checks = checks + 1
    if cond then print("  ok:   " .. msg) else failures = failures + 1; print("  FAIL: " .. msg) end
end
function approx(a, b, eps) return math.abs(a - b) <= (eps or 1e-6) end
function finish(name)
    print(string.format("\n%s %s (%d checks, %d failures)", name, failures == 0 and "PASSED" or "FAILED", checks, failures))
    if failures > 0 then error(name .. " failed", 0) end
end
