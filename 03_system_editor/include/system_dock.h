#ifndef system_dock_h
#define system_dock_h

#include <stdbool.h>
#include "system_element.h"
#include "system_mode_state.h"   /* system_mode_context */
#include <stdint.h>
#include <vulkan/vulkan.h>
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

typedef enum { dock_node_leaf, dock_node_split_horizontal, dock_node_split_vertical } enum_dock_node_type;
typedef enum { drop_zone_none, drop_zone_center, drop_zone_top, drop_zone_bottom, drop_zone_left, drop_zone_right } enum_drop_zone_type;

typedef struct system_dock_node
{
    enum_dock_node_type node_type;
    float split_ratio;

    int x, y, w, h;

    struct system_dock_node* left_child;
    struct system_dock_node* right_child;
    struct system_dock_node* parent;

    enum_view_type active_views[8];
    int view_count;
    int active_view_index;
    bool is_hovered;
} system_dock_node;

typedef struct manage_docking {
    system_dock_node* root;
    system_dock_node* target_hover_node;

    enum_view_type dragging_view;
    bool is_dragging;
    enum_drop_zone_type active_drop_zone;

    system_dock_node* resizing_splitter_node;
    bool is_resizing_splitter;
} manage_docking;

extern manage_docking dock_workspace;

enum { dock_min_w = 100, dock_min_h = 40, dock_grab = 5, dock_border = 2, dock_tab_id = 884000 };

/* the active view of a leaf, view_type_none when empty */
static inline enum_view_type dock_active_view(const system_dock_node* n)
{
    return (n && n->node_type == dock_node_leaf && n->view_count > 0) ? n->active_views[n->active_view_index] : view_type_none;
}
static inline bool dock_is_mode_menu(const system_dock_node* n) { return dock_active_view(n) == view_type_mode_menu; }

/* two kinds of dock window.
   tabbed: tab strip + sub menubar + content window. any number of views per leaf.
   strip:  grip header + content, no tabs, one view only. the mode menu (36 wide) is a strip window. */
typedef enum { dock_window_tabbed, dock_window_strip } enum_dock_window_kind;

static inline enum_dock_window_kind dock_window_kind_of(enum_view_type vert_to_edge_array)
{
    return (vert_to_edge_array == view_type_mode_menu) ? dock_window_strip : dock_window_tabbed;
}
/* strip windows never share a leaf. use this in drop handling before putting a view into a leaf. */
static inline bool dock_can_tab_together(enum_view_type a, enum_view_type b)
{
    return dock_window_kind_of(a) == dock_window_tabbed && dock_window_kind_of(b) == dock_window_tabbed;
}

/* one call per frame: interaction -> tab sync -> layout -> draw */
void start_dock_system(VkCommandBuffer cmd, manage_docking* manager, system_element_context* ui_ctx);
void shutdown_dock_system(manage_docking* manager);

/* used by the other editor files */
void push_dock_view(manage_docking* manager, enum_view_type view);
void evaluate_dock_interaction(manage_docking* manager, system_element_context* ui_ctx);
int  get_dock_cursor_kind(const manage_docking* manager, int mx, int my);   /* 0 arrow, 1 left/right, 2 up/down: the resize cursor over draggable dividers */
void compute_dock_layout(system_dock_node* node, int x, int y, int w, int h, system_dock_node* parent);
system_dock_node* create_pure_leaf_node(void);
void dock_leaf_remove_view(system_dock_node* leaf, int index);
void traverse_dock_tree(system_element_context* ui_ctx, system_dock_node* node, VkCommandBuffer current_cmd);
bool find_intersecting_splitter_line(system_dock_node* node, int mx, int my, system_dock_node** out_node);

/* predefined spots: the view is tabbed into the leaf that holds anchor_a (or anchor_b). if the view is already docked it is just activated. */
extern bool dock_spawn_background;   /* while true, spawn_dock_view_beside / push_dock_view add tabs without bringing them to the front */
void spawn_dock_view_beside(manage_docking* manager, enum_view_type anchor_a, enum_view_type anchor_b, enum_view_type view);
system_dock_node* dock_find_leaf_with_view(system_dock_node* node, enum_view_type view);
/* remove every tab of a view (mode menu uses it to keep only one mode's windows open). */
void dock_close_view_type(manage_docking* manager, enum_view_type view);
/* open the window pairs. safe to call every click: already docked views are just brought to the front. */
void open_texture_shop_windows(manage_docking* manager, system_element_context* ui_ctx);
void open_world_builder_windows(manage_docking* manager, system_element_context* ui_ctx);
/* Lighting tab next to Inspector. Windows menu + anything that wants to show it. */
void open_light_window(manage_docking* manager);
/* File Archive tab next to Hierarchy. starts docked there; call this to bring it back after it was closed. */
void open_file_archive_window(manage_docking* manager);
/* world builder click -> tools tab next to Hierarchy. texture shop click -> editor tab next to Scene / Game. */
void update_dock_spawn_requests(manage_docking* manager, system_element_context* ui_ctx);

/* window contents. every one gets an EXACT rect: x = node->x, width = node->w, y = content_y, height = content_h */
void draw_scene_archive_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_file_archive_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_sceneview_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer current_cmd);
void draw_gameview_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer current_cmd);
void draw_mode_menu_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_modeler_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_inspector_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_textureshop_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_textureshop_editor_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_textureshop_node_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_textureshop_skinmaker_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_timeline_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_worldbuilder_tool_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_board_builder_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_card_builder_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_sculpt_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_system_brush_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_system_paint_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);
void draw_system_light_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd);

/* mode menu (draw_system_mode_menu.c) */
void mode_menu_enter_mode(system_element_context* ui_element, system_mode_context ctx, bool open_windows);
void mode_menu_activate(system_element_context* ui_element, uint32_t id);
void mode_menu_follow_state(system_element_context* ui_element);     /* once per frame: flags + windows follow g_mode_state */
void mode_menu_open_sculpt_windows(system_element_context* ui_element);
void mode_menu_open_board_builder(system_element_context* ui_element);
void mode_menu_open_card_builder(system_element_context* ui_element);

bool find_intersecting_splitter_line(system_dock_node* node, int mx, int my, system_dock_node** out_node);
#endif