# Simple UI Plugin for Crayon Engine

`simpleUI` is an advanced native UI suite for Crayon Engine powered by Dear ImGui. It runs independently of Crayon's internal canvas framebuffer and is **completely unaffected by retro filters, CRT scanlines, dithering, vignette, or post-processing shaders**.

## Key Features

- **Post-Process Immune**: Intercepts the screen presentation right before `SDL_GL_SwapWindow`, rendering at full native display resolution directly to the physical backbuffer after all post-processing effects have finished.
- **Battle-Ready Component Suite**: Tabs, Combos/Dropdowns, Tooltips, Modal Popups, Multi-column Data Tables, Images/Textures, Multiline Text Inputs, Style Push/Pop, and Drag Sliders.
- **HUD & Frameless Overlays**: Use `WindowFlags` (e.g. `NoTitleBar`, `NoResize`, `NoMove`, `NoBackground`) to build transparent, razor-sharp HUDs floating over retro pixel games.
- **Zero Engine Core Changes**: Completely isolated in `plugins/simple_ui/`.
- **Strictly camelCase**: Native Crayon API conventions (`crayon.simpleUI`, `t.plugins.simpleUI = true`).

## Building

To build the plugin standalone:

### Windows
```bat
plugins\build_simple_ui.bat
```
*(or run `build.bat` inside `plugins\simple_ui\`)*

### Linux / macOS
```bash
plugins/build_simple_ui.sh
```

The compiled library (`simpleUI.dll` / `simpleUI.so` / `simpleUI.dylib`) is placed in `plugins/bin/` and automatically copied next to `crayon.exe`.

## Usage in Game

Enable the plugin in `crayon.config`:

```lua
function crayon.config(t)
    t.identity = "AirStrikeGunship"
    t.window = {
        title = "AirStrike Gunship",
        width = 1280,
        height = 720,
        virtualWidth = 640,
        virtualHeight = 360,
        resizable = true,
        vsync = true
    }
    t.graphics = {
        renderer = "gl",
        clearColor = {0.05, 0.08, 0.12, 1.0},
        crt = true,      -- Retro CRT/scanline effects applied to game canvas
        dither = true,
        vignette = true
    }
    t.plugins.simpleUI = true   -- Loads simpleUI.dll
end
```

In `crayon.update(dt)`:

```lua
function crayon.update(dt)
    local ui = crayon.simpleUI
    if not ui then return end

    ui.setNextWindowPos(20, 20, 2)
    ui.setNextWindowSize(380, 420, 2)

    local isOpen, shouldDraw = ui.beginWindow("Tactical Command", true)
    if shouldDraw then
        ui.textColored(0.2, 0.8, 1.0, 1.0, "TACTICAL RADAR ACTIVE")
        ui.separator()

        if ui.button("Launch Airstrike", 160, 32) then
            crayon.print("Airstrike triggered!")
        end
        if ui.isItemHovered() then
            ui.setTooltip("Orders attack flight to begin bombing run")
        end

        local fov, changed = ui.sliderFloat("FOV", 75.0, 60.0, 110.0)
    end
    ui.endWindow()
end
```

## Complete Lua API Reference

### Windows & Viewports
| Function | Parameters | Returns | Description |
|---|---|---|---|
| `beginWindow(title, [open], [flags])` | string, bool, int | `isOpen, shouldDraw` | Starts an ImGui window |
| `endWindow()` | - | - | Ends current window |
| `beginChild(id, [w, h], [border], [flags])` | string, float, float, bool, int | `shouldDraw` | Sub-window scrollable region |
| `endChild()` | - | - | Ends child region |
| `setNextWindowPos(x, y, [cond])` | float, float, int | - | Sets position of next window |
| `setNextWindowSize(w, h, [cond])` | float, float, int | - | Sets size of next window |
| `setNextWindowBgAlpha(alpha)` | float (0.0 - 1.0) | - | Background transparency |
| `setNextWindowFocus()` | - | - | Brings next window to focus |
| `isWindowHovered()` | - | `bool` | True if window is hovered |
| `isWindowFocused()` | - | `bool` | True if window is focused |
| `getWindowPos()` | - | `x, y` | Current window coordinates |
| `getWindowSize()` | - | `w, h` | Current window dimensions |
| `gameViewport(w, h, [border])` | float, float, bool | `shouldDraw` | Sub-region viewport framing |

### Tabs & Menus
| Function | Parameters | Returns | Description |
|---|---|---|---|
| `beginTabBar(id, [flags])` | string, int | `bool` | Starts a tab container |
| `endTabBar()` | - | - | Ends tab container |
| `beginTabItem(label, [open], [flags])` | string, bool, int | `selected, open` | Single tab item |
| `endTabItem()` | - | - | Ends tab item |
| `beginMenuBar()` | - | `bool` | Menu bar container |
| `endMenuBar()` | - | - | Ends menu bar |
| `beginMenu(label, [enabled])` | string, bool | `bool` | Dropdown menu header |
| `endMenu()` | - | - | Ends dropdown menu |
| `menuItem(label, [shortcut], [selected], [enabled])` | string, string, bool, bool | `clicked, selected` | Clickable menu entry |

### Widgets & Controls
| Function | Parameters | Returns | Description |
|---|---|---|---|
| `button(label, [w, h])` | string, float, float | `bool` | Primary clickable button |
| `smallButton(label)` | string | `bool` | Compact button |
| `invisibleButton(id, w, h)` | string, float, float | `bool` | Invisible interactive hit box |
| `checkbox(label, val)` | string, bool | `val, changed` | Toggle checkbox |
| `radioButton(label, active)` | string, bool | `bool` | Radio button |
| `combo(label, currentIdx, itemsTable)` | string, int, table | `newIdx, changed` | Dropdown combobox (1-indexed) |
| `sliderFloat(label, val, min, max, [fmt])` | string, float, float, float, string | `val, changed` | Float slider bar |
| `sliderInt(label, val, min, max, [fmt])` | string, int, int, int, string | `val, changed` | Integer slider bar |
| `dragFloat(label, val, [spd], [min], [max], [fmt])` | string, float, float, float, float, string | `val, changed` | Draggable float input |
| `dragInt(label, val, [spd], [min], [max], [fmt])` | string, int, float, int, int, string | `val, changed` | Draggable int input |
| `inputText(label, text, [maxLen])` | string, string, int | `text, changed` | Single-line text input |
| `inputTextMultiline(label, text, [w, h], [maxLen])` | string, string, float, float, int | `text, changed` | Multi-line text editor |
| `colorEdit3(label, r, g, b)` | string, float, float, float | `r, g, b, changed` | RGB color picker |
| `colorEdit4(label, r, g, b, a)` | string, float, float, float, float | `r, g, b, a, changed` | RGBA color picker |
| `progressBar(fraction, [w, h], [overlay])` | float, float, float, string | - | Progress bar (0.0 to 1.0) |

### Images & Textures
| Function | Parameters | Returns | Description |
|---|---|---|---|
| `image(texture, w, h, [uv0x, uv0y, uv1x, uv1y], [tintR..A], [borderR..A])` | tex/id, float, float, ... | - | Renders texture image |
| `imageButton(id, texture, w, h, [uv0x, uv0y, uv1x, uv1y], [bgR..A], [tintR..A])` | string, tex/id, float, float, ... | `bool` | Interactive texture button |

### Tooltips, Popups & Modals
| Function | Parameters | Returns | Description |
|---|---|---|---|
| `setTooltip(text)` | string | - | Quick tooltip on hovered item |
| `beginTooltip()` | - | `bool` | Custom multi-widget tooltip |
| `endTooltip()` | - | - | Ends custom tooltip |
| `openPopup(id, [flags])` | string, int | - | Triggers popup to open |
| `beginPopup(id, [flags])` | string, int | `bool` | Starts popup window |
| `beginPopupModal(name, [open], [flags])` | string, bool, int | `open, shouldDraw` | Modal popup (blocks outside input) |
| `endPopup()` | - | - | Ends popup / modal |
| `closeCurrentPopup()` | - | - | Dismisses active popup |
| `isPopupOpen(id)` | string | `bool` | Checks if popup is open |

### Data Tables
| Function | Parameters | Returns | Description |
|---|---|---|---|
| `beginTable(id, columns, [flags], [w, h])` | string, int, int, float, float | `bool` | Starts grid table |
| `endTable()` | - | - | Ends table |
| `tableNextRow([flags], [minHeight])` | int, float | - | Begins next table row |
| `tableNextColumn()` | - | `bool` | Advances to next cell column |
| `tableSetColumnIndex(colIdx)` | int (1-indexed) | `bool` | Jumps to specific column |
| `tableSetupColumn(label, [flags], [initW])` | string, int, float | - | Configures column header |
| `tableHeadersRow()` | - | - | Renders column header row |

### Constants Tables
- **`ui.WindowFlags`**: `None`, `NoTitleBar`, `NoResize`, `NoMove`, `NoScrollbar`, `NoScrollWithMouse`, `NoCollapse`, `AlwaysAutoResize`, `NoBackground`, `NoSavedSettings`, `NoMouseInputs`, `MenuBar`, `NoFocusOnAppearing`
- **`ui.TableFlags`**: `None`, `Resizable`, `Reorderable`, `Hideable`, `Sortable`, `RowBg`, `BordersInnerH`, `BordersOuterH`, `BordersInnerV`, `BordersOuterV`, `BordersH`, `BordersV`, `Borders`
