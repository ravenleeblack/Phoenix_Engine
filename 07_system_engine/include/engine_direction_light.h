#ifndef phoenix_engine_direction_light_h
#define phoenix_engine_direction_light_h

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

/* Needs editor_vec3 / editor_vertex / system_scene_object: include this from engine_globals.h AFTER the scene object and vertex types. */

#define light_max_objects 64
#define LIGHT_KIND_DIRECTIONAL 0
#define phoenix_pi        3.14159265f

typedef enum {
    light_kind_directional = 0,
    light_kind_point,
    light_kind_spot,
    light_kind_hemisphere,      /* sky color above, ground color below: soft outdoor fill */
    light_kind_area,            /* soft panel: engine_area_light.c */
    light_kind_count
} light_kind;

/* the scene registry stays the light list (id, name, transform); only the light specific bits live here. */
typedef struct {
    uint32_t   object_id;
    light_kind kind;
    bool       enabled;
    float      color[3];          /* directional / point / spot color, hemisphere sky color */
    float      ground_color[3];   /* hemisphere ground color */
    float      intensity;         /* 0..4 */
    float      range;             /* point / spot: distance where the light has faded out */
    float      cone_deg;          /* spot: half angle of the outer cone */
    float      softness;          /* spot: 0 = hard edge, 1 = fades from the centre */
    float      warmth;            /* -1 = cool / blue, 0 = as set, +1 = warm / orange */
} light_object;

/* a sun glyph: small octahedron core, four short rays (+-X, +-Y) and a long arrow down local -Z.
   the arrow is the direction the light travels. */
typedef struct {
    editor_vertex vertices[64];
    uint32_t      indices[256];
    int           vertex_count;
    int           index_count;
} glyph;

/* g_lights, g_light_count, g_light_driver, g_sun_glyph: extern in engine_globals.h, init in engine_globals.c */

/* ---- glyph building ---- */
int  add_glyph_vertex(glyph* g, float x, float y, float z, const float color[3]);
void add_glyph_triangle(glyph* g, int a, int b, int c, const float center[3]);
void add_glyph_pyramid(glyph* g, const float base_center[3], const float axis_u[3], const float axis_w[3], float half_size, const float tip[3], const float color[3]);
void add_glyph_box(glyph* g, float x0, float y0, float z0, float x1, float y1, float z1, const float color[3]);
void build_sun_glyph(glyph* g, float scale);

/* ---- registry ---- */
void remove_light_at(int index);
phoenix_engine_api bool create_system_directional_light(editor_vec3 position, float size, const char* name,
                                                        VkDevice device, VkPhysicalDevice physical_device,
                                                        VkCommandPool pool, VkQueue queue);
/* point / spot / hemisphere lights (editor side, system_directional_light.c). kind = light_kind */
bool create_system_light(light_kind kind, editor_vec3 position, VkDevice device, VkPhysicalDevice physical_device);
void reset_light_defaults(light_object* l, light_kind kind);

/* rotation (radians, x = pitch, y = yaw) -> sun azimuth / elevation in degrees */
void get_sun_from_rotation(editor_vec3 rotation, float* out_azimuth_deg, float* out_elevation_deg);
phoenix_engine_api void update_light_from_scene(void);

/* ---- access for the Lighting window ---- */
phoenix_engine_api int                 get_light_object_count(void);
phoenix_engine_api const light_object* get_light_object_at(int index);
phoenix_engine_api const char*         get_light_object_name(int index);
phoenix_engine_api void                set_light_object_enabled(int index, bool enabled);
phoenix_engine_api void                select_light_object(int index);
phoenix_engine_api bool                is_light_sun_driven(const char** out_name);

#endif