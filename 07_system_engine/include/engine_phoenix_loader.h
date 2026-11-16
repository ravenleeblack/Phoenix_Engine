#ifndef PHOENIX_ENGINE_LOADER_H
#define PHOENIX_ENGINE_LOADER_H

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

#ifdef PHOENIX_SYSTEM_LOADER_DLL
#define PHOENIX_LOADER_API __declspec(dllexport)
#else
#define PHOENIX_LOADER_API __declspec(dllimport)
#endif

typedef struct {
    float pos[3];
    float color[4];
    float uv[2];
    float normal[3];
} loader_vertex;                 /* 48 bytes: same layout as editor_vertex, so an imported mesh uploads as is */

typedef struct {
    loader_vertex* vertices;
    uint32_t*      indices;
    uint32_t       vertex_count;
    uint32_t       index_count;
} loader_mesh_data;

typedef struct {
    uint8_t* pixels; // Raw uncompressed sequential byte array buffer (BGRA or RGBA)
    int      width;
    int      height;
    int      channels;
} loader_image_data;

// Clean Exported Function Signatures
// load_3d_mesh_asset reads .fbx (binary or ascii, mesh geometry only). positions come out in meters.
PHOENIX_LOADER_API bool load_3d_mesh_asset(const char* filepath, loader_mesh_data* out_mesh);
PHOENIX_LOADER_API void remove_3d_mesh_asset(loader_mesh_data* mesh);
PHOENIX_LOADER_API bool save_3d_mesh_asset(const char* filepath, const loader_mesh_data* mesh, bool binary);

// load_image_asset reads .png and uncompressed .tga into 4 channel pixels. save_image_asset writes a .png.
PHOENIX_LOADER_API bool load_image_asset(const char* filepath, loader_image_data* out_image);
PHOENIX_LOADER_API void remove_image_asset(loader_image_data* image);
PHOENIX_LOADER_API bool save_image_asset(const char* filepath, const loader_image_data* image);

typedef bool (*load_image_asset_ptr)(const char* filepath, loader_image_data* out_image);
typedef void (*remove_image_asset_ptr)(loader_image_data* image);
typedef bool (*save_image_asset_ptr)(const char* filepath, const loader_image_data* image);
typedef bool (*load_3d_mesh_asset_ptr)(const char* filepath, loader_mesh_data* out_mesh);
typedef void (*remove_3d_mesh_asset_ptr)(loader_mesh_data* mesh);
typedef bool (*save_3d_mesh_asset_ptr)(const char* filepath, const loader_mesh_data* mesh, bool binary);

typedef struct loader_plugin
{
    load_image_asset_ptr load_image_asset;
    remove_image_asset_ptr remove_image_asset;
    load_3d_mesh_asset_ptr load_3d_mesh_asset;
    remove_3d_mesh_asset_ptr remove_3d_mesh_asset;
} loader_plugin;

#endif

