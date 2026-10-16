local crate_tex
local gray

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

    gray = crayon.graphics.loadShader([[
    #version 330 core
    layout (location = 0) in vec2 a_pos;
    layout (location = 1) in vec2 a_uv;
    out vec2 v_uv;
    void main() {
        v_uv = a_uv;
        gl_Position = vec4(a_pos, 0.0, 1.0);
    }
    ]], [[
    #version 330 core
    in vec2 v_uv;
    uniform sampler2D u_screen_texture;
    uniform float u_desat;
    out vec4 frag_color;
    void main() {
        vec4 c = texture(u_screen_texture, v_uv);
        float lum = dot(c.rgb, vec3(0.299, 0.587, 0.114));
        frag_color = vec4(mix(c.rgb, vec3(lum), u_desat), c.a);
    }
    ]])

    crayon.graphics.pushEffect(gray, "gray")
end

function crayon.update(dt)
    crayon.graphics.setEffectUniform("gray", "u_desat", 1.0)
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.15)

    crayon.graphics.setCamera3D({
        position = { 3, 3, -6 },
        target   = { 1, 1, 1 },
        up       = { 0, 1, 0 },
        fov      = 60, near = 0.1, far = 100,
    })
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawCube(1, 1, 1, 1, 1, 1, crate_tex, 0, 0, 0)

    crayon.graphics.setColor(1, 0.4, 0.4, 1)
    crayon.graphics.drawRect("fill", 50, 50, 100, 100)

    crayon.graphics.setColor(0.4, 1, 0.4, 1)
    crayon.graphics.drawRect("fill", 400, 50, 100, 100)
end
