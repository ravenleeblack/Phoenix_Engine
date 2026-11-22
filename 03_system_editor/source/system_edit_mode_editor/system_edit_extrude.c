#include "system_globals.h"



/* ========================================================================= */
/* EXTRUDE obj_vert_selected faces                                                    */
/* ========================================================================= */
static float s_extrude_distance = 1.0f;
void  set_edit_ops_extrude_distance(float d) { s_extrude_distance = d; }
float get_edit_ops_extrude_distance(void)    { return s_extrude_distance; }

/* Extrude on the terrain: the ground is a height grid, so extruding the selected faces pulls every vertex of them (and every selected
   vertex) straight up by the extrude distance. a negative distance digs. the commit writes the heights back and rebuilds the chunks. */
bool edit_op_terrain_extrude(edit_mode_mesh* mesh, const char** err)
{
    uint8_t* lift = (uint8_t*)calloc(mesh->vert_count, 1);
    if (!lift) { *err = "out of memory"; return false; }
    uint32_t count = 0;
    for (uint32_t i = 0; i < mesh->vert_count; ++i) {
        if (mesh->verts[i].obj_vert_selected) { lift[i] = 1; }
    }
    for (uint32_t i = 0; i < mesh->face_count; ++i) {
        const edit_mode_face* F = &mesh->faces[i];
        if (!F->obj_vert_selected) continue;
        for (int k = 0; k < F->face_type; ++k) lift[F->vert_to_edge_array[k]] = 1;
    }
    for (uint32_t i = 0; i < mesh->vert_count; ++i) {
        if (!lift[i]) continue;
        mesh->verts[i].obj_vert_pos.y += s_extrude_distance;
        count++;
    }
    free(lift);
    if (count == 0) { *err = "select vertices or faces of the terrain first"; return false; }
    return true;
}

bool edit_op_extrude(edit_mode_mesh* mesh, const char** err)
{
    uint32_t sel = 0;
    for (uint32_t i = 0; i < mesh->face_count; ++i) sel += mesh->faces[i].obj_vert_selected ? 1u : 0u;
    if (!sel) { *err = "select a face first (key 3 = face mode)"; return false; }

    const uint32_t vc = mesh->vert_count;
    uint32_t*    dup = (uint32_t*)malloc(sizeof(uint32_t) * vc);
    editor_vec3* acc = (editor_vec3*)calloc(vc, sizeof(editor_vec3));
    uint8_t*     in_sel = (uint8_t*)calloc(vc, 1);                  /* vertex belongs to at least one selected face */
    if (!dup || !acc || !in_sel) { free(dup); free(acc); free(in_sel); *err = "out of memory"; return false; }
    memset(dup, 0xFF, sizeof(uint32_t) * vc);

    for (uint32_t i = 0; i < mesh->face_count; ++i) {
        const edit_mode_face* F = &mesh->faces[i];
        if (!F->obj_vert_selected) continue;
        const editor_vec3 n = edit_face_normal(mesh, F);
        for (int k = 0; k < F->face_type; ++k) { acc[F->vert_to_edge_array[k]] = add3(acc[F->vert_to_edge_array[k]], n); in_sel[F->vert_to_edge_array[k]] = 1; }
    }
    for (uint32_t vi = 0; vi < vc; ++vi) {
        if (!in_sel[vi]) continue;
        /* EVERY vertex of a selected face gets a copy, even when the normals of the faces around it cancel out (then it simply does not move).
           Without a copy the cap face lost a corner and was silently dropped by the rebuild, so only some of the selected faces came out. */
        const float len2 = dot3(acc[vi], acc[vi]);
        const editor_vec3 p = (len2 < 1e-12f) ? mesh->verts[vi].obj_vert_pos
                                              : add3(mesh->verts[vi].obj_vert_pos, mul3(norm3(acc[vi]), s_extrude_distance));
        float col[4]; memcpy(col, mesh->verts[vi].obj_vert_color, sizeof(col));
        dup[vi] = em_add_vert(mesh, p, col);                      /* may realloc mesh->verts: nothing above is held across this call */
        if (dup[vi] == EM_NONE) { free(dup); free(acc); free(in_sel); *err = "out of memory"; return false; }
    }
    for (uint32_t vert_to_edge_array = 0; vert_to_edge_array < mesh->vert_count; ++vert_to_edge_array) mesh->verts[vert_to_edge_array].obj_vert_selected = 0;

    num_of_faces_list L; memset(&L, 0, sizeof(L));
    bool ok = true;
    for (uint32_t i = 0; i < mesh->face_count && ok; ++i) {
        const edit_mode_face* F = &mesh->faces[i];
        if (!F->obj_vert_selected) { ok = nl_push_existing(&L, mesh, i); continue; }

        num_of_faces cap; memset(&cap, 0, sizeof(cap));              /* the moved copy of the face */
        cap.face_type = F->face_type; cap.obj_vert_selected = 1;
        for (int k = 0; k < F->face_type; ++k) cap.vert_to_edge_array[k] = dup[F->vert_to_edge_array[k]];
        memcpy(cap.uv, F->uv, sizeof(cap.uv));
        ok = nl_push(&L, &cap);

        for (int k = 0; k < F->face_type && ok; ++k) {            /* side wall on every edge_to_face_array that borders the unselected world */
            const edit_mode_edge* E = &mesh->edges[F->edge_to_face_array[k]];
            uint32_t other = EM_NONE;
            if (E->face_count == 2) other = (E->face[0] == i) ? E->face[1] : E->face[0];
            if (other != EM_NONE && mesh->faces[other].obj_vert_selected) continue;
            const uint32_t a = F->vert_to_edge_array[k], b = F->vert_to_edge_array[(k + 1) % F->face_type];
            ok = nl_push_quad(&L, a, b, dup[b], dup[a]);      /* outward: a, b, b', a' */
        }
    }
    free(dup); free(acc); free(in_sel);
    if (ok) ok = em_rebuild(mesh, L.f, L.n, select_face);
    free(L.f);
    if (ok) {                                                   /* the rebuild forgets the active element: give the gizmo an anchor, all moved faces stay selected */
        for (uint32_t i = 0; i < mesh->face_count; ++i)
            if (mesh->faces[i].obj_vert_selected) { em_set_active_element(mesh, select_face, i); break; }
    }
    if (!ok) *err = "rebuild failed";
    return ok;
}