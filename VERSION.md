# Phoenix Engine Version

**Current version:** 1.5 ([tag v1.5](https://github.com/ravenleeblack/Phoenix_Engine/releases/tag/v1.5))

## What changed in 1.5

### Sculpt and Paint
- Entering Sculpt mode opens the Brushes window in front (next to the Hierarchy) and the Sculpt window next to the Inspector, the same places Paint uses. Switching between Sculpt and Paint keeps the Brushes window open.
- Each mode's own window is brought in front of the other tabs in its dock area when you enter the mode.
- Brush icons are lit spheres like the skins in Texture Shop. Every sculpt and paint brush has its own color and its own glyph. A ring shows hover and the picked brush. The Sc and Pt buttons in the mode strip use the same icons.
- Brushes window: rounded section lines, circle hit areas.

### Scene view and Game view
- Selection Highlight button: hides the outline and brackets around the selected object (useful while painting). Saved with the view options.
- Tooltips: a small rounded box with a description comes up when the mouse rests on a button (Scene and Game sub menus, toolbar, mode strip, Sculpt/Paint bars).
- Sky and Fog buttons on the Scene sub menu, and a Game sub menu with its own Sky and Fog buttons.

### Sky and fog
- Sky dome: an octagon dome (8 sides, 4 rings) that follows the camera. Gradient from the horizon to the top color, darker below the horizon, glow and disc at the sun. The horizon takes the fog color when fog is on.
- Fog: exponential height fog integrated along the view ray (density, height falloff, start distance, max opacity), with sun glow (directional inscattering).
- Fog volumes: Empty objects. Position = center, largest scale = radius. Each is a soft sphere, thickest in the middle, with its own density and color.
- Toolbar buttons create the sky dome and fog volumes. Preferences has a new Sky / Fog tab. All sky and fog settings are saved.
- Fog is applied in the mesh shader (`mesh_frag.glsl`), also when Lit is off.

### File > Save All
- Save Scene, Save Project and Save All now work: they save preferences and view state, every skin to `assets/skins`, and the paint map of every painted mesh. Scenes have no file format yet.

### Build fixes
- `system_editor_plugin.c` was missing the `draw_preferences_overlay` declaration (it was already broken in 1.4 but only showed up when the file was rebuilt).
- The makefile does not track header changes. After a header change run `make clean` (it keeps the preferences) and `make all`.

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
