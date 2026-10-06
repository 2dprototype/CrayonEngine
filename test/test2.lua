local canvas, shader

function crayon.init()
    crayon.window.setResolution(640, 360)
    canvas = crayon.graphics.createCanvas(320, 240)
    shader = crayon.graphics.loadShader([[
        #version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec2 aUV;
        out vec2 uv;
        uniform mat4 uProjection;
        uniform mat4 uView;
        void main() {
            uv = aUV;
            gl_Position = uProjection * uView * vec4(aPos, 1.0);
        }
    ]], [[
        #version 330 core
        in vec2 uv;
        out vec4 fragColor;
        void main() {
            fragColor = vec4(uv, 0.5, 1.0);
        }
    ]])
end

function crayon.draw()
    crayon.graphics.clear(0, 0, 0)
    canvas:renderTo(function()
        canvas:clear(0.2, 0.0, 0.4, 1.0)
        crayon.graphics.setColor(1, 1, 1)
        crayon.graphics.drawText("On Canvas", 10, 10, 2)
    end)
    crayon.graphics.drawSprite(canvas:getTexture(), 0, 0, 640, 480)
end