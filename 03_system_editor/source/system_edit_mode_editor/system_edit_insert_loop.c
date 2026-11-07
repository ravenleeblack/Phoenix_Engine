#include "system_globals.h"



/* ========================================================================= */
/* INSERT LOOP: hover a row, wheel = how many, drag = slide, release = cut   */
/* `slide` in -0.5..0.5 moves the new rows along the ring.                   */
/* ========================================================================= */
static int      s_loop_count = 1;
static bool     s_loop_have  = false;       /* a ring is under the cursor */
static edit_mode_selection_ring  s_ring;
static bool     s_loop_drag  = false;
static int      s_loop_x0    = 0;
static float    s_loop_slide = 0.0f;

static void corner_uv(const edit_mode_face* F, uint32_t vid, float out[2])
{
    out[0] = out[1] = 0.0f;
    for (int k = 0; k < F->face_type; ++k) if (F->vert_to_edge_array[k] == vid) { out[0] = F->uv[k][0]; out[1] = F->uv[k][1]; return; }
}

static uint32_t ring_P(const edit_mode_selection_ring* R, const uint32_t* nv, int face_type, uint32_t i, int j)
{
    if (j == 0) return R->a[i];
    if (j == face_type + 1) return R->b[i];
    return nv[(size_t)i * (size_t)face_type + (size_t)(j - 1)];
}

bool edit_op_cut_ring(edit_mode_mesh* mesh, const edit_mode_selection_ring* R, int face_type, float slide, const char** err)
{
    if (!R->clean) { *err = "this row ends on a triangle: loops need quads (or a border) at both ends"; return false; }

    const uint32_t nfaces = edit_ring_face_count(R);
    uint8_t* in_ring = (uint8_t*)calloc(mesh->face_count, 1);
    uint32_t* nv = (uint32_t*)malloc(sizeof(uint32_t) * (size_t)R->n * (size_t)face_type);
    if (!in_ring || !nv) { free(in_ring); free(nv); *err = "out of memory"; return false; }
    for (uint32_t i = 0; i < nfaces; ++i) in_ring[R->face[i]] = 1;

    for (uint32_t vert_to_edge_array = 0; vert_to_edge_array < mesh->vert_count; ++vert_to_edge_array) mesh->verts[vert_to_edge_array].obj_vert_selected = 0;

    for (uint32_t i = 0; i < R->n; ++i) {                      /* the new verts, shared by the two faces next to each ring edge_to_face_array */
        const editor_vec3 pa = mesh->verts[R->a[i]].obj_vert_pos, pb = mesh->verts[R->b[i]].obj_vert_pos;
        float ca[4], cb[4];
        memcpy(ca, mesh->verts[R->a[i]].obj_vert_color, sizeof(ca));
        memcpy(cb, mesh->verts[R->b[i]].obj_vert_color, sizeof(cb));
        for (int j = 1; j <= face_type; ++j) {
            const float t = system_math_compute_loop_t(j, face_type, slide);
            float col[4];
            for (int c = 0; c < 4; ++c) col[c] = ca[c] * (1.0f - t) + cb[c] * t;
            const uint32_t id = em_add_vert(mesh, lerp3(pa, pb, t), col);
            if (id == EM_NONE) { free(in_ring); free(nv); *err = "out of memory"; return false; }
            mesh->verts[id].obj_vert_selected = 1;
            nv[(size_t)i * (size_t)face_type + (size_t)(j - 1)] = id;
        }
    }

    num_of_faces_list L; memset(&L, 0, sizeof(L));
    bool ok = true;
    for (uint32_t i = 0; i < mesh->face_count && ok; ++i)
        if (!in_ring[i]) ok = nl_push_existing(&L, mesh, i);

    for (uint32_t i = 0; i < nfaces && ok; ++i) {
        const edit_mode_face* F = &mesh->faces[R->face[i]];
        const uint32_t e1 = i, e2 = (i + 1) % R->n;
        bool fwd = false;                                      /* does the face walk a1 -> b1 ? then so must the new quads */
        for (int k = 0; k < 4; ++k) if (F->vert_to_edge_array[k] == R->a[e1] && F->vert_to_edge_array[(k + 1) % 4] == R->b[e1]) fwd = true;

        float ua1[2], ub1[2], ua2[2], ub2[2];
        corner_uv(F, R->a[e1], ua1); corner_uv(F, R->b[e1], ub1);
        corner_uv(F, R->a[e2], ua2); corner_uv(F, R->b[e2], ub2);

        for (int k = 0; k <= face_type && ok; ++k) {
            const uint32_t p00 = ring_P(R, nv, face_type, e1, k),     p01 = ring_P(R, nv, face_type, e1, k + 1);
            const uint32_t p10 = ring_P(R, nv, face_type, e2, k),     p11 = ring_P(R, nv, face_type, e2, k + 1);
            const float t0 = system_math_compute_loop_t(k, face_type, slide), t1 = system_math_compute_loop_t(k + 1, face_type, slide);

            /* uv at (side s, along t): bilinear between the face's own corner uvs */
            float c00[2], c01[2], c10[2], c11[2];
            for (int c = 0; c < 2; ++c) {
                c00[c] = ua1[c] * (1 - t0) + ub1[c] * t0;  c01[c] = ua1[c] * (1 - t1) + ub1[c] * t1;
                c10[c] = ua2[c] * (1 - t0) + ub2[c] * t0;  c11[c] = ua2[c] * (1 - t1) + ub2[c] * t1;
            }
            num_of_faces q; memset(&q, 0, sizeof(q));
            q.face_type = 4;
            if (fwd) { q.vert_to_edge_array[0] = p00; q.vert_to_edge_array[1] = p01; q.vert_to_edge_array[2] = p11; q.vert_to_edge_array[3] = p10;
                       memcpy(q.uv[0], c00, 8); memcpy(q.uv[1], c01, 8); memcpy(q.uv[2], c11, 8); memcpy(q.uv[3], c10, 8); }
            else     { q.vert_to_edge_array[0] = p00; q.vert_to_edge_array[1] = p10; q.vert_to_edge_array[2] = p11; q.vert_to_edge_array[3] = p01;
                       memcpy(q.uv[0], c00, 8); memcpy(q.uv[1], c10, 8); memcpy(q.uv[2], c11, 8); memcpy(q.uv[3], c01, 8); }
            ok = nl_push(&L, &q);
        }
    }
    free(in_ring); free(nv);
    if (ok) ok = em_rebuild(mesh, L.f, L.n, select_vert);        /* the new verts stay obj_vert_selected: grab the gizmo and move the whole row */
    free(L.f);
    if (!ok) *err = "rebuild failed";
    return ok;
}

/* ------------------------------------------------------------------------- */
/* interactive part                                                          */
/* ------------------------------------------------------------------------- */
bool edit_insert_loop_dragging(void) { return s_loop_drag; }

void edit_insert_loop_cancel(system_element_context* ui_element)
{
    if (s_loop_drag && ui_element && ui_element->active_id == edit_ops_lock_id) ui_element->active_id = 0;
    s_loop_drag = false;
    s_loop_have = false;
    s_loop_slide = 0.0f;
}

bool edit_insert_loop_toggle(system_element_context* ui_element)
{
    if (g_mode_state.context != mode_context_edit) { edit_msg("Insert Loop: switch to Edit mode first"); return false; }
    if (is_terrain_object(find_scene_object_by_id(g_scene_registry.selected_object_id))) { edit_msg("Insert Loop: not on terrain (fixed height grid)"); return false; }
    toggle_manip_edit_tool(manip_edit_insert_loop);
    edit_target_weld_cancel(ui_element);
    edit_multicut_cancel(ui_element);
    edit_insert_loop_cancel(ui_element);
    if (g_manip.edit_tool == manip_edit_insert_loop) edit_msg("Insert Loop: hover a row, wheel = how many, drag = slide, release = cut");
    return true;
}

bool consume_edit_insert_loop_wheel(float notches)
{
    int d = (int)lroundf(notches);
    if (d == 0) d = (notches > 0.0f) ? 1 : -1;
    s_loop_count += d;
    if (s_loop_count < 1)  s_loop_count = 1;
    if (s_loop_count > 16) s_loop_count = 16;
    edit_msg("Insert Loop: %d row%s  (wheel = more / fewer, drag = slide, release = cut)", s_loop_count, s_loop_count == 1 ? "" : "s");
    return true;
}

void update_edit_insert_loop(const edit_frame* f)
{
    system_element_context* ui_element = f->ui_element;
    edit_mode_mesh* mesh = f->mesh;

    if (!s_loop_drag) {
        s_loop_have = false;
        if (f->inside) {
            const uint32_t e = edit_pick_edge(mesh, f->mx, f->my);
            if (e != EM_NONE && edit_ring_build(mesh, e, &s_ring)) s_loop_have = true;
        }
        if (f->press && f->inside && s_loop_have && ui_element->active_id == 0) {
            s_loop_drag = true; s_loop_x0 = ui_element->input.mouse_x; s_loop_slide = 0.0f; ui_element->active_id = edit_ops_lock_id;
        }
        return;
    }

    float s = (float)(ui_element->input.mouse_x - s_loop_x0) / 240.0f;
    if (s < -0.5f) s = -0.5f;
    if (s > 0.5f)  s = 0.5f;
    s_loop_slide = s;
    if (!f->down) {                                            /* release = cut */
        const float slide = s_loop_slide;
        edit_insert_loop_cancel(ui_element);
        const char* err = NULL;
        if (edit_op_cut_ring(mesh, &s_ring, s_loop_count, slide, &err) && em_commit_to_gpu(mesh, f->obj))
            edit_msg("Insert Loop: %d row%s cut. Move them with the gizmo.", s_loop_count, s_loop_count == 1 ? "" : "s");
        else edit_msg("Insert Loop: %s", err ? err : "failed");
    }
}

void edit_insert_loop_draw(VkCommandBuffer cmd, const edit_mode_camera_context* cam, const edit_mode_mesh* mesh)
{
    if (g_manip.edit_tool != manip_edit_insert_loop || !s_loop_have || s_ring.n < 2) return;
    const cam_screen* S = edit_screen_points();

    const uint32_t nf = edit_ring_face_count(&s_ring);
    const float slide = s_loop_drag ? s_loop_slide : 0.0f;
    for (uint32_t i = 0; i < s_ring.n; ++i) {                        /* the row under the cursor */
        const cam_screen a = S[s_ring.a[i]], b = S[s_ring.b[i]];
        if (a.ok && b.ok) edit_line2d(cmd, a.x, a.y, b.x, b.y, 1.0f, system_theme.system_color_edit_mode_hover_color);
    }
    for (uint32_t i = 0; i < nf; ++i) {                              /* the rows that would be cut */
        const uint32_t e1 = i, e2 = (i + 1) % s_ring.n;
        for (int j = 1; j <= s_loop_count; ++j) {
            const float t = system_math_compute_loop_t(j, s_loop_count, slide);
            const cam_screen p1 = project_edit_point(cam, lerp3(mesh->verts[s_ring.a[e1]].obj_vert_pos, mesh->verts[s_ring.b[e1]].obj_vert_pos, t));
            const cam_screen p2 = project_edit_point(cam, lerp3(mesh->verts[s_ring.a[e2]].obj_vert_pos, mesh->verts[s_ring.b[e2]].obj_vert_pos, t));
            if (p1.ok && p2.ok) edit_line2d(cmd, p1.x, p1.y, p2.x, p2.y, 2.0f, s_ring.clean ? system_theme.system_color_edit_mode_preview_color : system_theme.system_color_edit_mode_invalid_color);
        }
    }
}