-- ============================================================================
-- Example 04: Offscreen Canvases & Custom Shaders
-- Demonstrates: crayon.graphics.createCanvas, canvas:renderTo,
--               custom post-processing shaders, and uniform uploading.
-- ============================================================================

local gameCanvas = nil
local crtShader  = nil
local elapsedTime = 0.0
local playerX = 160
local playerY = 120

-- ----------------------------------------------------------------------------
-- Vertex shader: Crayon's loadShader() requires BOTH sources.
-- This matches the standard 2D batch attribute layout:
--   location 0 = position (vec2 in pixel space)
--   location 1 = texcoord (vec2)
--   location 2 = color    (vec4)
-- and expects a single u_projection matrix uniform.
-- ----------------------------------------------------------------------------
local crtVertShader = [[
#version 330 core
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;

uniform mat4 u_projection;

out vec2 v_uv;
out vec4 v_color;

void main() {
    v_uv = a_uv;
    v_color = a_color;
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
}
]]

-- ----------------------------------------------------------------------------
-- Fragment shader: CRT scanlines + vignette post-process.
-- ----------------------------------------------------------------------------
local crtFragShader = [[
#version 330 core
in vec2 v_uv;
in vec4 v_color;
out vec4 frag_color;

uniform sampler2D u_texture;
uniform float u_time;
uniform vec2  u_resolution;

void main() {
    vec2 uv = v_uv;

    // Sample the canvas texture
    vec4 texColor = texture(u_texture, uv);

    // Horizontal scanlines (darker lines every few pixels)
    float scanline = sin(uv.y * u_resolution.y * 1.5 + u_time * 5.0) * 0.08;
    vec3 color = texColor.rgb - scanline;

    // Vignette: darken edges
    vec2 centered = uv - 0.5;
    float dist = dot(centered, centered);
    color *= clamp(1.0 - dist * 1.2, 0.0, 1.0);

    frag_color = vec4(color, texColor.a) * v_color;
}
]]

function crayon.init()
    crayon.window.setTitle("04 - Canvases & Custom Shaders")
    crayon.window.setResolution(640, 480)

    -- 1. Create a 320x240 internal low-res canvas for retro pixel density
    gameCanvas = crayon.graphics.createCanvas(320, 240)

    -- 2. Compile custom post-processing shader (both sources required)
    crtShader = crayon.graphics.loadShader(crtVertShader, crtFragShader)
end

function crayon.update(dt)
    elapsedTime = elapsedTime + dt

    -- Player movement in virtual (canvas) resolution: 320x240
    local speed = 110.0
    if crayon.input.isKeyDown("left",  "a") then playerX = playerX - speed * dt end
    if crayon.input.isKeyDown("right", "d") then playerX = playerX + speed * dt end
    if crayon.input.isKeyDown("up",    "w") then playerY = playerY - speed * dt end
    if crayon.input.isKeyDown("down",  "s") then playerY = playerY + speed * dt end

    playerX = math.max(16, math.min(304, playerX))
    playerY = math.max(16, math.min(224, playerY))
end

function crayon.draw()
    -- 1. Render the game scene onto the offscreen canvas
    gameCanvas:renderTo(function()
        gameCanvas:clear(0.08, 0.09, 0.14, 1.0)

        -- Animated concentric rings
        for i = 1, 6 do
            local rad = 25 + i * 20 + math.sin(elapsedTime * 2.0 + i) * 6
            crayon.graphics.setColor(0.15, 0.2 + i * 0.08, 0.4 + i * 0.07, 0.4)
            crayon.graphics.drawCircle("line", 160, 120, rad, 32)
        end

        -- Player entity (rounded — use drawRoundedRect, not drawRect)
        crayon.graphics.setColor(1.0, 0.7, 0.2, 1.0)
        crayon.graphics.drawRoundedRect("fill", playerX - 10, playerY - 10, 20, 20, 4)
        crayon.graphics.setColor(1.0, 1.0, 1.0, 0.9)
        crayon.graphics.drawRoundedRect("line", playerX - 10, playerY - 10, 20, 20, 4)

        -- HUD text inside the canvas
        crayon.graphics.setColor(0.9, 0.9, 0.9, 1.0)
        crayon.graphics.drawText("Move: WASD / Arrow Keys", 10, 10, { scale = 1.0 })
    end)

    -- 2. Clear the main screen
    crayon.graphics.clear(0.0, 0.0, 0.0, 1.0)

    -- 3. Blit the canvas through the CRT shader, stretched to the full window
    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)

    if crtShader then
        crayon.graphics.setShader(crtShader)
        crtShader:sendFloat("u_time", elapsedTime)
        crtShader:sendVec2("u_resolution", 320.0, 240.0)  -- canvas-space
    end

    -- drawSprite() blits a Texture or Canvas; there is no drawTexture().
    crayon.graphics.drawSprite(gameCanvas:getTexture(), 0, 0, 640, 480)

    -- 4. Reset shader to default before drawing overlay text
    crayon.graphics.setShader(nil)

    crayon.graphics.setColor(1.0, 1.0, 1.0, 1.0)
    crayon.graphics.drawText("Post-Process: CRT Scanlines + Vignette via Custom Shader", 16, 455, { scale = 1.0 })
end