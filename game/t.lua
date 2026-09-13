local cam = {
    x = 0.0, y = 5.0, z = 14.0*2,
    yaw = -90.0, pitch = -15.0,
    fov = 60.0
}

local b1, b2, cube

function crayon.init()
    b1 = crayon.physics3d.createBox(0, -20, 0, 40, 0.1, 40, "static", 0.5, 0.2, 10)
    -- b2 = crayon.physics3d.createBox(0, 20, 0, 1, 1, 1, "dynamic", 0.5, 0.2, 10)
    
    cube = crayon.graphics.loadModel("cube")
	
    ball = crayon.physics3d.createSoftBodySphere({
        x = 0, y = 0, z = 10,
        radius = 10,
        rings = 8,
        sectors = 10,
        compliance = 0,
        pressure = 10
    })
end

function crayon.update(dt)
    local move_speed = 7.0 * dt
    local rad_yaw = math.rad(cam.yaw)
    local fwd_x, fwd_z = math.cos(rad_yaw), math.sin(rad_yaw)
    local right_x, right_z = -fwd_z, fwd_x

    if crayon.input.isDown("w") then cam.x = cam.x + fwd_x * move_speed; cam.z = cam.z + fwd_z * move_speed end
    if crayon.input.isDown("s") then cam.x = cam.x - fwd_x * move_speed; cam.z = cam.z - fwd_z * move_speed end
    if crayon.input.isDown("a") then cam.x = cam.x - right_x * move_speed; cam.z = cam.z - right_z * move_speed end
    if crayon.input.isDown("d") then cam.x = cam.x + right_x * move_speed; cam.z = cam.z + right_z * move_speed end
    if crayon.input.isDown("q") or crayon.input.isDown("space") then cam.y = cam.y + move_speed end
    if crayon.input.isDown("z") or crayon.input.isDown("lshift") then cam.y = cam.y - move_speed end

    if crayon.input.isDown("left") then cam.yaw = cam.yaw - 80.0 * dt end
    if crayon.input.isDown("right") then cam.yaw = cam.yaw + 80.0 * dt end
    if crayon.input.isDown("up") then cam.pitch = math.min(cam.pitch + 60.0 * dt, 80.0) end
    if crayon.input.isDown("down") then cam.pitch = math.max(cam.pitch - 60.0 * dt, -80.0) end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.25)
    crayon.graphics.setColor(0, 1, 0, 0.5)
    crayon.graphics.drawText("Physics Test (Love2D Userdata API)", 6, 6, 1.0)
    
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    local target_x = cam.x + math.cos(rad_pitch) * math.cos(rad_yaw)
    local target_y = cam.y + math.sin(rad_pitch)
    local target_z = cam.z + math.cos(rad_pitch) * math.sin(rad_yaw)

    crayon.graphics.setCamera3d({
        position = {cam.x, cam.y, cam.z},
        target = {target_x, target_y, target_z},
        up = {0, 1, 0},
        fov = cam.fov
    })
	
    -- Draw the soft body as a triangle mesh
	crayon.graphics.setColor(1, 1, 1, 1)
    local verts = ball:getVertices()
    local faces = ball:getFaces()
    for _, f in ipairs(faces) do
        crayon.graphics.drawTriangle3d(verts[f[1]], verts[f[2]], verts[f[3]])
    end
    
    -- if b2:isValid() then
        -- local px, py, pz = b2:getPosition()
        -- local rx, ry, rz = b2:getRotation()
        -- crayon.graphics.drawModel(cube, px, py, pz, math.rad(rx), math.rad(ry), math.rad(rz), 2, 2, 2)
    -- end
    
    crayon.physics3d.drawDebug()
end
