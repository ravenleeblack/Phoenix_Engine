#include "system_globals.h"
#include <stdarg.h>
#include <stdio.h>

/* local filled-rect helper: system_color in, one push_quad_element out. */
void push_menubar_rect(VkCommandBuffer cmd, float x, float y, float w, float h, system_color c)
{
    push_quad_element(cmd, x, y, w, h, c);
}

/* ========================================================================= */
/*  main menu bar                                                            */
/*                                                                           */
/*  [file][edit][element][objects][scripts] | dynamic mode options | [_][o][x]
/*  '------------- static -----------------'   '-- only dynamic --'  '-static-'
/*                                                                           */
/*  the static buttons are placed from the left edge_to_face_array / right edge_to_face_array only, so   */
/*  nothing in the dynamic zone can push, cover or reorder them. the dynamic */
/*  zone is clipped to the gap between the two vertical separators.          */
/*                                                                           */
/*  input here uses raw mouse state + one rising edge_to_face_array per frame, because the */
/*  dropdowns overlap the dock and the dock handles input first.             */
/* ========================================================================= */

const struct_menu_item file_menu_items[] = {
    { "New File",       menu_action_new_file,     true },
    { "New Folder",     menu_action_new_folder,   true },
    { "New Project",    menu_action_new_project,  false },
    { "New Scene",      menu_action_new_scene,    false },
    { NULL,              menu_action_none,        false },
    { "Save Scene",     menu_action_save_scene,   false },
    { "Save Project",   menu_action_save_project, false },
    { "Save All",       menu_action_save_all,     false },
    { NULL,              menu_action_none,        false },
    { "Import...",      menu_action_import,       true },
    { "Export...",      menu_action_export,       true },
    { "Preferences",    menu_action_preferences_submenu, true },
    { NULL,              menu_action_none,        false },
    { "Build",          menu_action_build,        false },
    { NULL,              menu_action_none,        false },
    { "Exit",           menu_action_exit,         true },
};

const struct_menu_item preferences_submenu_items[] = {
    { "Preferences...", menu_action_preferences,  true },
};

const struct_menu_def preferences_submenu = { "Preferences", preferences_submenu_items, (int)(sizeof(preferences_submenu_items) / sizeof(preferences_submenu_items[0])) };

const struct_menu_item edit_menu_items[] = {
    { "Undo",  menu_action_undo,  true },
    { "Redo",  menu_action_redo,  true },
    { NULL,    menu_action_none,   false },
    { "Copy",  menu_action_copy,   false },
    { "Paste", menu_action_paste,  false },
};

/* real engine ui_element building blocks (not unity-style canvas objects). */
const struct_menu_item element_menu_items[] = {
    { "Canvas",      menu_action_element_canvas, false },
    { "Panel",       menu_action_element_panel,  false },
    { NULL,          menu_action_none,          false },
    { "Button",      menu_action_element_button, false },
    { "Text",        menu_action_element_text,   false },
    { "Image",       menu_action_element_image,  false },
    { NULL,          menu_action_none,          false },
    { "Slider",      menu_action_element_slider, false },
    { "Toggle",      menu_action_element_toggle, false },
    { "Input Field", menu_action_element_input,  false },
    { "Scroll View", menu_action_element_scroll, false },
};

/* only what can actually be placed in a scene right now. */
const struct_menu_item objects_menu_items[] = {
    { "Create Empty",       menu_action_object_empty,             true },
    { NULL,                 menu_action_none,                    false },
    { "Terrain",            menu_action_object_terrain,           true },
    { "Cube",               menu_action_object_cube,              true },
    { "Camera",             menu_action_object_camera,            true },
    { "Directional Light",  menu_action_object_directional_light, true },
    { NULL,                 menu_action_none,                    false },
    { "Board Octagon",          menu_action_board_octagon,          true },
    { "Board Diamond",          menu_action_board_diamond,          true },
    { "Board Triangle (half)",  menu_action_board_triangle_half,    true },
    { "Board Triangle (quarter)", menu_action_board_triangle_quarter, true },
};

const struct_menu_item scripts_menu_items[] = {
    { "New Script",         menu_action_script_new,      false },
    { "Attach Script",      menu_action_script_attach,   false },
    { "Open in Editor",     menu_action_script_open,     false },
    { NULL,                 menu_action_none,            false },
    { "Reload Scripts",     menu_action_script_reload,   false },
    { "Script Settings...", menu_action_script_settings, false },
};

const struct_menu_item window_menu_items[] = {
    { "Hierarchy",     menu_action_window_hierarchy,     true },
    { "Scene",         menu_action_window_scene,         true },
    { "Game",          menu_action_window_game,          true },
    { "Inspector",     menu_action_window_inspector,     true },
    { NULL,            menu_action_none,                 false },
    { "Modeler",       menu_action_window_modeler,       true },
    { "Texture Shop",  menu_action_window_texture_shop,  true },
    { "World Builder", menu_action_window_world_builder, true },
    { "Timeline",      menu_action_window_timeline,      true },
};

const struct_menu_def main_menus[] = {
    { "File",    file_menu_items,    (int)(sizeof(file_menu_items) / sizeof(file_menu_items[0])) },
    { "Edit",    edit_menu_items,    (int)(sizeof(edit_menu_items) / sizeof(edit_menu_items[0])) },
    { "Element", element_menu_items, (int)(sizeof(element_menu_items) / sizeof(element_menu_items[0])) },
    { "Objects", objects_menu_items, (int)(sizeof(objects_menu_items) / sizeof(objects_menu_items[0])) },
    { "Scripts", scripts_menu_items, (int)(sizeof(scripts_menu_items) / sizeof(scripts_menu_items[0])) },
    { "Window",  window_menu_items,  (int)(sizeof(window_menu_items) / sizeof(window_menu_items[0])) },
};
/* ------------------------------------------------------------------------- */
/* state (one menu open at a time, -1 = none)                                 */
/* ------------------------------------------------------------------------- */

int  menubar_open_menu_index    = -1;
bool menubar_mouse_was_pressed  = false;     /* rising edge_to_face_array of left mouse this frame */
int  menubar_header_x[main_menu_count];      /* header x positions, shared with the dropdown drawn in the overlay stage */
bool menubar_click_used         = false;     /* the bar already used this frame's click */
bool previous_menubar_mouse_down = false;
bool menubar_submenu_open        = false;    /* the File > Preferences flyout */
int  menubar_submenu_row_y       = 0;        /* screen y of the row that opens the flyout */
const char* menubar_control_labels[menubar_control_count] = { "_", "[]", "X" };

/* the dock can call this to ignore clicks while a dropdown is open. */
bool is_menubar_menu_open(void) { return menubar_open_menu_index >= 0; }

bool is_point_in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < (x + w) && py >= y && py < (y + h);
}

void set_status_not_implemented(const char* label)
{
    snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "%s: not implemented yet.", label);
}

/* status text goes to the statusbar (draw_system_statusbar.c reads status_bar_text_buffer). */
void set_menubar_status(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), format, args);
    va_end(args);
}

/* ------------------------------------------------------------------------- */
/* actions                                                                    */
/* ------------------------------------------------------------------------- */

void toggle_menubar_window(enum_view_type view, enum_view_type beside, enum_view_type fallback)
{
    if (dock_find_leaf_with_view(dock_workspace.root, view)) {
        dock_close_view_type(&dock_workspace, view);
    } else {
        spawn_dock_view_beside(&dock_workspace, beside, fallback, view);
    }
}

void run_menu_action(const struct_menu_item* item, system_element_context* ui_ctx)
{
    switch (item->action) {
    case menu_action_exit:
        PostQuitMessage(0);
        break;

    case menu_action_undo:
        perform_system_undo();
        break;
    case menu_action_redo:
        perform_system_redo();
        break;

    /* create_system_* register the scene object themselves. do not spawn a second one. */
    case menu_action_object_cube: {
        const float cube_size = 4.0f;
        editor_vec3 spawn_pos = { cube_size * 0.5f, cube_size * 0.5f, cube_size * 0.5f };   /* fills the 4m cell 0..4 */
        phoenix_info("Menubar", "Create Cube clicked.");
        set_menubar_status("Created: Cube Primitive (4m).");
        create_system_cube(spawn_pos, cube_size, "Cube Primitive (4m)",
                           render_context.logical_device, render_context.physical_device,
                           render_context.command_pool, render_context.graphics_queue);
        break;
    }
    case menu_action_object_camera: {
        editor_vec3 cam_pos = { 0.0f, 5.0f, 12.0f };
        phoenix_info("Menubar", "Create Camera clicked.");
        set_menubar_status("Created: Editor Camera Object.");
        create_system_camera(cam_pos, 1.0f, "Editor Camera Object",
                             render_context.logical_device, render_context.physical_device,
                             render_context.command_pool, render_context.graphics_queue);
        break;
    }
    /* empty game object: transform only, no mesh. obj_vert_selected on creation, the gizmo works on it. */
    case menu_action_object_empty: {
        editor_vec3 origin = { 0.0f, 0.0f, 0.0f };
        create_system_empty("Empty", origin);
        set_menubar_status("Created: Empty.");
        break;
    }
    /* directional light: its rotation drives the sun while it exists (system_direction_light.c) */
    case menu_action_object_directional_light: {
        editor_vec3 light_pos = { 0.0f, 12.0f, 0.0f };
        create_system_directional_light(light_pos, 1.0f, "Directional Light",
                                        render_context.logical_device, render_context.physical_device,
                                        render_context.command_pool, render_context.graphics_queue);
        set_menubar_status("Created: Directional Light.");
        break;
    }
    case menu_action_object_terrain:
        phoenix_info("Menubar", "Create Terrain clicked.");
        create_terrain_system();   /* registers/rebuilds the single terrain object itself */
        set_menubar_status("Terrain created / rebuilt from the Inspector settings.");
        break;

    /* board pieces: board_spawn_tile registers the scene object, the object database entry and the edit mesh */
    case menu_action_board_octagon:          create_system_board_octagon(NULL);          set_menubar_status("Created: Board Octagon.");          break;
    case menu_action_board_diamond:          create_system_board_diamond(NULL);          set_menubar_status("Created: Board Diamond.");          break;
    case menu_action_board_triangle_half:    create_system_board_triangle_half(NULL);    set_menubar_status("Created: Board Triangle Half.");    break;
    case menu_action_board_triangle_quarter: create_system_board_triangle_quarter(NULL); set_menubar_status("Created: Board Triangle Quarter."); break;

    case menu_action_preferences:
        open_preferences_window();
        set_menubar_status("Preferences opened.");
        break;
    case menu_action_import:
        import_asset_from_dialog();
        break;
    case menu_action_export:
        open_export_window();
        break;
    case menu_action_window_hierarchy:
        toggle_menubar_window(view_type_hierarchy, view_type_scene, view_type_inspector);
        break;
    case menu_action_window_scene:
        toggle_menubar_window(view_type_scene, view_type_hierarchy, view_type_game);
        break;
    case menu_action_window_game:
        toggle_menubar_window(view_type_game, view_type_scene, view_type_hierarchy);
        break;
    case menu_action_window_inspector:
        toggle_menubar_window(view_type_inspector, view_type_scene, view_type_hierarchy);
        break;
    case menu_action_window_modeler:
        toggle_menubar_window(view_type_modeler, view_type_inspector, view_type_hierarchy);
        break;
    case menu_action_window_texture_shop:
        if (dock_find_leaf_with_view(dock_workspace.root, view_type_textureshop))
            dock_close_view_type(&dock_workspace, view_type_textureshop);
        else
            open_texture_shop_windows(&dock_workspace, ui_ctx);
        break;
    case menu_action_window_world_builder:
        ui_ctx->spawn_worldbuilder_window = !dock_find_leaf_with_view(dock_workspace.root, view_type_worldbld);
        if (ui_ctx->spawn_worldbuilder_window)
            open_world_builder_windows(&dock_workspace, ui_ctx);
        else
            dock_close_view_type(&dock_workspace, view_type_worldbld);
        break;
    case menu_action_window_timeline:
        toggle_menubar_window(view_type_timeline, view_type_scene, view_type_inspector);
        break;

    /* same thing the file archive right click menu does: goes into the obj_vert_selected folder (or the asset root) */
    case menu_action_new_file:
        file_archive_create_file("New File", FILE_ARCHIVE_EXT_TEXT);
        set_menubar_status("Created: New File.");
        break;
    case menu_action_new_folder:
        file_archive_create_folder();
        set_menubar_status("Created: New Folder.");
        break;

    default:
        set_status_not_implemented(item->label);
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* dropdown geometry + drawing                                                */
/* ------------------------------------------------------------------------- */

int get_menu_width(const struct_menu_def* menu)
{
    int w = menubar_dropdown_min_w;
    for (int i = 0; i < menu->item_count; i++) {
        if (!menu->items[i].label) continue;
        int needed = (int)strlen(menu->items[i].label) * menubar_char_w + 28;
        if (needed > w) w = needed;
    }
    return w;
}

int get_menu_height(const struct_menu_def* menu)
{
    int h = menu_pad_top + menu_pad_bottom;
    for (int i = 0; i < menu->item_count; i++) h += menu->items[i].label ? menu_item_h : menubar_separator_h;
    return h;
}

/* panel behind a dropdown: square top (butts against the menubar), rounded bottom, 1px border. */
void draw_dropdown_panel(VkCommandBuffer cmd, int x, int w, int h)
{
    push_menubar_rect(cmd, (float)(x - 1), (float)menu_drop_y, (float)(w + 2), (float)(h + 1),
                      system_theme.dropdown_outline);
    push_menubar_rect(cmd, (float)x, (float)menu_drop_y, (float)w, (float)h,
                      system_theme.menubar_background);
}

/* flyout panel for a submenu: same look as the dropdown, placed at (x, y) instead of under the menubar. */
void draw_flyout_panel(VkCommandBuffer cmd, int x, int y, int w, int h)
{
    push_menubar_rect(cmd, (float)(x - 1), (float)(y - 1), (float)(w + 2), (float)(h + 2), system_theme.dropdown_outline);
    push_menubar_rect(cmd, (float)x, (float)y, (float)w, (float)h, system_theme.menubar_background);
}

/* ------------------------------------------------------------------------- */
/* dynamic zone: options for whatever mode the mode menu obj_vert_selected (maya-like) */
/* clipped to [x0, x1). adding things here can never move a static button.    */
/* ------------------------------------------------------------------------- */

bool draw_dynamic_zone_button(VkCommandBuffer cmd, system_element_context* ui_ctx, const char* label,
                                     int* x, int x1, bool is_active)
{
    int w = (int)strlen(label) * menubar_char_w + 16;
    if (*x + w > x1) return false;                     /* clipped: not drawn, not clickable */

    bool is_hovered = is_point_in_rect(ui_ctx->input.mouse_x, ui_ctx->input.mouse_y,
                                       *x, menubar_header_y, w, menubar_header_h);

    if (is_active)       push_menubar_rect(cmd, (float)*x, (float)menubar_header_y, (float)w, (float)menubar_header_h, system_theme.menu_item_selected);
    else if (is_hovered) push_menubar_rect(cmd, (float)*x, (float)menubar_header_y, (float)w, (float)menubar_header_h, system_theme.menubar_button);

    draw_text_element(cmd, ui_ctx, label, *x + 8, menubar_header_y + (menubar_header_h - ui_font_h) / 2,
                      (is_active || is_hovered) ? system_theme.system_active_text : system_theme.system_text);
    *x += w + 2;
    return is_hovered && menubar_mouse_was_pressed;
}

/* read-only value shown in the dynamic zone (brush size, opacity). dropped when it does not fit, like a button. */
void draw_dynamic_zone_readout(VkCommandBuffer cmd, system_element_context* ui_ctx, const char* text, int* x, int x1)
{
    const int w = (int)strlen(text) * menubar_char_w;
    if (*x + w > x1) return;
    draw_text_element(cmd, ui_ctx, text, *x, menubar_header_y + (menubar_header_h - ui_font_h) / 2, get_theme_hint_color());
    *x += w + 12;
}

void draw_dynamic_zone(VkCommandBuffer cmd, system_element_context* ui_ctx, int x0, int x1)
{
    mode_state_sync_from_ui(ui_ctx);

    system_mode_context context = g_mode_state.context;
    int x = x0;

    const char* name = mode_context_name(context);
    int name_w = (int)strlen(name) * menubar_char_w;
    if (x + name_w > x1) return;
    draw_text_element(cmd, ui_ctx, name, x, menubar_header_y + (menubar_header_h - ui_font_h) / 2, system_theme.menu_active_text);
    x += name_w + 14;

    switch (context) {
    case mode_context_object:
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Select",    &x, x1, ui_ctx->active_tool == tool_select)) { mode_state_set_tool(ui_ctx, tool_select); set_menubar_status("Tool: Select."); }
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Translate", &x, x1, ui_ctx->active_tool == tool_translate)) { mode_state_set_tool(ui_ctx, tool_translate); set_menubar_status("Tool: Translate."); }
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Rotate",    &x, x1, ui_ctx->active_tool == tool_rotate)) { mode_state_set_tool(ui_ctx, tool_rotate); set_menubar_status("Tool: Rotate."); }
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Scale",     &x, x1, ui_ctx->active_tool == tool_scale)) { mode_state_set_tool(ui_ctx, tool_scale); set_menubar_status("Tool: Scale."); }
        break;

    case mode_context_edit:
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Vertex", &x, x1, ui_ctx->active_select_type == select_vert)) { ui_ctx->active_select_type = select_vert; g_mode_state.edit.select_type = select_vert; set_menubar_status("Select type: Vertex."); }
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Edge",   &x, x1, ui_ctx->active_select_type == select_edge)) { ui_ctx->active_select_type = select_edge; g_mode_state.edit.select_type = select_edge; set_menubar_status("Select type: Edge."); }
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Face",   &x, x1, ui_ctx->active_select_type == select_face)) { ui_ctx->active_select_type = select_face; g_mode_state.edit.select_type = select_face; set_menubar_status("Select type: Face."); }
        break;

    case mode_context_uv:
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Checker",    &x, x1, g_mode_state.uv.show_checker)) { g_mode_state.uv.show_checker = !g_mode_state.uv.show_checker; set_menubar_status("UV checker: %s.", g_mode_state.uv.show_checker ? "on" : "off"); }
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Pixel Snap", &x, x1, g_mode_state.uv.pixel_snap))   { g_mode_state.uv.pixel_snap = !g_mode_state.uv.pixel_snap; set_menubar_status("UV pixel snap: %s.", g_mode_state.uv.pixel_snap ? "on" : "off"); }
        break;

    case mode_context_sculpt: {
        const sculpt_brush_settings* brush = sculpt_current_settings();
        char readout[48];
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Brushes", &x, x1, false)) { brush_open_window(); set_menubar_status("Brushes window opened."); }
        snprintf(readout, sizeof(readout), "Radius %.2f", brush->radius);     draw_dynamic_zone_readout(cmd, ui_ctx, readout, &x, x1);
        snprintf(readout, sizeof(readout), "Strength %.2f", brush->strength); draw_dynamic_zone_readout(cmd, ui_ctx, readout, &x, x1);
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Symmetry X", &x, x1, g_mode_state.sculpt.symmetry_x)) { g_mode_state.sculpt.symmetry_x = !g_mode_state.sculpt.symmetry_x; set_menubar_status("Sculpt symmetry X: %s.", g_mode_state.sculpt.symmetry_x ? "on" : "off"); }
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Invert",     &x, x1, g_mode_state.sculpt.invert))     { g_mode_state.sculpt.invert = !g_mode_state.sculpt.invert; set_menubar_status("Sculpt invert: %s.", g_mode_state.sculpt.invert ? "on" : "off"); }
        break;
    }

    case mode_context_paint: {
        char readout[64];
        if (draw_dynamic_zone_button(cmd, ui_ctx, "Brushes", &x, x1, false)) { brush_open_window(); set_menubar_status("Brushes window opened."); }
        snprintf(readout, sizeof(readout), "Size %.2f", g_paint.radius);       draw_dynamic_zone_readout(cmd, ui_ctx, readout, &x, x1);
        snprintf(readout, sizeof(readout), "Opacity %.2f", g_paint.opacity);   draw_dynamic_zone_readout(cmd, ui_ctx, readout, &x, x1);
        if (x + 16 <= x1) {   /* current paint color */
            push_menubar_rect(cmd, (float)x, (float)(menubar_header_y + 3), 14.0f, (float)(menubar_header_h - 6),
                              (system_color){ g_paint.color[0], g_paint.color[1], g_paint.color[2], 1.0f });
            x += 20;
        }
        break;
    }

    default:
        break;   /* model / animation / texture shop: name only for now */
    }
}

/* ------------------------------------------------------------------------- */
/* the bar                                                                    */
/* ------------------------------------------------------------------------- */

void draw_system_menubar(VkCommandBuffer cmd, system_element_context* ui_ctx, editor_window_swapchain* swapchain)
{
    int w = (int)swapchain->extent.width;

    /* rising edge_to_face_array of the left mouse button, computed once per frame. */
    menubar_mouse_was_pressed = ui_ctx->input.mouse_pressed;
    if (menubar_mouse_was_pressed) {
        phoenix_trace("Menubar", "mouse press processed at (%d,%d)", ui_ctx->input.mouse_x, ui_ctx->input.mouse_y);
    }
    ui_ctx->input.mouse_pressed = false;
    previous_menubar_mouse_down = ui_ctx->input.mouse_down;

    const int mouse_x = ui_ctx->input.mouse_x;
    const int mouse_y = ui_ctx->input.mouse_y;
    bool click_used = false;                 /* this click was already used by something */

    /* ---- system_theme.menubar_background ---- */
    draw_menubar_element(cmd, ui_ctx, 0, menubar_y, w, menubar_h, system_theme.menubar_background);
    push_menubar_rect(cmd, 0.0f, (float)(menubar_y + menubar_h - 1), (float)w, 1.0f, system_theme.system_border);

    /* ================= static left: file edit element objects scripts ================= */
    int header_x[main_menu_count];
    int x = 8;
    for (int i = 0; i < main_menu_count; i++) {
        int header_w = (int)strlen(main_menus[i].title) * menubar_char_w + menubar_header_pad;
        header_x[i] = x;
        menubar_header_x[i] = x;

        bool is_hovered = is_point_in_rect(mouse_x, mouse_y, x, menubar_header_y, header_w, menubar_header_h);
        if (menubar_mouse_was_pressed && is_hovered) {
            phoenix_trace("Menubar", "opening %s at header x=%d y=%d", main_menus[i].title, x, menubar_header_y);
            if (menubar_open_menu_index == i) {
                menubar_open_menu_index = -1;
            } else {
                menubar_open_menu_index = i;
            }
            menubar_submenu_open = false;
            click_used = true;
        } else if (menubar_open_menu_index >= 0 && menubar_open_menu_index != i && is_hovered) {
            menubar_open_menu_index = i;             /* sliding across headers while one is open switches menus */
            menubar_submenu_open = false;
        }

        bool is_open = (menubar_open_menu_index == i);
        if (is_open)         push_menubar_rect(cmd, (float)x, (float)menubar_header_y, (float)header_w, (float)menubar_header_h, system_theme.menubar_background);
        else if (is_hovered) push_menubar_rect(cmd, (float)x, (float)menubar_header_y, (float)header_w, (float)menubar_header_h, system_theme.system_border);

        draw_text_element(cmd, ui_ctx, main_menus[i].title, x + menubar_header_pad / 2,
                          menubar_header_y + (menubar_header_h - ui_font_h) / 2,
                          (is_open || is_hovered) ? system_theme.system_active_text : system_theme.system_text);
        x += header_w;
    }

    /* ================= vertical separator 1 ================= */
    const int separator_1_x = x + 6;
    const int separator_y   = menubar_y + 5;
    const int separator_h   = menubar_h - 11;
    push_menubar_rect(cmd, (float)separator_1_x, (float)separator_y, 1.0f, (float)separator_h, system_theme.menu_vert_separator);

    /* ================= static right: minimize / maximize / close ================= */
    const int control_x0      = w - (menubar_control_w * menubar_control_count);
    const int separator_2_x   = control_x0 - 7;
    push_menubar_rect(cmd, (float)separator_2_x, (float)separator_y, 1.0f, (float)separator_h, system_theme.menu_vert_separator);

    for (int k = 0; k < menubar_control_count; k++) {
        int button_x = control_x0 + k * menubar_control_w;
        int button_h = menubar_h - 1;                                 /* stop above the border row */
        bool is_hovered = is_point_in_rect(mouse_x, mouse_y, button_x, menubar_y, menubar_control_w, button_h);

        if (is_hovered) {
            push_menubar_rect(cmd, (float)button_x, (float)menubar_y, (float)menubar_control_w, (float)button_h,
                                       (k == 2) ? system_theme.system_close_hover : system_theme.menubar_button);
        }

        int text_w = (int)strlen(menubar_control_labels[k]) * menubar_char_w;
        draw_text_element(cmd, ui_ctx, menubar_control_labels[k], button_x + (menubar_control_w - text_w) / 2,
                          menubar_y + (button_h - ui_font_h) / 2,
                          is_hovered ? system_theme.system_active_text : system_theme.system_text);

        if (is_hovered && menubar_mouse_was_pressed && !click_used) {
            click_used = true;
            menubar_open_menu_index = -1;
            HWND window = hwnd_window_00 ? hwnd_window_00 : GetActiveWindow();
            if (k == 0 && window)      ShowWindow(window, SW_MINIMIZE);
            else if (k == 1 && window) ShowWindow(window, IsZoomed(window) ? SW_RESTORE : SW_MAXIMIZE);
            else if (k == 2)           PostQuitMessage(0);
        }
    }

    /* ================= dynamic zone (the only part that varies) ================= */
    const int dynamic_x0 = separator_1_x + 10;
    const int dynamic_x1 = separator_2_x - 8;
    if (dynamic_x1 > dynamic_x0) draw_dynamic_zone(cmd, ui_ctx, dynamic_x0, dynamic_x1);
    menubar_click_used = click_used;
}

/* open dropdown: called from the overlay stage so the dock/toolbar can not cover it. */
void draw_menubar_dropdown(VkCommandBuffer cmd, system_element_context* ui_ctx)
{
    const int mouse_x = ui_ctx->input.mouse_x;
    const int mouse_y = ui_ctx->input.mouse_y;
    bool click_used = menubar_click_used;
    int* header_x = menubar_header_x;

    if (menubar_open_menu_index >= 0) {
        const struct_menu_def* menu = &main_menus[menubar_open_menu_index];
        int drop_x = header_x[menubar_open_menu_index];
        int drop_w = get_menu_width(menu);
        int drop_h = get_menu_height(menu);
        draw_dropdown_panel(cmd, drop_x, drop_w, drop_h);

        const struct_menu_item* clicked_item = NULL;
        int y = menu_drop_y + menu_pad_top;

        for (int i = 0; i < menu->item_count; i++) {
            const struct_menu_item* item = &menu->items[i];

            if (!item->label) {                                       /* separator row */
                push_menubar_rect(cmd, (float)(drop_x + 6), (float)(y + menubar_separator_h / 2),
                                           (float)(drop_w - 12), 1.0f, system_theme.menu_hort_separator);
                y += menubar_separator_h;
                continue;
            }

            bool is_hovered = is_point_in_rect(mouse_x, mouse_y, drop_x, y, drop_w, menu_item_h);
            if (is_hovered) {
                push_menubar_rect(cmd, (float)(drop_x + 2), (float)y, (float)(drop_w - 4), (float)menu_item_h,
                                           system_theme.menu_item_selected);
                if (menubar_mouse_was_pressed && !click_used) clicked_item = item;
            }

            system_color text_color = is_hovered ? system_theme.system_active_text
                                    : (item->implemented ? system_theme.menu_text : system_theme.menu_header_text);
            draw_text_element(cmd, ui_ctx, item->label, drop_x + 12, y + (menu_item_h - ui_font_h) / 2, text_color);
            if (item->action == menu_action_preferences_submenu) {
                draw_text_element(cmd, ui_ctx, ">", drop_x + drop_w - 16, y + (menu_item_h - ui_font_h) / 2, text_color);
                menubar_submenu_row_y = y;
            }
            y += menu_item_h;
        }

        const int sub_x = drop_x + drop_w - 2;
        const int sub_w = get_menu_width(&preferences_submenu);
        const int sub_h = get_menu_height(&preferences_submenu);
        const int sub_y = menubar_submenu_row_y - menu_pad_top;

        if (clicked_item && clicked_item->action == menu_action_preferences_submenu) {
            click_used = true;
            menubar_open_menu_index = -1;
            menubar_submenu_open = false;
            phoenix_info("Menubar", "selected %s", clicked_item->label);
            open_preferences_window();
            set_menubar_status("Preferences opened.");
        } else if (clicked_item) {
            click_used = true;
            menubar_open_menu_index = -1;
            menubar_submenu_open = false;
            phoenix_trace("Menubar", "selected %s", clicked_item->label);
            run_menu_action(clicked_item, ui_ctx);
        } else if (menubar_mouse_was_pressed && !click_used &&
                   !is_point_in_rect(mouse_x, mouse_y, drop_x, menu_drop_y, drop_w, drop_h) &&
                   !(menubar_submenu_open && is_point_in_rect(mouse_x, mouse_y, sub_x, sub_y, sub_w, sub_h))) {
            menubar_open_menu_index = -1;                                     /* click anywhere else closes it */
            menubar_submenu_open = false;
        }

        if (menubar_open_menu_index >= 0 && menubar_submenu_open) {
            draw_flyout_panel(cmd, sub_x, sub_y, sub_w, sub_h);
            int sub_item_y = sub_y + menu_pad_top;

            for (int i = 0; i < preferences_submenu.item_count; i++) {
                const struct_menu_item* sub_item = &preferences_submenu.items[i];
                bool sub_hovered = is_point_in_rect(mouse_x, mouse_y, sub_x, sub_item_y, sub_w, menu_item_h);
                if (sub_hovered) {
                    push_menubar_rect(cmd, (float)(sub_x + 2), (float)sub_item_y, (float)(sub_w - 4), (float)menu_item_h,
                                               system_theme.menu_item_selected);
                    if (menubar_mouse_was_pressed && !click_used) {
                        click_used = true;
                        menubar_open_menu_index = -1;
                        menubar_submenu_open = false;
                        phoenix_info("Menubar", "selected %s", sub_item->label);
                        run_menu_action(sub_item, ui_ctx);
                    }
                }
                system_color sub_text_color = system_theme.menu_text;
                if (sub_hovered) {
                    sub_text_color = system_theme.system_active_text;
                }
                draw_text_element(cmd, ui_ctx, sub_item->label, sub_x + 12, sub_item_y + (menu_item_h - ui_font_h) / 2,
                                  sub_text_color);
                sub_item_y += menu_item_h;
            }
        }
    }

}

/* called from editor_flush_ui, after toolbar/dock/statusbar/menubar have queued, so the window is on top of all of them. */
void draw_preferences_overlay(VkCommandBuffer cmd, system_element_context* ui_ctx, editor_window_swapchain* swapchain)
{
    draw_menubar_dropdown(cmd, ui_ctx);
    draw_preferences_window(cmd, ui_ctx, (int)swapchain->extent.width, (int)swapchain->extent.height);
    draw_export_window(cmd, ui_ctx, (int)swapchain->extent.width, (int)swapchain->extent.height);
}