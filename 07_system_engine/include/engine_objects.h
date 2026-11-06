#ifndef phoenix_engine_objects_h
#define phoenix_engine_objects_h

#include "engine_defines.h"
#include "engine_math.h"

float skin_empty_grey_linear(void);


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

/* No editor headers and no engine_math.h include here: editor_vec3, editor_mat4 and editor_vertex come from the
   headers engine_globals.h includes before this one.

   REMOVED from this file because engine_game_state.h already owns them (two copies = "conflicting types"):
     enum_object_type, camera_component, mesh_component, system_world_object, manage_game_world
   use world_camera, world_object_type (object_type_camera ...), system_world_object and manage_game_world from engine_game_state.h. */

/* system_scene_object is defined in engine_scene_objects.h. Only pointers are used here. */
struct system_scene_object;
typedef struct system_scene_object system_scene_object;


/*==================================================================================================*/
/*                                  PART 1: ENUMS AND STRUCTS                                       */
/*==================================================================================================*/

/* ------------------------------------------------------------------------- */
/* TYPES: Modeler objects database (CPU topology + Vulkan handles)           */
/* ------------------------------------------------------------------------- */
typedef struct {
    uint32_t object_id;
    char name[64];

    editor_vec3 position;
    editor_vec3 rotation;
    editor_vec3 scale;

    /* CPU data topology */
    editor_vertex* vertices;
    uint32_t* indices;
    uint32_t vertex_count;
    uint32_t index_count;

    /* vulkan vram handles */
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_memory;
    bool is_compiled_to_gpu;
} system_object;

typedef struct {
    system_object registry[128];
    uint32_t object_count;
    int32_t active_selection_index;
} system_object_database;


/* ------------------------------------------------------------------------- */
/* TYPES: Modeler workspace (Object mode / Edit mode, shapes, edit tools)    */
/* ------------------------------------------------------------------------- */
typedef enum
{
    modeler_mode_object,        /* object mode: standard world entity selection */
    modeler_mode_edit,          /* edit mode: face / vertex sculpting */
    modeler_mode_paint,         /* paint mode */
    modeler_mode_sculpt,        /* sculpt mode */
    modeler_mode_textureshop    /* texture mode: uv / texture sculpting */
} enum_modeler_mode;

typedef enum {
    model_shape_cube,
    model_shape_sphere,
    model_shape_cylinder,
    model_shape_octogon,
    model_shape_board_game_octogon,                /* used for board games. an octogon can be tiles but it needs diamonds, half diamonds and quarter diamonds to make a board rectangle or square. */
    model_shape_board_game_diamond,                /* used in between 4 octogons, shape depends on octogon size */
    model_shape_board_game_half_diamond,           /* squares off the top, bottom, left and right of boards. really a triangle. */
    model_shape_board_game_quarter_diamond,        /* squares off the four corners of a board. really a triangle. */
    model_shape_terrain_00,                        /* open world: same structure as sandbox but different tools and smoothing */
    model_shape_terrain_01,                        /* sandbox digger games */
    model_shape_custom
} enum_model_shape;

typedef enum system_edit_tools
{
    /* existing tools */
    edit_extrude,
    edit_bevel,
    edit_multicut,
    edit_target_weld,
    edit_arch_bridge,
    edit_bridge,

    /* topological and cutting tools */
    edit_insert_edge_loop,
    edit_offset_edge_loop,
    edit_slide_edge,
    edit_spin_edge,
    edit_knife_cut,

    /* merging and deletion tools */
    edit_collapse,
    edit_merge_vertices,
    edit_dissolve,
    edit_delete_component,

    /* subdivision and reshaping tools */
    edit_subdivide,
    edit_smooth,
    edit_poke_face,
    edit_wedge,

    /* separating and combining tools */
    edit_duplicate_face,
    edit_extract,
    edit_separate,
    edit_combine,

    /* normals and shading tools */
    edit_flip_normals,
    edit_soften_edge,
    edit_harden_edge
} system_edit_tools;

typedef struct {
    uint32_t model_id;
    char asset_name[64];
    enum_model_shape shape_type;

    editor_vec3 position;
    editor_vec3 scale;
    editor_vec3 rotation;

    /* hard mesh topology definitions tracked in the tool workspace */
    editor_vertex vertices[512];
    uint32_t indices[1536];
    uint32_t vertex_count;
    uint32_t index_count;
} model_object;

typedef struct {
    model_object active_models[64];
    uint32_t total_model_count;
    uint32_t selected_model_index;

    enum_modeler_mode current_mode;
} manage_modeler;


/*==================================================================================================*/
/*                                  PART 2: STRUCT VARIABLES                                        */
/*==================================================================================================*/

/* object_database (system_object_database) and edit_object_workspace (manage_modeler):
   extern in engine_globals.h, init in engine_globals.c. Need an object? Get it from the editor. */


/*==================================================================================================*/
/*                                  PART 3: FUNCTION PROTOTYPES                                     */
/*==================================================================================================*/

/* world objects */
phoenix_engine_api void create_system_camera(editor_vec3 position, float size, const char* name, VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue);
phoenix_engine_api void create_system_cube(editor_vec3 position, float size, const char* name, VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue);
phoenix_engine_api void cleanup_modeler_objects_vram(VkDevice device);

/* modeler objects database */
phoenix_engine_api void start_modeler_objects_database(void);

/* modeler workspace */
phoenix_engine_api void start_modeler_system(manage_modeler* manager);
phoenix_engine_api void generate_primitive_shape(manage_modeler* manager, enum_model_shape shape);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Object size                                                    */
/*                                                                           */
/* Object size = the mesh's own X / Y / Z extent times the object's scale.   */
/* It follows the object's local axes (rotation is ignored, it is NOT the    */
/* world box).                                                               */
/*                                                                           */
/* Setting a size changes the object's SCALE so the result is exactly that   */
/* size. Example: a 1 x 1 x 1 cube at scale 1 -> set X to 0.0625 ->          */
/* scale.x = 0.0625.                                                         */
/*                                                                           */
/* Nothing here for the terrain (a fixed height grid) or objects without a   */
/* mesh: they return false.                                                  */
/* ------------------------------------------------------------------------- */

/* the mesh's min / max corner in the object's own space (before position / rotation / scale). was object_get_local_bounds */
phoenix_engine_api bool get_object_local_bounds(system_scene_object* o, editor_vec3* out_min, editor_vec3* out_max);

/* world units along each local axis. false = no size (terrain, empty, light, camera ...). was object_get_mesh_size */
phoenix_engine_api bool get_object_mesh_size(system_scene_object* o, editor_vec3* out_size);

/* axis 0 = X, 1 = Y, 2 = Z. A flat axis (mesh extent 0) cannot be sized and returns false. was object_set_mesh_size_axis */
phoenix_engine_api bool set_object_mesh_size_axis(system_scene_object* o, int axis, float size);

/* all three axes at once. was object_set_mesh_size */
phoenix_engine_api bool set_object_mesh_size(system_scene_object* o, editor_vec3 size);

#endif