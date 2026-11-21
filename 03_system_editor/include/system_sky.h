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

/* Sky dome and fog.
   sky dome     an octagon dome mesh (8 sides, 4 rings) that follows the camera. unlit gradient: horizon color to top color,
                drawn by mesh_frag.glsl when the push constant light.w is sky_pc_marker.
   fog          distance fog thinned by height (fog_height_falloff), applied at the end of mesh_frag.glsl.
   fog volumes  Empty objects named "Fog Volume": position = center, largest scale = radius. The shader measures how far the
                view ray travels inside each sphere, so a thick patch of fog can sit in a valley or a room.
   Scene view and Game view each have their own Sky and Fog switch. The fog data rides in the light block (system_light_gpu.c). */

enum {
    sky_max_volumes = 4,
    sky_block_floats = 60           /* fog_a, fog_b, fog_c, sky top, sky horizon, sun direction, sun color, 4 volume centers, 4 volume colors: 15 vec4 */
};

typedef struct {
    bool  scene_sky_on;
    bool  scene_fog_on;
    bool  game_sky_on;
    bool  game_fog_on;

    float sky_top[3];               /* sRGB, like every color the editor stores */
    float sky_horizon[3];
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
    float    volume_density[sky_max_volumes];      /* per meter inside the sphere */
    float    volume_color[sky_max_volumes][3];

    bool  draw_sky_now;             /* set by the scene draw before each view: is the sky on in the view being drawn */
    bool  draw_fog_now;
    float draw_far;                 /* far clip of the view being drawn (the dome is 90% of it) */
} sky_state;

extern sky_state g_sky;

float                get_sky_linear_color(float c);

bool                 is_sky_dome_object(const system_scene_object* obj);
system_scene_object* create_system_sky_dome(VkDevice device, VkPhysicalDevice physical_device);
system_scene_object* create_system_fog_volume(editor_vec3 position);
void                 get_sky_dome_matrix(const editor_mat4* view, const system_scene_object* dome, editor_mat4* out_model);
void                 fill_sky_fog_block(float* block, const editor_mat4* view);
void                 prune_sky_volumes(void);

#endif
