#include "system_globals.h"

/* tooltips, see system_tooltip.h */

void set_tooltip_text(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return;
    }
    snprintf(g_tooltip.pending, sizeof(g_tooltip.pending), "%s", text);
}

void draw_tooltip_overlay(VkCommandBuffer cmd, system_element_context* ui_element, int screen_w, int screen_h)
{
    if (g_tooltip.pending[0] == '\0') {
        g_tooltip.shown[0] = '\0';
        g_tooltip.hover_frames = 0;
        return;
    }

    if (strcmp(g_tooltip.pending, g_tooltip.shown) != 0) {
        snprintf(g_tooltip.shown, sizeof(g_tooltip.shown), "%s", g_tooltip.pending);
        g_tooltip.hover_frames = 0;
    } else {
        g_tooltip.hover_frames++;
    }
    g_tooltip.pending[0] = '\0';

    if (g_tooltip.hover_frames < tooltip_delay_frames || ui_element->input.mouse_down) {
        return;
    }

    const int pad_x = 6;
    const int pad_y = 4;
    const int box_w = (int)strlen(g_tooltip.shown) * ui_font_w + pad_x * 2;
    const int box_h = ui_font_h + pad_y * 2;

    int box_x = ui_element->input.mouse_x + 14;
    int box_y = ui_element->input.mouse_y + 20;
    if (box_x + box_w > screen_w - 4) {
        box_x = screen_w - box_w - 4;
    }
    if (box_x < 4) {
        box_x = 4;
    }
    if (box_y + box_h > screen_h - 4) {
        box_y = ui_element->input.mouse_y - box_h - 8;
    }

    push_rounded_quad(cmd, (float)(box_x - 1), (float)(box_y - 1), (float)(box_w + 2), (float)(box_h + 2), system_theme.context_menu_border, 5, corner_all);
    push_rounded_quad(cmd, (float)box_x, (float)box_y, (float)box_w, (float)box_h, system_theme.context_menu_dropdown, 4, corner_all);
    draw_text_element(cmd, ui_element, g_tooltip.shown, box_x + pad_x, box_y + pad_y, system_theme.system_text);
}
