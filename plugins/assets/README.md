# Crayon Assets Plugin (`crayon.assets`)

Asynchronous mission-based asset loading and memory management for huge games in Crayon Engine.

Designed specifically for games with multiple maps and large asset sets (like *AirStrike Gunship*), where loading everything at startup is impossible or causes lag.

---

## Key Features

1. **Mission / Level Asset Bundles**: Group textures, 3D models, sounds, music, and data by mission (`common`, `mission_01_desert`, `mission_02_jungle`).
2. **Smooth Non-blocking Loading Screens**: Spreads asset decoding across frames using a configurable time budget (default: 8ms/frame). The main loop never freezes, keeping your 60 FPS loading screen animations silky smooth!
3. **Fine-Grained Progress Tracking**: Query `getProgress()` (0.0 to 1.0), `getPercent()` (0 to 100), and `getCurrentItem()` to render animated progress bars and mission briefing text.
4. **Instant Unloading & Memory Reclamation**: Unload all assets of a finished mission with `crayon.assets.unload("mission_01")` before loading the next mission, freeing RAM and VRAM.
5. **Fast $O(1)$ Access**: Retrieve handles directly (`getTexture`, `getModel`, `getSound`, `getData`, or `get`).

## API Reference

### Bundle Management
- `crayon.assets.define(bundleName, definitionTable)`: Registers a bundle definition.
- `crayon.assets.loadAsync(bundleName [, onCompleteCallback])`: Queues the bundle for async time-sliced loading.
- `crayon.assets.loadSync(bundleName)`: Loads the bundle immediately (blocking).
- `crayon.assets.unload(bundleName)`: Unloads all assets in the bundle and frees sound buffers and VRAM references.
- `crayon.assets.unloadAll()`: Unloads all active bundles.
- `crayon.assets.isLoaded(bundleName)`: Returns `true` if the bundle is fully loaded.
- `crayon.assets.getLoadedBundles()`: Returns an array of loaded bundle names.

### Progress & Time-Slicing
- `crayon.assets.isDone()`: Returns `true` if all async queues are finished.
- `crayon.assets.getProgress()`: Returns float between `0.0` and `1.0`.
- `crayon.assets.getPercent()`: Returns integer between `0` and `100`.
- `crayon.assets.getCurrentItem()`: Returns the file path currently being loaded.
- `crayon.assets.getCount()`: Returns `loadedCount, totalCount`.
- `crayon.assets.setBudget(milliseconds)`: Sets max loading time per frame (default: `8.0` ms).
- `crayon.assets.getBudget()`: Returns current per-frame budget.

### Asset Retrieval
- `crayon.assets.getTexture(id)`: Returns the `Graphics.Texture` userdata.
- `crayon.assets.getModel(id)`: Returns the `Graphics.Model` userdata.
- `crayon.assets.getSound(id)`: Returns the audio sound ID.
- `crayon.assets.getMusic(id)`: Returns the music path string.
- `crayon.assets.getData(id)`: Returns the data string.
- `crayon.assets.get(id)`: Auto-returns the asset regardless of type.
- `crayon.assets.has(id)`: Returns `true` if the asset ID is loaded and available.

---

## Building

```cmd
plugins\build_assets.bat     :: Windows
./plugins/build_assets.sh    :: Linux / macOS
```
