#ifndef phoenix_engine_scene_delete_h
#define phoenix_engine_scene_delete_h

#include "engine_defines.h"
#include "engine_element.h"
#include "engine_scene_objects.h"

/*
RULES THAT MUST BE FOLLOWED.
uncomment as we start getting things fixed: 
Must use lowercase seperated by underscores. Even structs names, enum names, etc.
Must use verbs before nouns when naming anything
Must not use ternerys. i fucking hate ternery operators.
Must use braced control-flow headers: every if, else if, and else must use braces.
Must call extern in engine_globals.h and init in engine_globals.c
Must not have statics in any file. I fucking hate statics.
No static structs, No static enums, No static anything.
Math functions must go in math file and header.
Drawing stuff must go into drawing stuff and headers.
ALl information gets routed into statusbar.

NO CAPITAL LETTERS UNLESS WINDOWS OR VULKAN DEMANDS IT.
DO NOT REMOVE THIS COMMENT EVER. THESE ARE THE RULES.
IF SOMETHING IS BREAKING THESE RULES CHANGE IT THEN WE CHANGE EVERYTHING CONNECTED AS WE GO.
THIS INCLUDES INSIDE THE SOURCE FILES NOT JUST HEADERS
*/

/* Deletes the object AND everything parented under it: unlinks it from the hierarchy,
   queues its GPU buffers for safe destruction, drops its edit mesh, compacts the registry
   and fixes every parent / child / sibling pointer. Returns false for the root and the terrain. */
phoenix_engine_api bool delete_scene_object(uint32_t id);

/* helpers (were static) */
phoenix_engine_api void collect_scene_subtree(const system_scene_object* o, uint32_t* ids, int* count, int capacity);
phoenix_engine_api void unlink_scene_object(system_scene_object* o);
phoenix_engine_api bool remove_scene_object(uint32_t id);

/* Delete key, Object mode only, cursor inside x y w h. (was system_scene_delete_hotkey) */
phoenix_engine_api void update_scene_delete_hotkey(system_element_context* ui_element, int x, int y, int w, int h);

#endif