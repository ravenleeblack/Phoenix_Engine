# Phoenix Engine Version

**Current version:** 1.5 ([tag v1.5](https://github.com/ravenleeblack/Phoenix_Engine/releases/tag/v1.5))

## What changed in 1.5

### Sculpt and Paint
- Entering Sculpt mode opens the Brushes window in front (next to the Hierarchy) and the Sculpt window next to the Inspector, the same places Paint uses. Switching between Sculpt and Paint keeps the Brushes window open.
- Each mode's own window is brought in front of the other tabs in its dock area when you enter the mode.
- Brush icons in the Brushes window are lit spheres like the skins in Texture Shop. Every sculpt and paint brush has its own color and its own glyph. A ring shows hover and the picked brush.
- Brushes window: rounded section lines, circle hit areas.

### Scene view and Game view
- Selection Highlight button: hides the outline and brackets around the selected object (useful while painting). Saved with the view options.
- Tooltips: a small rounded box with a description comes up when the mouse rests on a button (Scene and Game sub menus, toolbar, mode strip, Sculpt/Paint bars).
- Sky and Fog buttons on the Scene sub menu, and a Game sub menu with its own Sky and Fog buttons.

### Sky and fog
- Procedural sky dome (like Unreal's sky atmosphere): an octagon dome (8 sides, 4 rings) that follows the camera. The color is computed from the sun, so moving the directional light changes it: blue day sky, pale horizon, orange sunset, dark night with stars, haze, a glow and a disc at the sun. Sliders: Brightness and Haze.
- Height fog (Unreal's Exponential Height Fog): density falls off with height and is integrated along the whole view ray. Density, Height, Start, Opacity. The fog takes the sky's horizon color (or your own color) and glows toward the sun (Sun glow, Sun size).
- Fog volumes are boxes: position = center, rotation = orientation, scale = size in meters. Resize them with the Scale tool. The box is drawn as a cyan wire box in the Scene view. Creating one fills it with fog. Each volume has a type:
  - Cheap: one straight pass through the box with a soft edge. Fastest.
  - Good: ray marched through the box with moving noise, thicker toward the bottom, lit by the sun with a little self shadow. Looks like cloud.
  Per volume: density, edge softness, noise size, color.
- Toolbar buttons create the sky dome and fog volumes. Preferences has a Sky / Fog tab. The Scene and Game views each have their own Sky and Fog switch. Fog is applied in the mesh shader (`mesh_frag.glsl`), also when Lit is off.

### Mode strip icons
- Sculpt, Paint and Texture Shop in the mode strip are lit spheres like skins with an S, P or T over them.
### File > Save All
- Save Scene, Save Project and Save All now work: they save preferences and view state, every skin to `assets/skins`, and the paint map of every painted mesh. Scenes have no file format yet.

### Lighting
- The directional light is the source of everything: sky color, ambient and fog in-scattering follow its direction, color and intensity. Warm and cool shift the sun color, and fog reflects (scatters) light instead of blending in.
- Light and camera icons and gizmos are hidden in the Game view.

### Terrain editing (3D heightmap)
- Terrain is editable again in Edit, Sculpt and Paint mode. The terrain streaming update now runs in the editor, so edited chunks rebuild.
- Edit mode keeps Vertex / Edge / Face. The control grid is one chunk (100x100 cells) drawn on the ground; clicking terrain remembers the chunk, Ctrl+F frames it. Extrude pulls the selected vertices up by the extrude distance.
- Modeler window terrain tools: Raise, Lower, Smooth, Flatten, Set Height.
- Clicking a gizmo handle in Edit mode no longer selects the quads behind it.
- Sculpt brushes on terrain each do their own thing: Standard (raise), Clay Buildup (layers up to a flat top), Clay Soften, Carve, Grab, Polish, Inflate, Smooth, Flatten, and the new **Pinch** (pulls the ground in toward the middle of the brush; Ctrl spreads it out). Pinch also works on meshes.
- Sandbox terrain (whole cubes): speed follows Strength, and Smooth / Soften / Polish settle columns on their neighbors' average so they work on cubes.
- Grab holds the ground under the cursor and moves it straight up or down with the mouse, from the heights saved at the start of the stroke, so it can not drift or flip.
- The Invert button in the menu bar flips raise / lower for the sculpt brushes. Paint colors no longer get converted to sRGB twice.
- Paint workflow: the Paint window has Export / Import / Clear for the terrain paint map (`assets/textures/terrain_paint.png`, one pixel per column). The default Terrain skin is built in and already applied.
- Skin Maker uses the same color picker and saved-color palette as Paint.

### Gizmos
- Unity style transform gizmo: solid arrow heads, cube scale handles, X red / Y green / Z blue, yellow on hover, a bigger grab area at the tip. Corner orientation widget like Unity's. The gizmo moves on all axes (no Y-only limit) so overhang work is possible.

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
