# Crayon Engine - Lua API Documentation

## Overview
Crayon Engine is a 2D/3D game engine with retro aesthetics, modern physics, and Lua scripting. All engine functionality is accessed through the global `crayon` table.

## Table of Contents
1. [Window Module](#window-module)
2. [Graphics Module](#graphics-module)
3. [Input Module](#input-module)
4. [Time Module](#time-module)
5. [Physics Module](#physics-module)
6. [Complete Examples](#complete-examples)

---

## Window Module
Access via: `crayon.window`

### Functions

| Function | Description |
|----------|-------------|
| `set_resolution(w, h)` | Set virtual resolution (default: 320x240) |
| `get_resolution()` | Returns current virtual resolution |
| `set_window_size(w, h)` | Set physical window size |
| `get_window_size()` | Returns current window size |
| `set_position(x, y)` | Set window position on screen |
| `get_position()` | Returns window position |
| `center()` | Center window on screen |
| `set_min_size(w, h)` | Set minimum window size |
| `set_max_size(w, h)` | Set maximum window size |
| `set_fullscreen(enabled)` | Toggle fullscreen |
| `is_fullscreen()` | Returns fullscreen state |
| `set_vsync(enabled)` | Toggle VSync |
| `get_vsync()` | Returns VSync state |
| `set_title(title)` | Set window title |
| `get_title()` | Returns window title |
| `set_resizable(enabled)` | Allow/disable window resizing |
| `is_resizable()` | Returns resizable state |
| `set_bordered(enabled)` | Show/hide window borders |
| `is_bordered()` | Returns bordered state |
| `maximize()` | Maximize window |
| `minimize()` | Minimize window |
| `restore()` | Restore window from min/max |
| `is_maximized()` | Returns maximized state |
| `is_minimized()` | Returns minimized state |
| `is_focused()` | Returns focused state |
| `set_scaling_mode(mode)` | Set scaling: "integer", "aspect", "stretch", "center" |
| `get_scaling_mode()` | Returns current scaling mode |
| `set_opacity(opacity)` | Set window opacity (0.0-1.0) |
| `get_opacity()` | Returns window opacity |
| `set_always_on_top(enabled)` | Keep window on top |
| `is_always_on_top()` | Returns always-on-top state |
| `raise()` | Raise window to front |
| `focus()` | Focus window |
| `flash()` | Flash taskbar icon |
| `set_mouse_grab(enabled)` | Grab mouse to window |
| `is_mouse_grabbed()` | Returns mouse grab state |
| `set_transparent(enabled)` | Enable transparent window (requires OS support) |
| `is_transparent()` | Returns transparency state |
| `show_cursor(show)` | Show/hide cursor |
| `is_cursor_visible()` | Returns cursor visibility |
| `set_mouse_relative(enabled)` | Enable relative mouse mode |
| `is_mouse_relative()` | Returns relative mouse state |
| `get_display_size()` | Get primary monitor resolution |
| `get_fps()` | Returns current FPS |
| `quit()` | Quit the engine |

### Window Module Examples

```lua
-- Basic window setup
crayon.window.set_resolution(640, 360)
crayon.window.set_window_size(1280, 720)
crayon.window.set_title("My Game")
crayon.window.set_vsync(true)

-- Scaling modes
crayon.window.set_scaling_mode("integer")  -- Pixel-perfect
crayon.window.set_scaling_mode("aspect")   -- Letterbox
crayon.window.set_scaling_mode("stretch")  -- Fill screen

-- Fullscreen toggle
if crayon.input.is_pressed("f11") then
    crayon.window.set_fullscreen(not crayon.window.is_fullscreen())
end

-- Get display info
local w, h = crayon.window.get_display_size()
print("Display:", w, "x", h)
```

---

## Graphics Module
Access via: `crayon.graphics`

### Color & Canvas

| Function | Description |
|----------|-------------|
| `clear(r, g, b [, a])` | Clear screen with color (0-1) |
| `set_color(r, g, b [, a])` | Set active drawing color (0-1) |

### Retro Effects

| Function | Description |
|----------|-------------|
| `set_retro_effects(opts)` | Apply retro visual effects |

**Options Table**:
```lua
{
    jitter_resolution = {160, 120},  -- Pixel snap resolution, or nil to disable
    affine = 1.0,                     -- 0=perspective, 1=affine texture mapping
    dither = true,                    -- Enable dithering
    color_depth = 32,                 -- 32 or 8 (bits per channel)
    dither_levels = 32,               -- Override color_depth
    fog = {                           -- Distance fog
        start = 5.0,
        end = 25.0,
        color = {0.1, 0.1, 0.2}
    },
    crt = {                           -- CRT effects
        scanlines = 0.25,             -- or true for default
        curvature = 0.05,             -- or true for default
        vignette = 0.25               -- or true for default
    },
    scanlines = 0.25,                 -- Shortcut for crt.scanlines
    curvature = 0.05,                 -- Shortcut for crt.curvature
    vignette = 0.25                   -- Shortcut for crt.vignette
}
```

### 3D Camera

| Function | Description |
|----------|-------------|
| `set_camera3d(cam)` | Set 3D camera parameters |
| `get_camera_ray(sx, sy)` | Get 3D ray from screen position |

**Camera Table**:
```lua
{
    position = {x, y, z},
    target = {x, y, z},
    up = {x, y, z},       -- default: {0, 1, 0}
    fov = 60,             -- field of view in degrees
    near = 0.1,
    far = 1000.0,
    ortho = false,        -- true for orthographic projection
    ortho_size = 10.0     -- half-height in world units
}
```

### Lighting

| Function | Description |
|----------|-------------|
| `set_light(dx, dy, dz, lr, lg, lb, ar, ag, ab)` | Set directional light |
| `set_point_light(idx, x, y, z, r, g, b, radius, intensity)` | Set point light (idx: 0-3) |
| `set_point_light_enabled(idx, enabled)` | Enable/disable point light |
| `set_shading_mode(mode)` | Set shading: "gouraud", "flat", "unlit" |

### Textures

| Function | Description |
|----------|-------------|
| `load_texture(path)` | Load texture, returns texture ID |
| `get_texture_size(id)` | Returns texture width, height |
| `get_white_texture()` | Returns white texture ID (fallback) |

### Models (3D)

| Function | Description |
|----------|-------------|
| `load_model(name)` | Load/create model, returns handle |
| `create_mesh(data)` | Create custom mesh, returns handle |

**Model Names**: "cube", "plane", "sphere", "cylinder", "cone", "pyramid", "torus", "capsule", "grid", or path to .obj file

**Mesh Data**:
```lua
{
    vertices = {
        {pos = {x,y,z}, norm = {x,y,z}, uv = {u,v}, color = {r,g,b,a}},
        -- ...
    },
    indices = {0,1,2, 0,2,3, ...}  -- Triangle indices
}
```

### 3D Drawing

| Function | Description |
|----------|-------------|
| `draw_model(id, x, y, z, rx, ry, rz, sx, sy, sz, tex)` | Draw loaded model |
| `draw_cube(x, y, z, sx, sy, sz, tex, rx, ry, rz)` | Draw cube |
| `draw_plane(x, y, z, w, d, tex, rx, ry, rz)` | Draw plane |
| `draw_sphere(x, y, z, radius, tex, rx, ry, rz)` | Draw sphere |
| `draw_cylinder(x, y, z, radius, height, tex, rx, ry, rz)` | Draw cylinder |
| `draw_cone(x, y, z, radius, height, tex, rx, ry, rz)` | Draw cone |
| `draw_pyramid(x, y, z, base_size, height, tex, rx, ry, rz)` | Draw pyramid |
| `draw_torus(x, y, z, radius, tube, tex, rx, ry, rz)` | Draw torus |
| `draw_capsule(x, y, z, radius, height, tex, rx, ry, rz)` | Draw capsule |
| `draw_billboard(x, y, z, w, h, tex, mode, u0, v0, u1, v1)` | Draw billboard (mode: "cylindrical" or nil for spherical) |
| `draw_billboard_rot(tex, x, y, z, w, h, angle, mode, color)` | Rotated billboard |
| `draw_line_3d(x1,y1,z1, x2,y2,z2)` | Draw 3D line |
| `draw_lines_3d(points)` | Draw multiple 3D lines |
| `draw_grid_3d(size, divs, y)` | Draw 3D grid |
| `draw_triangle_3d(p1, p2, p3, tex)` | Draw 3D triangle |
| `draw_quad_3d(p1, p2, p3, p4, tex)` | Draw 3D quad |
| `draw_axes_3d(x, y, z, size)` | Draw coordinate axes |
| `draw_cube_wires(x,y,z, sx,sy,sz, color, rx,ry,rz)` | Draw wireframe cube |

### 2D Drawing

| Function | Description |
|----------|-------------|
| `draw_sprite(tex, x, y, w, h, rot, ox, oy)` | Draw sprite |
| `draw_sprite_part(tex, x, y, u0, v0, u1, v1, w, h, rot, ox, oy)` | Draw sprite part |
| `draw_sprite_tiled(tex, x, y, w, h, tile_w, tile_h, ox, oy)` | Draw tiled sprite |
| `draw_sprite_9slice(tex, x, y, w, h, left, top, right, bottom, tex_w, tex_h)` | Draw 9-slice sprite |
| `draw_texture_rot(tex, x, y, w, h, angle, ox, oy)` | Draw rotated texture |
| `draw_point(x, y, size)` | Draw point |
| `draw_line(x1,y1, x2,y2, thick)` | Draw line |
| `draw_rect(mode, x, y, w, h, thick)` | Draw rectangle (mode: "fill" or "line") |
| `draw_rounded_rect(mode, x, y, w, h, radius, segs)` | Draw rounded rectangle |
| `draw_rounded_rect_ex(mode, x, y, w, h, rtl, rtr, rbr, rbl, segs)` | Per-corner rounded rect |
| `draw_triangle(mode, x1,y1, x2,y2, x3,y3)` | Draw triangle |
| `draw_quad(mode, x1,y1, x2,y2, x3,y3, x4,y4)` | Draw quad |
| `draw_polygon(mode, points)` | Draw polygon (points: {{x,y}, ...}) |
| `draw_circle(mode, cx, cy, radius, segs)` | Draw circle |
| `draw_ellipse(mode, cx, cy, rx, ry, segs)` | Draw ellipse |
| `draw_arc(mode, cx, cy, radius, a0, a1, segs)` | Draw arc |
| `draw_ring(mode, cx, cy, inner_r, outer_r, segs)` | Draw ring |
| `draw_pie(mode, cx, cy, radius, a1, a2, segs)` | Draw pie slice |
| `draw_gradient_rect(x, y, w, h, c_tl, c_tr, c_br, c_bl)` | Draw gradient rect |
| `draw_gradient_h(x, y, w, h, c_left, c_right)` | Horizontal gradient |
| `draw_gradient_v(x, y, w, h, c_top, c_bottom)` | Vertical gradient |
| `draw_polyline(points, thick, loop)` | Draw polyline |
| `draw_bezier(x0,y0, x1,y1, x2,y2, thick, segs)` | Draw quadratic bezier |
| `draw_bezier_cubic(x0,y0, x1,y1, x2,y2, x3,y3, thick, segs)` | Draw cubic bezier |

### 2D Transforms & Camera

| Function | Description |
|----------|-------------|
| `push_matrix_2d()` | Push 2D transform matrix |
| `pop_matrix_2d()` | Pop 2D transform matrix |
| `translate_2d(x, y)` | Translate 2D |
| `rotate_2d(angle)` | Rotate 2D (radians) |
| `scale_2d(sx, sy)` | Scale 2D |
| `set_camera2d(cam)` | Set 2D camera |
| `reset_camera2d()` | Reset 2D camera |

**2D Camera Table**:
```lua
{
    x = 0, y = 0,      -- Position
    zoom = 1.0,        -- Zoom factor
    angle = 0.0,       -- Rotation (radians)
    origin_x = 0,      -- Origin offset
    origin_y = 0
}
```

### 3D Transforms

| Function | Description |
|----------|-------------|
| `push_matrix()` | Push 3D transform matrix |
| `pop_matrix()` | Pop 3D transform matrix |
| `translate(x, y, z)` | Translate 3D |
| `rotate(angle, ax, ay, az)` | Rotate 3D (radians) |
| `scale(sx, sy, sz)` | Scale 3D |

### Scissor

| Function | Description |
|----------|-------------|
| `set_scissor(x, y, w, h)` | Set scissor rect |
| `reset_scissor()` | Reset scissor |
| `push_scissor(x, y, w, h)` | Push scissor with intersection |
| `pop_scissor()` | Pop scissor |

### Blend Modes

| Function | Description |
|----------|-------------|
| `set_blend_mode(mode)` | Set blend mode: "alpha", "additive", "multiply", "none" |

### Text

| Function | Description |
|----------|-------------|
| `draw_text(text, x, y, scale)` | Draw text (8x8 monospace font) |
| `get_text_width(text, scale)` | Get text width |
| `get_text_height(text, scale)` | Get text height |

### 3D Helpers

| Function | Description |
|----------|-------------|
| `project(x, y, z)` | Project 3D point to screen (returns x, y, visible) |
| `unproject(sx, sy)` | Get 3D ray from screen (returns origin x,y,z, dir x,y,z) |

---

## Input Module
Access via: `crayon.input`

### Keyboard

| Function | Description |
|----------|-------------|
| `is_down(key)` | Key currently held? |
| `is_pressed(key)` | Key just pressed? |
| `is_released(key)` | Key just released? |
| `any_key_pressed()` | Any key pressed? |
| `get_pressed_keys()` | List of pressed keys |

**Key Names**: "a", "b", ..., "z", "space", "enter", "escape", "tab", "backspace", "up", "down", "left", "right", "shift", "ctrl", "alt", "gui", "f1", "f2", ..., "f12"

### Modifiers

| Function | Description |
|----------|-------------|
| `is_shift_down()` | Shift held? |
| `is_ctrl_down()` | Ctrl held? |
| `is_alt_down()` | Alt held? |
| `is_gui_down()` | GUI (Windows/Command) held? |
| `is_caps_lock()` | Caps Lock active? |

### Mouse

| Function | Description |
|----------|-------------|
| `get_mouse_pos()` | Returns virtual x, y |
| `get_mouse_window_pos()` | Returns window x, y |
| `get_mouse_delta()` | Returns mouse delta x, y |
| `is_mouse_down(btn)` | Mouse button held? |
| `is_mouse_pressed(btn)` | Mouse button just pressed? |
| `is_mouse_released(btn)` | Mouse button just released? |
| `get_mouse_wheel()` | Returns wheel x, y |
| `set_mouse_position(x, y)` | Set mouse position |

**Button Types**: 1-5, "left"/"l"/"1", "middle"/"mid"/"m"/"2", "right"/"r"/"3", "x1"/"mouse4"/"4", "x2"/"mouse5"/"5"

### Text Input & Clipboard

| Function | Description |
|----------|-------------|
| `start_text_input()` | Start text input (IME) |
| `stop_text_input()` | Stop text input |
| `is_text_input_active()` | Text input active? |
| `get_text_input()` | Get input text |
| `get_clipboard()` | Get clipboard text |
| `set_clipboard(text)` | Set clipboard text |

### Gamepad

| Function | Description |
|----------|-------------|
| `gamepad_is_down(btn)` | Gamepad button held? |
| `gamepad_axis(axis)` | Get gamepad axis value (-1 to 1) |

---

## Time Module
Access via: `crayon.time`

| Function | Description |
|----------|-------------|
| `get_time()` | Total elapsed time (seconds) |
| `get_dt()` | Delta time (seconds since last frame) |

---

## Physics Module
Access via: `crayon.physics`

### Body Creation

| Function | Description |
|----------|-------------|
| `create_box(x, y, z, hx, hy, hz, motion, friction, restitution, density)` | Create box body |
| `create_sphere(x, y, z, radius, motion, friction, restitution, density)` | Create sphere body |
| `create_capsule(x, y, z, half_h, radius, motion, friction, restitution, density)` | Create capsule body |
| `create_cylinder(x, y, z, half_h, radius, motion, friction, restitution, density)` | Create cylinder body |
| `create_plane(x, y, z, nx, ny, nz, half_extent)` | Create static plane body |

**Motion Types**: "static", "kinematic", "dynamic" (or 0, 1, 2)

### Body Management

| Function | Description |
|----------|-------------|
| `destroy_body(id)` | Destroy body |
| `destroy_all()` | Destroy all bodies |
| `is_valid(id)` | Body still exists? |
| `is_active(id)` | Body active? |
| `set_active(id, active)` | Activate/deactivate body |

### Transforms

| Function | Description |
|----------|-------------|
| `get_position(id)` | Returns x, y, z |
| `set_position(id, x, y, z, activate)` | Set position |
| `get_rotation(id)` | Returns euler x, y, z (degrees) |
| `set_rotation(id, rx, ry, rz, activate)` | Set rotation (degrees) |

### Dynamics

| Function | Description |
|----------|-------------|
| `get_velocity(id)` | Returns vx, vy, vz |
| `set_velocity(id, vx, vy, vz)` | Set linear velocity |
| `get_angular_velocity(id)` | Returns wx, wy, wz |
| `set_angular_velocity(id, wx, wy, wz)` | Set angular velocity |
| `apply_force(id, fx, fy, fz)` | Apply force |
| `apply_impulse(id, ix, iy, iz)` | Apply impulse |
| `apply_torque(id, tx, ty, tz)` | Apply torque |
| `set_gravity_factor(id, factor)` | Set gravity multiplier |
| `set_friction(id, friction)` | Set friction |
| `set_restitution(id, restitution)` | Set restitution |
| `set_motion_type(id, type)` | Change motion type |
| `set_damping(id, linear_damping, angular_damping)` | Set damping |

### World Settings

| Function | Description |
|----------|-------------|
| `set_gravity(gx, gy, gz)` | Set world gravity |
| `get_gravity()` | Returns gx, gy, gz |

### Raycast

| Function | Description |
|----------|-------------|
| `raycast(ox, oy, oz, dx, dy, dz, max_dist)` | Cast ray. Returns hit, pos x/y/z, normal x/y/z, distance, body_id |

### Constraints

| Function | Description |
|----------|-------------|
| `create_point_constraint(b1, b2, px, py, pz)` | Create point constraint |
| `create_hinge_constraint(b1, b2, px, py, pz, ax, ay, az, min_angle, max_angle)` | Create hinge constraint |
| `create_distance_constraint(b1, b2, p1x, p1y, p1z, p2x, p2y, p2z, min_d, max_d)` | Create distance constraint |
| `create_fixed_constraint(b1, b2)` | Create fixed constraint |
| `destroy_constraint(id)` | Destroy constraint |

### Sensors

| Function | Description |
|----------|-------------|
| `set_sensor(id, is_sensor)` | Make body a sensor (trigger) |
| `is_sensor(id)` | Is body a sensor? |

### Queries & Debug

| Function | Description |
|----------|-------------|
| `overlap_sphere(cx, cy, cz, radius)` | Returns table of body IDs in sphere |
| `draw_debug(r, g, b, a, sr, sg, sb, sa)` | Draw physics debug (AABB wireframes) |
| `get_body_count()` | Returns total bodies, active bodies |

---

## Complete Examples

### Example 1: Basic Game Loop

```lua
-- game.lua

function crayon.init()
    -- Setup window
    crayon.window.set_resolution(320, 240)
    crayon.window.set_window_size(960, 720)
    crayon.window.set_title("My Game")
    
    -- Setup graphics
    crayon.graphics.set_color(1, 1, 1)
end

function crayon.update(dt)
    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Draw some text
    crayon.graphics.draw_text("Hello World!", 10, 10, 2)
    
    -- Draw FPS
    local fps = crayon.window.get_fps()
    crayon.graphics.draw_text("FPS: " .. tostring(fps), 10, 30, 1)
end
```

### Example 2: 2D Sprite & Shapes

```lua
function crayon.init()
    crayon.window.set_resolution(640, 480)
    crayon.window.set_window_size(1280, 960)
    crayon.window.set_title("2D Demo")
    
    tex = crayon.graphics.load_texture("assets/player.png")
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Sprites
    local x, y = crayon.input.get_mouse_pos()
    crayon.graphics.draw_sprite(tex, x, y, 32, 32, 0, 16, 16)
    
    -- Sprite part (sprite sheet)
    crayon.graphics.draw_sprite_part(tex, 100, 100, 0, 0, 16, 16, 32, 32)
    
    -- Tiled sprite
    crayon.graphics.draw_sprite_tiled(tex, 200, 200, 100, 100, 16, 16)
    
    -- 9-slice
    crayon.graphics.draw_sprite_9slice(tex, 300, 300, 64, 64, 8, 8, 8, 8)
    
    -- Shapes
    crayon.graphics.set_color(1, 0, 0, 1)
    crayon.graphics.draw_rect("fill", 10, 10, 50, 50)
    
    crayon.graphics.set_color(0, 1, 0, 1)
    crayon.graphics.draw_circle("fill", 100, 100, 30)
    
    crayon.graphics.set_color(1, 1, 0, 1)
    crayon.graphics.draw_line(200, 200, 300, 300, 2)
    
    -- Polygon (hexagon)
    local hex = {}
    for i = 0, 5 do
        local angle = i * 3.14159 / 3
        hex[i+1] = {400 + math.cos(angle) * 40, 300 + math.sin(angle) * 40}
    end
    crayon.graphics.set_color(0.5, 0, 1, 1)
    crayon.graphics.draw_polygon("fill", hex)
    
    -- Text
    crayon.graphics.set_color(1, 1, 1, 1)
    crayon.graphics.draw_text("Hello 2D!", 20, 200, 2)
end
```

### Example 3: 3D Scene with Physics

```lua
-- 3D platformer example

function crayon.init()
    crayon.window.set_resolution(320, 240)
    crayon.window.set_window_size(960, 720)
    crayon.window.set_title("3D Physics Demo")
    
    -- Setup camera
    crayon.graphics.set_camera3d({
        position = {0, 3, 8},
        target = {0, 0, 0},
        fov = 60
    })
    
    -- Setup lighting
    crayon.graphics.set_light(-0.5, -1, -0.7, 1, 0.95, 0.9, 0.25, 0.25, 0.3)
    
    -- Retro effects
    crayon.graphics.set_retro_effects({
        jitter_resolution = {160, 120},
        affine = 1.0,
        dither = true,
        color_depth = 32,
        fog = {start = 10, end = 30, color = {0.1, 0.1, 0.2}}
    })
    
    -- Load textures
    floor_tex = crayon.graphics.load_texture("assets/floor.png")
    player_tex = crayon.graphics.load_texture("assets/player.png")
    
    -- Create physics floor
    floor = crayon.physics.create_plane(0, -0.5, 0, 0, 1, 0, 50)
    
    -- Create player
    player = crayon.physics.create_sphere(0, 2, 0, 0.5, "dynamic", 0.5, 0.5)
    crayon.physics.set_friction(player, 0.8)
    crayon.physics.set_restitution(player, 0.3)
    
    -- Create some obstacles
    obstacles = {}
    for i = 1, 5 do
        local x = (i - 3) * 2
        local z = 3
        local id = crayon.physics.create_box(x, 0.5, z, 0.5, 0.5, 0.5, "static")
        table.insert(obstacles, {id = id, x = x, z = z})
    end
end

function crayon.update(dt)
    -- Player controls
    local speed = 5
    local jump_force = 5
    
    if crayon.input.is_down("w") or crayon.input.is_down("up") then
        local vx, vy, vz = crayon.physics.get_velocity(player)
        crayon.physics.set_velocity(player, vx, vy, -speed)
    end
    if crayon.input.is_down("s") or crayon.input.is_down("down") then
        local vx, vy, vz = crayon.physics.get_velocity(player)
        crayon.physics.set_velocity(player, vx, vy, speed)
    end
    if crayon.input.is_down("a") or crayon.input.is_down("left") then
        local vx, vy, vz = crayon.physics.get_velocity(player)
        crayon.physics.set_velocity(player, -speed, vy, vz)
    end
    if crayon.input.is_down("d") or crayon.input.is_down("right") then
        local vx, vy, vz = crayon.physics.get_velocity(player)
        crayon.physics.set_velocity(player, speed, vy, vz)
    end
    
    if crayon.input.is_pressed("space") then
        local vx, vy, vz = crayon.physics.get_velocity(player)
        crayon.physics.set_velocity(player, vx, jump_force, vz)
    end
    
    -- Quit
    if crayon.input.is_pressed("escape") then
        crayon.window.quit()
    end
    
    -- Hot reload
    if crayon.input.is_pressed("f5") then
        print("Hot reload requested")
    end
    
    -- Physics camera follows player
    local px, py, pz = crayon.physics.get_position(player)
    crayon.graphics.set_camera3d({
        position = {px, 3, pz + 8},
        target = {px, 0, pz},
        fov = 60
    })
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Draw floor
    crayon.graphics.draw_plane(0, 0, 0, 20, 20, floor_tex, 0, 0, 0)
    
    -- Draw player
    local px, py, pz = crayon.physics.get_position(player)
    crayon.graphics.draw_sphere(px, py, pz, 0.5, player_tex)
    
    -- Draw obstacles
    for _, obs in ipairs(obstacles) do
        crayon.graphics.draw_cube(obs.x, 0.5, obs.z, 1, 1, 1)
    end
    
    -- Debug: physics visualization
    crayon.physics.draw_debug(0.2, 1, 0.4, 1, 0.5, 0.5, 0.5, 1)
    
    -- UI overlay
    crayon.graphics.set_color(1, 1, 1, 1)
    local fps = crayon.window.get_fps()
    crayon.graphics.draw_text("FPS: " .. tostring(fps), 10, 10, 1)
    crayon.graphics.draw_text("WASD to move, Space to jump", 10, 30, 1)
end
```

### Example 4: 3D Model Loading

```lua
function crayon.init()
    crayon.window.set_resolution(640, 480)
    crayon.window.set_title("3D Models")
    
    crayon.graphics.set_camera3d({
        position = {0, 3, 10},
        target = {0, 0, 0}
    })
    
    crayon.graphics.set_light(-0.5, -1, -0.7, 1, 0.95, 0.9, 0.3, 0.3, 0.35)
    
    -- Load models
    cube = crayon.graphics.load_model("cube")
    sphere = crayon.graphics.load_model("sphere")
    torus = crayon.graphics.load_model("torus")
    
    -- Load custom OBJ
    character = crayon.graphics.load_model("assets/character.obj")
    tex = crayon.graphics.load_texture("assets/character.png")
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    local t = crayon.time.get_time()
    
    -- Draw models with transforms
    crayon.graphics.draw_model(cube, -4, 0, 0, t, t*0.5, 0, 1, 1, 1)
    crayon.graphics.draw_model(sphere, -1.5, 0.5, 0, 0, t, 0, 1, 1, 1)
    crayon.graphics.draw_model(torus, 1.5, 0.5, 0, t, t*0.7, t*0.3, 1, 1, 1)
    
    -- Custom model with texture
    crayon.graphics.draw_model(character, 4, 0, 0, 0, t, 0, 1, 1, 1, tex)
    
    -- Using transform stack
    crayon.graphics.push_matrix()
    crayon.graphics.translate(0, 2, 0)
    crayon.graphics.rotate(t, 0, 1, 0)
    crayon.graphics.scale(1.5, 1.5, 1.5)
    crayon.graphics.draw_model(sphere, 0, 0, 0, 0, 0, 0, 1, 1, 1)
    crayon.graphics.pop_matrix()
    
    -- UI
    crayon.graphics.set_color(1, 1, 1, 1)
    crayon.graphics.draw_text("3D Models Demo", 10, 10, 2)
end
```

### Example 5: 2D Camera & Transforms

```lua
function crayon.init()
    crayon.window.set_resolution(640, 480)
    crayon.window.set_title("2D Camera Demo")
    
    tex = crayon.graphics.load_texture("assets/character.png")
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    local t = crayon.time.get_time()
    
    -- Camera controls
    local cx = math.sin(t * 0.3) * 100
    local cy = math.cos(t * 0.2) * 50
    local zoom = 1.0 + math.sin(t * 0.5) * 0.3
    
    crayon.graphics.set_camera2d({
        x = cx, y = cy,
        zoom = zoom,
        angle = t * 0.1
    })
    
    -- Draw grid
    for i = -20, 20 do
        for j = -15, 15 do
            local x = i * 32
            local y = j * 32
            local color = (i + j) % 2 == 0 and {0.2, 0.2, 0.3, 1} or {0.3, 0.3, 0.4, 1}
            crayon.graphics.set_color(unpack(color))
            crayon.graphics.draw_rect("fill", x, y, 32, 32)
        end
    end
    
    -- Draw character
    crayon.graphics.set_color(1, 1, 1, 1)
    crayon.graphics.draw_sprite(tex, 0, 0, 32, 32, t, 16, 16)
    
    -- Reset camera for UI
    crayon.graphics.reset_camera2d()
    crayon.graphics.set_color(1, 1, 1, 1)
    crayon.graphics.draw_text("2D Camera Demo", 10, 10, 2)
    crayon.graphics.draw_text("Zoom: " .. string.format("%.2f", zoom), 10, 30, 1)
end
```

### Example 6: Input Handling

```lua
function crayon.init()
    crayon.window.set_resolution(640, 480)
    crayon.window.set_title("Input Demo")
    
    crayon.input.start_text_input()
end

function crayon.update(dt)
    -- Keyboard
    if crayon.input.is_pressed("space") then
        print("Space pressed!")
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
    
    -- Gamepad
    if crayon.input.gamepad_is_down(0) then  -- A button
        print("A button held!")
    end
    
    local lx = crayon.input.gamepad_axis(0)   -- Left stick X
    local ly = crayon.input.gamepad_axis(1)   -- Left stick Y
    
    -- Text input
    local text = crayon.input.get_text_input()
    if text ~= "" then
        print("Text input:", text)
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    local mx, my = crayon.input.get_mouse_pos()
    local mwx, mwy = crayon.input.get_mouse_window_pos()
    
    crayon.graphics.set_color(1, 1, 1, 1)
    crayon.graphics.draw_text("Input Demo", 10, 10, 2)
    crayon.graphics.draw_text("Mouse: " .. tostring(mx) .. ", " .. tostring(my), 10, 40, 1)
    crayon.graphics.draw_text("Window Mouse: " .. tostring(mwx) .. ", " .. tostring(mwy), 10, 55, 1)
    
    local text = crayon.input.get_text_input()
    crayon.graphics.draw_text("Text: " .. text, 10, 70, 1)
    
    -- Draw crosshair at mouse position
    crayon.graphics.set_color(1, 0, 0, 1)
    crayon.graphics.draw_line(mx - 10, my, mx + 10, my, 1)
    crayon.graphics.draw_line(mx, my - 10, mx, my + 10, 1)
end
```

### Example 7: Physics Constraints

```lua
function crayon.init()
    crayon.window.set_resolution(640, 480)
    crayon.window.set_title("Constraints Demo")
    
    crayon.graphics.set_camera3d({
        position = {0, 5, 15},
        target = {0, 2, 0}
    })
    
    -- Create two dynamic bodies
    body1 = crayon.physics.create_box(-2, 3, 0, 0.5, 0.5, 0.5, "dynamic")
    body2 = crayon.physics.create_box(2, 3, 0, 0.5, 0.5, 0.5, "dynamic")
    
    crayon.physics.set_motion_type(body1, "kinematic")  -- Make one kinematic
    
    -- Create constraint between them
    constraint = crayon.physics.create_distance_constraint(
        body1, body2,
        -2, 3, 0,  -- Point on body1
        2, 3, 0,   -- Point on body2
        2, 4       -- Min and max distance
    )
    
    -- Alternatively, create a hinge
    -- constraint = crayon.physics.create_hinge_constraint(
    --     body1, body2,
    --     0, 3, 0,  -- Pivot point
    --     0, 1, 0,  -- Axis
    --     -1.57, 1.57  -- Angle limits
    -- )
    
    -- Static floor
    floor = crayon.physics.create_plane(0, -0.5, 0, 0, 1, 0, 20)
end

function crayon.update(dt)
    -- Rotate kinematic body
    local t = crayon.time.get_time()
    local px = math.sin(t * 2) * 2
    local pz = math.cos(t * 2) * 2
    crayon.physics.set_position(body1, px, 3, pz)
    crayon.physics.set_rotation(body1, t * 90, t * 45, 0)
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Draw bodies
    crayon.graphics.draw_cube(-2, 3, 0, 1, 1, 1, 0, 0, 0, 0)
    crayon.graphics.draw_cube(2, 3, 0, 1, 1, 1, 0, 0, 0, 0)
    crayon.graphics.draw_plane(0, 0, 0, 20, 20)
    
    -- Debug
    crayon.physics.draw_debug()
end
```

### Example 8: Raycasting

```lua
function crayon.init()
    crayon.window.set_resolution(640, 480)
    crayon.window.set_title("Raycast Demo")
    
    crayon.graphics.set_camera3d({
        position = {0, 10, 10},
        target = {0, 0, 0}
    })
    
    -- Create some obstacles
    obstacles = {}
    for i = 1, 8 do
        local angle = i * 3.14159 / 4
        local x = math.cos(angle) * 3
        local z = math.sin(angle) * 3
        local id = crayon.physics.create_box(x, 0.5, z, 0.5, 0.5, 0.5, "static")
        table.insert(obstacles, {id = id, x = x, z = z})
    end
end

function crayon.update(dt)
    -- Raycast from camera center
    local mx, my = crayon.input.get_mouse_pos()
    local origin_x, origin_y, origin_z, dir_x, dir_y, dir_z = crayon.graphics.unproject(mx, my)
    
    local hit, hx, hy, hz, nx, ny, nz, dist, body_id = crayon.physics.raycast(
        origin_x, origin_y, origin_z,
        dir_x, dir_y, dir_z,
        100
    )
    
    if hit then
        print("Hit at:", hx, hy, hz, "Distance:", dist)
        last_hit = {hx, hy, hz}
    end
end

function crayon.draw()
    crayon.graphics.clear(0.1, 0.1, 0.2)
    
    -- Draw obstacles
    for _, obs in ipairs(obstacles) do
        crayon.graphics.draw_cube(obs.x, 0.5, obs.z, 1, 1, 1)
    end
    
    -- Draw hit point
    if last_hit then
        crayon.graphics.set_color(1, 0, 0, 1)
        crayon.graphics.draw_sphere(last_hit[1], last_hit[2], last_hit[3], 0.2, 0)
    end
    
    -- UI
    crayon.graphics.set_color(1, 1, 1, 1)
    crayon.graphics.draw_text("Click to raycast", 10, 10, 2)
end
```

---

## Quick Reference Card

### Core Functions
```lua
crayon.init()      -- Called once on startup
crayon.update(dt)  -- Called every frame
crayon.draw()      -- Called every frame for rendering
```

### Common Patterns

**Loading Resources**:
```lua
tex = crayon.graphics.load_texture("path.png")
model = crayon.graphics.load_model("path.obj")
```

**Creating Physics Bodies**:
```lua
id = crayon.physics.create_box(x, y, z, hx, hy, hz, "dynamic", 0.5, 0.2)
```

**Getting Body State**:
```lua
x, y, z = crayon.physics.get_position(id)
vx, vy, vz = crayon.physics.get_velocity(id)
```

**Input Checking**:
```lua
if crayon.input.is_pressed("space") then
    -- Jump!
end
```

**Drawing 2D**:
```lua
crayon.graphics.set_color(r, g, b, a)
crayon.graphics.draw_rect("fill", x, y, w, h)
```

**Drawing 3D**:
```lua
crayon.graphics.draw_cube(x, y, z, sx, sy, sz, tex, rx, ry, rz)
```