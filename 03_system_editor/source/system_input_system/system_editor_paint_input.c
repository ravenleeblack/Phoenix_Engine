#include "engine_globals.h"
#include "system_globals.h"
#include "editor_input.h"

int brush_window_get_paint_brush(void);

/* state lives in the editor globals: g_paint_last, g_paint_was_painting, g_paint_prev_down, g_paint_last_frame */

/* ------------------------------------------------------------------------- */
/* own ray + mesh raycast (same idea as sculpt, but nothing is shared with it) */
/* ------------------------------------------------------------------------- */
editor_vec3 make_paint_vec3(float x, float y, float z)
{
    editor_vec3 result = { x, y, z };
    return result;
}

editor_vec3 subtract_paint_vec3(editor_vec3 a, editor_vec3 b)
{
    return make_paint_vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

float dot_paint_vec3(editor_vec3 a, editor_vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

editor_vec3 cross_paint_vec3(editor_vec3 a, editor_vec3 b)
{
    return make_paint_vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

editor_vec3 normalize_paint_vec3(editor_vec3 a)
{
    float length = sqrtf(dot_paint_vec3(a, a));
    if (length > 1e-12f) {
        return make_paint_vec3(a.x / length, a.y / length, a.z / length);
    }
    return make_paint_vec3(0.0f, 0.0f, 0.0f);
}

editor_vec3 transform_paint_point(const editor_mat4* matrix, editor_vec3 p)
{
    return make_paint_vec3(matrix->mesh[0][0] * p.x + matrix->mesh[1][0] * p.y + matrix->mesh[2][0] * p.z + matrix->mesh[3][0],
                           matrix->mesh[0][1] * p.x + matrix->mesh[1][1] * p.y + matrix->mesh[2][1] * p.z + matrix->mesh[3][1],
                           matrix->mesh[0][2] * p.x + matrix->mesh[1][2] * p.y + matrix->mesh[2][2] * p.z + matrix->mesh[3][2]);
}

/* world space ray through the mouse pixel: the same unproject the sculpt input uses (and its sculpt_pick_flip_y switch) */
bool build_paint_ray(const system_element_context* ui_element, editor_vec3* out_o, editor_vec3* out_d)
{
    return brush_mouse_world_ray(ui_element, out_o, out_d);
}

/* moller-trumbore, two sided. t = distance along the ray, u and v = barycentric of b and c. */
bool intersect_paint_triangle(editor_vec3 o, editor_vec3 d, editor_vec3 a, editor_vec3 b, editor_vec3 c,
                              float* out_t, float* out_u, float* out_v)
{
    const editor_vec3 e1 = subtract_paint_vec3(b, a);
    const editor_vec3 e2 = subtract_paint_vec3(c, a);
    const editor_vec3 p  = cross_paint_vec3(d, e2);
    const float det = dot_paint_vec3(e1, p);
    if (fabsf(det) < 1e-9f) return false;

    const float inv = 1.0f / det;
    const editor_vec3 s = subtract_paint_vec3(o, a);
    const float u = dot_paint_vec3(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;

    const editor_vec3 q = cross_paint_vec3(s, e1);
    const float v = dot_paint_vec3(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;

    const float t = dot_paint_vec3(e2, q) * inv;
    if (t <= 1e-5f) return false;

    *out_t = t;
    *out_u = u;
    *out_v = v;
    return true;
}

/* nearest face hit. the ray is in world space, the hit and the normal come back in object space (what paint_dab works in). */
bool raycast_paint_mesh(const edit_mode_mesh* mesh, const system_scene_object* obj, editor_vec3 ro, editor_vec3 rd, manage_paint_hit* out)
{
    const editor_mat4 model = system_math_model_matrix(obj->position, obj->rotation, obj->scale);
    float best_t = 1e30f;
    bool  found  = false;

    for (uint32_t f = 0; f < mesh->face_count; ++f) {
        const edit_mode_face* face = &mesh->faces[f];
        for (int k = 1; k + 1 < face->face_type; ++k) {                  /* fan: (0,k,k+1), same as em_triangulate */
            const editor_vec3 a = mesh->verts[face->vert_to_edge_array[0]].obj_vert_pos;
            const editor_vec3 b = mesh->verts[face->vert_to_edge_array[k]].obj_vert_pos;
            const editor_vec3 c = mesh->verts[face->vert_to_edge_array[k + 1]].obj_vert_pos;

            float t, u, v;
            if (!intersect_paint_triangle(ro, rd, transform_paint_point(&model, a), transform_paint_point(&model, b),
                                          transform_paint_point(&model, c), &t, &u, &v)) continue;
            if (t >= best_t) continue;

            best_t = t;
            found  = true;
            const float w = 1.0f - u - v;
            out->obj_vert_pos = make_paint_vec3(a.x * w + b.x * u + c.x * v, a.y * w + b.y * u + c.y * v, a.z * w + b.z * u + c.z * v);
            const editor_vec3 cr = cross_paint_vec3(subtract_paint_vec3(b, a), subtract_paint_vec3(c, a));
            out->nrm  = normalize_paint_vec3(cr);
            out->face = f;

            /* uv at the hit: barycentric blend of the three corner uvs (corner k of the face, not the shared vertex) */
            const float* ua = face->uv[0];
            const float* ub = face->uv[k];
            const float* uc = face->uv[k + 1];
            out->uv[0] = ua[0] * w + ub[0] * u + uc[0] * v;
            out->uv[1] = ua[1] * w + ub[1] * u + uc[1] * v;

            /* uv per metre: sqrt(uv area / surface area) of the triangle that was hit */
            const float area_pos = 0.5f * sqrtf(dot_paint_vec3(cr, cr));
            const float area_uv  = 0.5f * fabsf((ub[0] - ua[0]) * (uc[1] - ua[1]) - (uc[0] - ua[0]) * (ub[1] - ua[1]));
            out->uv_per_m = 0.0f;
            if (area_pos > 1e-12f) {
                out->uv_per_m = sqrtf(area_uv / area_pos);
            }

            out->uv_min[0] = face->uv[0][0];
            out->uv_max[0] = face->uv[0][0];
            out->uv_min[1] = face->uv[0][1];
            out->uv_max[1] = face->uv[0][1];
            for (int q = 1; q < face->face_type; ++q) {
                if (face->uv[q][0] < out->uv_min[0]) out->uv_min[0] = face->uv[q][0];
                if (face->uv[q][0] > out->uv_max[0]) out->uv_max[0] = face->uv[q][0];
                if (face->uv[q][1] < out->uv_min[1]) out->uv_min[1] = face->uv[q][1];
                if (face->uv[q][1] > out->uv_max[1]) out->uv_max[1] = face->uv[q][1];
            }
        }
    }
    return found;
}

/* ------------------------------------------------------------------------- */
/* per frame                                                                 */
/* ------------------------------------------------------------------------- */
void paint_input_update(system_element_context* ui_element)
{
    if (!ui_element) return;
    if (g_paint_last_frame == brush_frame_id()) return;           /* once per frame, whoever calls first */
    g_paint_last_frame = brush_frame_id();

    start_paint_mode();
    paint_clamp_settings();

    const bool down  = ui_element->input.mouse_down;
    const bool press = down && !g_paint_prev_down;
    g_paint_prev_down = down;

    system_scene_object* obj = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (obj && (obj->type != scene_object_mesh || is_terrain_object(obj))) obj = 0;   /* the terrain has its own brush (system_terrain_brush.c) */

    edit_mode_mesh* mesh = 0;
    if (obj) mesh = em_registry_get_or_create(obj);

    const bool usable = paint_mode_is_active() && mesh && !ui_element->input.alt_pressed && !ui_element->input.camera_manipulating
                        && !ui_element->input.middle_down && !is_context_menu_open()
                        && validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y,
                               ui_element->scene_view.offset_x, ui_element->scene_view.offset_y, ui_element->scene_view.width, ui_element->scene_view.height);
    if (!usable) {
        g_paint_was_painting = false;
        return;
    }

    editor_vec3 o, d;
    manage_paint_hit hit = {0};
    const bool have_hit = build_paint_ray(ui_element, &o, &d) && raycast_paint_mesh(mesh, obj, o, d, &hit);

    /* brush ring follows the mouse over the mesh */
    if (have_hit) {
        const editor_mat4 model = system_math_model_matrix(obj->position, obj->rotation, obj->scale);
        brush_cursor_set(ui_element, transform_paint_point(&model, hit.obj_vert_pos), g_paint.radius * obj->scale.x, g_paint.hardness, 0, g_paint.color);
    }

    if (!down) {
        g_paint_was_painting = false;
        return;
    }
    if (!g_paint_was_painting && !press) return;                  /* a stroke has to start with a click inside the view */
    if (!have_hit) return;

    if (!g_paint_was_painting) {
        brush_ensure_density();                                   /* a cube has 24 vertices: nothing to colour until it is subdivided */
        mesh = em_registry_get_or_create(obj);
        if (!mesh || !raycast_paint_mesh(mesh, obj, o, d, &hit)) return;
    }

    const int  brush    = brush_window_get_paint_brush();
    const bool airbrush = (brush == 1);
    if (g_paint_was_painting && !airbrush) {                      /* spacing, like sculpt */
        float dx = hit.obj_vert_pos.x - g_paint_last.x;
        float dy = hit.obj_vert_pos.y - g_paint_last.y;
        float dz = hit.obj_vert_pos.z - g_paint_last.z;
        if (sqrtf(dx * dx + dy * dy + dz * dz) < g_paint.spacing * g_paint.radius) return;
    }
    g_paint_was_painting = true;
    g_paint_last = hit.obj_vert_pos;

    const bool erase_now = input_is_key_down(phoenix_key_control);

    if (brush == 6) {                                             /* picker: take the colour from the paint texture */
        paint_texture_pick(obj->id, hit.uv, g_paint.color);
        return;
    }

    /* the paint map: this is what export png writes */
    paint_texture_dab(obj->id, hit.uv, hit.uv_min, hit.uv_max, hit.uv_per_m, brush, erase_now);

#if paint_also_vertex_color
    /* live feedback until the paint texture is shown on the mesh (gpu step). turn this off after that, or the paint is applied twice. */
    if (paint_dab(mesh, hit.obj_vert_pos, brush, erase_now)) em_commit_to_gpu(mesh, obj);
#endif
}