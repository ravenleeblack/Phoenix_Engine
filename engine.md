# Phoenix Engine Module

## Role

`07_system_engine/` is the native runtime host and Vulkan implementation. Phoenix builds it as `01_bin/phoenix_engine.exe`.

The executable owns:

- Application startup and shutdown
- Native Windows window creation
- Vulkan instance, device, swapchain, render passes, pipelines, buffers, and presentation
- The world and scene registry
- Runtime cameras and scene transforms
- Core objects, terrain runtime, lighting runtime, and object databases
- Shared math and memory systems
- Engine status logging
- Dynamic loading of editor, loader, and game DLLs
- Plugin function-table resolution

## Source Layout

Common engine areas include:

```text
07_system_engine/source/system_core_engine/
07_system_engine/source/system_load_engine/
07_system_engine/source/system_render_engine/
07_system_engine/source/system_scene_engine/
07_system_engine/source/system_database_engine/
07_system_engine/include/
```

Header names are canonicalized here when a type or API is shared between modules.

## Plugin ABI

The engine resolves the editor and game modules at runtime. The expected editor ABI is declared in `07_system_engine/include/engine_main.h`, including settings, frame, UI, window-message, and offscreen callbacks.

The engine should define a stable ABI rather than require plugins to depend on every internal implementation symbol. Shared runtime data must remain deliberately small and documented.

## Ownership Rules

Keep these systems in the engine:

- Rendering and Vulkan resource lifetime
- Runtime scene graph and object identity
- GPU mesh uploads and GPU resource retirement
- Runtime lighting state consumed by rendering
- Memory and logging services
- Plugin discovery and loading
- Serialization APIs shared with multiple modules

Keep authoring operations, docking, inspectors, brushes, edit tools, and editor state in the editor.

## Building

```bash
make engine
make clean_engine
make engine
```

The engine writes `02_obj/libphoenix_engine.a` for compatible module linking. The executable also exports runtime symbols used by plugin DLLs.

## Running and Debugging

VS Code launches `01_bin/phoenix_engine.exe` and runs the default `make all` build before launch. Set the working directory to `01_bin` so the executable loads modules beside it.
