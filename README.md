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
- Sculpt and paint work on a subdivided mesh and on terrain (Edit mode keeps Vertex / Edge / Face, with terrain tools in the Modeler window). Sculpt brushes include Pinch.

**Skins and textures**

- Texture Shop collects UV maps and lets you make skins (our name for materials). Skins are driven by a shader profile.
- Textures can be assigned.
- Object mode can assign skins onto objects.

## Future Progress

- Installer module and a working build for it.
- Texture Shop: finishing getting UV map collection object database then start making tab sub menu buttons work for uv mapping.
- A possible node system on top of the shader profile. Textureshop should be our go to place: for uving, skinning objects, and texturing a substance like program. Meaning when we want to texture we click uv, then create a skin, then use shader profile profile to adjust skin aka mat, the node should be a main node that also is a shader profile that we could use then add extra nodes n order to manipulate, different way then skin maker. Idk we will see.
- Terrain overhangs, tunnels and holes (the terrain is still one height per column). See wiki. version 1.5 has these added but not quite right it will break gimbal
for other modes like object and edit mode because I hadn't made the terrain tool cursor stop yet once mode change so if you try to make a lake the lake preview will take over as mouse
cursor and refuse to change back. Also even doe finalize says single object into terrain it doesn't embed properly yet. Will probably have to use a Boolean to merge properly at some point but for now Ill test as is until I get all the features for each tool right like mountains need options for duel and triple mountains with sharper peaks instead of smooth. Lake right now only has circular shape so need to make a way to create all kinds of different shapes and other stuff. Right now with the way we skin objects and keep track of terrian verts, we should be able to make over hangs and hole tools as well, just skinning the right side of vertex though as cubes but being a pain at the moment.
- The toolbar Run button.
- 


If window loses focus while tabbing press alt and move camera around then try again. Working on it. 

## Current Release

- **Version:** 1.5
- **Tag:** [v1.5](https://github.com/ravenleeblack/Phoenix_Engine/releases/tag/v1.5)

What is new in 1.5: Sculpt opens the Brushes window like Paint, brush icons are lit spheres with their own glyphs, tooltips on buttons, a Selection Highlight toggle, a procedural octagon sky dome that follows the sun, Unreal-style height fog with sun glow, resizable box fog volumes (cheap and good), with Scene and Game view switches, File > Save All, terrain Edit / Sculpt / Paint (with a new Pinch brush and a cursor-following Grab), a Unity-style gizmo, directional-light driven sky and fog, skin maps with a Shader Profile node, and Terrain Tools (river, tunnel, mountain, lake, ramp, overhang, canyon). Full details in [VERSION.md](VERSION.md).

Version 1.4 added the Edit mode tools (Create Edge, Create Face, Delete Edge, Bevel, Subdivide), more light kinds, skin maps, `.obj` import with unit profiles, and the build and installer tools.

Phoenix is under active development. Public APIs and editor workflows may change between releases. The Phoenix Engine does not using any third party api's or libs besides PURE WIN32 C API and VULKAN API everything is being designed from nothing. So no SDL, GLFW or anything like that involved nor any third party installers. 

## Screenshots

<img width="1386" height="960" alt="edit_mode" src="https://github.com/user-attachments/assets/0f99a75f-c4f1-4bee-9bed-46d54f2be516" />

<img width="1376" height="962" alt="object_mode_assign" src="https://github.com/user-attachments/assets/dcde501b-1c68-44d5-beb4-4b22bf174c51" />

<img width="1353" height="849" alt="light_and_skin" src="https://github.com/user-attachments/assets/554eff26-6526-4cd4-9191-f6c5c957e927" />

<img width="378" height="339" alt="shader_profile" src="https://github.com/user-attachments/assets/8aaccb4b-b5b1-4c43-9e23-942202e4e45d" />

<img width="1280" height="963" alt="paint_sculpt_00" src="https://github.com/user-attachments/assets/8fb461b0-2ced-4298-8c7d-310529451086" />


<img width="1386" height="915" alt="fog_skybox" src="https://github.com/user-attachments/assets/de39fc2a-0494-4400-a670-67f357599a0a" />


<img width="1920" height="1050" alt="paint_sculpt" src="https://github.com/user-attachments/assets/1cbc39b9-80c6-4f39-8070-fd38f17224fd" />

<img width="1520" height="930" alt="texure" src="https://github.com/user-attachments/assets/b7246768-e55d-46f7-b39f-06af85e15e21" />












