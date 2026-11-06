#ifndef phoenix_editor_input_h
#define phoenix_editor_input_h

#include "engine_input.h"
#include "system_element.h"
#include "engine_math.h"
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


/* editor_input.h: input that needs the editor's ui, mesh and scene types. lives in the editor project.
   this file needs system_element_context, editor_vec3, edit_mode_mesh and the scene types, so include system_globals.h before it.
   system_globals.h includes this file near its end, before its extern block (see editor_input_globals.txt). */

/* ---- ui hit testing geometry ----
   these only use plain numbers. your rule says math goes in the math file, so they can move to engine_math.h unchanged. */
typedef struct struct_point_2d {
    float x;
    float y;
} struct_point_2d;

typedef struct struct_polygon_shape {
    struct_point_2d vertices[8];
    int             vertex_count;
} struct_polygon_shape;

bool validate_rectangle_hit(int mx, int my, int rx, int ry, int rw, int rh);
bool validate_circle_hit(int mx, int my, int cx, int cy, int radius);
bool validate_polygon_hit(int mx, int my, const struct_polygon_shape* polygon);
bool validate_sloped_tab_hit(int mx, int my, int tx, int ty, int tw, int th, int slope_padding);

void process_element_hit_state(system_element_context* context, unsigned int id, bool is_hovered);
void update_element_input(system_element_context* context, int mouse_x, int mouse_y, bool mouse_down);

/* ---- brush helpers shared by paint and sculpt (defined in editor_sculpt_input.c) ---- */
bool brush_mouse_world_ray(const system_element_context* ui_element, editor_vec3* o, editor_vec3* d);
void brush_ensure_density(void);

/* ---- mouse wheel (editor_wheel_hook.c). call system_wheel_hook_update once per frame from editor_update. ---- */
void system_wheel_hook_update(void);
bool system_wheel_hook_active(void);
void system_camera_dolly(float notches);
void system_grid_zoom(float steps);   /* system_grid.c */

/* ---- paint ---- */

/* what the paint pick returns: where, on which face, and which texture coordinate is under the mouse */
typedef struct manage_paint_hit {
    editor_vec3  obj_vert_pos;          /* object space */
    editor_vec3  nrm;                   /* object space */
    unsigned int face;                  /* em_mesh face index */
    float        uv[2];                 /* texture coordinate at the hit */
    float        uv_min[2];             /* uv bounds of the hit face (the dab is clipped to it) */
    float        uv_max[2];
    float        uv_per_m;              /* uv units per object space metre at the hit */
} manage_paint_hit;

/* 1 = also paint the vertex colours (old behaviour, the only thing visible right now). 0 once the texture is drawn on the mesh. */
#ifndef paint_also_vertex_color
#define paint_also_vertex_color 1
#endif

/* paint input pass. call once per ui frame, next to sculpt_input_update.
   paint mode on + mesh object obj_vert_selected + left button in the scene view (no alt, no camera drag, no context menu):
   ray pick against the edit mesh, spacing check, one dab, commit to the gpu.
   has its own ray and mesh raycast, nothing is shared with the sculpt input file. */
void paint_input_update(system_element_context* ui_element);

/* the pick builds the ray from build_scene_view_camera(). if strokes land mirrored top to bottom, flip this to 1.
   the projection used there is the vulkan one, so 0 should be right. */
#ifndef paint_pick_flip_y
#define paint_pick_flip_y 0
#endif

/* ---- sculpt ---- */

typedef struct manage_sculpt_stroke {
    bool         active;
    unsigned int object_id;
    editor_vec3  start;
    editor_vec3  last_dab;
    editor_vec3  plane_n;               /* grab drags on the plane through start facing the camera */
} manage_sculpt_stroke;

/* sculpt input pass. call once per ui frame (traverse_dock_tree does, next to light_update_from_scene).
   mirrors g_sculpt into g_mode_state.sculpt (radius / strength / hardness / symmetry x / invert).
   sculpt mode on + mesh object obj_vert_selected + left button in the scene view (no alt): ray pick, stroke, commit to the gpu.
   maya controls: alt+lmb / alt+mmb camera stays in update_element_input, b + drag = brush size, ctrl = invert while stroking. */
void sculpt_input_update(system_element_context* ui_element);
void sculpt_sync_mode_state(void);
bool sculpt_input_is_stroking(void);

/* same flip switch as paint_pick_flip_y, for the sculpt pick. */
#ifndef sculpt_pick_flip_y
#define sculpt_pick_flip_y 0
#endif

#endif