#include "engine_globals.h"
#include "system_globals.h"
#include "editor_input.h"

/* state lives in the editor globals: g_sculpt_stroke, g_sculpt_prev_left, g_sculpt_prev_mx, g_sculpt_last_frame */

/* ========================================================================= */
/* g_sculpt -> g_mode_state.sculpt (one way: g_sculpt is the source of truth)  */
/* ========================================================================= */
void sculpt_sync_mode_state(void)
{
    start_mode_state();
    start_sculpt_mode();
    const sculpt_brush_settings* s = sculpt_current_settings();
    g_mode_state.sculpt.brush_radius   = s->radius;
    g_mode_state.sculpt.brush_strength = s->strength;
    g_mode_state.sculpt.brush_falloff  = s->hardness;
    g_mode_state.sculpt.symmetry_x     = g_sculpt.sym_x;
    g_mode_state.sculpt.invert         = g_sculpt.invert;
}

/* ========================================================================= */
/* math: 4x4 inverse (flat, column-major, index = col * 4 + row), rays        */
/* your rule says math goes in the math file: these can move to engine_math.c unchanged. */
/* ========================================================================= */
bool invert_sculpt_mat4(const float* in, float* out)
{
    double a[4][8];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            a[r][c] = in[c * 4 + r];
            a[r][4 + c] = 0.0;
            if (r == c) a[r][4 + c] = 1.0;
        }
    }

    for (int col = 0; col < 4; col++) {
        int piv = col;
        for (int r = col + 1; r < 4; r++) {
            if (fabs(a[r][col]) > fabs(a[piv][col])) piv = r;
        }
        if (fabs(a[piv][col]) < 1e-12) return false;
        if (piv != col) {
            for (int k = 0; k < 8; k++) {
                double t = a[col][k];
                a[col][k] = a[piv][k];
                a[piv][k] = t;
            }
        }

        const double d = a[col][col];
        for (int k = 0; k < 8; k++) a[col][k] /= d;
        for (int r = 0; r < 4; r++) {
            if (r == col) continue;
            const double f = a[r][col];
            if (f != 0.0) {
                for (int k = 0; k < 8; k++) a[r][k] -= f * a[col][k];
            }
        }
    }
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) out[c * 4 + r] = (float)a[r][4 + c];
    }
    return true;
}

void multiply_sculpt_mat4_point(const float* matrix, const float v[4], float out[4])
{
    for (int r = 0; r < 4; r++) {
        out[r] = matrix[0 * 4 + r] * v[0] + matrix[1 * 4 + r] * v[1] + matrix[2 * 4 + r] * v[2] + matrix[3 * 4 + r] * v[3];
    }
}

editor_vec3 subtract_sculpt_vec3(editor_vec3 a, editor_vec3 b)
{
    editor_vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

editor_vec3 add_sculpt_vec3(editor_vec3 a, editor_vec3 b)
{
    editor_vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}

editor_vec3 scale_sculpt_vec3(editor_vec3 a, float s)
{
    editor_vec3 r = { a.x * s, a.y * s, a.z * s };
    return r;
}

float dot_sculpt_vec3(editor_vec3 a, editor_vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float get_sculpt_vec3_length(editor_vec3 a)
{
    return sqrtf(dot_sculpt_vec3(a, a));
}

editor_vec3 cross_sculpt_vec3(editor_vec3 a, editor_vec3 b)
{
    editor_vec3 r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    return r;
}

editor_vec3 normalize_sculpt_vec3(editor_vec3 a)
{
    const float length = get_sculpt_vec3_length(a);
    if (length < 1e-8f) {
        editor_vec3 zero = { 0.0f, 0.0f, 0.0f };
        return zero;
    }
    return scale_sculpt_vec3(a, 1.0f / length);
}

/* mouse -> ray in world space. o = near plane point, d = unit direction. shared with the paint input. */
bool brush_mouse_world_ray(const system_element_context* ui_element, editor_vec3* o, editor_vec3* d)
{
    const struct_viewport_bounds* b = &ui_element->scene_view;
    if (b->width <= 0 || b->height <= 0) return false;

    editor_mat4 view, proj;
    build_scene_view_camera((float)b->width / (float)b->height, &view, &proj);
    const editor_mat4 vp = system_math_mat4_multiply(proj, view);
    float inv_vp[16];
    if (!invert_sculpt_mat4(&vp.mesh[0][0], inv_vp)) return false;

    float nx = 2.0f * (float)(ui_element->input.mouse_x - b->offset_x) / (float)b->width  - 1.0f;
    float ny = 2.0f * (float)(ui_element->input.mouse_y - b->offset_y) / (float)b->height - 1.0f;
    if (sculpt_pick_flip_y) ny = -ny;

    const float cn[4] = { nx, ny, 0.0f, 1.0f };
    const float cf[4] = { nx, ny, 1.0f, 1.0f };
    float wn[4], wf[4];
    multiply_sculpt_mat4_point(inv_vp, cn, wn);
    multiply_sculpt_mat4_point(inv_vp, cf, wf);
    if (fabsf(wn[3]) < 1e-9f || fabsf(wf[3]) < 1e-9f) return false;
    for (int i = 0; i < 3; i++) {
        wn[i] /= wn[3];
        wf[i] /= wf[3];
    }

    const editor_vec3 a = { wn[0], wn[1], wn[2] };
    const editor_vec3 c = { wf[0], wf[1], wf[2] };
    *o = a;
    *d = normalize_sculpt_vec3(subtract_sculpt_vec3(c, a));
    return get_sculpt_vec3_length(*d) > 0.5f;
}

/* meshes with too few vertices cannot be sculpted or painted (a cube has 24): subdivide flat, not rounded, up to ~1000+ faces */
void brush_ensure_density(void)
{
    const bool keep = g_sculpt.smooth_subdivide;
    g_sculpt.smooth_subdivide = false;
    for (int i = 0; i < 4; i++) {
        uint32_t vert_count = 0, face_count = 0;
        if (!sculpt_selected_mesh_counts(&vert_count, &face_count) || face_count >= 1000) break;
        if (!sculpt_subdivide_selected()) break;
    }
    g_sculpt.smooth_subdivide = keep;
}

/* object space point -> world */
editor_vec3 transform_sculpt_object_point(const system_scene_object* obj, editor_vec3 p)
{
    const editor_mat4 m = system_math_model_matrix(obj->position, obj->rotation, obj->scale);
    editor_vec3 w = { m.mesh[0][0] * p.x + m.mesh[1][0] * p.y + m.mesh[2][0] * p.z + m.mesh[3][0],
                      m.mesh[0][1] * p.x + m.mesh[1][1] * p.y + m.mesh[2][1] * p.z + m.mesh[3][1],
                      m.mesh[0][2] * p.x + m.mesh[1][2] * p.y + m.mesh[2][2] * p.z + m.mesh[3][2] };
    return w;
}

/* mouse -> ray in the object space of obj. o = point on the near plane, d = far point - near point (t runs 0..1). */
bool build_sculpt_object_ray(system_element_context* ui_element, const system_scene_object* obj, editor_vec3* o, editor_vec3* d)
{
    const struct_viewport_bounds* b = &ui_element->scene_view;
    if (b->width <= 0 || b->height <= 0) return false;

    editor_mat4 view, proj;
    build_scene_view_camera((float)b->width / (float)b->height, &view, &proj);
    const editor_mat4 vp = system_math_mat4_multiply(proj, view);

    float inv_vp[16];
    if (!invert_sculpt_mat4(&vp.mesh[0][0], inv_vp)) return false;

    float nx = 2.0f * (float)(ui_element->input.mouse_x - b->offset_x) / (float)b->width  - 1.0f;
    float ny = 2.0f * (float)(ui_element->input.mouse_y - b->offset_y) / (float)b->height - 1.0f;
    if (sculpt_pick_flip_y) ny = -ny;

    const float cn[4] = { nx, ny, 0.0f, 1.0f };
    const float cf[4] = { nx, ny, 1.0f, 1.0f };
    float wn[4], wf[4];
    multiply_sculpt_mat4_point(inv_vp, cn, wn);
    multiply_sculpt_mat4_point(inv_vp, cf, wf);
    if (fabsf(wn[3]) < 1e-9f || fabsf(wf[3]) < 1e-9f) return false;
    for (int i = 0; i < 3; i++) {
        wn[i] /= wn[3];
        wf[i] /= wf[3];
    }
    wn[3] = 1.0f;
    wf[3] = 1.0f;

    const editor_mat4 model = system_math_model_matrix(obj->position, obj->rotation, obj->scale);
    float inv_m[16];
    if (!invert_sculpt_mat4(&model.mesh[0][0], inv_m)) return false;

    float on[4], of[4];
    multiply_sculpt_mat4_point(inv_m, wn, on);
    multiply_sculpt_mat4_point(inv_m, wf, of);
    o->x = on[0];
    o->y = on[1];
    o->z = on[2];
    d->x = of[0] - on[0];
    d->y = of[1] - on[1];
    d->z = of[2] - on[2];
    return true;
}

/* two sided moller-trumbore. returns t in [0, best) or a negative number for a miss. */
float intersect_sculpt_triangle(editor_vec3 o, editor_vec3 d, editor_vec3 v0, editor_vec3 v1, editor_vec3 v2, float best)
{
    const editor_vec3 e1 = subtract_sculpt_vec3(v1, v0);
    const editor_vec3 e2 = subtract_sculpt_vec3(v2, v0);
    const editor_vec3 p  = cross_sculpt_vec3(d, e2);
    const float det = dot_sculpt_vec3(e1, p);
    if (fabsf(det) < 1e-12f) return -1.0f;

    const float inv = 1.0f / det;
    const editor_vec3 s = subtract_sculpt_vec3(o, v0);
    const float u = dot_sculpt_vec3(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return -1.0f;

    const editor_vec3 q = cross_sculpt_vec3(s, e1);
    const float v = dot_sculpt_vec3(d, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return -1.0f;

    const float t = dot_sculpt_vec3(e2, q) * inv;
    if (t >= 0.0f && t < best) return t;
    return -1.0f;
}

bool raycast_sculpt_mesh(const edit_mode_mesh* mesh, editor_vec3 o, editor_vec3 d, editor_vec3* out_point, editor_vec3* out_normal)
{
    float best = 1.0f + 1e-4f;            /* the far plane */
    bool  hit  = false;
    editor_vec3 n = { 0.0f, 0.0f, 1.0f };

    for (uint32_t f = 0; f < mesh->face_count; f++) {
        const edit_mode_face* face = &mesh->faces[f];
        int tris = 1;
        if (face->face_type == 4) tris = 2;

        for (int k = 0; k < tris; k++) {
            const uint32_t i0 = face->vert_to_edge_array[0];
            const uint32_t i1 = face->vert_to_edge_array[k + 1];
            const uint32_t i2 = face->vert_to_edge_array[k + 2];
            if (i0 >= mesh->vert_count || i1 >= mesh->vert_count || i2 >= mesh->vert_count) continue;

            const editor_vec3 a = mesh->verts[i0].obj_vert_pos;
            const editor_vec3 b = mesh->verts[i1].obj_vert_pos;
            const editor_vec3 c = mesh->verts[i2].obj_vert_pos;
            const float t = intersect_sculpt_triangle(o, d, a, b, c, best);
            if (t < 0.0f) continue;

            best = t;
            hit  = true;
            n = normalize_sculpt_vec3(cross_sculpt_vec3(subtract_sculpt_vec3(b, a), subtract_sculpt_vec3(c, a)));
        }
    }
    if (!hit) return false;
    if (dot_sculpt_vec3(n, d) > 0.0f) n = scale_sculpt_vec3(n, -1.0f);   /* face the viewer */
    *out_point  = add_sculpt_vec3(o, scale_sculpt_vec3(d, best));
    *out_normal = n;
    return true;
}

/* ========================================================================= */
/* the stroke                                                                 */
/* ========================================================================= */
bool sculpt_input_is_stroking(void)
{
    return g_sculpt_stroke.active;
}

bool check_sculpt_scene_view(const system_element_context* ui_element)
{
    const struct_viewport_bounds* b = &ui_element->scene_view;
    return b->width > 0 && b->height > 0 &&
           validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, b->offset_x, b->offset_y, b->width, b->height);
}

system_scene_object* find_sculpt_selected_mesh(void)
{
    system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (o && o->type == scene_object_mesh && !is_terrain_object(o)) return o;   /* the terrain has its own brush (system_terrain_brush.c) */
    return 0;
}

void end_sculpt_stroke(edit_mode_mesh* mesh, system_scene_object* obj)
{
    if (!g_sculpt_stroke.active) return;
    sculpt_stroke_end();
    g_sculpt_stroke.active = false;
    if (mesh && obj) em_commit_to_gpu(mesh, obj);
}

void sculpt_input_update(system_element_context* ui_element)
{
    if (!ui_element) return;
    if (g_sculpt_last_frame == brush_frame_id()) return;          /* once per frame, whoever calls first */
    g_sculpt_last_frame = brush_frame_id();
    sculpt_sync_mode_state();

    const bool left  = ui_element->input.mouse_down;
    const bool press = left && !g_sculpt_prev_left;
    const int  dx    = ui_element->input.mouse_x - g_sculpt_prev_mx;
    g_sculpt_prev_left = left;
    g_sculpt_prev_mx   = ui_element->input.mouse_x;

    system_scene_object* obj = find_sculpt_selected_mesh();
    edit_mode_mesh* mesh = 0;
    if (obj) mesh = em_registry_get_or_create(obj);

    /* anything that should stop a stroke */
    if (g_sculpt_stroke.active && (!left || !sculpt_mode_is_active() || !obj || !mesh || obj->id != g_sculpt_stroke.object_id ||
                                   ui_element->input.alt_pressed || ui_element->input.camera_manipulating || is_context_menu_open())) {
        edit_mode_mesh* stroke_mesh = 0;
        if (mesh && obj && obj->id == g_sculpt_stroke.object_id) stroke_mesh = mesh;
        end_sculpt_stroke(stroke_mesh, obj);
        return;
    }

    if (!sculpt_mode_is_active() || !obj || !mesh || is_context_menu_open()) return;
    if (ui_element->input.alt_pressed || ui_element->input.camera_manipulating) return;     /* maya: alt is the camera */

    sculpt_brush_settings* s = sculpt_current_settings();

    /* brush ring: follows the mouse over the mesh (also while no button is down) */
    if (check_sculpt_scene_view(ui_element)) {
        editor_vec3 ho, hd, hp, hn;
        if (build_sculpt_object_ray(ui_element, obj, &ho, &hd) && raycast_sculpt_mesh(mesh, ho, hd, &hp, &hn)) {
            const float tint[3] = { 1.0f, 0.45f, 0.45f };
            const bool  inverted = (g_sculpt.invert != input_is_key_down(phoenix_key_control));
            const float* ring_tint = 0;
            if (inverted) ring_tint = tint;
            brush_cursor_set(ui_element, transform_sculpt_object_point(obj, hp), s->radius * obj->scale.x, s->hardness, ring_tint, 0);
        }
    }

    /* maya: b + drag changes the brush size */
    if (left && check_sculpt_scene_view(ui_element) && input_is_key_down(phoenix_key_b)) {
        s->radius *= 1.0f + (float)dx * 0.01f;
        sculpt_clamp_settings(s);
        return;
    }

    const bool invert_now = input_is_key_down(phoenix_key_control);

    /* ---- stroke start ---- */
    if (!g_sculpt_stroke.active) {
        if (!press || !check_sculpt_scene_view(ui_element)) return;

        brush_ensure_density();                                   /* a cube / plane has too few vertices to move */
        mesh = em_registry_get_or_create(obj);
        if (!mesh) return;

        editor_vec3 o, d, hit, nrm;
        if (!build_sculpt_object_ray(ui_element, obj, &o, &d)) return;
        if (!raycast_sculpt_mesh(mesh, o, d, &hit, &nrm)) return;

        g_sculpt_stroke.active    = true;
        g_sculpt_stroke.object_id = obj->id;
        g_sculpt_stroke.start     = hit;
        g_sculpt_stroke.last_dab  = hit;
        g_sculpt_stroke.plane_n   = normalize_sculpt_vec3(d);
        sculpt_stroke_begin(mesh, hit, nrm);
        if (sculpt_stroke_update(mesh, hit, nrm, (editor_vec3){ 0.0f, 0.0f, 0.0f }, invert_now)) em_commit_to_gpu(mesh, obj);
        return;
    }

    /* ---- stroke continues ---- */
    editor_vec3 o, d;
    if (!build_sculpt_object_ray(ui_element, obj, &o, &d)) return;

    if (sculpt_get_brush() == sculpt_brush_grab) {
        /* drag on the plane through the grab point: stays steady even though the surface moves under the cursor */
        const float denom = dot_sculpt_vec3(d, g_sculpt_stroke.plane_n);
        if (fabsf(denom) < 1e-8f) return;
        const float t = dot_sculpt_vec3(subtract_sculpt_vec3(g_sculpt_stroke.start, o), g_sculpt_stroke.plane_n) / denom;
        const editor_vec3 now = add_sculpt_vec3(o, scale_sculpt_vec3(d, t));
        if (sculpt_stroke_update(mesh, g_sculpt_stroke.start, g_sculpt_stroke.plane_n, subtract_sculpt_vec3(now, g_sculpt_stroke.start), invert_now)) em_commit_to_gpu(mesh, obj);
        return;
    }

    editor_vec3 hit, nrm;
    if (!raycast_sculpt_mesh(mesh, o, d, &hit, &nrm)) return;                                   /* off the mesh: no dab */
    if (get_sculpt_vec3_length(subtract_sculpt_vec3(hit, g_sculpt_stroke.last_dab)) < s->spacing * s->radius) return;   /* spacing: a dab every fraction of the radius */

    g_sculpt_stroke.last_dab = hit;
    if (sculpt_stroke_update(mesh, hit, nrm, (editor_vec3){ 0.0f, 0.0f, 0.0f }, invert_now)) em_commit_to_gpu(mesh, obj);
}