
local r = 1

function crayon.init()
	print("hello")
	cube_model = crayon.graphics.load_model("cylinder")
end

function crayon.update()
	r = r + 1
end

function crayon.draw()
	crayon.graphics.clear(0.08, 0.09, 0.14)
	crayon.graphics.set_color(1.0, 0.85, 0.2, 0.5)
	crayon.graphics.draw_text("Hello", 6, 6, 1.0)
	
	crayon.graphics.rotate(r * 1.5, 0, 1, 0)
	crayon.graphics.draw_model(cube_model, 0, 0, 0, 0, 0, 0, 1.2, 1.2, 1.2, nil)
end