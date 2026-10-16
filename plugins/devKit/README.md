# Crayon DevKit Plugin (`devKit`)

`devKit` is a native in-game developer overlay, profiler, live REPL, and visual debugger for Crayon Engine. It allows game developers to inspect performance, tune shaders, control simulation time, and execute live Lua commands on the fly.

## Key Features

- **Toggle Hotkey**: Press **`F3`** (or **`~` / `` ` ``**) anytime during gameplay to toggle the DevKit overlay.
- **Post-Process Immune**: Intercepts `SDL_GL_SwapWindow` to render directly to the screen backbuffer at native display resolution, remaining crisp and readable regardless of CRT or retro post-processing filters.
- **Performance Profiler**: Real-time FPS, frame time (ms), min/max/average stats, and rolling plot graphs.
- **Simulation Time Control**:
  - Live Pause / Resume button.
  - Single-frame stepping (`Step 1 Frame`).
  - Time dilation slider from `0.05x` (slow-motion) up to `3.0x` (speedup) with quick presets.
- **Live Lua REPL & Console**:
  - Execute arbitrary Lua statements in the game's active state.
  - Scrollable log history and command input.
- **Retro Visuals Tuning**:
  - Live sliders for CRT scanline strength, curvature distortion, vignette intensity, and dithering.
- **Custom Game Debug Tabs**:
  - Register custom game tabs and cheat menus via `crayon.devKit.addTab(title, callback)`.
- **Zero Engine Core Modifications**: Completely standalone in `plugins/devKit/`.

## Building

To build the plugin standalone:

### Windows
```bat
plugins\build_devKit.bat
```
*(or run `build.bat` inside `plugins\devKit\`)*

### Linux / macOS
```bash
plugins/build_devKit.sh
```

The compiled library (`devKit.dll` / `libdevKit.so` / `devKit.dylib`) is written to `plugins/bin/` and automatically copied next to `crayon.exe`.

## Usage in Game

Enable `t.plugins.devKit = true` in `crayon.config`:

```lua
function crayon.config(t)
    t.title = "My Game"
    t.window = {
        title = "My Game",
        width = 1280,
        height = 720,
        virtualWidth = 640,
        virtualHeight = 360,
        resizable = true,
        vsync = true
    }
    t.graphics = {
        renderer = "gl",
        crt = true,
        dither = true
    }

    -- Enable DevKit plugin
    t.plugins.devKit = true
end
```

In `crayon.update(dt)`:

```lua
function crayon.update(dt)
    -- Respect DevKit time dilation and pause state
    local timeScale = crayon.devKit and crayon.devKit.getTimeScale() or 1.0
    local isPaused = crayon.devKit and crayon.devKit.isPaused() or false

    if isPaused then return end

    local simDt = dt * timeScale

    -- Only process game keyboard inputs when DevKit is not typing
    if not (crayon.devKit and crayon.devKit.wantCaptureKeyboard()) then
        -- Handle game controls using simDt
    end
end
```

## Lua API Reference (`crayon.devKit`)

| Function | Parameters | Returns | Description |
|---|---|---|---|
| `setVisible(bool)` | boolean | - | Sets DevKit overlay visibility |
| `isVisible()` | - | `bool` | Returns true if DevKit is currently open |
| `toggle()` | - | - | Toggles DevKit open/closed |
| `getTimeScale()` | - | `float` | Returns current time dilation multiplier (e.g. `1.0`) |
| `setTimeScale(scale)` | float | - | Sets time dilation multiplier (e.g. `0.2` for slow-mo) |
| `isPaused()` | - | `bool` | Returns true if simulation is paused in DevKit |
| `setPaused(bool)` | boolean | - | Pauses or unpauses simulation |
| `stepFrame()` | - | - | Requests a single frame step |
| `addTab(title, callback)` | string, function | - | Registers a custom debug tab in the DevKit |
| `log(text, [isError])` | string, bool | - | Prints a message to the DevKit in-game console |
| `wantCaptureMouse()` | - | `bool` | True if mouse is interacting with DevKit |
| `wantCaptureKeyboard()` | - | `bool` | True if user is typing in DevKit input fields |
