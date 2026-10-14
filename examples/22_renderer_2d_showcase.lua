-- ============================================================================
-- Example 22: 2D showcase for both renderers (GPU and CPU rasterizer)
--
--   crayon --game examples/22_renderer_2d_showcase.lua --renderer=soft
--
-- Exercises the 2D batcher paths the software backend reimplements: rects, rounded rects,
-- circles, ellipses, polygons, lines, gradients, text, sprites (rotated / tiled / 9-slice),
-- all blend modes (alpha / additive / multiply / none), scissor + scissor stack, the 2D matrix
-- stack and Camera2D. Press SPACE to toggle the camera zoom, ESC to quit.
-- ============================================================================
local G = crayon.graphics

function crayon.config(t)
    t.window.title = "22 - 2D showcase"
    t.window.width = 320
    t.window.height = 240
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.modules.physics3D = false
    t.modules.audio = false
    t.modules.mesh3D = false
end

local S = { zoomed = false }

function crayon.init()
    G = crayon.graphics
    S.tex = G.loadTexture("assets/models/cube_gltf/Cube_BaseColor.png")
    print("renderer: " .. G.getRendererName() .. " (software=" .. tostring(G.isSoftwareRenderer()) .. ")")
end

function crayon.keydown(k, isRepeat)
    if isRepeat then return end
    if k == "space" then S.zoomed = not S.zoomed end
    if k == "escape" then crayon.window.quit() end
end

function crayon.draw()
    local t = crayon.time.getTime()
    G.clear(0.12, 0.13, 0.2, 1)
    G.setBlendMode("alpha")

    if S.zoomed then G.setCamera2D({ x = 160, y = 120, zoom = 1.6, originX = 160, originY = 120 }) else G.resetCamera2D() end

    -- row 1: filled + outlined shapes
    G.setColor(0.9, 0.3, 0.3, 1);   G.drawRect("fill", 8, 8, 40, 28)
    G.setColor(1, 1, 1, 1);         G.drawRect("line", 8, 8, 40, 28, 2)
    G.setColor(0.3, 0.8, 0.4, 1);   G.drawRoundedRect("fill", 56, 8, 40, 28, 8)
    G.setColor(0.3, 0.6, 1, 1);     G.drawCircle("fill", 120, 22, 14, 24)
    G.setColor(1, 1, 1, 1);         G.drawCircle("line", 120, 22, 14, 24)
    G.setColor(1, 0.8, 0.2, 1);     G.drawEllipse("fill", 160, 22, 18, 10)
    G.setColor(0.8, 0.4, 1, 1);     G.drawPolygon("fill", { 190, 36, 205, 8, 220, 36, 205, 28 })
    G.setColor(1, 1, 1, 1);         G.drawLine(230, 8, 300, 36, 2)

    -- row 2: gradients
    G.drawGradientRect(8, 46, 70, 26, { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { 1, 1, 0, 1 })
    G.drawGradientH(86, 46, 70, 26, { 0, 0, 0, 1 }, { 1, 1, 1, 1 })
    G.drawGradientV(164, 46, 70, 26, { 1, 0.5, 0, 1 }, { 0.2, 0, 0.5, 1 })

    -- row 3: textured sprites (rotated, tiled, 9-slice)
    G.setColor(1, 1, 1, 1)
    G.drawSprite(S.tex, 8, 82, 40, 40, t, 20, 20)
    G.drawSpriteTiled(S.tex, 60, 82, 60, 40, 15, 15)
    G.drawSprite9Slice(S.tex, 130, 82, 80, 40, 6, 6, 6, 6)
    G.drawTextureRot(S.tex, 240, 82, 40, 40, -t)

    -- blend modes over a bright bar
    G.setColor(1, 1, 1, 1); G.drawRect("fill", 8, 132, 300, 20)
    G.setColor(0.9, 0.2, 0.2, 0.5); G.setBlendMode("alpha");    G.drawRect("fill", 12, 128, 40, 28)
    G.setColor(0.2, 0.5, 0.9, 0.5); G.setBlendMode("additive"); G.drawRect("fill", 100, 128, 40, 28)
    G.setColor(0.6, 0.9, 0.3, 1.0); G.setBlendMode("multiply"); G.drawRect("fill", 190, 128, 40, 28)
    G.setColor(0.9, 0.9, 0.2, 1.0); G.setBlendMode("none");     G.drawRect("fill", 260, 128, 40, 28)
    G.setBlendMode("alpha")

    -- scissor stack: the oversized magenta/cyan rects are clipped to their windows
    G.pushScissor(8, 162, 90, 30)
    G.setColor(1, 0, 1, 1); G.drawRect("fill", 0, 150, 200, 60)
    G.pushScissor(30, 170, 40, 14)
    G.setColor(0, 1, 1, 1); G.drawRect("fill", 0, 150, 200, 60)
    G.popScissor()
    G.popScissor()

    -- 2D matrix stack: rotating, scaling group
    G.pushMatrix2D()
    G.translate2D(200, 180)
    G.rotate2D(t)
    G.scale2D(1 + 0.2 * math.sin(t * 2), 1)
    G.setColor(1, 0.6, 0.2, 1); G.drawRect("fill", -20, -12, 40, 24)
    G.setColor(1, 1, 1, 1);     G.drawText("2D", -6, -4)
    G.popMatrix2D()

    -- text
    G.setColor(1, 1, 1, 1)
    G.drawText("2D showcase - " .. G.getRendererName(), 8, 205)
    G.drawText(string.format("%d fps   SPACE: camera zoom", math.floor(crayon.window.getFps())), 8, 220)
end
