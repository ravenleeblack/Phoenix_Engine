#ifndef PHOENIX_SYSTEM_SCENE_DELETE_H
#define PHOENIX_SYSTEM_SCENE_DELETE_H

#include <stdbool.h>
#include <stdint.h>
#include "system_element.h"
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
bool delete_scene_object(uint32_t id);

/* Delete key in Object mode, only while the mouse is over the given panel rectangle.
   Call from the Hierarchy window and the Scene window. */
void system_scene_delete_hotkey(system_element_context* ui_element, int x, int y, int w, int h);

#endif