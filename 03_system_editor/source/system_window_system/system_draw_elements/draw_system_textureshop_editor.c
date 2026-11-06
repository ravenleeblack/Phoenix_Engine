#include "system_globals.h"

/* Texture Shop UV Editor, laid out like Maya's UV Editor:
   tab sub menubar with the tool buttons (same style as the Scene tab), menu strip, big dark UV canvas with the 0-1 tile,
   status strip with U / V readout. */

enum {
    uv_menu_h = 18, uv_status_h = 18,
    uv_sub_gap = 2, uv_sub_sep = 7,
    uv_checker_cells = 8
};

typedef enum { uv_tool_select = 0, uv_tool_move, uv_tool_rotate, uv_tool_scale } enum_uv_tool;

typedef struct {
    bool  ready;
    float center_u, center_v;       /* uv at the middle of the canvas */
    float scale;                    /* pixels per 1.0 uv */
    bool  show_grid;
    enum_uv_tool tool;
    bool  prev_down;
    bool  panning;
    int   prev_mouse_x, prev_mouse_y;
    uint32_t faces_drawn;           /* faces of the obj_vert_selected object drawn last frame */
} uv_editor_view;

typedef struct { int x, y, w, h; } uv_rect;

static uv_editor_view g_uv_view = { .show_grid = true, .tool = uv_tool_select };

static const float k_uv_bg[4]       = { 0.22f, 0.22f, 0.22f, 1.0f };
static const float k_uv_tile[4]     = { 0.30f, 0.30f, 0.30f, 1.0f };
static const float k_uv_minor[4]    = { 0.33f, 0.33f, 0.33f, 1.0f };
static const float k_uv_mid[4]      = { 0.40f, 0.40f, 0.40f, 1.0f };
static const float k_uv_major[4]    = { 0.52f, 0.52f, 0.52f, 1.0f };
static const float k_uv_border[4]   = { 0.78f, 0.78f, 0.78f, 1.0f };
static const float k_uv_check_a[4]  = { 0.42f, 0.42f, 0.42f, 1.0f };
static const float k_uv_check_b[4]  = { 0.34f, 0.34f, 0.34f, 1.0f };

static float convert_u_to_x(const uv_rect* vp, float u) { return (float)vp->x + (float)vp->w * 0.5f + (u - g_uv_view.center_u) * g_uv_view.scale; }
static float convert_v_to_y(const uv_rect* vp, float vert_to_edge_array) { return (float)vp->y + (float)vp->h * 0.5f - (vert_to_edge_array - g_uv_view.center_v) * g_uv_view.scale; }

/* quad clipped to the canvas, so nothing ever paints outside it */
static void fill_view_rect(VkCommandBuffer cmd, const uv_rect* vp, float x, float y, float w, float h, const float c[4])
{
    float x0 = fmaxf(x, (float)vp->x),            y0 = fmaxf(y, (float)vp->y);
    float x1 = fminf(x + w, (float)(vp->x + vp->w)), y1 = fminf(y + h, (float)(vp->y + vp->h));
    if (x1 <= x0 || y1 <= y0) return;
    system_color color = { .red = c[0], .green = c[1], .blue = c[2], .alpha = c[3] };
    push_quad_element(cmd, x0, y0, x1 - x0, y1 - y0, color);
}

/* ------------------------------------------------------------------------- */
/* tab sub menubar (same look as the Scene tab)                              */
/* ------------------------------------------------------------------------- */
/* the dock can swallow clicks, so buttons use the raw rising edge_to_face_array of the mouse */
static bool draw_uv_sub_button(VkCommandBuffer cmd, system_element_context* ui_element, const char* label, const char* name,
                               int x, int y, int w, int h, bool active, bool edge_to_face_array, const char** hover_name)
{
    const bool hovered = validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, x, y, w, h);
    if (hovered) *hover_name = name;

    if (active) {
        draw_icon_element(cmd, x, y, w, h, color_with_alpha(system_theme.system_accent, 0.60f));
        draw_icon_border (cmd, x, y, w, h, system_theme.system_accent);
    } else if (hovered) {
        draw_icon_element(cmd, x, y, w, h, process_theme_color(system_theme.tab_sub_menubar, system_theme.tab_hover_delta));
    }

    const int tw = (int)strlen(label) * ui_font_w;
    draw_text_element(cmd, ui_element, label, x + (w - tw) / 2, y + (h - ui_font_h) / 2,
                      (active || hovered) ? system_theme.tab_item_text_active : system_theme.tab_item_text);
    return hovered && edge_to_face_array;
}

static void draw_uv_sub_menu(VkCommandBuffer cmd, system_element_context* ui_element, const system_dock_node* node, const uv_rect* vp, bool edge_to_face_array)
{
    const int x = node->x, y = node->y + tab_bar_h, w = node->w, h = tab_menu_h;
    draw_tab_sub_menubar(cmd, ui_element, x, y, w, h);

    int s = h - 5;
    if (s > 22) s = 22;
    if (s < 12) return;

    const int by = y + (h - 1 - s) / 2;
    const int right_limit = x + w - 6;
    int bx = x + 6;
    const char* hover_name = NULL;

    #define UV_BUTTON(label, name, active, action) \
        do { const int bw = (int)strlen(label) * ui_font_w + 12; \
             if (bx + bw <= right_limit) { \
                 if (draw_uv_sub_button(cmd, ui_element, label, name, bx, by, bw, s, active, edge_to_face_array, &hover_name)) { action; } \
                 bx += bw + uv_sub_gap; } } while (0)
    #define UV_SEPARATOR() \
        do { if (bx + uv_sub_sep < right_limit) { \
                 draw_icon_element(cmd, bx + uv_sub_sep / 2, by + 2, 1, s - 4, system_theme.tab_sub_menubar_divider); \
                 bx += uv_sub_sep; } } while (0)

    UV_BUTTON("Sel", "Select",  g_uv_view.tool == uv_tool_select, g_uv_view.tool = uv_tool_select);
    UV_BUTTON("Mov", "Move",    g_uv_view.tool == uv_tool_move,   g_uv_view.tool = uv_tool_move);
    UV_BUTTON("Rot", "Rotate",  g_uv_view.tool == uv_tool_rotate, g_uv_view.tool = uv_tool_rotate);
    UV_BUTTON("Scl", "Scale",   g_uv_view.tool == uv_tool_scale,  g_uv_view.tool = uv_tool_scale);
    UV_SEPARATOR();
    UV_BUTTON("Cut", "Cut (not wired yet)",    false, (void)0);
    UV_BUTTON("Sew", "Sew (not wired yet)",    false, (void)0);
    UV_BUTTON("Unf", "Unfold (not wired yet)", false, (void)0);
    UV_BUTTON("Lay", "Layout (not wired yet)", false, (void)0);
    UV_SEPARATOR();
    UV_BUTTON("Chk", "Checker",     g_mode_state.uv.show_checker, g_mode_state.uv.show_checker = !g_mode_state.uv.show_checker);
    UV_BUTTON("Grd", "Grid",        g_uv_view.show_grid,          g_uv_view.show_grid = !g_uv_view.show_grid);
    UV_BUTTON("Snp", "Pixel Snap",  g_mode_state.uv.pixel_snap,   g_mode_state.uv.pixel_snap = !g_mode_state.uv.pixel_snap);
    UV_SEPARATOR();
    UV_BUTTON("+",   "Zoom In",  false, g_uv_view.scale = fminf(g_uv_view.scale * 1.25f, 40000.0f));
    UV_BUTTON("-",   "Zoom Out", false, g_uv_view.scale = fmaxf(g_uv_view.scale * 0.8f, 20.0f));
    UV_BUTTON("Fit", "Fit",      false, { g_uv_view.center_u = 0.5f; g_uv_view.center_v = 0.5f;
                                          g_uv_view.scale = fmaxf(20.0f, fminf((float)vp->w, (float)vp->h) * 0.85f); });
    #undef UV_BUTTON
    #undef UV_SEPARATOR

    /* name of the hovered button, right aligned, only when it fits */
    if (hover_name) {
        const int tw = (int)strlen(hover_name) * ui_font_w;
        const int tx = right_limit - tw;
        if (tx > bx + 8) draw_text_element(cmd, ui_element, hover_name, tx, by + (s - ui_font_h) / 2, system_theme.tab_item_text);
    }
}

/* ------------------------------------------------------------------------- */
/* canvas                                                                    */
/* ------------------------------------------------------------------------- */
static void draw_uv_grid(VkCommandBuffer cmd, const uv_rect* vp)
{
    const float s = g_uv_view.scale;
    const float left_u  = g_uv_view.center_u - ((float)vp->w * 0.5f) / s;
    const float right_u = g_uv_view.center_u + ((float)vp->w * 0.5f) / s;
    const float low_v   = g_uv_view.center_v - ((float)vp->h * 0.5f) / s;
    const float high_v  = g_uv_view.center_v + ((float)vp->h * 0.5f) / s;

    /* tenths when there is room for them, whole tiles otherwise */
    const int stride = (s * 0.1f >= 9.0f) ? 1 : 10;

    int iu0 = (int)floorf(left_u  * 10.0f / (float)stride) * stride, iu1 = (int)ceilf(right_u * 10.0f);
    for (int i = iu0; i <= iu1; i += stride) {
        const float* c = (i % 10 == 0) ? k_uv_major : (i % 5 == 0) ? k_uv_mid : k_uv_minor;
        float x = floorf(convert_u_to_x(vp, (float)i * 0.1f));
        fill_view_rect(cmd, vp, x, (float)vp->y, 1.0f, (float)vp->h, c);
    }
    int iv0 = (int)floorf(low_v * 10.0f / (float)stride) * stride, iv1 = (int)ceilf(high_v * 10.0f);
    for (int i = iv0; i <= iv1; i += stride) {
        const float* c = (i % 10 == 0) ? k_uv_major : (i % 5 == 0) ? k_uv_mid : k_uv_minor;
        float y = floorf(convert_v_to_y(vp, (float)i * 0.1f));
        fill_view_rect(cmd, vp, (float)vp->x, y, (float)vp->w, 1.0f, c);
    }
}

static void draw_uv_labels(VkCommandBuffer cmd, system_element_context* ui_element, const uv_rect* vp)
{
    const float s = g_uv_view.scale;
    float step = 0.0f;
    if      (s * 0.1f >= 44.0f) step = 0.1f;
    else if (s * 0.5f >= 44.0f) step = 0.5f;
    else if (s        >= 44.0f) step = 1.0f;
    if (step <= 0.0f) return;

    const float left_u  = g_uv_view.center_u - ((float)vp->w * 0.5f) / s;
    const float right_u = g_uv_view.center_u + ((float)vp->w * 0.5f) / s;
    const float low_v   = g_uv_view.center_v - ((float)vp->h * 0.5f) / s;
    const float high_v  = g_uv_view.center_v + ((float)vp->h * 0.5f) / s;
    char text[24];

    for (int i = (int)floorf(left_u / step); i <= (int)ceilf(right_u / step); i++) {
        int x = (int)convert_u_to_x(vp, (float)i * step) + 3;
        if (x < vp->x + 2 || x + 32 > vp->x + vp->w) continue;
        snprintf(text, sizeof(text), "%.1f", (double)((float)i * step));
        draw_text_element(cmd, ui_element, text, x, vp->y + vp->h - 15, system_color_light_gray_2);
    }
    for (int i = (int)floorf(low_v / step); i <= (int)ceilf(high_v / step); i++) {
        int y = (int)convert_v_to_y(vp, (float)i * step) - 14;
        if (y < vp->y + 2 || y + 12 > vp->y + vp->h - 18) continue;
        snprintf(text, sizeof(text), "%.1f", (double)((float)i * step));
        draw_text_element(cmd, ui_element, text, vp->x + 4, y, system_color_light_gray_2);
    }
}

/* ------------------------------------------------------------------------- */
/* the obj_vert_selected object's UVs                                                 */
/* Source: edit_mode_face.uv[k] (system_edit_manager.h) = the texture coordinate of corner k of a face. Verts are shared but   */
/* UVs are per corner, so a seam is just two faces holding different uv for the same vert. em_triangulate() copies them */
/* into editor_vertex.u / .vert_to_edge_array, which is what the mesh shader receives (v_uv).                                           */
/* ------------------------------------------------------------------------- */
enum { uv_mesh_max_faces = 512 };            /* each edge_to_face_array is drawn as small quads, so keep big meshes out of the UI queue */

static const float k_uv_edge[4]     = { 0.95f, 0.75f, 0.25f, 1.0f };
static const float k_uv_edge_sel[4] = { 1.00f, 0.40f, 0.10f, 1.0f };

static edit_mode_mesh* get_uv_target_mesh(system_scene_object** out_obj)
{
    system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (!o || o == &g_scene_registry.objects[0]) return NULL;                       /* nothing obj_vert_selected / world root */
    if (o->type == scene_object_camera || o->type == scene_object_light) return NULL;
    if (is_terrain_object(o)) return NULL;                                  /* terrain has no hand made UVs (triplanar) */
    if (out_obj) *out_obj = o;
    return em_registry_get_or_create(o);                                            /* same lookup Edit mode uses */
}

static void draw_uv_line(VkCommandBuffer cmd, const uv_rect* vp, float x0, float y0, float x1, float y1, const float c[4])
{
    const float dx = x1 - x0, dy = y1 - y0;
    const int steps = (int)(sqrtf(dx * dx + dy * dy) * 0.5f) + 1;                   /* one 2px dot every 2px */
    if (steps > 3000) return;                                                       /* zoomed in until one edge_to_face_array is thousands of px */
    for (int i = 0; i <= steps; i++) {
        const float t = (float)i / (float)steps;
        fill_view_rect(cmd, vp, x0 + dx * t - 1.0f, y0 + dy * t - 1.0f, 2.0f, 2.0f, c);
    }
}

/* returns the face face_type drawn (for the status line) */
static uint32_t draw_uv_mesh(VkCommandBuffer cmd, const uv_rect* vp)
{
    const edit_mode_mesh* mesh = get_uv_target_mesh(NULL);
    if (!mesh || mesh->face_count == 0 || mesh->face_count > uv_mesh_max_faces) return 0;

    for (uint32_t pass = 0; pass < 2; pass++) {                                     /* obj_vert_selected faces second, so they sit on top */
        for (uint32_t fi = 0; fi < mesh->face_count; fi++) {
            const edit_mode_face* f = &mesh->faces[fi];
            if ((f->obj_vert_selected != 0) != (pass == 1)) continue;
            const float* col = pass ? k_uv_edge_sel : k_uv_edge;
            const int n = f->face_type;
            for (int k = 0; k < n; k++) {
                const float* a = f->uv[k];
                const float* b = f->uv[(k + 1) % n];
                draw_uv_line(cmd, vp, convert_u_to_x(vp, a[0]), convert_v_to_y(vp, a[1]),
                                      convert_u_to_x(vp, b[0]), convert_v_to_y(vp, b[1]), col);
            }
        }
    }
    return mesh->face_count;
}

static void draw_uv_canvas(VkCommandBuffer cmd, system_element_context* ui_element, const uv_rect* vp)
{
    fill_view_rect(cmd, vp, (float)vp->x, (float)vp->y, (float)vp->w, (float)vp->h, k_uv_bg);

    /* the 0-1 tile */
    const float tx0 = convert_u_to_x(vp, 0.0f), tx1 = convert_u_to_x(vp, 1.0f);
    const float ty0 = convert_v_to_y(vp, 1.0f), ty1 = convert_v_to_y(vp, 0.0f);
    fill_view_rect(cmd, vp, tx0, ty0, tx1 - tx0, ty1 - ty0, k_uv_tile);

    if (g_mode_state.uv.show_checker) {
        const float cw = (tx1 - tx0) / (float)uv_checker_cells, ch = (ty1 - ty0) / (float)uv_checker_cells;
        for (int j = 0; j < uv_checker_cells; j++)
            for (int i = 0; i < uv_checker_cells; i++)
                fill_view_rect(cmd, vp, tx0 + cw * (float)i, ty0 + ch * (float)j, cw + 0.5f, ch + 0.5f,
                               ((i + j) & 1) ? k_uv_check_b : k_uv_check_a);
    }

    if (g_uv_view.show_grid) draw_uv_grid(cmd, vp);

    /* tile border */
    fill_view_rect(cmd, vp, tx0 - 1.0f, ty0 - 1.0f, tx1 - tx0 + 2.0f, 2.0f, k_uv_border);
    fill_view_rect(cmd, vp, tx0 - 1.0f, ty1 - 1.0f, tx1 - tx0 + 2.0f, 2.0f, k_uv_border);
    fill_view_rect(cmd, vp, tx0 - 1.0f, ty0 - 1.0f, 2.0f, ty1 - ty0 + 2.0f, k_uv_border);
    fill_view_rect(cmd, vp, tx1 - 1.0f, ty0 - 1.0f, 2.0f, ty1 - ty0 + 2.0f, k_uv_border);

    g_uv_view.faces_drawn = draw_uv_mesh(cmd, vp);       /* the obj_vert_selected object's face outlines in UV space */

    draw_uv_labels(cmd, ui_element, vp);
}

void draw_textureshop_editor_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    const int x = node->x, w = node->w;

    /* the sub menubar sits under the tab bar; if the dock already left room for it, content_y is already below it */
    const int sub_bottom = node->y + tab_bar_h + tab_menu_h;
    const int top        = (content_y > sub_bottom) ? content_y : sub_bottom;
    const int area_h     = content_y + content_h - top;
    if (w <= 0 || area_h <= uv_menu_h + uv_status_h + 8) return;

    const bool edge_to_face_array = ui_ctx->input.mouse_down && !g_uv_view.prev_down;
    g_uv_view.prev_down = ui_ctx->input.mouse_down;

    const int menu_y   = top;
    const int status_y = content_y + content_h - uv_status_h;
    const uv_rect vp = { x, menu_y + uv_menu_h, w, status_y - (menu_y + uv_menu_h) };

    if (!g_uv_view.ready) {
        g_uv_view.center_u = 0.5f;
        g_uv_view.center_v = 0.5f;
        g_uv_view.scale = fminf((float)vp.w, (float)vp.h) * 0.85f;
        if (g_uv_view.scale < 20.0f) g_uv_view.scale = 20.0f;
        g_uv_view.ready = true;
    }

    /* ---- tab sub menubar: the tool buttons ---- */
    draw_uv_sub_menu(cmd, ui_ctx, node, &vp, edge_to_face_array);

    /* ---- menu strip (labels only for now) ---- */
    push_quad_element(cmd, (float)x, (float)menu_y, (float)w, (float)uv_menu_h, system_theme.tab_content_window_textureshop_editor);
    {
        static const char* k_menu[] = { "Select", "Tool", "Create", "Edit", "Modify", "Image", "Textures", "Display" };
        int mx = x + 8;
        for (int i = 0; i < (int)(sizeof(k_menu) / sizeof(k_menu[0])); i++) {
            int tw = (int)strlen(k_menu[i]) * ui_font_w;
            if (mx + tw > x + w) break;
            draw_text_element(cmd, ui_ctx, k_menu[i], mx, menu_y + 3, system_color_light_gray_2);
            mx += tw + 16;
        }
    }

    /* ---- pan: middle mouse, or alt + left mouse like Maya ---- */
    {
        const int mx = ui_ctx->input.mouse_x, my = ui_ctx->input.mouse_y;
        const bool inside = validate_rectangle_hit(mx, my, vp.x, vp.y, vp.w, vp.h);
        const bool drag = ui_ctx->input.middle_down || (ui_ctx->input.alt_pressed && ui_ctx->input.mouse_down);
        if (drag && (g_uv_view.panning || inside)) {
            if (g_uv_view.panning) {
                g_uv_view.center_u -= (float)(mx - g_uv_view.prev_mouse_x) / g_uv_view.scale;
                g_uv_view.center_v += (float)(my - g_uv_view.prev_mouse_y) / g_uv_view.scale;
            }
            g_uv_view.panning = true;
        } else {
            g_uv_view.panning = false;
        }
        g_uv_view.prev_mouse_x = mx;
        g_uv_view.prev_mouse_y = my;
    }

    draw_uv_canvas(cmd, ui_ctx, &vp);

    /* right click menu (Skin: new / duplicate / delete), drawn on top later by draw_context_menu */
    context_menu_textureshop_window(ui_ctx, vp.x, vp.y, vp.w, vp.h);

    /* ---- status strip: U / V under the cursor, UDIM tile, zoom, texture size ---- */
    push_quad_element(cmd, (float)x, (float)status_y, (float)w, (float)uv_status_h, system_color_dark_gray_2);
    {
        char text[160];
        const int mx = ui_ctx->input.mouse_x, my = ui_ctx->input.mouse_y;
        const bool inside = validate_rectangle_hit(mx, my, vp.x, vp.y, vp.w, vp.h);
        int len = 0;
        if (inside) {
            const float u = g_uv_view.center_u + ((float)mx - ((float)vp.x + (float)vp.w * 0.5f)) / g_uv_view.scale;
            const float vert_to_edge_array = g_uv_view.center_v - ((float)my - ((float)vp.y + (float)vp.h * 0.5f)) / g_uv_view.scale;
            len = snprintf(text, sizeof(text), "U: %.4f   V: %.4f", (double)u, (double)vert_to_edge_array);
            if (u >= 0.0f && vert_to_edge_array >= 0.0f && u < 10.0f)
                len += snprintf(text + len, sizeof(text) - (size_t)len, "   Tile: %d", 1001 + (int)floorf(u) + 10 * (int)floorf(vert_to_edge_array));
        } else {
            len = snprintf(text, sizeof(text), "U: -   V: -");
        }
        len += snprintf(text + len, sizeof(text) - (size_t)len, "   Zoom: %.0f px", (double)g_uv_view.scale);
        if (g_mode_state.uv.texture_size > 0)
            len += snprintf(text + len, sizeof(text) - (size_t)len, "   Texture: %d", g_mode_state.uv.texture_size);
        if (g_uv_view.faces_drawn > 0) snprintf(text + len, sizeof(text) - (size_t)len, "   Faces: %u", (unsigned)g_uv_view.faces_drawn);
        else                           snprintf(text + len, sizeof(text) - (size_t)len, "   Select a mesh object in the Hierarchy");
        draw_text_element(cmd, ui_ctx, text, x + 8, status_y + 3, system_color_light_gray_2);
    }
}