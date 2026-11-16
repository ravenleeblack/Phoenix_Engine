# Phoenix Editor Module

## Role

`03_system_editor/` is the authoring application. Phoenix builds it as `01_bin/phoenix_editor.dll`.

The editor currently provides:

- 3D scene hierarchy and object manipulation
- Object, Edit, Sculpt, and Paint modes
- Gizmos and transform tools
- Mesh selection and edit operations
- Sculpt and brush input
- Terrain editing
- Lighting controls and scene lighting
- Docking and floating editor UI
- Inspector, modeler, hierarchy, and scene controls
- Texture Shop and node-based texture work
- UV, paint-map, and skin UI foundations
- Editor preference and view-state persistence
- File > Import / Export: `.fbx` models and `.png` images through the loader, with an export options window
- Object-mode right-click Skin menu: assign the `.sprof` selected in the File Archive to the selected object
- Window focus: the scene view keeps the keyboard after a click; hotkeys and held keys are ignored while another app has focus

## Import and Export

File > Import opens the Windows file dialog.

- `.fbx` (binary or ASCII, mesh geometry only) is copied to `assets/models` and added to the scene.
- `.png` is copied to `assets/textures` and a skin is created from its average color in `assets/skins`.

File > Export opens an options window: export the selected object or the selected archive file, as FBX binary, FBX ASCII, or PNG. Export... then opens the save dialog. Objects are written in object space, in meters.

The editor loads `phoenix_loader.dll` at run time (`06_system_loader/`), so the editor itself links nothing extra.

## Assign Skin

Select a `.sprof` in the File Archive, select an object, then right-click in the scene view (object mode) > Skin > Assign Skin from Archive. Textureshop does not need to be open.

## Source Layout

```text
03_system_editor/source/system_editor_system/
03_system_editor/source/system_input_system/
03_system_editor/source/system_mode_system/
03_system_editor/source/system_object_mode_editor/
03_system_editor/source/system_scene_system/
03_system_editor/source/system_edit_mode_editor/
03_system_editor/source/system_window_system/
03_system_editor/include/
```

## Editor Plugin ABI

The engine loads the editor through exported functions named:

- `editor_load_settings`
- `editor_save_settings`
- `editor_start`
- `editor_update`
- `editor_begin_frame_pass`
- `editor_draw_ui`
- `editor_end_frame_pass`
- `editor_handle_message`
- `editor_shutdown`

The editor also exports scene-offscreen callbacks and `create_scene_object_pipeline`.

## Ownership Rules

Keep these systems in the editor:

- UI layout, dock windows, menus, and inspector panels
- Authoring selection and operation history
- Sculpt, paint, UV, skinning, and procedural texture workflows
- Scene-object placement tools that exist only for editing
- Editor camera controls and editor view state
- Commit logic that uploads edited runtime data to the engine

Keep only shared runtime state and GPU consumption in the engine.

## Startup Flow

1. The engine loads `phoenix_editor.dll`.
2. The engine resolves the editor ABI symbols.
3. Editor preferences are loaded before Vulkan startup.
4. The renderer creates editor-owned offscreen resources.
5. The editor initializes its UI and databases.
6. Each frame, the engine updates and draws the editor.

## Building

```bash
make editor
make clean_editor
make editor
```

The editor links against the engine import library, so rebuild the engine before rebuilding an editor when its ABI or exported runtime API changes.

## Saved State

Preferences and editor state are written to `01_bin/phoenix_editor.pdb`. Normal `make clean` keeps this file; only `make clean_prefs` or `make clean_all` removes it.
