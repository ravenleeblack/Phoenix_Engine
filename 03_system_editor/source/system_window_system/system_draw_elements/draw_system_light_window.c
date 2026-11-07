#include "system_globals.h"

/* ========================================================================= */
/* Lighting window (tab next to Inspector).                                   */
/*                                                                           */
/*   Layer      [ drop down ]    which terrain chunk layer stores the light   */
/*   Lighting   [Lit] [Unl]                                                   */
/*   Sun        azimuth / elevation / ambient   (or "driven by <object>")    */
/*   Terrain light   min light, rebuild, clear layer                          */
/*   Light objects   + Directional, one row per light: On/Off, Select         */
/*                                                                           */
/* the drop down list is drawn LAST so it sits on top. while it is open the   */
/* rest of the window sees no mouse button, so a click on the list never      */
/* also hits a button behind it.                                              */
/* ========================================================================= */

enum {
    lw_pad       = 10,
    lw_row_h     = 22,
    lw_btn_h     = 20,
    lw_label_w   = 92,
    lw_value_w   = 40,
    lw_gap       = 6,

    lw_id_base      = 888100,
    lw_id_layer     = lw_id_base + 1,
    lw_id_layer_it  = lw_id_base + 10,    /* + layer index */
    lw_id_lit       = lw_id_base + 30,
    lw_id_unl       = lw_id_base + 31,
    lw_id_rebuild   = lw_id_base + 32,
    lw_id_clear     = lw_id_base + 33,
    lw_id_add_dir   = lw_id_base + 34,
    lw_id_slider    = lw_id_base + 40,    /* + n */
    lw_id_light_on  = lw_id_base + 100,   /* + light index * 2 */
    lw_id_light_sel = lw_id_base + 101    /* + light index * 2 */
};

static bool s_layer_open = false;
static bool s_prev_down  = false;
static bool s_save_pending = false;       /* a slider moved: save prefs when the mouse is released */

static bool in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static bool room(int y, int h, int limit) { return y + h <= limit; }

/* small grey help line, cut off so it never runs out of the window */
static void hint(VkCommandBuffer cmd, system_element_context* ui_element, const char* text, int x, int y, int w)
{
    char buf[96];
    int chars = (ui_font_w > 0) ? w / ui_font_w : 0;
    if (chars < 0) chars = 0;
    snprintf(buf, sizeof(buf), "%.*s", chars, text);
    draw_text_element(cmd, ui_element, buf, x, y, system_theme.menu_separator);
}

static void section(VkCommandBuffer cmd, system_element_context* ui_element, const char* title, int x, int y, int w)
{
    draw_text_element(cmd, ui_element, title, x, y, system_theme.system_text);
    push_quad_element(cmd, (float)x, (float)(y + ui_font_h + 4), (float)w, 1.0f, system_theme.system_border);
}

static bool lw_button(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id, const char* label,
                      int x, int y, int w, bool obj_vert_selected)
{
    return draw_button_element(cmd, ui_element, id, label, x, y, w, lw_btn_h,
                               obj_vert_selected ? system_theme.system_accent : system_theme.tab_sub_menubar_button);
}

/* horizontal slider. edits *vert_to_edge_array live. returns true on the frame the mouse is released after a drag. */
static bool lw_slider(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id, const char* label,
                      int x, int y, int w, float* vert_to_edge_array, float lo, float hi, float disp_scale, bool enabled)
{
    const int h  = 18;
    const int tx = x + lw_label_w;
    const int tw = w - lw_label_w - lw_value_w;
    bool released = false;
    if (tw < 20) return false;

    draw_text_element(cmd, ui_element, label, x, y + (h - ui_font_h) / 2,
                      enabled ? system_theme.system_text : system_theme.menu_separator);

    const bool over = enabled && in_rect(ui_element->input.mouse_x, ui_element->input.mouse_y, tx - 4, y, tw + 8, h);
    if (over && ui_element->input.mouse_down && ui_element->active_id == 0) ui_element->active_id = id;

    if (ui_element->active_id == id) {
        if (ui_element->input.mouse_down) {
            float t = (float)(ui_element->input.mouse_x - tx) / (float)tw;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            *vert_to_edge_array = lo + t * (hi - lo);
        } else {
            ui_element->active_id = 0;
            released = true;
        }
    }

    float t = (*vert_to_edge_array - lo) / (hi - lo);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

    push_rounded_quad(cmd, (float)tx, (float)(y + h / 2 - 2), (float)tw, 4.0f, system_theme.system_border, 2, corner_all);
    if ((float)tw * t >= 4.0f)
        push_rounded_quad(cmd, (float)tx, (float)(y + h / 2 - 2), (float)tw * t, 4.0f,
                          enabled ? system_theme.system_accent : system_theme.menu_separator, 2, corner_all);
    const bool hot = over || ui_element->active_id == id;
    push_rounded_quad(cmd, (float)tx + (float)tw * t - 4.0f, (float)(y + h / 2 - 6), 8.0f, 12.0f,
                      hot ? system_theme.system_active_text : system_theme.system_text, 3, corner_all);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", (int)(*vert_to_edge_array * disp_scale + 0.5f));
    draw_text_element(cmd, ui_element, buf, tx + tw + 10, y + (h - ui_font_h) / 2, system_theme.system_text);
    return released;
}

void draw_system_light_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Lighting", node->x, content_y, node->w, content_h, system_theme.tab_content_window);

    const int x     = node->x + lw_pad;
    const int w     = node->w - lw_pad * 2;
    const int limit = content_y + content_h;
    int y = content_y + lw_pad;
    if (w < 120) return;

    const bool real_down = ui_ctx->input.mouse_down;
    const bool pressed   = real_down && !s_prev_down;
    s_prev_down = real_down;

    /* the drop down geometry is needed up front (outside click closes it, everything else is masked while it is open) */
    const int dd_x = x + lw_label_w, dd_w = w - lw_label_w, dd_y = y;
    const int list_n = terrain_max_layers - 1;                     /* layers 1..9, never the height layer */
    const int list_y = dd_y + lw_row_h + 2, list_h = list_n * lw_row_h + 4;
    if (s_layer_open && pressed) {
        const int mx = ui_ctx->input.mouse_x, my = ui_ctx->input.mouse_y;
        if (!in_rect(mx, my, dd_x, dd_y, dd_w, lw_row_h) && !in_rect(mx, my, dd_x, list_y, dd_w, list_h))
            s_layer_open = false;
    }

    /* ---- LAYER (the header button itself always gets the real mouse) ---- */
    draw_text_element(cmd, ui_ctx, "Layer", x, dd_y + (lw_row_h - ui_font_h) / 2, system_theme.system_text);
    {
        char cap[64];
        const int cur = get_terrain_light_layer();
        snprintf(cap, sizeof(cap), "%d  %s  %s", cur + 1, get_terrain_layer_name(cur), s_layer_open ? "^" : "vert_to_edge_array");
        if (draw_button_element(cmd, ui_ctx, lw_id_layer, cap, dd_x, dd_y, dd_w, lw_row_h, system_theme.tab_sub_menubar_button))
            s_layer_open = !s_layer_open;
    }
    y += lw_row_h + lw_gap + 4;

    /* everything below sees no mouse button while the list is open */
    if (s_layer_open) ui_ctx->input.mouse_down = false;

    /* ---- LIGHTING on / off ---- */
    if (room(y, lw_row_h + 14, limit)) {
        section(cmd, ui_ctx, "LIGHTING", x, y, w);
        y += ui_font_h + 10;
        draw_text_element(cmd, ui_ctx, "Mode", x, y + (lw_btn_h - ui_font_h) / 2, system_theme.system_text);
        const bool on = get_terrain_lighting_enabled();
        if (lw_button(cmd, ui_ctx, lw_id_lit, "Lit", x + lw_label_w,      y, 56,  on)) set_terrain_lighting_enabled(true);
        if (lw_button(cmd, ui_ctx, lw_id_unl, "Unl", x + lw_label_w + 62, y, 56, !on)) set_terrain_lighting_enabled(false);
        y += lw_row_h + lw_gap;
    }

    /* ---- SUN ---- */
    if (room(y, 6 * lw_row_h + 2 * (ui_font_h + 4), limit)) {
        section(cmd, ui_ctx, "SUN", x, y, w);
        y += ui_font_h + 10;

        const char* driver = NULL;
        const bool driven = is_light_sun_driven(&driver);
        bool changed = false;

        if (driven) {
            char line[96];
            snprintf(line, sizeof(line), "Driven by: %s", driver);
            draw_text_element(cmd, ui_ctx, line, x, y, system_theme.system_accent);
            y += ui_font_h + 6;
        }
        changed |= lw_slider(cmd, ui_ctx, lw_id_slider + 4, "Brightness", x, y, w, &g_light.sun_intensity,     0.0f, 3.0f,   100.0f, true); y += lw_row_h;
        changed |= lw_slider(cmd, ui_ctx, lw_id_slider + 5, "Warm/Cool",  x, y, w, &g_light.sun_warmth,        -1.0f, 1.0f,  100.0f, true); y += lw_row_h;
        changed |= lw_slider(cmd, ui_ctx, lw_id_slider + 2, "Ambient",    x, y, w, &g_scene_sun.ambient,       0.0f, 0.9f,   100.0f, true); y += lw_row_h;
        changed |= lw_slider(cmd, ui_ctx, lw_id_slider + 0, "Sun turn",   x, y, w, &g_scene_sun.azimuth_deg,   0.0f, 360.0f, 1.0f, !driven); y += lw_row_h;
        changed |= lw_slider(cmd, ui_ctx, lw_id_slider + 1, "Sun height", x, y, w, &g_scene_sun.elevation_deg, 5.0f, 90.0f,  1.0f, !driven); y += lw_row_h;
        hint(cmd, ui_ctx, "Brightness: 100 = normal, up to 300",     x, y, w); y += ui_font_h + 4;
        hint(cmd, ui_ctx, "Warm/Cool: left = blue, right = orange",  x, y, w); y += ui_font_h + 4;
        if (changed) s_save_pending = true;
        y += lw_gap;
    }

    /* ---- TERRAIN LIGHT ---- */
    if (room(y, 2 * lw_row_h + 14, limit)) {
        section(cmd, ui_ctx, "TERRAIN LIGHT", x, y, w);
        y += ui_font_h + 10;

        if (lw_slider(cmd, ui_ctx, lw_id_slider + 3, "Min light", x, y, w, &g_light.min_light, 0.0f, 1.0f, 100.0f, true)) {
            rebuild_terrain_light_all();                 /* baked into vertex colour: re-bake once the drag ends */
            s_save_pending = true;
        }
        y += lw_row_h;

        const int bw = (w - lw_gap) / 2;
        if (lw_button(cmd, ui_ctx, lw_id_rebuild, "Rebuild",     x,                y, bw, false)) rebuild_terrain_light_all();
        if (lw_button(cmd, ui_ctx, lw_id_clear,   "Clear layer", x + bw + lw_gap,  y, bw, false)) clear_terrain_light_layer();
        y += lw_row_h + lw_gap;
    }

    /* ---- LIGHT OBJECTS ---- */
    if (room(y, 2 * lw_row_h, limit)) {
        section(cmd, ui_ctx, "LIGHT OBJECTS", x, y, w);
        y += ui_font_h + 10;

        if (lw_button(cmd, ui_ctx, lw_id_add_dir, "+ Directional light", x, y, 150, false)) {
            editor_vec3 obj_vert_pos = { 0.0f, 12.0f, 0.0f };
            create_system_directional_light(obj_vert_pos, 1.0f, "Directional Light",
                                            render_context.logical_device, render_context.physical_device,
                                            render_context.command_pool, render_context.graphics_queue);
        }
        y += lw_row_h + 2;

        for (int i = 0; i < get_light_object_count() && room(y, lw_row_h, limit); i++) {
            const light_object* lo = get_light_object_at(i);
            if (!lo) break;
            const int sel_w = 52, on_w = 40;
            const int name_w = w - sel_w - on_w - lw_gap * 2;
            char nm[40];
            snprintf(nm, sizeof(nm), "%.*s", name_w > 0 ? name_w / ui_font_w : 0, get_light_object_name(i));
            draw_text_element(cmd, ui_ctx, nm, x, y + (lw_btn_h - ui_font_h) / 2,
                              lo->enabled ? system_theme.system_active_text : system_theme.system_text);
            if (lw_button(cmd, ui_ctx, (uint32_t)(lw_id_light_on  + i * 2), lo->enabled ? "On" : "Off",
                          x + name_w + lw_gap, y, on_w, lo->enabled))
                set_light_object_enabled(i, !lo->enabled);
            if (lw_button(cmd, ui_ctx, (uint32_t)(lw_id_light_sel + i * 2), "Select",
                          x + name_w + on_w + lw_gap * 2, y, sel_w, false))
                select_light_object(i);
            y += lw_row_h;
        }
    }

    /* mouse back to normal, then the drop down list on top of everything */
    ui_ctx->input.mouse_down = real_down;

    if (s_layer_open) {
        push_quad_element(cmd, (float)(dd_x - 1), (float)(list_y - 1), (float)(dd_w + 2), (float)(list_h + 2), system_theme.dropdown_outline);
        push_quad_element(cmd, (float)dd_x, (float)list_y, (float)dd_w, (float)list_h, system_theme.menu_open);

        for (int i = 0; i < list_n; i++) {
            const int layer = i + 1;
            char item[64];
            snprintf(item, sizeof(item), "%d  %s", layer + 1, get_terrain_layer_name(layer));
            const bool cur = (layer == get_terrain_light_layer());
            if (draw_button_element(cmd, ui_ctx, (uint32_t)(lw_id_layer_it + i), item, dd_x + 2, list_y + 2 + i * lw_row_h,
                                    dd_w - 4, lw_row_h - 2, cur ? system_theme.system_accent : system_theme.menu_open)) {
                set_terrain_light_layer(layer);
                s_layer_open = false;
                s_save_pending = true;
            }
        }
    }

    if (s_save_pending && !real_down) { prefs_save(); s_save_pending = false; }
}