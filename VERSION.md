# Phoenix Engine Version

**Current version:** 1.4 ([tag v1.4](https://github.com/ravenleeblack/Phoenix_Engine/releases/tag/v1.4))

## What changed in 1.4

### All modules

- Lighting: directional, point, spot, hemisphere, and area light kinds. Lights have color, intensity, range, cone, softness, and warmth. A directional light object drives the sun.
- Skins: specular, height, and ambient-occlusion maps packed into one auxiliary texture, with specular, gloss, height scale, and ambient controls.
- PNG textures on skins; right mouse button reaches the context menu; the start camera resets when it begins near the origin.
- New Edit mode tools: Create Edge, Create Face, Delete Edge, Bevel, Subdivide. Multicut keeps the gizmo usable when Ctrl is not held.

### Engine (`07_system_engine`, [engine.md](engine.md))

- Light runtime: light kinds, `apply_light_warmth`, and a sun driven by the scene light object.
- Skin GPU runtime and mesh shader (`mesh_frag.glsl`) read the extra skin maps.
- Grid, terrain, offscreen scene, and Vulkan backend/pipeline fixes.

### Editor (`03_system_editor`, [editor.md](editor.md))

- Preferences: unit profiles (Phoenix, Maya, Unity, ZBrush) for import and export.
- Inspector, light window, Texture Shop, skin maker, toolbar, menu bar, and archive updates.
- Docking fixes and scene drawing updates.

### Loader (`06_system_loader`, [Phoenix_Engine.md](Phoenix_Engine.md))

- `.obj` import added next to `.fbx` and `.png`.
- Unit-aware load and save (`load_3d_mesh_asset_scaled`, `save_3d_mesh_asset_scaled`); meshes are always meters inside the engine.

### Game (`08_system_game`, [game.md](game.md))

- No API changes. The game DLL is rebuilt against the updated engine.

### Build tools

- New `10_system_build` (`phoenix_build.exe`) turns `01_bin` into a game folder without the editor.
- New `09_system_installer` (`phoenix_installer.exe`) installs that folder.
- Makefile: new `build` and `installer` targets, both part of `make all`. `01_bin/game_build/` is git-ignored.

## Earlier versions

| Version | Summary |
| --- | --- |
| 1.3 | FBX/PNG import and export, export options window, Assign Skin from archive, focus handling |
| 1.2 | Preferences window, menu bar dropdown overlay, scene view fixes |
| 1.1 | First tagged release |
