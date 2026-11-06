#ifndef PHOENIX_SYSTEM_OBJECTS_H
#define PHOENIX_SYSTEM_OBJECTS_H

#include "engine_objects.h"

#endif

#if 0

#include "system_math.h"
#include <stdbool.h>
#include <stdint.h>
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
/* Defined in system_scene_objects.h. Only pointers are used here, so a forward declaration is enough.
   (Fixes: unknown type name 'system_scene_object'.) */
struct system_scene_object;
typedef struct system_scene_object system_scene_object;


/*==================================================================================================*/
/*                                  PART 1: ENUMS AND STRUCTS                                       */
/*==================================================================================================*/


/* ------------------------------------------------------------------------- */
/* TYPES: World objects (game world registry)                                */
/* ------------------------------------------------------------------------- */

typedef enum
{
    OBJECT_TYPE_MESH,
    OBJECT_TYPE_CAMERA,
    OBJECT_TYPE_LIGHT
} enum_object_type;

typedef struct {
    editor_vec3 position;
    editor_vec3 rotation;
    float fov;
    float aspect_ratio;
    float near_clip;
    float far_clip;
    editor_mat4 view_matrix;
    editor_mat4 projection_matrix;
    bool is_orthographic;
} camera_component;

typedef struct {
    uint32_t vertex_count;
    uint32_t index_count;
    void* vertex_buffer_handle;
    void* index_buffer_handle;
} mesh_component;

typedef struct {
    uint32_t entity_id;
    char name[64];
    enum_object_type object_type;

    editor_vec3 position;
    editor_vec3 scale;
    editor_vec3 rotation;

    union {
        camera_component camera;
        mesh_component   mesh;
    } data;
} system_world_object;

typedef struct {
    system_world_object object_registry[256];
    uint32_t object_count;

    camera_component editor_scene_camera;
    uint32_t active_game_camera_index;
} manage_game_world;


/* ------------------------------------------------------------------------- */
/* TYPES: Modeler objects database (CPU topology + Vulkan handles)           */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t object_id;
    char name[64];

    editor_vec3 position;
    editor_vec3 rotation;
    editor_vec3 scale;

    // CPU Data Topology
    editor_vertex* vertices;
    uint32_t* indices;
    uint32_t vertex_count;
    uint32_t index_count;

    // VULKAN HARDWARE VRAM HANDLES
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

// Simple ProBuilder mode toggle flags
typedef enum 
{
    MODELER_MODE_OBJECT,        // Object Mode: Standard world entity selection
    MODELER_MODE_EDIT,          // Edit Mode: Face/vertex sculpting
    MODELER_MODE_PAINT,         // PAINT Mode: painting
    MODELER_MODE_SCULPT,        // SCULPT Mode: sculpting
    MODELER_MODE_TEXTURESHOP    // TEXTURE Mode: uv/texturesculpting
} enum_modeler_mode;

typedef enum {
    MODEL_SHAPE_CUBE,
    MODEL_SHAPE_SPHERE,
    MODEL_SHAPE_CYLINDER,
    MODEL_SHAPE_OCTOGON,
    MODEL_SHAPE_BOARD_GAME_OCTOGON,                //used for board games. An octogon can be tiles but it needs diamonds, half diamonds and quarter diamonds to make board rectangle or sqaured.
    MODEL_SHAPE_BOARD_GAME_DIAMOND,                //used inbetweem 4 octogons and shape depends on octgon size
    MODEL_SHAPE_BOARD_GAME_HALF_DIAMOND,           //used to sqaure off top, bottom, left and right parts of boards. Really is a triangle but easier to say half diamond for this purpose
    MODEL_SHAPE_BOARD_GAME_QUARTER_DIAMOND,        //used to sqaure off the four corners of board. Really is a triangle but easy to say quarter diamond piece.
    MODEL_SHAPE_TERRAIN_00,                        // terrain_00 is for open world still uses same structure as sandbox for building but different tools and ways to smooth then sandbox.
    MODEL_SHAPE_TERRAIN_01,                        // terrain_01 is for sandbox digger games
    MODEL_SHAPE_CUSTOM
} enum_model_shape;

typedef enum system_edit_tools
{
    /* Existing Tools */
    edit_extrude,
    edit_bevel,
    edit_multicut,
    edit_target_weld,
    edit_arch_bridge,
    edit_bridge,

    /* Topological & Cutting Tools */
    edit_insert_edge_loop,
    edit_offset_edge_loop,
    edit_slide_edge,
    edit_spin_edge,
    edit_knife_cut,

    /* Merging & Deletion Tools */
    edit_collapse,
    edit_merge_vertices,
    edit_dissolve,
    edit_delete_component,

    /* Subdivision & Reshaping Tools */
    edit_subdivide,
    edit_smooth,
    edit_poke_face,
    edit_wedge,

    /* Separating & Combining Tools */
    edit_duplicate_face,
    edit_extract,
    edit_separate,
    edit_combine,

    /* Normals & Shading Tools */
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

    // Hard mesh topology definitions tracked in tool workspace
    editor_vertex vertices[512];
    uint32_t indices[1536];
    uint32_t vertex_count;
    uint32_t index_count;
} model_object;

typedef struct {
    model_object active_models[64];
    uint32_t total_model_count;
    uint32_t selected_model_index;

    // State tracking variable
    enum_modeler_mode current_mode;
} manage_modeler;


/*==================================================================================================*/
/*                                  PART 2: STRUCT VARIABLES                                        */
/*==================================================================================================*/

/* Modeler objects database. Need an object? Get it from the editor. */
extern system_object_database object_database;

/* Global instance handle for the modeler workspace data */
extern manage_modeler edit_object_workspace;


/*==================================================================================================*/
/*                                  PART 3: FUNCTION PROTOTYPES                                     */
/*==================================================================================================*/


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: World objects                                                  */
/* ------------------------------------------------------------------------- */

void create_system_camera(editor_vec3 position, float size, const char* name, VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue);
void create_system_cube(editor_vec3 position, float size, const char* name, VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue);
void cleanup_modeler_objects_vram(VkDevice device);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Modeler objects database                                       */
/* ------------------------------------------------------------------------- */

void start_modeler_objects_database(void);


/* ------------------------------------------------------------------------- */
/* FUNCTIONS: Modeler workspace                                              */
/* ------------------------------------------------------------------------- */

void start_modeler_system(manage_modeler* manager);
void generate_primitive_shape(manage_modeler* manager, enum_model_shape shape);


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

/* the mesh's min / max corner in the object's own space (before position / rotation / scale) */
bool object_get_local_bounds(system_scene_object* o, editor_vec3* out_min, editor_vec3* out_max);

/* world units along each local axis. false = no size (terrain, empty, light, camera ...) */
bool object_get_mesh_size(system_scene_object* o, editor_vec3* out_size);

/* axis 0 = X, 1 = Y, 2 = Z. A flat axis (mesh extent 0) cannot be sized and returns false. */
bool object_set_mesh_size_axis(system_scene_object* o, int axis, float size);

/* all three axes at once */
bool object_set_mesh_size(system_scene_object* o, editor_vec3 size);


#endif