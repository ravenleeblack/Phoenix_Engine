#ifndef PHOENIX_ENGINE_CAMERA_H
#define PHOENIX_ENGINE_CAMERA_H

#include "engine_defines.h"

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

/* Scene view camera settings. Edited in Preferences > CAMERA, saved in the settings database.
   g_camera_prefs: extern in engine_globals.h, defined in engine_globals.c. */
typedef struct camera_prefs
{
    float near_clip;     /* meters. smaller = you can get closer before things are cut away */
    float far_clip;      /* meters. anything farther than this is culled */
    float pan_speed;     /* multiplier on Alt + middle mouse pan */
} camera_prefs;

typedef struct camera_component
{
    editor_vec3 position;    /* world space, meters */
    editor_vec3 rotation;    /* radians: x = pitch (negative looks down), y = yaw (0 faces -Z), z = roll (unused by the fly camera) */
    float       fov;         /* vertical field of view in radians. 0 or less = the caller falls back to 1.0472f (60 degrees) */
} camera_component;

#endif
