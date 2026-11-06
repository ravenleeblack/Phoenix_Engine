#ifndef phoenix_engine_skin_gpu_h
#define phoenix_engine_skin_gpu_h

#include "engine_defines.h"
#include "engine_math.h"

VkDescriptorSet skin_gpu_get_empty_set(void);
VkDescriptorSet skin_gpu_get_set(uint32_t skin_id);
bool skin_gpu_has(uint32_t skin_id);
uint32_t get_object_skin_id(uint32_t object_id);
void texshop_build_skin_gpu(uint32_t skin_id);
bool skin_gpu_create_layout(VkDevice device);
VkDescriptorSetLayout skin_gpu_get_layout(void);
void skin_gpu_shutdown(VkDevice device);

#endif
