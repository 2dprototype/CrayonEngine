# Crayon Engine

A lightweight 2D/3D game engine with retro aesthetics, modern physics, and Lua scripting. Built with C++20, OpenGL 3.3, SDL3, and Jolt Physics.

## Features

- **Full Lua Scripting** - Complete engine API exposed to LuaJIT
- **2D & 3D Rendering** - Batched 2D sprites + 3D mesh rendering
- **Virtual Resolution** - Pixel-perfect scaling with 4 modes
- **Retro Effects** - Jitter, affine mapping, dithering, CRT scanlines, curvature, vignette, distance fog
- **3D Lighting** - Directional + 4 point lights with falloff
- **Shading Modes** - Gouraud, Flat, Unlit
- **Physics** - Jolt Physics 3D with rigid bodies, constraints, raycasting, sensors
- **Input** - Keyboard, mouse, gamepad, text input, clipboard
- **Hot Reload** - Instant script reload on file change or F5
- **Asset Loading** - PNG/JPG textures, OBJ models, procedural meshes

## Quick Start

### Command Line
```bash
crayon_engine.exe game/main.lua
```

### Basic Game Script
```lua
function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_title("My Game")
end

function crayon.update(dt)
    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    crayon.graphics.set_color(1, 1, 1, 1)
    crayon.graphics.draw_text("Hello World!", 10, 10, 2)
end
```

## Graphics API Examples

### 2D Rendering
```lua
-- Sprites
local tex = crayon.graphics.load_texture("player.png")
crayon.graphics.draw_sprite(tex, x, y, 32, 32, angle, 16, 16)

-- Shapes
crayon.graphics.set_color(1, 0, 0, 1)
crayon.graphics.draw_rect("fill", 10, 10, 50, 50)
crayon.graphics.draw_circle("fill", 100, 100, 30)

-- Text
crayon.graphics.draw_text("Score: " .. score, 10, 10, 2)

-- 2D Camera
crayon.graphics.set_camera2d({x = 0, y = 0, zoom = 1.0, angle = 0})

-- Transforms
crayon.graphics.push_matrix_2d()
crayon.graphics.translate_2d(100, 100)
crayon.graphics.rotate_2d(1.57)
crayon.graphics.draw_sprite(tex, 0, 0, 32, 32)
crayon.graphics.pop_matrix_2d()
```

### 3D Rendering
```lua
-- 3D Camera
crayon.graphics.set_camera3d({
    position = {0, 3, 8},
    target = {0, 0, 0},
    fov = 60,
    near = 0.1,
    far = 1000.0
})

-- Lighting
crayon.graphics.set_light(-0.5, -1, -0.7, 1, 0.95, 0.9, 0.25, 0.25, 0.3)
crayon.graphics.set_point_light(0, 0, 5, 0, 1, 1, 1, 10, 1)
crayon.graphics.set_shading_mode("gouraud")

-- 3D Objects
crayon.graphics.draw_cube(x, y, z, 1, 1, 1, tex, rx, ry, rz)
crayon.graphics.draw_sphere(x, y, z, 0.5, tex)
crayon.graphics.draw_billboard(x, y, z, 1, 1, tex, "cylindrical")

-- Models
local model = crayon.graphics.load_model("cube")  -- or "sphere", "torus", or .obj path
crayon.graphics.draw_model(model, x, y, z, rx, ry, rz, 1, 1, 1, tex)

-- 3D Transforms
crayon.graphics.push_matrix()
crayon.graphics.translate(0, 2, 0)
crayon.graphics.rotate(t, 0, 1, 0)
crayon.graphics.scale(1.5, 1.5, 1.5)
crayon.graphics.draw_sphere(0, 0, 0, 0.5)
crayon.graphics.pop_matrix()

-- Lines & Debug
crayon.graphics.draw_line_3d(x1, y1, z1, x2, y2, z2)
crayon.graphics.draw_grid_3d(20, 20, 0)
crayon.graphics.draw_axes_3d(0, 0, 0, 1)
```

### Retro Effects
```lua
crayon.graphics.set_retro_effects({
    jitter_resolution = {160, 120},  -- Pixel snap resolution
    affine = 1.0,                    -- 0=perspective, 1=affine
    dither = true,
    color_depth = 32,                -- 8 or 32
    fog = {
        start = 5.0,
        end = 25.0,
        color = {0.1, 0.1, 0.2}
    },
    crt = {
        scanlines = 0.25,
        curvature = 0.05,
        vignette = 0.25
    }
})
```

## Physics API Examples

### Bodies
```lua
-- Create bodies
local box = crayon.physics.create_box(0, 2, 0, 0.5, 0.5, 0.5, "dynamic", 0.5, 0.2)
local sphere = crayon.physics.create_sphere(0, 5, 0, 0.5, "dynamic", 0.5, 0.5)
local floor = crayon.physics.create_plane(0, -0.5, 0, 0, 1, 0, 50)

-- Motion types: "static", "kinematic", "dynamic"
crayon.physics.set_motion_type(box, "kinematic")

-- Transforms
x, y, z = crayon.physics.get_position(box)
crayon.physics.set_position(box, 0, 3, 0, true)
rx, ry, rz = crayon.physics.get_rotation(box)
crayon.physics.set_rotation(box, 0, 45, 0, true)

-- Dynamics
vx, vy, vz = crayon.physics.get_velocity(box)
crayon.physics.set_velocity(box, 5, 0, 0)
crayon.physics.apply_force(box, 0, -9.81, 0)
crayon.physics.apply_impulse(box, 0, 5, 0)

-- Properties
crayon.physics.set_friction(box, 0.8)
crayon.physics.set_restitution(box, 0.3)
crayon.physics.set_gravity_factor(box, 1.0)
crayon.physics.set_damping(box, 0.1, 0.1)
```

### Constraints
```lua
-- Point constraint
local c1 = crayon.physics.create_point_constraint(body1, body2, pivot_x, pivot_y, pivot_z)

-- Hinge constraint
local c2 = crayon.physics.create_hinge_constraint(
    body1, body2,
    px, py, pz,  -- Pivot
    ax, ay, az,  -- Axis
    -1.57, 1.57  -- Min/max angle (radians)
)

-- Distance constraint
local c3 = crayon.physics.create_distance_constraint(
    body1, body2,
    p1x, p1y, p1z,
    p2x, p2y, p2z,
    min_dist, max_dist
)

-- Fixed constraint
local c4 = crayon.physics.create_fixed_constraint(body1, body2)

-- Destroy
crayon.physics.destroy_constraint(c1)
```

### Queries
```lua
-- Raycast
local hit, hx, hy, hz, nx, ny, nz, dist, id = crayon.physics.raycast(
    ox, oy, oz,  -- Origin
    dx, dy, dz,  -- Direction
    max_dist
)

-- Overlap sphere
local hits = crayon.physics.overlap_sphere(cx, cy, cz, radius)
for _, id in ipairs(hits) do
    print("Body in sphere:", id)
end

-- Debug visualization
crayon.physics.draw_debug(0.2, 1, 0.4, 1, 0.5, 0.5, 0.5, 1)
```

## Input API Examples

```lua
-- Keyboard
if crayon.input.is_down("w") then
    -- Move forward
end
if crayon.input.is_pressed("space") then
    -- Jump!
end
if crayon.input.is_released("shift") then
    -- Stop sprinting
end

-- Modifiers
if crayon.input.is_shift_down() and crayon.input.is_pressed("s") then
    print("Shift+S pressed!")
end

-- Mouse
local mx, my = crayon.input.get_mouse_pos()
local mdx, mdy = crayon.input.get_mouse_delta()
if crayon.input.is_mouse_pressed("left") then
    print("Left click at:", mx, my)
end
local wx, wy = crayon.input.get_mouse_wheel()

-- Text Input
crayon.input.start_text_input()
local text = crayon.input.get_text_input()
crayon.input.stop_text_input()

-- Clipboard
local clip = crayon.input.get_clipboard()
crayon.input.set_clipboard("Hello!")

-- Gamepad
if crayon.input.gamepad_is_down(0) then  -- A button
    print("A button held!")
end
local lx = crayon.input.gamepad_axis(0)   -- Left stick X
local ly = crayon.input.gamepad_axis(1)   -- Left stick Y
```

## Window API Examples

```lua
-- Basic setup
crayon.window.set_resolution(320, 240)
crayon.window.set_window_size(960, 720)
crayon.window.set_title("My Game")
crayon.window.set_vsync(true)

-- Scaling modes
crayon.window.set_scaling_mode("integer")  -- Pixel-perfect
crayon.window.set_scaling_mode("aspect")   -- Letterbox
crayon.window.set_scaling_mode("stretch")  -- Fill screen
crayon.window.set_scaling_mode("center")   -- 1:1 unscaled

-- Fullscreen toggle
if crayon.input.is_pressed("f11") then
    crayon.window.set_fullscreen(not crayon.window.is_fullscreen())
end

-- Window state
crayon.window.maximize()
crayon.window.minimize()
crayon.window.restore()
crayon.window.set_position(100, 100)
crayon.window.center()

-- Advanced
crayon.window.set_opacity(0.9)
crayon.window.set_always_on_top(true)
crayon.window.set_mouse_grab(true)
crayon.window.show_cursor(false)

-- Get info
local w, h = crayon.window.get_window_size()
local rx, ry = crayon.window.get_resolution()
local fps = crayon.window.get_fps()
local dw, dh = crayon.window.get_display_size()
```

## Time API

```lua
local total_time = crayon.time.get_time()  -- Total elapsed seconds
local dt = crayon.time.get_dt()            -- Frame delta time
```

## Build Requirements

| Dependency | Version | Purpose |
|------------|---------|---------|
| C++ Compiler | C++20 | Language standard |
| OpenGL | 3.3+ | Graphics rendering |
| SDL3 | Latest | Windowing & input |
| GLM | Latest | Math operations |
| GLAD | Latest | OpenGL loading |
| Jolt Physics | Latest | 3D physics |
| LuaJIT | Latest | Scripting |
| stb_image | Latest | Texture loading |
| tinyobjloader | Latest | OBJ model loading |

## License

MIT License - See LICENSE file for details.