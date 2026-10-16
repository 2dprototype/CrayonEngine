local crate_tex
local gray_shader
local scene_canvas
local init_ok = false

function crayon.config(t)
    t.window.title         = "Grayscale 3D Test"
    t.window.width         = 640
    t.window.height        = 360
    t.window.virtualWidth  = 640
    t.window.virtualHeight = 360
    t.graphics.renderer    = "gl"
end

function crayon.init()
    crate_tex = crayon.graphics.loadTexture("assets/textures/crate.bmp")

    -- Minimal 2D-batch override shader. Matches Batch2D::Vertex2D exactly:
    --   loc 0 = a_pos   (vec2)
    --   loc 1 = a_uv    (vec2)
    --   loc 2 = a_color (vec4)
    -- The batch sets u_proj and u_texture for us.
    gray_shader = crayon.graphics.loadShader([[
    #version 330 core
    layout (location = 0) in vec2 a_pos;
    layout (location = 1) in vec2 a_uv;
    layout (location = 2) in vec4 a_color;

    uniform mat4 u_proj;

    out vec2 v_uv;
    out vec4 v_color;

    void main() {
        v_uv    = a_uv;
        v_color = a_color;
        gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
    }
    ]], [[
    #version 330 core
    in vec2 v_uv;
    in vec4 v_color;

    uniform sampler2D u_texture;

    out vec4 frag_color;

    void main() {
        vec4 c  = texture(u_texture, v_uv) * v_color;
        float l = dot(c.rgb, vec3(0.299, 0.587, 0.114));
        frag_color = vec4(vec3(l), c.a);
    }
    ]])

    if not gray_shader then
        crayon.print("WARN: gray shader failed to load")
        return
    end

    scene_canvas = crayon.graphics.createCanvas(640, 360)
    if not scene_canvas then
        crayon.print("WARN: createCanvas returned nil")
        return
    end

    init_ok = true
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.15)

    -- Fallback: if init failed, just draw everything plainly so you still
    -- get a picture and a working engine loop while you debug.
    if not init_ok then
        crayon.graphics.setColor(1, 0.4, 0.4, 1)
        crayon.graphics.drawRect("fill", 50, 50, 100, 100)
        crayon.graphics.setColor(0.4, 1, 0.4, 1)
        crayon.graphics.drawRect("fill", 400, 50, 100, 100)
        crayon.graphics.setColor(1, 1, 1, 1)
        crayon.graphics.drawText("INIT FAILED - check console", 8, 8, 1)
        return
    end

    -- 1. Render the 3D scene into the offscreen canvas.
    scene_canvas:renderTo(function()
        scene_canvas:clear(0.0, 0.0, 0.0, 1.0)

        crayon.graphics.setCamera3D({
            position = {  3, 3, -6 },
            target   = {  1, 1,  1 },
            up       = {  0, 1,  0 },
            fov      = 60, near = 0.1, far = 100,
        })

        crayon.graphics.setColor(1, 1, 1, 1)
        crayon.graphics.drawCube(1, 1, 1, 1, 1, 1, crate_tex, 0, 0, 0)
    end)

    -- 2. Blit only that canvas through the grayscale shader.
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.setShader(gray_shader)
    crayon.graphics.drawSprite(scene_canvas:getTexture(), 0, 0, 640, 360)
    crayon.graphics.setShader(nil)

    -- 3. HUD drawn with the default shader — unaffected.
    crayon.graphics.setColor(1, 0.4, 0.4, 1)
    crayon.graphics.drawRect("fill", 50, 50, 100, 100)

    crayon.graphics.setColor(0.4, 1, 0.4, 1)
    crayon.graphics.drawRect("fill", 400, 50, 100, 100)
end