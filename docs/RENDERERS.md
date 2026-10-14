# Renderers: GPU and CPU

Crayon has two interchangeable backends. The Lua API is identical on both.

| | OpenGL | Software (CPU) |
|---|---|---|
| Needs | GL 3.3 core / GLES 3.0 | nothing (SDL window surface) |
| 3D | shaders, FBO, GPU skinning | `crayon::soft::Rasterizer`, CPU skinning |
| 2D | GL batcher | same batcher, CPU fill (exact edge rules) |
| Post | shader chain + built-ins | dither, CRT scanlines, vignette |
| Custom GLSL shaders / `Canvas` / shader post effects | yes | not available (clear warning in log) |

## Choosing

Priority: `--renderer=` flag, then `CRAYON_RENDERER` env var, then `crayon.config`:

    crayon.config({ graphics = { renderer = "auto" } })   -- "auto" | "gl" | "soft"

`auto` (default) uses the GPU, and falls back to the CPU rasterizer when no OpenGL context can be
created or the driver is a software GL (llvmpipe, SwiftShader, Windows "GDI Generic"...).
Force a backend with `--renderer=gl` or `--renderer=soft`.

Lua: `graphics.isSoftwareRenderer()`, `graphics.getRendererName()`.

## Tests (headless, no window/GPU)

    g++ -std=c++20 -O3 -Isrc -Ivendor/glm -Ivendor/tinyobj/include test/soft/soft_raster_test.cpp \
        src/graphics/soft/soft_raster.cpp -o soft_test && ./soft_test examples/assets/models/monkey.obj out
    cmake --build . --target soft_raster_test

`test/soft/soft_engine_test.cpp` drives the real Texture/Batch2D/MeshRenderer3D/Model3D classes in
software mode and writes a PPM.

## Low-end tips

* Lower the virtual resolution (`--res 320x240`): the CPU backend cost scales with pixels.
* Keep `affine`/`jitter` retro effects on (cheap) and avoid huge alpha-blended overdraw.
* Use `nearest=false` textures only where needed: smooth textures are bilinear + mip-mapped.
