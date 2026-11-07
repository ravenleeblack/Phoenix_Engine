#include "engine_globals.h"

/* g_manip, g_manip_frame, g_manip_*_opts, g_manip_row_labels, g_manip_row_opts and g_ui_frame_index
   are defined in engine_globals.c. */

void set_manip_show(system_element_context* ui_element, bool on)
{
    g_manip.show_manipulator = on;
    if (on && ui_element) ui_element->active_tool = tool_select;     /* the plain gizmo handles hide, the manipulator takes over */
}

void set_manip_snap(manip_snap_mode mode)
{
    g_manip.snap = mode;
    phoenix_info("Manip", "snap mode %d (0 off, 1 point to point, 2 point to grid)", (int)mode);
}

void scale_manip_grid(float factor)
{
    float g = g_manip.grid_size * factor;
    if (g < 0.125f) g = 0.125f;
    if (g > 16.0f)  g = 16.0f;
    g_manip.grid_size = g;
    phoenix_info("Manip", "grid size %.3f", g);
}

void toggle_manip_edit_tool(manip_edit_tool tool)
{
    if (g_manip.edit_tool == tool) {
        g_manip.edit_tool = manip_edit_none;
    } else {
        g_manip.edit_tool = tool;
    }
}

/* ------------------------------------------------------------------------- */
/* snapping (the math itself lives in engine_math.h)                          */
/* ------------------------------------------------------------------------- */
editor_vec3 snap_manip_position(editor_vec3 p, uint32_t moving_object_id)
{
    if (g_manip.snap == manip_snap_point_to_grid) {
        const float g = g_manip.grid_size;
        p.x = round_value_to_step(p.x, g);
        p.y = round_value_to_step(p.y, g);
        p.z = round_value_to_step(p.z, g);
    }
    else if (g_manip.snap == manip_snap_point_to_point) {
        float best = g_manip.point_snap_radius * g_manip.point_snap_radius;
        const system_scene_object* hit = NULL;

        for (uint32_t i = 1; i < g_scene_registry.object_count; i++) {     /* 0 = root */
            const system_scene_object* o = &g_scene_registry.objects[i];
            if (o->id == moving_object_id) continue;
            const float d2 = get_vec3_distance_squared(o->position, p);
            if (d2 < best) { best = d2; hit = o; }
        }
        if (hit) p = hit->position;
    }
    return p;
}

/* ------------------------------------------------------------------------- */
/* Target Weld core                                                           */
/* ------------------------------------------------------------------------- */
void weld_manip_indices(uint32_t* indices, uint32_t* index_count, uint32_t src, uint32_t dst)
{
    if (!indices || !index_count) return;

    uint32_t out = 0;
    for (uint32_t t = 0; t + 2 < *index_count; t += 3) {
        uint32_t a = indices[t];
        uint32_t b = indices[t + 1];
        uint32_t c = indices[t + 2];
        if (a == src) a = dst;
        if (b == src) b = dst;
        if (c == src) c = dst;
        if (a == b || b == c || a == c) continue;                      /* collapsed triangle */
        indices[out++] = a; indices[out++] = b; indices[out++] = c;    /* out <= t, safe in place */
    }
    *index_count = out;
}

/* ------------------------------------------------------------------------- */
/* per frame input edges                                                      */
/* ------------------------------------------------------------------------- */
void begin_manip_frame_once(system_element_context* ui_element, int vx, int vy, int vw, int vh)
{
    if (g_manip_frame.frame == g_ui_frame_index) return;
    g_manip_frame.frame = g_ui_frame_index;

    g_manip_frame.edge      = ui_element->input.mouse_down && !g_manip_frame.prev_down;
    g_manip_frame.prev_down = ui_element->input.mouse_down;

    const bool t_now    = (GetAsyncKeyState('T')       & 0x8000) != 0;
    const bool esc_now  = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    const bool t_edge   = t_now && !g_manip_frame.prev_t;
    const bool esc_edge = esc_now && !g_manip_frame.prev_esc;
    g_manip_frame.prev_t   = t_now;
    g_manip_frame.prev_esc = esc_now;

    const int mx = ui_element->input.mouse_x;
    const int my = ui_element->input.mouse_y;
    const bool in_view = mx >= vx && mx < vx + vw && my >= vy && my < vy + vh;
    const bool free_ui = ui_element->active_id == 0 && !ui_element->input.camera_manipulating && !is_context_menu_open() && !is_number_field_active();

    if (t_edge && in_view && free_ui) set_manip_show(ui_element, !g_manip.show_manipulator);     /* Maya: T */
    if (esc_edge && !is_number_field_editing() && g_manip.edit_tool != manip_edit_none && !is_context_menu_open()) {
        g_manip.edit_tool = manip_edit_none;
    }
}

bool is_manip_overlay_using_mouse(void)
{
    return g_manip_frame.over || is_number_field_active();
}

/* ------------------------------------------------------------------------- */
/* manipulator panel helpers                                                  */
/* ------------------------------------------------------------------------- */
float* get_manip_field_ptr(system_scene_object* o, int index)
{
    switch (index) {
    case 0: return &o->position.x;
    case 1: return &o->position.y;
    case 2: return &o->position.z;
    case 3: return &o->rotation.x;
    case 4: return &o->rotation.y;
    case 5: return &o->rotation.z;
    case 6: return &o->scale.x;
    case 7: return &o->scale.y;
    default: return &o->scale.z;
    }
}

const char* get_manip_edit_tool_hint(manip_edit_tool tool)
{
    if (tool == manip_edit_target_weld) return "Target Weld: press a vertex, drag onto another, release  (Esc = cancel)";
    if (tool == manip_edit_insert_loop) return "Insert Loop: hover a row, wheel = how many, drag = slide, release = cut  (Esc = cancel)";
    return "Multicut: not wired yet  (Esc = cancel)";
}

/* ------------------------------------------------------------------------- */
/* draw + interact                                                            */
/* ------------------------------------------------------------------------- */
void draw_scene_manipulator_overlay(VkCommandBuffer cmd, system_element_context* ui_element, int vx, int vy, int vw, int vh)
{
    if (!ui_element) return;

    begin_manip_frame_once(ui_element, vx, vy, vw, vh);
    g_manip_frame.over = false;

    const bool edit_mode = (g_mode_state.context == mode_context_edit);
    const bool mode_ok   = edit_mode || (g_mode_state.context == mode_context_object);

    /* edit tools only exist in edit mode */
    if (!edit_mode) g_manip.edit_tool = manip_edit_none;

    if (g_manip.edit_tool != manip_edit_none) {
        const char* hint = get_manip_edit_tool_hint(g_manip.edit_tool);
        const int tw = (int)strlen(hint) * mo_char_w;
        const int hx = vx + (vw - tw) / 2;
        const int hy = vy + mo_margin;
        push_quad_element(cmd, (float)(hx - 6), (float)(hy - 3), (float)(tw + 12), 20.0f, system_theme.scene_manipulator_background);
        draw_text_element(cmd, ui_element, hint, hx, hy, system_theme.scene_manipulator_text);
    }

    system_scene_object* sel = find_scene_object_by_id(g_scene_registry.selected_object_id);
    const bool has_obj = sel && sel != &g_scene_registry.objects[0];     /* objects[0] = World Space Root */
    if (!mode_ok) return;

    const int mx = ui_element->input.mouse_x;
    const int my = ui_element->input.mouse_y;

    const int by = vy + mo_margin;
    int next_y = by;

    /* ---- snap readout ---- */
    if (g_manip.snap != manip_snap_off) {
        char snap[32];
        if (g_manip.snap == manip_snap_point_to_grid) {
            snprintf(snap, sizeof(snap), "Snap: Grid %.2f", g_manip.grid_size);
        } else {
            snprintf(snap, sizeof(snap), "Snap: Point");
        }
        const int tw = (int)strlen(snap) * mo_char_w;
        draw_text_element(cmd, ui_element, snap, vx + vw - mo_margin - tw, next_y, system_theme.system_text);
        next_y += 18;
    }

    if (!has_obj) return;

    editor_vec3 size = { 0.0f, 0.0f, 0.0f };
    const bool has_size = get_object_mesh_size(sel, &size);       /* false: terrain, empty, light ... */

    int size_h = 0;
    if (has_size) size_h = mo_sep_gap + mo_row_h;

    const int px = vx + vw - mo_margin - mo_panel_w;
    const int py = next_y;
    const int ph = 4 + mo_row_h * 4 + size_h + 4;
    if (validate_rectangle_hit(mx, my, px, py, mo_panel_w, ph)) g_manip_frame.over = true;

    push_quad_element(cmd, (float)px, (float)py, (float)mo_panel_w, (float)ph, system_theme.scene_manipulator_background);

    char title[32];
    snprintf(title, sizeof(title), "%.20s", sel->name);
    draw_text_element(cmd, ui_element, title, px + 8, py + 5, system_theme.scene_manipulator_text);

    for (int r = 0; r < 3; r++) {
        const int ry = py + 4 + mo_row_h * (1 + r);
        draw_text_element(cmd, ui_element, g_manip_row_labels[r], px + 8, ry + 3, system_theme.scene_manipulator_text);

        for (int c = 0; c < 3; c++) {
            const int idx = r * 3 + c;
            const int fx = px + 4 + mo_label_w + 2 + c * mo_field_w;
            float* value_ptr = get_manip_field_ptr(sel, idx);

            const number_field_result res = draw_number_field(cmd, ui_element, manip_field_id_base + (uint32_t)idx,
                                                              fx, ry, mo_field_w - 2, mo_row_h - 2, value_ptr, g_manip_row_opts[r]);

            /* position goes through the snap while dragging. a typed number is exact and is never snapped. */
            if (r == 0 && res == number_field_scrubbed) {
                const editor_vec3 p = snap_manip_position(sel->position, sel->id);
                if (g_manip.snap == manip_snap_point_to_point) {
                    sel->position = p;                          /* jumps onto the other pivot */
                } else {
                    *value_ptr = get_vec3_axis(p, c);           /* grid / off: dragged axis only */
                }
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
            const number_field_result res = draw_number_field(cmd, ui_element, manip_field_id_base + 9u + (uint32_t)c,
                                                              fx, ry, mo_field_w - 2, mo_row_h - 2, &sz[c], &g_manip_size_opts);
            if (res != number_field_none) set_object_mesh_size_axis(sel, c, sz[c]);
        }
    }
}