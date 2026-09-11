local cam = {
    x = 0.0, y = 5.0, z = 14.0,
    yaw = -90.0, pitch = -15.0,
    fov = 60.0
}

function crayon.init()
	b1 = crayon.physics.create_box(0, -5, 0, 5, 0.1, 5, "static", 0.5, 0.2, 10)
	b2 = crayon.physics.create_box(0, 2, 0, 1, 1, 1, "dynamic", 0.5, 0.2, 10)
	
	cube = crayon.graphics.load_model("cube")

end

function crayon.update(dt)
    local move_speed = 7.0 * dt
    local rad_yaw = math.rad(cam.yaw)
    local fwd_x, fwd_z = math.cos(rad_yaw), math.sin(rad_yaw)
    local right_x, right_z = -fwd_z, fwd_x

    if crayon.input.is_down("w") then cam.x = cam.x + fwd_x * move_speed; cam.z = cam.z + fwd_z * move_speed end
    if crayon.input.is_down("s") then cam.x = cam.x - fwd_x * move_speed; cam.z = cam.z - fwd_z * move_speed end
    if crayon.input.is_down("a") then cam.x = cam.x - right_x * move_speed; cam.z = cam.z - right_z * move_speed end
    if crayon.input.is_down("d") then cam.x = cam.x + right_x * move_speed; cam.z = cam.z + right_z * move_speed end
    if crayon.input.is_down("q") or crayon.input.is_down("space") then cam.y = cam.y + move_speed end
    if crayon.input.is_down("z") or crayon.input.is_down("lshift") then cam.y = cam.y - move_speed end

    if crayon.input.is_down("left") then cam.yaw = cam.yaw - 80.0 * dt end
    if crayon.input.is_down("right") then cam.yaw = cam.yaw + 80.0 * dt end
    if crayon.input.is_down("up") then cam.pitch = math.min(cam.pitch + 60.0 * dt, 80.0) end
    if crayon.input.is_down("down") then cam.pitch = math.max(cam.pitch - 60.0 * dt, -80.0) end
end

function crayon.draw()
	crayon.graphics.clear(0.1, 0.1, 0.25)
	crayon.graphics.set_color(0, 1, 0, 0.5)
	crayon.graphics.draw_text("Physics Test", 6, 6, 1.0)
	
    crayon.graphics.set_camera3d({
        position = {cam.x, cam.y, cam.z},
        target = {target_x, target_y, target_z},
        up = {0, 1, 0},
        fov = cam.fov
    })
	
	local px, py, pz = crayon.physics.get_position(b2)
	local rx, ry, rz = crayon.physics.get_rotation(b2)
	crayon.graphics.draw_model(cube, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), 2, 2, 2, nil)
	
	
	crayon.physics.draw_debug(0.3, 1.0, 0.4, 1.0, 0.6, 0.6, 0.8, 0.8)
end
