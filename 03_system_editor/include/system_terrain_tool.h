#ifndef PHOENIX_SYSTEM_TERRAIN_TOOL_H
#define PHOENIX_SYSTEM_TERRAIN_TOOL_H

#include <stdbool.h>
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

/* Terrain editing the way Unity and Unreal do it: a brush ring on the ground, drag to change the heights. The terrain is a height
   grid, so the tools only move Y (that is the one difference from a normal mesh).
   Edit mode with the terrain selected has the normal Vertex / Edge / Face tools (the control grid, with a Y-only gizmo) and, when one of
   these tools is picked in the Modeler window, a brush over the whole terrain instead. Sculpt and Paint mode use the Brushes, Sculpt and Paint
   windows. All three go through update_terrain_brush (system_terrain_brush.c). */

typedef enum {
    terrain_tool_off = 0,         /* no brush: Edit mode selects vertices, edges and faces of the control grid like any mesh */
    terrain_tool_raise,           /* push the ground up (Ctrl lowers) */
    terrain_tool_lower,           /* push the ground down (Ctrl raises) */
    terrain_tool_smooth,          /* average each column with its neighbors */
    terrain_tool_flatten,         /* pull toward the height where the stroke started */
    terrain_tool_set_height,      /* pull toward the Height value below */
    terrain_tool_count
} enum_terrain_tool;

typedef struct {
    int   tool;                   /* enum_terrain_tool */
    float radius_cells;           /* brush size in cubes (a cube is 4 m) */
    float strength;               /* 0..1 */
    float hardness;               /* 0..0.95: how much of the radius is full strength */
    float height_cells;           /* the Set Height target, in cubes */
} terrain_tool_state;

extern terrain_tool_state g_terrain_tool;

const char* get_terrain_tool_name(int tool);

#endif
