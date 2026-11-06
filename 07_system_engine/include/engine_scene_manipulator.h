#ifndef phoenix_engine_scene_manipulator_h
#define phoenix_engine_scene_manipulator_h

#include "engine_defines.h"
#include "engine_math.h"
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

/* Scene view tool box (top right corner) + Show Manipulator + snapping + edit mode tools.
   One state struct, so the context menu, the tool box and the transform gizmo all read the same values.
   g_manip, g_manip_frame and the number field option tables are extern in engine_globals.h. */

enum {
    mo_margin  = 8,
    mo_panel_w = 340,
    mo_row_h   = 20,
    mo_label_w = 40,
    mo_field_w = 96,
    mo_sep_gap = 10,
    mo_char_w  = 8
};

#define manip_field_id_base 0x4D4E0000u     /* ids for number_field: 9 transform boxes + 3 size boxes */

typedef enum {
    manip_snap_off = 0,
    manip_snap_point_to_point,     /* snaps to the pivot of the nearest other scene object */
    manip_snap_point_to_grid       /* snaps to multiples of grid_size */
} manip_snap_mode;

typedef enum {
    manip_edit_none = 0,
    manip_edit_target_weld,        /* click a vertex, then the vertex it should merge into */
    manip_edit_multicut,           /* click points on edges/faces, Enter to cut (not wired yet) */
    manip_edit_insert_loop         /* hover a row, wheel = number of rows, drag = slide, release = cut */
} manip_edit_tool;

typedef struct {
    bool            show_manipulator;     /* Maya "Show Manipulator" (T). replaces Move/Rotate/Scale while on */
    manip_snap_mode snap;
    float           grid_size;            /* world units */
    float           point_snap_radius;    /* world units: how close before point to point grabs */
    manip_edit_tool edit_tool;            /* edit mode only */
} manip_state;

/* per frame input edges (was file statics). computed once, however many scene views draw */
typedef struct {
    uint32_t frame;
    bool     edge;          /* left button went down this frame */
    bool     prev_down;
    bool     prev_t;
    bool     prev_esc;
    bool     over;          /* cursor over the tool box / panel this frame */
} manip_frame_state;

/* ---- state changes (called by the context menu and the tool box) ---- */
phoenix_engine_api void set_manip_show(system_element_context* ui_element, bool on);
phoenix_engine_api void set_manip_snap(manip_snap_mode mode);
phoenix_engine_api void scale_manip_grid(float factor);                        /* 2.0 = bigger grid, 0.5 = finer */
phoenix_engine_api void toggle_manip_edit_tool(manip_edit_tool tool);          /* picking the active one again turns it off */

/* ---- snapping. Feed every dragged position through this. Returns p untouched while snap is off. ---- */
phoenix_engine_api editor_vec3 snap_manip_position(editor_vec3 p, uint32_t moving_object_id);

/* ---- internal helpers (were static) ---- */
phoenix_engine_api void         begin_manip_frame_once(system_element_context* ui_element, int view_x, int view_y, int view_w, int view_h);
phoenix_engine_api float*       get_manip_field_ptr(system_scene_object* o, int index);
phoenix_engine_api const char*  get_manip_edit_tool_hint(manip_edit_tool tool);

/* ---- drawing / input: call once per frame from the Scene view draw ---- */
phoenix_engine_api void draw_scene_manipulator_overlay(VkCommandBuffer cmd, system_element_context* ui_element, int view_x, int view_y, int view_w, int view_h);

/* True while the cursor is over the tool box / manipulator panel or a field drag is running. (was manip_overlay_wants_mouse) */
phoenix_engine_api bool is_manip_overlay_using_mouse(void);

/* ---- Target Weld core, for triangle index buffers ----
   Every use of vertex src becomes dst, triangles that collapse are removed. *index_count shrinks. */
phoenix_engine_api void weld_manip_indices(uint32_t* indices, uint32_t* index_count, uint32_t src, uint32_t dst);

#endif