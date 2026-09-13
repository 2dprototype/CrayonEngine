local cam = {
    x = 0.0, y = 5.0, z = 14.0,
    yaw = -90.0, pitch = -15.0,
    fov = 60.0
}

local b1, b2, cube, human, anim

function crayon.init()
    crayon.graphics.setRetroEffects({
        affine = 0
    })
	
    b1 = crayon.physics3d.createBox(0, -5, 0, 5, 0.1, 5, "static", 0.5, 0.2, 10)
    b2 = crayon.physics3d.createBox(0, 2, 0, 1, 1, 1, "dynamic", 0.5, 0.2, 10)

    cube = crayon.graphics.loadModel("cube")

    human = crayon.graphics.loadModel("models/Soldier.glb")
    anim  = crayon.graphics.createAnimator(human)

    print(human:isValid(), human:isSkinned(),
          table.concat(human:getAnimationNames(), ", "))

    anim:play("Run")
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
	
    anim:update(dt)
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.25)
    crayon.graphics.setColor(0, 1, 0, 0.5)
    crayon.graphics.drawText("Skinned Animation Test", 6, 6, 1.0)

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

    -- Skinned draw: applies the animator's bone palette
    crayon.graphics.drawModelSkinned(human, anim, 0, 0, 0, 0, 0, 0, 4, 4, 4)

    crayon.physics3d.drawDebug()
end