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
- Docking system: windows can be moved into different areas. Drag-to-reorder tabs is currently broken.

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

## In Progress

- Installer module and a working build for it.
- Texture Shop: finishing UV map collection.
- A possible node system on top of the shader profile.
- Sculpt and paint on terrain.
- Drag-to-reorder for docked tabs.
- The toolbar Run button.

## Current Release

- **Version:** 1.4
- **Tag:** [v1.4](https://github.com/ravenleeblack/Phoenix_Engine/releases/tag/v1.4)

What is new in 1.4: new Edit mode tools (Create Edge, Create Face, Delete Edge, Bevel, and Subdivide at 2, 1/4, 1/8, and 1/16), and a Multicut that keeps the gizmo usable when Ctrl is not held.

**In progress:** version 1.5 is in development.

Phoenix is under active development. Public APIs and editor workflows may change between releases.

## Screenshots

<img width="1376" height="878" alt="sceneview" src="https://github.com/user-attachments/assets/cf55da02-cffe-4e2e-928d-d34a328fd91a" />

<img width="1254" height="951" alt="right_click_menu" src="https://github.com/user-attachments/assets/397589ab-dcd9-447c-9acc-623bf6806535" />

<img width="1353" height="849" alt="light_and_skin" src="https://github.com/user-attachments/assets/8dc70c3a-6231-42ca-93f4-34b4bd6d58c9" />

<img width="378" height="339" alt="shader_profile" src="https://github.com/user-attachments/assets/30d71d96-45dd-491e-8216-f8ad3508c1bf" />
