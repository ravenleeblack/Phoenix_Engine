#ifndef SYSTEM_SKIN_GPU_H
#define SYSTEM_SKIN_GPU_H

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
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

/* GPU side of a skin: one uniform buffer + one descriptor set per skin, cached by skin id (never per frame).
   Set 0, binding 0 of the mesh pipeline layout. mesh_frag.glsl reads it. Layout must match `Skin` in mesh_frag.glsl. */

#define SKIN_GPU_MAX_LAYERS 16

typedef struct {
    float    color[4];        /* rgb + a. written straight to the target, like vertex colour */
    float    opacity;         /* 0..1, multiplies color[3] */
    uint32_t channel_mask;    /* bit 0 = Color. Only Color is evaluated today */
    uint32_t blend;           /* 0 lerp, 1 multiply, 2 overlay, 3 add (same order as the Texture Shop enum) */
    uint32_t triplanar;       /* stored for later, not used by the shader yet */
} skin_gpu_layer;

typedef struct {
    uint32_t       layer_count;                    /* 1..SKIN_GPU_MAX_LAYERS, layer 0 = bottom */
    skin_gpu_layer layers[SKIN_GPU_MAX_LAYERS];
    const uint8_t* map_pixels;                     /* base color map: RGBA8 rows top to bottom, NULL = no map. only read during skin_gpu_set */
    uint32_t       map_width;
    uint32_t       map_height;
    const uint8_t* aux_pixels;                     /* packed extra maps, RGBA8: r = specular, g = height, b = ambient occlusion. NULL = none */
    uint32_t       aux_width;
    uint32_t       aux_height;
    uint32_t       aux_flags;                      /* bit 0 = specular map, bit 1 = height map, bit 2 = ambient map */
    float          specular;                       /* specular strength 0..1 */
    float          gloss;                          /* highlight tightness 0..1 */
    float          height_scale;                   /* bump strength 0..1 */
    float          ambient;                        /* how much of the scene ambient the skin takes, 0..2 */
} skin_gpu_data;

/* set layout + pool + the shared "no skin" set. Safe to call again. create_scene_object_pipeline calls it. */
bool                  skin_gpu_create_layout(VkDevice device);
VkDescriptorSetLayout skin_gpu_get_layout(void);

/* Build (or replace) the GPU data of a skin. The old buffer + set are retired and freed 3 frames later. */
bool skin_gpu_set(uint32_t skin_id, const skin_gpu_data* data);
void skin_gpu_remove(uint32_t skin_id);                 /* skin deleted: retire its buffer + set */
bool skin_gpu_has(uint32_t skin_id);

/* The skin's set, or the shared no-skin set (id 0 or unknown). Never VK_NULL_HANDLE once the layout exists. */
VkDescriptorSet skin_gpu_get_set(uint32_t skin_id);
VkDescriptorSet skin_gpu_get_empty_set(void);

void skin_gpu_tick(void);       /* ONCE per frame. system_edit_mesh.c calls it from em_gpu_tick() */
void skin_gpu_flush(void);      /* destroy everything retired now (called from em_gpu_flush) */
void skin_gpu_shutdown(VkDevice device);   /* destroy all skins, the pool and the layout */

/* ---- draw_system_textureshop.c ---- */
uint32_t get_object_skin_id(uint32_t object_id);   /* 0 = no skin */
void     texshop_build_skin_gpu(uint32_t skin_id); /* builds the skin's GPU data if it has none yet */

#endif