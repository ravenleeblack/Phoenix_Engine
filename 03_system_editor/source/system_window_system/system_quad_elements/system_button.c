#include "system_globals.h"


bool draw_button_element(VkCommandBuffer cmd, system_element_context* context, uint32_t id, const char* label, int x, int y, int w, int h, system_color base_color) 
{
    if (!context) return false;

    bool hovered = (context->input.mouse_x >= x && context->input.mouse_x <= (x + w) &&
                    context->input.mouse_y >= y && context->input.mouse_y <= (y + h));

    /* Only claim the mouse if nothing else owns it (stops drag-across-buttons from triggering them) */
    if (hovered) {
        context->hot_id = id;
        if (context->input.mouse_down && context->active_id == 0) {
            context->active_id = id;
        }
    } else if (context->hot_id == id) {
        context->hot_id = 0;
    }

    system_color final_color;
    if (hovered && context->active_id == id && context->input.mouse_down) {
        final_color = process_theme_color(base_color, system_theme.button_press_delta);
    } else if (hovered) {
        final_color = process_theme_color(base_color, system_theme.button_hover_delta);
    } else {
        final_color = process_theme_color(base_color, 0.0f);
    }

    push_rounded_quad(cmd, (float)x, (float)y, (float)w, (float)h, final_color, button_radius, corner_all);

    if (label && label[0]) {
        int text_w = (int)strlen(label) * ui_font_w;
        int text_x = x + (w - text_w) / 2;
        int text_y = y + (h - ui_font_h) / 2;
        draw_text_element(cmd, context, label, text_x, text_y, system_theme.button_text);
    }

    if (!context->input.mouse_down && context->active_id == id) 
    {
        context->active_id = 0;
        if (hovered) return true;
    }
    return false;
}