
local r = 1

function crayon.init()
	print("hello")
	cube_model = crayon.graphics.load_model("torus")
	txt = crayon.graphics.load_texture("game/assets/textures/grass.bmp")
end

function crayon.update()
	r = r + 1
end

function crayon.draw()
	crayon.graphics.clear(0.1, 0.1, 0.5)
	crayon.graphics.set_color(0, 1, 0, 0.5)
	crayon.graphics.draw_text("Hello world", 6, 6, 1.0)
	
	crayon.graphics.rotate(r * 0.01, r*0.1, 1, 0)
	crayon.graphics.draw_model(cube_model, 0, 0, 0, 0, 0, 0, 3, 3, 3, txt)
end

function crayon.mouse_down()
	print(10)
	
end