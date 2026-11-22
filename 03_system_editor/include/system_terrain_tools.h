#ifndef PHOENIX_SYSTEM_TERRAIN_TOOLS_H
#define PHOENIX_SYSTEM_TERRAIN_TOOLS_H

#include <stdbool.h>
#include <stdint.h>
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

/* Terrain Tools: shaped pieces of ground (river, tunnel, mountain, lake, ramp, overhang, canyon), made in five steps with a wireframe preview
   in the Scene view at every step. This is separate from the terrain brushes (system_terrain_brush.c).
     1 Pick       click the terrain: start and end of a path (or the middle of a mountain / lake). Optional snap to the terrain edge
     2 Segments   how many segments the curve has. drag any vertex, or every vertex at once (Move all). side pattern: smooth / steps / zigzag + offset
     3 Shape      width, depth (or height / rise) and the bottom shape: rounded, square or octagon
     4 Ridges     a rim along the top of the cut. with a lip it hangs over the cut (an overhang, only possible as a separate object)
     5 Finalize   cut it into the terrain and stitch the edges, as one object (the terrain itself) or a separate object next to it
   window: draw_system_terrain_tools.c (the icons), steps panel in the Modeler window (draw_system_modeler.c), logic and preview: system_terrain_tools.c */

typedef enum {
    tt_tool_none = 0,
    tt_tool_river,
    tt_tool_tunnel,
    tt_tool_mountain,
    tt_tool_lake,
    tt_tool_ramp,
    tt_tool_overhang,
    tt_tool_canyon,
    tt_tool_count
} enum_tt_tool;

typedef enum { tt_shape_rounded = 0, tt_shape_square, tt_shape_octagon, tt_shape_count } enum_tt_shape;
typedef enum { tt_curve_s = 0, tt_curve_arc } enum_tt_curve;
typedef enum { tt_walls_sloped = 0, tt_walls_vertical } enum_tt_walls;
typedef enum { tt_pattern_smooth = 0, tt_pattern_steps, tt_pattern_zigzag, tt_pattern_count } enum_tt_pattern;

enum { tt_max_segments = 48, tt_dense_per_segment = 6, tt_max_dense = tt_max_segments * tt_dense_per_segment + 1, tt_step_count = 5 };

typedef struct {
    int   tool;                                 /* enum_tt_tool */
    int   step;                                 /* 1..5 */
    int   picks;                                /* how many points step 1 has taken */
    int   start_edge;                           /* step 1: the edit mode edge that was picked for the start (-1 = none) */
    bool  separate;                             /* step 5: keep the piece as its own object */
    bool  wants_mouse;                          /* the Scene view click belongs to the tool (object picking stays out) */
    bool  prev_down;                            /* left mouse last frame (for the rising edge) */

    float start[2];                             /* terrain local x, z. a mountain / lake: the middle */
    float end[2];
    int   curve;                                /* enum_tt_curve: S winds both ways, Arc bows to one side */
    float bend;                                 /* -1..1: how far the curve bows to the side (fraction of the length) */
    int   segments;                             /* 2..tt_max_segments */
    float node[tt_max_segments + 1][2];         /* the vertices of the curve (drag them) */
    int   node_count;
    bool  nodes_edited;                         /* a vertex was moved: changing segments or bend remakes them */
    bool  node_selected[tt_max_segments + 1];   /* the vertices the translate gizmo moves (click a square, Shift adds) */
    float node_snap[tt_max_segments + 1][2];    /* where they were when the gizmo drag began */

    int   pattern;                              /* enum_tt_pattern: the side line of the cut */
    float pattern_offset;                       /* 0..0.8: how far the side moves in and out (fraction of the width) */

    float width;                                /* meters from the middle to the edge (radius for a mountain / lake) */
    float depth;                                /* meters cut down (river, canyon, lake, tunnel depth under the ground). a mountain: height. a ramp: rise */
    float bank;                                 /* meters of slope that stitches the cut back into the terrain */
    int   shape;                                /* enum_tt_shape */
    int   walls;                                /* enum_tt_walls: sloped (the bottom shape reaches the edge) or straight up and down */

    float ridge_height;                         /* meters the rim rises above the cut's edge */
    float ridge_width;                          /* meters across the rim */
    float lip;                                  /* meters the rim reaches out over the cut (separate object only) */

    /* made from the above every frame */
    float dense[tt_max_dense][4];               /* x, z, arc length 0..1, half width */
    float dense_ground[tt_max_dense];           /* the ground along the curve, smoothed */
    int   dense_count;
    float ramp_start_height;
    float ramp_end_height;
    float center_ground;
    float path_length;
} terrain_tools_state;

extern terrain_tools_state g_terrain_tools;

const char* get_terrain_tools_name(int tool);
const char* get_terrain_tools_step_name(int tool, int step);
const char* get_terrain_tools_shape_name(int shape);
const char* get_terrain_tools_pattern_name(int pattern);
bool        is_terrain_tools_radial(int tool);                       /* mountain, lake: a round piece instead of a path */

void select_terrain_tool(int tool);                                  /* picks a tool and starts it over at step 1 */
void close_terrain_tool(void);
void reset_terrain_tool_picks(void);
void set_terrain_tool_step(int step);
void build_terrain_tool_nodes(void);                                 /* start / end / bend / segments -> vertices */
bool accept_terrain_tool(void);                                      /* step 5: make it real */
bool is_terrain_tools_wanting_mouse(void);
bool is_terrain_tools_blocking_edit_picks(void);                     /* steps 2..5: clicks on the squares, not on the edit mode grid */
void select_all_terrain_tool_nodes(void);
void enter_terrain_tools_edit_mode(system_element_context* ui_element);   /* Edit mode on the terrain with edge select: step 1 picks edges */

/* the translate gizmo (system_transform_gizmo.c) moves the selected vertices of the tool */
bool get_terrain_tools_gizmo_pivot(editor_vec3* out_world);
void begin_terrain_tools_gizmo_drag(void);
void move_terrain_tools_gizmo_nodes(int axis, float distance);

/* every frame from the Scene view: input (picks, dragging vertices) and the wireframe preview */
void update_and_draw_terrain_tools(VkCommandBuffer cmd, system_element_context* ui_element);

void draw_terrain_tools_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);

#endif
