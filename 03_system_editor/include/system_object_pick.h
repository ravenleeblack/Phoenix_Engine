#ifndef phoenix_system_object_pick_h
#define phoenix_system_object_pick_h

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

/* click an object (or a terrain chunk) in the Scene view to select it (system_object_pick.c).
   once per frame from editor_update. */
void update_system_object_picking(system_element_context* ui_ctx);

/* the terrain under the cursor (a ray against the height map): the world position of the hit and its depth */
bool get_terrain_ray_hit(const system_element_context* ui_ctx, float mx, float my, editor_vec3* out_hit, float* out_depth);

/* Sculpt and Paint mode on the terrain: a ring on the ground, drag to raise / lower / smooth / flatten or paint (system_terrain_brush.c).
   once per frame. every other mesh keeps the normal sculpt / paint input. */
void update_terrain_brush(system_element_context* ui_ctx);

/* where an object really is in the world: the middle and the radius of its mesh bounds (a model whose geometry is not around its origin
   still frames correctly). false when it has no mesh data. Ctrl+F uses it. */
bool get_object_world_bounds(const system_scene_object* obj, editor_vec3* out_centre, float* out_radius);

/* selects an object and opens its parents in the Scene Archive so its row is visible */
void select_scene_object_and_show(uint32_t object_id);

/* the terrain chunk that was clicked last. It only counts while its region is still the selected object. */
extern int      g_pick_chunk_x;
extern int      g_pick_chunk_z;
extern uint32_t g_pick_chunk_region_object_id;

/* Ctrl+F on a terrain object: the centre, the radius and how many radii away the camera should sit.
   A clicked chunk frames the chunk, a region frames its 3 x 3 chunks, the terrain object frames the whole world.
   false when the selected object is not a terrain object. */
bool get_terrain_focus_target(uint32_t selected_object_id, editor_vec3* out_target, float* out_radius, float* out_distance_factor);

#endif
