#include "system_globals.h"
#include "system_edit_manager.h"

bool object_get_local_bounds(system_scene_object* o, editor_vec3* out_min, editor_vec3* out_max)
{
    if (!o || !out_min || !out_max) return false;
    if (is_terrain_object(o)) return false;          /* huge fixed grid: no size box, and no per frame scan */

    edit_mode_mesh* mesh = em_registry_get_or_create(o);               /* the same mesh Edit mode works on, so size follows live vertex edits */
    if (!mesh || mesh->vert_count == 0) return false;

    editor_vec3 lo = mesh->verts[0].obj_vert_pos, hi = lo;
    for (uint32_t i = 1; i < mesh->vert_count; ++i) {
        const editor_vec3 p = mesh->verts[i].obj_vert_pos;
        if (p.x < lo.x) lo.x = p.x;
        if (p.x > hi.x) hi.x = p.x;
        if (p.y < lo.y) lo.y = p.y;
        if (p.y > hi.y) hi.y = p.y;
        if (p.z < lo.z) lo.z = p.z;
        if (p.z > hi.z) hi.z = p.z;
    }
    *out_min = lo; *out_max = hi;
    return true;
}

bool get_object_mesh_size(system_scene_object* o, editor_vec3* out_size)
{
    editor_vec3 lo, hi;
    if (!out_size || !object_get_local_bounds(o, &lo, &hi)) return false;
    out_size->x = (hi.x - lo.x) * fabsf(o->scale.x);
    out_size->y = (hi.y - lo.y) * fabsf(o->scale.y);
    out_size->z = (hi.z - lo.z) * fabsf(o->scale.z);
    return true;
}

bool set_object_mesh_size_axis(system_scene_object* o, int axis, float size)
{
    if (axis < 0 || axis > 2 || size <= 0.0f) return false;
    editor_vec3 lo, hi;
    if (!object_get_local_bounds(o, &lo, &hi)) return false;

    const float extent = (axis == 0) ? hi.x - lo.x : (axis == 1) ? hi.y - lo.y : hi.z - lo.z;
    if (extent < 1e-6f) return false;                        /* flat in that axis: no scale can give it a size */

    float* s = (axis == 0) ? &o->scale.x : (axis == 1) ? &o->scale.y : &o->scale.z;
    const float sign = (*s < 0.0f) ? -1.0f : 1.0f;           /* a mirrored object stays mirrored */
    *s = sign * (size / extent);
    return true;
}

bool object_set_mesh_size(system_scene_object* o, editor_vec3 size)
{
    const bool x = set_object_mesh_size_axis(o, 0, size.x);
    const bool y = set_object_mesh_size_axis(o, 1, size.y);
    const bool z = set_object_mesh_size_axis(o, 2, size.z);
    return x && y && z;
}