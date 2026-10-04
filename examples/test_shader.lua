local shader

function crayon.config(t)
    t.window.width = 640
    t.window.height = 360
end

function crayon.init()
    crayon.window.setResolution(640, 360)

    shader = crayon.graphics.loadShader([[
        #version 330 core
        in vec2 v_uv;
        in vec4 v_color;

        uniform sampler2D u_texture;

        out vec4 frag_color;

        void main() {
            vec4 col = texture(u_texture, v_uv) * v_color;
            float lum = dot(col.rgb, vec3(0.299, 0.587, 0.114));
            frag_color = vec4(vec3(lum), col.a);
        }
    ]])
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.15)

    crayon.graphics.setShader(shader)

    -- Everything below is drawn in grayscale
    crayon.graphics.setColor(1, 0.4, 0.4, 1)
    crayon.graphics.drawRect("fill", 50, 50, 100, 100)
    crayon.graphics.drawCircle("fill", 300, 150, 60)


    crayon.graphics.setShader(nil)   -- restore default
    -- This one is drawn normally (colored)
    crayon.graphics.setColor(0.4, 1, 0.4, 1)
    crayon.graphics.drawRect("fill", 400, 50, 100, 100)
end