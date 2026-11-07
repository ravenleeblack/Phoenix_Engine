#ifndef PHOENIX_SYSTEM_SCENE_LOOK_H
#define PHOENIX_SYSTEM_SCENE_LOOK_H

#include "engine_scene_look.h"



#include <stdbool.h>
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

/* ------------------------------------------------------------------------- */
/* Scene / Game view "look": gradient background + the sun used to light     */
/* mesh objects. Everything here is live (no restart) and saved by           */
/* system_preferences.c.                                                     */
/* ------------------------------------------------------------------------- */

extern scene_background g_scene_bg;
extern scene_sun        g_scene_sun;

#define scene_look_preset_count 4
const char* scene_look_preset_name(int index);
void        scene_look_apply_preset(int index);     /* background only */

/* Called from create_scene_object_pipeline() with the OFFSCREEN render pass. */
bool scene_look_create_pipelines(VkDevice device, VkRenderPass offscreen_renderpass);
void scene_look_shutdown(VkDevice device);

/* Draw the gradient. Call it INSIDE the offscreen pass, after the viewport + scissor are set and
   BEFORE the grid, for both the Scene view and the Game view. Costs 3 vertices. */
void scene_look_draw_background(VkCommandBuffer cmd);

/* Direction TOWARD the sun in VIEW space (xyz) + ambient (w). All zero xyz = unlit (the shader then
   outputs the plain vertex colour). Unlit while g_light.enabled is false (the Lit / Unl button). */
void scene_look_get_light_view(const editor_mat4* view, float out[4]);

#endif