#include "system_globals.h"
#include <stdarg.h>
#include <stdio.h>

/* status text goes to the statusbar (draw_system_statusbar.c reads status_bar_text_buffer). */
static void set_scene_archive_status(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), format, args);
    va_end(args);
}

static bool s_click_edge = false;     /* left button went down this frame (set once per frame by the window) */

/* s_guide_continue[k] = the ancestor at indent level k has a next sibling. lets the tree lines of deeper rows keep going. */
static unsigned char s_guide_continue[tree_guide_levels];



void draw_scene_archive_node(VkCommandBuffer cmd, system_element_context* system_ui, system_scene_object* obj, int x, int* current_y, int window_w, int indent_level)
{
    if (!obj) return;

    int item_height = 20;
    int local_x = x + (indent_level * 16); 
    
    bool is_hovered = (system_ui->input.mouse_x >= x && system_ui->input.mouse_x < (x + window_w) &&
                        system_ui->input.mouse_y >= *current_y && system_ui->input.mouse_y < (*current_y + item_height));

    if (indent_level >= 0 && indent_level < tree_guide_levels)
        s_guide_continue[indent_level] = (obj->next_sibling != NULL) ? 1 : 0;

    /* right click on a row selects it first, so the right click menu acts on what you clicked */
    if (context_menu_right_click_in(x, *current_y, window_w, item_height))
        g_scene_registry.selected_object_id = obj->id;

    if (obj->id == g_scene_registry.selected_object_id) {
        push_quad_element(cmd, (float)x, (float)*current_y, (float)window_w, (float)item_height, system_theme.menu_item_selected);
    } else if (is_hovered) {
        push_quad_element(cmd, (float)x, (float)*current_y, (float)window_w, (float)item_height,
                          process_theme_color(system_theme.tab_content_window_sceneview, system_theme.system_hover_delta));
    }

    if (is_hovered && system_ui->input.mouse_down && system_ui->active_id == 0) {
        system_ui->active_id = obj->id;
        g_scene_registry.selected_object_id = obj->id; 
    }

    /* one text color for the row: readable on the accent (selected) and on the plain / hover background */
    const system_color row_text = (obj->id == g_scene_registry.selected_object_id) ? system_theme.system_active_text : system_theme.system_text;

    /* the little lines that tie this row to its parent */
    draw_tree_guide_lines(cmd, x, *current_y, item_height, indent_level, s_guide_continue,
                          obj->next_sibling != NULL, obj->first_child != NULL);

    if (obj->first_child != NULL) {
        bool arrow_hovered = (system_ui->input.mouse_x >= local_x && system_ui->input.mouse_x < (local_x + 12) &&
                              system_ui->input.mouse_y >= *current_y && system_ui->input.mouse_y < (*current_y + item_height));
        
        if (arrow_hovered && system_ui->input.mouse_down && system_ui->active_id == obj->id) {
            obj->is_open = !obj->is_open;
            system_ui->input.mouse_down = false; 
        }

        if (obj->is_open) {
            push_quad_element(cmd, (float)local_x + 2.0f, (float)*current_y + 9.0f, 6.0f, 2.0f, row_text);
        } else {
            push_quad_element(cmd, (float)local_x + 4.0f, (float)*current_y + 6.0f, 2.0f, 6.0f, row_text);
        }
    }

    system_color label_color = row_text;
    draw_text_element(cmd, system_ui, obj->name, local_x + 16, *current_y + 4, label_color);

    /* region_xx rows: chunk face_type, On / Off, Solo. Off = the region is not meshed at all (big speed win). */
    {
        const int reg = get_terrain_region_of_object(obj->id);
        if (reg >= 0) {
            const bool on = is_terrain_region_enabled(reg);
            const int bw = 36, by = *current_y + 2, bx = x + window_w - bw * 2 - 8;
            const int mx = system_ui->input.mouse_x, my = system_ui->input.mouse_y;
            const bool h_on   = validate_rectangle_hit(mx, my, bx, by, bw, 16);
            const bool h_solo = validate_rectangle_hit(mx, my, bx + bw + 4, by, bw, 16);

            char info[32];
            snprintf(info, sizeof(info), "%s%d ch", reg == get_terrain_camera_region() ? "here  " : "", g_terrain.region_resident[reg]);
            draw_text_element(cmd, system_ui, info, bx - (int)strlen(info) * 8 - 6, *current_y + 4, get_theme_hint_color());

            push_quad_element(cmd, (float)bx, (float)by, (float)bw, 16.0f, on ? system_theme.strip_button_selected : (h_on ? system_theme.tab_item : system_theme.strip_button));
            draw_text_element(cmd, system_ui, on ? "On" : "Off", bx + 8, by + 2, system_theme.button_text);
            push_quad_element(cmd, (float)(bx + bw + 4), (float)by, (float)bw, 16.0f, h_solo ? system_theme.tab_item : system_theme.strip_button);
            draw_text_element(cmd, system_ui, "Solo", bx + bw + 8, by + 2, system_theme.button_text);

            if (s_click_edge && h_on)   { set_terrain_region_enabled(reg, !on); set_scene_archive_status("%s: %s.", obj->name, on ? "off (not meshed)" : "on"); }
            if (s_click_edge && h_solo) { solo_terrain_region(reg);             set_scene_archive_status("%s: solo.", obj->name); }
        }
    }

    *current_y += item_height;

    if (obj->is_open && obj->first_child != NULL) {
        draw_scene_archive_node(cmd, system_ui, obj->first_child, x, current_y, window_w, indent_level + 1);
    }

    if (obj->next_sibling != NULL) {
        draw_scene_archive_node(cmd, system_ui, obj->next_sibling, x, current_y, window_w, indent_level);
    }
}

void draw_scene_archive_window(system_element_context* system_ui, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, system_ui, "Scene Archive", node->x, content_y, node->w, content_h, system_theme.tab_content_window_sceneview);

    if (g_scene_registry.object_count == 0) {
        start_scene_registry();
    }

    /* right click menu for this window (drawn on top later by draw_context_menu) */
    context_menu_hierarchy_window(system_ui, node->x, content_y, node->w, content_h);

    static bool prev_down = false;
    s_click_edge = system_ui->input.mouse_down && !prev_down;
    prev_down = system_ui->input.mouse_down;

    int current_y_row = content_y + 30; 
    int start_x_offset = node->x + 10;

    if (!system_ui->input.mouse_down && system_ui->active_id != 0) {
        if (system_ui->active_id >= 1000 && system_ui->active_id < 1300) {
            system_ui->active_id = 0;
        }
    }

    draw_scene_archive_node(cmd, system_ui, &g_scene_registry.objects[0], start_x_offset, &current_y_row, node->w - 20, 0);
}