-- Crayon Engine 3D Scene Loader
-- Loads map scenes exported from the Crayon 3D Scene Editor into any Crayon game.

local scene_loader = {}

function scene_loader.load(filepath)
    -- Normalize path
    local chunk, err = loadfile(filepath)
    if not chunk then
        print("[scene_loader ERROR] Failed to load scene file: " .. tostring(filepath) .. "\n" .. tostring(err))
        return nil
    end

    local data = chunk()
    if not data or not data.objects then
        print("[scene_loader ERROR] Invalid scene format in: " .. tostring(filepath))
        return nil
    end

    local scene = {
        name = data.name or "Untitled Scene",
        objects = data.objects or {},
        models = {},
        physics_bodies = {}
    }

    -- Cache loaded models and setup physics if physics is available
    for _, obj in ipairs(scene.objects) do
        if obj.type == "model" and obj.asset and obj.asset ~= "" then
            if not scene.models[obj.asset] then
                scene.models[obj.asset] = crayon.graphics.load_model(obj.asset)
            end
        end

        -- Auto-setup collision if physics is enabled on this object
        if obj.has_collider and crayon.physics3d then
            local pos = obj.pos or {0, 0, 0}
            local scale = obj.scale or {1, 1, 1}
            local motion = (obj.collider_motion == "dynamic") and 1 or 0

            if obj.collider_type == "sphere" then
                local radius = (scale[1] or 1) * 0.5
                obj.physics_body = crayon.physics3d.create_sphere(pos[1], pos[2], pos[3], radius, motion)
            else
                -- Default to box
                local hx = (scale[1] or 1) * 0.5
                local hy = (scale[2] or 1) * 0.5
                local hz = (scale[3] or 1) * 0.5
                obj.physics_body = crayon.physics3d.create_box(pos[1], pos[2], pos[3], hx, hy, hz, motion)
            end
        end
    end

    -- Draw all 3D objects in the scene
    function scene:draw()
        for _, obj in ipairs(self.objects) do
            if obj.visible ~= false then
                local pos = obj.pos or {0, 0, 0}
                local rot = obj.rot or {0, 0, 0}
                local scale = obj.scale or {1, 1, 1}
                local col = obj.color or {1, 1, 1, 1}

                crayon.graphics.push_matrix()
                crayon.graphics.translate(pos[1], pos[2], pos[3])
                if rot[1] ~= 0 then crayon.graphics.rotate(rot[1], 1, 0, 0) end
                if rot[2] ~= 0 then crayon.graphics.rotate(rot[2], 0, 1, 0) end
                if rot[3] ~= 0 then crayon.graphics.rotate(rot[3], 0, 0, 1) end
                crayon.graphics.scale(scale[1], scale[2], scale[3])

                crayon.graphics.set_color(col[1], col[2], col[3], col[4] or 1.0)

                if obj.type == "model" and obj.asset and self.models[obj.asset] then
                    crayon.graphics.draw_model(self.models[obj.asset], 0, 0, 0)
                elseif obj.type == "cube" then
                    crayon.graphics.draw_cube(1.0)
                elseif obj.type == "plane" then
                    crayon.graphics.draw_plane(1.0, 1.0)
                elseif obj.type == "sphere" then
                    crayon.graphics.draw_sphere(0.5)
                elseif obj.type == "cylinder" then
                    crayon.graphics.draw_cylinder(0.5, 1.0)
                end

                crayon.graphics.pop_matrix()
            end
        end
    end

    function scene:get_object(name)
        for _, obj in ipairs(self.objects) do
            if obj.name == name then
                return obj
            end
        end
        return nil
    end

    return scene
end

return scene_loader
