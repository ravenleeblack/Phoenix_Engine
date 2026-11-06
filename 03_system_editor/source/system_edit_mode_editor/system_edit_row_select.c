#include "system_globals.h"


/* ========================================================================= */
/* ROW SELECT (double click, no tool on)                                     */
/* ========================================================================= */
static edit_pick_selection    s_last_click = { select_vert, EM_NONE };
static edit_pick_selection    s_prev_click = { select_vert, EM_NONE };
static ULONGLONG s_last_time = 0;
static int       s_last_mx = 0, s_last_my = 0;

static bool edges_share_face(const edit_mode_mesh* mesh, uint32_t x, uint32_t y)
{
    for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) {
        const uint32_t fx = mesh->edges[x].face[i], fy = mesh->edges[y].face[j];
        if (fx != EM_NONE && fx == fy) return true;
    }
    return false;
}

/* the edge_to_face_array that carries on straight through vertex w */
static uint32_t opposite_edge(const edit_mode_mesh* mesh, uint32_t e, uint32_t w)
{
    uint32_t inc[8]; int ni = 0;
    for (uint32_t i = 0; i < mesh->edge_count; ++i)
        if (i != e && (mesh->edges[i].vert_to_edge_array[0] == w || mesh->edges[i].vert_to_edge_array[1] == w)) { if (ni < 8) inc[ni] = i; ++ni; }
    if (ni == 3) { for (int i = 0; i < 3; ++i) if (!edges_share_face(mesh, e, inc[i])) return inc[i]; }       /* interior vertex, 4 edges */
    if (ni == 2 && mesh->edges[e].face_count == 1) { for (int i = 0; i < 2; ++i) if (mesh->edges[inc[i]].face_count == 1) return inc[i]; }   /* along a border */
    return EM_NONE;
}

static void walk_edge_loop(edit_mode_mesh* mesh, uint32_t e, uint32_t w)
{
    for (int guard = 0; guard < 4096; ++guard) {
        const uint32_t nx = opposite_edge(mesh, e, w);
        if (nx == EM_NONE || mesh->edges[nx].obj_vert_selected) return;
        mesh->edges[nx].obj_vert_selected = 1;
        w = (mesh->edges[nx].vert_to_edge_array[0] == w) ? mesh->edges[nx].vert_to_edge_array[1] : mesh->edges[nx].vert_to_edge_array[0];
        e = nx;
    }
}

/* shortest path between two verts over the edge_to_face_array graph (layer by layer). marks the verts on the path. */
static bool path_verts(edit_mode_mesh* mesh, uint32_t va, uint32_t vb)
{
    const uint32_t n = mesh->vert_count;
    int32_t* dist = (int32_t*)malloc(sizeof(int32_t) * n);
    if (!dist) return false;
    for (uint32_t i = 0; i < n; ++i) dist[i] = -1;
    dist[va] = 0;
    bool grew = true;
    for (int layer = 0; dist[vb] < 0 && grew && layer < 100000; ++layer) {
        grew = false;
        for (uint32_t i = 0; i < mesh->edge_count; ++i) {
            const uint32_t a = mesh->edges[i].vert_to_edge_array[0], b = mesh->edges[i].vert_to_edge_array[1];
            if (dist[a] == layer && dist[b] < 0) { dist[b] = layer + 1; grew = true; }
            else if (dist[b] == layer && dist[a] < 0) { dist[a] = layer + 1; grew = true; }
        }
    }
    bool ok = dist[vb] >= 0;
    if (ok) {
        uint32_t cur = vb;
        mesh->verts[cur].obj_vert_selected = 1;
        while (cur != va) {
            uint32_t next = EM_NONE;
            for (uint32_t i = 0; i < mesh->edge_count && next == EM_NONE; ++i) {
                const uint32_t a = mesh->edges[i].vert_to_edge_array[0], b = mesh->edges[i].vert_to_edge_array[1];
                if (a == cur && dist[b] == dist[cur] - 1) next = b;
                else if (b == cur && dist[a] == dist[cur] - 1) next = a;
            }
            if (next == EM_NONE) break;
            cur = next;
            mesh->verts[cur].obj_vert_selected = 1;
        }
    }
    free(dist);
    return ok;
}

static bool path_faces(edit_mode_mesh* mesh, uint32_t fa, uint32_t fb)
{
    const uint32_t n = mesh->face_count;
    int32_t* dist = (int32_t*)malloc(sizeof(int32_t) * n);
    if (!dist) return false;
    for (uint32_t i = 0; i < n; ++i) dist[i] = -1;
    dist[fa] = 0;
    bool grew = true;
    for (int layer = 0; dist[fb] < 0 && grew && layer < 100000; ++layer) {
        grew = false;
        for (uint32_t i = 0; i < mesh->edge_count; ++i) {
            if (mesh->edges[i].face_count < 2) continue;
            const uint32_t a = mesh->edges[i].face[0], b = mesh->edges[i].face[1];
            if (a == EM_NONE || b == EM_NONE) continue;
            if (dist[a] == layer && dist[b] < 0) { dist[b] = layer + 1; grew = true; }
            else if (dist[b] == layer && dist[a] < 0) { dist[a] = layer + 1; grew = true; }
        }
    }
    bool ok = dist[fb] >= 0;
    if (ok) {
        uint32_t cur = fb;
        mesh->faces[cur].obj_vert_selected = 1;
        while (cur != fa) {
            uint32_t next = EM_NONE;
            for (uint32_t i = 0; i < mesh->edge_count && next == EM_NONE; ++i) {
                if (mesh->edges[i].face_count < 2) continue;
                const uint32_t a = mesh->edges[i].face[0], b = mesh->edges[i].face[1];
                if (a == cur && b != EM_NONE && dist[b] == dist[cur] - 1) next = b;
                else if (b == cur && a != EM_NONE && dist[a] == dist[cur] - 1) next = a;
            }
            if (next == EM_NONE) break;
            cur = next;
            mesh->faces[cur].obj_vert_selected = 1;
        }
    }
    free(dist);
    return ok;
}

/* A was clicked, then B was double clicked: select the row that runs through both. */
static void select_row(edit_mode_mesh* mesh, enum_mesh_selection kind, uint32_t A, uint32_t B)
{
    static edit_mode_selection_ring R;
    em_clear_selection(mesh);

    if (kind == select_face) {
        uint32_t shared = EM_NONE;
        for (int k = 0; k < mesh->faces[A].face_type && shared == EM_NONE; ++k) {
            const edit_mode_edge* E = &mesh->edges[mesh->faces[A].edge_to_face_array[k]];
            if (E->face_count == 2 && (E->face[0] == B || E->face[1] == B)) shared = mesh->faces[A].edge_to_face_array[k];
        }
        if (shared != EM_NONE && edit_ring_build(mesh, shared, &R)) {
            for (uint32_t i = 0; i < edit_ring_face_count(&R); ++i) mesh->faces[R.face[i]].obj_vert_selected = 1;   /* the whole strip across the shared edge_to_face_array */
        } else if (!path_faces(mesh, A, B)) { mesh->faces[A].obj_vert_selected = mesh->faces[B].obj_vert_selected = 1; }
        em_sync_selection(mesh, select_face);
    }
    else if (kind == select_edge) {
        const edit_mode_edge *EA = &mesh->edges[A], *EB = &mesh->edges[B];
        uint32_t w = EM_NONE;                                   /* a shared vertex means the two edges lie on one loop */
        for (int i = 0; i < 2 && w == EM_NONE; ++i) for (int j = 0; j < 2; ++j) if (EA->vert_to_edge_array[i] == EB->vert_to_edge_array[j]) { w = EA->vert_to_edge_array[i]; break; }
        mesh->edges[A].obj_vert_selected = mesh->edges[B].obj_vert_selected = 1;
        if (w != EM_NONE) {
            walk_edge_loop(mesh, A, EA->vert_to_edge_array[0] == w ? EA->vert_to_edge_array[1] : EA->vert_to_edge_array[0]);     /* carry on beyond each end ... */
            walk_edge_loop(mesh, B, EB->vert_to_edge_array[0] == w ? EB->vert_to_edge_array[1] : EB->vert_to_edge_array[0]);
        } else if (edges_share_face(mesh, A, B) && edit_ring_build(mesh, A, &R)) {   /* opposite sides of a quad: the ring (a row of quads) */
            for (uint32_t i = 0; i < R.n; ++i) mesh->edges[R.edge_to_face_array[i]].obj_vert_selected = 1;
        } else {                                                          /* far apart: the shortest way between them */
            uint32_t bi = 0, bj = 0; float best = 1e30f;
            for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) {
                const editor_vec3 d = sub3(mesh->verts[EA->vert_to_edge_array[i]].obj_vert_pos, mesh->verts[EB->vert_to_edge_array[j]].obj_vert_pos);
                if (dot3(d, d) < best) { best = dot3(d, d); bi = (uint32_t)i; bj = (uint32_t)j; }
            }
            em_clear_selection(mesh);
            if (path_verts(mesh, EA->vert_to_edge_array[bi], EB->vert_to_edge_array[bj])) {
                for (uint32_t i = 0; i < mesh->edge_count; ++i)
                    if (mesh->verts[mesh->edges[i].vert_to_edge_array[0]].obj_vert_selected && mesh->verts[mesh->edges[i].vert_to_edge_array[1]].obj_vert_selected) mesh->edges[i].obj_vert_selected = 1;
            }
            mesh->edges[A].obj_vert_selected = mesh->edges[B].obj_vert_selected = 1;
        }
        em_sync_selection(mesh, select_edge);
    }
    else {
        mesh->verts[A].obj_vert_selected = 1;
        path_verts(mesh, A, B);
        em_sync_selection(mesh, select_vert);
    }
}

void edit_row_select_update(const edit_frame* f)
{
    if (!f->press || !f->inside) return;
    system_element_context* ui_element = f->ui_element;
    edit_mode_mesh* mesh = f->mesh;

    const enum_mesh_selection kind = ui_element->active_select_type;
    const edit_pick_selection p = edit_pick_element(mesh, kind, f->mx, f->my);
    const ULONGLONG now = GetTickCount64();
    const bool same = (p.index != EM_NONE) && s_last_click.index == p.index && s_last_click.kind == p.kind;
    const bool dbl  = same && (now - s_last_time) < 350 && abs(ui_element->input.mouse_x - s_last_mx) < 6 && abs(ui_element->input.mouse_y - s_last_my) < 6;

    if (dbl) {
        if (s_prev_click.index != EM_NONE && s_prev_click.kind == p.kind && s_prev_click.index != p.index) {
            select_row(mesh, kind, s_prev_click.index, p.index);
            em_set_active_element(mesh, kind, p.index);
            edit_msg("Row obj_vert_selected");
        }
        s_prev_click = p; s_last_click = p; s_last_time = 0;
    } else {
        s_prev_click = s_last_click;
        s_last_click = p;
        s_last_time  = now;
        s_last_mx = ui_element->input.mouse_x; s_last_my = ui_element->input.mouse_y;
    }
}