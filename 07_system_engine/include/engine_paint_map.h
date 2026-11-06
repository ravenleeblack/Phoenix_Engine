#ifndef PHOENIX_SYSTEM_PAINT_MAP_H
#define PHOENIX_SYSTEM_PAINT_MAP_H

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

/* A paint map (.pmap) comes in TWO kinds. Same file extension, same folder, the kind is stored in the header.

   pmap_kind_id     LOW RES, HARD EDGES. One byte per texel = a region id (0 unpainted, 1..255 from the palette: rock, dirt, grass).
                    Decides WHERE a skin layer or a scatter rule applies. Never blurred, never blended: ids cannot blend.
                    Goes to object_paint_data.id_map. A skin layer picks a region with PROFILE_MASK_ID_COLOR (id_color = the region's
                    palette colour), so palette colours are the KEY and must be unique inside one map.
   pmap_kind_paint  HIGH RES, SOFT. RGBA8 per texel, straight (not premultiplied) alpha = how painted. Hand painted colour detail,
                    for example painting the terrain yourself. Goes to object_paint_data.paint[] (PROFILE_SRC_PAINT_MAP / PROFILE_MASK_PAINT_MAP).
                    Painted with a soft brush (hardness) and softened / resampled on export.

   A map belongs to the mesh (or terrain) it was painted on: it lives in that object's UV (terrain: XZ) space.
   Like skins: map id never reused, stored inside the file, so an object keeps pointing at the same map after a restart. */

typedef enum { pmap_kind_id = 0, pmap_kind_paint = 1 } enum_pmap_kind;

enum { pmap_max_regions = 32, pmap_name_len = 24, pmap_max_size_id = 1024, pmap_max_size_paint = 4096 };
#define PMAP_FILE_EXT     ".pmap"
#define PMAP_FILE_VERSION 1u

typedef struct {
    uint8_t id;                         /* 1..255 */
    char    name[pmap_name_len];
    uint8_t color[4];                   /* unique per map. shown while painting AND the key PROFILE_MASK_ID_COLOR matches */
} pmap_region;

typedef struct {
    uint32_t    id;                     /* never reused, 0 = none. objects store THIS */
    uint8_t     kind;                   /* enum_pmap_kind */
    char        name[pmap_name_len];    /* also the file name: assets/paintmaps/<name>.pmap */
    uint16_t    width, height;
    uint8_t*    texels;                 /* id: width*height bytes. paint: width*height*4 bytes RGBA. row 0 first */
    int         region_count;           /* id kind only */
    pmap_region regions[pmap_max_regions];
    uint32_t    revision;               /* +1 on every edit. GPU copy rebuilds when it differs from what it was built from */
    uint32_t    saved_revision;         /* differs = unsaved (shown as *) */
} paint_map;

bool paint_map_create(paint_map* mesh, const char* name, enum_pmap_kind kind, int width, int height);   /* id kind starts with "region 1" */
void paint_map_free(paint_map* mesh);
bool paint_map_is_dirty(const paint_map* mesh);
void paint_map_clear(paint_map* mesh);                                          /* both kinds: back to unpainted */

/* ---- id kind: palette ---- */
int  paint_map_add_region(paint_map* mesh, const char* name, const uint8_t color[4]);   /* color NULL = picks a distinct one. returns the region id, 0 when full / colour already used / wrong kind */
const pmap_region* paint_map_find_region(const paint_map* mesh, uint8_t region_id);
bool paint_map_remove_region(paint_map* mesh, uint8_t region_id);                       /* its texels go back to 0 */

/* ---- id kind: painting. u, vert_to_edge_array 0..1, radius in uv units. hard circle, no blending ---- */
void paint_map_stamp(paint_map* mesh, float u, float vert_to_edge_array, float radius_uv, uint8_t region_id);
void paint_map_stamp_line(paint_map* mesh, float u0, float v0, float u1, float v1, float radius_uv, uint8_t region_id);
void paint_map_fill(paint_map* mesh, uint8_t region_id);
/* what the id_map texture asset holds: every texel = its region's palette colour (alpha 255), unpainted = 0,0,0,0.
   out must hold width*height*4 bytes. Upload with NEAREST filtering. PROFILE_MASK_ID_COLOR then compares against region colour. */
bool paint_map_id_to_rgba(const paint_map* mesh, uint8_t* out);

/* ---- paint kind: soft brush. hardness 0..0.99 (1 = hard edge_to_face_array), opacity 0..1, colour rgb 0..1 (g_paint.color) ---- */
void paint_map_paint_stamp(paint_map* mesh, float u, float vert_to_edge_array, float radius_uv, float hardness, float opacity, const float color[3], bool erase);
void paint_map_paint_line(paint_map* mesh, float u0, float v0, float u1, float v1, float radius_uv, float hardness, float opacity,
                          float spacing /* fraction of the brush diameter, g_paint.spacing */, const float color[3], bool erase);

/* ---- export: the two kinds, each with its own options ---- */
typedef struct {
    uint8_t kind;           /* must equal the map's kind */
    int     resolution;     /* width in texels (height keeps the aspect). id 16..1024, paint 16..4096 */
    float   softness;       /* PAINT ONLY, 0..1. blur radius = softness * 1% of the width. id exports ignore it: edges stay hard */
} pmap_export_settings;

void pmap_export_defaults(pmap_export_settings* s, enum_pmap_kind kind);   /* id: 256, hard.  paint: 2048, softness 0 */
int  pmap_resolution_count(enum_pmap_kind kind);                           /* the presets for the UI picker */
int  pmap_resolution_at(enum_pmap_kind kind, int index);                   /* id: 64 128 256 512 1024.  paint: 512 1024 2048 4096 */

/* Builds the exported copy: id = nearest sampling (hard), paint = area average when shrinking / bilinear when growing, then softness. */
bool paint_map_export(const paint_map* src, const pmap_export_settings* s, paint_map* out);
/* export + save + free in one call. on success the source is marked clean. */
bool paint_map_export_to_file(const char* path, paint_map* src, const pmap_export_settings* s);

/* ---- file. false on any failure, out is left untouched ---- */
bool paint_map_save(const char* path, paint_map* mesh);
bool paint_map_load(const char* path, paint_map* out);

#endif