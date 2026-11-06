#include "system_globals.h"

/* ========================================================================= */
/* tabbed window = four parts, each with its own color in system_theme:       */
/*   tab_element         strip behind the tabs                                */
/*   tab_item            one tab (body, accent, label, close button)          */
/*   tab_sub_menubar     bar under the tabs                                   */
/*   tab_content_window  body behind the window contents                      */
/* strip window (mode menu) = strip_window_body + strip_grip, no tabs.        */
/* ========================================================================= */

static bool hit_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

/* ------------------------------------------------------------ tab_element */
void draw_tab_element(VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, int w, int h)
{
    if (!ui_element) return;

    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, system_theme.tab_element);
}

/* --------------------------------------------------------------- tab_item */
enum_tab_result draw_tab_item(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id, const char* label,
                              int x, int y, int w, int h, bool is_active, bool closable)
{
    if (!ui_element || !label) return tab_result_none;

    const bool hovered = hit_rect(ui_element->input.mouse_x, ui_element->input.mouse_y, x, y, w, h);

    system_color body = is_active ? system_theme.tab_item_active : system_theme.tab_item;
    if (hovered && !is_active) body = process_theme_color(body, system_theme.tab_hover_delta);

    /* body starts under the dock's 2px top border, 1px gap either side so the curves read,
       runs to the bottom of the strip with square bottom corners */
    const int tab_x = x + 1, tab_y = y + 2, tab_w = w - 2, tab_h = h - 2;
    push_rounded_quad(cmd, (float)tab_x, (float)tab_y, (float)tab_w, (float)tab_h, body, tab_radius, corner_top);

    draw_text_element(cmd, ui_element, label, x + tab_text_pad, y + ((h - ui_font_h) / 2),
                      is_active ? system_theme.tab_item_text_active : system_theme.tab_item_text);

    enum_tab_result result = tab_result_none;
    bool close_hovered = false;

    if (closable) {
        const int cx = x + w - tab_close_size - tab_close_margin;
        const int cy = y + ((h - tab_close_size) / 2);
        const uint32_t close_id = id + 5555;

        close_hovered = hit_rect(ui_element->input.mouse_x, ui_element->input.mouse_y, cx, cy, tab_close_size, tab_close_size);
        if (close_hovered)
            push_rounded_quad(cmd, (float)cx, (float)cy, (float)tab_close_size, (float)tab_close_size, system_theme.tab_close_hover, 3, corner_all);
        draw_text_element(cmd, ui_element, "x", cx + (tab_close_size - ui_font_w) / 2, cy + (tab_close_size - ui_font_h) / 2, system_theme.tab_close_text);

        if (ui_element->input.mouse_down && close_hovered && ui_element->active_id == 0) ui_element->active_id = close_id;
        if (!ui_element->input.mouse_down && ui_element->active_id == close_id) {
            ui_element->active_id = 0;
            if (close_hovered) result = tab_result_closed;
        }
    }

    if (ui_element->input.mouse_down && hovered && !close_hovered && ui_element->active_id == 0) ui_element->active_id = id;
    if (!ui_element->input.mouse_down && ui_element->active_id == id) {
        ui_element->active_id = 0;
        if (hovered && !close_hovered) result = tab_result_clicked;
    }
    return result;
}

/* -------------------------------------------------------- tab_sub_menubar */
/* h includes the 1px divider on the bottom row */
void draw_tab_sub_menubar(VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, int w, int h)
{
    if (!ui_element) return;

    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, system_theme.tab_sub_menubar);
    push_quad_element(cmd, (float)x, (float)(y + h - 1), (float)w, 1.0f, system_theme.tab_sub_menubar_divider);
}

/* ------------------------------------------------------ tab_content_window */
void draw_tab_content_window(VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, int w, int h)
{
    if (!ui_element || w <= 0 || h <= 0) return;
    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, system_theme.tab_content_window);
}

/* ------------------------------------------------------ strip window parts */
void draw_strip_window_body(VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, int w, int h)
{
    if (!ui_element) return;

    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, system_theme.strip_window_body);
}

/* the strip's only header control. same shape and height as a tab so the two window types line up.
   returns true while it is held down. the dock turns a held + moved grip into a dock drag. */
bool draw_strip_grip(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id, int x, int y, int w, int h)
{
    if (!ui_element) return false;

    const bool hovered = hit_rect(ui_element->input.mouse_x, ui_element->input.mouse_y, x, y, w, h);
    system_color body = system_theme.strip_grip;
    if (hovered) body = process_theme_color(body, system_theme.tab_hover_delta);

    push_rounded_quad(cmd, (float)(x + 1), (float)(y + 2), (float)(w - 2), (float)(h - 2), body, tab_radius, corner_top);

    /* three grip lines */
    const int line_w = (w - 2 > 16) ? 16 : w - 6;
    const int line_x = x + (w - line_w) / 2;
    const int line_y = y + 2 + ((h - 2) - 7) / 2;
    for (int i = 0; i < 3; i++)
        push_quad_element(cmd, (float)line_x, (float)(line_y + i * 3), (float)line_w, 1.0f, system_theme.strip_grip_lines);

    return ui_element->active_id == id && ui_element->input.mouse_down;
}