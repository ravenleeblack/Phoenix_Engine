#include "system_globals.h"

/* Board builder. drawn by the World Builder Tools window when the Board button is picked.
   the four 4 mesh pieces, same calls as the toolbar. each one registers itself as a scene object. */
void draw_board_builder(system_element_context* ui_element, VkCommandBuffer cmd, int x, int y, int w, int h, bool edge_to_face_array)
{
    if (h < 40) return;
    (void)w;

    draw_text_element(cmd, ui_element, "Board (4 mesh pieces)", x, y, system_theme.system_text);

    static const struct { const char* label; const char* note; } k_piece[] = {
        { "Octagon", "main tile" }, { "Diamond", "fills gaps between octagons" },
        { "Half", "border piece" }, { "Quarter", "corner piece" },
    };
    const int bw = 80, bh = 24, gap = 5;
    int by = y + 20;

    for (int i = 0; i < 4; i++) {
        if (by + bh > y + h) break;
        const bool clicked = draw_button_element(cmd, ui_element, 95500u + (uint32_t)i, k_piece[i].label, x, by, bw, bh, system_theme.strip_button);
        const bool hovered = validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, x, by, bw, bh);
        draw_text_element(cmd, ui_element, k_piece[i].note, x + bw + 8, by + (bh - ui_font_h) / 2, system_theme.system_text);

        if (clicked || (hovered && edge_to_face_array)) {
            if      (i == 0) create_system_board_octagon(NULL);
            else if (i == 1) create_system_board_diamond(NULL);
            else if (i == 2) create_system_board_triangle_half(NULL);
            else             create_system_board_triangle_quarter(NULL);
        }
        by += bh + gap;
    }
}

/* Board Builder window (mode menu "Bd", or the Board button in World Builder Tools). */
void draw_board_builder_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Board Builder", node->x, content_y, node->w, content_h, system_theme.tab_content_window);

    static bool prev_down = false;                      /* the dock can swallow clicks: raw rising edge_to_face_array of the mouse */
    const bool edge_to_face_array = ui_ctx->input.mouse_down && !prev_down;
    prev_down = ui_ctx->input.mouse_down;

    draw_board_builder(ui_ctx, cmd, node->x + 10, content_y + 10, node->w - 20, content_h - 14, edge_to_face_array);
}