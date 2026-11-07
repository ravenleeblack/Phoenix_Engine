#include <vulkan/vulkan.h>
#include "engine_scene_offscreen.h"

/* Wrapper functions implemented in system_scene_offscreen.c */
bool create_editor_offscreen_renderpass(VkDevice device, VkFormat color_format, VkFormat depth_format);
VkRenderPass editor_get_offscreen_renderpass(void);
bool create_editor_offscreen_resources(void);
void shutdown_editor_offscreen(void);
void record_editor_offscreen_frame(VkCommandBuffer cmd);
void offscreen_request_size(int kind, double width, double height);
VkDescriptorSet offscreen_get_descriptor(offscreen_kind kind);