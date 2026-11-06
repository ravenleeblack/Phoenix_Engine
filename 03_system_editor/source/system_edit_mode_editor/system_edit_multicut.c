#include "system_globals.h"



/* ========================================================================= */
/* MULTICUT                                                                  */
/*   hold CTRL            : show where the cut would go, with its percent    */
/*   CTRL + wheel         : more / fewer cut lines (each one gets a percent) */
/*   CTRL + left click    : make the cut                                     */
/* The lines are spread evenly along the hovered edge: 1 line = 50 %,        */
/* 3 lines = 25 / 50 / 75 %. The percent counts from the edge's first vertex. */
/* The transform gizmo is hidden while this tool is on.                      */
/* ========================================================================= */
#define MULTICUT_MAX_LINES  32
#define MULTICUT_LABEL_DIST 18.0f       /* pixels the number sits away from the edge, along its cut line */

static int      s_cut_count = 1;
static bool     s_cut_have  = false;    /* an edge is under the cursor and CTRL is held */
static uint32_t s_cut_edge  = EM_NONE;
static edit_mode_selection_ring s_cut_ring;

static bool is_ctrl_down(void) { return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0; }

void edit_multicut_cancel(system_element_context* ui_element)
{
    (void)ui_element;
    s_cut_have = false;
    s_cut_edge = EM_NONE;
}

bool edit_multicut_toggle(system_element_context* ui_element)
{
    if (g_mode_state.context != mode_context_edit) { edit_msg("Multicut: switch to Edit mode first"); return false; }
    if (is_terrain_object(find_scene_object_by_id(g_scene_registry.selected_object_id))) { edit_msg("Multicut: not on terrain (fixed height grid)"); return false; }
    manip_toggle_edit_tool(manip_edit_multicut);
    edit_target_weld_cancel(ui_element);
    edit_insert_loop_cancel(ui_element);
    edit_multicut_cancel(ui_element);
    if (g_manip.edit_tool == manip_edit_multicut) edit_msg("Multicut: hold CTRL to preview, CTRL + wheel = more lines, CTRL + click = cut");
    return true;
}

bool edit_multicut_wheel(float notches)
{
    if (!is_ctrl_down()) return false;                          /* no CTRL: the wheel keeps zooming the camera */
    int d = (int)lroundf(notches);
    if (d == 0) d = (notches > 0.0f) ? 1 : -1;
    s_cut_count += d;
    if (s_cut_count < 1)                 s_cut_count = 1;
    if (s_cut_count > MULTICUT_MAX_LINES) s_cut_count = MULTICUT_MAX_LINES;
    edit_msg("Multicut: %d line%s  (CTRL + wheel = more / fewer, CTRL + click = cut)", s_cut_count, s_cut_count == 1 ? "" : "s");
    return true;
}

void edit_multicut_update(const edit_frame* f)
{
    s_cut_have = false;
    s_cut_edge = EM_NONE;
    if (!f->inside || !is_ctrl_down()) return;                  /* CTRL is what arms the tool */

    const uint32_t e = edit_pick_edge(f->mesh, f->mx, f->my);
    if (e == EM_NONE || !edit_ring_build(f->mesh, e, &s_cut_ring)) return;
    s_cut_have = true;
    s_cut_edge = e;

    if (f->press) {                                             /* CTRL + left click = submit */
        const char* err = NULL;
        s_cut_have = false;                                     /* the mesh is about to change: the ring is stale after this */
        if (edit_op_cut_ring(f->mesh, &s_cut_ring, s_cut_count, 0.0f, &err) && em_commit_to_gpu(f->mesh, f->obj))
            edit_msg("Multicut: %d line%s cut. Move them with the gizmo (switch the tool off first).", s_cut_count, s_cut_count == 1 ? "" : "s");
        else edit_msg("Multicut: %s", err ? err : "failed");
    }
}

static void draw_percent_label(VkCommandBuffer cmd, system_element_context* ui_element, float cx, float cy, const char* text)
{
    const int tw = (int)strlen(text) * 8;
    const float x = cx - (float)tw * 0.5f, y = cy - 8.0f;
    push_quad_element(cmd, x - 4.0f, y - 3.0f, (float)(tw + 8), 20.0f, system_theme.edit_mode_multicut_percent_bg);
    draw_text_element(cmd, ui_element, text, (int)x, (int)y, system_theme.edit_mode_multicut_percent_text);
}

void edit_multicut_draw(VkCommandBuffer cmd, system_element_context* ui_element, const edit_mode_camera_context* cam, const edit_mode_mesh* mesh)
{
    if (g_manip.edit_tool != manip_edit_multicut || !s_cut_have || s_cut_ring.n < 2 || s_cut_edge >= mesh->edge_count) return;
    const edit_mode_selection_ring* R = &s_cut_ring;
    const cam_screen* S = edit_screen_points();
    const system_color line_color = R->clean ? system_theme.system_color_edit_mode_preview_color : system_theme.system_color_edit_mode_invalid_color;

    for (uint32_t i = 0; i < R->n; ++i) {                       /* the row that would be cut */
        const cam_screen a = S[R->a[i]], b = S[R->b[i]];
        if (a.ok && b.ok) edit_line2d(cmd, a.x, a.y, b.x, b.y, 1.0f, system_theme.system_color_edit_mode_hover_color);
    }

    const uint32_t nf = edit_ring_face_count(R);
    for (int j = 1; j <= s_cut_count; ++j) {                    /* the cut lines */
        const float t = system_math_compute_loop_t(j, s_cut_count, 0.0f);
        for (uint32_t i = 0; i < nf; ++i) {
            const uint32_t e1 = i, e2 = (i + 1) % R->n;
            const cam_screen p1 = edit_project(cam, lerp3(mesh->verts[R->a[e1]].obj_vert_pos, mesh->verts[R->b[e1]].obj_vert_pos, t));
            const cam_screen p2 = edit_project(cam, lerp3(mesh->verts[R->a[e2]].obj_vert_pos, mesh->verts[R->b[e2]].obj_vert_pos, t));
            if (p1.ok && p2.ok) edit_line2d(cmd, p1.x, p1.y, p2.x, p2.y, 2.0f, line_color);
        }
    }

    /* the numbers: on the hovered edge's own cut point, nudged along the cut line so they stay readable */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < R->n; ++i) if (R->edge_to_face_array[i] == s_cut_edge) { idx = i; break; }
    uint32_t nb = (idx + 1 < R->n) ? idx + 1 : (R->closed ? 0 : idx - 1);

    for (int j = 1; j <= s_cut_count; ++j) {
        const float t = system_math_compute_loop_t(j, s_cut_count, 0.0f);
        const cam_screen p = edit_project(cam, lerp3(mesh->verts[R->a[idx]].obj_vert_pos, mesh->verts[R->b[idx]].obj_vert_pos, t));
        const cam_screen q = edit_project(cam, lerp3(mesh->verts[R->a[nb]].obj_vert_pos,  mesh->verts[R->b[nb]].obj_vert_pos,  t));
        if (!p.ok || !q.ok) continue;
        edit_box2d(cmd, p.x, p.y, 4.0f, line_color);
        float ox, oy;
        system_math_compute_label_offset(p.x, p.y, q.x, q.y, MULTICUT_LABEL_DIST, &ox, &oy);
        char text[24];
        snprintf(text, sizeof(text), "%.4g%%", system_math_compute_cut_percent(j, s_cut_count));    /* 50 -> "50%", 33.333 -> "33.33%" */
        draw_percent_label(cmd, ui_element, p.x + ox, p.y + oy, text);
    }
}