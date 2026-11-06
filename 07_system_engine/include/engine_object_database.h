#ifndef phoenix_engine_object_database_h
#define phoenix_engine_object_database_h

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

/* Per-object database.
   Every scene object id owns its OWN small record set (up to object_db_max_kinds blocks, one per kind).
   Looking an object up is one hash probe, and only that object's blocks are touched, so selecting an object or
   entering edit mode never walks the data of any other object.
   The scene registry stays the light list (id, name, transform, hierarchy). Heavy or type-specific data goes here. */

enum { object_db_max_kinds = 8 };
#define OBJECT_DB_MAX_KINDS object_db_max_kinds

typedef enum {
    object_data_none            = 0,
    object_data_mesh_blob       = 1,    /* CPU copy of the mesh, for edit mode and saving (see set_object_mesh_data) */
    object_data_board_tile      = 2,    /* board_tile_data */
    object_data_terrain         = 3,    /* terrain_settings (the settings the terrain was built from) */
    object_data_terrain_region  = 4,    /* region_00..region_08: [16 byte header][(region_cells+1)^2 float heights]. The live height store. */
    object_data_terrain_tunnels = 5,    /* on the Terrain object: array of terrain_tunnel */
    object_data_terrain_color   = 6,    /* region_00..region_08: region_cells^2 RGBA8 paint cells (A = paint coverage). Created on the first paint dab. */
    object_data_skin_ref        = 7,
    object_data_user            = 100   /* 100 and up are free for anything else */
} enum_object_data_kind;

#define OBJECT_DATA_NONE object_data_none
#define OBJECT_DATA_SKIN_REF object_data_skin_ref
#define OBJECT_DATA_MESH_BLOB object_data_mesh_blob
#define OBJECT_DATA_TERRAIN object_data_terrain
#define OBJECT_DATA_TERRAIN_COLOR object_data_terrain_color
#define OBJECT_DATA_TERRAIN_REGION object_data_terrain_region
#define OBJECT_DATA_TERRAIN_TUNNELS object_data_terrain_tunnels

/* NOTE: engine_scene_delete.c uses OBJECT_DATA_SKIN_REF. It is not in this enum. Add  object_data_skin_ref = <free number>
   here (or wherever the skin system defines it) and use the lowercase name in the delete code. */

phoenix_engine_api void start_object_database(void);       /* empties the database. optional: every call below starts it on first use */
phoenix_engine_api void shutdown_object_database(void);    /* frees everything */

/* One block per (object id, kind). The database owns a copy of the bytes.
   data == NULL stores a zeroed block of `size` bytes. Returns false if the object already has object_db_max_kinds kinds.
   was object_db_set */
phoenix_engine_api bool  set_object_data(uint32_t object_id, enum_object_data_kind kind, const void* data, uint32_t size);

/* NULL when the object has no block of that kind. The pointer stays valid until that block is set again or removed.
   was object_db_get */
phoenix_engine_api void* get_object_data(uint32_t object_id, enum_object_data_kind kind, uint32_t* out_size);

phoenix_engine_api bool     remove_object_data(uint32_t object_id, enum_object_data_kind kind);   /* was object_db_remove */
phoenix_engine_api void     remove_object_data_all(uint32_t object_id);                           /* was object_db_remove_object: call when a scene object is deleted */
phoenix_engine_api bool     has_object_data(uint32_t object_id);                                  /* was object_db_has_object */
phoenix_engine_api uint32_t get_object_database_count(void);                                      /* was object_db_object_count */

/* mesh blob: vertex_size must be a multiple of 4 */
phoenix_engine_api bool set_object_mesh_data(uint32_t object_id, const void* vertices, uint32_t vertex_size, uint32_t vertex_count,
                                             const uint32_t* indices, uint32_t index_count);       /* was object_db_set_mesh */
phoenix_engine_api bool get_object_mesh_data(uint32_t object_id, const void** vertices, uint32_t* vertex_size, uint32_t* vertex_count,
                                             const uint32_t** indices, uint32_t* index_count);     /* was object_db_get_mesh */

/* Whole database <-> a file next to the exe (see settings_db_make_path). Load merges into what is already there. */
phoenix_engine_api bool save_object_database(const char* file_name);   /* was object_db_save */
phoenix_engine_api bool load_object_database(const char* file_name);   /* was object_db_load */

phoenix_engine_api bool object_db_set(uint32_t object_id, enum_object_data_kind kind, const void* data, uint32_t size);
phoenix_engine_api void* object_db_get(uint32_t object_id, enum_object_data_kind kind, uint32_t* out_size);
phoenix_engine_api bool object_db_remove(uint32_t object_id, enum_object_data_kind kind);
phoenix_engine_api void object_db_remove_object(uint32_t object_id);

#endif