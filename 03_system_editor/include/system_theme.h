#ifndef system_theme_h
#define system_theme_h

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

/* ========================================================================= */
/* system theme: the color struct, the palette, and every ui_element layout number.   */
/* the palette values live in system_theme.c. change a color or a size there  */
/* (or here for layout) and the whole editor follows.                         */
/* ========================================================================= */

/* ---------------------------------------------------------- color struct */
typedef struct system_color
{
    float red;
    float green;
    float blue;
    float alpha;
} system_color;


/* ------------------------------------------------------------- ui_element layout */
/* vertical stack, every bar touches the next one (no gaps)
   y=0   .. 26  menubar   (menubar_h = 28, last row y=27 is its border)
   y=28  .. 58  toolbar   (31px)
   y=59         toolbar bottom border (1px)   -> toolbar_h = 32 including border
   y=60  ..     dock root starts here (dock_top)
   ..    h-26   dock root ends here
   h-26  .. h   statusbar

   dock root must be set to: x=0, y=dock_top, w=window_w, h=window_h - dock_top - statusbar_h */
enum {
    menubar_y           = 0,
    menubar_h           = 28,
    toolbar_y           = menubar_y + menubar_h,     /* 28 */
    toolbar_h           = 32,                        /* includes 1px bottom border */
    dock_top            = toolbar_y + toolbar_h,     /* 60 */
    statusbar_h         = 26,

    /* dock leaf: [tab bar 26][sub bar 24 incl. 1px divider][content] nice and small to conserve*/
    tab_bar_h           = 26,
    tab_menu_h          = 24,
    dock_content_offset = tab_bar_h + tab_menu_h,     /* 56 */

    /* mode menu strip width (dock tree enforces this) */
    mode_menu_w         = 36,

    ui_font_w           = 8,
    ui_font_h           = 8,

    /* menubar dropdowns */
    menu_drop_y         = menubar_h,   /* dropdown starts right under the menubar */
    menu_pad_top        = 4,
    menu_pad_bottom     = 8,           /* panel extends this far below the last item */
    menu_item_h         = 22,
    menu_radius         = 6,            /* bottom corners only */
    button_radius        = 2,

    /* tabbed windows */
    tab_radius          = 4,
    tab_close_size      = 12,
    tab_close_margin    = 6,
    tab_text_pad        = 10,

    /* strip windows (mode menu): grip header instead of tabs */
    strip_header_h      = tab_bar_h,
    strip_button_w      = 28,
    strip_button_h      = 24,
    strip_button_gap    = 3,
    strip_group_gap     = 7,
};

/* rounded corner flags for push_rounded_quad */
enum {
    corner_tl = 1, corner_tr = 2, corner_bl = 4, corner_br = 8,
    corner_top    = corner_tl | corner_tr,
    corner_bottom = corner_bl | corner_br,
    corner_all    = corner_top | corner_bottom
};

/* -------------------------------------------------------- color palette */


extern system_color system_color_edit_mode_face_selection;
extern system_color system_color_edit_mode_active_face_hover;


/* ---- themed colors. every role has one color per theme: _00 maya (default), _01 zbrush, _02 plum, _03 graphite.
   to add a theme: add a _04 line to every role below + in system_theme.c, then write build_xxx() and add a row to k_themes[]. */

extern system_color system_color_active_hover_00;
extern system_color system_color_active_hover_01;
extern system_color system_color_active_hover_02;
extern system_color system_color_active_hover_03;

extern system_color system_color_close_hover_00;
extern system_color system_color_close_hover_01;
extern system_color system_color_close_hover_02;
extern system_color system_color_close_hover_03;

extern system_color system_color_text_00;
extern system_color system_color_text_01;
extern system_color system_color_text_02;
extern system_color system_color_text_03;

extern system_color system_color_active_text_00;
extern system_color system_color_active_text_01;
extern system_color system_color_active_text_02;
extern system_color system_color_active_text_03;

extern system_color system_color_border_00;
extern system_color system_color_border_01;
extern system_color system_color_border_02;
extern system_color system_color_border_03;

extern system_color system_color_accent_00;
extern system_color system_color_accent_01;
extern system_color system_color_accent_02;
extern system_color system_color_accent_03;

extern system_color system_color_main_menu_bg_00;
extern system_color system_color_main_menu_bg_01;
extern system_color system_color_main_menu_bg_02;
extern system_color system_color_main_menu_bg_03;

extern system_color system_color_main_menu_button_bg_00;
extern system_color system_color_main_menu_button_bg_01;
extern system_color system_color_main_menu_button_bg_02;
extern system_color system_color_main_menu_button_bg_03;

extern system_color system_color_main_menu_vert_sep_00;
extern system_color system_color_main_menu_vert_sep_01;
extern system_color system_color_main_menu_vert_sep_02;
extern system_color system_color_main_menu_vert_sep_03;

extern system_color system_color_main_menu_hort_sep_00;
extern system_color system_color_main_menu_hort_sep_01;
extern system_color system_color_main_menu_hort_sep_02;
extern system_color system_color_main_menu_hort_sep_03;

extern system_color system_color_main_menu_active_text_00;
extern system_color system_color_main_menu_active_text_01;
extern system_color system_color_main_menu_active_text_02;
extern system_color system_color_main_menu_active_text_03;

extern system_color system_color_tool_menu_bg_00;
extern system_color system_color_tool_menu_bg_01;
extern system_color system_color_tool_menu_bg_02;
extern system_color system_color_tool_menu_bg_03;

extern system_color system_color_tool_menu_button_bg_00;
extern system_color system_color_tool_menu_button_bg_01;
extern system_color system_color_tool_menu_button_bg_02;
extern system_color system_color_tool_menu_button_bg_03;

extern system_color system_color_tool_menu_stop_button_bg_00;
extern system_color system_color_tool_menu_stop_button_bg_01;
extern system_color system_color_tool_menu_stop_button_bg_02;
extern system_color system_color_tool_menu_stop_button_bg_03;

extern system_color system_color_tool_menu_run_button_bg_00;
extern system_color system_color_tool_menu_run_button_bg_01;
extern system_color system_color_tool_menu_run_button_bg_02;
extern system_color system_color_tool_menu_run_button_bg_03;

extern system_color system_color_tool_menu_vert_sep_00;
extern system_color system_color_tool_menu_vert_sep_01;
extern system_color system_color_tool_menu_vert_sep_02;
extern system_color system_color_tool_menu_vert_sep_03;

extern system_color system_color_tool_menu_hort_sep_00;
extern system_color system_color_tool_menu_hort_sep_01;
extern system_color system_color_tool_menu_hort_sep_02;
extern system_color system_color_tool_menu_hort_sep_03;

extern system_color system_color_stop_status_menu_bg_00;
extern system_color system_color_stop_status_menu_bg_01;
extern system_color system_color_stop_status_menu_bg_02;
extern system_color system_color_stop_status_menu_bg_03;

extern system_color system_color_run_status_menu_bg_00;
extern system_color system_color_run_status_menu_bg_01;
extern system_color system_color_run_status_menu_bg_02;
extern system_color system_color_run_status_menu_bg_03;

extern system_color system_color_stop_status_menu_text_00;
extern system_color system_color_stop_status_menu_text_01;
extern system_color system_color_stop_status_menu_text_02;
extern system_color system_color_stop_status_menu_text_03;

extern system_color system_color_run_status_menu_text_00;
extern system_color system_color_run_status_menu_text_01;
extern system_color system_color_run_status_menu_text_02;
extern system_color system_color_run_status_menu_text_03;

extern system_color system_color_mode_menu_bg_00;
extern system_color system_color_mode_menu_bg_01;
extern system_color system_color_mode_menu_bg_02;
extern system_color system_color_mode_menu_bg_03;

extern system_color system_color_mode_menu_button_bg_00;
extern system_color system_color_mode_menu_button_bg_01;
extern system_color system_color_mode_menu_button_bg_02;
extern system_color system_color_mode_menu_button_bg_03;

extern system_color system_color_dropdown_outline_00;
extern system_color system_color_dropdown_outline_01;
extern system_color system_color_dropdown_outline_02;
extern system_color system_color_dropdown_outline_03;

extern system_color system_color_menu_hover_00;
extern system_color system_color_menu_hover_01;
extern system_color system_color_menu_hover_02;
extern system_color system_color_menu_hover_03;

extern system_color system_color_menu_open_00;
extern system_color system_color_menu_open_01;
extern system_color system_color_menu_open_02;
extern system_color system_color_menu_open_03;

extern system_color system_color_menu_item_selected_00;
extern system_color system_color_menu_item_selected_01;
extern system_color system_color_menu_item_selected_02;
extern system_color system_color_menu_item_selected_03;

extern system_color system_color_menu_separator_00;
extern system_color system_color_menu_separator_01;
extern system_color system_color_menu_separator_02;
extern system_color system_color_menu_separator_03;

extern system_color system_color_menu_text_00;
extern system_color system_color_menu_text_01;
extern system_color system_color_menu_text_02;
extern system_color system_color_menu_text_03;

extern system_color system_color_menu_header_text_00;
extern system_color system_color_menu_header_text_01;
extern system_color system_color_menu_header_text_02;
extern system_color system_color_menu_header_text_03;

extern system_color system_color_button_text_00;
extern system_color system_color_button_text_01;
extern system_color system_color_button_text_02;
extern system_color system_color_button_text_03;

extern system_color system_color_context_menu_button_00;
extern system_color system_color_context_menu_button_01;
extern system_color system_color_context_menu_button_02;
extern system_color system_color_context_menu_button_03;

extern system_color system_color_context_menu_button_open_00;
extern system_color system_color_context_menu_button_open_01;
extern system_color system_color_context_menu_button_open_02;
extern system_color system_color_context_menu_button_open_03;

extern system_color system_color_context_menu_border_00;
extern system_color system_color_context_menu_border_01;
extern system_color system_color_context_menu_border_02;
extern system_color system_color_context_menu_border_03;

extern system_color system_color_context_menu_center_00;
extern system_color system_color_context_menu_center_01;
extern system_color system_color_context_menu_center_02;
extern system_color system_color_context_menu_center_03;

extern system_color system_color_context_menu_center_ring_00;
extern system_color system_color_context_menu_center_ring_01;
extern system_color system_color_context_menu_center_ring_02;
extern system_color system_color_context_menu_center_ring_03;

extern system_color system_color_context_menu_text_00;
extern system_color system_color_context_menu_text_01;
extern system_color system_color_context_menu_text_02;
extern system_color system_color_context_menu_text_03;

extern system_color system_color_context_menu_arrow_00;
extern system_color system_color_context_menu_arrow_01;
extern system_color system_color_context_menu_arrow_02;
extern system_color system_color_context_menu_arrow_03;

extern system_color system_color_context_menu_dropdown_00;
extern system_color system_color_context_menu_dropdown_01;
extern system_color system_color_context_menu_dropdown_02;
extern system_color system_color_context_menu_dropdown_03;

extern system_color system_color_context_menu_item_hover_00;
extern system_color system_color_context_menu_item_hover_01;
extern system_color system_color_context_menu_item_hover_02;
extern system_color system_color_context_menu_item_hover_03;

extern system_color system_color_context_menu_item_text_00;
extern system_color system_color_context_menu_item_text_01;
extern system_color system_color_context_menu_item_text_02;
extern system_color system_color_context_menu_item_text_03;

extern system_color system_color_dock_frame_00;
extern system_color system_color_dock_frame_01;
extern system_color system_color_dock_frame_02;
extern system_color system_color_dock_frame_03;

extern system_color system_color_dock_splitter_00;
extern system_color system_color_dock_splitter_01;
extern system_color system_color_dock_splitter_02;
extern system_color system_color_dock_splitter_03;

extern system_color system_color_tab_element_00;
extern system_color system_color_tab_element_01;
extern system_color system_color_tab_element_02;
extern system_color system_color_tab_element_03;

extern system_color system_color_tab_item_00;
extern system_color system_color_tab_item_01;
extern system_color system_color_tab_item_02;
extern system_color system_color_tab_item_03;

extern system_color system_color_tab_item_active_00;
extern system_color system_color_tab_item_active_01;
extern system_color system_color_tab_item_active_02;
extern system_color system_color_tab_item_active_03;

extern system_color system_color_tab_item_accent_00;
extern system_color system_color_tab_item_accent_01;
extern system_color system_color_tab_item_accent_02;
extern system_color system_color_tab_item_accent_03;

extern system_color system_color_tab_item_text_00;
extern system_color system_color_tab_item_text_01;
extern system_color system_color_tab_item_text_02;
extern system_color system_color_tab_item_text_03;

extern system_color system_color_tab_item_text_active_00;
extern system_color system_color_tab_item_text_active_01;
extern system_color system_color_tab_item_text_active_02;
extern system_color system_color_tab_item_text_active_03;

extern system_color system_color_tab_close_hover_00;
extern system_color system_color_tab_close_hover_01;
extern system_color system_color_tab_close_hover_02;
extern system_color system_color_tab_close_hover_03;

extern system_color system_color_tab_close_text_00;
extern system_color system_color_tab_close_text_01;
extern system_color system_color_tab_close_text_02;
extern system_color system_color_tab_close_text_03;

extern system_color system_color_tab_sub_menubar_00;
extern system_color system_color_tab_sub_menubar_01;
extern system_color system_color_tab_sub_menubar_02;
extern system_color system_color_tab_sub_menubar_03;

extern system_color system_color_tab_sub_menubar_divider_00;
extern system_color system_color_tab_sub_menubar_divider_01;
extern system_color system_color_tab_sub_menubar_divider_02;
extern system_color system_color_tab_sub_menubar_divider_03;

extern system_color system_color_tab_sub_menubar_button_00;
extern system_color system_color_tab_sub_menubar_button_01;
extern system_color system_color_tab_sub_menubar_button_02;
extern system_color system_color_tab_sub_menubar_button_03;

extern system_color system_color_tab_sub_menubar_button_active_00;
extern system_color system_color_tab_sub_menubar_button_active_01;
extern system_color system_color_tab_sub_menubar_button_active_02;
extern system_color system_color_tab_sub_menubar_button_active_03;

extern system_color system_color_tab_content_window_bg_00;
extern system_color system_color_tab_content_window_bg_01;
extern system_color system_color_tab_content_window_bg_02;
extern system_color system_color_tab_content_window_bg_03;

extern system_color system_color_edit_mode_multicut_percent_bg_00;
extern system_color system_color_edit_mode_multicut_percent_bg_01;
extern system_color system_color_edit_mode_multicut_percent_bg_02;
extern system_color system_color_edit_mode_multicut_percent_bg_03;

extern system_color system_color_edit_mode_multicut_percent_text_00;
extern system_color system_color_edit_mode_multicut_percent_text_01;
extern system_color system_color_edit_mode_multicut_percent_text_02;
extern system_color system_color_edit_mode_multicut_percent_text_03;

extern system_color system_color_scene_manipulator_bg_00;
extern system_color system_color_scene_manipulator_bg_01;
extern system_color system_color_scene_manipulator_bg_02;
extern system_color system_color_scene_manipulator_bg_03;

extern system_color system_color_scene_manipulator_input_bg_00;
extern system_color system_color_scene_manipulator_input_bg_01;
extern system_color system_color_scene_manipulator_input_bg_02;
extern system_color system_color_scene_manipulator_input_bg_03;

extern system_color system_color_scene_manipulator_text_00;
extern system_color system_color_scene_manipulator_text_01;
extern system_color system_color_scene_manipulator_text_02;
extern system_color system_color_scene_manipulator_text_03;

extern system_color system_color_scene_manipulator_input_text_00;
extern system_color system_color_scene_manipulator_input_text_01;
extern system_color system_color_scene_manipulator_input_text_02;
extern system_color system_color_scene_manipulator_input_text_03;

extern system_color system_color_strip_window_body_00;
extern system_color system_color_strip_window_body_01;
extern system_color system_color_strip_window_body_02;
extern system_color system_color_strip_window_body_03;

extern system_color system_color_strip_grip_00;
extern system_color system_color_strip_grip_01;
extern system_color system_color_strip_grip_02;
extern system_color system_color_strip_grip_03;

extern system_color system_color_strip_grip_lines_00;
extern system_color system_color_strip_grip_lines_01;
extern system_color system_color_strip_grip_lines_02;
extern system_color system_color_strip_grip_lines_03;

extern system_color system_color_strip_button_00;
extern system_color system_color_strip_button_01;
extern system_color system_color_strip_button_02;
extern system_color system_color_strip_button_03;

extern system_color system_color_strip_button_selected_00;
extern system_color system_color_strip_button_selected_01;
extern system_color system_color_strip_button_selected_02;
extern system_color system_color_strip_button_selected_03;

extern system_color system_color_tab_content_editor_bg_00;
extern system_color system_color_tab_content_editor_bg_01;
extern system_color system_color_tab_content_editor_bg_02;
extern system_color system_color_tab_content_editor_bg_03;



/* ------------------------------------------------------------ theme struct */
/* every color the editor draws comes from system_theme. change a field in a theme builder
   (system_theme.c) and every window that uses it follows. alpha lives in the system_color. */
typedef struct phoenix_system_theme
{
    /*Global theme*/
    system_color system_active_hover;
    system_color system_close_hover;
    system_color system_text;
    system_color system_active_text;
    system_color system_border;
    system_color system_accent;        /* the one highlight color: selected buttons, selected menu rows */
    float        system_hover_delta;   /* the one hover brightness step every widget shares */

    /* bars: menubar, toolbar, statusbar */
    system_color menubar_background;
    system_color menubar_button;

    system_color menu_vert_separator;
    system_color menu_hort_separator;
    
    system_color menu_active_text;


    system_color toolbar_background;
    system_color toolbar_icon_button;
    system_color toolbar_stop_button;
    system_color toolbar_run_button;

    system_color tool_menu_vert_separator;
    system_color tool_menu_hort_separator;

    system_color statusbar_stop_background;
    system_color statusbar_run_background;
    system_color statusbar_stop_text;
    system_color statusbar_run_text;

    system_color mode_menu_background;
    system_color mode_menu_icon_button;


    /* menus and buttons */
    system_color dropdown_outline;
    system_color menu_hover;
    system_color menu_open;
    system_color menu_item_selected;
    system_color menu_separator;
    system_color menu_text;
    system_color menu_header_text;
    system_color button_text;
    float        button_hover_delta;
    float        button_press_delta;
    float        menu_hover_delta;
    float        menu_header_hover_delta;

    /* right click radial menu (system_context_menu.c). derived from the globals for every theme in derive_from_globals();
       a theme that wants its own look sets these AFTER its derive_from_globals() call. */
    system_color context_menu_button;          /* a slot button */
    system_color context_menu_button_open;     /* the slot whose dropdown is showing */
    system_color context_menu_border;          /* outline around slots and the dropdown */
    system_color context_menu_center;          /* the center circle */
    system_color context_menu_center_ring;     /* ring around the center circle */
    system_color context_menu_text;            /* slot labels + the title in the center */
    system_color context_menu_arrow;           /* the > on slots that open a dropdown */
    system_color context_menu_dropdown;        /* dropdown body */
    system_color context_menu_item_hover;      /* hovered dropdown row */
    system_color context_menu_item_text;       /* dropdown row text (not-wired rows are drawn dimmer) */
    float        context_menu_hover_delta;     /* hover brightness step on slots and the center circle */

    /* dock frame */
    system_color dock_frame;
    system_color dock_splitter;

    /* tabbed window, four parts: tab_element > tab_item, tab_sub_menubar, tab_content_window */
    system_color tab_element;                 /* strip behind the tabs */
    system_color tab_item;
    system_color tab_item_active;
    system_color tab_item_accent;             /* unused: the line on top of the active tab was removed */
    system_color tab_item_text;
    system_color tab_item_text_active;
    system_color tab_close_hover;
    system_color tab_close_text;
    float        tab_hover_delta;
    system_color tab_sub_menubar;             /* bar under the tabs */
    system_color tab_sub_menubar_divider;
    system_color tab_sub_menubar_button;
    system_color tab_sub_menubar_button_active;
    system_color tab_content_window;          /* body behind the window contents */
    system_color tab_content_window_modeler;          /* body behind the window contents */
    system_color tab_content_window_gameview_00;          /* body behind the window contents */
    system_color tab_content_inspector;          /* body behind the window contents */
    system_color tab_content_window_paint;          /* body behind the window contents */
    system_color tab_content_window_sculpt;          /* body behind the window contents */
    system_color tab_content_window_sceneview;          /* body behind the window contents */
    system_color tab_content_window_fileview;          /* body behind the window contents */

    system_color tab_content_window_textureshop;          /* body behind the window contents */
    system_color tab_content_window_textureshop_editor;          /* body behind the window contents */
    system_color tab_content_window_textureshop_node;          /* body behind the window contents */
    system_color tab_content_window_textureshop_skin;          /* body behind the window contents */

    /* EDIT MODE MULTICUT PRECENT WINDOW AND TEXT */
    system_color edit_mode_multicut_percent_bg;
    system_color edit_mode_multicut_percent_text;

    /* Scene Manipulator */
    system_color scene_manipulator_background;
    system_color scene_manipulator_input_background;
    system_color scene_manipulator_text;
    system_color scene_manipulator_input_text;

    /* strip window (mode menu) */
    system_color strip_window_body;
    system_color strip_grip;
    system_color strip_grip_lines;
    system_color strip_button;
    system_color strip_button_selected;

    /* EDIT MODE THEMES */
    system_color system_color_edit_mode_target_weld;
    system_color system_color_edit_mode_normal_color;

    system_color system_color_edit_mode_vert_color;
    system_color system_color_edit_mode_vert_active_color;

    system_color system_color_edit_mode_edge_color;
    system_color system_color_edit_mode_edge_active_color;

    system_color system_color_edit_mode_face_color;
    system_color system_color_edit_mode_face_active_color;

    system_color system_color_edit_mode_extrude_face_color;
    system_color system_color_edit_mode_extrude_face_active_color;

    system_color system_color_edit_mode_vert_selection_color;
    system_color system_color_edit_mode_edge_selection_color;
    system_color system_color_edit_mode_face_selection_color;

    system_color system_color_edit_mode_hover_color;        /* element under the cursor, row under the cursor in Insert Loop */
    system_color system_color_edit_mode_preview_color;      /* what a tool is about to do: loop cut lines, weld drag line */
    system_color system_color_edit_mode_invalid_color;      /* what a tool cannot do: loop cut on a ring that ends on a triangle */


    system_color system_edit_mode_active_face_hover;
} phoenix_system_theme;

extern phoenix_system_theme system_theme;

/* ---- theme switching ----
   every theme is a builder in system_theme.c. add one: write build_xxx(), add a row to k_themes[]. */
int         theme_count(void);
const char* theme_name(int index);
int         theme_get_index(void);
void        theme_set(int index);          /* remember + load into system_theme. safe to call any time, even before the window exists */
void        theme_apply_current(void);     /* reload the remembered theme. called once from start_editor_elements. */
void        theme_load_default(void);      /* = theme_set(0) */

/* same color, different alpha */
system_color color_with_alpha(system_color color, float alpha);

/* dimmed versions of system_text, so hints and disabled rows follow every theme */
system_color get_theme_hint_color(void);       /* secondary text: hints, "not implemented yet" notes */
system_color get_theme_disabled_color(void);   /* disabled buttons and rows */

/* ------------------------------------------------------------- functions */
system_color process_theme_color(struct system_color raw_color, float brightness_modifier);

/* rounded rectangle built from 1px strips. only used by buttons, tabs and menu dropdowns. */
void push_rounded_quad(VkCommandBuffer cmd, float x, float y, float w, float h,
                       system_color c, int radius, int corners);

extern bool g_swapchain_is_srgb;

#endif