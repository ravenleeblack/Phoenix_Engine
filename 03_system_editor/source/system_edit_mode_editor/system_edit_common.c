#include "system_globals.h"
#include "system_edit_manager.h"
#include "system_scene_terrain.h"



/* ========================================================================= */
/* messages                                                                  */
/* ========================================================================= */
static char       s_msg[112];
static ULONGLONG  s_msg_time = 0;

void edit_msg(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_msg, sizeof(s_msg), fmt, ap);
    va_end(ap);
    s_msg_time = GetTickCount64();
    phoenix_info("EditOps", "%s", s_msg);
}

/* short message at the bottom of the Scene view: what the last tool did, or why it could not */
void edit_msg_draw(VkCommandBuffer cmd, system_element_context* ui_element)
{
    if (s_msg[0] && GetTickCount64() - s_msg_time < 3500) {
        const struct_viewport_bounds* b = &ui_element->scene_view;
        const int tw = (int)strlen(s_msg) * 8;
        const int x = b->offset_x + (b->width - tw) / 2, y = b->offset_y + b->height - 34;
        push_quad_element(cmd, (float)(x - 8), (float)(y - 4), (float)(tw + 16), 22.0f, system_theme.tab_content_window);
        draw_text_element(cmd, ui_element, s_msg, x, y, system_theme.system_text);
    }
}

/* ========================================================================= */
/* target: the mesh of the obj_vert_selected object                                   */
/* ========================================================================= */
bool get_edit_target(system_scene_object** out_obj, edit_mode_mesh** out_m)
{
    if (g_mode_state.context != mode_context_edit) return false;
    system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (!o || o == &g_scene_registry.objects[0]) return false;
    edit_mode_mesh* mesh = is_terrain_object(o) ? terrain_edit_mesh_acquire(o)     /* terrain: the control grid of the edit chunk */
                                                : em_registry_get_or_create(o);
    if (!mesh || mesh->face_count == 0) return false;
    *out_obj = o;
    *out_m   = mesh;
    return true;
}

editor_vec3 edit_face_normal(const edit_mode_mesh* mesh, const edit_mode_face* F)
{
    const editor_vec3 p0 = mesh->verts[F->vert_to_edge_array[0]].obj_vert_pos;
    const editor_vec3 p1 = mesh->verts[F->vert_to_edge_array[1]].obj_vert_pos;
    const editor_vec3 p2 = mesh->verts[F->vert_to_edge_array[2]].obj_vert_pos;
    const editor_vec3 p3 = (F->face_type == 4) ? mesh->verts[F->vert_to_edge_array[3]].obj_vert_pos : p0;
    return system_math_compute_face_normal(p0, p1, p2, p3, F->face_type);
}

/* ========================================================================= */
/* face lists + rebuild                                                      */
/* ========================================================================= */
bool nl_push(num_of_faces_list* L, const num_of_faces* f)
{
    if (L->n == L->cap) {
        uint32_t nc = L->cap ? L->cap * 2 : 64;
        num_of_faces* p = (num_of_faces*)realloc(L->f, (size_t)nc * sizeof(num_of_faces));
        if (!p) return false;
        L->f = p; L->cap = nc;
    }
    L->f[L->n++] = *f;
    return true;
}

bool nl_push_existing(num_of_faces_list* L, const edit_mode_mesh* mesh, uint32_t fi)
{
    const edit_mode_face* F = &mesh->faces[fi];
    num_of_faces n;
    memset(&n, 0, sizeof(n));
    n.face_type = F->face_type;
    for (int k = 0; k < F->face_type; ++k) n.vert_to_edge_array[k] = F->vert_to_edge_array[k];
    memcpy(n.uv, F->uv, sizeof(n.uv));
    n.obj_vert_selected = F->obj_vert_selected;
    return nl_push(L, &n);
}

static void nface_default_uv(num_of_faces* f)
{
    static const float q[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    static const float t[3][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 } };
    for (int k = 0; k < f->face_type; ++k) {
        f->uv[k][0] = (f->face_type == 4) ? q[k][0] : t[k][0];
        f->uv[k][1] = (f->face_type == 4) ? q[k][1] : t[k][1];
    }
}

bool nl_push_quad(num_of_faces_list* L, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    num_of_faces n;
    memset(&n, 0, sizeof(n));
    n.face_type = 4; n.vert_to_edge_array[0] = a; n.vert_to_edge_array[1] = b; n.vert_to_edge_array[2] = c; n.vert_to_edge_array[3] = d;
    nface_default_uv(&n);
    return nl_push(L, &n);
}

/* verts keep their obj_vert_selected flag, faces keep theirs, then em_sync_selection(basis) makes edges agree. */
bool em_rebuild(edit_mode_mesh* mesh, const num_of_faces* faces, uint32_t fc, enum_mesh_selection basis)
{
    if (!mesh || !faces || fc == 0 || mesh->vert_count == 0) return false;

    uint32_t* remap = (uint32_t*)malloc(sizeof(uint32_t) * mesh->vert_count);
    uint8_t*  used  = (uint8_t*)calloc(mesh->vert_count, 1);
    if (!remap || !used) { free(remap); free(used); return false; }
    memset(remap, 0xFF, sizeof(uint32_t) * mesh->vert_count);

    for (uint32_t i = 0; i < fc; ++i)
        for (int k = 0; k < faces[i].face_type; ++k)
            if (faces[i].vert_to_edge_array[k] < mesh->vert_count) used[faces[i].vert_to_edge_array[k]] = 1;

    edit_mode_mesh t;
    memset(&t, 0, sizeof(t));
    bool ok = true;
    for (uint32_t i = 0; i < mesh->vert_count && ok; ++i) {
        if (!used[i]) continue;
        const uint32_t id = em_add_vert(&t, mesh->verts[i].obj_vert_pos, mesh->verts[i].obj_vert_color);
        if (id == EM_NONE) { ok = false; break; }
        t.verts[id].obj_vert_selected = mesh->verts[i].obj_vert_selected;
        remap[i] = id;
    }
    for (uint32_t i = 0; i < fc && ok; ++i) {
        uint32_t v2[4];
        bool good = true;
        for (int k = 0; k < faces[i].face_type; ++k) {
            v2[k] = (faces[i].vert_to_edge_array[k] < mesh->vert_count) ? remap[faces[i].vert_to_edge_array[k]] : EM_NONE;
            if (v2[k] == EM_NONE) good = false;
        }
        if (!good) continue;
        const uint32_t fi = em_add_face(&t, v2, faces[i].face_type);
        if (fi == EM_NONE) continue;                         /* duplicate corner: the face collapsed, drop it */
        memcpy(t.faces[fi].uv, faces[i].uv, sizeof(faces[i].uv));
        t.faces[fi].obj_vert_selected = faces[i].obj_vert_selected;
    }

    free(remap); free(used);
    if (!ok || t.face_count == 0) { em_mesh_free(&t); return false; }

    em_mesh_free(mesh);
    *mesh = t;
    em_sync_selection(mesh, basis);
    return true;
}

/* ========================================================================= */
/* WELD core: map[vert_to_edge_array] = the vertex vert_to_edge_array becomes. Used by Target Weld and Merge.  */
/* ========================================================================= */
bool edit_weld_with_map(edit_mode_mesh* mesh, const uint32_t* map, uint32_t keep_selected_vert)
{
    num_of_faces_list L; memset(&L, 0, sizeof(L));
    bool ok = true;
    for (uint32_t i = 0; i < mesh->face_count && ok; ++i) {
        const edit_mode_face* F = &mesh->faces[i];
        uint32_t t[4], u[4]; float uv[4][2]; int un = 0;
        for (int k = 0; k < F->face_type; ++k) t[k] = map[F->vert_to_edge_array[k]];
        for (int k = 0; k < F->face_type; ++k) {                  /* drop corners that now repeat their neighbour */
            if (un > 0 && u[un - 1] == t[k]) continue;
            u[un] = t[k]; uv[un][0] = F->uv[k][0]; uv[un][1] = F->uv[k][1]; ++un;
        }
        if (un > 1 && u[0] == u[un - 1]) --un;
        if (un < 3) continue;                                  /* collapsed */
        if (un == 4 && (u[0] == u[2] || u[1] == u[3])) continue;

        num_of_faces n; memset(&n, 0, sizeof(n));
        n.face_type = un; n.obj_vert_selected = 0;
        for (int k = 0; k < un; ++k) { n.vert_to_edge_array[k] = u[k]; n.uv[k][0] = uv[k][0]; n.uv[k][1] = uv[k][1]; }
        ok = nl_push(&L, &n);
    }
    for (uint32_t vert_to_edge_array = 0; vert_to_edge_array < mesh->vert_count; ++vert_to_edge_array) mesh->verts[vert_to_edge_array].obj_vert_selected = (vert_to_edge_array == keep_selected_vert) ? 1 : 0;
    if (ok && L.n > 0) ok = em_rebuild(mesh, L.f, L.n, select_vert);
    else ok = false;
    free(L.f);
    return ok;
}

/* ========================================================================= */
/* picking, matched to the Scene view camera                                 */
/* ========================================================================= */
#define PICK_VERT_PX 12.0f
#define PICK_EDGE_PX 8.0f

static cam_screen*     s_scr = NULL;
static uint32_t s_scr_cap = 0;

const cam_screen* edit_screen_points(void) { return s_scr; }

bool begin_edit_camera(edit_mode_camera_context* c, const system_element_context* ui_element, const system_scene_object* obj)
{
    c->vx = ui_element->scene_view.offset_x; c->vy = ui_element->scene_view.offset_y;
    c->vw = ui_element->scene_view.width;    c->vh = ui_element->scene_view.height;
    if (c->vw <= 0 || c->vh <= 0) return false;
    editor_mat4 view, proj;
    build_scene_view_camera((float)c->vw / (float)c->vh, &view, &proj);
    const editor_mat4 model = system_math_model_matrix(obj->position, obj->rotation, obj->scale);
    c->mvp = system_math_mat4_multiply(proj, system_math_mat4_multiply(view, model));
    return true;
}

cam_screen project_edit_point(const edit_mode_camera_context* c, editor_vec3 p) { return system_math_project_point(c, p); }

bool project_edit_points_all(const edit_mode_camera_context* c, const edit_mode_mesh* mesh)
{
    if (mesh->vert_count > s_scr_cap) {
        cam_screen* p = (cam_screen*)realloc(s_scr, sizeof(cam_screen) * mesh->vert_count);
        if (!p) return false;
        s_scr = p; s_scr_cap = mesh->vert_count;
    }
    for (uint32_t i = 0; i < mesh->vert_count; ++i) s_scr[i] = project_edit_point(c, mesh->verts[i].obj_vert_pos);
    return true;
}

/* all pick_* need edit_project_all() first */
uint32_t edit_pick_vert(const edit_mode_mesh* mesh, float mx, float my, uint32_t exclude)
{
    float d0 = 1e30f;
    for (uint32_t i = 0; i < mesh->vert_count; ++i) {
        if (!s_scr[i].ok || i == exclude) continue;
        const float d = hypotf(s_scr[i].x - mx, s_scr[i].y - my);
        if (d <= PICK_VERT_PX && d < d0) d0 = d;
    }
    uint32_t best = EM_NONE; float bw = 1e30f;               /* among the near-equal ones the front-most wins */
    for (uint32_t i = 0; i < mesh->vert_count; ++i) {
        if (!s_scr[i].ok || i == exclude) continue;
        const float d = hypotf(s_scr[i].x - mx, s_scr[i].y - my);
        if (d <= PICK_VERT_PX && d <= d0 + 4.0f && s_scr[i].w < bw) { bw = s_scr[i].w; best = i; }
    }
    return best;
}

uint32_t edit_pick_edge(const edit_mode_mesh* mesh, float mx, float my)
{
    float d0 = 1e30f;
    for (uint32_t i = 0; i < mesh->edge_count; ++i) {
        const cam_screen a = s_scr[mesh->edges[i].vert_to_edge_array[0]], b = s_scr[mesh->edges[i].vert_to_edge_array[1]];
        if (!a.ok || !b.ok) continue;
        const float d = system_math_compute_segment_distance(mx, my, a.x, a.y, b.x, b.y);
        if (d <= PICK_EDGE_PX && d < d0) d0 = d;
    }
    uint32_t best = EM_NONE; float bw = 1e30f;
    for (uint32_t i = 0; i < mesh->edge_count; ++i) {
        const cam_screen a = s_scr[mesh->edges[i].vert_to_edge_array[0]], b = s_scr[mesh->edges[i].vert_to_edge_array[1]];
        if (!a.ok || !b.ok) continue;
        const float d = system_math_compute_segment_distance(mx, my, a.x, a.y, b.x, b.y);
        const float w = (a.w + b.w) * 0.5f;
        if (d <= PICK_EDGE_PX && d <= d0 + 3.0f && w < bw) { bw = w; best = i; }
    }
    return best;
}

uint32_t edit_pick_face(const edit_mode_mesh* mesh, float mx, float my)
{
    uint32_t best = EM_NONE; float bw = 1e30f;
    for (uint32_t i = 0; i < mesh->face_count; ++i) {
        const edit_mode_face* F = &mesh->faces[i];
        bool ok = true; float w = 0.0f;
        for (int k = 0; k < F->face_type; ++k) { if (!s_scr[F->vert_to_edge_array[k]].ok) ok = false; w += s_scr[F->vert_to_edge_array[k]].w; }
        if (!ok) continue;
        w /= (float)F->face_type;
        const cam_screen a = s_scr[F->vert_to_edge_array[0]], b = s_scr[F->vert_to_edge_array[1]], c = s_scr[F->vert_to_edge_array[2]];
        bool hit = system_math_is_point_in_triangle(mx, my, a, b, c);
        if (!hit && F->face_type == 4) hit = system_math_is_point_in_triangle(mx, my, a, c, s_scr[F->vert_to_edge_array[3]]);
        if (hit && w < bw) { bw = w; best = i; }
    }
    return best;
}

edit_pick_selection pick_edit_element(const edit_mode_mesh* mesh, enum_mesh_selection kind, float mx, float my)
{
    edit_pick_selection p; p.kind = kind; p.index = EM_NONE;
    if      (kind == select_vert) p.index = edit_pick_vert(mesh, mx, my, EM_NONE);
    else if (kind == select_edge) p.index = edit_pick_edge(mesh, mx, my);
    else                          p.index = edit_pick_face(mesh, mx, my);
    return p;
}

bool is_edit_in_scene_view(const system_element_context* ui_element)
{
    const struct_viewport_bounds* b = &ui_element->scene_view;
    return b->width > 0 && validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, b->offset_x, b->offset_y, b->width, b->height);
}