#include "system_globals.h"



/* ========================================================================= */
/* DELETE obj_vert_selected faces                                                     */
/* ========================================================================= */
bool edit_op_delete_faces(edit_mode_mesh* mesh, const char** err)
{
    num_of_faces_list L; memset(&L, 0, sizeof(L));
    uint32_t del = 0;
    bool ok = true;
    for (uint32_t i = 0; i < mesh->face_count && ok; ++i) {
        if (mesh->faces[i].obj_vert_selected) { ++del; continue; }
        ok = nl_push_existing(&L, mesh, i);
    }
    if (!del)      { free(L.f); *err = "select a face first (key 3)"; return false; }
    if (!L.n)      { free(L.f); *err = "that would delete the whole mesh"; return false; }
    for (uint32_t vert_to_edge_array = 0; vert_to_edge_array < mesh->vert_count; ++vert_to_edge_array) mesh->verts[vert_to_edge_array].obj_vert_selected = 0;
    if (ok) ok = em_rebuild(mesh, L.f, L.n, select_vert);
    free(L.f);
    if (!ok) *err = "rebuild failed";
    return ok;
}