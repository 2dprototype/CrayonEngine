function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("My Game")
-- crayon.graphics.setRetroEffects({
    -- jitterResolution = {160, 120},   -- Pixel snap resolution
    -- affine           = 1.0,          -- 0 = perspective, 1 = affine
    -- dither           = true,
    -- colorDepth       = 32,           -- 8 or 32
    -- fog = {
        -- startDist = 5.0,
        -- endDist   = 25.0,
        -- color     = {0.1, 0.1, 0.2}
    -- },
    -- crt = {
        -- scanlines = 0.25,
        -- curvature = 0.05,
        -- vignette  = 0.25
    -- }
-- })
-- One-shot synth voice from a wave template
local blip = crayon.audio.createSound({
    wave = "square", freq = 880, duration = 0.12,
    envelope = { attack = 0.001, decay = 0.03, sustain = 0.4, release = 0.08 },
    filter   = { cutoff = 3000, highpass = true },
    pitchSweep = { from = 1200, to = 400, time = 0.1 }
})
crayon.audio.playSound(blip)

-- Bake a buffer by sampling a Lua function
local pad = crayon.audio.createBuffer(44100, function(t, i)
    return math.sin(t * 220.0 * 6.28318) * 0.4
end)
crayon.audio.playSound(pad)
end

function crayon.update(dt)
    if crayon.key.isPressed("escape") then
        crayon.window.quit()
    end
end

x, y, z, rx, ry, rz = 1, 2, 3, 4, 5, 6
tex = nil
t = 10
x1, y1, z1, x2, y2, z2 = 3, 4, 5, 6, 7, 8

function test_1()
-- 3D Camera
crayon.graphics.setCamera3D({
    position = {0, 3, 8},
    target   = {0, 0, 0},
    fov      = 60,
    near     = 0.1,
    far      = 1000.0
})

-- Lighting
crayon.graphics.setLight(-0.5, -1, -0.7, 1, 0.95, 0.9, 0.25, 0.25, 0.3)
crayon.graphics.setPointLight(0, 0, 5, 0, 1, 1, 1, 10, 1)
crayon.graphics.setSpotLight(0, 0, 5, 0, 0, -1, 0, 15, 1, 1, 1, 1, 15, 25)
crayon.graphics.setShadingMode("gouraud")

-- 3D Objects
crayon.graphics.drawCube(x, y, z, 1, 1, 1, tex, rx, ry, rz)
crayon.graphics.drawSphere(x, y, z, 0.5, tex)
crayon.graphics.drawBillboard(x, y, z, 1, 1, tex, "cylindrical")

-- Models
local model = crayon.graphics.loadModel("cube")  -- or "sphere", "torus", or .obj/.gltf/.glb path
crayon.graphics.drawModel(model, x, y, z, rx, ry, rz, 1, 1, 1, tex)

-- 3D Transforms
crayon.graphics.pushMatrix()
crayon.graphics.translate(0, 2, 0)
crayon.graphics.rotate(t, 0, 1, 0)
crayon.graphics.scale(1.5, 1.5, 1.5)
crayon.graphics.drawSphere(0, 0, 0, 0.5)
crayon.graphics.popMatrix()

-- Lines & Debug
crayon.graphics.drawLine3D(x1, y1, z1, x2, y2, z2)
crayon.graphics.drawGrid3D(20, 20, 0)
crayon.graphics.drawAxes3D(0, 0, 0, 1)
crayon.graphics.drawCubeWires(0, 1, 0, 1, 1, 1)
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("Hello World!", 10, 10, 2)
	
	-- test_1()
	-- t = t + 1
	local canvas = crayon.graphics.createCanvas(320, 240)
canvas:renderTo(function()
    canvas:clear(0, 0, 0, 1)
    crayon.graphics.setColor(1, 0.5, 0)
    crayon.graphics.drawCircle("fill", 160, 120, 50)
end)
crayon.graphics.drawSprite(canvas:getTexture(), 0, 0)
end