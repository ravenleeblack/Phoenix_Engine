#ifndef PHOENIX_SYSTEM_TERRAIN_H
#define PHOENIX_SYSTEM_TERRAIN_H

#include "engine_terrain.h"

#endif

#if 0

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <vulkan/vulkan.h>
#include "system_math.h"
#include "system_element.h"
#include "system_scene_objects.h"
#include "system_edit_manager.h"
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

/* ------------------------------------------------------------------------- */
/* Layout                                                                    */
/*   World = 10500 x 10500 cubes, 400 cubes high, every cube system_unit_m (4 mesh) */
/*   Terrain (scene object, empty container)                                 */
/*     region_00 .. region_08   3 x 3 scene objects, each owns a height block */
/*                              in the object database (3500 x 3500 columns)  */
/*       chunks                 500 x 500 columns each, 7 x 7 per region     */
/*                              (data, not scene objects)                     */
/* The ground is ONE height per column, stored per region. Cubes are not stored: */
/* every column is turned into cube faces (terrain_quad) when its chunk is meshed. */
/* The world is created FLAT (every height 0). Edit  */
/* Mode change the heights; nothing is procedural any more.                  */
/* ------------------------------------------------------------------------- */
#define TERRAIN_UNITS_PER_METER        16
#define TERRAIN_MAX_LAYERS             10
#define TERRAIN_REGION_SIDE_CHUNKS     3        /* ONE REGION = 3 x 3 chunks = 9 chunks = 300 x 300 cubes. The world is a grid of 35 x 35 such regions (10500 cubes).
                                                   Only the region the camera is over is meshed and edited, so 9 chunks are alive at any time. */
#define TERRAIN_MAX_LOAD_RADIUS        32       /* (no longer limits anything: every chunk of every region is meshed, flat chunks cost 4 vertices) */
#define TERRAIN_SUBSTEPS               1        /* heights snap to whole cubes: every face is one cell, 4 x 4 mesh. (8 = old 1/8 cell steps) */
#define TERRAIN_CHUNK_CELLS            100      /* a chunk is 100 x 400 x 100 cubes (edit grid = 101 x 101 verts) */
#define TERRAIN_REGION_GRID            3        /* 3 x 3 regions */
#define TERRAIN_REGION_COUNT           9
#define TERRAIN_DEFAULT_REGION_CHUNKS  35       /* chunks per region side: 35 x 35 = 1225 chunks per region. 3 x 35 x 100 = 10500 cubes per world side (same world as before) */
#define TERRAIN_MAX_REGION_CHUNKS      35       /* 35 x 100 = 3500 cells per region side: 3501^2 floats = 49 MB per region. 36 would be 52 MB; keep under the 64 MB DB block cap */
#define TERRAIN_SUB_STEPS              16       /* a top corner can sit 0..16 sixteenths of a cube above its cube layer (see terrain_quad.sub) */
#define TERRAIN_MAX_TUNNELS            64

typedef enum {
    TERRAIN_LAYER_TERRAIN = 0,
    TERRAIN_LAYER_TREES,
    TERRAIN_LAYER_GRASS,
    TERRAIN_LAYER_BUILDINGS,
    TERRAIN_LAYER_PROPS,
    TERRAIN_LAYER_ATMOSPHERE,
    TERRAIN_LAYER_TUNNELS,
    TERRAIN_LAYER_TUNNEL_LIGHTING,
    TERRAIN_LAYER_NPCS_A,
    TERRAIN_LAYER_NPCS_B
} terrain_layer_id;

typedef enum {
    TERRAIN_CELL_AIR = 0,
    TERRAIN_CELL_GRASS,
    TERRAIN_CELL_DIRT,
    TERRAIN_CELL_STONE,
    TERRAIN_CELL_SAND,
    TERRAIN_CELL_TYPE_COUNT
} terrain_cell_type;

/* OPENWORLD: smooth surface.  SANDBOX: cube columns, every column top sits on a whole cube and
   brush / edit-mode results are snapped to cubes. Same heights either way: switching is lossless. */
typedef enum {
    TERRAIN_MODE_OPENWORLD = 0,
    TERRAIN_MODE_SANDBOX
} terrain_mode;

typedef struct { int32_t x, y, z; } terrain_location_q;

/* Stored in the object database under the Terrain object (OBJECT_DATA_TERRAIN).
   Fields marked (legacy) are no longer used by the generator but kept so other systems that read them still compile. */
typedef struct {
    terrain_mode mode;
    int   cells_x, cells_y, cells_z;   /* x/z are derived from region_chunks, y is the vertical limit (400 cubes) */
    float cell_size_m;
    int   chunk_cells;                 /* always TERRAIN_CHUNK_CELLS */
    int   load_radius;                 /* chunks around the camera that get a mesh (0 = camera chunk only, 1 = 3x3). One flat chunk is ~1M verts. */

    float mountain_amount;             /* (legacy) */
    float canyon_amount;               /* (legacy) */
    float river_amount;                /* (legacy) */
    float overhang_amount;             /* (legacy) */
    int   world_seed;                  /* (legacy) */

    int   smooth_iterations;           /* (legacy) */
    float smooth_strength;             /* (legacy) */
    float smooth_max_offset;           /* (legacy) */

    int   hs_blur_iterations;          /* (legacy) */
    float hs_blur_strength;            /* (legacy) */
    float hs_max_slope_deg;            /* (legacy) */
    int   hs_slope_iterations;         /* (legacy) */

    float sun_azimuth_deg;             /* baked surface colour */
    float sun_elevation_deg;
    float shade_amount;
    float rock_slope_deg;
    float sand_line_m;
    float snow_line_m;

    terrain_location_q location;

    int   region_chunks;               /* chunks per region side */
    int   edit_stride;                 /* (ignored: Edit Mode always uses one vertex per cell) */
} terrain_settings;

typedef struct {
    int   cells_x, cells_y, cells_z;
    int   chunk_cells;
    int   chunks_x, chunks_z;
    int   origin_x, origin_z;          /* cell index of world (0,0): the middle of region_04 */
    bool  evenly_divisible;
    float size_x_m, size_y_m, size_z_m;
    int   region_chunks;               /* chunks per region side */
    int   region_cells;                /* cells per region side */
} terrain_dims;

/* ONE face of ONE cube. Everything that draws, picks or edits terrain faces works from these.
   A face is always cell_size_m x cell_size_m (4 x 4 mesh) and always gets its own 4 vertices. A cube with all six faces = 24 verts.
   Faces that are buried inside solid ground are simply not in the array. */
typedef enum {
    TERRAIN_FACE_TOP = 0,      /* +Y */
    TERRAIN_FACE_BOTTOM,       /* -Y */
    TERRAIN_FACE_WEST,         /* -X */
    TERRAIN_FACE_EAST,         /* +X */
    TERRAIN_FACE_NORTH,        /* -Z */
    TERRAIN_FACE_SOUTH,        /* +Z */
    TERRAIN_FACE_COUNT
} terrain_face;

typedef struct {
    int32_t lx, lz;            /* cube column inside the chunk, 0..499 */
    int32_t ly;                /* cube layer, 0..399. Layer 200 starts at y = 0 mesh on a world with 400 layers */
    int32_t face;              /* terrain_face */
    int32_t material;          /* terrain_cell_type */
    int32_t sub[4];            /* TOP faces only: raise of each top corner in 1/TERRAIN_SUB_STEPS of a cube, 0..16.
                                  Corner order = terrain_quad_corners(): (x0,z0) (x0,z1) (x1,z1) (x1,z0). All 0 = a flat cube top. */
    int32_t reserved[2];       /* free for real data (paint id, region id, flags ...) */
} terrain_quad;

typedef struct {
    uint8_t*  cells;        /* layers 2..10 only (light layer etc.) */
    uint16_t* heights;      /* unused: heights live in the object database */
    bool      dirty;
    bool      modified;
    bool      resident;
    bool      mesh_built;
    editor_vertex* mesh_v;
    uint32_t*      mesh_i;
    uint32_t       mesh_vc, mesh_ic;
    terrain_quad*  quads;       /* cube faces of this chunk (full detail sandbox chunks only, else NULL) */
    uint32_t       quad_count;
} terrain_chunk;

typedef struct {
    bool           visible;
    terrain_chunk* chunks;
} terrain_layer;

/* A tunnel is a straight tube between two points on the surface (terrain-local metres).
   The surface gets a round hole at each end. */
typedef struct { float ax, ay, az, bx, by, bz, radius; } terrain_tunnel;

typedef struct {
    bool             exists;
    terrain_settings settings;
    terrain_dims     dims;
    terrain_layer    layers[TERRAIN_MAX_LAYERS];
    uint32_t         scene_object_id;      /* the "Terrain" container */
    VkDevice         device;
    VkPhysicalDevice physical_device;
    uint32_t         last_vertex_count;
    uint32_t         last_index_count;

    bool region_valid;
    int  center_cx, center_cz;
    int  region_cx0, region_cx1, region_cz0, region_cz1;
    int  resident_chunks;

    /* region_00 .. region_08 */
    uint32_t region_ids[TERRAIN_REGION_COUNT];
    bool     region_dirty[TERRAIN_REGION_COUNT];
    int      region_resident[TERRAIN_REGION_COUNT];   /* chunks of that region currently meshed */

    bool     builder_bound;                 /* World Builder is showing this terrain's options */
    bool     edited;                        /* heights were changed since the last flat reset */
    uint32_t edit_revision;                 /* bumped when tools change heights (Edit Mode re-syncs) */
    int      edit_cx, edit_cz;              /* chunk Edit Mode works on */
    bool     edit_follow;                   /* edit chunk follows the camera while nothing is obj_vert_selected */

    terrain_tunnel tunnels[TERRAIN_MAX_TUNNELS];
    int            tunnel_count;

    bool           region_off[TERRAIN_REGION_COUNT];   /* true = this region gets no chunk meshes at all (Scene Archive On / Off / Solo) */
} system_terrain;

extern system_terrain   g_terrain;
extern terrain_settings g_terrain_ui;       /* defaults used the next time a terrain is created */

/* ---- settings / dims ---- */
void         terrain_settings_defaults(terrain_settings* s);
terrain_dims terrain_resolve_dims(const terrain_settings* s);
const char*  terrain_layer_name(int layer);
void         terrain_region_name(int region, char* out, size_t out_size);   /* "region_00" */

/* ---- lifetime ---- */
bool create_terrain_system(void);      /* toolbar / menubar. Creates a FLAT terrain, or just re-selects the existing one. Never wipes edits. */
bool spawn_terrain_system(const terrain_settings* s, VkDevice device, VkPhysicalDevice physical_device);   /* (re)creates flat */
bool reset_terrain_to_flat(void);         /* World Builder "Reset to flat" */
bool rebuild_terrain_mesh(void);
void update_terrain_streaming(void);   /* once per frame (as before) */
void shutdown_terrain_system(void);
bool delete_terrain(void);             /* removes the Terrain object, all 9 regions, their database blocks and the CPU data. Use this instead of delete_scene_object() for the Terrain. */

/* ---- World Builder ---- */
void bind_terrain_builder(uint32_t object_id);       /* call from a drop handler: accepts the Terrain object or any region */
bool is_terrain_object(const system_scene_object* obj);
void set_terrain_mode(terrain_mode mode);
bool set_terrain_region_chunks(int region_chunks);   /* only while nothing was edited */
void set_terrain_load_radius(int radius);
void set_terrain_edit_chunk(int cx, int cz);
void set_terrain_edit_stride(int stride);

/* ---- regions: On / Off / Solo from the Scene Archive rows ---- */
int   region_of_terrain_object(uint32_t object_id);   /* 0..8, or -1 when the object is not a region_xx */
int   terrain_camera_region(void);                    /* the region the camera is over, -1 if unknown */
bool  terrain_region_is_enabled(int region);
void  terrain_region_set_enabled(int region, bool on);
void  terrain_region_solo(int region);                /* only this region stays on */
void  terrain_region_show_all(void);

/* ---- terrain paint layer ---- */
/* One paint dab. center is terrain-local metres. Paint is stored per cube column (4 x 4 mesh): every face of that column takes the colour.
   only_region -1 = any region, 0..8 = stay inside that region. Returns true when a colour changed. */
bool  terrain_paint_dab(editor_vec3 center_local, float radius, const float color[3], float opacity, float hardness, bool erase, int only_region);

/* ---- heights (corner grid, terrain-local metres) ---- */
/*In sandbox mode height is adjustable by steps of 1/16 of 4m, meaning we can slope things, by one side top being 0,0 on verts while other is at 1/6 or 1.4 
as long as top never reaches 0, 0, 0, 0, for all top verts other wise that cube is all aire except bottom. WHich bottom shares with neighbor so now its the
neighbor bottom top. cube is gone.*/
float terrain_corner_height(int gx, int gz);
void  terrain_set_corner_height(int gx, int gz, float y_m);      /* snaps in SANDBOX mode, clamps to the vertical limit */
float terrain_sample_height_m(float local_x_m, float local_z_m); /* SANDBOX: top of the column under the point. OPENWORLD: bilinear */
void  terrain_mark_dirty_rect(int gx0, int gz0, int gx1, int gz1);   /* corners, inclusive */
void  terrain_touch(void);                                       /* heights changed: flag edited + bump revision */
bool  terrain_root_position(editor_vec3* out);                   /* the Terrain object's position (its location) */
bool  terrain_add_tunnel(const terrain_tunnel* t);

/* ---- cube faces ---- */
/* The quads of one chunk (only chunks meshed at full detail in SANDBOX mode have them). Returns the face_type, *out = the array or NULL. */
uint32_t get_terrain_chunk_quads(int cx, int cz, const terrain_quad** out);
/* The 4 corners of a quad, terrain-local metres, counter-clockwise seen from outside. The only place that knows face geometry. */
void     terrain_quad_corners(int cx, int cz, const terrain_quad* q, float out[4][3]);

/* ---- Edit Mode bridge ---- */
edit_mode_mesh* terrain_edit_mesh_acquire(system_scene_object* obj);    /* NULL unless obj is the Terrain or a region */
bool     terrain_commit_edit_mesh(edit_mode_mesh* mesh, system_scene_object* obj);
/* The edit mesh Edit Mode is working on right now (NULL if none yet). Does NOT rebuild or re-sync, so the transform gizmo can call it every frame. */
edit_mode_mesh* terrain_edit_mesh_current(void);

/* ---- cells (layers 2..10; the terrain surface itself is the height grid) ---- */
uint8_t terrain_get_cell(const system_terrain* t, int layer, int gx, int gy, int gz);
bool    terrain_set_cell(int layer, int gx, int gy, int gz, uint8_t type);
void    terrain_fill_box(int layer, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t type);

void terrain_apply_location(void);
void terrain_set_location(terrain_location_q loc);

#endif