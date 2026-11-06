#ifndef phoenix_engine_edit_manager_h
#define phoenix_engine_edit_manager_h

/* engine_edit_manager.h is ONE file: the edit manager. Part 0 = edit mesh data, registry, gpu.
   Parts 1 and 2 = the manager of the edit tools.
   NEVER include engine_globals.h from a header: engine_globals.h includes this one (include cycle). */
#include "engine_defines.h"
#include "engine_math.h"
#include "engine_element.h"
#include "engine_mode.h"
#include "engine_scene_objects.h"
#include "engine_objects.h"


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

/* Moved here from the old editor header system_edit_manager.h. */

/*==================================================================================================*/
/*                                  PART 0: EDIT MESH DATA, REGISTRY, GPU                           */
/*==================================================================================================*/

/* >>> PASTE HERE, from the old 03_system_editor/include/system_edit_manager.h, everything that is NOT in parts 1 and 2 below:
       struct edit_mode_mesh (+ its typedef body), edit_mode_camera_context, cam_screen,
       the em_* registry and gpu prototypes (em_registry_remove, em_gpu_retire ...) and the mesh operation prototypes.
       Without the body of struct edit_mode_mesh here, engine_globals.c cannot build g_edit_mesh. <<<
   engine_mode.h only forward declares:  struct edit_mode_mesh;  typedef struct edit_mode_mesh edit_mode_mesh;
   so define it as   struct edit_mode_mesh { ... };   (do not typedef it a second time with a new body). */

/*==================================================================================================*/
/*                                  CONSTANTS                                                       */
/*==================================================================================================*/

#define edit_ops_lock_id 0x7C7C7C03u        /* ui_element->active_id while a tool drag runs, so nothing underneath reacts */
#define EDIT_OPS_LOCK_ID edit_ops_lock_id


/*==================================================================================================*/
/*                                  PART 1: ENUMS AND STRUCTS                                       */
/*==================================================================================================*/

/* ------------------------------------------------------------------------- */
/* TYPES: Edit mode (camera + picking, matched to the Scene view camera)     */
/* ------------------------------------------------------------------------- */
typedef struct
{
    editor_vec3 obj_vert_pos;
    float       obj_vert_color[4];
    uint8_t     obj_vert_selected;
    uint8_t     obj_vert_active;
} edit_mode_vert;

typedef struct
{
    uint32_t vert_to_edge_array[2];
    uint32_t face[2];
    uint32_t face_count;
    uint8_t  obj_vert_selected;
    uint8_t  obj_vert_active;
} edit_mode_edge;

typedef struct
{
    uint32_t vert_to_edge_array[4];
    uint32_t edge_to_face_array[4];
    uint8_t  face_type;
    uint8_t  obj_vert_selected;
    uint8_t  obj_vert_active;
    float    uv[4][2];
} edit_mode_face;

struct edit_mode_mesh
{
    edit_mode_vert* verts;
    uint32_t vert_count;
    uint32_t vert_cap;
    edit_mode_edge* edges;
    uint32_t edge_count;
    uint32_t edge_cap;
    edit_mode_face* faces;
    uint32_t face_count;
    uint32_t face_cap;
};

typedef struct { enum_mesh_selection kind; uint32_t index; } edit_pick_selection;

/* ------------------------------------------------------------------------- */
/* TYPES: Edit mode (one frame of input, handed to the interactive tools     */
/* by the dispatcher)                                                        */
/* ------------------------------------------------------------------------- */
typedef struct 
{
    system_element_context* ui_element;
    system_scene_object*    obj;
    edit_mode_mesh*         mesh;
    const edit_mode_camera_context* cam;
    float                   mx, my;
    bool                    inside;     /* cursor in the Scene view and not over the manipulator overlay */
    bool                    press;      /* left button went down this frame */
    bool                    down;       /* left button is held */
} edit_frame;

/* ------------------------------------------------------------------------- */
/* TYPES: Edit manager (selection mask, active tool)                         */
/* ------------------------------------------------------------------------- */
typedef enum
{
   selection_mask_object,
   selection_mask_vertex,
   selection_mask_edge,
   selection_mask_face
} system_selection_mask;

typedef struct sceneview_edit_tool
{
   void(*on_activate) (void* system_editor_context);
   void(*on_deactivate) (void* system_editor_context);
   /* mouse callbacks like on mouse press, on mouse drag, on mouse release, on draw overlay */
   bool show_transform_gizmo;
} sceneview_edit_tool;

typedef struct edit_manager
{
    sceneview_edit_tool* active_tool;
    sceneview_edit_tool* default_select_tool;
    system_selection_mask current_mask;
} edit_manager;


/*==================================================================================================*/
/*                                  PART 2: FUNCTION PROTOTYPES                                     */
/*==================================================================================================*/

/* ---- on screen message, bottom of the Scene view (was edit_msg, edit_msg_draw) ---- */
void show_edit_message(const char* fmt, ...);
void draw_edit_message(VkCommandBuffer cmd, system_element_context* ui_element);

/* ---- the mesh a tool works on: Edit mode, a selected mesh object, not the terrain (was edit_get_target) ---- */
bool get_edit_target(system_scene_object** out_obj, edit_mode_mesh** out_mesh);

/* ---- camera + picking (was edit_cam_begin, edit_project, edit_project_all, edit_screen_points, edit_pick_*, edit_in_scene_view) ---- */
bool              begin_edit_camera(edit_mode_camera_context* c, const system_element_context* ui_element, const system_scene_object* obj);
cam_screen        project_edit_point(const edit_mode_camera_context* c, editor_vec3 p);
bool              project_edit_points_all(const edit_mode_camera_context* c, const edit_mode_mesh* mesh);   /* fills the table get_edit_screen_points() returns */
const cam_screen* get_edit_screen_points(void);
uint32_t          pick_edit_vert(const edit_mode_mesh* mesh, float mx, float my, uint32_t exclude);
uint32_t          pick_edit_edge(const edit_mode_mesh* mesh, float mx, float my);
uint32_t          pick_edit_face(const edit_mode_mesh* mesh, float mx, float my);
edit_pick_selection pick_edit_element(const edit_mode_mesh* mesh, enum_mesh_selection kind, float mx, float my);
bool              is_edit_in_scene_view(const system_element_context* ui_element);

/* ---- 2D overlay drawing (was edit_line2d, edit_box2d) ---- */
void draw_edit_line_2d(VkCommandBuffer cmd, float x0, float y0, float x1, float y1, float thickness, system_color color);
void draw_edit_box_2d(VkCommandBuffer cmd, float x, float y, float radius, system_color color);

/* ---- interactive tool: Target Weld ---- */
bool toggle_edit_target_weld(system_element_context* ui_element);
void update_edit_target_weld(const edit_frame* frame);
void draw_edit_target_weld(VkCommandBuffer cmd, system_element_context* ui_element, const edit_mode_mesh* mesh);
void cancel_edit_target_weld(system_element_context* ui_element);
bool is_edit_target_weld_dragging(void);

/* ---- interactive tool: Insert Loop ---- */
bool toggle_edit_insert_loop(system_element_context* ui_element);
void update_edit_insert_loop(const edit_frame* frame);
void draw_edit_insert_loop(VkCommandBuffer cmd, const edit_mode_camera_context* cam, const edit_mode_mesh* mesh);
void cancel_edit_insert_loop(system_element_context* ui_element);
bool is_edit_insert_loop_dragging(void);
bool consume_edit_insert_loop_wheel(float notches);          /* number of rows; true = wheel used */

/* ---- interactive tool: Multicut ----
   hold CTRL = preview the cut with percent labels, CTRL + wheel = more or fewer lines, CTRL + left click = cut */
bool toggle_edit_multicut(system_element_context* ui_element);
void update_edit_multicut(const edit_frame* frame);
void draw_edit_multicut(VkCommandBuffer cmd, system_element_context* ui_element, const edit_mode_camera_context* cam, const edit_mode_mesh* mesh);
void cancel_edit_multicut(system_element_context* ui_element);
bool consume_edit_multicut_wheel(float notches);             /* only uses the wheel while CTRL is held; true = wheel used */

/* ---- row select: double click = select a row, no tool on (was edit_row_select_update) ---- */
void update_edit_row_select(const edit_frame* frame);

/* ---- dispatcher: extrude, target weld, bridge, insert loop (wheel = number of loops, drag = slide), merge, delete,
   flip normals, and the double click "select a row". Works on the edit mesh of the selected object. ---- */

/* THE dispatcher. The Modeler window buttons and the right click menu both call this with a system_edit_tools value.
   Instant tools (extrude, bridge, merge ...) run now. Interactive tools (target weld, insert loop) toggle on or off.
   Returns true when the tool ran / switched. The reason for a false is shown on screen (see draw_edit_ops). (was edit_ops_run) */
bool run_edit_ops(system_element_context* ui_element, system_edit_tools tool);

/* true for the tools run_edit_ops really does. Use it to dim the others in the Modeler window. (was edit_ops_tool_is_wired) */
bool is_edit_ops_tool_wired(system_edit_tools tool);

/* Once per frame in the Scene view, right AFTER system_edit_mode_update(ui_element) and system_edit_mode_draw(cmd, ui_element). */
void update_edit_ops(system_element_context* ui_element);
void draw_edit_ops(VkCommandBuffer cmd, system_element_context* ui_element);

/* true while a modeling tool owns the mouse in the Scene view (a tool is on and the cursor is in the view, or a drag runs).
   Put   if (is_edit_ops_using_mouse()) return;   at the top of the click handling in system_edit_mode_update. (was edit_ops_wants_mouse) */
bool is_edit_ops_using_mouse(void);

/* Mouse wheel. Called by system_wheel_hook.c BEFORE it dollies the camera.
   Returns true when the wheel was used (Insert Loop: number of loops), so the camera must not zoom. (was edit_ops_consume_wheel) */
bool consume_edit_ops_wheel(float notches);

/* true while a modeling tool with its own on-screen guide is on (Target Weld, Multicut, Insert Loop).
   The transform gizmo must not draw, and must not take mouse hits, while this is true. (was edit_ops_wants_gizmo_hidden) */
bool is_edit_ops_hiding_gizmo(void);

/* extrude distance in object units, used by Extrude before you drag the new faces. */
void  set_edit_ops_extrude_distance(float distance);
float get_edit_ops_extrude_distance(void);

#endif