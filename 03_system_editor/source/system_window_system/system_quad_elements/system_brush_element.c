#include "system_globals.h"

extern void build_scene_view_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj);

/* ------------------------------------------------------------------------- */
/* frame                                                                     */
/* ------------------------------------------------------------------------- */
static unsigned s_frame = 0;

unsigned brush_frame_id(void) { return s_frame; }

/* ------------------------------------------------------------------------- */
/* brush cursor                                                              */
/* ------------------------------------------------------------------------- */
static struct {
    bool       on;
    float      px, hard;
    float      ring[3], swatch[3];
    bool       has_swatch;
    ULONGLONG  stamp;
} s_bc;

bool brush_cursor_is_on(void)
{
    /* stamp check: if the Scene view stops drawing (tab hidden) the arrow must come back */
    return s_bc.on && (GetTickCount64() - s_bc.stamp) < 120ull;
}

void brush_cursor_set(const system_element_context* ui_element, editor_vec3 w, float radius_m, float hardness,
                      const float* ring_rgb, const float* swatch_rgb)
{
    const struct_viewport_bounds* b = &ui_element->scene_view;
    if (b->width <= 0 || b->height <= 0) return;

    editor_mat4 view, proj;
    build_scene_view_camera((float)b->width / (float)b->height, &view, &proj);
    float depth = fabsf(view.mesh[0][2] * w.x + view.mesh[1][2] * w.y + view.mesh[2][2] * w.z + view.mesh[3][2]);
    if (depth < 0.01f) depth = 0.01f;

    s_bc.px   = radius_m * fabsf(proj.mesh[1][1]) / depth * 0.5f * (float)b->height;
    s_bc.hard = hardness;
    for (int i = 0; i < 3; i++) s_bc.ring[i] = ring_rgb ? ring_rgb[i] : 1.0f;
    s_bc.has_swatch = swatch_rgb != NULL;
    if (swatch_rgb) for (int i = 0; i < 3; i++) s_bc.swatch[i] = swatch_rgb[i];
    s_bc.on    = true;
    s_bc.stamp = GetTickCount64();
}

static void ring_dots(VkCommandBuffer cmd, const struct_viewport_bounds* vert_to_edge_array, int cx, int cy, float r, system_color c)
{
    int n = (int)(r * 1.2f);
    if (n < 32) n = 32;
    if (n > 720) n = 720;
    for (int i = 0; i < n; i++) {
        const float a = 6.2831853f * (float)i / (float)n;
        const int x = cx + (int)(cosf(a) * r), y = cy + (int)(sinf(a) * r);
        if (x < vert_to_edge_array->offset_x || y < vert_to_edge_array->offset_y || x >= vert_to_edge_array->offset_x + vert_to_edge_array->width - 2 || y >= vert_to_edge_array->offset_y + vert_to_edge_array->height - 2) continue;
        draw_icon_element(cmd, x, y, 2, 2, c);
    }
}

void draw_brush_cursor(VkCommandBuffer cmd, const system_element_context* ui_element)
{
    if (!brush_cursor_is_on()) return;
    const struct_viewport_bounds* vert_to_edge_array = &ui_element->scene_view;
    float r = s_bc.px;
    if (r < 4.0f) r = 4.0f;
    if (r > 1500.0f) r = 1500.0f;
    const int cx = ui_element->input.mouse_x, cy = ui_element->input.mouse_y;

    const system_color shadow = { 0.0f, 0.0f, 0.0f, 0.55f };
    const system_color ring   = { s_bc.ring[0], s_bc.ring[1], s_bc.ring[2], 0.95f };
    const system_color inner  = { s_bc.ring[0], s_bc.ring[1], s_bc.ring[2], 0.50f };

    ring_dots(cmd, vert_to_edge_array, cx + 1, cy + 1, r, shadow);
    ring_dots(cmd, vert_to_edge_array, cx, cy, r, ring);
    if (s_bc.hard > 0.05f && s_bc.hard < 0.95f && r > 12.0f) ring_dots(cmd, vert_to_edge_array, cx, cy, r * s_bc.hard, inner);   /* full strength inside this ring */

    draw_icon_element(cmd, cx - 3, cy - 3, 6, 6, shadow);
    if (s_bc.has_swatch) {
        const system_color sw = { s_bc.swatch[0], s_bc.swatch[1], s_bc.swatch[2], 1.0f };
        draw_icon_element(cmd, cx - 2, cy - 2, 4, 4, sw);
    } else {
        draw_icon_element(cmd, cx - 2, cy - 2, 4, 4, ring);
    }
}

/* ------------------------------------------------------------------------- */
/* tab sub menubar buttons (same look and spacing as the Scene tab)           */
/* ------------------------------------------------------------------------- */
int brush_bar_bottom(const system_dock_node* node, int content_y)
{
    const int below = node->y + tab_bar_h + tab_menu_h;
    return below > content_y ? below : content_y;
}

void brush_bar_begin(brush_bar* b, VkCommandBuffer cmd, system_element_context* ui_element, const system_dock_node* node, uint32_t id_base)
{
    memset(b, 0, sizeof(*b));
    b->cmd = cmd; b->ui_element = ui_element; b->id_base = id_base;

    const int x = node->x, y = node->y + tab_bar_h, w = node->w, h = tab_menu_h;
    draw_tab_sub_menubar(cmd, ui_element, x, y, w, h);

    int s = h - 5;                      /* h includes the 1px divider on the bottom row */
    if (s > 22) s = 22;
    if (s < 12) return;

    b->ok    = true;
    b->s     = s;
    b->x     = x + 6;
    b->y     = y + (h - 1 - s) / 2;
    b->right = x + w - 6;
}

bool brush_bar_button(brush_bar* b, const char* label, const char* name, bool on)
{
    if (!b->ok) return false;
    system_element_context* ui_element = b->ui_element;

    int w = (int)strlen(label) * ui_font_w + 8;
    if (w < b->s) w = b->s;
    if (b->x + w > b->right) return false;

    const int x = b->x, y = b->y, s = b->s;
    const uint32_t id = dock_tab_id + b->id_base + (uint32_t)b->face_type;
    b->face_type++;
    b->x += w + 2;

    const bool hovered = ui_element->input.mouse_x >= x && ui_element->input.mouse_x < x + w && ui_element->input.mouse_y >= y && ui_element->input.mouse_y < y + s;
    if (hovered) b->hover_name = name;

    if (on) {
        draw_icon_element(b->cmd, x, y, w, s, color_with_alpha(system_theme.system_accent, 0.60f));
        draw_icon_border (b->cmd, x, y, w, s, system_theme.system_accent);
    } else if (hovered) {
        draw_icon_element(b->cmd, x, y, w, s, process_theme_color(system_theme.tab_sub_menubar, system_theme.tab_hover_delta));
    }

    const int tw = (int)strlen(label) * ui_font_w;
    draw_text_element(b->cmd, ui_element, label, x + (w - tw) / 2, y + (s - ui_font_h) / 2,
                      (on || hovered) ? system_theme.tab_item_text_active : system_theme.tab_item_text);

    bool clicked = false;
    if (ui_element->input.mouse_down && hovered && ui_element->active_id == 0 && !ui_element->input.camera_manipulating) ui_element->active_id = id;
    if (!ui_element->input.mouse_down && ui_element->active_id == id) {
        ui_element->active_id = 0;
        clicked = hovered;
    }
    return clicked;
}

void brush_bar_separator(brush_bar* b)
{
    if (!b->ok || b->face_type == 0) return;
    if (b->x + 7 + b->s > b->right) return;
    draw_icon_element(b->cmd, b->x + 2, b->y + 2, 1, b->s - 4, system_theme.tab_sub_menubar_divider);
    b->x += 7;
}

void brush_bar_end(brush_bar* b)
{
    if (!b->ok || !b->hover_name) return;
    const int tw = (int)strlen(b->hover_name) * ui_font_w;
    const int tx = b->right - tw;
    if (tx > b->x + 8) draw_text_element(b->cmd, b->ui_element, b->hover_name, tx, b->y + (b->s - ui_font_h) / 2, system_theme.tab_item_text);
}

/* ------------------------------------------------------------------------- */
/* once per frame                                                            */
/* ------------------------------------------------------------------------- */
void brush_ui_frame(system_element_context* ui_element)
{
    s_frame++;
    s_bc.on = false;
    if (!ui_element) return;
    brush_follow_modes();         /* entering Paint mode opens the Brushes window */
    sculpt_input_update(ui_element);      /* mesh obj_vert_selected */
    paint_input_update(ui_element);
}

/* ========================================================================= */
/* windows                                                                   */
/* ========================================================================= */
extern uint32_t g_ui_frame_index;      /* system_element.c, ticks once per UI frame */

void brush_open_window(void)
{
    /* same spot the Sc button uses: tabbed next to the Hierarchy. already docked = just brought to the front. */
    spawn_dock_view_beside(&dock_workspace, view_type_hierarchy, view_type_inspector, view_type_brush);
}

void brush_follow_modes(void)
{
    static uint32_t s_last_frame = 0xFFFFFFFFu;
    static bool     s_prev_paint = false;
    if (s_last_frame == g_ui_frame_index) return;       /* once per UI frame, whoever calls first */
    s_last_frame = g_ui_frame_index;

    const bool paint = paint_mode_is_active();
    if (paint && !s_prev_paint) brush_open_window();    /* Pt pressed: the paint brushes are in the Brushes window */
    s_prev_paint = paint;
}

/* ========================================================================= */
/* mesh detail                                                               */
/* ========================================================================= */
int brush_densify_selected(uint32_t target_faces)
{
    uint32_t faces = 0;
    if (!sculpt_selected_mesh_counts(NULL, &faces)) {
        snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Select a mesh object first.");
        return 0;
    }
    int added = 0;
    while (added < (int)sculpt_max_levels) {
        if (!sculpt_selected_mesh_counts(NULL, &faces) || faces >= target_faces) break;
        if (!sculpt_subdivide_selected()) break;
        added++;
    }
    sculpt_selected_mesh_counts(NULL, &faces);
    if (added > 0) snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Subdivided %d level(s): %u faces.", added, (unsigned)faces);
    else           snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "No more subdividing possible (%u faces, limit reached).", (unsigned)faces);
    return added;
}

/* ========================================================================= */
/* colour math                                                               */
/* ========================================================================= */
static float clamp01f(float vert_to_edge_array) { return vert_to_edge_array < 0.0f ? 0.0f : (vert_to_edge_array > 1.0f ? 1.0f : vert_to_edge_array); }

static void hsv_to_rgb(float h, float s, float vert_to_edge_array, float out[3])
{
    h -= floorf(h);
    const float hh = h * 6.0f;
    const int   i  = (int)hh;
    const float f  = hh - (float)i;
    const float p = vert_to_edge_array * (1.0f - s), q = vert_to_edge_array * (1.0f - s * f), t = vert_to_edge_array * (1.0f - s * (1.0f - f));
    switch (i % 6) {
    case 0:  out[0] = vert_to_edge_array; out[1] = t; out[2] = p; break;
    case 1:  out[0] = q; out[1] = vert_to_edge_array; out[2] = p; break;
    case 2:  out[0] = p; out[1] = vert_to_edge_array; out[2] = t; break;
    case 3:  out[0] = p; out[1] = q; out[2] = vert_to_edge_array; break;
    case 4:  out[0] = t; out[1] = p; out[2] = vert_to_edge_array; break;
    default: out[0] = vert_to_edge_array; out[1] = p; out[2] = q; break;
    }
}

/* *h is left alone for greys, so the hue marker does not jump to red */
static void rgb_to_hsv(const float c[3], float* h, float* s, float* vert_to_edge_array)
{
    const float mx = fmaxf(c[0], fmaxf(c[1], c[2])), mn = fminf(c[0], fminf(c[1], c[2]));
    const float d = mx - mn;
    *vert_to_edge_array = mx;
    *s = mx > 1e-6f ? d / mx : 0.0f;
    if (d < 1e-6f) return;
    float hh;
    if (mx == c[0])      hh = fmodf((c[1] - c[2]) / d, 6.0f);
    else if (mx == c[1]) hh = (c[2] - c[0]) / d + 2.0f;
    else                 hh = (c[0] - c[1]) / d + 4.0f;
    hh /= 6.0f;
    if (hh < 0.0f) hh += 1.0f;
    *h = hh;
}

/* picker state: HSV is kept here so greys and black do not lose their hue */
static struct {
    float h, s, vert_to_edge_array;
    float last_rgb[3];
    bool  init;
    bool  del_mode;
} s_cp = { 0.0f, 1.0f, 1.0f, { 0, 0, 0 }, false, false };

void brush_color_shift(float rgb[3], float dv)
{
    float h = s_cp.h, s, vert_to_edge_array;
    rgb_to_hsv(rgb, &h, &s, &vert_to_edge_array);
    vert_to_edge_array = clamp01f(vert_to_edge_array + dv);
    hsv_to_rgb(h, s, vert_to_edge_array, rgb);
}

/* ========================================================================= */
/* palette + saved colours                                                   */
/* ========================================================================= */
void brush_palette_color(int index, float out[3])
{
    static const float k_earth[12][3] = {
        { 0.99f, 0.87f, 0.77f }, { 0.96f, 0.76f, 0.62f }, { 0.87f, 0.64f, 0.49f }, { 0.76f, 0.52f, 0.37f },
        { 0.62f, 0.41f, 0.28f }, { 0.47f, 0.30f, 0.20f }, { 0.33f, 0.21f, 0.14f }, { 0.60f, 0.52f, 0.34f },
        { 0.42f, 0.52f, 0.26f }, { 0.25f, 0.38f, 0.22f }, { 0.35f, 0.45f, 0.58f }, { 0.55f, 0.55f, 0.60f }
    };
    if (index < 0) index = 0;
    if (index >= brush_palette_count) index = brush_palette_count - 1;
    const int row = index / 12, col = index % 12;
    switch (row) {
    case 0:  { const float g = 1.0f - (float)col / 11.0f; out[0] = out[1] = out[2] = g; } break;   /* white -> black */
    case 1:  hsv_to_rgb((float)col / 12.0f, 1.0f,  1.0f,  out); break;                              /* pure */
    case 2:  hsv_to_rgb((float)col / 12.0f, 0.45f, 1.0f,  out); break;                              /* light */
    case 3:  hsv_to_rgb((float)col / 12.0f, 1.0f,  0.55f, out); break;                              /* dark */
    default: out[0] = k_earth[col][0]; out[1] = k_earth[col][1]; out[2] = k_earth[col][2]; break;   /* skin / earth */
    }
}

static float s_saved[brush_saved_max][3];
static int   s_saved_n      = 0;
static bool  s_saved_loaded = false;
static int   s_saved_cycle  = -1;

#define SAVED_KEY_COLORS "paint.saved_colors"
#define SAVED_KEY_COUNT  "paint.saved_count"

static void saved_load(void)
{
    if (s_saved_loaded) return;
    s_saved_loaded = true;
    int n = 0;
    float f[brush_saved_max * 3];
    if (settings_db_get_int(SAVED_KEY_COUNT, &n) && n > 0 && n <= brush_saved_max && settings_db_get_floats(SAVED_KEY_COLORS, f, n * 3)) {
        for (int i = 0; i < n; i++) for (int c = 0; c < 3; c++) s_saved[i][c] = clamp01f(f[i * 3 + c]);
        s_saved_n = n;
    }
}

static void saved_write(void)
{
    float f[brush_saved_max * 3];
    for (int i = 0; i < s_saved_n; i++) for (int c = 0; c < 3; c++) f[i * 3 + c] = s_saved[i][c];
    settings_db_set_int(SAVED_KEY_COUNT, s_saved_n);
    if (s_saved_n > 0) settings_db_set_floats(SAVED_KEY_COLORS, f, s_saved_n * 3);
    settings_db_save();
}

int  brush_saved_color_count(void) { saved_load(); return s_saved_n; }

bool brush_saved_color_get(int index, float out[3])
{
    saved_load();
    if (index < 0 || index >= s_saved_n) return false;
    for (int c = 0; c < 3; c++) out[c] = s_saved[index][c];
    return true;
}

void brush_saved_color_add(const float rgb[3])
{
    saved_load();
    for (int i = 0; i < s_saved_n; i++) {            /* already saved: nothing to do */
        if (fabsf(s_saved[i][0] - rgb[0]) < 0.003f && fabsf(s_saved[i][1] - rgb[1]) < 0.003f && fabsf(s_saved[i][2] - rgb[2]) < 0.003f) {
            snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Colour is already saved (slot %d).", i + 1);
            return;
        }
    }
    if (s_saved_n >= brush_saved_max) {              /* full: the oldest one goes */
        for (int i = 1; i < brush_saved_max; i++) for (int c = 0; c < 3; c++) s_saved[i - 1][c] = s_saved[i][c];
        s_saved_n = brush_saved_max - 1;
    }
    for (int c = 0; c < 3; c++) s_saved[s_saved_n][c] = clamp01f(rgb[c]);
    s_saved_n++;
    saved_write();
    snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Colour saved (slot %d of %d).", s_saved_n, (int)brush_saved_max);
}

void brush_saved_color_remove(int index)
{
    saved_load();
    if (index < 0 || index >= s_saved_n) return;
    for (int i = index + 1; i < s_saved_n; i++) for (int c = 0; c < 3; c++) s_saved[i - 1][c] = s_saved[i][c];
    s_saved_n--;
    saved_write();
}

bool brush_saved_color_cycle(int dir, float rgb[3])
{
    saved_load();
    if (s_saved_n <= 0) { snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "No saved colours yet. Use Save in the Paint window."); return false; }
    s_saved_cycle = (s_saved_cycle + (dir >= 0 ? 1 : -1) + s_saved_n) % s_saved_n;
    for (int c = 0; c < 3; c++) rgb[c] = s_saved[s_saved_cycle][c];
    return true;
}

/* ========================================================================= */
/* sliders                                                                   */
/* ========================================================================= */
enum { ui_id_base = 0x5B1D0000u };

static struct { float t0; int anchor_x; bool fine; } s_sl;

bool brush_slider(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id_small, const char* label,
                  int x, int y, int w, int h, float* value, float lo, float hi, bool log_scale, const char* fmt)
{
    if (!ui_element || !value || !label || hi <= lo) return false;
    if (log_scale && lo <= 0.0f) log_scale = false;

    const uint32_t id = (uint32_t)ui_id_base + id_small;

    int lw = (int)strlen(label) * ui_font_w + 8;
    if (lw > w / 2) lw = w / 2;
    const int tx = x + lw, tw = w - lw;
    if (tw < 24 || h < 8) return false;

    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    const bool hovered = validate_rectangle_hit(mx, my, tx, y, tw, h);
    const bool down    = ui_element->input.mouse_down;
    const bool shift   = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    float vert_to_edge_array = *value < lo ? lo : (*value > hi ? hi : *value);
    float t = log_scale ? logf(vert_to_edge_array / lo) / logf(hi / lo) : (vert_to_edge_array - lo) / (hi - lo);
    t = clamp01f(t);

    /* a window that stopped drawing mid drag must not leave the mouse locked */
    if (ui_element->active_id == id && !down) ui_element->active_id = 0;

    if (down && hovered && ui_element->active_id == 0 && !ui_element->input.camera_manipulating) {
        ui_element->active_id = id;
        s_sl.anchor_x = mx;
        s_sl.fine     = shift;
        s_sl.t0       = shift ? t : clamp01f((float)(mx - tx) / (float)(tw - 1));   /* plain click jumps there, Shift+click does not */
    }

    bool changed = false;
    if (ui_element->active_id == id && down) {
        if (shift != s_sl.fine) { s_sl.t0 = t; s_sl.anchor_x = mx; s_sl.fine = shift; }  /* Shift pressed or released mid drag: no jump */
        const float nt = clamp01f(s_sl.t0 + (float)(mx - s_sl.anchor_x) * (shift ? 0.1f : 1.0f) / (float)(tw - 1));
        const float nv = log_scale ? lo * expf(nt * logf(hi / lo)) : lo + nt * (hi - lo);
        if (nv != *value) { *value = nv; changed = true; }
        t = nt;
    }
    const bool active = (ui_element->active_id == id);

    /* draw */
    draw_text_element(cmd, ui_element, label, x, y + (h - ui_font_h) / 2, system_color_light_gray_2);
    push_quad_element(cmd, (float)tx, (float)y, (float)tw, (float)h, system_theme.strip_button);
    const int fw = (int)(t * (float)(tw - 2));
    if (fw > 0) push_quad_element(cmd, (float)(tx + 1), (float)(y + 1), (float)fw, (float)(h - 2), color_with_alpha(system_theme.system_accent, 0.65f));
    draw_icon_border(cmd, tx, y, tw, h, (active || hovered) ? system_theme.system_accent : system_theme.system_border);

    char buf[48];
    snprintf(buf, sizeof(buf), fmt ? fmt : "%.2f", (double)(*value));
    const int vw = (int)strlen(buf) * ui_font_w;
    draw_text_element(cmd, ui_element, buf, tx + (tw - vw) / 2, y + (h - ui_font_h) / 2, system_theme.tab_item_text_active);
    return changed;
}

bool brush_slider_int(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id, const char* label,
                      int x, int y, int w, int h, int* value, int lo, int hi)
{
    float f = (float)*value;
    const bool c = brush_slider(cmd, ui_element, id, label, x, y, w, h, &f, (float)lo, (float)hi, false, "%.0f");
    if (c) {
        const int nv = (int)(f + 0.5f);
        if (nv != *value) { *value = nv; return true; }
    }
    return false;
}

/* ========================================================================= */
/* colour picker                                                             */
/* ========================================================================= */
enum { cp_id_sv = 0x300, cp_id_hue = 0x301 };

/* one mouse press edge_to_face_array per UI frame, shared by the picker and the palette */
static bool cp_press(const system_element_context* uic)
{
    static uint32_t s_f = 0xFFFFFFFFu;
    static bool     s_p = false, s_prev = false;
    system_element_context* ui_element = (system_element_context*)uic;
    if (s_f != g_ui_frame_index) {
        s_f = g_ui_frame_index;
        const bool d = ui_element->input.mouse_down;
        s_p    = d && !s_prev && ui_element->active_id == 0 && !ui_element->input.camera_manipulating;
        s_prev = d;
    }
    return s_p;
}

static bool cp_button(VkCommandBuffer cmd, system_element_context* ui_element, bool press, const char* label, int x, int y, int w, int h, bool sel)
{
    const bool hov = validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, x, y, w, h);
    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h,
                      sel ? system_theme.strip_button_selected : (hov ? system_theme.tab_item : system_theme.strip_button));
    const int tw = (int)strlen(label) * ui_font_w;
    draw_text_element(cmd, ui_element, label, x + (w - tw) / 2, y + (h - ui_font_h) / 2, system_theme.button_text);
    return hov && press;
}

int brush_color_picker(VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, int w, int h_avail, float rgb[3])
{
    saved_load();
    const bool press = cp_press(ui_element);
    const int  row_h = 22;

    int sv_h = 96;
    if (sv_h > h_avail - row_h - 6) sv_h = h_avail - row_h - 6;
    if (sv_h < 40 || w < 90) return 0;

    const int hue_w = 14, gap = 6;
    int sv_w = w - hue_w - gap;
    if (sv_w > 220) sv_w = 220;
    const int hue_x = x + sv_w + gap;

    /* outside changes (eyedropper, context menu, saved colour) move the markers */
    if (!s_cp.init || fabsf(rgb[0] - s_cp.last_rgb[0]) > 1e-4f || fabsf(rgb[1] - s_cp.last_rgb[1]) > 1e-4f || fabsf(rgb[2] - s_cp.last_rgb[2]) > 1e-4f) {
        rgb_to_hsv(rgb, &s_cp.h, &s_cp.s, &s_cp.vert_to_edge_array);
        s_cp.init = true;
    }

    /* ---- input ---- */
    const uint32_t id_sv = (uint32_t)ui_id_base + cp_id_sv, id_hue = (uint32_t)ui_id_base + cp_id_hue;
    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    const bool down = ui_element->input.mouse_down;

    if ((ui_element->active_id == id_sv || ui_element->active_id == id_hue) && !down) ui_element->active_id = 0;
    if (press) {
        if (validate_rectangle_hit(mx, my, x, y, sv_w, sv_h))        ui_element->active_id = id_sv;
        else if (validate_rectangle_hit(mx, my, hue_x, y, hue_w, sv_h)) ui_element->active_id = id_hue;
    }
    bool edited = false;
    if (ui_element->active_id == id_sv && down) {
        s_cp.s = clamp01f((float)(mx - x) / (float)(sv_w - 1));
        s_cp.vert_to_edge_array = 1.0f - clamp01f((float)(my - y) / (float)(sv_h - 1));
        edited = true;
    } else if (ui_element->active_id == id_hue && down) {
        s_cp.h = clamp01f((float)(my - y) / (float)(sv_h - 1)) * 0.9999f;
        edited = true;
    }
    if (edited) hsv_to_rgb(s_cp.h, s_cp.s, s_cp.vert_to_edge_array, rgb);
    for (int c = 0; c < 3; c++) s_cp.last_rgb[c] = rgb[c];

    /* ---- saturation / value square ---- */
    const int cols = 20, rows = 14;
    for (int r = 0; r < rows; r++) {
        const int y0 = y + (r * sv_h) / rows, y1 = y + ((r + 1) * sv_h) / rows;
        for (int c = 0; c < cols; c++) {
            const int x0 = x + (c * sv_w) / cols, x1 = x + ((c + 1) * sv_w) / cols;
            float cc[3];
            hsv_to_rgb(s_cp.h, ((float)c + 0.5f) / (float)cols, 1.0f - ((float)r + 0.5f) / (float)rows, cc);
            push_quad_element(cmd, (float)x0, (float)y0, (float)(x1 - x0), (float)(y1 - y0), (system_color){ cc[0], cc[1], cc[2], 1.0f });
        }
    }
    {   /* marker */
        const int px = x + (int)(s_cp.s * (float)(sv_w - 1)), py = y + (int)((1.0f - s_cp.vert_to_edge_array) * (float)(sv_h - 1));
        draw_icon_border(cmd, px - 4, py - 4, 9, 9, (system_color){ 0.0f, 0.0f, 0.0f, 0.9f });
        draw_icon_border(cmd, px - 3, py - 3, 7, 7, system_color_pure_white);
    }
    draw_icon_border(cmd, x, y, sv_w, sv_h, system_theme.system_border);

    /* ---- hue bar ---- */
    const int hue_cells = 36;
    for (int i = 0; i < hue_cells; i++) {
        const int y0 = y + (i * sv_h) / hue_cells, y1 = y + ((i + 1) * sv_h) / hue_cells;
        float cc[3];
        hsv_to_rgb(((float)i + 0.5f) / (float)hue_cells, 1.0f, 1.0f, cc);
        push_quad_element(cmd, (float)hue_x, (float)y0, (float)hue_w, (float)(y1 - y0), (system_color){ cc[0], cc[1], cc[2], 1.0f });
    }
    {
        const int hy = y + (int)(s_cp.h * (float)(sv_h - 1));
        draw_icon_border(cmd, hue_x - 1, hy - 2, hue_w + 2, 5, (system_color){ 0.0f, 0.0f, 0.0f, 0.9f });
        draw_icon_border(cmd, hue_x,     hy - 1, hue_w,     3, system_color_pure_white);
    }

    /* ---- preview, hex, Save, Del ---- */
    const int ry = y + sv_h + 6;
    push_quad_element(cmd, (float)x, (float)ry, 28.0f, (float)(row_h - 2), (system_color){ rgb[0], rgb[1], rgb[2], 1.0f });
    draw_icon_border(cmd, x, ry, 28, row_h - 2, system_theme.system_border);

    char hex[16];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", (int)(clamp01f(rgb[0]) * 255.0f + 0.5f), (int)(clamp01f(rgb[1]) * 255.0f + 0.5f), (int)(clamp01f(rgb[2]) * 255.0f + 0.5f));

    const int save_w = 4 * ui_font_w + 8, del_w = 3 * ui_font_w + 8;
    const int bx_del = x + w - del_w, bx_save = bx_del - 4 - save_w;
    if (bx_save > x + 32 + 7 * ui_font_w + 4)
        draw_text_element(cmd, ui_element, hex, x + 34, ry + (row_h - 2 - ui_font_h) / 2, system_color_light_gray_2);
    if (bx_save > x + 34) {
        if (cp_button(cmd, ui_element, press, "Save", bx_save, ry, save_w, row_h - 2, false)) brush_saved_color_add(rgb);
        if (cp_button(cmd, ui_element, press, "Del",  bx_del,  ry, del_w,  row_h - 2, s_cp.del_mode)) s_cp.del_mode = !s_cp.del_mode;
    }
    return sv_h + 6 + row_h;
}

int brush_color_palette(VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, int w, int h_avail, float rgb[3])
{
    saved_load();
    const bool press = cp_press(ui_element);
    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;

    const int cols = 12, g = 2;
    int cell = (w - (cols - 1) * g) / cols;
    if (cell > 20) cell = 20;
    if (cell < 7) return 0;

    int cy = y;
    const int limit = y + h_avail;

    /* ---- saved ---- */
    const int saved_rows = (brush_saved_max + cols - 1) / cols;
    if (cy + 12 + saved_rows * (cell + g) > limit) goto palette;
    draw_text_element(cmd, ui_element, s_cp.del_mode ? "Saved (click one to delete it)" : "Saved colours", x, cy, s_cp.del_mode ? system_color_amber : system_color_near_white);
    cy += 12;
    for (int i = 0; i < brush_saved_max; i++) {
        const int sx = x + (i % cols) * (cell + g), sy = cy + (i / cols) * (cell + g);
        const bool filled = i < s_saved_n;
        const bool hov = validate_rectangle_hit(mx, my, sx, sy, cell, cell);
        if (filled) push_quad_element(cmd, (float)sx, (float)sy, (float)cell, (float)cell, (system_color){ s_saved[i][0], s_saved[i][1], s_saved[i][2], 1.0f });
        else        push_quad_element(cmd, (float)sx, (float)sy, (float)cell, (float)cell, system_theme.strip_button);
        draw_icon_border(cmd, sx, sy, cell, cell, (hov && filled) ? (s_cp.del_mode ? system_color_red : system_color_pure_white) : system_theme.system_border);
        if (filled && hov && press) {
            if (s_cp.del_mode) { brush_saved_color_remove(i); break; }
            for (int c = 0; c < 3; c++) rgb[c] = s_saved[i][c];
        }
    }
    cy += saved_rows * (cell + g) + 6;

palette:
    /* ---- presets ---- */
    {
        const int rows = brush_palette_count / cols;
        if (cy + 12 + rows * (cell + g) > limit) return cy - y;
        draw_text_element(cmd, ui_element, "Colours", x, cy, system_color_near_white);
        cy += 12;
        for (int i = 0; i < brush_palette_count; i++) {
            const int sx = x + (i % cols) * (cell + g), sy = cy + (i / cols) * (cell + g);
            float cc[3];
            brush_palette_color(i, cc);
            const bool hov = validate_rectangle_hit(mx, my, sx, sy, cell, cell);
            push_quad_element(cmd, (float)sx, (float)sy, (float)cell, (float)cell, (system_color){ cc[0], cc[1], cc[2], 1.0f });
            draw_icon_border(cmd, sx, sy, cell, cell, hov ? system_color_pure_white : system_theme.system_border);
            if (hov && press) for (int c = 0; c < 3; c++) rgb[c] = cc[c];
        }
        cy += rows * (cell + g);
    }
    return cy - y;
}