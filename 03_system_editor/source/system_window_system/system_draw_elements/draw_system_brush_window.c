#include "system_globals.h"

/* Brushes window.
     SCULPT   the nine sculpt brushes. Picking one sets the brush (its settings are in the Sculpt window) and turns Sculpt mode on.
     ------   separator
     PAINT    the paint brushes. Picking one sets the paint brush and turns Paint mode on; the paint tools read it with brush_window_get_paint_brush().
              Pressing Pt in the mode menu / Paint window opens this window too (brush_follow_modes).
   Every button is a 64 x 64 square tile (ZBrush style): coloured strip on top, brush dab in the middle, name at the bottom.
   opens next to the Hierarchy (scene archive) with the Sc button. */
enum { br_tile = 64, br_pad = 10, br_gap = 6, br_strip_h = 4, br_dab = 26 };

/* ------------------------------------------------------------------------- */
/* paint brushes                                                              */
/* ------------------------------------------------------------------------- */
typedef enum {
    brush_paint_standard = 0,
    brush_paint_airbrush,
    brush_paint_eraser,
    brush_paint_smudge,
    brush_paint_blur,
    brush_paint_fill,
    brush_paint_picker,
    brush_paint_lighten,
    brush_paint_darken,
    brush_paint_count
} brush_paint_kind;

static const struct { const char* name; float r, g, b; } k_paint_brushes[brush_paint_count] = {
    { "Paint",    0.90f, 0.35f, 0.30f },
    { "Airbrush", 0.95f, 0.60f, 0.25f },
    { "Eraser",   0.85f, 0.85f, 0.85f },
    { "Smudge",   0.60f, 0.45f, 0.80f },
    { "Blur",     0.40f, 0.65f, 0.90f },
    { "Fill",     0.35f, 0.75f, 0.45f },
    { "Picker",   0.95f, 0.85f, 0.30f },
    { "Lighten",  1.00f, 1.00f, 0.80f },
    { "Darken",   0.30f, 0.30f, 0.38f }
};

static int s_paint_brush = brush_paint_standard;

/* The paint tools read the chosen brush through these. The number is the order of brush_paint_kind above
   (0 Paint, 1 Airbrush, 2 Eraser, 3 Smudge, 4 Blur, 5 Fill, 6 Picker, 7 Lighten, 8 Darken). */
int  brush_window_get_paint_brush(void)      { return s_paint_brush; }
void brush_window_set_paint_brush(int brush) { if (brush >= 0 && brush < brush_paint_count) s_paint_brush = brush; }
const char* brush_window_paint_brush_name(int brush)
{
    return (brush >= 0 && brush < brush_paint_count) ? k_paint_brushes[brush].name : "?";
}

/* ------------------------------------------------------------------------- */
/* drawing helpers                                                            */
/* ------------------------------------------------------------------------- */
static void section_header(VkCommandBuffer cmd, system_element_context* ui_element, const char* title, int hx, int hy, int hw)
{
    draw_text_element(cmd, ui_element, title, hx, hy, system_theme.system_text);
    push_quad_element(cmd, (float)hx, (float)(hy + ui_font_h + 4), (float)hw, 1.0f, system_theme.system_border);
}

/* One square tile. Returns true on the frame it is clicked. */
static bool brush_tile(VkCommandBuffer cmd, system_element_context* ui_element, bool edge_to_face_array,
                       int tx, int ty, const char* label, bool obj_vert_selected, system_color strip, system_color dab)
{
    const bool hovered = validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, tx, ty, br_tile, br_tile);

    push_quad_element(cmd, (float)tx, (float)ty, (float)br_tile, (float)br_tile,
                      obj_vert_selected ? system_theme.strip_button_selected : (hovered ? system_theme.tab_item : system_theme.strip_button));
    push_quad_element(cmd, (float)tx, (float)ty, (float)br_tile, (float)br_strip_h, strip);

    /* brush dab: a round blob centred in the free area above the name */
    const int area_top = ty + br_strip_h;
    const int area_h   = br_tile - br_strip_h - ui_font_h - 8;
    push_rounded_quad(cmd, (float)(tx + (br_tile - br_dab) / 2), (float)(area_top + (area_h - br_dab) / 2),
                      (float)br_dab, (float)br_dab, dab, br_dab / 2, corner_all);

    /* name, centred and cut so it never leaves the tile */
    char name[24];
    int chars = (ui_font_w > 0) ? (br_tile - 4) / ui_font_w : 0;
    if (chars < 0) chars = 0;
    if (chars > (int)sizeof(name) - 1) chars = (int)sizeof(name) - 1;
    snprintf(name, sizeof(name), "%.*s", chars, label);
    int nx = tx + (br_tile - (int)strlen(name) * ui_font_w) / 2;
    if (nx < tx + 2) nx = tx + 2;
    draw_text_element(cmd, ui_element, name, nx, ty + br_tile - ui_font_h - 5, system_theme.button_text);

    if (obj_vert_selected) draw_full_border(cmd, ui_element, tx, ty, br_tile, br_tile, 2, system_color_pure_white);
    return hovered && edge_to_face_array;
}

/* ------------------------------------------------------------------------- */
/* window                                                                     */
/* ------------------------------------------------------------------------- */
void draw_system_brush_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Brushes", node->x, content_y, node->w, content_h, system_theme.tab_content_window);

    static bool prev_down = false;
    const bool edge_to_face_array = ui_ctx->input.mouse_down && !prev_down;
    prev_down = ui_ctx->input.mouse_down;

    start_sculpt_mode();
    const int limit   = content_y + content_h;
    const int left    = node->x + br_pad;
    const int inner_w = node->w - br_pad * 2;

    int per_row = (inner_w + br_gap) / (br_tile + br_gap);
    if (per_row < 1) per_row = 1;

    int cursor = content_y + br_pad;

    /* ---- SCULPT ---- */
    if (cursor + ui_font_h + 6 > limit) return;
    section_header(cmd, ui_ctx, "SCULPT", left, cursor, inner_w);
    cursor += ui_font_h + 10;

    for (int slot = 0; slot < sculpt_brush_count; slot++) {
        const enum_sculpt_brush brush = (enum_sculpt_brush)slot;
        const int tx = left + (slot % per_row) * (br_tile + br_gap);
        const int ty = cursor + (slot / per_row) * (br_tile + br_gap);
        if (ty + br_tile > limit) continue;
        if (tx + br_tile > node->x + node->w) continue;

        const bool obj_vert_selected = (sculpt_get_brush() == brush);
        if (brush_tile(cmd, ui_ctx, edge_to_face_array, tx, ty, sculpt_brush_name(brush), obj_vert_selected,
                       system_theme.system_accent, system_color_near_white)) {
            sculpt_set_brush(brush);
            if (!sculpt_mode_is_active()) mode_menu_enter_mode(ui_ctx, mode_context_sculpt, false);   /* a brush needs Sculpt mode */
        }
    }
    cursor += ((sculpt_brush_count + per_row - 1) / per_row) * (br_tile + br_gap);

    /* ---- separator ---- */
    cursor += 2;
    if (cursor + 2 > limit) return;
    push_quad_element(cmd, (float)left, (float)cursor, (float)inner_w, 2.0f, system_theme.system_border);
    cursor += 12;

    /* ---- PAINT ---- */
    if (cursor + ui_font_h + 6 > limit) return;
    section_header(cmd, ui_ctx, "PAINT", left, cursor, inner_w);
    cursor += ui_font_h + 10;

    for (int slot = 0; slot < brush_paint_count; slot++) {
        const int tx = left + (slot % per_row) * (br_tile + br_gap);
        const int ty = cursor + (slot / per_row) * (br_tile + br_gap);
        if (ty + br_tile > limit) continue;
        if (tx + br_tile > node->x + node->w) continue;

        const system_color dab = { .red = k_paint_brushes[slot].r, .green = k_paint_brushes[slot].g,
                                   .blue = k_paint_brushes[slot].b, .alpha = 1.0f };
        if (brush_tile(cmd, ui_ctx, edge_to_face_array, tx, ty, k_paint_brushes[slot].name, s_paint_brush == slot,
                       system_color_amber, dab)) {
            s_paint_brush = slot;
            if (!paint_mode_is_active()) mode_menu_enter_mode(ui_ctx, mode_context_paint, false);   /* a paint brush needs Paint mode */
        }
    }
    cursor += ((brush_paint_count + per_row - 1) / per_row) * (br_tile + br_gap) + 6;

    /* ---- status ---- */
    if (cursor + 14 > limit) return;
    char buf[96];
    snprintf(buf, sizeof(buf), "Sculpt: %s   Paint: %s", sculpt_brush_name(sculpt_get_brush()), k_paint_brushes[s_paint_brush].name);
    draw_text_element(cmd, ui_ctx, buf, left, cursor, system_color_near_white);
    if (!sculpt_mode_is_active() && !paint_mode_is_active() && cursor + 30 <= limit)
        draw_text_element(cmd, ui_ctx, "Pick a brush to turn Sculpt or Paint mode on.", left, cursor + 16, system_color_amber);
}