#ifndef PHOENIX_SYSTEM_SHADER_PROFILE_H
#define PHOENIX_SYSTEM_SHADER_PROFILE_H

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

/* ------------------------------------------------------------------------- */
/* Shader profiles and materials. Design notes: shader_profile_design.md      */
/*                                                                           */
/*   Material  = a texture pool + TWO slots (skin, wire)                     */
/*   Slot      = which shader profile to use + its values + its texture binds */
/*   Profile   = a recipe: SKIN (layer stack) or WIRE (edge_to_face_array rule + ribbon)    */
/*   Object    = owns a material AND the per-object paint data (splat/paint)  */
/*                                                                           */
/* Profiles are DATA. They hold no code. The textureshop writes them, the    */
/* renderer reads them. Fixed-size arrays everywhere, so a profile is one     */
/* flat blob that can be saved, copied and hashed with memcpy.                */
/* ------------------------------------------------------------------------- */

#define PROFILE_NAME_LEN        64
#define PROFILE_LAYER_NAME_LEN  32
#define PROFILE_MAX_LAYERS      32     /* layers one profile AUTHORS. Raised from 8: the stack is compiled and baked before runtime, see below. */
#define PROFILE_MAX_MASKS       4      /* masks on one layer, multiplied together */
#define PROFILE_MAX_TEX_INPUTS  16     /* named texture inputs a profile can read */
#define PROFILE_MAX_PARAMS      16     /* exposed numbers (tint, width, opacity...) */
#define MATERIAL_TEXTURE_POOL   16     /* textures one material owns */
#define OBJECT_MAX_SPLATMAPS    2      /* RGBA each = 8 masks */
#define OBJECT_MAX_PAINTMAPS    4
#define PROFILE_MAX_INCLUDES    4      /* other profiles one profile pulls in as layers */
#define PROFILE_INCLUDE_MAX_DEPTH 3    /* profile inside profile inside profile. Cycles are rejected when the include is added. */

#define PROFILE_TEX_NONE        0xFFu  /* "no texture": use the layer's constant value instead */
#define PROFILE_FILE_MAGIC      0x46525053u   /* 'SPRF' little endian */
#define PROFILE_FILE_VERSION    1u

/* ------------------------------------------------------------------------- */
/* Shared enums                                                              */
/* ------------------------------------------------------------------------- */

typedef enum {
    PROFILE_KIND_SKIN = 0,      /* draws the triangles */
    PROFILE_KIND_WIRE           /* draws the picked edges as ribbons */
} profile_kind;

/* The PBR outputs a skin layer can write. The index is used for per-channel arrays, the
   bit (1u << index) for channel masks. */
typedef enum {
    PROFILE_OUT_COLOR = 0,
    PROFILE_OUT_NORMAL,
    PROFILE_OUT_ROUGHNESS,
    PROFILE_OUT_METALLIC,
    PROFILE_OUT_HEIGHT,
    PROFILE_OUT_AO,
    PROFILE_OUT_EMISSIVE,
    PROFILE_OUT_OPACITY,
    PROFILE_OUT_COUNT
} profile_output;

#define PROFILE_OUT_BIT(o)  (1u << (o))

/* Which channel(s) of a texture to read. Packed maps (ORM etc.) use R/G/B/A per output. */
typedef enum {
    PROFILE_CH_R = 0,
    PROFILE_CH_G,
    PROFILE_CH_B,
    PROFILE_CH_A,
    PROFILE_CH_RGB,
    PROFILE_CH_RGBA
} profile_channel;

/* Blend of one layer's channel onto the result below it. Substance blends every channel on its
   own, so this is stored per output. NORMAL_RNB is for PROFILE_OUT_NORMAL only (reoriented
   normal blending), a plain lerp of normals flattens detail. */
typedef enum {
    PROFILE_BLEND_LERP = 0,     /* mix(below, layer, weight) */
    PROFILE_BLEND_MULTIPLY,
    PROFILE_BLEND_OVERLAY,
    PROFILE_BLEND_ADD,
    PROFILE_BLEND_OVERRIDE,     /* ignore what is below inside the mask */
    PROFILE_BLEND_HEIGHT,       /* weight biased by each side's height: the higher surface wins */
    PROFILE_BLEND_NORMAL_RNB
} profile_blend_mode;

typedef enum {
    PROFILE_SHADING_STANDARD = 0,
    PROFILE_SHADING_CLOTH,      /* sheen lobe, optional anisotropy */
    PROFILE_SHADING_SKIN,       /* later */
    PROFILE_SHADING_FOLIAGE     /* later */
} profile_shading_model;

#define PROFILE_FEATURE_TRIPLANAR_DEFAULT  (1u << 0)
#define PROFILE_FEATURE_HEIGHT_BLEND       (1u << 1)
#define PROFILE_FEATURE_EMISSIVE           (1u << 2)
#define PROFILE_FEATURE_MACRO_VARIATION    (1u << 3)   /* large-scale tint/roughness over the tiling detail */
#define PROFILE_FEATURE_ANISOTROPY         (1u << 4)

/* ------------------------------------------------------------------------- */
/* Sources and masks                                                         */
/* ------------------------------------------------------------------------- */

/* What feeds one output of a layer. A profile layer is not limited to a texture. */
typedef enum {
    PROFILE_SRC_TEXTURE = 0,    /* a texture input of the profile (tex_slot) */
    PROFILE_SRC_CONSTANT,       /* the layer's constant value */
    PROFILE_SRC_NODE_GRAPH,     /* output of a Texture Shop node graph (graph_id, graph_output) */
    PROFILE_SRC_PAINT_MAP       /* an object paint map: tex_slot = index into object_paint_data.paint[] */
} profile_source_kind;

/* Where a value comes from. tex_slot is a profile TEXTURE INPUT (0..PROFILE_MAX_TEX_INPUTS-1),
   NOT a pool index: the material's tex_bindings[] says which pool texture fills that input.
   That is what lets one profile serve many materials. */
typedef struct {
    uint8_t  kind;              /* profile_source_kind */
    uint32_t graph_id;          /* PROFILE_SRC_NODE_GRAPH: node graph asset id */
    uint8_t  graph_output;      /* which Output node of that graph */
    uint8_t  tex_slot;          /* PROFILE_TEX_NONE = use the constant instead */
    uint8_t  channel;           /* profile_channel */
    bool     triplanar;         /* no UVs needed (ground, rocks) */
    float    tiling[2];
    float    offset[2];
} profile_source;

typedef enum {
    PROFILE_MASK_TEXTURE_CHANNEL = 0,   /* one channel of a texture */
    PROFILE_MASK_SPLAT_CHANNEL,         /* one RGBA channel of an object splatmap */
    PROFILE_MASK_PAINT_MAP,             /* an object paint map (also drives wire highlight) */
    PROFILE_MASK_ID_COLOR,              /* flat-colour region id map (cloth parts) */
    PROFILE_MASK_HEIGHT_RANGE,          /* from the stack's height so far */
    PROFILE_MASK_SLOPE,                 /* surface angle: rock on cliffs, grass on flats */
    PROFILE_MASK_CURVATURE,             /* edge_to_face_array wear / grime */
    PROFILE_MASK_AO,
    PROFILE_MASK_WORLD_Y,               /* sand line, snow line */
    PROFILE_MASK_NOISE
} profile_mask_type;

/* Which table tex_slot indexes for a mask. Splat and paint maps belong to the OBJECT. */
typedef enum {
    PROFILE_MASK_SRC_PROFILE_INPUT = 0, /* material texture via tex_bindings[] */
    PROFILE_MASK_SRC_OBJECT_SPLAT,      /* object_paint_data.splat[] */
    PROFILE_MASK_SRC_OBJECT_PAINT       /* object_paint_data.paint[] */
} profile_mask_source;

typedef struct {
    uint8_t  type;              /* profile_mask_type */
    uint8_t  source;            /* profile_mask_source */
    uint8_t  tex_slot;
    uint8_t  channel;           /* profile_channel */
    bool     invert;
    float    lo, hi;            /* remap range (slope in degrees, height / world Y in metres) */
    float    contrast;          /* 1 = unchanged */
    float    id_color[3];       /* PROFILE_MASK_ID_COLOR */
    float    id_tolerance;
    float    noise_scale;       /* PROFILE_MASK_NOISE */
} profile_mask;

/* ------------------------------------------------------------------------- */
/* SKIN profile: a Substance-style layer stack                               */
/* Layers are evaluated bottom (0) to top, each blending into a running set  */
/* of PBR channels. Lighting runs ONCE after the last layer.                 */
/* ------------------------------------------------------------------------- */

typedef struct {
    char           name[PROFILE_LAYER_NAME_LEN];
    bool           enabled;

    uint32_t       channel_mask;                    /* PROFILE_OUT_BIT(...) of the outputs this layer writes */
    profile_source src[PROFILE_OUT_COUNT];          /* texture feeding each output (packed maps: same slot, different channel) */
    float          value[PROFILE_OUT_COUNT];        /* constant used where src.tex_slot == PROFILE_TEX_NONE */
    uint8_t        blend[PROFILE_OUT_COUNT];        /* profile_blend_mode per output */

    float          color_tint[4];
    float          opacity;
    float          rotation;                        /* radians, texture rotation */
    float          rough_mul, rough_add;
    float          height_scale;                    /* strength of this layer's height in PROFILE_BLEND_HEIGHT */

    uint8_t        mask_count;
    profile_mask   masks[PROFILE_MAX_MASKS];        /* all multiplied together */
} skin_layer;

/* Another profile used as a block of layers inside this one (a ground profile that pulls in a rock profile, a cloth
   profile that pulls in a wear profile). The renderer FLATTENS includes into one layer list when the skin is assigned,
   so the GPU still sees one stack. The mask says where the included profile shows. */
typedef struct {
    uint32_t     profile_id;                        /* a PROFILE_KIND_SKIN profile asset, 0 = empty */
    bool         enabled;
    uint8_t      blend;                             /* profile_blend_mode, applied to every output the included profile writes */
    float        opacity;
    uint8_t      mask_count;
    profile_mask masks[PROFILE_MAX_MASKS];
} profile_include;

typedef struct {
    uint8_t     shading_model;                      /* profile_shading_model */
    uint8_t     layer_count;
    skin_layer  layers[PROFILE_MAX_LAYERS];
    uint8_t         include_count;
    profile_include includes[PROFILE_MAX_INCLUDES];   /* evaluated after the layers above, in order */
    float       sheen_color[3];                     /* cloth */
    float       sheen_roughness;
    float       anisotropy;                         /* -1..1, thread direction */
    float       macro_scale;                        /* PROFILE_FEATURE_MACRO_VARIATION */
} skin_profile_data;

/* ------------------------------------------------------------------------- */
/* WIRE profile: which edges, then how the ribbons look                      */
/* The edges are picked ONCE (when the profile is assigned or the mesh is    */
/* committed) and baked into a wire_edge list. They are drawn as camera-     */
/* facing ribbons, not as Vulkan lines, so width and AA are fully ours.      */
/* ------------------------------------------------------------------------- */

typedef enum {
    WIRE_PICK_ALL = 0,
    WIRE_PICK_BORDER_LOOP,          /* boundary edges only */
    WIRE_PICK_EVERY_NTH,            /* uses pick_param */
    WIRE_PICK_REGION_BOUNDARY,      /* edges between two paint / id regions */
    WIRE_PICK_GROUP                 /* edges already tagged with pick_group */
} wire_pick_rule;

typedef enum {
    WIRE_GRID_NONE = 0,             /* rule above decides */
    WIRE_GRID_SQUARE,               /* square-grid preset */
    WIRE_GRID_OCTAGON,              /* octagon + square preset */
    WIRE_GRID_HEX
} wire_grid_kind;

typedef enum {
    WIRE_COLOR_SOLID = 0,
    WIRE_COLOR_FROM_SKIN,           /* tint of whatever skin colour is underneath */
    WIRE_COLOR_FROM_LAYERS          /* its own layer sampling, same textures as the skin */
} wire_color_mode;

/* One baked edge_to_face_array. Endpoints index the mesh verts (same indices as the skin). */
typedef struct {
    uint32_t v0, v1;
    uint32_t group;                 /* highlight group */
    uint32_t flags;
} wire_edge;

typedef struct {
    uint8_t        pick_rule;       /* wire_pick_rule */
    uint8_t        grid_kind;       /* wire_grid_kind */
    uint8_t        pick_group;      /* WIRE_PICK_GROUP: which group */
    uint32_t       pick_param;      /* N for EVERY_NTH */

    uint8_t        color_mode;      /* wire_color_mode */
    float          tint[4];
    float          highlight_tint[4];
    profile_mask   highlight_mask;  /* paint map / splat channel / group: where the wire lights up */

    float          width;           /* pixels or metres, see width_in_pixels */
    bool           width_in_pixels;
    float          opacity;
    float          edge_softness;   /* 0..1 falloff across the ribbon */
    float          dash_length;     /* 0 = solid */
    float          depth_bias;      /* keeps the ribbon from z-fighting the skin */
    bool           over_skin;       /* overlay on the skin, or replace it */

    uint8_t        layer_count;     /* WIRE_COLOR_FROM_LAYERS */
    skin_layer     layers[PROFILE_MAX_LAYERS / 4];
} wire_profile_data;

/* ------------------------------------------------------------------------- */
/* One profile asset = one .sprof file                                       */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t magic;                 /* PROFILE_FILE_MAGIC */
    uint32_t version;               /* PROFILE_FILE_VERSION */
    uint32_t id;                    /* asset id, 0 = unsaved */
    uint8_t  kind;                  /* profile_kind */
    char     name[PROFILE_NAME_LEN];
    uint32_t feature_flags;         /* PROFILE_FEATURE_* */

    /* Names of the texture inputs the profile reads, so the Material inspector can label its slots. */
    char     tex_input_name[PROFILE_MAX_TEX_INPUTS][PROFILE_LAYER_NAME_LEN];
    uint8_t  tex_input_count;

    /* Exposed values the material may override without editing the profile. */
    char     param_name[PROFILE_MAX_PARAMS][PROFILE_LAYER_NAME_LEN];
    float    param_default[PROFILE_MAX_PARAMS];
    uint8_t  param_count;

    union {
        skin_profile_data skin;     /* kind == PROFILE_KIND_SKIN */
        wire_profile_data wire;     /* kind == PROFILE_KIND_WIRE */
    } u;
} shader_profile;

/* ------------------------------------------------------------------------- */
/* Material: texture pool + a skin slot + a wire slot                        */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t profile_id;                              /* 0 = NONE: the slot is empty until a profile is assigned */
    float    params[PROFILE_MAX_PARAMS];              /* overrides of the profile's exposed values */
    uint8_t  param_overridden[PROFILE_MAX_PARAMS];    /* 0 = use the profile default */
    uint8_t  tex_bindings[PROFILE_MAX_TEX_INPUTS];    /* profile input -> pool index, PROFILE_TEX_NONE = unassigned */
} material_slot;

typedef struct {
    uint32_t      id;
    char          name[PROFILE_NAME_LEN];
    uint32_t      textures[MATERIAL_TEXTURE_POOL];    /* texture asset ids, 0 = empty. Shared by BOTH slots. */
    material_slot skin;
    material_slot wire;
    bool          wire_enabled;
} material;

/* Per-object data: where each layer shows on THIS surface. Not part of the material. */
typedef struct {
    uint32_t splat[OBJECT_MAX_SPLATMAPS];             /* texture asset ids */
    uint32_t paint[OBJECT_MAX_PAINTMAPS];
    uint32_t id_map;                                  /* region id texture, 0 = none */
} object_paint_data;

/* ------------------------------------------------------------------------- */
/* API (implemented in system_shader_profile.c)                              */
/* ------------------------------------------------------------------------- */

void shader_profile_defaults(shader_profile* p, profile_kind kind);   /* a valid, drawable profile: flat grey / thin white wire */
bool shader_profile_save(const char* path, const shader_profile* p);
bool shader_profile_load(const char* path, shader_profile* out);      /* checks magic + version */

/* ------------------------------------------------------------------------- */
/* Compile + bake at ASSIGN time, never per frame                            */
/*                                                                           */
/*   assign a profile to a skin  ->  shader_profile_compile()                */
/*     1. flatten includes into ONE layer list (depth <= PROFILE_INCLUDE_MAX_DEPTH, cycles rejected)  */
/*     2. bake every node graph a layer uses into a texture (PROFILE_SRC_NODE_GRAPH -> baked texture) */
/*     3. check the result fits, and say WHY when it does not                 */
/*   The renderer only ever sees the compiled form: a flat list in a storage  */
/*   buffer. Authoring cap 32 layers, compiled cap 32 layers AFTER flattening.*/
/*                                                                           */
/*   Live vs baked: terrain masks depend on the world (slope, height, splat), so terrain stays LIVE.      */
/*   An object with UVs can also be fully BAKED to albedo / normal / ORM maps: constant cost, any layer face_type. */
/* ------------------------------------------------------------------------- */
#define PROFILE_COMPILED_MAX_LAYERS  32
#define PROFILE_COMPILE_ERROR_LEN    96

typedef struct {
    bool          ok;
    char          error[PROFILE_COMPILE_ERROR_LEN];       /* "Too many layers: 41 of 32 after includes." */
    uint8_t       shading_model;
    uint8_t       layer_count;                            /* after flattening */
    skin_layer    layers[PROFILE_COMPILED_MAX_LAYERS];    /* includes already merged in, masks already multiplied by the include's masks */
    uint32_t      baked_graph_texture[PROFILE_COMPILED_MAX_LAYERS][PROFILE_OUT_COUNT];   /* texture asset id of a baked node graph, 0 = none */
    uint32_t      source_hash;                            /* of the profile + everything it includes: skip the compile when nothing changed */
} compiled_skin;

bool shader_profile_compile(const shader_profile* p, const material_slot* slot, compiled_skin* out);   /* fills out->error on failure */
bool shader_profile_bake_object(const compiled_skin* c, uint32_t object_id, uint32_t map_size);        /* UV'd objects: albedo / normal / ORM maps */

void material_defaults(material* mesh);                                  /* EMPTY: no surface profile, no wire profile, no textures. Objects without a skin draw neutral grey. */

#endif