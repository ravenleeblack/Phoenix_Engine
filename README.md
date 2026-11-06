# Phoenix Engine

Phoenix is a pure-C, Vulkan-powered game engine and 3D content-authoring environment built for Windows.

<p>
  <a href="https://github.com/ravenleeblack/Phoenix_Engine/wiki">Documentation Wiki</a>
  ·
  <a href="https://github.com/ravenleeblack/Phoenix_Engine/releases">Releases</a>
  ·
  <a href="Phoenix_Engine.md">Project Overview</a>
</p>

## Welcome

Phoenix combines a native runtime with modeler, sculpting, painting, scene editing, asset-loading, docking, and game-development tools.

The complete documentation lives in the [Phoenix Engine Wiki](https://github.com/ravenleeblack/Phoenix_Engine/wiki):

- [Getting Started](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Getting-Started)
- [Engine](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Engine)
- [Editor](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Editor)
- [Game Module](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Game-Module)
- [Building and Cleaning](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Building-and-Cleaning)
- [Architecture](https://github.com/ravenleeblack/Phoenix_Engine/wiki/Architecture)

## Architecture

| Component | Output | Responsibility |
| --- | --- | --- |
| Engine | `phoenix_engine.exe` | Windowing, Vulkan, runtime scene, rendering, services, plugin loading |
| Editor | `phoenix_editor.dll` | UI, docking, modeler, sculpting, painting, scene tools, authoring |
| Game | `system_game.dll` | Gameplay, simulation, and project-specific runtime behavior |
| Loader | `phoenix_loader.dll` | Native image and 3D asset import/export support |

## Quick Build

Requirements include Windows, Vulkan-capable hardware, the Vulkan SDK, and MinGW-w64 or the supported native build environment.

```bash
make all
```

For focused work:

```bash
make engine
make editor
make loader
make game
```

Run the executable from its output directory:

```powershell
cd 01_bin
.\phoenix_engine.exe
```

Clean only one module while preserving normal editor preferences:

```bash
make clean_engine
make clean_editor
make clean_loader
make clean_game
```

See [Phoenix Engine Wiki](https://github.com/ravenleeblack/Phoenix_Engine/wiki) for setup, architecture, module ownership, controls, debugging, and development guidance.

## Repository Documentation

- [Phoenix Engine overview](Phoenix_Engine.md)
- [Engine module](engine.md)
- [Editor module](editor.md)
- [Game module](game.md)

## Current Release

- **Version:** 1.1
- **Tag:** [v1.1](https://github.com/ravenleeblack/Phoenix_Engine/releases/tag/v1.1)

Phoenix is under active development. Public APIs and editor workflows may change between releases.
