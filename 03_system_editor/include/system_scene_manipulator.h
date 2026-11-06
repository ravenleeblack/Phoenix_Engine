#ifndef PHOENIX_SYSTEM_MANIPULATOR_H
#define PHOENIX_SYSTEM_MANIPULATOR_H

#include "engine_scene_manipulator.h"

#endif

#if 0
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include "system_element.h"   /* system_element_context */
#include "system_math.h"      /* editor_vec3 */
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

/* ------------------------------------------------------------------------- */
/* Scene view tool box (top right corner) + Show Manipulator + snapping +    */
/* edit mode tools. One state struct, so the context menu, the tool box and   */
/* the transform gizmo all read the same values.                              */
/* ------------------------------------------------------------------------- */
typedef enum {
    manip_snap_off = 0,
    manip_snap_point_to_point,     /* snaps to the pivot of the nearest other scene object */
    manip_snap_point_to_grid       /* snaps to multiples of grid_size */
} manip_snap_mode;

typedef enum {
    manip_edit_none = 0,
    manip_edit_target_weld,        /* click a vertex, then the vertex it should merge into */
    manip_edit_multicut,           /* click points on edges/faces, Enter to cut (not wired yet) */
    manip_edit_insert_loop         /* hover a row, wheel = number of rows, drag = slide, release = cut (system_edit_ops.c) */
} manip_edit_tool;

typedef struct {
    bool            show_manipulator;     /* Maya "Show Manipulator" (T). replaces Move/Rotate/Scale while on */
    manip_snap_mode snap;
    float           grid_size;            /* world units */
    float           point_snap_radius;    /* world units: how close before point to point grabs */
    manip_edit_tool edit_tool;            /* edit mode only */
} manip_state;

extern manip_state g_manip;

/* ---- state changes (called by the context menu and the tool box) ---- */
void manip_set_show(system_element_context* ui_element, bool on);   /* on = also drops Move/Rotate/Scale so only one manipulator shows */
void manip_set_snap(manip_snap_mode mode);
void manip_scale_grid(float factor);                        /* 2.0 = bigger grid, 0.5 = finer */
void manip_toggle_edit_tool(manip_edit_tool tool);          /* picking the active one again turns it off */

/* ---- snapping. Feed every dragged position through this. ----
   Move gizmo:  obj->position = manip_snap_position(new_position, obj->id);
   Returns p untouched while snap is off. */
editor_vec3 manip_snap_position(editor_vec3 p, uint32_t moving_object_id);

/* ---- drawing / input: call once per frame from the Scene view draw ---- */
void draw_scene_manipulator_overlay(VkCommandBuffer cmd, system_element_context* ui_element, int view_x, int view_y, int view_w, int view_h);

/* True while the cursor is over the tool box / manipulator panel or a field drag is running.
   Use it to stop viewport click picking from deselecting the object. */
bool manip_overlay_wants_mouse(void);

/* ---- Target Weld core, for triangle index buffers ----
   Every use of vertex `src` becomes `dst`, triangles that collapse are removed. *index_count shrinks.
   The orphaned src vertex stays in the vertex buffer until you compact it. */
void manip_weld_indices(uint32_t* indices, uint32_t* index_count, uint32_t src, uint32_t dst);

#endif