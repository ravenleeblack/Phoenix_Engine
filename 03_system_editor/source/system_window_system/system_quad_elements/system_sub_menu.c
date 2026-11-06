#include "system_globals.h"

bool draw_menu_item_element(VkCommandBuffer cmd, system_element_context* context, uint32_t id, const char* label, int x, int y, int w, int h, system_color base_color) {
    if (!context) return false;

    bool hovered = (context->input.mouse_x >= x && context->input.mouse_x <= (x + w) &&
                    context->input.mouse_y >= y && context->input.mouse_y <= (y + h));
                    
    system_color final_color;

    if (hovered) {
        context->hot_id = id;
        final_color = process_theme_color(base_color, system_theme.menu_hover_delta);
    } else {
        if (context->hot_id == id) context->hot_id = 0;
        final_color = process_theme_color(base_color, 0.0f);
    }

    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, final_color);
    
    // FIX: Forwarded cmd context parameter natively to match the updated text layout
    draw_text_element(cmd, context, label, x + 12, y + 4, system_theme.menu_text);

    if (context->input.mouse_down && hovered && context->active_id == 0) context->active_id = id;
    if (!context->input.mouse_down && context->active_id == id) {
        context->active_id = 0;
        return true;
    }
    return false;
}

static uint32_t g_active_open_menu_id = 0;

bool draw_main_menu_header_button(VkCommandBuffer cmd, system_element_context* context, uint32_t id, const char* label, int x, int y, int w, int h, system_color base_color) {
    if (!context) return false;

    bool hovered = (context->input.mouse_x >= x && context->input.mouse_x <= (x + w) &&
                    context->input.mouse_y >= y && context->input.mouse_y <= (y + h));
                    
    bool is_open = (g_active_open_menu_id == id);
    system_color final_color;

    if (hovered || is_open) {
        final_color = process_theme_color(base_color, system_theme.menu_header_hover_delta);
    } else {
        final_color = process_theme_color(base_color, 0.0f);
    }

    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, final_color);
    
    // FIX: Forwarded cmd context parameter natively to match the updated text layout
    draw_text_element(cmd, context, label, x + 10, y + 6, system_theme.menu_header_text);

    if (context->input.mouse_down && hovered && context->active_id == 0) {
        context->active_id = id;
        g_active_open_menu_id = is_open ? 0 : id;
    }
    if (hovered && g_active_open_menu_id != 0 && g_active_open_menu_id != id) g_active_open_menu_id = id;
    if (!context->input.mouse_down && context->active_id == id) context->active_id = 0;

    return is_open;
}

void close_all_system_menus(void) { g_active_open_menu_id = 0; }
uint32_t get_active_open_menu_id(void) { return g_active_open_menu_id; }