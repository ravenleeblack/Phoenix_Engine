#include "system_globals.h"
#include <stdarg.h>
#include <stdio.h>

/* status text goes to the statusbar (draw_system_statusbar.c reads status_bar_text_buffer). */
static void set_file_archive_status(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), format, args);
    va_end(args);
}

/* ========================================================================= */
/* tree guide lines (shared with the scene archive, declared in system_file_archive.h) */
/*                                                                           */
/*   parent                                                                  */
/*   |-- child        vertical line down the parent's column, a tick into    */
/*   |-- child        each row. the last child's line stops at its tick.     */
/*   '-- child        ancestors that still have siblings keep their line.     */
/* ========================================================================= */
void draw_tree_guide_lines(VkCommandBuffer cmd, int x, int row_y, int row_h, int level,
                           const unsigned char* ancestor_continues, bool has_next_sibling, bool has_children)
{
    if (level <= 0) return;                                  /* the root has no parent to connect to */

    const system_color c   = system_theme.menu_separator;
    const int          mid = row_y + row_h / 2;

    /* lines of the ancestors that continue past this row */
    for (int k = 1; k < level && k < tree_guide_levels; k++) {
        if (ancestor_continues[k])
            push_quad_element(cmd, (float)(x + (k - 1) * tree_indent + 5), (float)row_y, 1.0f, (float)row_h, c);
    }

    /* this row's own connector: vertical (full height if more siblings follow, else down to the tick) + horizontal tick */
    const int lx     = x + (level - 1) * tree_indent + 5;
    const int bottom = has_next_sibling ? row_h : (row_h / 2 + 1);
    push_quad_element(cmd, (float)lx, (float)row_y, 1.0f, (float)bottom, c);

    const int tick_end = x + level * tree_indent + (has_children ? 1 : 11);   /* stop short of the +/- mark or the icon */
    if (tick_end > lx) push_quad_element(cmd, (float)lx, (float)mid, (float)(tick_end - lx), 1.0f, c);
}

/* ========================================================================= */
/* file archive window                                                        */
/* ========================================================================= */
int file_archive_take_wheel(void);          /* system_file_archive.c */

enum { fa_row_h = 20, fa_top_gap = 30, fa_wheel_rows = 3 };

static bool          s_click_edge = false;                    /* left button went down this frame */
static int           s_scroll_px  = 0;
static unsigned char s_continue[tree_guide_levels];           /* s_continue[k] = the ancestor at level k has a next sibling */

static bool in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < (x + w) && py >= y && py < (y + h);
}

static int count_rows(int i)
{
    int rows = 0;
    while (i >= 0) {
        const file_archive_node* n = file_archive_node_at(i);
        if (!n) break;
        rows++;
        if (n->kind == file_node_folder && n->is_open && n->first_child >= 0) rows += count_rows(n->first_child);
        i = n->next_sibling;
    }
    return rows;
}

/* a folder: body + the little tab on top. a file: a page. 12x10 area at (x, y). */
static void draw_node_icon(VkCommandBuffer cmd, int x, int y, bool folder)
{
    if (folder) {
        push_quad_element(cmd, (float)x,       (float)(y + 1), 5.0f,  2.0f, system_theme.menu_active_text);
        push_quad_element(cmd, (float)x,       (float)(y + 3), 12.0f, 7.0f, system_theme.menu_active_text);
    } else {
        push_quad_element(cmd, (float)(x + 2), (float)y,       8.0f,  10.0f, system_theme.system_text);
        push_quad_element(cmd, (float)(x + 4), (float)(y + 3), 4.0f,  1.0f,  system_theme.tab_content_window_fileview);
        push_quad_element(cmd, (float)(x + 4), (float)(y + 6), 4.0f,  1.0f,  system_theme.tab_content_window_fileview);
    }
}

/* draws one level of siblings (and, for open folders, everything below). rows outside [view_top, view_bottom) are skipped. */
static void draw_nodes(VkCommandBuffer cmd, system_element_context* ui_element, int first, int x, int* y, int w,
                       int level, int view_top, int view_bottom)
{
    for (int i = first; i >= 0; ) {
        const file_archive_node* n = file_archive_node_at(i);
        if (!n) return;

        const bool is_folder  = (n->kind == file_node_folder);
        const bool has_next   = (n->next_sibling >= 0);
        const bool has_kids   = is_folder && n->first_child >= 0;
        const int  row_y      = *y;
        const int  local_x    = x + level * tree_indent;
        const bool visible    = (row_y >= view_top) && (row_y + fa_row_h <= view_bottom);

        if (level < tree_guide_levels) s_continue[level] = has_next ? 1 : 0;

        if (visible) {
            const int  mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
            const bool hovered = in_rect(mx, my, x, row_y, w, fa_row_h);
            const bool obj_vert_selected = (file_archive_selected() == i);

            /* a right click on a row selects it first, so the right click menu acts on what you clicked */
            if (context_menu_right_click_in(x, row_y, w, fa_row_h)) file_archive_select(i);

            if (hovered && s_click_edge && ui_element->active_id == 0) {
                const bool on_arrow = has_kids && mx >= local_x && mx < (local_x + 12);
                if (on_arrow) {
                    set_file_archive_status("%s folder: %s.", n->is_open ? "Collapsed" : "Expanded", n->name);
                    file_archive_toggle_open(i);
                } else {
                    file_archive_select(i);
                    set_file_archive_status("Selected %s: %s.", is_folder ? "folder" : "file", n->name);
                }
            }

            if (file_archive_selected() == i)
                push_quad_element(cmd, (float)x, (float)row_y, (float)w, (float)fa_row_h, system_theme.menu_item_selected);
            else if (hovered)
                push_quad_element(cmd, (float)x, (float)row_y, (float)w, (float)fa_row_h,
                                  process_theme_color(system_theme.tab_content_window_fileview, system_theme.system_hover_delta));
            (void)obj_vert_selected;

            /* one text color for the row: readable on the accent (selected) and on the plain / hover background */
            const system_color row_text = (file_archive_selected() == i) ? system_theme.system_active_text : system_theme.system_text;

            draw_tree_guide_lines(cmd, x, row_y, fa_row_h, level, s_continue, has_next, has_kids);

            if (has_kids) {
                if (n->is_open) push_quad_element(cmd, (float)local_x + 2.0f, (float)row_y + 9.0f, 6.0f, 2.0f, row_text);
                else            push_quad_element(cmd, (float)local_x + 4.0f, (float)row_y + 6.0f, 2.0f, 6.0f, row_text);
            }

            draw_node_icon(cmd, local_x + 16, row_y + 5, is_folder);

            /* cut the label so it never runs out of the window */
            char label[file_archive_name_max];
            snprintf(label, sizeof(label), "%s", n->name);
            const int room  = (x + w) - (local_x + 32) - 4;
            const int chars = room > 0 ? room / ui_font_w : 0;
            if (chars < (int)strlen(label)) {
                if (chars > 2) { label[chars - 2] = '.'; label[chars - 1] = '.'; label[chars] = 0; }
                else           label[chars > 0 ? chars : 0] = 0;
            }

            const system_color label_color = row_text;
            draw_text_element(cmd, ui_element, label, local_x + 32, row_y + 4, label_color);
        }

        *y += fa_row_h;

        if (is_folder && n->is_open && n->first_child >= 0)
            draw_nodes(cmd, ui_element, n->first_child, x, y, w, level + 1, view_top, view_bottom);

        i = n->next_sibling;
    }
}

void draw_file_archive_window(system_element_context* system_ui, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    file_archive_start();                                   /* first call scans the folder, later calls do nothing */

    draw_window_element(cmd, system_ui, "File Archive", node->x, content_y, node->w, content_h, system_theme.tab_content_window_fileview);

    /* right click menu for this window (drawn on top later by draw_context_menu) */
    context_menu_files_window(system_ui, node->x, content_y, node->w, content_h);

    static bool prev_down = false;
    s_click_edge = system_ui->input.mouse_down && !prev_down;
    prev_down    = system_ui->input.mouse_down;

    const int view_top    = content_y + fa_top_gap;
    const int view_bottom = content_y + content_h;
    const int row_x       = node->x + 10;
    const int row_w       = node->w - 20;

    /* wheel scrolling: only when the mouse is over this window. the pending wheel is cleared either way. */
    const int wheel = file_archive_take_wheel();
    if (wheel != 0 && in_rect(system_ui->input.mouse_x, system_ui->input.mouse_y, node->x, content_y, node->w, content_h))
        s_scroll_px -= (wheel / 120) * fa_wheel_rows * fa_row_h;

    const int total_h  = count_rows(0) * fa_row_h;
    const int max_scroll = total_h > (view_bottom - view_top) ? total_h - (view_bottom - view_top) : 0;
    if (s_scroll_px > max_scroll) s_scroll_px = max_scroll;
    if (s_scroll_px < 0)          s_scroll_px = 0;
    s_scroll_px -= s_scroll_px % fa_row_h;                  /* whole rows only, so no row is ever cut in half */

    int y = view_top - s_scroll_px;
    draw_nodes(cmd, system_ui, 0, row_x, &y, row_w, 0, view_top, view_bottom);

    const file_archive_node* root = file_archive_node_at(0);
    if (root && root->first_child < 0 && root->is_open)
        draw_text_element(cmd, system_ui, "Empty. Right click to add.", row_x + 32, view_top + fa_row_h + 4, get_theme_hint_color());
}