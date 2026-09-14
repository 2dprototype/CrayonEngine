-- Crayon 3D Scene File
return {
    name = "Default Map",
    objects = {
        {
            name = "Ground",
            type = "plane",
            pos = { 0.00, 0.00, 0.00 },
            rot = { 0.00, 0.00, 0.00 },
            scale = { 20.00, 1.00, 20.00 },
            color = { 0.30, 0.45, 0.30, 1.00 },
            has_collider = true,
            collider_type = "box",
            collider_motion = "static",
            visible = true
        },
        {
            name = "Pillar_Left",
            type = "cube",
            pos = { -4.00, 1.50, -3.00 },
            rot = { 0.00, 0.00, 0.00 },
            scale = { 1.00, 3.00, 1.00 },
            color = { 0.60, 0.60, 0.65, 1.00 },
            has_collider = true,
            collider_type = "box",
            collider_motion = "static",
            visible = true
        },
        {
            name = "Pillar_Right",
            type = "cube",
            pos = { 4.00, 1.50, -3.00 },
            rot = { 0.00, 0.00, 0.00 },
            scale = { 1.00, 3.00, 1.00 },
            color = { 0.60, 0.60, 0.65, 1.00 },
            has_collider = true,
            collider_type = "box",
            collider_motion = "static",
            visible = true
        },
        {
            name = "Center_Altar",
            type = "cylinder",
            pos = { 0.00, 0.50, 0.00 },
            rot = { 0.00, 0.00, 0.00 },
            scale = { 2.00, 1.00, 2.00 },
            color = { 0.85, 0.55, 0.25, 1.00 },
            has_collider = true,
            collider_type = "sphere",
            collider_motion = "static",
            visible = true
        }
    }
}
