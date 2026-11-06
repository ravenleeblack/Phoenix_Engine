# Phoenix Game Module

## Role

`08_system_game/` contains project gameplay code and builds as `01_bin/system_game.dll`.

The game DLL owns project-specific simulation, game state, and gameplay behavior. It must depend on engine runtime APIs through the engine module boundary rather than reach into editor implementation files.

## Source Layout

```text
08_system_game/source/system_core_game/
08_system_game/source/system_game_management/
08_system_game/include/system_core_game/
08_system_game/include/system_state_game/
08_system_game/include/system_game_management/
```

## Example Lifecycle

The game entry currently supplies a `game_state` with callbacks for:

- Allocation and startup
- Per-frame update
- Shutdown

The engine creates this state, calls its startup function, updates the game during the engine loop, and shuts it down during application shutdown.

## Ownership Rules

Keep in the game module:

- Player and AI behavior
- Gameplay rules and scoring
- Project-specific simulation
- Gameplay state machines
- Game-only input interpretation
- Project-specific scene spawning and transitions

Use engine services for:

- Window and input plumbing
- Vulkan rendering
- Runtime scene objects
- Memory allocation
- Logging
- Asset loading
- Physics or shared simulation services once they are established

Do not put editor UI, docking, brushes, or asset-authoring tools in the game DLL.

## Building

```bash
make game
make clean_game
make game
```

The game DLL links against the engine import library. The engine executable and game DLL use the same 64-bit MinGW ABI.

## Adding Gameplay

1. Add gameplay headers under `08_system_game/include/`.
2. Add implementation files under `08_system_game/source/`.
3. Keep callbacks and shared data explicit.
4. Build with `make game`.
5. Launch through `phoenix_engine.exe`, which loads the DLL from `01_bin/`.
