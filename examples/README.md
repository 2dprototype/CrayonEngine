# Crayon Engine Examples

This directory contains complete, standalone examples demonstrating Crayon Engine's modernized subsystems and 100% `camelCase` Lua APIs.

## Running Examples

From the root repository directory, run any example with the engine player binary:

```bash
# Windows
.\build\crayon.exe examples\01_primitives_2d.lua
.\build\crayon.exe examples\02_fonts_and_text.lua
.\build\crayon.exe examples\03_particles.lua
.\build\crayon.exe examples\04_canvas_and_shaders.lua
.\build\crayon.exe examples\05_audio.lua
.\build\crayon.exe examples\06_physics_3d.lua
.\build\crayon.exe examples\07_primitives_3d_lighting.lua
.\build\crayon.exe examples\08_input_and_fs.lua
.\build\crayon.exe examples\09_math_and_noise.lua
```

---

## Example Index

| File | Subsystems & Features Demonstrated |
| :--- | :--- |
| **[`01_primitives_2d.lua`](01_primitives_2d.lua)** | Basic 2D primitives (`drawRect`, `drawCircle`, `drawLine`, `drawTriangle`, `drawPolygon`), matrix stack transformations (`push`, `pop`, `translate`, `rotate`, `scale`), and alpha blending. |
| **[`02_fonts_and_text.lua`](02_fonts_and_text.lua)** | Dynamic TTF typography, multi-line paragraph formatting, automatic word wrapping (`wrap`), text alignments (`"left"`, `"center"`, `"right"`), and text scaling. |
| **[`03_particles.lua`](03_particles.lua)** | Particle engine (`crayon.particles.createEmitter`), continuous emission, click bursts, gravity convection, lifetime management, and additive blend modes. |
| **[`04_canvas_and_shaders.lua`](04_canvas_and_shaders.lua)** | Framebuffer rendering (`createCanvas`, `canvas:renderTo`), custom GLSL post-processing shaders (`loadShader`), uniform parameter passing (`sendFloat`, `sendVec2`), and CRT scanline/vignette filters. |
| **[`05_audio.lua`](05_audio.lua)** | Audio engine (`crayon.audio.loadSound`, `playSound`), polyphonic SFX playback, volume buses (`master`, `sfx`, `music`), pitch/panning modulation, and 3D spatial sound listener positioning. |
| **[`06_physics_3d.lua`](06_physics_3d.lua)** | Unified Jolt 3D physics (`crayon.physics.createBox`, `createSphere`), dynamic rigid bodies, impulse applications, Continuous Collision Detection (`linearCast`), and collision event callbacks (`crayon.onCollisionEnter`). |
| **[`07_primitives_3d_lighting.lua`](07_primitives_3d_lighting.lua)** | 3D perspective camera controls, atmospheric sky gradients (`drawSkyGradient`), directional sunlight (`setLight`), colored point lights (`setPointLight`), spotlights (`setSpotLight`), and shading modes (`"flat"`, `"gouraud"`, `"unlit"`). |
| **[`08_input_and_fs.lua`](08_input_and_fs.lua)** | Unified input handling (`crayon.input`), window lifecycle callbacks (`crayon.windowResized`, `crayon.focusChanged`, `crayon.quit`), and persistent file storage (`crayon.fs.writeText`, `crayon.fs.readText`, `crayon.fs.exists`). |
| **[`09_math_and_noise.lua`](09_math_and_noise.lua)** | Vector mathematics (`vec2Length`, `vec2Normalize`, `vec2Dot`, `vec2Distance`), interpolation curves (`lerp`, `smoothstep`), smooth spring damping (`damp`), and procedural 2D noise generation (`noise`). |
