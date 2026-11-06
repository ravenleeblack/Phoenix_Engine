#ifndef phoenix_engine_game_state_h
#define phoenix_engine_game_state_h

#include "engine_defines.h"
#include "engine_math.h"      /* editor_vec3 */

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

/* THE ONLY place world_object_type, world_camera, system_world_object and manage_game_world are defined.
   The editor's old system_objects.h defined them a second time ("conflicting types"): that copy is gone,
   system_objects.h is now a stub that includes engine_objects.h. Do not define these again anywhere. */

/* ---- game world: what manage_game_world, start_world_registry and system_engine_context use ---- */
#define world_max_objects      256
#define world_object_name_size 64

/* camera keeps the value 1 so nothing that already stores / compares it changes.
   mesh and light are here because the editor's old enum_object_type had them (OBJECT_TYPE_MESH / OBJECT_TYPE_LIGHT). */
typedef enum world_object_type
{
    object_type_none = 0,
    object_type_camera,
    object_type_mesh,
    object_type_light,
    object_type_count
} world_object_type;

typedef struct world_camera_data
{
    float fov;
    float near_clip;
    float far_clip;
    bool  is_orthographic;
} world_camera_data;

typedef struct world_mesh_data
{
    uint32_t vertex_count;
    uint32_t index_count;
    void*    vertex_buffer_handle;
    void*    index_buffer_handle;
} world_mesh_data;

typedef union world_object_data
{
    world_camera_data camera;
    world_mesh_data   mesh;
} world_object_data;

/* the editor's old camera_component is this. rename camera_component -> world_camera in every file that still uses it. */
typedef struct world_camera
{
    editor_vec3 position;
    editor_vec3 rotation;
    float       fov;
    float       near_clip;
    float       far_clip;
    bool        is_orthographic;
} world_camera;

typedef struct system_world_object
{
    uint32_t          entity_id;
    char              name[world_object_name_size];
    world_object_type object_type;
    editor_vec3       position;
    editor_vec3       rotation;
    editor_vec3       scale;
    world_object_data data;
} system_world_object;

typedef struct manage_game_world
{
    world_camera        editor_scene_camera;
    system_world_object object_registry[world_max_objects];
    int                 object_count;
    int                 active_game_camera_index;
} manage_game_world;

/* the game plugin (engine owns it) */
typedef struct game_state
{
    void* state;                                              /* opaque: only the game knows what is inside */
    bool (*start)(struct game_state* game);
    void (*update)(struct game_state* game, float delta_time);
    void (*shutdown)(struct game_state* game);
} game_state;

typedef bool (*create_game_ptr)(game_state* out_game);

#endif