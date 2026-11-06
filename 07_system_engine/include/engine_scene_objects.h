#ifndef phoenix_engine_scene_objects_h
#define phoenix_engine_scene_objects_h

#include "engine_defines.h"
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

/* No editor headers here. editor_vec3, editor_mat4 and editor_vertex come from engine_math.h / engine_render.h,
   which engine_globals.h includes before this file. */

#define max_object_name   64
#define max_scene_objects 256
/* Lens used by hierarchy (game) cameras. ONE place: the Game view projection and the blue
   culling-plane helper in the Scene view both read these, so they can never disagree. */
#define game_camera_fov   1.0472f    /* 60 degrees */
#define game_camera_near  0.1f
#define game_camera_far   4000.0f    /* anything farther than this is culled */

typedef enum {
    scene_object_empty = 0,
    scene_object_mesh,
    scene_object_camera,
    scene_object_light
} enum_scene_object_type;

typedef struct system_scene_object {
    uint32_t id;
    char     name[max_object_name];
    bool     is_open;

    enum_scene_object_type type;

    editor_vec3 position;
    editor_vec3 rotation;   /* radians: x = pitch, y = yaw, z = roll */
    editor_vec3 scale;

    /* GPU mesh handles stored as uint64_t. Cast back with (VkBuffer)(uintptr_t)h when drawing. */
    uint64_t vertex_buffer;
    uint64_t vertex_memory;
    uint64_t index_buffer;
    uint64_t index_memory;
    uint32_t index_count;

    /* Furthest vertex from the object origin (unscaled). Set in upload_scene_object_mesh. Used by Ctrl+F. */
    float bounds_radius;

    struct system_scene_object* parent;
    struct system_scene_object* first_child;
    struct system_scene_object* next_sibling;
} system_scene_object;

typedef struct {
    system_scene_object objects[max_scene_objects];
    uint32_t object_count;
    uint32_t selected_object_id;
    uint32_t next_object_id;
} system_scene_registry;

/* g_scene_registry: extern in engine_globals.h, defined in engine_globals.c. */

phoenix_engine_api void start_scene_registry(void);
phoenix_engine_api system_scene_object* spawn_new_scene_object(const char* name, editor_vec3 obj_vert_pos);
phoenix_engine_api system_scene_object* find_scene_object_by_id(uint32_t id);

/* engine_empty_object.c: Unity style empty game object (transform only, no mesh). Selects the new object. NULL if the scene is full. */
phoenix_engine_api system_scene_object* create_system_empty(const char* name, editor_vec3 position);

/* scene draw */
phoenix_engine_api bool upload_scene_object_mesh(system_scene_object* obj, VkDevice device, VkPhysicalDevice physical_device,
                                                 const editor_vertex* vertices, uint32_t vertex_count,
                                                 const uint32_t* indices, uint32_t index_count);
phoenix_engine_api void draw_scene_objects(VkCommandBuffer cmd, const float* view16, const float* proj16);
phoenix_engine_api void draw_game_scene_objects(VkCommandBuffer cmd, const float* view16, const float* proj16);   /* meshes only, no editor helpers */
phoenix_engine_api void shutdown_scene_object_rendering(VkDevice device);
phoenix_engine_api void set_scene_terrain_borders(bool on);    /* region (orange) + chunk (cyan) outlines in the Scene view. On by default. */
phoenix_engine_api bool get_scene_terrain_borders(void);

/* grid / orbit camera: set_system_grid_orbit_camera and update_system_grid_hotkeys are declared in engine_grid.h now. */
phoenix_engine_api void focus_system_grid_on_selected_object(void);

/* engine_scene_camera.c
   SYSTEM camera = editor fly camera (Scene view). GAME camera = first camera object in the hierarchy (Game view). */
phoenix_engine_api void get_scene_clip_planes(editor_vec3 eye, float* out_near, float* out_far);   /* was static scene_clip_planes */
phoenix_engine_api void build_scene_view_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj);
phoenix_engine_api system_scene_object* find_scene_game_camera(void);
phoenix_engine_api bool build_game_view_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj);

#endif