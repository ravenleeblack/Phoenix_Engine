#include "system_globals.h"

manip_state g_manip = {
    .show_manipulator  = false,
    .snap              = manip_snap_off,
    .grid_size         = 1.0f,
    .point_snap_radius = 0.75f,
    .edit_tool         = manip_edit_none,
};

extern uint32_t g_ui_frame_index;      /* system_element.c */


/* drag step per pixel / rounding step. 0.0625 = 1/16 so building lands on 0.0625, 0.1250, 0.2500 ...  Shift = fine, no rounding. */
static const number_field_opts k_pos_opts  = { .step = 0.03125f, .quantum = 0.0625f };
static const number_field_opts k_rot_opts  = { .step = 0.5f,     .quantum = 0.5f, .display_scale = 57.29578f };   /* radians stored, degrees shown */
static const number_field_opts k_scl_opts  = { .step = 0.03125f, .quantum = 0.0625f };
static const number_field_opts k_size_opts = { .step = 0.03125f, .quantum = 0.0625f, .min = 0.001f, .max = 1000000.0f };

#define RAD_TO_DEG 57.29578f
#define DEG_TO_RAD 0.01745329f

/* ------------------------------------------------------------------------- */
/* state changes                                                              */
/* ------------------------------------------------------------------------- */
void manip_set_show(system_element_context* ui_element, bool on)
{
    g_manip.show_manipulator = on;
    if (on && ui_element) ui_element->active_tool = tool_select;     /* the plain gizmo handles hide, the manipulator takes over */
}

void manip_set_snap(manip_snap_mode mode)
{
    g_manip.snap = mode;
    phoenix_info("Manip", "snap mode %d (0 off, 1 point to point, 2 point to grid)", (int)mode);
}

void manip_scale_grid(float factor)
{
    float g = g_manip.grid_size * factor;
    if (g < 0.125f) g = 0.125f;
    if (g > 16.0f)  g = 16.0f;
    g_manip.grid_size = g;
    phoenix_info("Manip", "grid size %.3f", g);
}

void manip_toggle_edit_tool(manip_edit_tool tool)
{
    g_manip.edit_tool = (g_manip.edit_tool == tool) ? manip_edit_none : tool;
}

/* ------------------------------------------------------------------------- */
/* snapping                                                                   */
/* ------------------------------------------------------------------------- */
editor_vec3 manip_snap_position(editor_vec3 p, uint32_t moving_object_id)
{
    if (g_manip.snap == manip_snap_point_to_grid) {
        const float g = g_manip.grid_size;
        p.x = roundf(p.x / g) * g;
        p.y = roundf(p.y / g) * g;
        p.z = roundf(p.z / g) * g;
    }
    else if (g_manip.snap == manip_snap_point_to_point) {
        float best = g_manip.point_snap_radius * g_manip.point_snap_radius;
        const system_scene_object* hit = NULL;

        for (uint32_t i = 1; i < g_scene_registry.object_count; i++) {     /* 0 = root */
            const system_scene_object* o = &g_scene_registry.objects[i];
            if (o->id == moving_object_id) continue;
            const float dx = o->position.x - p.x, dy = o->position.y - p.y, dz = o->position.z - p.z;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best) { best = d2; hit = o; }
        }
        if (hit) p = hit->position;
    }
    return p;
}

/* ------------------------------------------------------------------------- */
/* Target Weld core                                                           */
/* ------------------------------------------------------------------------- */
void manip_weld_indices(uint32_t* indices, uint32_t* index_count, uint32_t src, uint32_t dst)
{
    if (!indices || !index_count) return;

    uint32_t out = 0;
    for (uint32_t t = 0; t + 2 < *index_count; t += 3) {
        const uint32_t a = (indices[t]     == src) ? dst : indices[t];
        const uint32_t b = (indices[t + 1] == src) ? dst : indices[t + 1];
        const uint32_t c = (indices[t + 2] == src) ? dst : indices[t + 2];
        if (a == b || b == c || a == c) continue;                  /* collapsed triangle */
        indices[out++] = a; indices[out++] = b; indices[out++] = c;   /* out <= t, safe in place */
    }
    *index_count = out;
}

/* ------------------------------------------------------------------------- */
/* per frame input edges (computed once, however many scene views draw)       */
/* ------------------------------------------------------------------------- */
static uint32_t s_frame     = 0xFFFFFFFFu;
static bool     s_edge      = false;     /* left button went down this frame */
static bool     s_prev_down = false;
static bool     s_prev_t    = false;
static bool     s_prev_esc  = false;
static bool     s_over      = false;     /* cursor over the tool box / panel this frame */

static void begin_frame_once(system_element_context* ui_element, int vx, int vy, int vw, int vh)
{
    if (s_frame == g_ui_frame_index) return;
    s_frame = g_ui_frame_index;

    s_edge      = ui_element->input.mouse_down && !s_prev_down;
    s_prev_down = ui_element->input.mouse_down;

    const bool t_now   = (GetAsyncKeyState('T')       & 0x8000) != 0;
    const bool esc_now = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    const bool t_edge   = t_now && !s_prev_t;
    const bool esc_edge = esc_now && !s_prev_esc;
    s_prev_t = t_now;
    s_prev_esc = esc_now;

    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    const bool in_view = mx >= vx && mx < vx + vw && my >= vy && my < vy + vh;
    const bool free_ui = ui_element->active_id == 0 && !ui_element->input.camera_manipulating && !is_context_menu_open() && !number_field_is_active();

    if (t_edge && in_view && free_ui) manip_set_show(ui_element, !g_manip.show_manipulator);     /* Maya: T */
    if (esc_edge && !number_field_is_editing() && g_manip.edit_tool != manip_edit_none && !is_context_menu_open()) g_manip.edit_tool = manip_edit_none;
}

bool manip_overlay_wants_mouse(void) { return s_over || number_field_is_active(); }

/* ------------------------------------------------------------------------- */
/* manipulator panel helpers                                                  */
/* ------------------------------------------------------------------------- */
static const uint32_t k_field_id_base = 0x4D4E0000u;     /* ids for number_field: 9 transform boxes + 3 size boxes */

static float* field_ptr(system_scene_object* o, int idx)
{
    switch (idx) {
    case 0: return &o->position.x; case 1: return &o->position.y; case 2: return &o->position.z;
    case 3: return &o->rotation.x; case 4: return &o->rotation.y; case 5: return &o->rotation.z;
    case 6: return &o->scale.x;    case 7: return &o->scale.y;    default: return &o->scale.z;
    }
}

/* ------------------------------------------------------------------------- */
/* draw + interact                                                            */
/* ------------------------------------------------------------------------- */
void draw_scene_manipulator_overlay(VkCommandBuffer cmd, system_element_context* ui_element, int vx, int vy, int vw, int vh)
{
    if (!ui_element) return;

    begin_frame_once(ui_element, vx, vy, vw, vh);
    s_over = false;

    const bool edit_mode = (g_mode_state.context == mode_context_edit);
    const bool mode_ok   = edit_mode || (g_mode_state.context == mode_context_object);

    /* edit tools only exist in edit mode */
    if (!edit_mode) g_manip.edit_tool = manip_edit_none;

    if (g_manip.edit_tool != manip_edit_none) {
        const char* hint =
            (g_manip.edit_tool == manip_edit_target_weld) ? "Target Weld: press a vertex, drag onto another, release  (Esc = cancel)" :
            (g_manip.edit_tool == manip_edit_insert_loop) ? "Insert Loop: hover a row, wheel = how many, drag = slide, release = cut  (Esc = cancel)" :
                                                            "Multicut: not wired yet  (Esc = cancel)";
        const int tw = (int)strlen(hint) * mo_char_w;
        const int hx = vx + (vw - tw) / 2, hy = vy + mo_margin;
        push_quad_element(cmd, (float)(hx - 6), (float)(hy - 3), (float)(tw + 12), 20.0f, system_theme.scene_manipulator_background);
        draw_text_element(cmd, ui_element, hint, hx, hy, system_theme.scene_manipulator_text);
    }

    /* the manipulator shows whenever an object (cube, terrain, region_xx ...) is obj_vert_selected in Object or Edit mode */
    system_scene_object* sel = find_scene_object_by_id(g_scene_registry.selected_object_id);
    const bool has_obj = sel && sel != &g_scene_registry.objects[0];     /* objects[0] = World Space Root */
    if (!mode_ok) return;

    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;

    /* the manipulator panel sits in the top right corner of the Scene view */
    const int by = vy + mo_margin;
    int next_y = by;

    /* ---- snap readout ---- */
    if (g_manip.snap != manip_snap_off) {
        char snap[32];
        if (g_manip.snap == manip_snap_point_to_grid) snprintf(snap, sizeof(snap), "Snap: Grid %.2f", g_manip.grid_size);
        else                                          snprintf(snap, sizeof(snap), "Snap: Point");
        const int tw = (int)strlen(snap) * mo_char_w;
        draw_text_element(cmd, ui_element, snap, vx + vw - mo_margin - tw, next_y, system_color_mid_gray_2);
        next_y += 18;
    }

    /* ---- Manipulator: Pos / Rot / Scl, a separator, then the object Size ----
       every number is a number_field: drag it to scrub, click it to type (0.0625 or 1/16). */
    if (!has_obj) return;

    editor_vec3 size = { 0.0f, 0.0f, 0.0f };
    const bool has_size = object_get_mesh_size(sel, &size);       /* false: terrain, empty, light ... */

    const int px = vx + vw - mo_margin - mo_panel_w;
    const int py = next_y;
    const int ph = 4 + mo_row_h * 4 + (has_size ? mo_sep_gap + mo_row_h : 0) + 4;
    if (validate_rectangle_hit(mx, my, px, py, mo_panel_w, ph)) s_over = true;

    push_quad_element(cmd, (float)px, (float)py, (float)mo_panel_w, (float)ph, system_theme.scene_manipulator_background);

    char title[32];
    snprintf(title, sizeof(title), "%.20s", sel->name);
    draw_text_element(cmd, ui_element, title, px + 8, py + 5, system_theme.scene_manipulator_text);

    static const char* k_row_label[3] = { "Pos", "Rot", "Scl" };
    static const number_field_opts* k_row_opts[3] = { &k_pos_opts, &k_rot_opts, &k_scl_opts };

    for (int r = 0; r < 3; r++) {
        const int ry = py + 4 + mo_row_h * (1 + r);
        draw_text_element(cmd, ui_element, k_row_label[r], px + 8, ry + 3, system_theme.scene_manipulator_text);

        for (int c = 0; c < 3; c++) {
            const int idx = r * 3 + c;
            const int fx = px + 4 + mo_label_w + 2 + c * mo_field_w;
            float* vert_to_edge_array = field_ptr(sel, idx);

            const number_field_result res = ui_number_field(cmd, ui_element, k_field_id_base + (uint32_t)idx,
                                                            fx, ry, mo_field_w - 2, mo_row_h - 2, vert_to_edge_array, k_row_opts[r]);

            /* position goes through the snap while dragging. a typed number is exact and is never snapped. */
            if (r == 0 && res == number_field_scrubbed) {
                const editor_vec3 p = manip_snap_position(sel->position, sel->id);
                if (g_manip.snap == manip_snap_point_to_point) sel->position = p;                  /* jumps onto the other pivot */
                else *vert_to_edge_array = (c == 0) ? p.x : (c == 1 ? p.y : p.z);                                   /* grid / off: dragged axis only */
            }
        }
    }

    /* ---- separator + object size (mesh extent x scale). editing a size changes the scale ---- */
    if (has_size) {
        const int sy = py + 4 + mo_row_h * 4 + 4;
        push_quad_element(cmd, (float)(px + 6), (float)sy, (float)(mo_panel_w - 12), 1.0f, system_theme.scene_manipulator_input_background);

        const int ry = sy + 5;
        draw_text_element(cmd, ui_element, "Size", px + 8, ry + 3, system_theme.scene_manipulator_input_text);

        float sz[3] = { size.x, size.y, size.z };
        for (int c = 0; c < 3; c++) {
            const int fx = px + 4 + mo_label_w + 2 + c * mo_field_w;
            const number_field_result res = ui_number_field(cmd, ui_element, k_field_id_base + 9u + (uint32_t)c,
                                                            fx, ry, mo_field_w - 2, mo_row_h - 2, &sz[c], &k_size_opts);
            if (res != number_field_none) object_set_mesh_size_axis(sel, c, sz[c]);
        }
    }
}