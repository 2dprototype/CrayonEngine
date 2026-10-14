-- crayon.lights2d: 2D point/spot lighting for any scene (GPU or CPU renderer).
--
--   local lights = crayon.lights2d.new()                     -- light map at virtual resolution
--   lights:setAmbient(0.15, 0.15, 0.25)
--   -- every frame, AFTER drawing the scene and BEFORE the HUD:
--   lights:clear()
--   lights:add({ x = px, y = py, radius = 120, color = { 1, 0.8, 0.5 }, intensity = 1.1 })
--   lights:add({ x = lx, y = ly, radius = 160, spot = true, direction = 0.5, cone = 0.4, color = { 0.6, 0.8, 1 } })
--   lights:draw()                                            -- multiplies the light map over the scene
--
-- Options: crayon.lights2d.new({ width = 320, height = 240, resolution = 0.5 })
--   resolution < 1 renders the light map at lower resolution (cheaper; smooth upscaling hides it).
-- Light positions are in world units; call lights:setView(camX, camY [, zoom]) when using a camera.
local G = crayon.graphics
local lights2d = {}
local Lights = {}
Lights.__index = Lights

function lights2d.new(opts)
    opts = opts or {}
    local vw, vh = opts.width, opts.height
    if not (vw and vh) then
        local cw, ch = 320, 240
        if crayon.window and crayon.window.getResolution then cw, ch = crayon.window.getResolution() end
        vw, vh = vw or cw, vh or ch
    end
    local res = opts.resolution or 1
    local lw, lh = math.max(1, math.floor(vw * res + 0.5)), math.max(1, math.floor(vh * res + 0.5))
    local img = G.newImageData(lw, lh)
    local self = setmetatable({ img = img, tex = G.newTexture(img, { filter = "linear", wrap = "clamp" }),
                                vw = vw, vh = vh, res = res, ambient = { 0.1, 0.1, 0.15 }, list = {},
                                cam_x = 0, cam_y = 0, zoom = 1 }, Lights)
    return self
end

function Lights:setAmbient(r, g, b) self.ambient = { r, g, b }; end
function Lights:setView(x, y, zoom) self.cam_x, self.cam_y, self.zoom = x, y, zoom or 1 end
function Lights:clear() self.list = {} end
function Lights:add(light) self.list[#self.list + 1] = light; return light end
function Lights:count() return #self.list end

-- Renders the light map (CPU) and uploads it; call draw() to composite, or use this to get the
-- ImageData for custom use.
function Lights:render()
    local scale = self.res * self.zoom
    G.renderLightmap(self.img, self.ambient, self.list,
        { offsetX = self.cam_x, offsetY = self.cam_y, scaleX = scale, scaleY = scale })
    self.tex:update(self.img)
    return self.img
end

function Lights:draw()
    self:render()
    G.setBlendMode("multiply")
    G.setColor(1, 1, 1, 1)
    G.drawSprite(self.tex, 0, 0, self.vw, self.vh)
    G.setBlendMode("alpha")
end

function Lights:destroy()
    if self.tex then self.tex:destroy(); self.tex = nil end
end

crayon.lights2d = lights2d
