#include "system_globals.h"



/* ========================================================================= */
/* FLIP NORMALS: obj_vert_selected faces, or every face when nothing is obj_vert_selected      */
/* ========================================================================= */
bool edit_op_flip_normals(edit_mode_mesh* mesh, const char** err)
{
    bool any = false;
    for (uint32_t i = 0; i < mesh->face_count; ++i) if (mesh->faces[i].obj_vert_selected) { any = true; break; }

    num_of_faces_list L; memset(&L, 0, sizeof(L));
    bool ok = true;
    for (uint32_t i = 0; i < mesh->face_count && ok; ++i) {
        if (!nl_push_existing(&L, mesh, i)) { ok = false; break; }
        num_of_faces* n = &L.f[L.n - 1];
        if (any && !mesh->faces[i].obj_vert_selected) continue;
        for (int a = 0, b = n->face_type - 1; a < b; ++a, --b) {  /* reverse corner order, uv travels with its corner */
            uint32_t tv = n->vert_to_edge_array[a]; n->vert_to_edge_array[a] = n->vert_to_edge_array[b]; n->vert_to_edge_array[b] = tv;
            float tu = n->uv[a][0], tw = n->uv[a][1];
            n->uv[a][0] = n->uv[b][0]; n->uv[a][1] = n->uv[b][1];
            n->uv[b][0] = tu;          n->uv[b][1] = tw;
        }
    }
    if (ok) ok = em_rebuild(mesh, L.f, L.n, select_face);
    free(L.f);
    if (!ok) *err = "rebuild failed";
    return ok;
}