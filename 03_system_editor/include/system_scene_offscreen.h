#ifndef SYSTEM_OFFSCREEN_H
#define SYSTEM_OFFSCREEN_H

#include "engine_scene_offscreen.h"

#endif

#if 0
#include <stdbool.h>
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

/* Offscreen scene targets.
   Scene view and Game view each render into their OWN image (MSAA color + depth, resolved into a 1-sample
   sampled image), in a render pass that runs BEFORE the main UI pass. The UI pass then draws the resolved
   image as an ordinary quad, so 3D can never leak outside the viewport rectangle, depth sorting works, and the
   resolved image is the input for the FXAA / TAA passes later. */

typedef enum {
    OFFSCREEN_SCENE = 0,
    OFFSCREEN_GAME  = 1,
    OFFSCREEN_COUNT
} offscreen_kind;

/* 1. Call after the swapchain exists and BEFORE create_vulkan_graphics_pipelines (grid + mesh pipelines are built against this pass). */
bool offscreen_create_renderpass(VkFormat color_format, VkFormat depth_format);

/* 2. Call after create_vulkan_graphics_pipelines (needs render_context.descriptor_layout): sampler + descriptor sets. Images are created lazily. */
bool offscreen_create_resources(void);

/* Call from shutdown, after vkDeviceWaitIdle. */
void offscreen_shutdown(void);

/* The UI flush reports the rect of each viewport quad it met this frame; the NEXT frame's offscreen pass uses that size. */
void offscreen_request_size(offscreen_kind kind, float width, float height);

/* Records both offscreen passes into cmd. Must be called OUTSIDE any render pass, before the main pass begins. */
void offscreen_record_frame(VkCommandBuffer cmd);

/* Descriptor set of the resolved image, or VK_NULL_HANDLE if it was not rendered this frame (skip the quad then). */
VkDescriptorSet offscreen_get_descriptor(offscreen_kind kind);

#endif