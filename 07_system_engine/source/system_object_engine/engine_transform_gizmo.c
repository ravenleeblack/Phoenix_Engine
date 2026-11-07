#include "engine_globals.h"


/*
RULES THAT MUST BE FOLLOWED.
uncomment as we start getting things fixed: 
Must use lowercase seperated by underscores. Even structs names, enum names, etc.
Must use verbs before nouns when naming anything
Must not use ternerys. i fucking hate ternery operators.
Must call extern in engine_globals.h and init in engine_globals.c
Must not have statics in any file. I fucking hate statics.
No static structs, No static enums, No static anything.
Math functions must go in math file and header.
Drawing stuff must go into drawing stuff and headers.
ALl information gets routed into statusbar.

NO CAPITAL LETTERS UNLESS WINDOWS OR VULKAN DEMANDS IT.
DO NOT REMOVE THIS COMMENT EVER. THESE ARE THE RULES.
IF SOMETHING IS BREAKING THESE RULES CHANGE IT THEN WE CHANGE EVERYTHING CONNECTED AS WE GO.
THIS INCLUDES INSIDE THE SOURCE FILES NOT JUST HEADERS
*/

/* types, constants and the gizmo state live in engine_transform_gizmo.h and engine_globals.h/.c (g_gizmo ...). */

/* ---------------------------------------------------------------- math
   TODO: these math helpers belong in engine_math.c / engine_math.h. kept here only until they are moved. */

bool project_gizmo_point(const gizmo_cam* c, editor_vec3 p, float* sx, float* sy)
{
    float o[4];
    multiply_gizmo_vec4(&c->vp, p.x, p.y, p.z, 1.0f, o);
    if (o[3] <= 0.01f) return false;
    /* proj is the Vulkan one (Y already flipped), so ndc +Y is screen down */
    *sx = (float)c->b.offset_x + (o[0] / o[3] * 0.5f + 0.5f) * (float)c->b.width;
    *sy = (float)c->b.offset_y + (o[1] / o[3] * 0.5f + 0.5f) * (float)c->b.height;
    return true;
}

float get_gizmo_view_depth(const gizmo_cam* c, editor_vec3 p)
{
    float o[4];
    multiply_gizmo_vec4(&c->view, p.x, p.y, p.z, 1.0f, o);
    return -o[2];
}

/* z of a direction in view space. > 0 means it points toward the viewer. */
float get_gizmo_view_dir_z(const gizmo_cam* c, editor_vec3 d)
{
    return c->view.mesh[0][2] * d.x + c->view.mesh[1][2] * d.y + c->view.mesh[2][2] * d.z;
}

float* get_vec3_component(editor_vec3* v, int i)
{
    if (i == 0) return &v->x;
    if (i == 1) return &v->y;
    return &v->z;
}

float snap_gizmo_value(float value, float step)
{
    if (step > 0.0f) return roundf(value / step) * step;
    return value;
}

float get_segment_distance(float px, float py, float ax, float ay, float bx, float by, float* out_t)
{
    float vx = bx - ax, vy = by - ay;
    float l2 = vx * vx + vy * vy;
    float t = 0.0f;
    if (l2 > 1e-6f) t = ((px - ax) * vx + (py - ay) * vy) / l2;
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;
    if (out_t) *out_t = t;
    float cx = ax + vx * t - px, cy = ay + vy * t - py;
    return sqrtf(cx * cx + cy * cy);
}

editor_vec3 get_gizmo_ring_point(int axis, float a, float L, editor_vec3 o)
{
    float c = cosf(a) * L, s = sinf(a) * L;
    editor_vec3 p = o;
    if (axis == gizmo_axis_x)      { p.y += c; p.z += s; }
    else if (axis == gizmo_axis_y) { p.x += c; p.z += s; }
    else                           { p.x += c; p.y += s; }
    return p;
}

/* ---- affine helpers (use the real model matrix, so they never disagree with the renderer) ---- */

editor_vec3 transform_gizmo_point(const editor_mat4* m, editor_vec3 p)
{
    editor_vec3 o;
    o.x = m->mesh[0][0] * p.x + m->mesh[1][0] * p.y + m->mesh[2][0] * p.z + m->mesh[3][0];
    o.y = m->mesh[0][1] * p.x + m->mesh[1][1] * p.y + m->mesh[2][1] * p.z + m->mesh[3][1];
    o.z = m->mesh[0][2] * p.x + m->mesh[1][2] * p.y + m->mesh[2][2] * p.z + m->mesh[3][2];
    return o;
}

bool invert_gizmo_affine(const editor_mat4* m, editor_mat4* out)
{
    float a[3][3], inv[3][3];
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) a[r][c] = m->mesh[c][r];

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

    float t0 = m->mesh[3][0], t1 = m->mesh[3][1], t2 = m->mesh[3][2];
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) out->mesh[c][r] = inv[r][c];
        out->mesh[3][r] = -(inv[r][0] * t0 + inv[r][1] * t1 + inv[r][2] * t2);
    }
    out->mesh[0][3] = 0.0f;
    out->mesh[1][3] = 0.0f;
    out->mesh[2][3] = 0.0f;
    out->mesh[3][3] = 1.0f;
    return true;
}



/* ---------------------------------------------------------------- drawing
   TODO: drawing helpers belong in the drawing file + header. kept here until they are moved. */

void draw_gizmo_dot(VkCommandBuffer cmd, const gizmo_cam* c, float x, float y, float size, system_color col)
{
    if (x < (float)c->b.offset_x || y < (float)c->b.offset_y ||
        x > (float)(c->b.offset_x + c->b.width) || y > (float)(c->b.offset_y + c->b.height)) return;
    col.alpha = 1.0f;
    push_quad_element(cmd, x - size * 0.5f, y - size * 0.5f, size, size, col);
}

void draw_gizmo_line(VkCommandBuffer cmd, const gizmo_cam* c, float x0, float y0, float x1, float y1, system_color col)
{
    float dx = x1 - x0, dy = y1 - y0;
    int steps = (int)(sqrtf(dx * dx + dy * dy) / 2.0f) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        draw_gizmo_dot(cmd, c, x0 + dx * t, y0 + dy * t, 3.0f, col);
    }
}

system_color get_gizmo_axis_color(int axis, bool highlighted)
{
    if (highlighted)                    return system_theme.system_color_edit_mode_preview_color;
    if (axis == gizmo_axis_x)           return system_theme.system_color_edit_mode_invalid_color;
    if (axis == gizmo_axis_y)           return system_theme.system_color_edit_mode_normal_color;
    if (axis == gizmo_axis_z)           return system_theme.system_edit_mode_active_face_hover;
    return system_theme.system_color_edit_mode_hover_color;
}

/* ---------------------------------------------------------------- terrain sync (Object mode only) */

/* Terrain position is authoritative in int32 1/16 mesh quanta, so quantize and push it through the terrain API. */
void commit_gizmo_object(system_scene_object* o)
{
    if (g_terrain.exists && o->id == g_terrain.scene_object_id) {
        terrain_location_q q;
        q.x = (int32_t)lroundf(o->position.x * (float)terrain_units_per_meter);
        q.y = (int32_t)lroundf(o->position.y * (float)terrain_units_per_meter);
        q.z = (int32_t)lroundf(o->position.z * (float)terrain_units_per_meter);
        set_terrain_location(q);        /* stores the location, persists it and moves the terrain and region objects */
        o->position.x = (float)q.x / (float)terrain_units_per_meter;
        o->position.y = (float)q.y / (float)terrain_units_per_meter;
        o->position.z = (float)q.z / (float)terrain_units_per_meter;
    }
}

/* ---------------------------------------------------------------- target resolution */

/* Object mode: pivot = object position.
   Edit mode:   pivot = world-space centroid of the obj_vert_selected vertices (no selection => no gizmo). */
bool resolve_gizmo_target(system_element_context* ui_element, system_scene_object** out_o, edit_mode_mesh** out_em, editor_vec3* out_pivot)
{
    system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (!o || o->id == gizmo_scene_root_id) return false;

    if (ui_element->active_mode == mode_object) {
        *out_o = o; *out_em = NULL; *out_pivot = o->position;
        return true;
    }
    if (ui_element->active_mode == mode_edit_mesh) {
        edit_mode_mesh* mesh = NULL;
        if (is_terrain_object(o)) mesh = terrain_edit_mesh_current();   /* Terrain / region_xx: the control grid Edit Mode is showing */
        if (!mesh) return false;
        editor_vec3 lc;
        if (em_selection_centroid(mesh, &lc) == 0) return false;
        editor_mat4 model = system_math_model_matrix(o->position, o->rotation, o->scale);
        *out_o = o; *out_em = mesh; *out_pivot = transform_gizmo_point(&model, lc);
        return true;
    }
    return false;
}

bool take_gizmo_edit_snapshot(edit_mode_mesh* mesh, system_scene_object* o)
{
    g_gizmo_model = system_math_model_matrix(o->position, o->rotation, o->scale);
    if (!invert_gizmo_affine(&g_gizmo_model, &g_gizmo_inverse)) return false;
    if (mesh->vert_count > g_gizmo_snap_cap) {
        uint32_t nc = 64;
        if (g_gizmo_snap_cap) nc = g_gizmo_snap_cap;
        while (nc < mesh->vert_count) nc *= 2;
        editor_vec3* p = (editor_vec3*)realloc(g_gizmo_snap, sizeof(editor_vec3) * nc);
        if (!p) return false;
        g_gizmo_snap = p; g_gizmo_snap_cap = nc;
    }
    for (uint32_t i = 0; i < mesh->vert_count; i++) g_gizmo_snap[i] = mesh->verts[i].obj_vert_pos;
    g_gizmo_mesh = mesh;
    return true;
}

/* ---------------------------------------------------------------- hit test */

int test_gizmo_hit(const gizmo_cam* c, enum_dcc_tool tool, editor_vec3 origin, float L, float mx, float my)
{
    float ox, oy;
    if (!project_gizmo_point(c, origin, &ox, &oy)) return gizmo_axis_none;

    if (tool == tool_scale) {
        float dx = mx - ox, dy = my - oy;
        if (sqrtf(dx * dx + dy * dy) < 9.0f) return gizmo_axis_uniform;
    }

    int best = gizmo_axis_none;
    float best_d = gizmo_pick_radius_px;

    for (int i = 0; i < 3; i++) {
        if (tool == tool_rotate) {
            float px = 0.0f, py = 0.0f, qx = 0.0f, qy = 0.0f;
            bool have_prev = false;
            for (int s = 0; s <= gizmo_ring_segments; s++) {
                float a = (float)s / (float)gizmo_ring_segments * 2.0f * gizmo_pi;
                editor_vec3 p = get_gizmo_ring_point(i, a, L, origin);
                editor_vec3 off = { p.x - origin.x, p.y - origin.y, p.z - origin.z };
                bool facing = get_gizmo_view_dir_z(c, off) >= 0.0f;
                bool ok = facing && project_gizmo_point(c, p, &qx, &qy);
                if (ok && have_prev) {
                    float d = get_segment_distance(mx, my, px, py, qx, qy, NULL);
                    if (d < best_d) { best_d = d; best = i; }
                }
                have_prev = ok; px = qx; py = qy;
            }
        } else {
            editor_vec3 tip = { origin.x + g_gizmo_axes[i].x * L, origin.y + g_gizmo_axes[i].y * L, origin.z + g_gizmo_axes[i].z * L };
            float tx, ty;
            if (!project_gizmo_point(c, tip, &tx, &ty)) continue;
            float d = get_segment_distance(mx, my, ox, oy, tx, ty, NULL);
            if (d < best_d) { best_d = d; best = i; }
        }
    }
    return best;
}

/* ---------------------------------------------------------------- drag */

void begin_gizmo_drag(const gizmo_cam* c, system_scene_object* o, edit_mode_mesh* em, editor_vec3 pivot,
                      enum_dcc_tool tool, int axis, float L, float mx, float my)
{
    if (em && !take_gizmo_edit_snapshot(em, o)) return;

    g_gizmo.dragging = true; g_gizmo.edit = (em != NULL);
    g_gizmo.axis = axis; g_gizmo.tool = tool; g_gizmo.id = o->id; g_gizmo.L = L;
    g_gizmo.p0 = pivot; g_gizmo.r0 = o->rotation; g_gizmo.s0 = o->scale;
    g_gizmo.mx0 = mx; g_gizmo.my0 = my;

    float ox = 0.0f, oy = 0.0f;
    project_gizmo_point(c, pivot, &ox, &oy);

    if (tool == tool_rotate) {
        g_gizmo.sx = ox; g_gizmo.sy = oy;            /* ring centre, fixed for the whole drag */
        g_gizmo.last_ang = atan2f(my - oy, mx - ox);
        g_gizmo.accum = 0.0f;
        /* CCW about an axis pointing at the viewer is CCW on screen, which is a NEGATIVE angle with Y down. */
        g_gizmo.sign = 1.0f;
        if (get_gizmo_view_dir_z(c, g_gizmo_axes[axis]) > 0.0f) g_gizmo.sign = -1.0f;
    } else if (axis != gizmo_axis_uniform) {
        editor_vec3 tip = { pivot.x + g_gizmo_axes[axis].x * L, pivot.y + g_gizmo_axes[axis].y * L, pivot.z + g_gizmo_axes[axis].z * L };
        float tx = ox, ty = oy;
        project_gizmo_point(c, tip, &tx, &ty);
        g_gizmo.sx = ox; g_gizmo.sy = oy; g_gizmo.dx = tx - ox; g_gizmo.dy = ty - oy;
        g_gizmo.len2 = g_gizmo.dx * g_gizmo.dx + g_gizmo.dy * g_gizmo.dy;
        g_gizmo.t_grab = 0.0f;
        if (g_gizmo.len2 > 1e-4f) g_gizmo.t_grab = ((mx - g_gizmo.sx) * g_gizmo.dx + (my - g_gizmo.sy) * g_gizmo.dy) / g_gizmo.len2;
    }
}

/* Edit mode: apply the drag to the obj_vert_selected verts. `value` is distance (mesh), angle (rad) or scale factor.
   Works in world space around the drag-start pivot, then converts back to object space. */
void apply_gizmo_edit(system_scene_object* o, float value)
{
    edit_mode_mesh* mesh = g_gizmo_mesh;
    if (!mesh) return;
    const editor_vec3 pv = g_gizmo.p0;
    editor_vec3 ax = g_gizmo_axes[0];
    if (g_gizmo.axis >= 0 && g_gizmo.axis <= 2) ax = g_gizmo_axes[g_gizmo.axis];

    for (uint32_t i = 0; i < mesh->vert_count; i++) {
        if (!mesh->verts[i].obj_vert_selected) continue;

        editor_vec3 w = transform_gizmo_point(&g_gizmo_model, g_gizmo_snap[i]);
        editor_vec3 r = { w.x - pv.x, w.y - pv.y, w.z - pv.z };

        if (g_gizmo.tool == tool_translate) {
            w.x += ax.x * value; w.y += ax.y * value; w.z += ax.z * value;
        }
        else if (g_gizmo.tool == tool_rotate) {
            r = rotate_vec3_about_axis(r, ax, value);
            w.x = pv.x + r.x; w.y = pv.y + r.y; w.z = pv.z + r.z;
        }
        else if (g_gizmo.tool == tool_scale) {
            if (g_gizmo.axis == gizmo_axis_uniform) {
                w.x = pv.x + r.x * value; w.y = pv.y + r.y * value; w.z = pv.z + r.z * value;
            } else {
                float k = (r.x * ax.x + r.y * ax.y + r.z * ax.z) * (value - 1.0f);
                w.x += ax.x * k; w.y += ax.y * k; w.z += ax.z * k;
            }
        }
        mesh->verts[i].obj_vert_pos = transform_gizmo_point(&g_gizmo_inverse, w);
    }
    em_commit_to_gpu(mesh, o);
}

void update_gizmo_drag(system_scene_object* o, float mx, float my)
{
    const object_mode_state* os = &g_mode_state.object;

    if (g_gizmo.tool == tool_rotate) {
        float ox = g_gizmo.sx, oy = g_gizmo.sy;
        float ang = atan2f(my - oy, mx - ox);
        float d = ang - g_gizmo.last_ang;
        while (d >  gizmo_pi) d -= 2.0f * gizmo_pi;
        while (d < -gizmo_pi) d += 2.0f * gizmo_pi;
        g_gizmo.last_ang = ang;
        g_gizmo.accum += d * g_gizmo.sign;

        float total = g_gizmo.accum;
        if (os->snap_enabled) total = snap_gizmo_value(total, os->snap_rotate_deg * gizmo_pi / 180.0f);
        if (g_gizmo.edit) { apply_gizmo_edit(o, total); return; }
        o->rotation = g_gizmo.r0;
        *get_vec3_component(&o->rotation, g_gizmo.axis) = *get_vec3_component(&g_gizmo.r0, g_gizmo.axis) + total;
    }
    else if (g_gizmo.tool == tool_translate) {
        if (g_gizmo.len2 < 1e-4f) return;     /* axis points straight at the camera */
        float t = ((mx - g_gizmo.sx) * g_gizmo.dx + (my - g_gizmo.sy) * g_gizmo.dy) / g_gizmo.len2 - g_gizmo.t_grab;
        float dist = t * g_gizmo.L;
        if (os->snap_enabled) dist = snap_gizmo_value(dist, os->snap_translate_m);
        if (g_gizmo.edit) { apply_gizmo_edit(o, dist); return; }
        o->position = g_gizmo.p0;
        *get_vec3_component(&o->position, g_gizmo.axis) = *get_vec3_component(&g_gizmo.p0, g_gizmo.axis) + dist;
    }
    else if (g_gizmo.tool == tool_scale) {
        float f;
        if (g_gizmo.axis == gizmo_axis_uniform) {
            f = 1.0f + ((mx - g_gizmo.mx0) - (my - g_gizmo.my0)) / 150.0f;
            if (f < 0.01f) f = 0.01f;
            if (os->snap_enabled) f = snap_gizmo_value(f, os->snap_scale);
            if (f < 0.01f) f = 0.01f;
            if (g_gizmo.edit) { apply_gizmo_edit(o, f); return; }
            o->scale.x = g_gizmo.s0.x * f; o->scale.y = g_gizmo.s0.y * f; o->scale.z = g_gizmo.s0.z * f;
        } else {
            if (g_gizmo.len2 < 1e-4f) return;
            float t = ((mx - g_gizmo.sx) * g_gizmo.dx + (my - g_gizmo.sy) * g_gizmo.dy) / g_gizmo.len2 - g_gizmo.t_grab;
            if (g_gizmo.edit) {
                f = 1.0f + t;
                if (os->snap_enabled) f = snap_gizmo_value(f, os->snap_scale);
                if (f < 0.001f) f = 0.001f;
                apply_gizmo_edit(o, f);
                return;
            }
            float base = *get_vec3_component(&g_gizmo.s0, g_gizmo.axis);
            float new_scale = base * (1.0f + t);
            if (os->snap_enabled) new_scale = snap_gizmo_value(new_scale, os->snap_scale);
            if (new_scale < 0.001f) new_scale = 0.001f;
            o->scale = g_gizmo.s0;
            *get_vec3_component(&o->scale, g_gizmo.axis) = new_scale;
        }
    }
    commit_gizmo_object(o);
}

/* ---------------------------------------------------------------- public */

bool system_transform_gizmo_wants_mouse(void)
{
    return g_gizmo.dragging || g_gizmo.hover != gizmo_axis_none;
}

void cancel_gizmo_drag(void)
{
    g_gizmo.dragging = false;
    g_gizmo.axis = gizmo_axis_none;
}

void run_transform_gizmo(VkCommandBuffer cmd, system_element_context* ui_element, bool do_update, bool do_draw)
{
    bool pressed = false;

    if (do_update) {
        mode_state_sync_from_ui(ui_element);
        const bool down = ui_element->input.mouse_down;
        pressed = down && !g_gizmo_prev_down;
        g_gizmo_prev_down = down;
        if (g_gizmo.dragging && !down) cancel_gizmo_drag();
        g_gizmo.hover = gizmo_axis_none;
    }

    const enum_dcc_tool tool = ui_element->active_tool;
    gizmo_cam c;
    c.b = ui_element->scene_view;
    if (c.b.width <= 0 || c.b.height <= 0) { g_gizmo.dragging = false; return; }

    system_scene_object* o = NULL;
    edit_mode_mesh* em = NULL;
    editor_vec3 pivot = { 0, 0, 0 };

    bool usable = (tool != tool_select) && resolve_gizmo_target(ui_element, &o, &em, &pivot);
    if (!usable) { cancel_gizmo_drag(); return; }
    if (g_gizmo.dragging && (o->id != g_gizmo.id || g_gizmo.edit != (em != NULL))) { cancel_gizmo_drag(); return; }

    /* Let the fly camera / orbit have the mouse */
    if (ui_element->input.alt_pressed || ui_element->input.middle_down || ui_element->input.camera_manipulating) {
        cancel_gizmo_drag(); return;
    }

    float aspect = (float)c.b.width / (float)c.b.height;
    editor_mat4 view, proj;
    build_scene_orbit_camera(aspect, &view, &proj);
    c.view = view;
    c.vp   = system_math_mat4_multiply(proj, view);

    float depth = get_gizmo_view_depth(&c, pivot);
    if (depth < 0.5f) depth = 0.5f;
    float L = depth * gizmo_screen_k;

    const float mx = (float)ui_element->input.mouse_x, my = (float)ui_element->input.mouse_y;
    const bool in_view = mx >= c.b.offset_x && my >= c.b.offset_y &&
                         mx < c.b.offset_x + c.b.width && my < c.b.offset_y + c.b.height;

    if (do_update) {
        if (g_gizmo.dragging) {
            update_gizmo_drag(o, mx, my);
        } else if (in_view) {
            g_gizmo.hover = test_gizmo_hit(&c, tool, pivot, L, mx, my);
            if (pressed && g_gizmo.hover != gizmo_axis_none) begin_gizmo_drag(&c, o, em, pivot, tool, g_gizmo.hover, L, mx, my);
        }
        if (g_gizmo.dragging) g_gizmo.hover = g_gizmo.axis;
    }
    if (!do_draw) return;

    /* ---- draw (re-resolve so the handle sits on the possibly just-moved selection) ---- */
    if (!resolve_gizmo_target(ui_element, &o, &em, &pivot)) return;

    float ox, oy;
    if (!project_gizmo_point(&c, pivot, &ox, &oy)) return;

    for (int i = 0; i < 3; i++) {
        system_color col = get_gizmo_axis_color(i, g_gizmo.hover == i);
        if (tool == tool_rotate) {
            float px = 0.0f, py = 0.0f, qx = 0.0f, qy = 0.0f;
            bool have_prev = false;
            for (int s = 0; s <= gizmo_ring_segments; s++) {
                float a = (float)s / (float)gizmo_ring_segments * 2.0f * gizmo_pi;
                editor_vec3 p = get_gizmo_ring_point(i, a, L, pivot);
                editor_vec3 off = { p.x - pivot.x, p.y - pivot.y, p.z - pivot.z };
                bool ok = get_gizmo_view_dir_z(&c, off) >= 0.0f && project_gizmo_point(&c, p, &qx, &qy);
                if (ok && have_prev) draw_gizmo_line(cmd, &c, px, py, qx, qy, col);
                have_prev = ok; px = qx; py = qy;
            }
        } else {
            editor_vec3 tip = { pivot.x + g_gizmo_axes[i].x * L, pivot.y + g_gizmo_axes[i].y * L, pivot.z + g_gizmo_axes[i].z * L };
            float tx, ty;
            if (!project_gizmo_point(&c, tip, &tx, &ty)) continue;
            draw_gizmo_line(cmd, &c, ox, oy, tx, ty, col);
            float tip_size = 8.0f;
            if (tool == tool_scale) tip_size = 10.0f;
            draw_gizmo_dot(cmd, &c, tx, ty, tip_size, col);
        }
    }
    if (tool == tool_scale) draw_gizmo_dot(cmd, &c, ox, oy, 12.0f, get_gizmo_axis_color(gizmo_axis_uniform, g_gizmo.hover == gizmo_axis_uniform));
}

/* Input + transform logic only. Safe to call from anywhere that runs every frame. */
void system_transform_gizmo_update(system_element_context* ui_element)
{
    run_transform_gizmo(VK_NULL_HANDLE, ui_element, true, false);
}

/* Drawing only. Must run after the Scene view image is drawn or it will be covered. */
void system_transform_gizmo_draw(VkCommandBuffer cmd, system_element_context* ui_element)
{
    run_transform_gizmo(cmd, ui_element, false, true);
}

void system_transform_gizmo_update_and_draw(VkCommandBuffer cmd, system_element_context* ui_element)
{
    run_transform_gizmo(cmd, ui_element, true, true);
}

/* World -> Scene view pixel projection. Lives here with the object gizmo, not with the UI overlay. */
bool project_world_to_viewport(editor_vec3 world_pos, editor_mat4 view, editor_mat4 proj, struct_viewport_bounds view_bounds, float* out_x, float* out_y)
{
    /* clip space coordinate translation matrix multiplication */
    float x = world_pos.x * view.mesh[0][0] + world_pos.y * view.mesh[1][0] + world_pos.z * view.mesh[2][0] + view.mesh[3][0];
    float y = world_pos.x * view.mesh[0][1] + world_pos.y * view.mesh[1][1] + world_pos.z * view.mesh[2][1] + view.mesh[3][1];
    float z = world_pos.x * view.mesh[0][2] + world_pos.y * view.mesh[1][2] + world_pos.z * view.mesh[2][2] + view.mesh[3][2];
    float w = world_pos.x * view.mesh[0][3] + world_pos.y * view.mesh[1][3] + world_pos.z * view.mesh[2][3] + view.mesh[3][3];

    if (w <= 0.001f) return false;   /* behind camera clip prevention drop */

    /* normalized device coordinates (ndc) mapping */
    float ndc_x = (x * proj.mesh[0][0] + y * proj.mesh[1][0] + z * proj.mesh[2][0] + proj.mesh[3][0]) / w;
    float ndc_y = (x * proj.mesh[0][1] + y * proj.mesh[1][1] + z * proj.mesh[2][1] + proj.mesh[3][1]) / w;

    /* convert ndc (-1 to 1) into actual local pixel offsets inside the workspace panel bounds */
    *out_x = (float)view_bounds.offset_x + ((ndc_x + 1.0f) * 0.5f) * (float)view_bounds.width;
    *out_y = (float)view_bounds.offset_y + ((1.0f - ndc_y) * 0.5f) * (float)view_bounds.height;

    return true;
}