-- crayon.drawqueue: z-ordered draw calls (2D layers, y-sorting, mixed sprite/shape depth).
--
--   local q = crayon.drawqueue.new()
--   q:add(10, function() G.drawRect("fill", 0, 0, 50, 50) end)            -- z = 10
--   q:add(player.y, function() player_anim:draw(player.x, player.y) end)  -- y-sort
--   q:flush()                    -- runs everything sorted by z (ties keep insertion order), then clears
local drawqueue = {}
local Queue = {}
Queue.__index = Queue

function drawqueue.new()
    return setmetatable({ items = {}, seq = 0 }, Queue)
end

-- add(z, fn, ...)   fn is called as fn(...) when flushed
function Queue:add(z, fn, ...)
    assert(type(fn) == "function", "drawqueue:add needs a function")
    self.seq = self.seq + 1
    self.items[#self.items + 1] = { z = z, seq = self.seq, fn = fn, n = select("#", ...), args = { ... } }
end

function Queue:count() return #self.items end
function Queue:clear() self.items = {}; self.seq = 0 end

local unpack = table.unpack or unpack
function Queue:flush()
    local items = self.items
    table.sort(items, function(a, b)
        if a.z ~= b.z then return a.z < b.z end
        return a.seq < b.seq
    end)
    for i = 1, #items do
        local it = items[i]
        it.fn(unpack(it.args, 1, it.n))
    end
    self:clear()
end

crayon.drawqueue = drawqueue
