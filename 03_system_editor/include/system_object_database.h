#ifndef SYSTEM_OBJECT_DATABASE_H
#define SYSTEM_OBJECT_DATABASE_H

#include "engine_object_database.h"

#endif

#if 0
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
/* Per-object database.
   Every scene object id owns its OWN small record set (up to OBJECT_DB_MAX_KINDS blocks, one per kind).
   Looking an object up is one hash probe, and only that object's blocks are touched, so selecting an object or
   entering edit mode never walks the data of any other object.
   The scene registry stays the light list (id, name, transform, hierarchy). Heavy or type-specific data goes here. */

enum { OBJECT_DB_MAX_KINDS = 8 };

typedef enum {
    OBJECT_DATA_NONE       = 0,
    OBJECT_DATA_MESH_BLOB  = 1,    /* CPU copy of the mesh, for edit mode and saving (see object_db_set_mesh) */
    OBJECT_DATA_BOARD_TILE = 2,    /* board_tile_data, see system_board.h */
    OBJECT_DATA_TERRAIN    = 3,    /* terrain_settings (the settings the terrain was built from), see system_scene_terrain.h */
    OBJECT_DATA_TERRAIN_REGION  = 4,   /* region_00..region_08: [16 byte header][(region_cells+1)^2 float heights]. The live height store. */
    OBJECT_DATA_TERRAIN_TUNNELS = 5,   /* on the Terrain object: array of terrain_tunnel */
    OBJECT_DATA_TERRAIN_COLOR   = 6,   /* region_00..region_08: region_cells^2 RGBA8 paint cells (A = paint coverage). Created on the first paint dab. */
    OBJECT_DATA_USER       = 100   /* 100 and up are free for anything else */
} enum_object_data_kind;

void start_object_database(void);       /* empties the database. optional: every call below starts it on first use */
void shutdown_object_database(void);    /* frees everything */

/* One block per (object id, kind). The database owns a copy of the bytes.
   data == NULL stores a zeroed block of `size` bytes. Returns false if the object already has OBJECT_DB_MAX_KINDS kinds. */
bool  object_db_set(uint32_t object_id, enum_object_data_kind kind, const void* data, uint32_t size);

/* NULL when the object has no block of that kind. The pointer stays valid until that block is set again or removed. */
void* object_db_get(uint32_t object_id, enum_object_data_kind kind, uint32_t* out_size);

bool  object_db_remove(uint32_t object_id, enum_object_data_kind kind);
void  object_db_remove_object(uint32_t object_id);     /* call when a scene object is deleted */
bool  object_db_has_object(uint32_t object_id);
uint32_t object_db_object_count(void);

/* mesh blob: vertex_size must be a multiple of 4 */
bool object_db_set_mesh(uint32_t object_id, const void* vertices, uint32_t vertex_size, uint32_t vertex_count,
                        const uint32_t* indices, uint32_t index_count);
bool object_db_get_mesh(uint32_t object_id, const void** vertices, uint32_t* vertex_size, uint32_t* vertex_count,
                        const uint32_t** indices, uint32_t* index_count);

/* Whole database <-> a file next to the exe (see settings_db_make_path). Load merges into what is already there. */
bool object_db_save(const char* file_name);
bool object_db_load(const char* file_name);

#endif