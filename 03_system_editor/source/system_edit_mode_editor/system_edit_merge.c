#include "system_globals.h"



/* ========================================================================= */
/* MERGE / COLLAPSE: all obj_vert_selected vertices become one, at their center       */
/* ========================================================================= */
bool edit_op_merge_selected(edit_mode_mesh* mesh, const char** err)
{
    uint32_t first = EM_NONE, cnt = 0;
    editor_vec3 c = { 0, 0, 0 };
    for (uint32_t i = 0; i < mesh->vert_count; ++i) {
        if (!mesh->verts[i].obj_vert_selected) continue;
        if (first == EM_NONE) first = i;
        c = add3(c, mesh->verts[i].obj_vert_pos); ++cnt;
    }
    if (cnt < 2) { *err = "select two or more vertices (key 1) first"; return false; }
    mesh->verts[first].obj_vert_pos = mul3(c, 1.0f / (float)cnt);

    uint32_t* map = (uint32_t*)malloc(sizeof(uint32_t) * mesh->vert_count);
    if (!map) { *err = "out of memory"; return false; }
    for (uint32_t i = 0; i < mesh->vert_count; ++i) map[i] = mesh->verts[i].obj_vert_selected ? first : i;
    const bool ok = edit_weld_with_map(mesh, map, first);
    free(map);
    if (!ok) *err = "merge failed";
    return ok;
}