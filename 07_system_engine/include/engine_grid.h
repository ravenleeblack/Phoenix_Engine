#ifndef phoenix_engine_grid_h
#define phoenix_engine_grid_h

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

/* Include from engine_globals.h after editor_vec3, editor_mat4 and world_camera are defined. */

/*
 * The grid is generated entirely inside grid_vert.glsl from gl_VertexIndex
 * (grid_half_cells = cells per half side, passed as firstInstance; vertices = (2 * half + 1) * 4), so NO vertex buffer is needed.
 *
 * There is ONE camera: engine_context.world_state.editor_scene_camera.
 * It is the same camera the gimbal overlay and the WASD / Alt-drag input use,
 * so the grid, meshes and gizmo cannot disagree.
 * Convention: yaw 0 faces -Z, rotation.x is pitch (negative looks down).
 */
#define zoom_min_distance   1.0f
#define zoom_max_distance   50000.0f
#define zoom_step_factor    0.88f      /* one wheel notch */
#define zoom_key_steps      0.25f      /* per frame while +/- is held */
#define grid_default_fov    1.0471976f /* 60 degrees */


/* Must match: layout(push_constant) uniform camera_block { mat4 view; mat4 projection; } in grid_vert.glsl */
typedef struct {
    editor_mat4 view;
    editor_mat4 projection;
} grid_camera_push_constant;   /* 128 bytes */

/* g_scene_cursor_in_view: extern in engine_globals.h, init (true) in engine_globals.c.
   The EDITOR sets it every frame (cursor over the Scene view). The engine never reads editor ui state. */

/* ---- helpers (no statics any more, so they live here) ---- */
world_camera* get_editor_camera(void);
editor_vec3   get_camera_forward(const world_camera* cam);
bool          is_cursor_in_scene_view(void);
editor_vec3   get_zoom_target(void);
float         get_distance_between(editor_vec3 a, editor_vec3 b);
float         get_zoom_reference_distance(void);
void          sync_clip_planes(world_camera* cam, float distance);

/* ---- api ---- */
phoenix_engine_api void set_system_grid_orbit_camera(float yaw, float pitch, float distance);
phoenix_engine_api void zoom_system_grid(float steps);            /* was system_grid_zoom */
phoenix_engine_api void update_system_grid_hotkeys(void);
phoenix_engine_api void build_scene_orbit_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj);
phoenix_engine_api void draw_system_grid_axis_stream(VkCommandBuffer cmd, VkPipelineLayout layout_3d, VkShaderStageFlags stages, float aspect);

#endif