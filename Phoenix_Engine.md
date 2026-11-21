# Phoenix Engine

Phoenix Engine is a pure C and Vulkan game, rendering, and content-authoring platform. Its executable hosts the window, Vulkan renderer, runtime scene, plugin loading, and application lifecycle. Editor and game functionality are loaded as separate DLLs so authoring tools and gameplay code can be developed independently.

## Modules

| Output | Purpose |
|---|---|
| `01_bin/phoenix_engine.exe` | Engine host, window, Vulkan runtime, scene state, and module loader |
| `01_bin/phoenix_editor.dll` | 3D modeling, sculpting, painting, scene UI, UV and material authoring, lighting, terrain, and docking |
| `01_bin/system_game.dll` | Project gameplay and simulation code |
| `01_bin/phoenix_loader.dll` | Mesh (`.fbx`, `.obj`) and image (`.png`) asset loading |
| `01_bin/phoenix_build.exe` | Turns `01_bin` into a game folder without the editor |
| `01_bin/phoenix_installer.exe` | Installs a built game folder |

Current version: 1.4. See [VERSION.md](VERSION.md) for what changed.

## User Guide

### Requirements

- A Vulkan-capable 64-bit GPU and driver
- Windows 64-bit
- MinGW-w64 cross-compilation tools when building from WSL
- Vulkan SDK for shaders and Vulkan development libraries

### Running Phoenix

The executable must be run from `01_bin/`, so its companion DLLs and saved preferences can be found next to it.

```powershell
cd 01_bin
.\phoenix_engine.exe
```

Or press `F5` in VS Code using the `Phoenix Editor` launch configuration.

### Building

Run commands from the workspace root:

```bash
make all
make engine
make editor
make game
make loader
```

`make all` builds the engine, editor, loader, game module, and shaders. Module targets rebuild the dependencies needed by the selected output.

### Cleaning Builds

```bash
make clean_engine
make clean_editor
make clean_game
make clean
make clean_prefs
make clean_all
```

Normal `make clean` preserves editor preferences. `make clean_prefs` removes them, and `make clean_all` removes builds and preferences.

### Editor Preferences

Editor preferences and persistent view state are stored in:

```text
01_bin/phoenix_editor.pdb
```

This file is preserved by normal clean builds. Use `make clean_prefs` only when the saved editor state should be reset.

### Intended Workflow

1. Build the engine and editor.
2. Launch Phoenix from VS Code or `01_bin`.
3. Use the Scene, Object, Model, Sculpt, Paint, Lighting, Terrain, and inspector tools in the editor.
4. Keep engine/runtime concerns in the engine.
5. Keep authoring tools and UI in the editor.
6. Keep gameplay behavior in the game DLL.

See [engine.md](engine.md), [editor.md](editor.md), and [game.md](game.md) for module details.
