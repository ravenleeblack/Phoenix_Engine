#ifndef phoenix_engine_scene_look_h
#define phoenix_engine_scene_look_h

#include "engine_defines.h"
#include "engine_math.h"

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

/* Scene / Game view "look": gradient background + the sun used to light mesh objects.
   Live (no restart), saved by the editor preferences.
   g_scene_bg, g_scene_sun, g_scene_look_presets, g_scene_bg_pipeline, g_scene_bg_layout: extern in engine_globals.h. */

#define scene_look_preset_count 4
#define SCENE_LOOK_PRESET_COUNT scene_look_preset_count

typedef struct {
    float top[3];       /* sRGB 0..1, colour at the top of the viewport    */
    float bottom[3];    /* sRGB 0..1, colour at the bottom of the viewport */
} scene_background;

typedef struct {
    float azimuth_deg;      /* compass angle of the sun around Y                */
    float elevation_deg;    /* 0 = on the horizon, 90 = straight overhead       */
    float ambient;          /* 0..1, brightness of a face the sun does not hit  */
} scene_sun;

typedef struct {
    const char* name;
    float       top[3];
    float       bottom[3];
} scene_look_preset;

phoenix_engine_api const char* get_scene_look_preset_name(int index);
phoenix_engine_api void        apply_scene_look_preset(int index);     /* background only */

/* Called from create_scene_object_pipeline() with the OFFSCREEN render pass. */
phoenix_engine_api bool create_scene_look_pipelines(VkDevice device, VkRenderPass offscreen_renderpass);
phoenix_engine_api void shutdown_scene_look(VkDevice device);

/* Draw the gradient INSIDE the offscreen pass, after viewport + scissor, BEFORE the grid. Costs 3 vertices. */
phoenix_engine_api void draw_scene_look_background(VkCommandBuffer cmd);

/* Direction TOWARD the sun in VIEW space (xyz) + ambient (w). All zero xyz = unlit. */
phoenix_engine_api void get_scene_look_light_view(const editor_mat4* view, float out[4]);
#define scene_look_get_light_view get_scene_look_light_view
#define scene_look_shutdown shutdown_scene_look
#define scene_look_create_pipelines create_scene_look_pipelines
#define scene_look_draw_background draw_scene_look_background

#endif