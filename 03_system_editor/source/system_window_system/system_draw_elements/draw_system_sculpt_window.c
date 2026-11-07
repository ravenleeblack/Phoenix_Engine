#include "system_globals.h"

/* Sculpt window. Tab sub menubar (same style as the Scene tab): mode, symmetry, invert, navigation, subdivision (+ - ++), Brushes.
   Settings are sliders now (drag = set, Shift = fine), the radius slider is logarithmic so small and huge brushes are one drag away.
   Body: target + subdivision info, then Tool Settings of the brush picked in the Brushes window (only the settings that brush uses).
   opens as a tab next to Lighting with the Sc button. */
enum { sw_pad = 10, sw_slider_h = 18, sw_row = 24 };

typedef struct {
    system_element_context* ui_element;
    VkCommandBuffer         cmd;
    int                     x, w;       /* content column */
    int                     y, limit;   /* cursor + bottom of the window */
} sw_panel;

static bool sw_fits(const sw_panel* p, int h) { return p->y + h <= p->limit; }

static void sw_header(sw_panel* p, const char* text)
{
    if (!sw_fits(p, 20)) return;
    draw_text_element(p->cmd, p->ui_element, text, p->x, p->y + 2, system_theme.system_text);
    push_quad_element(p->cmd, (float)p->x, (float)(p->y + 18), (float)p->w, 1.0f, system_theme.menu_separator);
    p->y += 24;
}

static void sw_label(sw_panel* p, const char* text, system_color color)
{
    if (!sw_fits(p, 16)) return;
    draw_text_element(p->cmd, p->ui_element, text, p->x, p->y + 2, color);
    p->y += 18;
}

/* "Label [=====  0.50  ]": drag to set, Shift = fine. log_scale = the track is logarithmic (radius). */
static void sw_slider(sw_panel* p, uint32_t id, const char* label, float* vert_to_edge_array, float lo, float hi, bool log_scale, const char* fmt)
{
    if (!sw_fits(p, sw_slider_h)) return;
    brush_slider(p->cmd, p->ui_element, id, label, p->x, p->y, p->w, sw_slider_h, vert_to_edge_array, lo, hi, log_scale, fmt);
    p->y += sw_row;
}

static void sw_slider_int(sw_panel* p, uint32_t id, const char* label, int* vert_to_edge_array, int lo, int hi)
{
    if (!sw_fits(p, sw_slider_h)) return;
    brush_slider_int(p->cmd, p->ui_element, id, label, p->x, p->y, p->w, sw_slider_h, vert_to_edge_array, lo, hi);
    p->y += sw_row;
}

void draw_sculpt_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Sculpt", node->x, content_y, node->w, content_h, system_theme.tab_content_window_sculpt);

    start_sculpt_mode();

    {
        static uint32_t last_sel = 0xFFFFFFFFu;
        const system_scene_object* s = find_scene_object_by_id(g_scene_registry.selected_object_id);
        const uint32_t id = s ? s->id : 0u;
        if (id != last_sel) {
            last_sel = id;
            phoenix_info("SculptWin", "selection: '%s' id %u type %d terrain=%d sculpt_active=%d", s ? s->name : "none", (unsigned)id, s ? (int)s->type : -1,
                         (int)(s && is_terrain_object(s)), (int)sculpt_mode_is_active());
        }
    }

    /* ================= tab sub menubar ================= */
    {
        brush_bar bar;
        brush_bar_begin(&bar, cmd, ui_ctx, node, 900u);

        const bool on = sculpt_mode_is_active();
        if (brush_bar_button(&bar, "Sc", on ? "Sculpt Mode: ON (click = Object mode)" : "Sculpt Mode: OFF", on))
        {
            phoenix_info("SculptWin", "Sc pressed: sculpt %s -> %s", on ? "ON" : "OFF", on ? "OFF (object mode)" : "ON");
            mode_menu_enter_mode(ui_ctx, on ? mode_context_object : mode_context_sculpt, false);
        }

        brush_bar_separator(&bar);
        if (brush_bar_button(&bar, "X", "Symmetry X", g_sculpt.sym_x)) g_sculpt.sym_x = !g_sculpt.sym_x;
        if (brush_bar_button(&bar, "Y", "Symmetry Y", g_sculpt.sym_y)) g_sculpt.sym_y = !g_sculpt.sym_y;
        if (brush_bar_button(&bar, "Z", "Symmetry Z", g_sculpt.sym_z)) g_sculpt.sym_z = !g_sculpt.sym_z;

        brush_bar_separator(&bar);
        if (brush_bar_button(&bar, "In", "Invert (or hold Ctrl)", g_sculpt.invert)) g_sculpt.invert = !g_sculpt.invert;
        if (brush_bar_button(&bar, g_sculpt.nav == sculpt_nav_maya ? "Ma" : "ZB",
                             g_sculpt.nav == sculpt_nav_maya ? "Navigation: Maya (click = ZBrush)" : "Navigation: ZBrush (click = Maya)", false))
            g_sculpt.nav = (g_sculpt.nav == sculpt_nav_maya) ? sculpt_nav_zbrush : sculpt_nav_maya;

        brush_bar_separator(&bar);
        {
            uint32_t verts = 0, faces = 0;
            const bool has_mesh = sculpt_selected_mesh_counts(&verts, &faces);
            if (brush_bar_button(&bar, "+", "Subdivide", false)) {
                const bool ok = has_mesh && sculpt_subdivide_selected();
                phoenix_info("SculptWin", "+ pressed: has_mesh=%d result=%s", (int)has_mesh, ok ? "ok" : "refused");
            }
            if (brush_bar_button(&bar, "-", "Undivide", false)) {
                const bool ok = has_mesh && sculpt_undivide_selected();
                phoenix_info("SculptWin", "- pressed: has_mesh=%d result=%s", (int)has_mesh, ok ? "ok" : "refused");
            }
            if (brush_bar_button(&bar, "++", "Densify (subdivide to ~20000 faces)", false)) {
                phoenix_info("SculptWin", "++ pressed: has_mesh=%d (densify to ~20000 faces)", (int)has_mesh);
                if (has_mesh) brush_densify_selected(20000u);
            }
            if (brush_bar_button(&bar, "Sm", "Smooth subdivision (off = flat)", g_sculpt.smooth_subdivide)) g_sculpt.smooth_subdivide = !g_sculpt.smooth_subdivide;
        }
        brush_bar_separator(&bar);
        if (brush_bar_button(&bar, "Br", "Open Brushes window", false)) brush_open_window();
        brush_bar_end(&bar);
    }

    sw_panel p = { ui_ctx, cmd, node->x + sw_pad, node->w - sw_pad * 2, brush_bar_bottom(node, content_y) + 8, content_y + content_h };

    /* ================= sculpt options (info) ================= */
    sw_header(&p, "Sculpt Options");

    {
        uint32_t verts = 0, faces = 0;
        const bool has_mesh = sculpt_selected_mesh_counts(&verts, &faces);
        char buf[96];
        if (has_mesh) snprintf(buf, sizeof(buf), "Subdivision level %d   %u faces", sculpt_subdiv_level(), (unsigned)faces);
        else          snprintf(buf, sizeof(buf), "Subdivision: select a mesh object");
        if (has_mesh) {
            sw_label(&p, buf, system_theme.system_text);
        } else {
            sw_label(&p, buf, system_theme.system_color_edit_mode_preview_color);
        }
        if (has_mesh && faces < 200) sw_label(&p, "Few faces: press ++ in the bar above (or the first stroke subdivides for you).", system_theme.system_text);
    }
    p.y += 4;

    /* ================= tool settings ================= */
    char title[64];
    snprintf(title, sizeof(title), "Tool Settings: %s", sculpt_brush_name(sculpt_get_brush()));
    sw_header(&p, title);

    sculpt_brush_settings* s = sculpt_current_settings();
    uint32_t mask = sculpt_brush_setting_mask(sculpt_get_brush());

    if (mask & sculpt_setting_radius)        sw_slider(&p, 0x101, "Radius",       &s->radius,       0.02f, 50.0f, true,  "%.2f mesh");
    if (mask & sculpt_setting_strength)      sw_slider(&p, 0x102, "Strength",     &s->strength,     0.0f,  1.0f,  false, "%.2f");
    if (mask & sculpt_setting_hardness)      sw_slider(&p, 0x103, "Hardness",     &s->hardness,     0.0f,  0.99f, false, "%.2f");
    if (mask & sculpt_setting_plane_offset)  sw_slider(&p, 0x104, "Plane offset", &s->plane_offset, -1.0f, 1.0f,  false, "%.2f");
    if (mask & sculpt_setting_smooth_passes) sw_slider_int(&p, 0x105, "Smooth passes", &s->smooth_passes, 1, 8);
    if (mask & sculpt_setting_spacing)       sw_slider(&p, 0x106, "Spacing",      &s->spacing,      0.02f, 1.0f,  false, "%.2f");
    sculpt_clamp_settings(s);

    /* status */
    if (!sculpt_mode_is_active())            sw_label(&p, "Sculpt mode is off. Press Sc in the bar above.", system_theme.system_color_edit_mode_preview_color);
    else if (!sculpt_selected_mesh_counts(NULL, NULL)) sw_label(&p, "Select a mesh object to sculpt.", system_theme.system_color_edit_mode_preview_color);
    else                                     sw_label(&p, "Ready: drag on the mesh in the Scene view.", system_theme.system_text);
}