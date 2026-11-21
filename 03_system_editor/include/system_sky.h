#ifndef PHOENIX_SYSTEM_SKY_H
#define PHOENIX_SYSTEM_SKY_H

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

/* Procedural sky and fog.
   sky dome     an octagon dome mesh (8 sides, 4 rings) that follows the camera. The color is computed in mesh_frag.glsl from the
                sun: blue day sky, orange sunset, dark night, haze and a glow and disc at the sun (like Unreal's sky atmosphere).
                No colors to paint: move the directional light and the sky follows.
   fog          exponential height fog integrated along the view ray, colored by the sky at the horizon (or a color of your own),
                with a glow toward the sun. Applied at the end of mesh_frag.glsl.
   fog volumes  Empty objects named "Fog Volume": position = center, rotation = orientation, scale = full size in meters. The
                box is drawn as a wire box in the Scene view and is resized with the Scale tool. Each volume is filled with fog:
                  cheap  one straight pass through the box with a soft edge. Fastest.
                  good   ray marched through the box with moving noise, thicker toward the bottom, lit by the sun. Looks like cloud.
   Scene view and Game view each have their own Sky and Fog switch. The data rides in the light block (system_light_gpu.c). */

enum {
    sky_max_volumes = 4,
    sky_block_floats = 104,         /* 8 vec4 of sky / fog / sun data, then per volume: color, params, 3 matrix rows (26 vec4 in all) */
    fog_volume_cheap = 0,
    fog_volume_good  = 1
};

typedef struct {
    bool  scene_sky_on;
    bool  scene_fog_on;
    bool  game_sky_on;
    bool  game_fog_on;

    float sky_brightness;           /* multiplies the whole sky (1 = normal) */
    float sky_haze;                 /* 0 = clear air, 1 = hazy: a bigger whiter glow around the sun and a paler horizon */
    bool  fog_sky_color;            /* true: the fog takes the sky's horizon color. false: fog_color below */
    float fog_color[3];
    float fog_density;              /* per meter */
    float fog_height_falloff;       /* per meter of height: higher = fog hugs the ground */
    float fog_base_height;          /* fog is at full thickness at and below this height (meters) */
    float fog_start;                /* no fog closer than this (meters) */
    float fog_max_opacity;          /* the fog never gets thicker than this (0..1) */
    float fog_sun_scatter;          /* how much the fog glows toward the sun, like Unreal's directional inscattering (0..2) */
    float fog_sun_exponent;         /* how tight that glow is: higher = a smaller hot spot around the sun */

    uint32_t dome_object_id;        /* 0 = no dome yet */
    int      volume_count;
    uint32_t volume_object_ids[sky_max_volumes];
    int      volume_type[sky_max_volumes];         /* fog_volume_cheap or fog_volume_good */
    float    volume_density[sky_max_volumes];      /* per meter at the middle of the box */
    float    volume_softness[sky_max_volumes];     /* how far in from the box edge the fog fades out (0 = hard edge, 1 = fades all the way to the middle) */
    float    volume_noise_scale[sky_max_volumes];  /* good fog: how many noise cells fit across the box */
    float    volume_color[sky_max_volumes][3];

    bool  draw_sky_now;             /* set by the scene draw before each view: is the sky on in the view being drawn */
    bool  draw_fog_now;
    float draw_far;                 /* far clip of the view being drawn (the dome is 90% of it) */
    float draw_time;                /* seconds, drives the moving noise of good fog */
} sky_state;
extern sky_state g_sky;

float                get_sky_linear_color(float c);

bool                 is_sky_dome_object(const system_scene_object* obj);
system_scene_object* create_system_sky_dome(VkDevice device, VkPhysicalDevice physical_device);
system_scene_object* create_system_fog_volume(editor_vec3 position);
void                 get_sky_dome_matrix(const editor_mat4* view, const system_scene_object* dome, editor_mat4* out_model);
void                 fill_sky_fog_block(float* block, const editor_mat4* view);
void                 prune_sky_volumes(void);
void                 get_volume_local_rows(const editor_mat4* view, const system_scene_object* obj, float rows[3][4]);

#endif
