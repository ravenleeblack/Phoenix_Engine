#ifndef phoenix_engine_element_h
#define phoenix_engine_element_h

#include "engine_defines.h"
#include "engine_math.h"     /* editor_vec3 */
#include "engine_theme.h"    /* system_color. engine_mode.h includes THIS file, never the other way round */

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

/*Always use snake case with lowercase letters seprated by underscores. verbs always come first in anming scheme then nouns. DO NOT REMOVE THIS COMMENT*/
/* All math must goe into engine_math.h and engine_math.c no static fucking math in headers or source files.*/

struct editor_window_swapchain;
typedef struct 
{
    char main_menubar_text_buffer[512];
    char sub_menu_text_buffer[1024];
    char statusbar_text_buffer[512];
    char window_tabs_text_buffer[256];
    char content_view_text_buffer[2048];
    char mode_menu_text_buffer[2048];

} system_text_buffers;

typedef enum {
    tool_select,
    tool_translate,
    tool_rotate,
    tool_scale
} enum_dcc_tool;

typedef enum {
    mode_object,
    mode_edit_mesh
} enum_dcc_mode;

typedef enum {
    select_vert,
    select_edge,
    select_face
} enum_mesh_selection;

#define EM_NONE (-1)

typedef enum 
{
    surface_none,
    surface_uv,
    surface_paint,
    surface_sculpt
} enum_surface_tool;

typedef struct 
{
    float position[2];   
    float size[2];       
    float color[4];      
    float resolution[2]; 
} ui_push_constant;

typedef struct
{
    int mouse_x;
    int mouse_y;
    bool mouse_down;
    bool mouse_pressed;
    bool alt_pressed;
    bool middle_down;
    bool camera_manipulating;
} ui_input_state;

typedef struct {
    int width;
    int height;
    int offset_x;
    int offset_y;
} struct_viewport_bounds;

typedef enum {
    workflow_mode_uv,
    workflow_mode_model,
    workflow_mode_sculpt,
    workflow_mode_paint,
    workflow_mode_animation
} enum_workflow_mode;

typedef struct 
{
    ui_input_state input;
    uint32_t active_id;
    uint32_t hot_id;
    
    struct_viewport_bounds scene_view;
    struct_viewport_bounds game_view;

    enum_dcc_tool        active_tool;
    enum_dcc_mode        active_mode;
    enum_mesh_selection  active_select_type;
    enum_surface_tool    active_surface_mode;

    enum_workflow_mode   current_workflow;
    bool                 spawn_textureshop_window;
    bool                 spawn_worldbuilder_window;
    
       system_text_buffers text_buffers;
} system_element_context;



/* UNIFIED SYSTEM VIEWPORT TYPES - MATCHED TO NATIVE TRACKING PARAMETERS */
typedef enum {
    view_type_none = 0,
    view_type_mode_menu = 1,
    view_type_hierarchy = 2,
    view_type_scene     = 3,
    view_type_game      = 4,
    view_type_inspector = 5,
    view_type_modeler   = 6,
    view_type_textureshop   = 7,
    view_type_worldbld  = 8,
    view_type_timeline  = 9,
    view_type_texshop_editor = 10,
    view_type_worldbld_tools = 11,
    view_type_board_builder  = 12,
    view_type_card_builder   = 13,
    view_type_sculpt         = 14,
    view_type_brush          = 15,
    view_type_light           = 16,
    view_type_texshop_node    = 17,     /* Texture Shop Nodes: tabbed next to the Tex Editor (UV editor) */
    view_type_paint = 18,
    view_type_file_archive = 19,     /* File Archive: folders and files of the asset folder */
    view_type_texshop_skinmaker = 20
} enum_view_type;

/* UNIFIED DCC EDITOR INTERACTION CONTROL IDENTIFIERS */
typedef enum {
    // Mode menu button tracking IDs safely moved clear of main bar streams
    id_tool_grab       = 882001,
    id_tool_translate  = 882002,
    id_tool_rotate     = 882003,
    id_tool_scale      = 882004,
    id_mode_object     = 882005,
    id_mode_mesh       = 882006,
    id_work_uv         = 882007,
    id_work_model      = 882008,
    id_work_sculpt     = 882009,
    id_work_paint      = 882010,
    id_work_anim       = 882011,
    id_spawn_texshop   = 882012,
    id_spawn_worldbld  = 882013,

    // Static uncollidable system frame button identifiers
    id_sys_minimize    = 991001,
    id_sys_maximize    = 991002,
    id_sys_close       = 991003,

    // Main drop-down submenu toggle index mappings
    id_win_hierarchy   = 661001,
    id_win_scene       = 661002,
    id_win_game        = 661003,
    id_win_inspector   = 661004,
    id_win_modeler     = 661005,
    id_win_texshop     = 661006,
    id_win_worldbld    = 661007,
    id_win_timeline    = 661008
} enum_editor_tool_id;



/* ------------------------------------------------------------------------- */
/* ONE number box for the whole editor.                                       */
/*   drag left / right     = scrub the value (Shift = fine, no rounding)      */
/*   click without moving  = type a number:  0.0625   1/16   -2.5              */
/*                           Enter = apply   Esc = cancel   click away = apply */
/* Any panel that shows a float uses this instead of draw_text_element().     */
/*                                                                            */
/*   static const number_field_opts k_opts = { .step = 0.03125f, .quantum = 0.0625f };  */
/*   draw_number_field(cmd, ui_element, my_unique_id, x, y, w, h, &thing->value, &k_opts);        */
/*                                                                            */
/* A float holds multiples of 1/16 EXACTLY up to about +-1,048,576, so 0.0625 / 0.1250 / 0.2500 */
/* never drift. `decimals` only controls what is SHOWN.                       */
/* ------------------------------------------------------------------------- */
typedef struct {
    float step;            /* change per pixel of horizontal drag, in DISPLAY units (0 = 0.0625) */
    float quantum;         /* scrubbing rounds to a multiple of this (display units). 0 = free. Typed values are never rounded. */
    float display_scale;   /* shown = value * display_scale. 0 = 1. Radians shown as degrees: 57.29578f */
    float min, max;        /* display units. both 0 = unlimited */
    int   decimals;        /* digits after the point. 0 = 4 (so 1/16 reads 0.0625) */
} number_field_opts;

typedef enum {
    number_field_none = 0,
    number_field_scrubbed,     /* the value changed this frame because of a drag */
    number_field_committed     /* a typed value was applied this frame */
} number_field_result;

/* id must be unique per box (and the same every frame). Returns what happened to *value this frame. */
number_field_result draw_number_field(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id,
                                    int x, int y, int w, int h, float* value, const number_field_opts* opts);

/* true while any box is being scrubbed or typed into */
bool is_number_field_active(void);

/* true while a box is taking typed keys. Put   if (is_number_field_editing()) return;
   at the top of every keyboard shortcut (Delete, Backspace, 1 2 3, T ...) so typing a number does not trigger them. */
bool is_number_field_editing(void);


/* Base Initialization Operations */
void start_editor_elements(system_element_context* context);
void update_element_input(system_element_context* context, int mouse_x, int mouse_y, bool mouse_down);
void start_system_theme_profile(system_element_context* ui_ctx);

/* Core Low-Level Back-end Batch Rendering Primitives.
   push_quad_element: color.alpha is a plain 0..1 alpha, always. nothing else is encoded in it. */
void push_quad_element(VkCommandBuffer cmd, float x, float y, float w, float h, system_color color);

/* scene / game view composite. the renderer tag travels as its own argument, not inside a color. */
typedef enum {
    viewport_kind_scene,
    viewport_kind_game
} enum_viewport_kind;
void push_viewport_quad_element(VkCommandBuffer cmd, float x, float y, float w, float h, enum_viewport_kind kind);
void render_element_commands(VkCommandBuffer cmd, VkExtent2D extent, VkPipelineLayout pipeline_layout);

/* High-Level UI Layout Blocks */
/* High-Level UI Layout Blocks */
void draw_menubar_element(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, system_color color);
void draw_toolbar_element(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, system_color color);
void draw_statusbar_element(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, system_color color);

void draw_window_element(VkCommandBuffer cmd, system_element_context* context, const char* title, int x, int y, int w, int h, system_color base_color);


/* Immediate-Mode Button Controls */
bool draw_button_element(VkCommandBuffer cmd, system_element_context* context, uint32_t id, const char* label, int x, int y, int w, int h, system_color base_color);

/* Contextual Sub-Menu & Window Dropdowns */
/* Contextual Sub-Menu & Window Dropdowns */
bool draw_main_menu_header_button(VkCommandBuffer cmd, system_element_context* context, uint32_t id, const char* label, int x, int y, int w, int h, system_color base_color);
bool draw_menu_item_element(VkCommandBuffer cmd, system_element_context* context, uint32_t id, const char* label, int x, int y, int w, int h, system_color base_color);


void close_all_system_menus(void);
uint32_t get_active_open_menu_id(void);


/* Tabbed window parts. colors come from system_theme (tab_*), there is no color argument.
   strip behind the tabs -> tab_element, one tab -> tab_item, bar under the tabs -> tab_sub_menubar,
   body behind the contents -> tab_content_window */
typedef enum {
    tab_result_none,
    tab_result_clicked,
    tab_result_closed
} enum_tab_result;

void            draw_tab_element(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h);
enum_tab_result draw_tab_item(VkCommandBuffer cmd, system_element_context* context, uint32_t id, const char* label, int x, int y, int w, int h, bool is_active, bool closable);
void            draw_tab_sub_menubar(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h);
void            draw_tab_content_window(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h);

/* Strip window parts (mode menu): no tabs, one grip to drag it, colors from system_theme (strip_*) */
void draw_strip_window_body(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h);
bool draw_strip_grip(VkCommandBuffer cmd, system_element_context* context, uint32_t id, int x, int y, int w, int h);

/* Border Styling Functions System */
/* 1px Window Layout Accent Bars and Frame Outlines */
void draw_left_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int h, int thickness, system_color color);
void draw_right_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int h, int thickness, system_color color);
void draw_bottom_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int thickness, system_color color);
void draw_top_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int thickness, system_color color);

/* Multi-Axis Bound Containers */
void draw_parallel_top_and_bottom(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, int thickness, system_color color);
void draw_parallel_left_and_right(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, int thickness, system_color color);
void draw_full_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, int thickness, system_color color);
void draw_tab_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, int thickness, system_color color);


/* Utility Renderers */

/* Text Rendering Pipeline */
void draw_text_element(VkCommandBuffer cmd, system_element_context* context, const char* text, int x, int y, system_color color);

/* Frame Layout System Compositors */
void draw_system_menubar(VkCommandBuffer cmd, system_element_context* ui_ctx, struct editor_window_swapchain* swapchain);
void draw_system_toolbar(VkCommandBuffer cmd, system_element_context* ui_ctx, struct editor_window_swapchain* swapchain);
void draw_system_statusbar(VkCommandBuffer cmd, system_element_context* ui_ctx, struct editor_window_swapchain* swapchain, bool is_running);
void draw_system_viewports(VkCommandBuffer cmd, system_element_context* ui_ctx, struct editor_window_swapchain* swapchain);
void draw_system_heirarchy(VkCommandBuffer cmd, system_element_context* ui_ctx, struct editor_window_swapchain* swapchain);
void draw_system_tool_window(VkCommandBuffer cmd, system_element_context* ui_ctx, struct editor_window_swapchain* swapchain);

/* Viewport Integration Render pipelines */
void draw_sceneview_viewport(system_element_context* context, VkCommandBuffer cmd, int x, int y, int w, int h, VkDescriptorSet scene_texture_handle);
void draw_gameview_viewport_00(system_element_context* context, VkCommandBuffer cmd, int x, int y, int w, int h, VkDescriptorSet game_texture_00_handle);
void draw_gameview_viewport_01(system_element_context* context, VkCommandBuffer cmd, int x, int y, int w, int h, VkDescriptorSet game_texture_01_handle);
void draw_gameview_viewport_02(system_element_context* context, VkCommandBuffer cmd, int x, int y, int w, int h, VkDescriptorSet game_texture_02_handle);

void draw_sceneview_gimbal_overlay(VkCommandBuffer cmd, system_element_context* ui_ctx, struct_viewport_bounds bounds, editor_vec3 object_origin);

/* Icon primitives (system_icon_element.c): flat filled rect, and a 1px frame drawn inside the rect */
void draw_icon_element(VkCommandBuffer cmd, int x, int y, int w, int h, system_color c);
void draw_icon_border (VkCommandBuffer cmd, int x, int y, int w, int h, system_color c);

/* Scene view state: the sub menubar buttons in draw_system_sceneview.c. Defined in system_globals.c.
   Lit lives in g_light.enabled and AA in g_prefs.fxaa. */
typedef enum {
    scene_shading_wire,
    scene_shading_shaded,
    scene_shading_textured
} enum_scene_shading;

typedef struct {
    bool        show_grid;
    bool        show_origin;
    bool        show_cameras;       /* camera glyphs, frustum lines, blue culling plane */
    bool        camera_locked;      /* editor camera is held at lock_position / lock_rotation */
    editor_vec3 lock_position;
    editor_vec3 lock_rotation;

    enum_scene_shading shading;     /* wire / smooth shade / textured: pick one */
    bool        wire_on_shaded;     /* edges drawn over the shaded surface */
    bool        xray;               /* see through surfaces */

    float       wire_color[3];             /* Wireframe mode lines */
    float       wire_on_shaded_color[3];   /* Wireframe on Shaded lines */
} scene_view_state;

bool is_context_menu_open(void);     /* whoever defines the context menu (name was is_context_menu_open in the old code) */
bool validate_rectangle_hit(int mx, int my, int x, int y, int w, int h);

#endif