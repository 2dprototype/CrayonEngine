function crayon.init()
	crayon.physics4D.createArena(6.0)                                   -- floor + walls (x, z and w)

	box  = crayon.physics4D.createTesseract(0, 6, 0, 0.8)         -- 4D box, slightly off the w = 0 slice
	ball = crayon.physics4D.createHypersphere(2, 5, 0, 0, 2)
	-- ball:setLinearVelocity(-3, 2, 0, 0)
end

function crayon.update(dt)
    crayon.physics4D.setSlice(math.sin(crayon.time.getTime()) * 1.5) -- sweep the slice through the objects
    box:rotateBy(0, 0, 0.01, 0, 0, 0)                 -- spin in the xw plane (impossible in 3D)
end

function crayon.draw()
    crayon.graphics.clear(0.05, 0.06, 0.1)
    crayon.graphics.setCamera3D({ position = {0, 6, 12}, target = {0, 1, 0}, up = {0, 1, 0}, fov = 50 })
	crayon.physics4D.drawDebug({
		shapes = true,       -- wireframe of each body's current 3D cross-section
		fill = true,        -- lit, filled cross-section surfaces
		projection = true,  -- ghost wireframe of the *whole* 4D body, fading with distance from the slice
		contacts = true,     -- contact points + normals (projected into the slice)
		bounds = true,      -- bounding hyperspheres
		velocities = true,  -- velocity arrows (a +/- cross marks motion along the slice normal, i.e. w)
		planes = true,       -- grid patches for half-spaces
		color = {0.2, 1, 0.4, 1}
	})
end