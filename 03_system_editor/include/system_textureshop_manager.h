#ifndef PHOENIX_SYSTEM_TEXTURESHOP_MANAGEMENT_H
#define PHOENIX_SYSTEM_TEXTURESHOP_MANAGEMENT_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "system_math.h"
#include "system_object_database.h"   /* object_data_user, enum_object_data_kind */
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

/* 1 = editor_vertex is x y z r g b a u vert_to_edge_array nx ny nz (48 bytes). The mesh pipeline (system_scene_draw.c) and
   mesh_vert.glsl / mesh_frag.glsl read all of it. The grid shader (grid_vert.glsl) has no vertex input, so it is not affected.
   Set to 0 only together with reverting editor_vertex, the pipeline attributes and the mesh shaders. */
#define PHOENIX_VERTEX_HAS_UV_NORMAL 1

/* The look of an object that has NO skin assigned: neutral grey, like any 3D modeler.
   ONE place, so the cube, the terrain and anything else created without a skin agree.
   Once the skin shader exists, the shader draws this grey itself and vertex colour goes back to "baked light only". */
#define SKIN_EMPTY_GREY_SRGB 0.55f



/* ------------------------------------------------------------------------- */
/* Skin assignment: which skin an object wears.                              */
/*                                                                           */
/* One small block per object in the object database (system_object_database.h), kind object_data_user + 0.  */
/* The scene registry stays the light list, so system_scene_object does not change and nothing needs a make clean.   */
/* The block holds a skin ID (texshop_skin.id), never an array index: indexes move when a skin is deleted.        */
/* No block, or an id that no longer exists = no skin = the object keeps the empty-skin grey.                      */
/* Later this becomes a material id (shader_profile_design.md section 7). Keep the name, change the meaning.      */
/* ------------------------------------------------------------------------- */

typedef struct { uint32_t skin_id; } skin_ref;

/* draw_system_textureshop.c. All of them act on the object obj_vert_selected in the Hierarchy. */
void     assign_texshop_skin_to_selected(void);        /* the skin obj_vert_selected in the Texture Shop window -> the obj_vert_selected object */
void     clear_texshop_skin_on_selected(void);         /* obj_vert_selected object goes back to the empty-skin grey */
uint32_t get_object_skin_id(uint32_t object_id);       /* 0 = no skin (no block, or the skin was deleted). The renderer will call this. */
const char* get_object_skin_name(uint32_t object_id);  /* NULL = no skin. Points into the skin list, do not keep it across a frame. */

#endif