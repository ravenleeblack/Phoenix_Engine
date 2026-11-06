#ifndef phoenix_engine_transform_gizmo_h
#define phoenix_engine_transform_gizmo_h

#include "engine_defines.h"


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

/* Included by engine_globals.h AFTER engine_edit_manager.h and engine_scene_objects.h.
   enum_dcc_tool, struct_viewport_bounds, editor_vec3, editor_mat4 and system_scene_object come from the headers before it.

   edit_mode_mesh MUST BE A COMPLETE TYPE where engine_transform_gizmo.c is compiled (it reads vert_count, verts[i].obj_vert_pos,
   verts[i].obj_vert_selected). The compile error "invalid use of incomplete typedef" means the header that defines the struct
   is not reached from engine_globals.h. Include that header here. */

#define gizmo_pi           3.14159265f
#define gizmo_scene_root_id 1000
#define gizmo_pick_radius_px 8.0f
#define gizmo_ring_segments  48
#define gizmo_screen_k       0.18f      /* gizmo world length = view depth * this (constant on-screen size) */

typedef enum {
    gizmo_axis_none    = -1,
    gizmo_axis_x       = 0,
    gizmo_axis_y       = 1,
    gizmo_axis_z       = 2,
    gizmo_axis_uniform = 3
} enum_gizmo_axis;

/* the camera the gizmo projects with: scene view rect + view and view-projection matrices */
typedef struct {
    struct_viewport_bounds b;
    editor_mat4            view;
    editor_mat4            vp;
} gizmo_cam;

typedef struct {
    bool  dragging;
    bool  edit;                 /* true: dragging mesh components, false: dragging the whole object */
    int   axis;
    int   hover;
    enum_dcc_tool tool;
    uint32_t id;

    /* translate / scale: screen-space axis captured at drag start */
    float sx, sy, dx, dy, len2, t_grab, L;
    float mx0, my0;
    /* rotate */
    float last_ang, accum, sign;

    editor_vec3 p0, r0, s0;     /* p0 = pivot at drag start (object position in Object mode, selection centroid in Edit mode) */
} gizmo_state;

/* edit mode mesh helpers. these were being called with no prototype. if the real functions live in engine_edit_manager.h
   under other names, delete these two lines and change the two call sites in engine_transform_gizmo.c to those names. */
phoenix_engine_api uint32_t em_selection_centroid(const edit_mode_mesh* mesh, editor_vec3* out_centroid);
phoenix_engine_api bool em_commit_to_gpu(edit_mode_mesh* mesh, system_scene_object* o);

/* the orbit camera the Scene view really renders with (defined in the grid file). was a loose extern inside the gizmo .c */
phoenix_engine_api void build_scene_orbit_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj);

/* defined in engine_terrain.c. engine_terrain.h cannot declare them (it has no editor types), so they are declared here. */
phoenix_engine_api bool            is_terrain_object(const system_scene_object* obj);
phoenix_engine_api edit_mode_mesh* terrain_edit_mesh_current(void);

#endif