#include "system_globals.h"



/* ========================================================================= */
/* BRIDGE two obj_vert_selected faces                                                 */
/* ========================================================================= */
bool edit_op_bridge(edit_mode_mesh* mesh, const char** err)
{
    uint32_t fa = EM_NONE, fb = EM_NONE, sel = 0;
    for (uint32_t i = 0; i < mesh->face_count; ++i) {
        if (!mesh->faces[i].obj_vert_selected) continue;
        if (sel == 0) fa = i; else if (sel == 1) fb = i;
        ++sel;
    }
    if (sel != 2) { *err = "select exactly two faces (click one, shift+click the other)"; return false; }

    const edit_mode_face *A = &mesh->faces[fa], *B = &mesh->faces[fb];
    if (A->face_type != B->face_type) { *err = "both faces must have the same number of sides"; return false; }
    const int n = A->face_type;
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j)
        if (A->vert_to_edge_array[i] == B->vert_to_edge_array[j]) { *err = "the faces touch each other, nothing to fill between"; return false; }
    if (dot3(edit_face_normal(mesh, A), edit_face_normal(mesh, B)) > 0.5f) { *err = "the faces point the same way: Flip Normals on one of them first"; return false; }

    /* B runs the other way round when seen from A, so corner i of A meets corner (s - i) of B. pick the s with the shortest total span */
    int best_s = 0; float best = 1e30f;
    for (int s = 0; s < n; ++s) {
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) {
            const editor_vec3 d = sub3(mesh->verts[A->vert_to_edge_array[i]].obj_vert_pos, mesh->verts[B->vert_to_edge_array[((s - i) % n + n) % n]].obj_vert_pos);
            sum += dot3(d, d);
        }
        if (sum < best) { best = sum; best_s = s; }
    }

    num_of_faces_list L; memset(&L, 0, sizeof(L));
    bool ok = true;
    for (uint32_t i = 0; i < mesh->face_count && ok; ++i)
        if (i != fa && i != fb) ok = nl_push_existing(&L, mesh, i);
    for (int i = 0; i < n && ok; ++i) {
        const uint32_t a0 = A->vert_to_edge_array[i], a1 = A->vert_to_edge_array[(i + 1) % n];
        const uint32_t b0 = B->vert_to_edge_array[((best_s - i) % n + n) % n];
        const uint32_t b1 = B->vert_to_edge_array[((best_s - i - 1) % n + n) % n];
        ok = nl_push_quad(&L, a1, a0, b0, b1);
    }
    for (uint32_t vert_to_edge_array = 0; vert_to_edge_array < mesh->vert_count; ++vert_to_edge_array) mesh->verts[vert_to_edge_array].obj_vert_selected = 0;
    if (ok) ok = em_rebuild(mesh, L.f, L.n, select_vert);
    free(L.f);
    if (!ok) *err = "rebuild failed";
    return ok;
}