#ifndef PHOENIX_SYSTEM_PAINT_TEXTURE_H
#define PHOENIX_SYSTEM_PAINT_TEXTURE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
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

/* One paint texture per mesh object: RGBA8, sRGB encoded (the same 0..1 sRGB the Paint window colour uses).
   Row 0 is vert_to_edge_array = 0. The mesh uv decides where on the texture a face lives (cube = 3x3 atlas, see em_make_cube). */
#ifndef PAINT_TEXTURE_SIZE
#define PAINT_TEXTURE_SIZE 1024
#endif

typedef struct {
    uint32_t object_id;
    int      width, height;
    uint8_t* pixels;        /* width * height * 4 */
    bool     dirty;         /* changed since the GPU copy was made (used by the GPU upload step) */
    uint32_t revision;      /* +1 on every change */
} paint_texture;

/* NULL when the object has none and create is false. A new texture starts as the default mesh grey. */
paint_texture* paint_texture_get(uint32_t object_id, bool create);
void           paint_texture_remove(uint32_t object_id);   /* call when the scene object is deleted */
void           paint_texture_shutdown(void);

/* One dab from g_paint (colour, radius in object space metres, opacity, hardness).
   uv          texture coordinate under the brush
   uv_min/max  uv bounds of the face that was hit: the dab never leaves them, so it cannot bleed onto a neighbouring atlas cell
   uv_per_m    how many uv units one object space metre covers at the hit (turns the metre radius into a texel radius)
   brush ids as in system_paint_mode.h: 0 paint, 1 airbrush, 2 eraser, 7 lighten, 8 darken. 3 smudge, 4 blur, 5 fill: not on textures yet.
   erase_now (Ctrl) turns paint / airbrush into an eraser. Returns true when any texel changed. */
bool paint_texture_dab(uint32_t object_id, const float uv[2], const float uv_min[2], const float uv_max[2],
                       float uv_per_m, int brush, bool erase_now);

/* Picker: copies the texel under uv into rgb (sRGB 0..1). */
bool paint_texture_pick(uint32_t object_id, const float uv[2], float rgb[3]);

/* Plain PNG (RGBA, 8 bit, no compression, no dependencies). */
bool paint_texture_export_png(uint32_t object_id, const char* path);

/* Builds "paint_<name>.png" (name cleaned for a file name) and exports it. out_path receives the file name. */
bool paint_texture_export_for_object(uint32_t object_id, const char* object_name, char* out_path, size_t out_size);

#endif