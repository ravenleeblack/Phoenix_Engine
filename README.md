# Phoenix Engine

Phoenix is a pure-C, Vulkan-powered game engine and 3D content-authoring environment built for Windows.

<p>
  <a href="https://github.com/ravenleeblack/Phoenix_Engine/wiki">Documentation Wiki</a>
  ·
  <a href="https://github.com/ravenleeblack/Phoenix_Engine/releases">Releases</a>
</p>

## Editor first

Most engines start with the runtime and add an editor later. Phoenix went the other way: the editor's UI handling came first, before the engine was built around it.

The result is that Phoenix has real working views from the start:

- **Scene view**: a real viewport you build and edit in, with its own editor camera, gizmos, and edit-mode overlays.
- **Game view**: a real viewport that renders through the game camera, so you see how the game will actually look while you work.

Modeling, sculpting, painting, terrain, lighting, and skins all happen inside these views, not in separate tools.

## Documentation

Everything else lives in the [Phoenix Engine Wiki](https://github.com/ravenleeblack/Phoenix_Engine/wiki):

| Page | What it covers |
| --- | --- |
| [Getting Started](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Getting-Started) | Requirements, first build, first launch |
| [Editor](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Editor) | Modes, edit tools, import and export, skins |
| [Engine](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Engine) | The runtime host and Vulkan implementation |
| [Game Module](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Game-Module) | Gameplay code and its lifecycle |
| [Building and Cleaning](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Building-and-Cleaning) | Every make target |
| [Architecture](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Architecture) | How the modules fit together |

## What Has Been Done

**Interface**

- Menu bar with dropdown menus that work as buttons, and a dynamic menu that changes with the current mode.
- Mode menu bar that changes modes and opens the windows each mode needs.
- Toolbar buttons all work except Run, for now.
- Status bar showing program info.
- Tab sub-menu options.
- Preferences window, with universal theme settings that apply everywhere and can be saved.
- Docking system: windows can be moved into different areas. Drag-to-reorder tabs.

**Right-click menus**

- Over the Scene view: change modes and pick tools.
- Over the File Archive: create and delete files and folders.

**Scene**

- Scene view and Game view, each with its own camera.
- Cube object and terrain (terrain is still buggy).
- Light objects and camera objects. The camera object is separate from the Scene view camera.
- Wireframe and textured shading modes.
- Some anti-aliasing (MSAA and FXAA).
- 60-step undo / redo.

**Importing**

- Import `.fbx`, `.obj`, and `.png`. Imported files go into the File Archive, which is Phoenix's version of an asset folder.

**Modeling**

- Edit mode: extrude, multicut, target weld, multiple select, and the 1.4 tools (create edge, create face, delete edge, bevel, subdivide).
- Sculpt and paint work on a cube once it is subdivided. They do not work on terrain yet.

**Skins and textures**

- Texture Shop collects UV maps and lets you make skins (our name for materials). Skins are driven by a shader profile.
- Textures can be assigned.
- Object mode can assign skins onto objects.

## Future Progress

- Installer module and a working build for it.
- Texture Shop: finishing getting UV map collection object database then start making tab sub menu buttons work for uv mapping.
- A possible node system on top of the shader profile. Textureshop should be our go to place: for uving, skinning objects, and texturing a substance like program.
- Sculpt and paint on terrain. Terrain right now is a true 3D Heightmap will should allow vertical tunnel, trenches, overhangs, holes. See wiki.
- Getting sculpt and paint icons to look more like skins icons.
- The toolbar Run button.
- skybox system with volume for fog since we have lighting.
- 


If window loses focus while tabbing press alt and move camera around then try again. Working on it. 

## Current Release

- **Version:** 1.5
- **Tag:** [v1.5](https://github.com/ravenleeblack/Phoenix_Engine/releases/tag/v1.5)

What is new in 1.5: Sculpt opens the Brushes window like Paint, brush icons are lit spheres with their own glyphs, tooltips on buttons, a Selection Highlight toggle, an octagon sky dome, Unreal-style height fog with sun glow and soft fog volumes (with Scene and Game view switches), and File > Save All. Full details in [VERSION.md](VERSION.md).

Version 1.4 added the Edit mode tools (Create Edge, Create Face, Delete Edge, Bevel, Subdivide), more light kinds, skin maps, `.obj` import with unit profiles, and the build and installer tools.

Phoenix is under active development. Public APIs and editor workflows may change between releases.

## Screenshots

<img width="1376" height="878" alt="sceneview" src="https://github.com/user-attachments/assets/cf55da02-cffe-4e2e-928d-d34a328fd91a" />

<img width="1254" height="951" alt="right_click_menu" src="https://github.com/user-attachments/assets/397589ab-dcd9-447c-9acc-623bf6806535" />

<img width="1353" height="849" alt="light_and_skin" src="https://github.com/user-attachments/assets/8dc70c3a-6231-42ca-93f4-34b4bd6d58c9" />

<img width="378" height="339" alt="shader_profile" src="https://github.com/user-attachments/assets/30d71d96-45dd-491e-8216-f8ad3508c1bf" />

<img width="1920" height="1050" alt="paint_sculpt" src="https://github.com/user-attachments/assets/91e8852c-89ee-48ee-ac01-8a2f5cdb0259" />

