#ifndef PHOENIX_SYSTEM_TEXTURESHOP_SKINS_H
#define PHOENIX_SYSTEM_TEXTURESHOP_SKINS_H

#include <stdbool.h>
#include <stddef.h>
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

/* The skin library (draw_system_textureshop.c) and its window (draw_system_textureshop_skinmaker.c) share these.
   A texshop_skin is the editable form of a skin shader profile: Export writes it as a .sprof (system_shader_profile.h)
   into assets/skins, and the skin id is stored inside that file (shader_profile.id), so an object's skin_ref keeps
   pointing at the same skin after a restart. */

enum { texshop_max_skins = 32, texshop_max_layers = 16, texshop_max_masks = 4, texshop_name_len = 24 };

#define texshop_bit(vert_to_edge_array) (1u << (unsigned)(vert_to_edge_array))

typedef enum {
    texshop_out_color = 0, texshop_out_normal, texshop_out_rough, texshop_out_metal, texshop_out_height, texshop_out_count
} enum_texshop_output;

typedef enum {
    texshop_blend_lerp = 0, texshop_blend_multiply, texshop_blend_overlay, texshop_blend_add, texshop_blend_count
} enum_texshop_blend;

typedef enum {
    texshop_mask_texture = 0, texshop_mask_paint, texshop_mask_height, texshop_mask_slope, texshop_mask_curvature, texshop_mask_count
} enum_texshop_mask;

typedef struct {
    char               name[texshop_name_len];
    uint32_t           channel_mask;                    /* texshop_bit(output) */
    enum_texshop_blend blend;
    int                mask_count;
    enum_texshop_mask  masks[texshop_max_masks];
    bool               triplanar;
    float              color[4];                        /* rgb written straight to the target */
    float              opacity;                         /* 0..1 */
} texshop_layer;

typedef struct {
    uint32_t      id;                                   /* never reused, never an array index. Objects store THIS. 0 = none. */
    char          name[texshop_name_len];               /* also the file name: assets/skins/<name>.sprof */
    texshop_layer layers[texshop_max_layers];
    int           layer_count;
    int           selected_layer;
    uint32_t      gpu_hash;                             /* hash the GPU copy was built from. differs = rebuild */
    uint32_t      saved_hash;                           /* hash at the last Export / load. differs = unsaved edits (shown as *) */
} texshop_skin;

typedef struct {
    bool         ready;
    texshop_skin skins[texshop_max_skins];
    int          skin_count;
    int          selected_skin;
    int          next_skin_number;
    uint32_t     next_skin_id;
    char         status[96];
} texshop_state;

extern texshop_state g_texshop;

extern const char* const k_texshop_output_name[texshop_out_count];
extern const char        k_texshop_output_letter[texshop_out_count];
extern const char* const k_texshop_blend_name[texshop_blend_count];
extern const char* const k_texshop_mask_name[texshop_mask_count];

/* ---- editing (used by the Skin Maker window) ---- */
void           check_texshop_ready(void);
void           set_texshop_status(const char* text);
texshop_skin*  get_texshop_skin(void);                       /* the obj_vert_selected skin, NULL when there is none */
texshop_layer* get_texshop_layer(texshop_skin* s);           /* the obj_vert_selected layer */
void           add_texshop_layer(texshop_skin* s);
void           remove_texshop_layer(texshop_skin* s);
void           move_texshop_layer(texshop_skin* s, int direction);
void           add_texshop_mask(texshop_layer* l, enum_texshop_mask mask);
void           remove_texshop_mask(texshop_layer* l, int index);
bool           texshop_skin_is_dirty(const texshop_skin* s);
void           texshop_describe_target(char* out, size_t cap);   /* "<object>: <skin or no skin>" for the obj_vert_selected object */

/* once per frame from the window: rebuilds the obj_vert_selected skin's GPU copy when an edit changed it */
void           texshop_sync_skin_gpu(void);

#endif