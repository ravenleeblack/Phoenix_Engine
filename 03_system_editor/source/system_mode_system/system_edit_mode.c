#include "system_globals.h"
bool system_transform_gizmo_wants_mouse(void);

/* Defined in system_grid.c: the camera the Scene view really renders with. */
/* the Scene view is rendered with the editor fly camera (system_scene_camera.c): picking and overlays must use the same one */
extern void build_scene_view_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj);

/* If the overlay is mirrored vertically, flip this. (Your gimbal code proves the projection is already Y-flipped, so 0.) */
#define EDIT_PROJ_Y_FLIP        0

#define EDIT_PICK_VERT_PX   10.0f
#define EDIT_PICK_EDGE_PX    8.0f

typedef struct { float sx, sy, z; uint8_t ok; } em_proj;

enum { ST_NONE = 0, ST_EDIT, ST_OUTLINE, ST_BRACKET };

static int                  s_state;
static edit_mode_mesh*             s_mesh;
static system_scene_object* s_obj;
static em_proj*             s_proj;     static uint32_t s_proj_cap;
static uint8_t*             s_vvis;     static uint32_t s_vvis_cap;
static uint8_t*             s_front;    static uint32_t s_front_cap;
static editor_vec3*         s_wpos;     static uint32_t s_wpos_cap;     /* every vertex in world space */
static editor_mat4          s_view_mat, s_proj_mat;                     /* the camera used by the last prepare() */
static editor_vec3          s_eye;                                      /* camera position, world space */
static uint32_t             s_hover = EM_NONE;
static bool                 s_over_view;
static bool                 s_prev_down;
static bool                 s_have_mode;
static enum_mesh_selection  s_last_mode;

static const float C_WIRE[4]       = { 0.05f, 0.05f, 0.05f, 0.85f };
static const float C_VERT[4]       = { 0.02f, 0.02f, 0.02f, 1.00f };
static const float C_SEL[4]        = { 1.00f, 0.60f, 0.10f, 1.00f };
static const float C_HOVER[4]      = { 1.00f, 1.00f, 1.00f, 1.00f };

bool system_edit_mode_wants_mouse(void) { return (s_state == ST_EDIT && s_mesh != NULL && s_over_view); }

/* ------------------------------------------------------------------------- */
/* small vector helpers                                                      */
/* ------------------------------------------------------------------------- */
static editor_vec3 make_vec3(float x, float y, float z) { editor_vec3 r = { x, y, z }; return r; }
static editor_vec3 sub_vec3(editor_vec3 a, editor_vec3 b) { return make_vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
static float       dot_vec3(editor_vec3 a, editor_vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static editor_vec3 cross_vec3(editor_vec3 a, editor_vec3 b)
{
    return make_vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static editor_vec3 normalize_vec3(editor_vec3 a)
{
    float l = sqrtf(dot_vec3(a, a));
    return l > 1e-12f ? make_vec3(a.x / l, a.y / l, a.z / l) : make_vec3(0, 0, 0);
}

/* column-major point transform (same layout project_point reads) */
static editor_vec3 transform_point(const editor_mat4* M, editor_vec3 p)
{
    return make_vec3(M->mesh[0][0] * p.x + M->mesh[1][0] * p.y + M->mesh[2][0] * p.z + M->mesh[3][0],
                     M->mesh[0][1] * p.x + M->mesh[1][1] * p.y + M->mesh[2][1] * p.z + M->mesh[3][1],
                     M->mesh[0][2] * p.x + M->mesh[1][2] * p.y + M->mesh[2][2] * p.z + M->mesh[3][2]);
}

/* ------------------------------------------------------------------------- */
/* projection                                                                */
/* ------------------------------------------------------------------------- */
static void project_point(const editor_mat4* M, editor_vec3 p, const struct_viewport_bounds* b, em_proj* o)
{
    float cx = M->mesh[0][0] * p.x + M->mesh[1][0] * p.y + M->mesh[2][0] * p.z + M->mesh[3][0];
    float cy = M->mesh[0][1] * p.x + M->mesh[1][1] * p.y + M->mesh[2][1] * p.z + M->mesh[3][1];
    float cz = M->mesh[0][2] * p.x + M->mesh[1][2] * p.y + M->mesh[2][2] * p.z + M->mesh[3][2];
    float cw = M->mesh[0][3] * p.x + M->mesh[1][3] * p.y + M->mesh[2][3] * p.z + M->mesh[3][3];
    if (cw < 1e-5f) { o->ok = 0; return; }
    float nx = cx / cw, ny = cy / cw;
#if EDIT_PROJ_Y_FLIP
    ny = -ny;
#endif
    o->sx = (float)b->offset_x + (nx * 0.5f + 0.5f) * (float)b->width;
    o->sy = (float)b->offset_y + (ny * 0.5f + 0.5f) * (float)b->height;
    o->z  = cz / cw;
    o->ok = 1;
}

/* camera position in world space, read back out of the view matrix: eye = -(R^T * t) */
static editor_vec3 compute_camera_eye(const editor_mat4* vert_to_edge_array)
{
    const editor_vec3 r0 = make_vec3(vert_to_edge_array->mesh[0][0], vert_to_edge_array->mesh[1][0], vert_to_edge_array->mesh[2][0]);
    const editor_vec3 r1 = make_vec3(vert_to_edge_array->mesh[0][1], vert_to_edge_array->mesh[1][1], vert_to_edge_array->mesh[2][1]);
    const editor_vec3 r2 = make_vec3(vert_to_edge_array->mesh[0][2], vert_to_edge_array->mesh[1][2], vert_to_edge_array->mesh[2][2]);
    const float t0 = vert_to_edge_array->mesh[3][0], t1 = vert_to_edge_array->mesh[3][1], t2 = vert_to_edge_array->mesh[3][2];
    return make_vec3(-(t0 * r0.x + t1 * r1.x + t2 * r2.x),
                     -(t0 * r0.y + t1 * r1.y + t2 * r2.y),
                     -(t0 * r0.z + t1 * r1.z + t2 * r2.z));
}

/* world space ray through a pixel of the Scene view. Works with any projection that flips Y or handedness,
   because it reads the signs straight out of the matrices. */
static void build_mouse_ray(const editor_mat4* view, const editor_mat4* proj, const struct_viewport_bounds* b,
                            float mx, float my, editor_vec3* out_o, editor_vec3* out_d)
{
    float nx = ((mx - (float)b->offset_x) / (float)b->width)  * 2.0f - 1.0f;
    float ny = ((my - (float)b->offset_y) / (float)b->height) * 2.0f - 1.0f;
#if EDIT_PROJ_Y_FLIP
    ny = -ny;
#endif
    float p00 = proj->mesh[0][0], p11 = proj->mesh[1][1], p23 = proj->mesh[2][3];
    if (fabsf(p00) < 1e-8f) p00 = 1.0f;
    if (fabsf(p11) < 1e-8f) p11 = 1.0f;
    if (fabsf(p23) < 1e-8f) p23 = -1.0f;

    /* view space direction: w = p23 * z must be positive in front of the camera */
    const float vz = (p23 > 0.0f) ? 1.0f : -1.0f;
    const float vx = nx * fabsf(p23) / p00;
    const float vy = ny * fabsf(p23) / p11;

    const editor_vec3 r0 = make_vec3(view->mesh[0][0], view->mesh[1][0], view->mesh[2][0]);
    const editor_vec3 r1 = make_vec3(view->mesh[0][1], view->mesh[1][1], view->mesh[2][1]);
    const editor_vec3 r2 = make_vec3(view->mesh[0][2], view->mesh[1][2], view->mesh[2][2]);
    editor_vec3 d = make_vec3(vx * r0.x + vy * r1.x + vz * r2.x,
                              vx * r0.y + vy * r1.y + vz * r2.y,
                              vx * r0.z + vy * r1.z + vz * r2.z);
    *out_d = normalize_vec3(d);
    *out_o = compute_camera_eye(view);
}

/* Moller-Trumbore, two sided. t is the distance along d. */
static bool intersect_ray_triangle(editor_vec3 o, editor_vec3 d, editor_vec3 a, editor_vec3 b, editor_vec3 c, float* out_t)
{
    const editor_vec3 e1 = sub_vec3(b, a), e2 = sub_vec3(c, a);
    const editor_vec3 p  = cross_vec3(d, e2);
    const float det = dot_vec3(e1, p);
    if (fabsf(det) < 1e-9f) return false;
    const float inv = 1.0f / det;
    const editor_vec3 s = sub_vec3(o, a);
    const float u = dot_vec3(s, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const editor_vec3 q = cross_vec3(s, e1);
    const float vert_to_edge_array = dot_vec3(d, q) * inv;
    if (vert_to_edge_array < 0.0f || u + vert_to_edge_array > 1.0f) return false;
    const float t = dot_vec3(e2, q) * inv;
    if (t <= 1e-5f) return false;
    *out_t = t;
    return true;
}

/* does the face look at the camera? Uses the world space normal and the camera position, no screen space winding guess. */
static bool check_face_facing(const edit_mode_face* F)
{
    editor_vec3 n = make_vec3(0, 0, 0), c = make_vec3(0, 0, 0);
    for (int k = 0; k < F->face_type; ++k) {                         /* Newell's method: tris and quads */
        const editor_vec3 p = s_wpos[F->vert_to_edge_array[k]], q = s_wpos[F->vert_to_edge_array[(k + 1) % F->face_type]];
        n.x += (p.y - q.y) * (p.z + q.z);
        n.y += (p.z - q.z) * (p.x + q.x);
        n.z += (p.x - q.x) * (p.y + q.y);
        c.x += p.x; c.y += p.y; c.z += p.z;
    }
    const float inv = 1.0f / (float)F->face_type;
    c = make_vec3(c.x * inv, c.y * inv, c.z * inv);
    return dot_vec3(n, sub_vec3(s_eye, c)) > 0.0f;
}

static bool grow(void** p, size_t elem, uint32_t* cap, uint32_t need)
{
    if (need <= *cap) return true;
    uint32_t nc = *cap ? *cap : 64;
    while (nc < need) nc *= 2;
    void* n = realloc(*p, (size_t)nc * elem);
    if (!n) return false;
    *p = n; *cap = nc;
    return true;
}

static bool prepare(system_element_context* ui_element, system_scene_object* obj, edit_mode_mesh* mesh, bool respect_xray)
{
    const struct_viewport_bounds b = ui_element->scene_view;
    if (b.width <= 0 || b.height <= 0) return false;
    if (!grow((void**)&s_proj,  sizeof(em_proj),     &s_proj_cap,  mesh->vert_count)) return false;
    if (!grow((void**)&s_vvis,  1,                   &s_vvis_cap,  mesh->vert_count)) return false;
    if (!grow((void**)&s_front, 1,                   &s_front_cap, mesh->face_count)) return false;
    if (!grow((void**)&s_wpos,  sizeof(editor_vec3), &s_wpos_cap,  mesh->vert_count)) return false;

    build_scene_view_camera((float)b.width / (float)b.height, &s_view_mat, &s_proj_mat);
    editor_mat4 model = system_math_model_matrix(obj->position, obj->rotation, obj->scale);
    editor_mat4 mvp   = system_math_mat4_multiply(s_proj_mat, system_math_mat4_multiply(s_view_mat, model));
    s_eye = compute_camera_eye(&s_view_mat);

    for (uint32_t i = 0; i < mesh->vert_count; ++i) {
        project_point(&mvp, mesh->verts[i].obj_vert_pos, &b, &s_proj[i]);
        s_wpos[i] = transform_point(&model, mesh->verts[i].obj_vert_pos);
        s_vvis[i] = 0;
    }

    const bool xray = respect_xray && g_mode_state.edit.xray;
    for (uint32_t f = 0; f < mesh->face_count; ++f) {
        const edit_mode_face* F = &mesh->faces[f];
        bool all_ok = true;
        for (int k = 0; k < F->face_type; ++k) all_ok &= (s_proj[F->vert_to_edge_array[k]].ok != 0);
        bool front = false;
        if (all_ok) front = xray ? true : check_face_facing(F);
        s_front[f] = front ? 1 : 0;
        if (front) for (int k = 0; k < F->face_type; ++k) s_vvis[F->vert_to_edge_array[k]] = 1;
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/* picking                                                                   */
/* ------------------------------------------------------------------------- */
static float dist_point_segment(float px, float py, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay;
    float l2 = dx * dx + dy * dy;
    float t = l2 > 1e-6f ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0.0f;
    if (t < 0) t = 0; else if (t > 1) t = 1;
    float cx = ax + dx * t - px, cy = ay + dy * t - py;
    return sqrtf(cx * cx + cy * cy);
}

static uint32_t pick(const edit_mode_mesh* mesh, enum_mesh_selection mode, const struct_viewport_bounds* b, float mx, float my)
{
    uint32_t best = EM_NONE;

    if (mode == select_vert) {
        float bd = EDIT_PICK_VERT_PX;
        for (uint32_t i = 0; i < mesh->vert_count; ++i) {
            if (!s_proj[i].ok || !s_vvis[i]) continue;
            float dx = s_proj[i].sx - mx, dy = s_proj[i].sy - my;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < bd) { bd = d; best = i; }
        }
    }
    else if (mode == select_edge) {
        float bd = EDIT_PICK_EDGE_PX;
        for (uint32_t i = 0; i < mesh->edge_count; ++i) {
            const edit_mode_edge* e = &mesh->edges[i];
            if (!s_proj[e->vert_to_edge_array[0]].ok || !s_proj[e->vert_to_edge_array[1]].ok) continue;
            bool vis = false;
            for (uint32_t k = 0; k < e->face_count; ++k) vis |= (s_front[e->face[k]] != 0);
            if (!vis) continue;
            float d = dist_point_segment(mx, my, s_proj[e->vert_to_edge_array[0]].sx, s_proj[e->vert_to_edge_array[0]].sy,
                                                 s_proj[e->vert_to_edge_array[1]].sx, s_proj[e->vert_to_edge_array[1]].sy);
            if (d < bd) { bd = d; best = i; }
        }
    }
    else {
        /* faces: a real ray from the mouse against the world space triangles, nearest hit wins */
        editor_vec3 ro, rd;
        build_mouse_ray(&s_view_mat, &s_proj_mat, b, mx, my, &ro, &rd);
        float best_t = 1e30f;
        for (uint32_t f = 0; f < mesh->face_count; ++f) {
            if (!s_front[f]) continue;
            const edit_mode_face* F = &mesh->faces[f];
            float t;
            if (intersect_ray_triangle(ro, rd, s_wpos[F->vert_to_edge_array[0]], s_wpos[F->vert_to_edge_array[1]], s_wpos[F->vert_to_edge_array[2]], &t) && t < best_t) { best_t = t; best = f; }
            if (F->face_type == 4 &&
                intersect_ray_triangle(ro, rd, s_wpos[F->vert_to_edge_array[0]], s_wpos[F->vert_to_edge_array[2]], s_wpos[F->vert_to_edge_array[3]], &t) && t < best_t) { best_t = t; best = f; }
        }
    }
    return best;
}

/* ------------------------------------------------------------------------- */
/* update                                                                    */
/* ------------------------------------------------------------------------- */
void system_edit_mode_update(system_element_context* ui_element)
{
    s_state = ST_NONE; s_mesh = NULL; s_obj = NULL; s_hover = EM_NONE; s_over_view = false;
    em_gpu_tick();      /* retire old GPU buffers; must run every frame, in any mode */

    bool down = ui_element->input.mouse_down;
    bool press = down && !s_prev_down;
    s_prev_down = down;

    system_scene_object* obj = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (!obj || obj == &g_scene_registry.objects[0]) { s_have_mode = false; return; }   /* nothing / root */

    /* ---------------- Object mode: outline the obj_vert_selected object ---------------- */
    if (ui_element->active_mode == mode_object) {
        s_have_mode = false;
        if (g_scene_view.hide_selection) { s_state = ST_NONE; return; }   /* Selection Highlight button off: no outline / brackets */
        edit_mode_mesh* mesh = (obj->type == scene_object_mesh) ? em_registry_get_or_create(obj) : NULL;
        if (mesh && prepare(ui_element, obj, mesh, false)) { s_mesh = mesh; s_obj = obj; s_state = ST_OUTLINE; }
        else                                  { s_obj = obj; s_state = ST_BRACKET; }
        return;
    }
    if (ui_element->active_mode != mode_edit_mesh) { s_have_mode = false; return; }

    /* ---------------- Edit mode: component selection ---------------- */
    if (is_terrain_object(obj) && g_terrain_tool.tool != terrain_tool_off) {
        s_have_mode = false;                 /* a terrain brush tool is on (Modeler window): drag on the ground, no control grid */
        s_state = ST_NONE;
        return;
    }
    edit_mode_mesh* mesh = is_terrain_object(obj) ? terrain_edit_mesh_acquire(obj) : em_registry_get_or_create(obj);
    if (!mesh) mesh = terrain_edit_mesh_acquire(obj);                   /* Terrain / region_xx: one chunk's control grid */
    if (!mesh) { s_obj = obj; s_state = ST_BRACKET; return; }       /* camera: not component-editable */

    /* 1 / 2 / 3 switch vert / edge_to_face_array / face (digit keys aren't in the keys enum, so cast) */
    static const struct { int code; enum_mesh_selection mode; } hot[3] = {
        { 0x31, select_vert }, { 0x32, select_edge }, { 0x33, select_face }
    };
    for (int i = 0; i < 3; ++i) {
        keys k = (keys)hot[i].code;
        if (input_is_key_down(k) && input_was_key_up(k)) ui_element->active_select_type = hot[i].mode;
    }

    if (s_have_mode && s_last_mode != ui_element->active_select_type) {
        em_sync_selection(mesh, s_last_mode);
        em_sync_selection(mesh, ui_element->active_select_type);
    }
    s_last_mode = ui_element->active_select_type;
    s_have_mode = true;

    if (!prepare(ui_element, obj, mesh, true)) return;
    s_mesh = mesh; s_obj = obj; s_state = ST_EDIT;

    const struct_viewport_bounds b = ui_element->scene_view;
    int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    s_over_view = validate_rectangle_hit(mx, my, b.offset_x, b.offset_y, b.width, b.height);

    bool navigating = ui_element->input.alt_pressed || ui_element->input.middle_down || ui_element->input.camera_manipulating;
    if (!s_over_view || navigating) return;

    s_hover = pick(mesh, ui_element->active_select_type, &b, (float)mx, (float)my);

    /* click = select the hovered vert / edge / face. shift or ctrl + click = add it to the selection (or remove it if it is already in).
       Not while a modeling tool owns the mouse (Target Weld, Insert Loop), over the manipulator overlay, or while something else holds the mouse. */
    if (press && s_hover != EM_NONE && ui_element->active_id == 0 && !is_edit_ops_using_mouse() && !is_manip_overlay_using_mouse() && !system_transform_gizmo_wants_mouse() && !is_terrain_tools_blocking_edit_picks()) {
        const bool additive = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
        em_select_element(mesh, ui_element->active_select_type, s_hover, additive);
    }
}

/* ------------------------------------------------------------------------- */
/* drawing (2D overlay through push_quad_element)                            */
/* ------------------------------------------------------------------------- */
static void px_rect(VkCommandBuffer cmd, const struct_viewport_bounds* b, float x, float y, float w, float h, const float c[4])
{
    float x0 = fmaxf(x, (float)b->offset_x),       y0 = fmaxf(y, (float)b->offset_y);
    float x1 = fminf(x + w, (float)(b->offset_x + b->width)), y1 = fminf(y + h, (float)(b->offset_y + b->height));
    if (x1 <= x0 || y1 <= y0) return;
    system_color color = { c[0], c[1], c[2], c[3] };
    push_quad_element(cmd, x0, y0, x1 - x0, y1 - y0, color);
}

/* Liang-Barsky clip to the viewport so zoomed-in edges never emit thousands of quads */
static bool clip_line(const struct_viewport_bounds* b, float* x0, float* y0, float* x1, float* y1)
{
    float xmin = (float)b->offset_x, xmax = (float)(b->offset_x + b->width);
    float ymin = (float)b->offset_y, ymax = (float)(b->offset_y + b->height);
    float dx = *x1 - *x0, dy = *y1 - *y0;
    float p[4] = { -dx, dx, -dy, dy };
    float q[4] = { *x0 - xmin, xmax - *x0, *y0 - ymin, ymax - *y0 };
    float t0 = 0, t1 = 1;
    for (int i = 0; i < 4; ++i) {
        if (fabsf(p[i]) < 1e-6f) { if (q[i] < 0) return false; }
        else {
            float t = q[i] / p[i];
            if (p[i] < 0) { if (t > t1) return false; if (t > t0) t0 = t; }
            else          { if (t < t0) return false; if (t < t1) t1 = t; }
        }
    }
    float ax = *x0, ay = *y0;
    *x0 = ax + t0 * dx; *y0 = ay + t0 * dy;
    *x1 = ax + t1 * dx; *y1 = ay + t1 * dy;
    return true;
}

static void draw_line(VkCommandBuffer cmd, const struct_viewport_bounds* b,
                      float x0, float y0, float x1, float y1, float thick, const float c[4])
{
    if (!clip_line(b, &x0, &y0, &x1, &y1)) return;
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    int steps = (int)(len / thick) + 1;
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        px_rect(cmd, b, x0 + dx * t - thick * 0.5f, y0 + dy * t - thick * 0.5f, thick, thick, c);
    }
}

/* convex polygon scanline fill, 3px bands */
static void fill_poly(VkCommandBuffer cmd, const struct_viewport_bounds* b, const float* xs, const float* ys, int n, const float c[4])
{
    const float band = 3.0f;
    float miny = ys[0], maxy = ys[0];
    for (int i = 1; i < n; ++i) { if (ys[i] < miny) miny = ys[i]; if (ys[i] > maxy) maxy = ys[i]; }
    miny = fmaxf(miny, (float)b->offset_y);
    maxy = fminf(maxy, (float)(b->offset_y + b->height));
    for (float y = miny; y < maxy; y += band) {
        float yc = y + band * 0.5f, xmin = 1e30f, xmax = -1e30f;
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            float y0 = ys[i], y1 = ys[j];
            if ((y0 <= yc && y1 > yc) || (y1 <= yc && y0 > yc)) {
                float x = xs[i] + (yc - y0) / (y1 - y0) * (xs[j] - xs[i]);
                if (x < xmin) xmin = x;
                if (x > xmax) xmax = x;
            }
        }
        if (xmax > xmin) px_rect(cmd, b, xmin, y, xmax - xmin, band, c);
    }
}

static void draw_face_fill(VkCommandBuffer cmd, const struct_viewport_bounds* b, const edit_mode_face* F, system_color color)
{
    const float c[4] = { color.red, color.green, color.blue, color.alpha };
    float xs[4], ys[4];
    for (int k = 0; k < F->face_type; ++k) {
        if (!s_proj[F->vert_to_edge_array[k]].ok) return;
        xs[k] = s_proj[F->vert_to_edge_array[k]].sx; ys[k] = s_proj[F->vert_to_edge_array[k]].sy;
    }
    fill_poly(cmd, b, xs, ys, F->face_type, c);
}

static void draw_edit_mode_selection_overlay(VkCommandBuffer cmd, system_element_context* ui_element)
{
    const edit_mode_mesh* mesh = s_mesh;
    const struct_viewport_bounds b = ui_element->scene_view;
    const enum_mesh_selection mode = ui_element->active_select_type;

    /* 1. faces */
    for (uint32_t f = 0; f < mesh->face_count; ++f) {
        if (!s_front[f]) continue;
        if (mesh->faces[f].obj_vert_selected) draw_face_fill(cmd, &b, &mesh->faces[f], system_theme.system_color_edit_mode_face_selection_color);
    }
    if (mode == select_face && s_hover != EM_NONE && !mesh->faces[s_hover].obj_vert_selected)
        draw_face_fill(cmd, &b, &mesh->faces[s_hover], system_theme.system_edit_mode_active_face_hover);

    /* the terrain edit grid (one chunk, 100 x 100 cells): the ground draws its own cell grid with the wire shader, so the plain edges are skipped here
       and EVERY control vertex / face centre is shown, not only the ones near the mouse. other big meshes: plain wire / points only near the cursor */
    const bool terrain_grid = (s_obj != NULL && is_terrain_object(s_obj));
    const bool big = mesh->vert_count > 2000 && !terrain_grid;
    const float mxp = (float)ui_element->input.mouse_x, myp = (float)ui_element->input.mouse_y;
    #define near_cursor(px, py) (fabsf((px) - mxp) < 260.0f && fabsf((py) - myp) < 260.0f)

    /* 2. edges: plain first, obj_vert_selected on top, hover last */
    for (int pass = 0; pass < 2; ++pass) {
        for (uint32_t i = 0; i < mesh->edge_count; ++i) {
            const edit_mode_edge* e = &mesh->edges[i];
            if ((e->obj_vert_selected != 0) != (pass == 1)) continue;
            if (terrain_grid && pass == 0) continue;
            if (!s_proj[e->vert_to_edge_array[0]].ok || !s_proj[e->vert_to_edge_array[1]].ok) continue;
            if (big && pass == 0 && !near_cursor(s_proj[e->vert_to_edge_array[0]].sx, s_proj[e->vert_to_edge_array[0]].sy)) continue;
            bool vis = false;
            for (uint32_t k = 0; k < e->face_count; ++k) vis |= (s_front[e->face[k]] != 0);
            if (!vis) continue;
            draw_line(cmd, &b, s_proj[e->vert_to_edge_array[0]].sx, s_proj[e->vert_to_edge_array[0]].sy, s_proj[e->vert_to_edge_array[1]].sx, s_proj[e->vert_to_edge_array[1]].sy,
                      pass ? 3.0f : 2.0f, pass ? C_SEL : C_WIRE);
        }
    }
    if (mode == select_edge && s_hover != EM_NONE) {
        const edit_mode_edge* e = &mesh->edges[s_hover];
        if (s_proj[e->vert_to_edge_array[0]].ok && s_proj[e->vert_to_edge_array[1]].ok)
            draw_line(cmd, &b, s_proj[e->vert_to_edge_array[0]].sx, s_proj[e->vert_to_edge_array[0]].sy, s_proj[e->vert_to_edge_array[1]].sx, s_proj[e->vert_to_edge_array[1]].sy, 3.0f, C_HOVER);
    }

    /* 3. points: verts in vertex mode, face centres in face mode */
    if (mode == select_vert) {
        for (uint32_t i = 0; i < mesh->vert_count; ++i) {
            if (!s_proj[i].ok || !s_vvis[i]) continue;
            if (big && i != s_hover && !mesh->verts[i].obj_vert_selected && !near_cursor(s_proj[i].sx, s_proj[i].sy)) continue;
            const float* c = (i == s_hover) ? C_HOVER : (mesh->verts[i].obj_vert_selected ? C_SEL : C_VERT);
            float sz = (i == s_hover || mesh->verts[i].obj_vert_selected) ? 8.0f : 6.0f;
            if (terrain_grid && i != s_hover && !mesh->verts[i].obj_vert_selected) sz = 4.0f;
            px_rect(cmd, &b, s_proj[i].sx - sz * 0.5f, s_proj[i].sy - sz * 0.5f, sz, sz, c);
        }
    }
    else if (mode == select_face) {
        for (uint32_t f = 0; f < mesh->face_count; ++f) {
            if (!s_front[f]) continue;
            const edit_mode_face* F = &mesh->faces[f];
            float cx = 0, cy = 0;
            for (int k = 0; k < F->face_type; ++k) { cx += s_proj[F->vert_to_edge_array[k]].sx; cy += s_proj[F->vert_to_edge_array[k]].sy; }
            cx /= F->face_type; cy /= F->face_type;
            if (big && !F->obj_vert_selected && !near_cursor(cx, cy)) continue;
            px_rect(cmd, &b, cx - 2.5f, cy - 2.5f, 5.0f, 5.0f, F->obj_vert_selected ? C_SEL : C_VERT);
        }
    }
    #undef near_cursor
}

/* Object mode: orange silhouette (edges where a front face meets a back face, plus open boundary edges) */
static void draw_outline(VkCommandBuffer cmd, system_element_context* ui_element)
{
    const edit_mode_mesh* mesh = s_mesh;
    const struct_viewport_bounds b = ui_element->scene_view;
    for (uint32_t i = 0; i < mesh->edge_count; ++i) {
        const edit_mode_edge* e = &mesh->edges[i];
        if (!s_proj[e->vert_to_edge_array[0]].ok || !s_proj[e->vert_to_edge_array[1]].ok) continue;
        bool show = false;
        if (e->face_count == 1)      show = s_front[e->face[0]] != 0;
        else if (e->face_count >= 2) show = (s_front[e->face[0]] != 0) != (s_front[e->face[1]] != 0);
        if (!show) continue;
        draw_line(cmd, &b, s_proj[e->vert_to_edge_array[0]].sx, s_proj[e->vert_to_edge_array[0]].sy, s_proj[e->vert_to_edge_array[1]].sx, s_proj[e->vert_to_edge_array[1]].sy, 3.0f, C_SEL);
    }
}

/* Objects with no edit mesh (cameras etc.): four orange corner brackets around the object */
static void bracket_corner(VkCommandBuffer cmd, const struct_viewport_bounds* b, float x, float y, float dx, float dy, float len)
{
    draw_line(cmd, b, x, y, x + dx * len, y, 3.0f, C_SEL);
    draw_line(cmd, b, x, y, x, y + dy * len, 3.0f, C_SEL);
}

static void draw_bracket(VkCommandBuffer cmd, system_element_context* ui_element)
{
    const struct_viewport_bounds b = ui_element->scene_view;
    if (b.width <= 0 || b.height <= 0 || !s_obj) return;

    editor_mat4 view, proj;
    build_scene_view_camera((float)b.width / (float)b.height, &view, &proj);
    editor_mat4 vp = system_math_mat4_multiply(proj, view);

    float ms = fmaxf(fabsf(s_obj->scale.x), fmaxf(fabsf(s_obj->scale.y), fabsf(s_obj->scale.z)));
    float radius = s_obj->bounds_radius * ms;
    if (radius < 0.25f) radius = 0.25f;

    editor_vec3 right = { view.mesh[0][0], view.mesh[1][0], view.mesh[2][0] };        /* camera right in world space */
    editor_vec3 edge_to_face_array  = { s_obj->position.x + right.x * radius,
                          s_obj->position.y + right.y * radius,
                          s_obj->position.z + right.z * radius };

    em_proj c, e;
    project_point(&vp, s_obj->position, &b, &c);
    project_point(&vp, edge_to_face_array, &b, &e);
    if (!c.ok || !e.ok) return;

    float r = fmaxf(fabsf(e.sx - c.sx), 14.0f);
    float len = fmaxf(8.0f, r * 0.4f);
    bracket_corner(cmd, &b, c.sx - r, c.sy - r,  1,  1, len);
    bracket_corner(cmd, &b, c.sx + r, c.sy - r, -1,  1, len);
    bracket_corner(cmd, &b, c.sx - r, c.sy + r,  1, -1, len);
    bracket_corner(cmd, &b, c.sx + r, c.sy + r, -1, -1, len);
}

void system_edit_mode_draw(VkCommandBuffer cmd, system_element_context* ui_element)
{
    switch (s_state) {
    case ST_EDIT:    if (s_mesh) draw_edit_mode_selection_overlay(cmd, ui_element); break;
    case ST_OUTLINE: if (s_mesh) draw_outline(cmd, ui_element); break;
    case ST_BRACKET: draw_bracket(cmd, ui_element);             break;
    default: break;
    }
}

void system_edit_mode_update_and_draw(VkCommandBuffer cmd, system_element_context* ui_element)
{
    system_edit_mode_update(ui_element);
    system_edit_mode_draw(cmd, ui_element);
}