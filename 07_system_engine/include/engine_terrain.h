#ifndef phoenix_engine_terrain_h
#define phoenix_engine_terrain_h

#include "engine_defines.h"
#include "engine_math.h"            /* editor_vec3, editor_mat4, editor_vertex */

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
/* layout                                                                    */
/*   world = 10500 x 10500 cubes, 400 cubes high, every cube system_unit_m (4 mesh) */
/*   terrain (scene object, empty container)                                 */
/*     region_00 .. region_08   3 x 3 scene objects, each owns a height block */
/*                              in the object database (3500 x 3500 columns)  */
/*       chunks                 100 x 100 columns each, 35 x 35 per region   */
/*                              (data, not scene objects)                     */
/* the ground is one height per column, stored per region. cubes are not stored: */
/* every column is turned into cube faces (terrain_quad) when its chunk is meshed. */
/* the world is created flat (every height 0). edit mode changes the heights; */
/* nothing is procedural any more.                                           */
/* ------------------------------------------------------------------------- */
#define terrain_units_per_meter        16
#define terrain_max_layers             10
#define terrain_region_side_chunks     3        /* one region = 3 x 3 chunks = 9 chunks = 300 x 300 cubes. the world is a grid of 35 x 35 such regions (10500 cubes).
                                                   only the region the camera is over is meshed and edited, so 9 chunks are alive at any time. */
#define terrain_max_load_radius        32       /* (no longer limits anything: every chunk of every region is meshed, flat chunks cost 4 vertices) */
#define terrain_substeps               1        /* heights snap to whole cubes: every face is one cell, 4 x 4 mesh. (8 = old 1/8 cell steps) */
#define terrain_chunk_cells            100      /* a chunk is 100 x 400 x 100 cubes (edit grid = 101 x 101 verts) */
#define terrain_region_grid            3        /* 3 x 3 regions */
#define terrain_region_count           9
#define terrain_default_region_chunks  35       /* chunks per region side: 35 x 35 = 1225 chunks per region. 3 x 35 x 100 = 10500 cubes per world side (same world as before) */
#define terrain_max_region_chunks      35       /* 35 x 100 = 3500 cells per region side: 3501^2 floats = 49 mb per region. 36 would be 52 mb; keep under the 64 mb db block cap */
#define terrain_sub_steps              16       /* a top corner can sit 0..16 sixteenths of a cube above its cube layer (see terrain_quad.sub) */
#define terrain_max_tunnels            64

enum terrain_constant
{
    terrain_regions_use_world_position = 1,
    terrain_region_file_magic = 0x4e474552u,
    terrain_region_file_version = 1u,
    terrain_region_file_header = 16u,
    terrain_bake_skin_color = 0,
    terrain_tube_segments = 12,
    terrain_no_layer = -1000000
};

/* set to 0 if your renderer multiplies a child's transform by its parent's.
   1 = every region object carries the terrain's world position itself (what edit mode assumes: it only reads obj->position). */
#define terrain_regions_use_world_position 1


/* ------------------------------------------------------------------------- */
/* height storage: one float grid per region, held by the object database.   */
/* region block = [header 16 bytes][(region_cells+1)^2 floats]. a corner     */
/* belongs to the region that owns the cell to its +x/+z side (the last       */
/* region also owns the far edge_to_face_array), so no corner is ever stored twice.         */
/* a zeroed block is a flat world.                                           */
/* in sandbox mode the value at corner (gx,gz) is the height of the cube      */
/* column (gx,gz).                                                           */
/* ------------------------------------------------------------------------- */
#define region_magic   0x4e474552u      /* "regn" */
#define region_version 1u
#define region_header_size  16u

typedef struct region_header
{
    uint32_t magic;
    uint32_t version;
    uint32_t side;
    uint32_t reserved;
} region_header;

/* 0 = terrain has no baked skin: every vertex gets the neutral empty-skin grey (system_skin_default.h) and the look comes from the skin
   assigned to the terrain (shader profile). 1 = the old grass / rock / sand / snow bake. */
#define terrain_bake_skin_color 0

/* ------------------------------------------------------------------------- */
/* tunnels: holes in the surface + a tube                                    */
/* ------------------------------------------------------------------------- */
#define tube_segments 12


/* ------------------------------------------------------------------------- */
/* sandbox: cube faces.                                                      */
/* every column is a stack of cubes. a column of height l (top cube layer) shows:                              */
/*   - one top face on its top cube                                                                            */
/*   - one wall face per cube where the neighbouring column is lower (so a 5 cube cliff = 5 faces, each 4 x 4 mesh) */
/* those faces are the terrain_quad array of the chunk. the mesh is built from that array: 4 verts per quad.   */
/* ------------------------------------------------------------------------- */
#define terrain_no_layer (-1000000)






typedef enum {
    terrain_layer_terrain = 0,
    terrain_layer_trees,
    terrain_layer_grass,
    terrain_layer_buildings,
    terrain_layer_props,
    terrain_layer_atmosphere,
    terrain_layer_tunnels,
    terrain_layer_tunnel_lighting,
    terrain_layer_npcs_a,
    terrain_layer_npcs_b
} terrain_layer_id;

typedef enum {
    terrain_cell_air = 0,
    terrain_cell_grass,
    terrain_cell_dirt,
    terrain_cell_stone,
    terrain_cell_sand,
    terrain_cell_type_count
} terrain_cell_type;

/* openworld: smooth surface.  sandbox: cube columns, every column top sits on a whole cube and
   brush / edit-mode results are snapped to cubes. same heights either way: switching is lossless. */
typedef enum {
    terrain_mode_openworld = 0,
    terrain_mode_sandbox
} terrain_mode;

typedef struct { int32_t x, y, z; } terrain_location_q;

/* stored in the object database under the terrain object (the object data terrain type).
   fields marked (legacy) are no longer used by the generator but kept so other systems that read them still compile. */
typedef struct {
    terrain_mode mode;
    int   cells_x, cells_y, cells_z;   /* x/z are derived from region_chunks, y is the vertical limit (400 cubes) */
    float cell_size_m;
    int   chunk_cells;                 /* always terrain_chunk_cells */
    int   load_radius;                 /* chunks around the camera that get a mesh (0 = camera chunk only, 1 = 3x3). one flat chunk is ~1m verts. */

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
    int   edit_stride;                 /* (ignored: edit mode always uses one vertex per cell) */
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

/* one face of one cube. everything that draws, picks or edits terrain faces works from these.
   a face is always cell_size_m x cell_size_m (4 x 4 mesh) and always gets its own 4 vertices. a cube with all six faces = 24 verts.
   faces that are buried inside solid ground are simply not in the array. */
typedef enum {
    terrain_face_top = 0,      /* +y */
    terrain_face_bottom,       /* -y */
    terrain_face_west,         /* -x */
    terrain_face_east,         /* +x */
    terrain_face_north,        /* -z */
    terrain_face_south,        /* +z */
    terrain_face_count
} terrain_face;

typedef struct {
    int32_t lx, lz;            /* cube column inside the chunk, 0..99 */
    int32_t ly;                /* cube layer, 0..399. layer 200 starts at y = 0 mesh on a world with 400 layers */
    int32_t face;              /* terrain_face */
    int32_t material;          /* terrain_cell_type */
    int32_t sub[4];            /* top faces only: raise of each top corner in 1/terrain_sub_steps of a cube, 0..16.
                                  corner order = get_terrain_quad_corners(): (x0,z0) (x0,z1) (x1,z1) (x1,z0). all 0 = a flat cube top. */
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
    terrain_quad*  quads;       /* cube faces of this chunk (full detail sandbox chunks only, else null) */
    uint32_t       quad_count;
} terrain_chunk;

typedef struct {
    bool           visible;
    terrain_chunk* chunks;
} terrain_layer;

/* a tunnel is a straight tube between two points on the surface (terrain-local metres).
   the surface gets a round hole at each end. */
typedef struct { float ax, ay, az, bx, by, bz, radius; } terrain_tunnel;

typedef struct {
    bool             exists;
    terrain_settings settings;
    terrain_dims     dims;
    terrain_layer    layers[terrain_max_layers];
    uint32_t         scene_object_id;      /* the terrain container */
    VkDevice         device;
    VkPhysicalDevice physical_device;
    uint32_t         last_vertex_count;
    uint32_t         last_index_count;

    bool region_valid;
    int  center_cx, center_cz;
    int  region_cx0, region_cx1, region_cz0, region_cz1;
    int  resident_chunks;

    /* region_00 .. region_08 */
    uint32_t region_ids[terrain_region_count];
    bool     region_dirty[terrain_region_count];
    int      region_resident[terrain_region_count];   /* chunks of that region currently meshed */

    bool     builder_bound;                 /* world builder is showing this terrain's options */
    bool     edited;                        /* heights were changed since the last flat reset */
    uint32_t edit_revision;                 /* bumped when tools change heights (edit mode re-syncs) */
    int      edit_cx, edit_cz;              /* chunk edit mode works on */
    bool     edit_follow;                   /* edit chunk follows the camera while nothing is obj_vert_selected */

    terrain_tunnel tunnels[terrain_max_tunnels];
    int            tunnel_count;

    bool           region_off[terrain_region_count];   /* true = this region gets no chunk meshes at all (scene archive on / off / solo) */
} system_terrain;

/* g_terrain and g_terrain_ui: extern lives in engine_globals.h, init lives in engine_globals.c.
   g_terrain_ui holds the defaults used the next time a terrain is created. */

/* ---- settings / dims ---- */
void         set_terrain_settings_defaults(terrain_settings* s);
terrain_dims resolve_terrain_dims(const terrain_settings* s);
const char*  get_terrain_layer_name(int layer);
void         get_terrain_region_name(int region, char* out, size_t out_size);   /* "region_00" */

/* ---- lifetime ---- */
bool create_terrain_system(void);      /* toolbar / menubar. creates a flat terrain, or just re-selects the existing one. never wipes edits. */
bool spawn_terrain_system(const terrain_settings* s, VkDevice device, VkPhysicalDevice physical_device);   /* (re)creates flat */
bool reset_terrain_to_flat(void);         /* world builder reset to flat */
bool rebuild_terrain_mesh(void);
void update_terrain_streaming(void);   /* once per frame (as before) */
void shutdown_terrain_system(void);
bool delete_terrain(void);             /* removes the terrain object, all 9 regions, their database blocks and the cpu data. use this instead of delete_scene_object() for the terrain. */

/* ---- world builder ---- */
void bind_terrain_builder(uint32_t object_id);       /* call from a drop handler: accepts the terrain object or any region */
void set_terrain_mode(terrain_mode mode);
bool set_terrain_region_chunks(int region_chunks);   /* only while nothing was edited */
void set_terrain_load_radius(int radius);
void set_terrain_edit_chunk(int cx, int cz);
void set_terrain_edit_stride(int stride);

/* ---- regions: on / off / solo from the scene archive rows ---- */
int   get_terrain_region_of_object(uint32_t object_id);   /* 0..8, or -1 when the object is not a region_xx */
int   get_terrain_camera_region(void);                    /* the region the camera is over, -1 if unknown */
bool  is_terrain_region_enabled(int region);
void  set_terrain_region_enabled(int region, bool on);
void  solo_terrain_region(int region);                    /* only this region stays on */
void  show_all_terrain_regions(void);

/* ---- terrain paint layer ---- */
/* one paint dab. center is terrain-local metres. paint is stored per cube column (4 x 4 mesh): every face of that column takes the colour.
   only_region -1 = any region, 0..8 = stay inside that region. returns true when a colour changed. */
bool  paint_terrain_dab(editor_vec3 center_local, float radius, const float color[3], float opacity, float hardness, bool erase, int only_region);

/* ---- heights (corner grid, terrain-local metres) ---- */
/* in sandbox mode height is adjustable by steps of 1/16 of 4m, meaning we can slope things, by one side top being 0,0 on verts while other is at 1/6 or 1.4
   as long as top never reaches 0, 0, 0, 0, for all top verts otherwise that cube is all air except bottom. which bottom shares with neighbor so now its the
   neighbor bottom top. cube is gone. */
float get_terrain_corner_height(int gx, int gz);
void  set_terrain_corner_height(int gx, int gz, float y_m);      /* snaps in sandbox mode, clamps to the vertical limit */
float sample_terrain_height_m(float local_x_m, float local_z_m); /* sandbox: top of the column under the point. openworld: bilinear */
void  mark_terrain_dirty_rect(int gx0, int gz0, int gx1, int gz1);   /* corners, inclusive */
void  touch_terrain(void);                                       /* heights changed: flag edited + bump revision */
bool  get_terrain_root_position(editor_vec3* out);               /* the terrain object's position (its location) */
float terrain_sample_height_m(float local_x_m, float local_z_m);
bool terrain_root_position(editor_vec3* out);
bool  add_terrain_tunnel(const terrain_tunnel* t);

/* ---- cube faces ---- */
/* the quads of one chunk (only chunks meshed at full detail in sandbox mode have them). returns the quad count, *out = the array or null. */
uint32_t get_terrain_chunk_quads(int cx, int cz, const terrain_quad** out);
/* the 4 corners of a quad, terrain-local metres, counter-clockwise seen from outside. the only place that knows face geometry. */
void     get_terrain_quad_corners(int cx, int cz, const terrain_quad* q, float out[4][3]);

/* the edit mode bridge and is_terrain_object use editor types (edit_mode_mesh, system_scene_object),
   so they are declared in the editor's system_scene_terrain.h, not here. */

/* ---- cells (layers 2..10; the terrain surface itself is the height grid) ---- */
uint8_t get_terrain_cell(const system_terrain* t, int layer, int gx, int gy, int gz);
bool    set_terrain_cell(int layer, int gx, int gy, int gz, uint8_t type);
void    fill_terrain_box(int layer, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t type);

void apply_terrain_location(void);
void set_terrain_location(terrain_location_q loc);

#endif