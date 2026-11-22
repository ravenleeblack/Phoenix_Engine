#include "system_globals.h"

/* Terrain Tools window: one 64 x 64 square icon per tool, a title above each. Picking one opens its steps in the Modeler window (draw_system_modeler.c)
   and starts the wireframe preview in the Scene view (system_terrain_tools.c). Opens next to the File Archive from the Tt button of the mode menu. */

enum { tt_tile = 64, tt_title_h = 14, tt_gap = 10, tt_pad = 12, tt_glyph_cell = 5 };

void mode_menu_open_terrain_windows(system_element_context* ui_element);

const char* const k_tt_glyphs[tt_tool_count][9] = {
    { "", "", "", "", "", "", "", "", "" },
    { ".........", "..XX.....", "...XX....", "....XX...", "...XX....", "..XX.....", "...XX....", "....XX...", "........." },                  /* river    */
    { "..XXXXX..", ".XX...XX.", "XX.....XX", "X.......X", "X.......X", "X.......X", "X.......X", "X.......X", "XXXXXXXXX" },              /* tunnel   */
    { "....X....", "...XXX...", "...XXX...", "..XXXXX..", "..XXXXX..", ".XXXXXXX.", ".XXXXXXX.", "XXXXXXXXX", "........." },              /* mountain */
    { ".........", "..XXXXX..", ".XXXXXXX.", "XXXXXXXXX", "XXXXXXXXX", "XXXXXXXXX", ".XXXXXXX.", "..XXXXX..", "........." },              /* lake     */
    { "........X", ".......XX", "......XXX", ".....XXXX", "....XXXXX", "...XXXXXX", "..XXXXXXX", ".XXXXXXXX", "XXXXXXXXX" },              /* ramp     */
    { "XXXXXXXXX", "XXXXXXXXX", "XXXXXXXX.", "XXXXXX...", "XXXX.....", "XX.......", "X........", "X........", "X........" },              /* overhang */
    { "XX.....XX", "XXX...XXX", "XXX...XXX", "XXXX.XXXX", "XXXX.XXXX", "XXXXXXXXX", "XXXXXXXXX", "XXXXXXXXX", "XXXXXXXXX" }               /* canyon   */
};

const float k_tt_colors[tt_tool_count][3] = {
    { 0.0f, 0.0f, 0.0f },
    { 0.25f, 0.50f, 0.85f },
    { 0.45f, 0.40f, 0.38f },
    { 0.55f, 0.50f, 0.42f },
    { 0.20f, 0.62f, 0.72f },
    { 0.80f, 0.60f, 0.25f },
    { 0.62f, 0.55f, 0.50f },
    { 0.75f, 0.38f, 0.25f }
};

const char* const k_tt_tips[tt_tool_count] = {
    "",
    "River Tool: a curved channel between two picks.",
    "Tunnel Tool: a tube under the ground between two picks.",
    "Mountain Tool: a hill with a shaped side and ridges.",
    "Lake Tool: a round basin with a rim.",
    "Ramp Tool: a sloped path that joins two heights.",
    "Overhang Tool: a ledge that hangs over the ground (its own object).",
    "Canyon Tool: a deep cut with steps along its sides."
};

void draw_terrain_tools_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Terrain Tools", node->x, content_y, node->w, content_h, system_theme.tab_content_window_modeler);

    static bool prev_down = false;
    const bool press = ui_ctx->input.mouse_down && !prev_down;
    prev_down = ui_ctx->input.mouse_down;

    draw_text_element(cmd, ui_ctx, "Terrain Tools", node->x + tt_pad, content_y + tt_pad, system_theme.menu_active_text);
    draw_text_element(cmd, ui_ctx, "Pick a tool. Its steps open in the Modeler window.", node->x + tt_pad, content_y + tt_pad + 16, get_theme_hint_color());

    int per_row = (node->w - tt_pad * 2 + tt_gap) / (tt_tile + tt_gap);
    if (per_row < 1) {
        per_row = 1;
    }
    const int top = content_y + tt_pad + 44;
    const int row_h = tt_title_h + tt_tile + tt_gap + 4;

    for (int i = 1; i < tt_tool_count; i++) {
        const int slot = i - 1;
        const int x = node->x + tt_pad + (slot % per_row) * (tt_tile + tt_gap);
        const int y = top + (slot / per_row) * row_h;
        if (y + row_h > content_y + content_h) {
            break;
        }

        const bool selected = (g_terrain_tools.tool == i);
        const bool hovered = validate_rectangle_hit(ui_ctx->input.mouse_x, ui_ctx->input.mouse_y, x, y, tt_tile, tt_title_h + tt_tile);

        char title[24];
        snprintf(title, sizeof(title), "%s", get_terrain_tools_name(i));
        char* tool_word = strstr(title, " Tool");
        if (tool_word) {
            *tool_word = '\0';
        }
        const int title_x = x + (tt_tile - (int)strlen(title) * ui_font_w) / 2;
        system_color title_color = system_theme.system_text;
        if (selected) {
            title_color = system_theme.menu_active_text;
        }
        draw_text_element(cmd, ui_ctx, title, title_x, y, title_color);

        const int tile_y = y + tt_title_h;
        system_color back = { k_tt_colors[i][0] * 0.55f, k_tt_colors[i][1] * 0.55f, k_tt_colors[i][2] * 0.55f, 1.0f };
        if (hovered) {
            back.red += 0.12f; back.green += 0.12f; back.blue += 0.12f;
        }
        if (selected) {
            push_rounded_quad(cmd, (float)(x - 3), (float)(tile_y - 3), (float)(tt_tile + 6), (float)(tt_tile + 6), system_theme.system_edit_mode_active_face_hover, 10, corner_all);
        }
        push_rounded_quad(cmd, (float)x, (float)tile_y, (float)tt_tile, (float)tt_tile, back, 8, corner_all);

        const system_color glyph = { k_tt_colors[i][0] + 0.25f, k_tt_colors[i][1] + 0.25f, k_tt_colors[i][2] + 0.25f, 1.0f };
        const int glyph_x = x + (tt_tile - 9 * tt_glyph_cell) / 2;
        const int glyph_y = tile_y + (tt_tile - 9 * tt_glyph_cell) / 2;
        for (int row = 0; row < 9; row++) {
            for (int column = 0; column < 9; column++) {
                if (k_tt_glyphs[i][row][column] == 'X') {
                    draw_icon_element(cmd, glyph_x + column * tt_glyph_cell, glyph_y + row * tt_glyph_cell, tt_glyph_cell, tt_glyph_cell, glyph);
                }
            }
        }

        if (hovered) {
            set_tooltip_text(k_tt_tips[i]);
            if (press) {
                if (selected) {
                    close_terrain_tool();
                } else {
                    select_terrain_tool(i);
                    mode_menu_open_terrain_windows(ui_ctx);
                    enter_terrain_tools_edit_mode(ui_ctx);        /* Edit mode, edge select on the terrain: step 1 picks edges */
                }
            }
        }
    }

    char status[96];
    if (g_terrain_tools.tool == tt_tool_none) {
        snprintf(status, sizeof(status), "No tool picked.");
    } else {
        snprintf(status, sizeof(status), "%s: step %d of %d, %s", get_terrain_tools_name(g_terrain_tools.tool), g_terrain_tools.step, tt_step_count,
                 get_terrain_tools_step_name(g_terrain_tools.tool, g_terrain_tools.step));
    }
    draw_text_element(cmd, ui_ctx, status, node->x + tt_pad, content_y + content_h - 22, system_theme.system_color_edit_mode_preview_color);
}
