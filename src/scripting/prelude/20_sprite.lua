-- crayon.sprite: sprite sheets, texture atlases and frame animation.
--
--   local sheet = crayon.sprite.newSheet(tex, 32, 32)               -- uniform grid
--   local atlas = crayon.sprite.newAtlas(tex, { idle = {0,0,32,32}, jump = {32,0,32,40} })
--   sheet:draw(5, x, y, { scale = 2, flipX = true, rotation = 0.3, ox = 16, oy = 32 })
--   local run = crayon.sprite.newAnimation(sheet, sheet:range(1, 8), 12)    -- 12 fps, loops
--   run:update(dt);  run:draw(x, y, { flipX = facingLeft })
--   local player = crayon.sprite.newAnimator(sheet, {
--       idle = { frames = {1, 2}, fps = 2 },
--       run  = { frames = sheet:range(9, 16), fps = 14 },
--       jump = { frames = {17}, mode = "once" } })
--   player:play("run"); player:update(dt); player:draw(x, y)
local G = crayon.graphics
local sprite = {}

local Sheet = {}
Sheet.__index = Sheet
local Animation = {}
Animation.__index = Animation
local Animator = {}
Animator.__index = Animator

local function texture_size(texture)
    if texture.getWidth then return texture:getWidth(), texture:getHeight() end
    return G.getTextureSize(texture)
end

local function make_quad(x, y, w, h, tw, th, ox, oy)
    return { x = x, y = y, w = w, h = h, ox = ox or 0, oy = oy or 0,
             u0 = x / tw, v0 = y / th, u1 = (x + w) / tw, v1 = (y + h) / th }
end

-- Uniform grid sheet. opts: margin, spacing, columns, rows, count, names = { name = frameIndex }
function sprite.newSheet(texture, frame_w, frame_h, opts)
    assert(texture, "newSheet: texture is nil")
    assert(frame_w and frame_h and frame_w > 0 and frame_h > 0, "newSheet: frame size must be positive")
    opts = opts or {}
    local tw, th = texture_size(texture)
    local margin, spacing = opts.margin or 0, opts.spacing or 0
    local cols = opts.columns or math.floor((tw - 2 * margin + spacing) / (frame_w + spacing))
    local rows = opts.rows or math.floor((th - 2 * margin + spacing) / (frame_h + spacing))
    local count = opts.count or (cols * rows)
    local sheet = setmetatable({ texture = texture, frames = {}, names = {}, columns = cols, rows = rows,
                                 count = count, frame_w = frame_w, frame_h = frame_h, tex_w = tw, tex_h = th }, Sheet)
    for i = 1, count do
        local col, row = (i - 1) % cols, math.floor((i - 1) / cols)
        sheet.frames[i] = make_quad(margin + col * (frame_w + spacing), margin + row * (frame_h + spacing), frame_w, frame_h, tw, th)
    end
    if opts.names then for name, idx in pairs(opts.names) do sheet.names[name] = idx end end
    return sheet
end

-- Free-form atlas. rects: { name = { x, y, w, h [, pivotX, pivotY] } } (also accepts x=,y=,w=,h= keys).
-- Frames are numbered in name order so indices are stable; look them up by name.
function sprite.newAtlas(texture, rects)
    assert(texture, "newAtlas: texture is nil")
    local tw, th = texture_size(texture)
    local names = {}
    for name in pairs(rects) do names[#names + 1] = name end
    table.sort(names)
    local sheet = setmetatable({ texture = texture, frames = {}, names = {}, count = #names, tex_w = tw, tex_h = th }, Sheet)
    for i, name in ipairs(names) do
        local r = rects[name]
        sheet.frames[i] = make_quad(r.x or r[1], r.y or r[2], r.w or r[3], r.h or r[4], tw, th, r.ox or r[5], r.oy or r[6])
        sheet.names[name] = i
    end
    return sheet
end

function Sheet:getFrame(id)
    local q = type(id) == "string" and self.frames[self.names[id] or -1] or self.frames[id]
    if not q then error("sprite sheet has no frame '" .. tostring(id) .. "'", 3) end
    return q
end

function Sheet:getFrameCount() return self.count end

-- { a, a+1, ..., b }
function Sheet:range(a, b)
    local t = {}
    for i = a, b, (b >= a) and 1 or -1 do t[#t + 1] = i end
    return t
end

-- opts: scale | sx, sy;  rotation (radians);  ox, oy (origin inside the frame, pixels);
--       flipX, flipY;  color = {r,g,b,a} (sets the current colour)
function Sheet:draw(frame, x, y, opts)
    local q = self:getFrame(frame)
    opts = opts or {}
    if opts.color then G.setColor(opts.color[1], opts.color[2], opts.color[3], opts.color[4] or 1) end
    local sx = opts.sx or opts.scale or 1
    local sy = opts.sy or opts.scale or 1
    local ox = opts.ox or q.ox
    local oy = opts.oy or q.oy
    local u0, v0, u1, v1 = q.u0, q.v0, q.u1, q.v1
    if opts.flipX then u0, u1 = u1, u0; ox = q.w - ox end
    if opts.flipY then v0, v1 = v1, v0; oy = q.h - oy end
    G.drawSpritePart(self.texture, x, y, u0, v0, u1, v1, q.w * sx, q.h * sy, opts.rotation or 0, ox * sx, oy * sy)
end

-- ---------------------------------------------------------------------------------------------
-- Animation. frames: list of frame indices/names. mode: "loop" (default) | "once" | "pingpong".
-- opts: mode, durations = { seconds per frame }, onFinish = fn(anim), onLoop = fn(anim)
function sprite.newAnimation(sheet, frames, fps, opts)
    assert(sheet and frames and #frames > 0, "newAnimation: needs a sheet and at least one frame")
    opts = opts or {}
    local mode = opts.mode or "loop"
    local step = 1 / (fps or 10)
    local seq, dur = {}, {}
    for i, f in ipairs(frames) do seq[i] = f; dur[i] = (opts.durations and opts.durations[i]) or step end
    if mode == "pingpong" then
        for i = #frames - 1, 2, -1 do seq[#seq + 1] = frames[i]; dur[#dur + 1] = dur[i] end
    end
    local total = 0
    for _, d in ipairs(dur) do total = total + d end
    return setmetatable({ sheet = sheet, seq = seq, dur = dur, total = total, mode = mode, time = 0, index = 1,
                          playing = true, finished = false, speed = 1, loops = 0,
                          onFinish = opts.onFinish, onLoop = opts.onLoop }, Animation)
end

local function locate(self)
    local t, idx = self.time, 1
    for i = 1, #self.dur do
        if t < self.dur[i] then idx = i; break end
        t = t - self.dur[i]
        idx = i
    end
    self.index = idx
end

function Animation:update(dt)
    if not self.playing or self.finished then return end
    self.time = self.time + dt * self.speed
    if self.mode == "once" then
        if self.time >= self.total then
            self.time = self.total
            self.index = #self.seq
            self.finished = true
            self.playing = false
            if self.onFinish then self.onFinish(self) end
            return
        end
    elseif self.time >= self.total then
        local wraps = math.floor(self.time / self.total)
        self.time = self.time - wraps * self.total
        self.loops = self.loops + wraps
        if self.onLoop then self.onLoop(self) end
    end
    locate(self)
end

function Animation:draw(x, y, opts) self.sheet:draw(self.seq[self.index], x, y, opts) end
function Animation:getFrame() return self.seq[self.index] end
function Animation:getFrameIndex() return self.index end
function Animation:isFinished() return self.finished end
function Animation:isPlaying() return self.playing end
function Animation:pause() self.playing = false end
function Animation:resume() if not self.finished then self.playing = true end end
function Animation:setSpeed(s) self.speed = s end
function Animation:setFps(fps)
    local step = 1 / fps
    self.total = 0
    for i = 1, #self.dur do self.dur[i] = step; self.total = self.total + step end
end
function Animation:reset()
    self.time, self.index, self.finished, self.playing, self.loops = 0, 1, false, true, 0
end
-- jump to a frame position (1-based) inside the sequence
function Animation:seek(i)
    i = math.max(1, math.min(i, #self.seq))
    local t = 0
    for k = 1, i - 1 do t = t + self.dur[k] end
    self.time, self.index = t, i
end

-- ---------------------------------------------------------------------------------------------
-- Animator: a named set of animations over one sheet.
function sprite.newAnimator(sheet, defs)
    local a = setmetatable({ sheet = sheet, anims = {}, current_name = nil, current = nil }, Animator)
    for name, d in pairs(defs) do
        a.anims[name] = sprite.newAnimation(sheet, d.frames, d.fps or 10,
            { mode = d.mode, durations = d.durations, onFinish = d.onFinish, onLoop = d.onLoop })
    end
    return a
end

-- play(name [, restart]): does nothing when `name` is already playing unless restart is true.
function Animator:play(name, restart)
    local anim = self.anims[name]
    if not anim then error("animator has no animation '" .. tostring(name) .. "'", 2) end
    if self.current_name == name and not restart then return anim end
    self.current_name, self.current = name, anim
    anim:reset()
    return anim
end
function Animator:update(dt) if self.current then self.current:update(dt) end end
function Animator:draw(x, y, opts) if self.current then self.current:draw(x, y, opts) end end
function Animator:getCurrent() return self.current_name end
function Animator:isFinished() return self.current ~= nil and self.current.finished end

crayon.sprite = sprite
