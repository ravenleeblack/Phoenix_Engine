#include "system_globals.h"


#define GIZMO_PI        3.14159265f
#define SCENE_ROOT_ID   1000
#define PICK_RADIUS_PX  10.0f
#define TIP_GRAB_PX     26.0f      /* the arrow head / cube at the end of an axis can be grabbed from this far away */
#define RING_SEGMENTS   48
#define GIZMO_SCREEN_K  0.18f      /* gizmo world length = view depth * this (constant on-screen size) */

enum { AX_NONE = -1, AX_X = 0, AX_Y = 1, AX_Z = 2, AX_UNIFORM = 3 };

static const editor_vec3 AXES[3] = { {1,0,0}, {0,1,0}, {0,0,1} };


static struct {
    bool  dragging;
    bool  edit;                 /* true: dragging mesh components, false: dragging the whole object */
    bool  tt;                   /* true: dragging the vertices of a Terrain Tools piece (system_terrain_tools.c) */
    int   axis;
    int   hover;
    enum_dcc_tool tool;
    uint32_t id;

    /* translate / scale: screen-space axis captured at drag start */
    float sx, sy, dx, dy, len2, t_grab, L;
    float mx0, my0;
    /* rotate */
    float last_ang, accum, sign;

    editor_vec3 p0, r0, s0;     /* p0 = pivot at drag start (object position in Object mode, selection centroid in Edit mode) */
} g = { .axis = AX_NONE, .hover = AX_NONE };

/* Edit-mode drag snapshot */
static edit_mode_mesh*     s_mesh;
static editor_vec3* s_snap;      static uint32_t s_snap_cap;   /* vertex positions at drag start (object space) */
static editor_mat4  s_model, s_inv;

/* Defined in system_grid.c: the orbit camera the Scene view really renders with. */
extern void build_scene_orbit_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj);

/* ---------------------------------------------------------------- math */

static void mul_vec4(const editor_mat4* mesh, float x, float y, float z, float w, float out[4])
{
    for (int r = 0; r < 4; r++)
        out[r] = mesh->mesh[0][r] * x + mesh->mesh[1][r] * y + mesh->mesh[2][r] * z + mesh->mesh[3][r] * w;
}

static bool project(const gizmo_cam* c, editor_vec3 p, float* sx, float* sy)
{
    float o[4];
    mul_vec4(&c->vp, p.x, p.y, p.z, 1.0f, o);
    if (o[3] <= 0.01f) return false;
    /* proj is the Vulkan one (Y already flipped), so ndc +Y is screen down */
    *sx = (float)c->b.offset_x + (o[0] / o[3] * 0.5f + 0.5f) * (float)c->b.width;
    *sy = (float)c->b.offset_y + (o[1] / o[3] * 0.5f + 0.5f) * (float)c->b.height;
    return true;
}

static float view_depth(const gizmo_cam* c, editor_vec3 p)
{
    float o[4];
    mul_vec4(&c->view, p.x, p.y, p.z, 1.0f, o);
    return -o[2];
}

/* z of a direction in view space. > 0 means it points toward the viewer. */
static float view_dir_z(const gizmo_cam* c, editor_vec3 d)
{
    return c->view.mesh[0][2] * d.x + c->view.mesh[1][2] * d.y + c->view.mesh[2][2] * d.z;
}

static float* comp(editor_vec3* vert_to_edge_array, int i) { return i == 0 ? &vert_to_edge_array->x : (i == 1 ? &vert_to_edge_array->y : &vert_to_edge_array->z); }

static float snap_to(float vert_to_edge_array, float step) { return step > 0.0f ? roundf(vert_to_edge_array / step) * step : vert_to_edge_array; }

static float seg_dist(float px, float py, float ax, float ay, float bx, float by, float* out_t)
{
    float vx = bx - ax, vy = by - ay;
    float l2 = vx * vx + vy * vy;
    float t = (l2 > 1e-6f) ? ((px - ax) * vx + (py - ay) * vy) / l2 : 0.0f;
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    if (out_t) *out_t = t;
    float cx = ax + vx * t - px, cy = ay + vy * t - py;
    return sqrtf(cx * cx + cy * cy);
}

static editor_vec3 ring_point(int axis, float a, float L, editor_vec3 o)
{
    float c = cosf(a) * L, s = sinf(a) * L;
    editor_vec3 p = o;
    if (axis == AX_X)      { p.y += c; p.z += s; }
    else if (axis == AX_Y) { p.x += c; p.z += s; }
    else                   { p.x += c; p.y += s; }
    return p;
}

/* ---- affine helpers (use the real model matrix, so they never disagree with the renderer) ---- */

static editor_vec3 xform_point(const editor_mat4* M, editor_vec3 p)
{
    editor_vec3 o;
    o.x = M->mesh[0][0] * p.x + M->mesh[1][0] * p.y + M->mesh[2][0] * p.z + M->mesh[3][0];
    o.y = M->mesh[0][1] * p.x + M->mesh[1][1] * p.y + M->mesh[2][1] * p.z + M->mesh[3][1];
    o.z = M->mesh[0][2] * p.x + M->mesh[1][2] * p.y + M->mesh[2][2] * p.z + M->mesh[3][2];
    return o;
}

static bool affine_inverse(const editor_mat4* M, editor_mat4* out)
{
    float a[3][3], inv[3][3];
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) a[r][c] = M->mesh[c][r];

    float c00 = a[1][1] * a[2][2] - a[1][2] * a[2][1];
    float c01 = a[1][2] * a[2][0] - a[1][0] * a[2][2];
    float c02 = a[1][0] * a[2][1] - a[1][1] * a[2][0];
    float det = a[0][0] * c00 + a[0][1] * c01 + a[0][2] * c02;
    if (fabsf(det) < 1e-12f) return false;
    float id = 1.0f / det;

    inv[0][0] = c00 * id;
    inv[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) * id;
    inv[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) * id;
    inv[1][0] = c01 * id;
    inv[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) * id;
    inv[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) * id;
    inv[2][0] = c02 * id;
    inv[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) * id;
    inv[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) * id;

    float t0 = M->mesh[3][0], t1 = M->mesh[3][1], t2 = M->mesh[3][2];
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) out->mesh[c][r] = inv[r][c];
        out->mesh[3][r] = -(inv[r][0] * t0 + inv[r][1] * t1 + inv[r][2] * t2);
    }
    out->mesh[0][3] = out->mesh[1][3] = out->mesh[2][3] = 0.0f;
    out->mesh[3][3] = 1.0f;
    return true;
}

static editor_vec3 rotate_about_axis(editor_vec3 vert_to_edge_array, editor_vec3 a, float ang)
{
    float c = cosf(ang), s = sinf(ang);
    float d = a.x * vert_to_edge_array.x + a.y * vert_to_edge_array.y + a.z * vert_to_edge_array.z;
    editor_vec3 cr = { a.y * vert_to_edge_array.z - a.z * vert_to_edge_array.y, a.z * vert_to_edge_array.x - a.x * vert_to_edge_array.z, a.x * vert_to_edge_array.y - a.y * vert_to_edge_array.x };
    editor_vec3 r = {
        vert_to_edge_array.x * c + cr.x * s + a.x * d * (1.0f - c),
        vert_to_edge_array.y * c + cr.y * s + a.y * d * (1.0f - c),
        vert_to_edge_array.z * c + cr.z * s + a.z * d * (1.0f - c)
    };
    return r;
}

/* ---------------------------------------------------------------- drawing */

static void dot(VkCommandBuffer cmd, const gizmo_cam* c, float x, float y, float size, system_color col)
{
    if (x < (float)c->b.offset_x || y < (float)c->b.offset_y ||
        x > (float)(c->b.offset_x + c->b.width) || y > (float)(c->b.offset_y + c->b.height)) return;
    col.alpha = 1.0f;
    push_quad_element(cmd, x - size * 0.5f, y - size * 0.5f, size, size, col);
}

static void line(VkCommandBuffer cmd, const gizmo_cam* c, float x0, float y0, float x1, float y1, system_color col)
{
    float dx = x1 - x0, dy = y1 - y0;
    int steps = (int)(sqrtf(dx * dx + dy * dy) / 2.0f) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        dot(cmd, c, x0 + dx * t, y0 + dy * t, 3.0f, col);
    }
}

/* the colors every game engine uses: X red, Y green, Z blue, the hovered / dragged handle turns yellow */
static system_color axis_color(int axis, bool highlighted)
{
    if (highlighted)        return (system_color){ 0.96f, 0.93f, 0.22f, 1.0f };
    if (axis == AX_X)       return (system_color){ 0.86f, 0.25f, 0.20f, 1.0f };
    if (axis == AX_Y)       return (system_color){ 0.56f, 0.88f, 0.28f, 1.0f };
    if (axis == AX_Z)       return (system_color){ 0.24f, 0.50f, 0.96f, 1.0f };
    return (system_color){ 0.88f, 0.88f, 0.88f, 1.0f };
}

/* a solid line, 4 px wide, with a dark edge so it reads on any ground */
static void thick_line(VkCommandBuffer cmd, const gizmo_cam* c, float x0, float y0, float x1, float y1, system_color col)
{
    const system_color edge = { 0.0f, 0.0f, 0.0f, 1.0f };
    float dx = x1 - x0, dy = y1 - y0;
    int steps = (int)(sqrtf(dx * dx + dy * dy) / 1.5f) + 1;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i <= steps; i++) {
            const float t = (float)i / (float)steps;
            if (pass == 0) {
                dot(cmd, c, x0 + dx * t, y0 + dy * t, 6.0f, edge);
            } else {
                dot(cmd, c, x0 + dx * t, y0 + dy * t, 4.0f, col);
            }
        }
    }
}

/* the arrow head of a move handle: a solid cone from base to tip, drawn as slices across the axis */
static void arrow_head(VkCommandBuffer cmd, const gizmo_cam* c, float bx, float by, float tx, float ty, float half_width, system_color col)
{
    float dx = tx - bx, dy = ty - by;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) return;
    const float px = -dy / len, py = dx / len;
    const int slices = (int)(len / 1.5f) + 2;
    for (int i = 0; i <= slices; i++) {
        const float t = (float)i / (float)slices;
        const float w = half_width * (1.0f - t);
        const float cx = bx + dx * t, cy = by + dy * t;
        const int across = (int)(w / 1.5f) + 1;
        for (int k = -across; k <= across; k++) {
            const float o = (float)k / (float)across * w;
            dot(cmd, c, cx + px * o, cy + py * o, 3.0f, col);
        }
    }
}

/* the end of a scale handle: a solid cube with a dark edge */
static void cube_handle(VkCommandBuffer cmd, const gizmo_cam* c, float x, float y, float size, system_color col)
{
    dot(cmd, c, x, y, size + 3.0f, (system_color){ 0.0f, 0.0f, 0.0f, 1.0f });
    dot(cmd, c, x, y, size, col);
}

/* ---------------------------------------------------------------- terrain sync (Object mode only) */

/* Terrain position is authoritative in int32 1/16 mesh quanta, so quantize and push it through the terrain API. */
static void commit_object(system_scene_object* o)
{
    if (g_terrain.exists && o->id == g_terrain.scene_object_id) {
        terrain_location_q q;
        q.x = (int32_t)lroundf(o->position.x * (float)terrain_units_per_meter);
        q.y = (int32_t)lroundf(o->position.y * (float)terrain_units_per_meter);
        q.z = (int32_t)lroundf(o->position.z * (float)terrain_units_per_meter);
        g_terrain.settings.location = q;
        g_terrain_ui.location       = q;
        terrain_apply_location();
        o->position.x = (float)q.x / (float)terrain_units_per_meter;
        o->position.y = (float)q.y / (float)terrain_units_per_meter;
        o->position.z = (float)q.z / (float)terrain_units_per_meter;
    }
}

/* ---------------------------------------------------------------- target resolution */

/* Object mode: pivot = object position.
   Edit mode:   pivot = world-space centroid of the obj_vert_selected vertices (no selection => no gizmo). */
static bool resolve_target(system_element_context* ui_element, system_scene_object** out_o, edit_mode_mesh** out_em, editor_vec3* out_pivot)
{
    if (g_terrain_tools.tool != tt_tool_none) {
        /* Terrain Tools: step 1 picks edges (no gizmo), after that the gizmo moves the vertices of the piece */
        editor_vec3 world_pivot;
        system_scene_object* terrain_object = find_scene_object_by_id(g_terrain.scene_object_id);
        if (!terrain_object || !get_terrain_tools_gizmo_pivot(&world_pivot)) return false;
        *out_o = terrain_object; *out_em = NULL; *out_pivot = world_pivot;
        return true;
    }

    system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (!o || o->id == SCENE_ROOT_ID) return false;

    if (ui_element->active_mode == mode_object) {
        *out_o = o; *out_em = NULL; *out_pivot = o->position;
        return true;
    }
    if (ui_element->active_mode == mode_edit_mesh) {
        if (is_terrain_object(o) && g_terrain_tool.tool != terrain_tool_off) return false;   /* a terrain brush tool is on: no control grid, nothing to grab */
        edit_mode_mesh* mesh = em_registry_get_or_create(o);
        if (!mesh && is_terrain_object(o)) mesh = terrain_edit_mesh_current();   /* Terrain / region_xx: the control grid Edit Mode is showing */
        if (!mesh) return false;
        editor_vec3 lc;
        if (em_selection_centroid(mesh, &lc) == 0) return false;
        editor_mat4 model = system_math_model_matrix(o->position, o->rotation, o->scale);
        *out_o = o; *out_em = mesh; *out_pivot = xform_point(&model, lc);
        return true;
    }
    return false;
}

static bool snapshot_edit(edit_mode_mesh* mesh, system_scene_object* o)
{
    s_model = system_math_model_matrix(o->position, o->rotation, o->scale);
    if (!affine_inverse(&s_model, &s_inv)) return false;
    if (mesh->vert_count > s_snap_cap) {
        uint32_t nc = s_snap_cap ? s_snap_cap : 64;
        while (nc < mesh->vert_count) nc *= 2;
        editor_vec3* p = (editor_vec3*)realloc(s_snap, sizeof(editor_vec3) * nc);
        if (!p) return false;
        s_snap = p; s_snap_cap = nc;
    }
    for (uint32_t i = 0; i < mesh->vert_count; i++) s_snap[i] = mesh->verts[i].obj_vert_pos;
    s_mesh = mesh;
    return true;
}

/* ---------------------------------------------------------------- hit test */

static int hit_test(const gizmo_cam* c, enum_dcc_tool tool, editor_vec3 origin, float L, float mx, float my)
{
    float ox, oy;
    if (!project(c, origin, &ox, &oy)) return AX_NONE;

    if (tool == tool_scale) {
        float dx = mx - ox, dy = my - oy;
        if (sqrtf(dx * dx + dy * dy) < 9.0f) return AX_UNIFORM;
    }

    int best = AX_NONE;
    float best_d = PICK_RADIUS_PX;

    for (int i = 0; i < 3; i++) {
        if (tool == tool_rotate) {
            float px = 0, py = 0, qx, qy;
            bool have_prev = false;
            for (int s = 0; s <= RING_SEGMENTS; s++) {
                float a = (float)s / (float)RING_SEGMENTS * 2.0f * GIZMO_PI;
                editor_vec3 p = ring_point(i, a, L, origin);
                editor_vec3 off = { p.x - origin.x, p.y - origin.y, p.z - origin.z };
                bool facing = view_dir_z(c, off) >= 0.0f;
                bool ok = facing && project(c, p, &qx, &qy);
                if (ok && have_prev) {
                    float d = seg_dist(mx, my, px, py, qx, qy, NULL);
                    if (d < best_d) { best_d = d; best = i; }
                }
                have_prev = ok; px = qx; py = qy;
            }
        } else {
            editor_vec3 tip = { origin.x + AXES[i].x * L, origin.y + AXES[i].y * L, origin.z + AXES[i].z * L };
            float tx, ty;
            if (!project(c, tip, &tx, &ty)) continue;
            float d = seg_dist(mx, my, ox, oy, tx, ty, NULL);
            const float tdx = mx - tx, tdy = my - ty;
            const float tip_d = sqrtf(tdx * tdx + tdy * tdy);
            if (tip_d < TIP_GRAB_PX) {
                d = fminf(d, tip_d * 0.4f);            /* the head at the end is the easy part to grab */
            }
            if (d < best_d) { best_d = d; best = i; }
        }
    }
    return best;
}

/* ---------------------------------------------------------------- drag */

static void begin_drag(const gizmo_cam* c, system_scene_object* o, edit_mode_mesh* em, editor_vec3 pivot,
                       enum_dcc_tool tool, int axis, float L, float mx, float my)
{
    if (em && !snapshot_edit(em, o)) return;

    g.dragging = true; g.edit = (em != NULL);
    g.tt = (g_terrain_tools.tool != tt_tool_none);
    if (g.tt) { begin_terrain_tools_gizmo_drag(); }
    g.axis = axis; g.tool = tool; g.id = o->id; g.L = L;
    g.p0 = pivot; g.r0 = o->rotation; g.s0 = o->scale;
    g.mx0 = mx; g.my0 = my;

    float ox, oy;
    project(c, pivot, &ox, &oy);

    if (tool == tool_rotate) {
        g.sx = ox; g.sy = oy;            /* ring centre, fixed for the whole drag */
        g.last_ang = atan2f(my - oy, mx - ox);
        g.accum = 0.0f;
        /* CCW about an axis pointing at the viewer is CCW on screen, which is a NEGATIVE angle with Y down. */
        g.sign = (view_dir_z(c, AXES[axis]) > 0.0f) ? -1.0f : 1.0f;
    } else if (axis != AX_UNIFORM) {
        editor_vec3 tip = { pivot.x + AXES[axis].x * L, pivot.y + AXES[axis].y * L, pivot.z + AXES[axis].z * L };
        float tx = ox, ty = oy;
        project(c, tip, &tx, &ty);
        g.sx = ox; g.sy = oy; g.dx = tx - ox; g.dy = ty - oy;
        g.len2 = g.dx * g.dx + g.dy * g.dy;
        g.t_grab = (g.len2 > 1e-4f) ? ((mx - g.sx) * g.dx + (my - g.sy) * g.dy) / g.len2 : 0.0f;
    }
}

/* Edit mode: apply the drag to the obj_vert_selected verts. `value` is distance (mesh), angle (rad) or scale factor.
   Works in world space around the drag-start pivot, then converts back to object space. */
static void edit_apply(system_scene_object* o, float value)
{
    edit_mode_mesh* mesh = s_mesh;
    if (!mesh) return;
    const editor_vec3 pv = g.p0;
    const editor_vec3 ax = (g.axis >= 0 && g.axis <= 2) ? AXES[g.axis] : AXES[0];

    for (uint32_t i = 0; i < mesh->vert_count; i++) {
        if (!mesh->verts[i].obj_vert_selected) continue;

        editor_vec3 w = xform_point(&s_model, s_snap[i]);
        editor_vec3 r = { w.x - pv.x, w.y - pv.y, w.z - pv.z };

        if (g.tool == tool_translate) {
            w.x += ax.x * value; w.y += ax.y * value; w.z += ax.z * value;
        }
        else if (g.tool == tool_rotate) {
            r = rotate_about_axis(r, ax, value);
            w.x = pv.x + r.x; w.y = pv.y + r.y; w.z = pv.z + r.z;
        }
        else if (g.tool == tool_scale) {
            if (g.axis == AX_UNIFORM) {
                w.x = pv.x + r.x * value; w.y = pv.y + r.y * value; w.z = pv.z + r.z * value;
            } else {
                float k = (r.x * ax.x + r.y * ax.y + r.z * ax.z) * (value - 1.0f);
                w.x += ax.x * k; w.y += ax.y * k; w.z += ax.z * k;
            }
        }
        mesh->verts[i].obj_vert_pos = xform_point(&s_inv, w);
    }
    em_commit_to_gpu(mesh, o);
}

static void update_drag(system_scene_object* o, float mx, float my)
{
    const object_mode_state* os = &g_mode_state.object;

    if (g.tool == tool_rotate) {
        float ox = g.sx, oy = g.sy;
        float ang = atan2f(my - oy, mx - ox);
        float d = ang - g.last_ang;
        while (d >  GIZMO_PI) d -= 2.0f * GIZMO_PI;
        while (d < -GIZMO_PI) d += 2.0f * GIZMO_PI;
        g.last_ang = ang;
        g.accum += d * g.sign;

        float total = g.accum;
        if (os->snap_enabled) total = snap_to(total, os->snap_rotate_deg * GIZMO_PI / 180.0f);
        if (g.edit) { edit_apply(o, total); return; }
        o->rotation = g.r0;
        *comp(&o->rotation, g.axis) = *comp(&g.r0, g.axis) + total;
    }
    else if (g.tool == tool_translate) {
        if (g.len2 < 1e-4f) return;     /* axis points straight at the camera */
        float t = ((mx - g.sx) * g.dx + (my - g.sy) * g.dy) / g.len2 - g.t_grab;
        float dist = t * g.L;
        if (os->snap_enabled) dist = snap_to(dist, os->snap_translate_m);
        if (g.tt) { move_terrain_tools_gizmo_nodes(g.axis, dist); return; }
        if (g.edit) { edit_apply(o, dist); return; }
        o->position = g.p0;
        *comp(&o->position, g.axis) = *comp(&g.p0, g.axis) + dist;
    }
    else if (g.tool == tool_scale) {
        float f;
        if (g.axis == AX_UNIFORM) {
            f = 1.0f + ((mx - g.mx0) - (my - g.my0)) / 150.0f;
            if (f < 0.01f) f = 0.01f;
            if (os->snap_enabled) f = snap_to(f, os->snap_scale);
            if (f < 0.01f) f = 0.01f;
            if (g.edit) { edit_apply(o, f); return; }
            o->scale.x = g.s0.x * f; o->scale.y = g.s0.y * f; o->scale.z = g.s0.z * f;
        } else {
            if (g.len2 < 1e-4f) return;
            float t = ((mx - g.sx) * g.dx + (my - g.sy) * g.dy) / g.len2 - g.t_grab;
            if (g.edit) {
                f = 1.0f + t;
                if (os->snap_enabled) f = snap_to(f, os->snap_scale);
                if (f < 0.001f) f = 0.001f;
                edit_apply(o, f);
                return;
            }
            float base = *comp(&g.s0, g.axis);
            float vert_to_edge_array = base * (1.0f + t);
            if (os->snap_enabled) vert_to_edge_array = snap_to(vert_to_edge_array, os->snap_scale);
            if (vert_to_edge_array < 0.001f) vert_to_edge_array = 0.001f;
            o->scale = g.s0;
            *comp(&o->scale, g.axis) = vert_to_edge_array;
        }
    }
    commit_object(o);
}

/* ---------------------------------------------------------------- public */

bool system_transform_gizmo_wants_mouse(void)
{
    return g.dragging || g.hover != AX_NONE;
}

static void cancel_drag(void) { g.dragging = false; g.axis = AX_NONE; }

static void run_gizmo(VkCommandBuffer cmd, system_element_context* ui_element, bool do_update, bool do_draw)
{
    static bool prev_down = false;
    bool pressed = false;

    if (do_update) {
        mode_state_sync_from_ui(ui_element);
        const bool down = ui_element->input.mouse_down;
        pressed   = down && !prev_down;
        prev_down = down;
        if (g.dragging && !down) cancel_drag();
        g.hover = AX_NONE;
    }

    const enum_dcc_tool tool = ui_element->active_tool;
    gizmo_cam c;
    c.b = ui_element->scene_view;
    if (c.b.width <= 0 || c.b.height <= 0) { g.dragging = false; return; }

    system_scene_object* o = NULL;
    edit_mode_mesh* em = NULL;
    editor_vec3 pivot = { 0, 0, 0 };

    bool usable = (tool != tool_select) && !(g_terrain_tools.tool != tt_tool_none && tool != tool_translate) && resolve_target(ui_element, &o, &em, &pivot);
    if (!usable || (g.dragging && (o->id != g.id || g.edit != (em != NULL)))) { cancel_drag(); return; }

    /* Let the fly camera / orbit have the mouse */
    if (ui_element->input.alt_pressed || ui_element->input.middle_down || ui_element->input.camera_manipulating) {
        cancel_drag(); return;
    }

    float aspect = (float)c.b.width / (float)c.b.height;
    editor_mat4 view, proj;
    build_scene_orbit_camera(aspect, &view, &proj);
    c.view = view;
    c.vp   = system_math_mat4_multiply(proj, view);

    float depth = view_depth(&c, pivot);
    if (depth < 0.5f) depth = 0.5f;
    float L = depth * GIZMO_SCREEN_K * 1.0f;

    const float mx = (float)ui_element->input.mouse_x, my = (float)ui_element->input.mouse_y;
    const bool in_view = mx >= c.b.offset_x && my >= c.b.offset_y &&
                         mx < c.b.offset_x + c.b.width && my < c.b.offset_y + c.b.height;

    if (do_update) {
        if (g.dragging) {
            update_drag(o, mx, my);
        } else if (in_view) {
            g.hover = hit_test(&c, tool, pivot, L, mx, my);
            if (pressed && g.hover != AX_NONE) begin_drag(&c, o, em, pivot, tool, g.hover, L, mx, my);
        }
        if (g.dragging) g.hover = g.axis;
    }
    if (!do_draw) return;

    /* ---- draw (re-resolve so the handle sits on the possibly just-moved selection) ---- */
    if (!resolve_target(ui_element, &o, &em, &pivot)) return;
    if (g_terrain_tools.tool != tt_tool_none && tool != tool_translate) return;

    float ox, oy;
    if (!project(&c, pivot, &ox, &oy)) return;

    for (int i = 0; i < 3; i++) {
        system_color col = axis_color(i, g.hover == i);
        if (tool == tool_rotate) {
            float px = 0, py = 0, qx, qy; bool have_prev = false;
            for (int s = 0; s <= RING_SEGMENTS; s++) {
                float a = (float)s / (float)RING_SEGMENTS * 2.0f * GIZMO_PI;
                editor_vec3 p = ring_point(i, a, L, pivot);
                editor_vec3 off = { p.x - pivot.x, p.y - pivot.y, p.z - pivot.z };
                bool ok = view_dir_z(&c, off) >= 0.0f && project(&c, p, &qx, &qy);
                if (ok && have_prev) line(cmd, &c, px, py, qx, qy, col);
                have_prev = ok; px = qx; py = qy;
            }
        } else {
            editor_vec3 tip = { pivot.x + AXES[i].x * L, pivot.y + AXES[i].y * L, pivot.z + AXES[i].z * L };
            float tx, ty;
            if (!project(&c, tip, &tx, &ty)) continue;
            const float adx = tx - ox, ady = ty - oy;
            const float alen = sqrtf(adx * adx + ady * ady);
            if (alen < 2.0f) continue;
            if (tool == tool_scale) {
                thick_line(cmd, &c, ox, oy, tx, ty, col);
                cube_handle(cmd, &c, tx, ty, 14.0f, col);
            } else {
                float head = 28.0f;
                if (head > alen * 0.4f) head = alen * 0.4f;
                const float ux = adx / alen, uy = ady / alen;
                const float sx = tx - ux * head, sy = ty - uy * head;
                thick_line(cmd, &c, ox, oy, sx, sy, col);
                arrow_head(cmd, &c, sx, sy, tx, ty, head * 0.30f, col);
            }
        }
    }
    if (tool == tool_scale) {
        cube_handle(cmd, &c, ox, oy, 14.0f, axis_color(AX_UNIFORM, g.hover == AX_UNIFORM));
    } else if (tool == tool_translate) {
        dot(cmd, &c, ox, oy, 6.0f, (system_color){ 0.85f, 0.85f, 0.85f, 1.0f });
    }
}

/* Input + transform logic only. Safe to call from anywhere that runs every frame. */
void system_transform_gizmo_update(system_element_context* ui_element)
{
    run_gizmo(VK_NULL_HANDLE, ui_element, true, false);
}

/* Drawing only. Must run after the Scene view image is drawn or it will be covered. */
void system_transform_gizmo_draw(VkCommandBuffer cmd, system_element_context* ui_element)
{
    run_gizmo(cmd, ui_element, false, true);
}

void system_transform_gizmo_update_and_draw(VkCommandBuffer cmd, system_element_context* ui_element)
{
    run_gizmo(cmd, ui_element, true, true);
}

/* World -> Scene view pixel projection. Lives here with the object gizmo, not with the UI overlay. */
bool project_world_to_viewport(editor_vec3 world_pos, editor_mat4 view, editor_mat4 proj, struct_viewport_bounds view_bounds, float* out_x, float* out_y)
{
    // Clip space coordinate translation matrix multiplication
    float x = world_pos.x * view.mesh[0][0] + world_pos.y * view.mesh[1][0] + world_pos.z * view.mesh[2][0] + view.mesh[3][0];
    float y = world_pos.x * view.mesh[0][1] + world_pos.y * view.mesh[1][1] + world_pos.z * view.mesh[2][1] + view.mesh[3][1];
    float z = world_pos.x * view.mesh[0][2] + world_pos.y * view.mesh[1][2] + world_pos.z * view.mesh[2][2] + view.mesh[3][2];
    float w = world_pos.x * view.mesh[0][3] + world_pos.y * view.mesh[1][3] + world_pos.z * view.mesh[2][3] + view.mesh[3][3];

    if (w <= 0.001f) return false; // Behind camera clip prevention drop

    // Normalized Device Coordinates (NDC) mapping
    float ndc_x = (x * proj.mesh[0][0] + y * proj.mesh[1][0] + z * proj.mesh[2][0] + proj.mesh[3][0]) / w;
    float ndc_y = (x * proj.mesh[0][1] + y * proj.mesh[1][1] + z * proj.mesh[2][1] + proj.mesh[3][1]) / w;

    // Convert NDC (-1 to 1) into actual local pixel offsets inside the workspace panel bounds
    *out_x = (float)view_bounds.offset_x + ((ndc_x + 1.0f) * 0.5f) * (float)view_bounds.width;
    *out_y = (float)view_bounds.offset_y + ((1.0f - ndc_y) * 0.5f) * (float)view_bounds.height; 

    return true;
}