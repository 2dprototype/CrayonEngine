# Crayon on Android (alongside Windows)

One source tree, two targets. Windows keeps building exactly as before
(`build.bat`, MinGW + OpenGL 3.3 core). Android builds `libmain.so` with the NDK
and runs on **OpenGL ES 3.0**. Everything platform-specific is behind
`CRAYON_PLATFORM_ANDROID` / `CRAYON_GLES` (see `src/core/platform.hpp`).

> **Status:** every engine source file was syntax-checked against your real `vendor/`
> headers (SDL 3.4.16, Jolt, glm, Lua, ...) both as the normal Windows build and with the Android
> path switched on, with zero errors. The GLSL adapter and APK-asset extraction were also run as
> unit tests, and hypervis4d's own 3017 self-tests pass. What has **not** happened is an actual NDK
> build or a run on a device - expect a few link/runtime fixes on the first real attempt.

## What changed in the engine

| Area | Change |
|---|---|
| Graphics API | Context is created as GLES 3.0 on Android (`window.cpp`), loaded with `gladLoadGLES2Loader`. |
| Shaders | `graphics/glsl_compat.hpp` rewrites `#version 330 core` -> `#version 300 es` + precision statements. Applied inside `Shader::compile_stage`, so built-in shaders, post-process effects **and** Lua-supplied shaders all work with no edits. |
| Textures / fonts | `GL_TEXTURE_SWIZZLE_RGBA` (desktop only) replaced by four per-channel `glTexParameteri` calls. Same result on Windows. |
| Window | Overlay flags (transparent, borderless, always-on-top, click-through, skip-taskbar, opacity) are skipped on Android. Window is always fullscreen; its real size is read back from the OS. `SDL_EVENT_TERMINATING` ends the main loop cleanly. |
| Game files | The engine needs real file paths, but a game inside an APK has none. `core/android_storage.cpp` copies `assets/game/**` to internal storage on first launch (and whenever the build changes), then the normal "run a project folder" path takes over. |
| Dev loop | If `/sdcard/Android/data/<package>/files/game/` contains `main.lua` or `.crayonproj`, it is used instead of the APK copy (no rebuild needed: `adb push`, restart). |
| Entry point | `main()` is renamed to `SDL_main()` via `<SDL3/SDL_main.h>` (Android only). |
| Logging | `CRAYON_LOG_*` and `crayon.print` go to logcat (tag `Crayon`) instead of stdout. |
| Input | New multi-touch API (below). Android Back button is delivered as `"escape"`. Leaving the app fires `crayon.focusChanged(false)`. SDL already turns the first finger into mouse events, so mouse-only games work untouched. |
| Build | `CMakeLists.txt` has an `if(ANDROID)` branch: shared lib `main`, SDL3 built from source, no editor, no `-march=x86-64`, links `GLESv3 EGL android log`. |

### New Lua API (works on Windows touchscreens too)

```lua
crayon.touch.getCount()      -- number of fingers down
crayon.touch.getTouches()    -- { {id=, x=, y=, pressure=}, ... }  (virtual-canvas coords)
crayon.touch.isAvailable()   -- device has a touch screen
crayon.window.getPlatform()  -- "android", "windows", "linux", "macos"

function crayon.touchdown(id, x, y, pressure) end
function crayon.touchmoved(id, x, y, pressure) end
function crayon.touchup(id, x, y, pressure) end
```

## Dependencies

Already in `vendor/` and wired into CMake - nothing to do:

- **Jolt** is compiled from source for the target ABI (`vendor/JoltPhysics/CMakeLists.txt`, your wrapper).
  CMake forces `DEBUG_RENDERER_IN_DISTRIBUTION`, `PROFILER_IN_DISTRIBUTION` and `ENABLE_OBJECT_STREAM` on, so the
  library always matches the `JPH_*` defines the engine sets (a mismatch would crash, not fail to link).
- **box2d**, **hypervis4d**, glm, stb, tinyobj, tinygltf: plain source, built by CMake.
- **GLES loader**: `vendor/glad_gles/` is a small shim over the NDK's `<GLES3/gl3.h>`. ES 3.0 functions are exported by
  `libGLESv3.so`, so no generated loader is needed.

Two things are fetched/built by one script, `android/setup_deps.sh` (Linux, macOS or WSL):

- **SDL3 source** -> `vendor/sdl3-src` (tag `release-3.4.16`, matching your desktop SDL). `libSDL3.so` and the Java
  `SDLActivity` are both built from it, so they can't drift apart.
- **LuaJIT** -> `vendor/android/<abi>/lib/libluajit-5.1.a`, cross-built with the NDK.

```bash
export ANDROID_NDK_HOME=~/Android/Sdk/ndk/27.1.12297006   # NDK r26+ (needs libc++ <format>)
./android/setup_deps.sh                  # arm64-v8a;  ABIS="arm64-v8a x86_64" for emulators too
```

You also need Android Studio (or the command-line SDK) with CMake 3.22+ installed from the SDK manager.

## Build and run

```bat
# On Windows, simply run:
build_android.bat

# Or install directly to your connected device:
build_android.bat install
```

### Loading Games on Android

Crayon Engine has full storage access on Android and acts as a **game loader**:

1. **Shared Storage folder (`/sdcard/Crayon/`):**
   - Create a folder for your game, e.g.:
     `/sdcard/Crayon/mygame/`
   - Place your `main.lua` and/or `.crayonproj` and all assets (images, sounds, models) in that folder.
   - When you launch Crayon Engine, it automatically discovers and runs your project, loading all assets directly from that folder!
   - If you have multiple game folders in `/sdcard/Crayon/`, Crayon will run the most recently modified game, or you can write the folder name in `/sdcard/Crayon/active.txt`.

2. **Open from File Manager:**
   - Tap any `.crayonproj` or `.lua` file in your Android File Manager (e.g. Files, ZArchiver, etc.) $\rightarrow$ Open with **Crayon Game**!

3. **Fast launch via ADB:**
   ```bash
   adb shell am start -n com.crayonengine.player/.CrayonActivity -e game /sdcard/Crayon/mygame
   ```


## Windows build

Unchanged: `build.bat`. The only runtime differences are the texture-swizzle calls
(equivalent) and the new touch events/Lua functions (a Windows touchscreen now works too).

## Known limitations / things to check on the first device run

- **Skeletal animation uniform size.** The 3D vertex shader declares `mat4 u_bone_matrices[128]`
  = 512 uniform vectors. GLES 3.0 only *guarantees* 256; most Adreno/Mali GPUs give 1024, but a
  low-end device may fail to compile the 3D shader. If so, lower `MAX_BONES` to 64 (and the matching
  C++ limit) or move bones to a texture/UBO.
- **Window size vs pixel size.** Touch -> virtual-canvas mapping uses `SDL_GetWindowSize`, the same as
  the desktop code. On Android that should equal the pixel size; if touches look offset on some device,
  check `SDL_GetWindowPixelDensity` and switch `Window` to pixel sizes.
- **Notches / system bars.** No safe-area API yet (`SDL_GetWindowSafeArea` is the hook).
- **Lua shaders** must be valid GLSL ES 3.00: no implicit int->float (`float x = 1;` fails), compile
  errors show line numbers +4. All built-in shaders were audited and are fine.
- **Desktop-only features are no-ops:** transparent/click-through "screenpet" windows, drag-and-drop
  files, window move/resize/min-size, F5 hot reload (use the adb push loop).
- **Saved data.** `crayon.fs` paths are relative to the extracted game folder in internal storage,
  which persists across launches but is wiped on uninstall. Files not listed in the APK are never overwritten
  by an update.
- **Performance.** Heavy 3D (soft bodies, big ragdoll scenes, post-processing chains) is much costlier on
  phones; set `t.fpsLimit` and keep the virtual resolution low.
- **GL context loss** after a driver reset is not handled (normal pause/resume is fine: SDL blocks the
  app thread in the background and keeps the context).
- Only `arm64-v8a` is configured. For `armeabi-v7a` / `x86_64` build Jolt + LuaJIT for that ABI and add it
  to `abiFilters` in `android/app/build.gradle`.
