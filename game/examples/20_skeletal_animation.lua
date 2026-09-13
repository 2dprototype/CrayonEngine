-- =====================================================================
-- Example 20: Skeletal Animation Playback & Skinning Pipeline
-- Crayon Engine
-- Demonstrates:
--  - Loading and inspecting rigged/skinned glTF humanoid characters
--  - Skeletal Animation Playback with Graphics.Animator
--  - Smooth CrossFading (Idle <-> Walk <-> Run)
--  - 1D Locomotion Blend Tree (phase-synchronized Walk/Run blending)
--  - Multi-Layer Bone Masking (aiming/shooting upper-body while legs walk)
--  - Bidirectional Physics Ragdoll Integration (animator -> pose & pose -> animator)
--  - Retro shaders (dithering, affine texture mapping, distance fog) on skinned meshes
-- =====================================================================

local cam = {
    x = 0.0, y = 3.5, z = 7.0,
    target_x = 0.0, target_y = 1.6, target_z = 0.0,
    yaw = -90.0, pitch = -15.0,
    dist = 6.5
}

-- Character & Animation state
local character_model = nil
local animator = nil
local is_skinned_model = false
local anim_names = {}
local current_anim_idx = 1
local playback_speed = 1.0

-- Locomotion Blend Tree state
local blend_mode = false
local blend_factor = 0.0 -- 0.0 = walk, 1.0 = run

-- Upper-body Masking Layer state
local upper_body_mask_enabled = false
local upper_body_weight = 1.0

-- Ragdoll & Physics State
local physics_ragdoll_active = false
local character_skeleton = nil
local character_pose = nil
local character_ragdoll = nil

-- Visuals
local light_rot = 0.0
local retro_enabled = true
local ground_tex = nil

-- Procedural fallback skeleton and mesh if no glTF model file exists
local fallback_mode = false
local fallback_bones = {}

function crayon.init()
    crayon.window.setResolution(320, 240)
    crayon.window.setTitle("Crayon Engine - 3D Skeletal Animation & Ragdoll Showcase")

    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.setShadingMode("gouraud")
    crayon.graphics.setLight(-0.6, -1.0, 0.4, 1.0, 0.95, 0.9, 0.2, 0.2, 0.25)

    -- Try loading a glTF humanoid character (replace path with your rigged .glb / .gltf)
    local sample_paths = {
        "models/Soldier.glb",
    }

    for _, path in ipairs(sample_paths) do
        local m = crayon.graphics.loadModel(path)
        if m and m:isValid() then
            character_model = m
            if m:isSkinned() then
                is_skinned_model = true
                print("[SkeletalDemo] Successfully loaded skinned glTF model: " .. path)
                break
            end
        end
    end

    if character_model and is_skinned_model then
        -- 1. Initialize Animator
        animator = character_model:createAnimator()
        anim_names = character_model:getAnimationNames()
        print("[SkeletalDemo] Found " .. #anim_names .. " animations: " .. table.concat(anim_names, ", "))

        if #anim_names > 0 then
            animator:play(anim_names[1], true, 1.0)
        end

        -- 2. Setup upper-body layer mask (Layer 1)
        -- Joint "Spine" or "Chest" can be used as root joint for the mask
        local spine_joint = "Spine"
        if character_model:getJointIndex("Spine1") >= 0 then
            spine_joint = "Spine1"
        elseif character_model:getJointIndex("mixamorig:Spine") >= 0 then
            spine_joint = "mixamorig:Spine"
        end

        animator:setLayerMask(1, spine_joint, true)

        -- 3. Create physics skeleton directly from model joints
        character_skeleton = character_model:createPhysicsSkeleton()
        if character_skeleton and character_skeleton:isValid() then
            character_pose = crayon.physics3d.createSkeletonPose(character_skeleton)
        end
    else
        print("[SkeletalDemo] No rigged glTF model found; running in procedural skinned demonstration mode.")
        fallback_mode = true
    end

    -- Physics ground plane
    crayon.physics3d.createPlane(0, 1, 0, 0, 0.6, 0.2)
end

function crayon.update(dt)
    -- Camera Orbit Controls
    if crayon.input.isDown("mouse_right") or crayon.input.isDown("mouse_middle") then
        local dx, dy = crayon.input.getMouseDelta()
        cam.yaw = cam.yaw + dx * 0.3
        cam.pitch = math.max(-85.0, math.min(85.0, cam.pitch - dy * 0.3))
    end

    local scroll = crayon.input.getMouseWheel()
    if scroll ~= 0 then
        cam.dist = math.max(2.0, math.min(30.0, cam.dist - scroll * 0.75))
    end

    -- Update Camera Position
    local rad_yaw = math.rad(cam.yaw)
    local rad_pitch = math.rad(cam.pitch)
    cam.x = cam.target_x + cam.dist * math.cos(rad_pitch) * math.cos(rad_yaw)
    cam.y = cam.target_y + cam.dist * math.sin(rad_pitch)
    cam.z = cam.target_z + cam.dist * math.cos(rad_pitch) * math.sin(rad_yaw)

    -- Toggle Retro Shader Effects
    if crayon.input.isPressed("f1") then
        retro_enabled = not retro_enabled
    end

    -- Toggle Locomotion Blend Tree (Walk <-> Run)
    if crayon.input.isPressed("b") then
        blend_mode = not blend_mode
        print("[SkeletalDemo] Locomotion Blend Tree: " .. (blend_mode and "ACTIVE" or "OFF"))
    end

    -- Toggle Upper-Body Bone Masking (Aim/Shoot over locomotion)
    if crayon.input.isPressed("m") then
        upper_body_mask_enabled = not upper_body_mask_enabled
        print("[SkeletalDemo] Upper-Body Layer Masking: " .. (upper_body_mask_enabled and "ACTIVE" or "OFF"))
    end

    -- Switch Animation Clip via [1..9]
    if not blend_mode and #anim_names > 0 then
        for i = 1, math.min(9, #anim_names) do
            if crayon.input.isPressed(tostring(i)) then
                current_anim_idx = i
                animator:crossFade(anim_names[current_anim_idx], 0.25, true)
                print("[SkeletalDemo] CrossFading to clip: " .. anim_names[current_anim_idx])
            end
        end
    end

    -- Adjust Locomotion Blend Factor (Left/Right Arrows)
    if blend_mode then
        if crayon.input.isDown("left") then
            blend_factor = math.max(0.0, blend_factor - dt * 1.5)
        elseif crayon.input.isDown("right") then
            blend_factor = math.min(1.0, blend_factor + dt * 1.5)
        end

        if #anim_names >= 2 and animator then
            animator:blend(anim_names[1], anim_names[2], blend_factor)
        end
    end

    -- Layer 1 Upper-Body action clip
    if animator and upper_body_mask_enabled and #anim_names >= 3 then
        animator:setLayerClip(1, anim_names[3], true, 1.0)
        animator:setLayerWeight(1, 1.0)
    elseif animator then
        animator:setLayerWeight(1, 0.0)
    end

    -- Toggle Physics Ragdoll Mode (SPACE)
    if crayon.input.isPressed("space") and character_pose and animator then
        physics_ragdoll_active = not physics_ragdoll_active
        if physics_ragdoll_active then
            -- 1. Transfer current animated pose to physics SkeletonPose
            animator:applyToPhysicsPose(character_pose)
            print("[SkeletalDemo] Animated pose captured into Physics SkeletonPose! Ragdoll dynamic.")
        else
            -- 2. Transfer ragdoll pose back to animator for seamless stand-up
            animator:capturePhysicsPose(character_pose)
            print("[SkeletalDemo] Ragdoll captured back into Animator. Resuming animation playback.")
        end
    end

    -- Update Animator (Engine automatically steps physics at fixed 60Hz in game loop)
    if animator and not physics_ragdoll_active then
        animator:update(dt)
    end

    -- Rotate lighting for atmospheric retro look
    light_rot = light_rot + dt * 0.4
    crayon.graphics.setLight(math.cos(light_rot), -1.2, math.sin(light_rot), 1.0, 0.92, 0.85, 0.2, 0.2, 0.25)
end

function crayon.draw()
    -- Apply Retro Effects
    if retro_enabled then
        crayon.graphics.setRetroEffects({
            jitterResolution = {320, 240},
            affine = 0.8,
            dither = true,
            colorDepth = 32,
            fog = {
                startDist = 8.0,
                endDist = 28.0,
                color = {0.12, 0.13, 0.18}
            }
        })
    else
        crayon.graphics.setRetroEffects({})
    end

    crayon.graphics.clear(0.12, 0.13, 0.18, 1.0)

    -- Setup 3D Camera
    crayon.graphics.setCamera3d({
        position = {cam.x, cam.y, cam.z},
        target = {cam.target_x, cam.target_y, cam.target_z},
        up = {0, 1, 0},
        fov = 55.0,
        near = 0.1,
        far = 100.0
    })

    -- Draw Ground Grid & Arena Floor
    crayon.graphics.setColor(0.2, 0.25, 0.35, 1.0)
    crayon.graphics.drawGrid3d(24.0, 24, 0.0)

    -- Draw Skinned Character Model
    if character_model and is_skinned_model then
        crayon.graphics.setColor(1, 1, 1, 1)

        if physics_ragdoll_active and character_pose then
            -- Render skinned character driven by physics SkeletonPose
            character_model:drawSkinned(character_pose, 0, 0, 0, 0, 0, 0, 1, 1, 1)
        elseif animator then
            -- Render skinned character driven by Graphics.Animator (blend tree + layers)
            character_model:drawSkinned(animator, 0, 0, 0, 0, 0, 0, 1, 1, 1)
        end
    else
        -- Fallback demonstration visualizer: Procedural articulated mannequin
        local t = crayon.time.getTime()
        local leg_swing = math.sin(t * 5.0) * 0.45
        local arm_swing = math.cos(t * 5.0) * 0.45

        crayon.graphics.setColor(0.3, 0.6, 0.9, 1.0)
        -- Torso
        crayon.graphics.drawCube(0, 1.6, 0, 0.6, 0.8, 0.4)
        -- Head
        crayon.graphics.setColor(0.95, 0.8, 0.65, 1.0)
        crayon.graphics.drawSphere(0, 2.3, 0, 0.3)
        -- Limbs
        crayon.graphics.setColor(0.2, 0.4, 0.7, 1.0)
        crayon.graphics.drawCylinder(-0.25, 0.6, leg_swing * 0.4, 0.12, 0.8, nil, leg_swing * 40, 0, 0)
        crayon.graphics.drawCylinder(0.25, 0.6, -leg_swing * 0.4, 0.12, 0.8, nil, -leg_swing * 40, 0, 0)
        crayon.graphics.drawCylinder(-0.45, 1.5, arm_swing * 0.3, 0.1, 0.6, nil, arm_swing * 45, 0, 0)
        crayon.graphics.drawCylinder(0.45, 1.5, -arm_swing * 0.3, 0.1, 0.6, nil, -arm_swing * 45, 0, 0)
    end

    -- Draw UI Overlay
    crayon.graphics.resetCamera2d()
    crayon.graphics.setColor(1, 1, 1, 1)
    crayon.graphics.drawText("=== CRAYON ENGINE - 3D SKELETAL ANIMATION PIPELINE ===", 15, 15, 1)
    crayon.graphics.drawText("FPS: " .. tostring(crayon.window.getFps()), 15, 30, 1)

    if character_model and is_skinned_model then
        crayon.graphics.drawText("Status: Loaded Skinned glTF Model (" .. character_model:getJointCount() .. " joints)", 15, 50, 1)
        crayon.graphics.drawText("[1.." .. #anim_names .. "] CrossFade Clip (Current: " .. (anim_names[current_anim_idx] or "None") .. ")", 15, 65, 1)
        crayon.graphics.drawText("[B] Blend Tree: " .. (blend_mode and string.format("ON (Factor: %.2f)", blend_factor) or "OFF") .. " (Left/Right Arrow)", 15, 80, 1)
        crayon.graphics.drawText("[M] Upper-Body Masking: " .. (upper_body_mask_enabled and "ENABLED" or "DISABLED"), 15, 95, 1)
        crayon.graphics.drawText("[SPACE] Toggle Ragdoll Physics Pose: " .. (physics_ragdoll_active and "ACTIVE" or "ANIMATED"), 15, 110, 1)
    else
        crayon.graphics.drawText("Status: Procedural Animation Demo Mode (place rigged glTF in game/assets/models/)", 15, 50, 1)
    end

    crayon.graphics.drawText("[F1] Retro Shader Effects: " .. (retro_enabled and "ON" or "OFF"), 15, 130, 1)
    crayon.graphics.drawText("Controls: Right/Middle Drag = Orbit Cam | Wheel = Zoom", 15, 145, 1)
end
