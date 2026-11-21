#ifndef SYSTEM_LIGHT_GPU_H
#define SYSTEM_LIGHT_GPU_H

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include "system_math.h"
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

/* GPU side of the scene lights: a ring of uniform block copies in one dynamic uniform buffer, bound as set 1 of the mesh
   pipeline layout. Every mesh list draw writes the next slot and binds it with a dynamic offset, so a slot is never
   rewritten while an earlier frame may still read it. Layout must match `light_block` in mesh_frag.glsl. */

enum {
    light_gpu_max_lights  = 8,
    light_gpu_slot_count  = 32,
    light_gpu_slot_size   = 1024,                                   /* >= the 768 byte block, multiple of any offset alignment */
    light_gpu_block_floats = 12 + light_gpu_max_lights * 16 + 60    /* counts + sky + ground + lights (4 vec4 each) + sky / fog data (system_sky.h) */
};

typedef struct {
    VkDevice              device;
    VkDescriptorSetLayout layout;
    VkDescriptorPool      pool;
    VkDescriptorSet       set;
    VkBuffer              buffer;
    VkDeviceMemory        memory;
    uint8_t*              mapped;
    uint32_t              next_slot;
} light_gpu_state;

extern light_gpu_state g_light_gpu;

bool                  create_light_gpu_layout(VkDevice device, VkPhysicalDevice physical_device);
VkDescriptorSetLayout get_light_gpu_layout(void);
void                  bind_light_gpu_set(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, const editor_mat4* view);
void                  destroy_light_gpu(VkDevice device);
void                  convert_to_view_space(const editor_mat4* view, const float v[3], bool is_point, float out[3]);
void                  normalize_vector3(float v[3]);

#endif
