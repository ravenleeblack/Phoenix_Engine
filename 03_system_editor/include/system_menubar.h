#ifndef PHOENIX_SYSTEM_MENUBAR_H
#define PHOENIX_SYSTEM_MENUBAR_H

#define menubar_separator_h 7
#define menubar_header_pad 24
#define menubar_dropdown_min_w 150
#define menubar_control_w 32
#define menubar_control_count 3
#define menubar_header_y 4
#define menubar_header_h 20
#define menubar_char_w 8
#define main_menu_count 6

typedef enum {
    menu_action_none = 0,
    menu_action_new_file, menu_action_new_folder, menu_action_new_project, menu_action_new_scene,
    menu_action_save_scene, menu_action_save_project, menu_action_save_all,
    menu_action_import, menu_action_export, menu_action_preferences_submenu, menu_action_preferences,
    menu_action_build, menu_action_exit,
    menu_action_window_hierarchy, menu_action_window_scene, menu_action_window_game,
    menu_action_window_inspector, menu_action_window_modeler, menu_action_window_texture_shop,
    menu_action_window_world_builder, menu_action_window_timeline,
    menu_action_undo, menu_action_redo, menu_action_copy, menu_action_paste,
    menu_action_element_canvas, menu_action_element_panel, menu_action_element_button,
    menu_action_element_text, menu_action_element_image, menu_action_element_slider,
    menu_action_element_toggle, menu_action_element_input, menu_action_element_scroll,
    menu_action_object_empty, menu_action_object_terrain, menu_action_object_cube, menu_action_object_camera,
    menu_action_object_directional_light,
    menu_action_board_octagon, menu_action_board_diamond, menu_action_board_triangle_half, menu_action_board_triangle_quarter,
    menu_action_script_new, menu_action_script_attach, menu_action_script_open,
    menu_action_script_reload, menu_action_script_settings
} enum_menu_action;

typedef struct {
    const char* label;
    enum_menu_action action;
    bool implemented;
} struct_menu_item;

typedef struct {
    const char* title;
    const struct_menu_item* items;
    int item_count;
} struct_menu_def;

bool is_menubar_menu_open(void);

#endif
