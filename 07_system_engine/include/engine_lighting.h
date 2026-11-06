#ifndef phoenix_engine_lighting_h
#define phoenix_engine_lighting_h

#include "engine_defines.h"
#include "engine_terrain.h"   /* system_terrain, terrain_chunk, terrain_layer, terrain_* and editor_vertex live in the engine now */
#include "engine_edit_manager.h"

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

/* Lighting lives on a terrain chunk layer (default: Atmosphere, the lighting / fog / weather layer).
   The Lighting window has a layer drop down: it calls set_terrain_light_layer().
   The light OBJECT registry and the sun glyph live in engine_direction_light.h, not here.
   No editor headers are included here: the engine must never include system_*.h. */

/*
 * One byte per cell in the light layer's chunk `cells` array:
 *   low nibble  = 15 - sky light   (so a zeroed, never-touched cell is fully sky lit)
 *   high nibble = block light      (torches, lamps)
 * The array is only allocated for a chunk when something sets a light in it.
 */
typedef struct
{
    float min_light;        /* brightness of light level 0 (0 = pitch black, 1 = no darkening) */
    bool  enabled;          /* Lit / Unl buttons in the Scene tab bar */
    int   layer;            /* terrain_layer_id the light cells live on (1 .. terrain_max_layers-1, never the height layer) */
    float sun_intensity;    /* sun brightness: 1 = normal, 0 = no sun (ambient only), up to 3 = very bright */
    float sun_warmth;       /* -1 = cool / blue, 0 = white, +1 = warm / orange */
} light_settings;

/* g_light: extern in engine_globals.h, init in engine_globals.c */

/* old name, kept so other files that read it keep compiling. It follows the drop down now. */
#define terrain_light_layer (g_light.layer)

/* Layer drop down. Switching does NOT move stored light, the new layer starts empty (= fully sky lit).
   Rebuilds the terrain meshes. */
phoenix_engine_api void set_terrain_light_layer(int layer);
phoenix_engine_api int  get_terrain_light_layer(void);

/* Sun colour for the mesh shader: rgb = tint * brightness, a = unused.
   `ambient` is the ambient value that goes to the shader next to the sun direction. (was light_get_sun_color) */
phoenix_engine_api void get_light_sun_color(float ambient, float out[4]);
#define light_get_sun_color get_light_sun_color

/* Re-bake the light into every terrain mesh (after min_light changed, for example). (was terrain_light_rebuild_all) */
phoenix_engine_api void rebuild_terrain_light_all(void);

/* Frees every light cell of the current layer: everything is fully sky lit again. Rebuilds the meshes. */
phoenix_engine_api void clear_terrain_light_layer(void);

/* Lit / Unl. Changing it rebuilds the terrain meshes (the light is baked into vertex colour). */
phoenix_engine_api void set_terrain_lighting_enabled(bool enabled);
phoenix_engine_api bool get_terrain_lighting_enabled(void);

phoenix_engine_api void get_terrain_light(const system_terrain* t, int gx, int gy, int gz, int* out_sky, int* out_block);
phoenix_engine_api bool set_terrain_light(int gx, int gy, int gz, int sky, int block);
phoenix_engine_api void fill_terrain_light_box(int x0, int y0, int z0, int x1, int y1, int z1, int sky, int block);

/* Multiplies the colour of every quad (4 vertices) by the light level of the air in front of it.
   Call on a chunk mesh right after it is built. Does nothing while lighting is disabled. */
phoenix_engine_api void apply_terrain_light_to_mesh(const system_terrain* t, editor_vertex* verts, uint32_t vertex_count);

/* helpers that used to be static */
unsigned char   pack_light_value(int sky, int block);
terrain_chunk*  get_terrain_light_chunk(const system_terrain* t, int cx, int cz);
size_t          get_terrain_light_index(const system_terrain* t, int lx, int ly, int lz);
bool            set_terrain_light_raw(system_terrain* t, int gx, int gy, int gz, int sky, int block);
void            mark_terrain_light_dirty(system_terrain* t, int gx0, int gz0, int gx1, int gz1);

#endif