#ifndef PHOENIX_SYSTEM_MODE_STATE_H
#define PHOENIX_SYSTEM_MODE_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "system_element.h"
#include "system_edit_manager.h"
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

/*==================================================================================================*/
/*                                  PART 1: ENUMS AND STRUCTS                                       */
/*==================================================================================================*/
struct edit_mode_mesh;
typedef struct edit_mode_mesh edit_mode_mesh;

/* ------------------------------------------------------------------------- */
/* TYPES: Mode system (shared by all modes)                                  */
/* ------------------------------------------------------------------------- */

/* What the Modeler window shows tools for. Follows whichever mode-menu button was clicked last. */
typedef enum {
    mode_context_object = 0,
    mode_context_edit,
    mode_context_uv,
    mode_context_model,
    mode_context_sculpt,
    mode_context_paint,
    mode_context_animation,
    mode_context_textureshop        /* Texture Shop: skins. right click makes / selects / deletes skins */
} system_mode_context;


/* ------------------------------------------------------------------------- */
/* TYPES: Object mode                                                        */
/* The DEFAULT mode. Transform tools (G T R S) on scene objects.             */
/* Its window is the Modeler (shared with Edit mode).                        */
/* ------------------------------------------------------------------------- */

/* Object mode: transform tools on scene objects */
typedef struct {
    enum_dcc_tool tool;             /* mirrors element_context.active_tool */
    bool  snap_enabled;
    float snap_translate_m;         /* meters            */
    float snap_rotate_deg;          /* degrees           */
    float snap_scale;               /* scale increment   */
} object_mode_state;

typedef struct {
    bool active;        /* the Ob button; set by mode_menu_enter_mode */
} object_state;


/* ------------------------------------------------------------------------- */
/* TYPES: Edit (poly component) mode                                         */
/* ------------------------------------------------------------------------- */

typedef struct {
    enum_mesh_selection select_type;    /* vert / edge_to_face_array / face */
    bool  soft_select;
    float soft_select_radius;
    bool  xray;
    float extrude_distance;
    float bevel_width;
    int   bevel_segments;
    float merge_threshold;
} edit_mode_state;


/* ------------------------------------------------------------------------- */
/* TYPES: UV mode                                                            */
/* ------------------------------------------------------------------------- */

typedef struct {
    int   active_uv_set;
    bool  show_checker;
    bool  pixel_snap;
    int   texture_size;             /* px, square */
} uv_mode_state;


/* ------------------------------------------------------------------------- */
/* TYPES: Model mode (mesh modeling operators)                               */
/* ------------------------------------------------------------------------- */

typedef struct {
    float extrude_distance;
    float bevel_width;
    int   bevel_segments;
    float merge_threshold;
} model_mode_state;


/* ------------------------------------------------------------------------- */
/* TYPES: Paint mode                                                         */
/* ------------------------------------------------------------------------- */

/* Paint mode settings (mode state) */
typedef struct {
    float color[4];
    float brush_size;
    float opacity;
    float hardness;
    int   active_layer;
} paint_mode_state;

/* Vertex paint state. Field order matches the initializer in system_paint_mode.c:
   { active, color, radius, opacity, hardness, spacing } */
typedef struct {
    bool  active;
    float color[3];     /* sRGB 0..1, converted to linear when the swapchain is sRGB */
    float radius;       /* object space metres */
    float opacity;      /* 0..1 */
    float hardness;     /* 0..0.99 */
    float spacing;      /* fraction of radius between dabs */
} paint_state;


/* ------------------------------------------------------------------------- */
/* TYPES: Sculpt mode                                                        */
/* Brush list, per-brush settings, sculpt options, brush math on an edit_mode_mesh, */
/* and subdivide / undivide levels.                                          */
/*                                                                           */
/* The Sc button in the mode menu turns the sculpt flag on (mode_state says  */
/* Sculpt). The Brushes window picks the brush, the Sculpt window edits      */
/* options + the settings of the picked brush. Input comes next: it only     */
/* needs to call sculpt_stroke_begin / update / end with a hit point.        */
/* ------------------------------------------------------------------------- */

/* Sculpt mode settings (mode state) */
typedef struct {
    float brush_radius;
    float brush_strength;           /* 0..1 */
    float brush_falloff;            /* 0..1 */
    bool  symmetry_x;
    bool  invert;
} sculpt_mode_state;

typedef enum {
    sculpt_brush_clay_buildup = 0,
    sculpt_brush_clay_soften,
    sculpt_brush_standard,
    sculpt_brush_carve,         /* was "Dam Standard" */
    sculpt_brush_grab,
    sculpt_brush_polish,
    sculpt_brush_inflate,
    sculpt_brush_smooth,
    sculpt_brush_flatten,
    sculpt_brush_count
} enum_sculpt_brush;

/* which settings a brush shows in the Sculpt window (bit mask) */
enum {
    sculpt_setting_radius        = 1 << 0,
    sculpt_setting_strength      = 1 << 1,
    sculpt_setting_hardness      = 1 << 2,
    sculpt_setting_plane_offset  = 1 << 3,
    sculpt_setting_smooth_passes = 1 << 4,
    sculpt_setting_spacing       = 1 << 5
};

/* subdivision limits */
enum { sculpt_max_levels = 6, sculpt_max_faces = 1 << 20 };

/* Maya = Alt+LMB orbit, Alt+MMB pan, Alt+RMB zoom, B+drag brush size.
   ZBrush = RMB drag orbit, Alt+RMB pan, Alt+RMB then release Alt zoom, S for the brush ring. (read by the input pass) */
typedef enum { sculpt_nav_maya = 0, sculpt_nav_zbrush } enum_sculpt_nav;

typedef struct {
    float radius;           /* object space units (meters)                              */
    float strength;         /* 0..1                                                     */
    float hardness;         /* 0..0.99: 0 = soft all the way, 1 = flat top, sharp rim   */
    float plane_offset;     /* -1..1: lifts / sinks the work plane (clay, flatten)      */
    int   smooth_passes;    /* 1..8: relax passes per dab (smooth, polish, clay soften) */
    float spacing;          /* 0.02..1: dab distance as a fraction of the radius        */
} sculpt_brush_settings;

typedef struct {
    bool                   active;                        /* the Sc button: use sculpt_mode_is_active() */
    enum_sculpt_brush      brush;
    sculpt_brush_settings  settings[sculpt_brush_count];  /* every brush remembers its own */

    bool  sym_x, sym_y, sym_z;                            /* mirror across the object space planes */
    bool  invert;                                         /* brush works backwards (also toggled by Ctrl while stroking) */
    bool  smooth_subdivide;                               /* Catmull-Clark smoothing when subdividing, off = cage stays put */
    enum_sculpt_nav nav;
} sculpt_state;


/* ------------------------------------------------------------------------- */
/* TYPES: Combined mode state (holds every mode's settings)                  */
/* ------------------------------------------------------------------------- */

typedef struct {
    bool                initialized;
    system_mode_context context;
    enum_dcc_tool       mode;           /* object / edit mesh   */
    enum_workflow_mode  workflow;       /* uv / model / sculpt / paint / animation */

    object_mode_state   object;
    edit_mode_state     edit;
    uv_mode_state       uv;
    sculpt_mode_state   sculpt;
    model_mode_state    model;
    paint_mode_state    paint;
} system_mode_state;


/*==================================================================================================*/
/*                                  PART 2: STRUCT VARIABLES                                        */
/*==================================================================================================*/

/* Mode system (all modes) */
extern system_mode_state g_mode_state;

/* Object mode */
extern object_state g_object;

/* Paint mode */
extern paint_state g_paint;

/* Sculpt mode */
extern sculpt_state g_sculpt;


/*==================================================================================================*/
/*                                  PART 3: FUNCTION PROTOTYPES                                     */
/*==================================================================================================*/


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Mode system (all modes)                                        */
/* ------------------------------------------------------------------------- */

const char* mode_context_name(system_mode_context c);
void start_mode_state(void);                                    /* safe to call more than once */
void mode_state_sync_from_ui(system_element_context* ui_element);       /* UI -> g_mode_state (call once per frame) */
void mode_state_set_tool(system_element_context* ui_element, enum_dcc_tool tool);
void mode_state_set_mode(system_element_context* ui_element, enum_dcc_mode mode);
void mode_state_set_workflow(system_element_context* ui_element, enum_workflow_mode workflow);

/* ONE active mode: Object, Edit, Sculpt, Paint or Texture Shop (others are switched off). Use this from every menu / button.
   Also sets the old ui_element fields (active_mode / current_workflow) so the rest of the editor stays in step. */
void mode_state_set_exclusive(system_element_context* ui_element, system_mode_context ctx);

/* tools that change the world must ask these first. terrain brush: if (!mode_state_is_sculpt()) return; */
bool mode_state_is_sculpt(void);
bool mode_state_is_paint(void);
bool mode_state_is_edit(void);
bool mode_state_is_object(void);
bool mode_state_is_textureshop(void);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Object mode                                                    */
/* Same pattern as sculpt and paint: a flag + start + is_active.             */
/* ------------------------------------------------------------------------- */

void start_object_mode(void);                 /* safe to call more than once. object mode starts ON */
void object_mode_set_active(bool on);
bool object_mode_is_active(void);             /* flag AND mode_state says Object */

/* snapping, reads g_mode_state.object. returns vert_to_edge_array untouched when snapping is off. */
float object_snap_translate(float vert_to_edge_array);
float object_snap_rotate(float deg);
float object_snap_scale(float vert_to_edge_array);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode                                                      */
/* ------------------------------------------------------------------------- */

/* Call once per frame in the Scene view UI pass, AFTER the scene image is drawn,
   INSTEAD of the transform gizmo while ui_element->active_mode == MODE_EDIT_MESH.
   Handles: 1/2/3 hotkeys (vert/edge_to_face_array/face), hover, click / shift+click select, overlay drawing. */
void system_edit_mode_update_and_draw(VkCommandBuffer cmd, system_element_context* ui_element);

/* Split form */
void system_edit_mode_update(system_element_context* ui_element);
void system_edit_mode_draw(VkCommandBuffer cmd, system_element_context* ui_element);

/* True while edit mode owns the Scene view mouse (use it to block object click-picking). */
bool system_edit_mode_wants_mouse(void);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Paint mode                                                     */
/* ------------------------------------------------------------------------- */

void start_paint_mode(void);
void paint_mode_set_active(bool on);
bool paint_mode_is_active(void);
void paint_clamp_settings(void);

/* one dab at ctr (object space). brush ids: 0 paint, 1 airbrush, 2 eraser, 3 smudge (todo), 4 blur, 5 fill, 6 picker, 7 lighten, 8 darken.
   returns true when any vertex colour changed. */
bool paint_dab(edit_mode_mesh* mesh, editor_vec3 ctr, int brush, bool erase_now);

/* once per UI frame. picks the mesh under the mouse with its own ray, dabs, commits to the GPU. no dependency on the sculpt input file. */
void paint_input_update(system_element_context* ui_element);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Sculpt mode                                                    */
/* ------------------------------------------------------------------------- */

/* ---- state ---- */
void  start_sculpt_mode(void);                 /* safe to call more than once */
void  sculpt_mode_set_active(bool on);         /* called by mode_menu_enter_mode */
bool  sculpt_mode_is_active(void);             /* flag AND mode_state says Sculpt */

void                    sculpt_set_brush(enum_sculpt_brush brush);
enum_sculpt_brush       sculpt_get_brush(void);
const char*             sculpt_brush_name(enum_sculpt_brush brush);
uint32_t                sculpt_brush_setting_mask(enum_sculpt_brush brush);
sculpt_brush_settings*  sculpt_current_settings(void);
void                    sculpt_clamp_settings(sculpt_brush_settings* s);

/* ---- math (object space) ---- */
/* 1 at the centre, 0 at the rim. inside `hardness * radius` it stays 1, then a smoothstep to 0. */
float sculpt_falloff(float dist, float radius, float hardness);
/* area weighted vertex normals, out must hold mesh->vert_count entries */
void  sculpt_compute_vertex_normals(const edit_mode_mesh* mesh, editor_vec3* out);

/* ---- strokes ----
   begin once when the button goes down, update for every dab, end when it is released.
   center / hit_normal are OBJECT space (transform the ray hit with the inverse model matrix first).
   drag_delta is the object space movement since begin (only Grab reads it).
   update returns true when vertices moved: call em_commit_to_gpu(mesh, obj) after it (once per frame is enough). */
void sculpt_stroke_begin(edit_mode_mesh* mesh, editor_vec3 center, editor_vec3 hit_normal);
bool sculpt_stroke_update(edit_mode_mesh* mesh, editor_vec3 center, editor_vec3 hit_normal, editor_vec3 drag_delta, bool invert_now);
void sculpt_stroke_end(void);

/* ---- subdivision levels on the obj_vert_selected mesh object ---- */
bool sculpt_subdivide_selected(void);     /* +1 level (Catmull-Clark topology, tri -> 3 quads, quad -> 4 quads) */
bool sculpt_undivide_selected(void);      /* -1 level, the sculpted detail on the old vertices is carried down */
int  sculpt_subdiv_level(void);           /* 0 when the obj_vert_selected object was never subdivided here */
bool sculpt_selected_mesh_counts(uint32_t* verts, uint32_t* faces);   /* false when no mesh object is obj_vert_selected */


#endif