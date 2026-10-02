# Crayon Project Manager (editor.exe)

A lightweight project manager / creator / runner for `.crayonproj` games.

- **New Project**: name, author, location, template (Empty / 2D / 3D) -> creates `.crayonproj`, `src/main.lua`, `assets/`
- **Open Project**: pick any folder (or file) containing a `.crayonproj`
- **Overview**: edit the manifest (name, version, author, license, entry script, description)
- **Files**: browse, create, edit, save and delete project files
- **Run / Stop**: launches the separate `crayon.exe` player on the project; output shows in the Console tab

Shortcuts: F5 run, Shift+F5 stop, Ctrl+S save, Ctrl+N new, Ctrl+O open.
Set the player path under File > Settings (auto-detected next to editor.exe by default).
Build: `build_editor.bat` (editor) and `build_crayon.bat` (player). `/src` is unchanged.
