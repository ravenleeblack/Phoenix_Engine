#include "system_globals.h"
#include <stdarg.h>
#include <stdio.h>

/* status text goes to the statusbar (draw_system_statusbar.c reads status_bar_text_buffer). */
static void set_paint_window_status(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), format, args);
    va_end(args);
}

/* Paint window. Tab sub menubar (same style as the Scene tab):
     Pt mode   Ex export   +  -  ++ subdivide / undivide / densify   Br Brushes window   Co colour picker on/off
   Body: target + mesh detail, colour picker (HSV square, hue bar, Save / Del), sliders (Size, Opacity, Hardness, Spacing),
   then the saved colours and a 60 colour palette.
   Pressing Pt also opens the Brushes window (brush_follow_modes), like Sc does for sculpting. */
enum { pw_pad = 10, pw_slider_h = 18, pw_row = 24 };

typedef struct { system_element_context* ui_element; VkCommandBuffer cmd; int x, w, y, limit; } pw_panel;

static bool s_show_color = true;

static void pw_label(pw_panel* p, const char* text, system_color color)
{
    if (p->y + 16 > p->limit) return;
    draw_text_element(p->cmd, p->ui_element, text, p->x, p->y + 2, color);
    p->y += 18;
}

static void pw_slider(pw_panel* p, uint32_t id, const char* label, float* vert_to_edge_array, float lo, float hi, bool log_scale, const char* fmt)
{
    if (p->y + pw_slider_h > p->limit) return;
    brush_slider(p->cmd, p->ui_element, id, label, p->x, p->y, p->w, pw_slider_h, vert_to_edge_array, lo, hi, log_scale, fmt);
    p->y += pw_row;
}

void draw_system_paint_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Paint", node->x, content_y, node->w, content_h, system_theme.tab_content_window_paint);

    start_paint_mode();
    brush_follow_modes();              /* Pt pressed -> Brushes window opens (once per frame, shared with the Scene view) */

    const bool on = paint_mode_is_active();
    const system_scene_object* sel = find_scene_object_by_id(g_scene_registry.selected_object_id);
    const bool terrain_sel = sel && is_terrain_object(sel);
    const bool mesh_sel    = sel && sel->type == scene_object_mesh && !terrain_sel;   /* ordinary mesh: subdivide / export allowed */
    const bool has_mesh    = mesh_sel || terrain_sel;                                 /* paint target (terrain paints through terrain_paint_dab) */

    {
        static uint32_t last_sel = 0xFFFFFFFFu;
        const uint32_t id = sel ? sel->id : 0u;
        if (id != last_sel) {
            last_sel = id;
            phoenix_info("PaintWin", "selection: '%s' id %u type %d terrain=%d paintable_mesh=%d paint_active=%d", sel ? sel->name : "none", (unsigned)id,
                         sel ? (int)sel->type : -1, (int)terrain_sel, (int)mesh_sel, (int)on);
        }
    }

    /* ================= tab sub menubar ================= */
    {
        brush_bar bar;
        brush_bar_begin(&bar, cmd, ui_ctx, node, 950u);

        if (brush_bar_button(&bar, "Pt", on ? "Paint Mode: ON (click = Object mode)" : "Paint Mode: OFF", on))
        {
            phoenix_info("PaintWin", "Pt pressed: paint %s -> %s", on ? "ON" : "OFF", on ? "OFF (object mode)" : "ON");
            mode_menu_enter_mode(ui_ctx, on ? mode_context_object : mode_context_paint, false);
            set_paint_window_status("Mode: %s.", on ? mode_context_name(mode_context_object) : mode_context_name(mode_context_paint));
        }

        brush_bar_separator(&bar);
        const bool terrain_target = g_terrain.exists && (!sel || sel == &g_scene_registry.objects[0] || terrain_sel);   /* nothing or the terrain selected: the terrain is what gets painted */
        const char* export_tip = "Export PNG";
        if (terrain_target) {
            export_tip = "Export the terrain paint as assets/textures/terrain_paint.png (one pixel per cube column)";
        }
        if (brush_bar_button(&bar, "Ex", export_tip, false)) {
            if (terrain_target) {
                export_terrain_paint_png();
            } else if (mesh_sel) {
                char file[96];
                if (paint_texture_export_for_object(sel->id, sel->name, file, sizeof(file)))
                    snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Paint map saved: %s", file);
                else
                    snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Nothing painted on '%s' yet.", sel->name);
            } else {
                snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Select a mesh object to export its paint map.");
            }
        }
        if (terrain_target) {
            if (brush_bar_button(&bar, "Im", "Load assets/textures/terrain_paint.png as the terrain paint", false)) {
                import_terrain_paint_png();
            }
            if (brush_bar_button(&bar, "Cl", "Erase all the terrain paint", false)) {
                terrain_clear_paint();
                set_paint_window_status("Terrain paint erased.");
            }
        }

        /* paint lives on vertices: more faces = more detail. same buttons as the Sculpt window */
        brush_bar_separator(&bar);
        if (brush_bar_button(&bar, "+",  "Subdivide (x4 faces)", false)) {
            if (mesh_sel) { const bool ok = sculpt_subdivide_selected(); phoenix_info("PaintWin", "+ subdivide '%s': %s", sel->name, ok ? "ok" : "refused"); set_paint_window_status("Subdivide '%s': %s.", sel->name, ok ? "done" : "refused"); }
            else snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Select a mesh object to subdivide it.");
        }
        if (brush_bar_button(&bar, "-",  "Undivide", false)) {
            if (mesh_sel) { const bool ok = sculpt_undivide_selected(); phoenix_info("PaintWin", "- undivide '%s': %s", sel->name, ok ? "ok" : "refused"); set_paint_window_status("Undivide '%s': %s.", sel->name, ok ? "done" : "refused"); }
            else { phoenix_warn("PaintWin", "- pressed but selection is not an ordinary mesh (terrain=%d)", (int)terrain_sel); set_paint_window_status("Select a mesh object to undivide it."); }
        }
        if (brush_bar_button(&bar, "++", "Densify (subdivide to ~20000 faces)", false)) {
            if (mesh_sel) { phoenix_info("PaintWin", "++ densify '%s' to ~20000 faces", sel->name); brush_densify_selected(20000u); set_paint_window_status("Densify '%s' to ~20000 faces.", sel->name); }
            else snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Select a mesh object to densify it.");
        }

        brush_bar_separator(&bar);
        if (brush_bar_button(&bar, "Br", "Open Brushes window", false)) { brush_open_window(); set_paint_window_status("Brushes window opened."); }
        if (brush_bar_button(&bar, "Co", s_show_color ? "Colour picker: shown (click = hide)" : "Colour picker: hidden (click = show)", s_show_color))
        { s_show_color = !s_show_color; set_paint_window_status("Colour picker: %s.", s_show_color ? "shown" : "hidden"); }
        brush_bar_end(&bar);
    }

    pw_panel p = { ui_ctx, cmd, node->x + pw_pad, node->w - pw_pad * 2, brush_bar_bottom(node, content_y) + 8, content_y + content_h };

    /* target object: painting always goes to the obj_vert_selected mesh */
    char buf[96];
    if (has_mesh) snprintf(buf, sizeof(buf), "Target: %s", sel->name);
    else          snprintf(buf, sizeof(buf), "Select a mesh object to paint");
    pw_label(&p, buf, has_mesh ? system_theme.system_text : system_theme.menu_active_text);

    snprintf(buf, sizeof(buf), "Brush: %s (pick in Brushes)", brush_window_paint_brush_name(brush_window_get_paint_brush()));
    pw_label(&p, buf, system_theme.system_active_text);

    /* detail of the mesh: this is what limits how fine you can paint */
    if (terrain_sel) {
        pw_label(&p, "Terrain: painted per 4 mesh cube column (no subdivide needed).", system_theme.system_text);
    } else if (mesh_sel) {
        uint32_t verts = 0, faces = 0;
        sculpt_selected_mesh_counts(&verts, &faces);
        snprintf(buf, sizeof(buf), "Mesh: %u verts  %u faces  level %d", (unsigned)verts, (unsigned)faces, sculpt_subdiv_level());
        pw_label(&p, buf, system_theme.system_text);
        if (verts < 2000u) pw_label(&p, "Few vertices: press ++ above for more paint detail.", system_theme.menu_active_text);
    }
    p.y += 4;

    /* ================= colour ================= */
    float* col = g_paint.color;
    if (s_show_color) {
        const int used = brush_color_picker(cmd, ui_ctx, p.x, p.y, p.w, p.limit - p.y, col);
        p.y += used + (used > 0 ? 8 : 0);
    } else if (p.y + 24 <= p.limit) {
        push_quad_element(cmd, (float)p.x, (float)p.y, (float)p.w, 18.0f, (system_color){ col[0], col[1], col[2], 1.0f });
        draw_icon_border(cmd, p.x, p.y, p.w, 18, system_theme.system_border);
        p.y += 26;
    }

    /* ================= sliders ================= */
    pw_slider(&p, 0x201, "Size",     &g_paint.radius,   0.02f, 50.0f, true,  "%.2f mesh");   /* log: small and huge brushes are one drag away */
    pw_slider(&p, 0x202, "Opacity",  &g_paint.opacity,  0.0f,  1.0f,       false, "%.2f");
    pw_slider(&p, 0x203, "Hardness", &g_paint.hardness, 0.0f,  0.99f,      false, "%.2f");
    pw_slider(&p, 0x204, "Spacing", &g_paint.spacing, 0.02f, 1.0f, false, "%.2f");
    paint_clamp_settings();

    /* ================= status ================= */
    if (!on)                         pw_label(&p, "Paint mode is off. Press Pt in the bar above.", system_theme.menu_active_text);
    else if (has_mesh)               pw_label(&p, "Ready: drag on the mesh in the Scene view. Ctrl = erase.", get_theme_hint_color());
    p.y += 4;

    /* ================= saved colours + palette ================= */
    if (s_show_color && p.y + 40 <= p.limit)
        brush_color_palette(cmd, ui_ctx, p.x, p.y, p.w, p.limit - p.y, col);
}