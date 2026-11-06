#ifndef PHOENIX_SYSTEM_EDIT_MANAGER_H
#define PHOENIX_SYSTEM_EDIT_MANAGER_H

#include "engine_edit_manager.h"

typedef struct { uint32_t vert_to_edge_array[4]; int face_type; float uv[4][2]; uint8_t obj_vert_selected; } num_of_faces;
typedef struct { num_of_faces* f; uint32_t n, cap; } num_of_faces_list;

#define edit_mode_selection_ring_max 36000
typedef struct {
    uint32_t edge_to_face_array[edit_mode_selection_ring_max];
    uint32_t a[edit_mode_selection_ring_max];
    uint32_t b[edit_mode_selection_ring_max];
    uint32_t face[edit_mode_selection_ring_max];
    uint32_t n;
    bool closed;
    bool clean;
} edit_mode_selection_ring;

bool nl_push(num_of_faces_list* list, const num_of_faces* face);
bool nl_push_existing(num_of_faces_list* list, const edit_mode_mesh* mesh, uint32_t face_index);
bool nl_push_quad(num_of_faces_list* list, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
bool edit_op_extrude(edit_mode_mesh* mesh, const char** err);
bool edit_op_bridge(edit_mode_mesh* mesh, const char** err);
bool edit_op_merge_selected(edit_mode_mesh* mesh, const char** err);
bool edit_op_delete_faces(edit_mode_mesh* mesh, const char** err);
bool edit_op_flip_normals(edit_mode_mesh* mesh, const char** err);
editor_vec3 edit_face_normal(const edit_mode_mesh* mesh, const edit_mode_face* face);
bool em_rebuild(edit_mode_mesh* mesh, const num_of_faces* faces, uint32_t face_count, enum_mesh_selection basis);
uint32_t em_add_vert(edit_mode_mesh* mesh, editor_vec3 position, const float color[4]);
bool edit_op_cut_ring(edit_mode_mesh* mesh, const edit_mode_selection_ring* ring, int count, float slide, const char** err);
bool edit_ring_build(const edit_mode_mesh* mesh, uint32_t edge, edit_mode_selection_ring* ring);
uint32_t edit_ring_face_count(const edit_mode_selection_ring* ring);
uint32_t edit_pick_edge(const edit_mode_mesh* mesh, float mx, float my);
cam_screen edit_project(const edit_mode_camera_context* camera, editor_vec3 position);
void edit_line2d(VkCommandBuffer cmd, float ax, float ay, float bx, float by, float width, system_color color);
void edit_box2d(VkCommandBuffer cmd, float x, float y, float radius, system_color color);
edit_pick_selection edit_pick_element(const edit_mode_mesh* mesh, enum_mesh_selection kind, float mx, float my);
void em_set_active_element(edit_mode_mesh* mesh, enum_mesh_selection kind, uint32_t index);
void edit_msg(const char* format, ...);

#endif

#if 0

#include "system_element.h"
#include "system_scene_objects.h"
#include "system_objects.h"
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
/*                                  CONSTANTS (needed by the types below)                           */
/*==================================================================================================*/

#define EM_NONE 0xFFFFFFFFu                 /* "no index" for edge_to_face_array->face links and similar */
#define EDIT_OPS_LOCK_ID 0x7C7C7C03u        /* ui_element->active_id while a tool drag runs, so nothing underneath reacts */
#define edit_mode_selection_ring_max 36000                        /* longest ring (strip of quads) the ring builder handles not to be confused with context menu ring ui_element*/


/*==================================================================================================*/
/*                                  PART 1: ENUMS AND STRUCTS                                       */
/*==================================================================================================*/


/* ------------------------------------------------------------------------- */
/* TYPES: Edit mesh (editable polygon mesh)                                  */
/* Shared verts, unique edges, tri/quad faces.                               */
/* Everything refers to everything else by INDEX (safe across realloc).      */
/* Face winding: counter-clockwise when viewed from outside.                 */
/* ------------------------------------------------------------------------- */

typedef struct 
{
    editor_vec3 obj_vert_pos;            /* object space */
    float       obj_vert_color[4];
    uint8_t     obj_vert_selected;
    uint8_t     obj_vert_active;
} edit_mode_vert;

typedef struct 
{
    uint32_t vert_to_edge_array[2];
    uint32_t face[2];           /* neighbouring faces, EM_NONE if unused */
    uint32_t face_count;        /* 1 = boundary edge_to_face_array, 2 = interior */
    uint8_t  obj_vert_selected;
    uint8_t  obj_vert_active;
} edit_mode_edge;

typedef struct 
{
    uint32_t vert_to_edge_array[4];              /* vert_to_edge_array[3] unused when face_type == 3 */
    uint32_t edge_to_face_array[4];           /* edge_to_face_array[i] joins vert_to_edge_array[i] -> vert_to_edge_array[(i+1)%face_type] */
    uint8_t  face_type;             /* 3 = half quad (triangle), 4 = full quad */
    uint8_t  obj_vert_selected;
    uint8_t  obj_vert_active;
    float    uv[4][2];          /* texture coords per CORNER (uv[k] belongs to vert_to_edge_array[k]). Verts are shared, UVs are not, so seams are free. */
} edit_mode_face;

typedef struct edit_mode_mesh {
    edit_mode_vert* verts;  uint32_t vert_count, vert_cap;
    edit_mode_edge* edges;  uint32_t edge_count, edge_cap;
    edit_mode_face* faces;  uint32_t face_count, face_cap;
} edit_mode_mesh;


typedef struct edit_mode_wire_mesh
{
    edit_mode_vert* verts;  uint32_t vert_count, vert_cap;
    edit_mode_edge* edges;  uint32_t edge_count, edge_cap;
    edit_mode_face* faces;  uint32_t face_count, face_cap;

} edit_mode_wire_mesh;


/* ------------------------------------------------------------------------- */
/* TYPES: Edit mode (face lists + rebuild)                                   */
/* ------------------------------------------------------------------------- */

typedef struct { uint32_t vert_to_edge_array[4]; int face_type; float uv[4][2]; uint8_t obj_vert_selected; } num_of_faces;
typedef struct { num_of_faces* f; uint32_t n, cap; } num_of_faces_list;


/* ------------------------------------------------------------------------- */
/* TYPES: Edit mode (camera + picking, matched to the Scene view camera)     */
/* ------------------------------------------------------------------------- */

typedef struct { enum_mesh_selection kind; uint32_t index; } edit_pick_selection;


/* ------------------------------------------------------------------------- */
/* TYPES: Edit mode (one frame of input, handed to the interactive tools     */
/* by the dispatcher)                                                        */
/* ------------------------------------------------------------------------- */

typedef struct 
{
    system_element_context* ui_element;
    system_scene_object*    obj;
    edit_mode_mesh*                mesh;
    const edit_mode_camera_context*          cam;
    float                   mx, my;
    bool                    inside;     /* cursor in the Scene view and not over the manipulator overlay */
    bool                    press;      /* left button went down this frame */
    bool                    down;       /* left button is held */
} edit_frame;


/* ------------------------------------------------------------------------- */
/* TYPES: Edit mode (ring: the strip of quads that one edge_to_face_array crosses,         */
/* used by rows and loop cut)                                                */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t edge_to_face_array[edit_mode_selection_ring_max];     /* ring edges, in order */
    uint32_t a[edit_mode_selection_ring_max];        /* a[i] -> b[i] is the SAME side of every ring edge_to_face_array, so t means the same thing on all of them */
    uint32_t b[edit_mode_selection_ring_max];
    uint32_t face[edit_mode_selection_ring_max];     /* face[i] sits between edge_to_face_array[i] and edge_to_face_array[i+1]; closed rings also have face[n-1] between edge_to_face_array[n-1] and edge_to_face_array[0] */
    uint32_t n;
    bool     closed;
    bool     clean;              /* every end is a mesh border and every face is a quad: safe to cut */
} edit_mode_selection_ring;


/* ------------------------------------------------------------------------- */
/* TYPES: Edit manager (selection mask, active tool)                         */
/* ------------------------------------------------------------------------- */

typedef enum
{
   selection_mask_object,
   selection_mask_vertex,
   selection_mask_edge,
   selection_mask_face,

} system_selection_mask;

typedef struct sceneview_edit_tool
{
   void(*on_activate) (void* system_editor_context);
   void(*on_deactivate) (void* system_editor_context);
   //mousecallbacks like on mouse press, on mouse drag, on mouse release, on draw overlay
   bool show_transform_gizmo;
} sceneview_edit_tool;

typedef struct edit_manager
{

    sceneview_edit_tool * active_tool;
    sceneview_edit_tool* default_select_tool;
    system_selection_mask current_mask;
} edit_manager;






/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (on screen message, bottom of the Scene view)        */
/* ------------------------------------------------------------------------- */

void edit_msg(const char* fmt, ...);
void edit_msg_draw(VkCommandBuffer cmd, system_element_context* ui_element);

/* ---- the mesh a tool works on: Edit mode, a selected mesh object, not the terrain ---- */
bool edit_get_target(system_scene_object** out_obj, edit_mode_mesh** out_m);
editor_vec3 edit_face_normal(const edit_mode_mesh* mesh, const edit_mode_face* F);   /* thin mesh wrapper over system_math_compute_face_normal */


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (face lists + rebuild)                               */
/* ------------------------------------------------------------------------- */

bool nl_push(num_of_faces_list* L, const num_of_faces* f);
bool nl_push_existing(num_of_faces_list* L, const edit_mode_mesh* mesh, uint32_t fi);
bool nl_push_quad(num_of_faces_list* L, uint32_t a, uint32_t b, uint32_t c, uint32_t d);

/* faces in, a clean edit_mode_mesh out (edges rebuilt, unused verts dropped) */
bool em_rebuild(edit_mode_mesh* mesh, const num_of_faces* faces, uint32_t fc, enum_mesh_selection basis);

/* map[vert_to_edge_array] = the vertex vert_to_edge_array becomes. Used by Target Weld and Merge. */
bool edit_weld_with_map(edit_mode_mesh* mesh, const uint32_t* map, uint32_t keep_selected_vert);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (camera + picking, matched to the Scene view camera) */
/* ------------------------------------------------------------------------- */

bool         edit_cam_begin(edit_mode_camera_context* c, const system_element_context* ui_element, const system_scene_object* obj);
cam_screen          edit_project(const edit_mode_camera_context* c, editor_vec3 p);
bool         edit_project_all(const edit_mode_camera_context* c, const edit_mode_mesh* mesh);      /* fills the table edit_screen_points() returns */
const cam_screen*   edit_screen_points(void);
uint32_t     edit_pick_vert(const edit_mode_mesh* mesh, float mx, float my, uint32_t exclude);
uint32_t     edit_pick_edge(const edit_mode_mesh* mesh, float mx, float my);
uint32_t     edit_pick_face(const edit_mode_mesh* mesh, float mx, float my);
edit_pick_selection       edit_pick_element(const edit_mode_mesh* mesh, enum_mesh_selection kind, float mx, float my);
bool         edit_in_scene_view(const system_element_context* ui_element);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (2D overlay drawing)                                 */
/* ------------------------------------------------------------------------- */

void edit_line2d(VkCommandBuffer cmd, float x0, float y0, float x1, float y1, float th, system_color c);
void edit_box2d(VkCommandBuffer cmd, float x, float y, float r, system_color c);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (instant tools: return false and set *err to say why) */
/* ------------------------------------------------------------------------- */

bool edit_op_extrude(edit_mode_mesh* mesh, const char** err);
bool edit_op_bridge(edit_mode_mesh* mesh, const char** err);
bool edit_op_merge_selected(edit_mode_mesh* mesh, const char** err);
bool edit_op_delete_faces(edit_mode_mesh* mesh, const char** err);
bool edit_op_flip_normals(edit_mode_mesh* mesh, const char** err);

/* cut across the ring of quads: `count` evenly spaced rows, `slide` -0.5..0.5 moves them. Used by Insert Loop and Multicut. */
bool edit_op_cut_ring(edit_mode_mesh* mesh, const edit_mode_selection_ring* R, int count, float slide, const char** err);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (interactive tool: Target Weld)                      */
/* ------------------------------------------------------------------------- */

bool edit_target_weld_toggle(system_element_context* ui_element);
void edit_target_weld_update(const edit_frame* f);
void edit_target_weld_draw(VkCommandBuffer cmd, system_element_context* ui_element, const edit_mode_mesh* mesh);
void edit_target_weld_cancel(system_element_context* ui_element);
bool edit_target_weld_dragging(void);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (interactive tool: Insert Loop)                      */
/* ------------------------------------------------------------------------- */

bool edit_insert_loop_toggle(system_element_context* ui_element);
void edit_insert_loop_update(const edit_frame* f);
void edit_insert_loop_draw(VkCommandBuffer cmd, const edit_mode_camera_context* cam, const edit_mode_mesh* mesh);
void edit_insert_loop_cancel(system_element_context* ui_element);
bool edit_insert_loop_dragging(void);
bool edit_insert_loop_wheel(float notches);          /* number of rows; true = wheel used */


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (interactive tool: Multicut)                         */
/* hold CTRL = preview the cut with percent labels, CTRL + wheel = more or   */
/* fewer lines, CTRL + left click = cut                                      */
/* ------------------------------------------------------------------------- */

bool edit_multicut_toggle(system_element_context* ui_element);
void edit_multicut_update(const edit_frame* f);
void edit_multicut_draw(VkCommandBuffer cmd, system_element_context* ui_element, const edit_mode_camera_context* cam, const edit_mode_mesh* mesh);
void edit_multicut_cancel(system_element_context* ui_element);
bool edit_multicut_wheel(float notches);             /* only uses the wheel while CTRL is held; true = wheel used */


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (row select: double click = select a row, no tool on) */
/* ------------------------------------------------------------------------- */

void edit_row_select_update(const edit_frame* f);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode (ring: the strip of quads that one edge_to_face_array crosses)     */
/* ------------------------------------------------------------------------- */

bool     edit_ring_build(const edit_mode_mesh* mesh, uint32_t e0, edit_mode_selection_ring* R);
uint32_t edit_ring_face_count(const edit_mode_selection_ring* R);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mesh (lifetime)                                           */
/* ------------------------------------------------------------------------- */

void em_mesh_clear(edit_mode_mesh* mesh);     /* keeps allocations */
void em_mesh_free(edit_mode_mesh* mesh);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mesh (building)                                           */
/* ------------------------------------------------------------------------- */

uint32_t em_add_vert(edit_mode_mesh* mesh, editor_vec3 obj_vert_pos, const float color[4]);        /* color may be NULL */
uint32_t em_add_face(edit_mode_mesh* mesh, const uint32_t* verts, int face_type);             /* face_type 3 or 4; creates edges */
void     em_make_cube(edit_mode_mesh* mesh, float half_size);                             /* 8 verts, 12 edges, 6 quads */
/* nx * nz quads in the XZ plane, every quad is system_unit_m x system_unit_m (4 mesh). vert (i,j) = j*(nx+1)+i at x=i*cell, z=j*cell, y=0.
   Edges are written directly, no search: O(n). Faces face +Y. */
void     em_make_grid(edit_mode_mesh* mesh, uint32_t nx, uint32_t nz);
void     em_make_grid_sized(edit_mode_mesh* mesh, uint32_t nx, uint32_t nz, float cell_size);

/* Weld verts closer than weld_eps and (optionally) merge coplanar triangle pairs into quads. */
bool em_build_from_triangles(edit_mode_mesh* mesh, const editor_vertex* verts, uint32_t vert_count,
                             const uint32_t* indices, uint32_t index_count,
                             float weld_eps, bool merge_quads);

/* Fan-triangulates. EVERY FACE GETS ITS OWN VERTS (4 per quad, 3 per triangle), so a cube comes out as 24 verts and each
   face can carry its own uv / normal. Caller free()s *out_verts and *out_indices. */
bool em_triangulate(const edit_mode_mesh* mesh, editor_vertex** out_verts, uint32_t* out_vert_count,
                    uint32_t** out_indices, uint32_t* out_index_count);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mesh (selection)                                          */
/* ------------------------------------------------------------------------- */

void     em_clear_selection(edit_mode_mesh* mesh);
void     em_sync_selection(edit_mode_mesh* mesh, enum_mesh_selection basis);   /* make verts/edges/faces agree */
void     em_select_element(edit_mode_mesh* mesh, enum_mesh_selection mode, uint32_t index, bool additive);
uint32_t em_selection_centroid(const edit_mode_mesh* mesh, editor_vec3* out);  /* returns the selected vert count */
void     em_translate_selected(edit_mode_mesh* mesh, editor_vec3 delta);
void     em_selection_counts(const edit_mode_mesh* mesh, uint32_t* verts, uint32_t* edges, uint32_t* faces);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mesh (per-object registry: scene object id -> edit mesh)  */
/* ------------------------------------------------------------------------- */

edit_mode_mesh* em_registry_get(uint32_t object_id);
edit_mode_mesh* em_registry_get_or_create(system_scene_object* obj);   /* lazy cube fallback for mesh objects */
bool     em_registry_adopt_triangles(uint32_t object_id, const editor_vertex* verts, uint32_t vert_count,
                                     const uint32_t* indices, uint32_t index_count);
void     em_registry_remove(uint32_t object_id);      /* call when a scene object is deleted */
void     em_registry_shutdown(void);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mesh (GPU)                                                */
/* ------------------------------------------------------------------------- */

/* Rebuild the object's GPU buffers from the edit mesh. Old buffers are destroyed 3 frames later. */
bool em_commit_to_gpu(edit_mode_mesh* mesh, system_scene_object* obj);
void em_gpu_tick(void);     /* once per frame */
/* Queue GPU handles for destruction 3 frames from now (safe while a command buffer is being recorded). */
void em_gpu_retire(uint64_t vb, uint64_t vm, uint64_t ib, uint64_t im);
void em_gpu_flush(void);    /* before the device is destroyed */


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Edit mode modeling (dispatcher)                                */
/* Extrude, Target Weld, Bridge, Insert Loop (wheel = number of loops,       */
/* drag = slide), Merge, Delete, Flip Normals, and the double click          */
/* "select a row".  Works on the em_mesh of the selected object.             */
/* ------------------------------------------------------------------------- */

/* THE dispatcher. The Modeler window buttons and the right click menu both call this with a system_edit_tools value.
   Instant tools (extrude, bridge, merge ...) run now. Interactive tools (target weld, insert loop) toggle on or off.
   Returns true when the tool ran / switched. The reason for a false is shown on screen (see edit_ops_draw). */
bool edit_ops_run(system_element_context* ui_element, system_edit_tools tool);

/* true for the tools edit_ops_run really does. Use it to dim the others in the Modeler window. */
bool edit_ops_tool_is_wired(system_edit_tools tool);

/* Once per frame in the Scene view, right AFTER system_edit_mode_update(ui_element) and system_edit_mode_draw(cmd, ui_element). */
void edit_ops_update(system_element_context* ui_element);
void edit_ops_draw(VkCommandBuffer cmd, system_element_context* ui_element);

/* true while a modeling tool owns the mouse in the Scene view (a tool is on and the cursor is in the view, or a drag runs).
   Put   if (edit_ops_wants_mouse()) return;   at the top of the click handling in system_edit_mode_update. */
bool edit_ops_wants_mouse(void);

/* Mouse wheel. Called by system_wheel_hook.c BEFORE it dollies the camera.
   Returns true when the wheel was used (Insert Loop: number of loops), so the camera must not zoom. */
bool edit_ops_consume_wheel(float notches);

/* true while a modeling tool with its own on-screen guide is on (Target Weld, Multicut, Insert Loop).
   The transform gizmo must not draw, and must not take mouse hits, while this is true. */
bool edit_ops_wants_gizmo_hidden(void);

/* extrude distance in object units, used by Extrude before you drag the new faces. */
void  edit_ops_set_extrude_distance(float d);
float edit_ops_get_extrude_distance(void);


#endif