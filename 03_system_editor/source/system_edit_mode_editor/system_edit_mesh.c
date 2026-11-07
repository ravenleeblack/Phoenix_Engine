#include "system_globals.h"
#include "system_object_database.h"


/* Set to 0 if upload_scene_object_mesh() already destroys the object's old buffers. */
#define EM_FREE_OLD_BUFFERS 0   /* upload_scene_object_mesh() now retires the old buffers itself */

/* ------------------------------------------------------------------------- */
/* small helpers                                                             */
/* ------------------------------------------------------------------------- */
static bool em_reserve(void** data, size_t elem, uint32_t* cap, uint32_t need)
{
    if (need <= *cap) return true;
    uint32_t nc = *cap ? *cap : 16;
    while (nc < need) nc *= 2;
    void* p = realloc(*data, (size_t)nc * elem);
    if (!p) return false;
    *data = p;
    *cap = nc;
    return true;
}

/* ------------------------------------------------------------------------- */
/* lifetime                                                                  */
/* ------------------------------------------------------------------------- */
void em_mesh_clear(edit_mode_mesh* mesh)
{
    if (!mesh) return;
    mesh->vert_count = mesh->edge_count = mesh->face_count = 0;
}

void em_mesh_free(edit_mode_mesh* mesh)
{
    if (!mesh) return;
    free(mesh->verts); free(mesh->edges); free(mesh->faces);
    memset(mesh, 0, sizeof(*mesh));
}

/* ------------------------------------------------------------------------- */
/* face uv                                                                   */
/* ------------------------------------------------------------------------- */
/* Put a face's corners into one cell of a cols x rows atlas. Quad corners = (0,0) (1,0) (1,1) (0,1), triangle = (0,0) (1,0) (0,1).
   cols = rows = 1 is the whole 0..1 square. Faces that must not share paint texels need different cells. */
static void em_face_set_cell(edit_mode_face* f, int cx, int cy, int cols, int rows)
{
    static const float q[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    static const float t[3][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 } };
    for (int k = 0; k < f->face_type && k < 4; ++k) {
        const float lu = (f->face_type == 4) ? q[k][0] : t[k][0];
        const float lv = (f->face_type == 4) ? q[k][1] : t[k][1];
        f->uv[k][0] = ((float)cx + lu) / (float)cols;
        f->uv[k][1] = ((float)cy + lv) / (float)rows;
    }
}

/* ------------------------------------------------------------------------- */
/* building                                                                  */
/* ------------------------------------------------------------------------- */
uint32_t em_add_vert(edit_mode_mesh* mesh, editor_vec3 obj_vert_pos, const float color[4])
{
    if (!em_reserve((void**)&mesh->verts, sizeof(edit_mode_vert), &mesh->vert_cap, mesh->vert_count + 1)) return EM_NONE;
    edit_mode_vert* vert_to_edge_array = &mesh->verts[mesh->vert_count];
    memset(vert_to_edge_array, 0, sizeof(*vert_to_edge_array));
    vert_to_edge_array->obj_vert_pos = obj_vert_pos;
    if (color) memcpy(vert_to_edge_array->obj_vert_color, color, sizeof(float) * 4);
    else { vert_to_edge_array->obj_vert_color[0] = vert_to_edge_array->obj_vert_color[1] = vert_to_edge_array->obj_vert_color[2] = 0.8f; vert_to_edge_array->obj_vert_color[3] = 1.0f; }
    return mesh->vert_count++;
}

/* Find the edge_to_face_array a<->b or create it, and register `face` as one of its neighbours.
   Linear search: fine at editor-object scale. Swap for a hash when meshes get big.
   A third face on one edge_to_face_array (non-manifold) is not recorded. */
static uint32_t em_edge_for(edit_mode_mesh* mesh, uint32_t a, uint32_t b, uint32_t face)
{
    for (uint32_t i = 0; i < mesh->edge_count; ++i) {
        edit_mode_edge* e = &mesh->edges[i];
        if ((e->vert_to_edge_array[0] == a && e->vert_to_edge_array[1] == b) || (e->vert_to_edge_array[0] == b && e->vert_to_edge_array[1] == a)) {
            if (e->face_count < 2) e->face[e->face_count++] = face;
            return i;
        }
    }
    if (!em_reserve((void**)&mesh->edges, sizeof(edit_mode_edge), &mesh->edge_cap, mesh->edge_count + 1)) return EM_NONE;
    edit_mode_edge* e = &mesh->edges[mesh->edge_count];
    memset(e, 0, sizeof(*e));
    e->vert_to_edge_array[0] = a; e->vert_to_edge_array[1] = b;
    e->face[0] = e->face[1] = EM_NONE;
    e->face[0] = face;
    e->face_count = 1;
    return mesh->edge_count++;
}

uint32_t em_add_face(edit_mode_mesh* mesh, const uint32_t* verts, int face_type)
{
    if (!mesh || !verts || (face_type != 3 && face_type != 4)) return EM_NONE;
    for (int i = 0; i < face_type; ++i) {
        if (verts[i] >= mesh->vert_count) return EM_NONE;
        for (int j = i + 1; j < face_type; ++j) if (verts[i] == verts[j]) return EM_NONE;
    }
    if (!em_reserve((void**)&mesh->faces, sizeof(edit_mode_face), &mesh->face_cap, mesh->face_count + 1)) return EM_NONE;

    uint32_t fi = mesh->face_count;
    edit_mode_face f;
    memset(&f, 0, sizeof(f));
    f.face_type = (uint8_t)face_type;
    for (int i = 0; i < face_type; ++i) f.vert_to_edge_array[i] = verts[i];
    em_face_set_cell(&f, 0, 0, 1, 1);      /* default: the whole 0..1 square. callers that know better overwrite faces[fi].uv */
    for (int i = 0; i < face_type; ++i) f.edge_to_face_array[i] = em_edge_for(mesh, verts[i], verts[(i + 1) % face_type], fi);

    /* em_edge_for may have realloc'd edges, not faces, so writing here is safe */
    mesh->faces[fi] = f;
    mesh->face_count++;
    return fi;
}

void em_make_cube(edit_mode_mesh* mesh, float h)
{
    em_mesh_clear(mesh);
    const editor_vec3 p[8] = {
        { -h, -h, -h }, {  h, -h, -h }, {  h,  h, -h }, { -h,  h, -h },
        { -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h }
    };
    for (int i = 0; i < 8; ++i) em_add_vert(mesh, p[i], NULL);

    /* outward-facing, counter-clockwise seen from outside */
    static const uint32_t quads[6][4] = {
        { 4, 5, 6, 7 },   /* +Z */
        { 1, 0, 3, 2 },   /* -Z */
        { 5, 1, 2, 6 },   /* +X */
        { 0, 4, 7, 3 },   /* -X */
        { 7, 6, 2, 3 },   /* +Y */
        { 0, 1, 5, 4 }    /* -Y */
    };
    /* 6 faces in a 3 x 3 atlas (3 cells stay empty) so no two faces share texels and every texel stays square */
    for (int i = 0; i < 6; ++i) {
        const uint32_t fi = em_add_face(mesh, quads[i], 4);
        if (fi != EM_NONE) em_face_set_cell(&mesh->faces[fi], i % 3, i / 3, 3, 3);
    }
}

void em_make_grid_sized(edit_mode_mesh* mesh, uint32_t nx, uint32_t nz, float cell)
{
    if (!mesh || nx == 0 || nz == 0) return;
    if (cell <= 0.0f) cell = system_unit_m;
    em_mesh_clear(mesh);
    const uint32_t vc = (nx + 1) * (nz + 1);
    const uint32_t hc = nx * (nz + 1);               /* horizontal edges (i,j)-(i+1,j) */
    const uint32_t ec = hc + (nx + 1) * nz;          /* + vertical edges (i,j)-(i,j+1) */
    const uint32_t fc = nx * nz;
    if (!em_reserve((void**)&mesh->verts, sizeof(edit_mode_vert),  &mesh->vert_cap, vc)) return;
    if (!em_reserve((void**)&mesh->edges, sizeof(edit_mode_edge),  &mesh->edge_cap, ec)) return;
    if (!em_reserve((void**)&mesh->faces, sizeof(edit_mode_face),  &mesh->face_cap, fc)) return;

    for (uint32_t j = 0; j <= nz; ++j)
        for (uint32_t i = 0; i <= nx; ++i) {
            editor_vec3 p = { (float)i * cell, 0.0f, (float)j * cell };
            em_add_vert(mesh, p, NULL);
        }
    for (uint32_t j = 0; j <= nz; ++j)
        for (uint32_t i = 0; i < nx; ++i) {
            edit_mode_edge* e = &mesh->edges[j * nx + i];
            memset(e, 0, sizeof(*e));
            e->vert_to_edge_array[0] = j * (nx + 1) + i; e->vert_to_edge_array[1] = e->vert_to_edge_array[0] + 1;
            e->face[0] = e->face[1] = EM_NONE;
        }
    for (uint32_t j = 0; j < nz; ++j)
        for (uint32_t i = 0; i <= nx; ++i) {
            edit_mode_edge* e = &mesh->edges[hc + j * (nx + 1) + i];
            memset(e, 0, sizeof(*e));
            e->vert_to_edge_array[0] = j * (nx + 1) + i; e->vert_to_edge_array[1] = e->vert_to_edge_array[0] + (nx + 1);
            e->face[0] = e->face[1] = EM_NONE;
        }
    mesh->edge_count = ec;

    for (uint32_t j = 0; j < nz; ++j)
        for (uint32_t i = 0; i < nx; ++i) {
            const uint32_t fi = j * nx + i;
            edit_mode_face* f = &mesh->faces[fi];
            memset(f, 0, sizeof(*f));
            f->face_type = 4;
            em_face_set_cell(f, 0, 0, 1, 1);
            f->vert_to_edge_array[0] = j * (nx + 1) + i;       /* (i,j) (i,j+1) (i+1,j+1) (i+1,j): CCW seen from +Y, same as the cube's +Y face */
            f->vert_to_edge_array[1] = f->vert_to_edge_array[0] + (nx + 1);
            f->vert_to_edge_array[2] = f->vert_to_edge_array[1] + 1;
            f->vert_to_edge_array[3] = f->vert_to_edge_array[0] + 1;
            f->edge_to_face_array[0] = hc + j * (nx + 1) + i;          /* v0->v1 vertical at i     */
            f->edge_to_face_array[1] = (j + 1) * nx + i;               /* v1->v2 horizontal at j+1 */
            f->edge_to_face_array[2] = hc + j * (nx + 1) + i + 1;      /* v2->v3 vertical at i+1   */
            f->edge_to_face_array[3] = j * nx + i;                     /* v3->v0 horizontal at j   */
            for (int k = 0; k < 4; ++k) {
                edit_mode_edge* e = &mesh->edges[f->edge_to_face_array[k]];
                e->face[e->face_count++] = fi;
            }
        }
    mesh->face_count = fc;
}

void em_make_grid(edit_mode_mesh* mesh, uint32_t nx, uint32_t nz)
{
    em_make_grid_sized(mesh, nx, nz, system_unit_m);      /* every quad is 4 x 4 mesh */
}

static editor_vec3 tri_normal(const edit_mode_mesh* mesh, const uint32_t* t)
{
    editor_vec3 a = mesh->verts[t[0]].obj_vert_pos, b = mesh->verts[t[1]].obj_vert_pos, c = mesh->verts[t[2]].obj_vert_pos;
    return norm3(cross3(sub3(b, a), sub3(c, a)));
}

/* Try to fuse triangles A and B (cyclic vertex order, outward CCW) into one quad. */
static bool em_try_merge(const edit_mode_mesh* mesh, const uint32_t* A, const uint32_t* B, uint32_t out[4])
{
    for (int i = 0; i < 3; ++i) {
        uint32_t u = A[i], vert_to_edge_array = A[(i + 1) % 3], x = A[(i + 2) % 3];
        for (int j = 0; j < 3; ++j) {
            if (B[j] == vert_to_edge_array && B[(j + 1) % 3] == u) {
                uint32_t w = B[(j + 2) % 3];
                editor_vec3 nA = tri_normal(mesh, A), nB = tri_normal(mesh, B);
                if (dot3(nA, nB) < 0.9995f) return false;             /* not coplanar */

                /* boundary walk: x -> u -> w -> vert_to_edge_array */
                uint32_t q[4] = { x, u, w, vert_to_edge_array };
                for (int k = 0; k < 4; ++k) {                           /* must be convex */
                    editor_vec3 p0 = mesh->verts[q[k]].obj_vert_pos;
                    editor_vec3 p1 = mesh->verts[q[(k + 1) % 4]].obj_vert_pos;
                    editor_vec3 p2 = mesh->verts[q[(k + 2) % 4]].obj_vert_pos;
                    if (dot3(cross3(sub3(p1, p0), sub3(p2, p1)), nA) <= 1e-9f) return false;
                }
                memcpy(out, q, sizeof(q));
                return true;
            }
        }
    }
    return false;
}

/* uv of corner k of triangle t, read from the SOURCE vertex (welding only merges positions, uv stays per face corner) */
static void em_src_uv(const editor_vertex* verts, uint32_t vc, const uint32_t* idx, uint32_t t, int k, float out[2])
{
    const uint32_t src = idx[t * 3 + k];
    out[0] = (src < vc) ? verts[src].u : 0.0f;
    out[1] = (src < vc) ? verts[src].vert_to_edge_array : 0.0f;
}

/* uv for the 4 corners of a quad made from triangles t and u. false when the two triangles disagree about a shared corner
   (a uv seam), because merging them would stretch the texture. */
static bool em_quad_uv(const uint32_t* tri_t, const uint32_t* tri_u, const editor_vertex* verts, uint32_t vc,
                       const uint32_t* idx, uint32_t t, uint32_t u, const uint32_t quad[4], float out[4][2])
{
    for (int k = 0; k < 4; ++k) {
        float a[2] = { 0, 0 }, b[2] = { 0, 0 };
        bool in_t = false, in_u = false;
        for (int j = 0; j < 3; ++j) if (tri_t[j] == quad[k]) { em_src_uv(verts, vc, idx, t, j, a); in_t = true; break; }
        for (int j = 0; j < 3; ++j) if (tri_u[j] == quad[k]) { em_src_uv(verts, vc, idx, u, j, b); in_u = true; break; }
        if (in_t && in_u && (fabsf(a[0] - b[0]) > 1e-4f || fabsf(a[1] - b[1]) > 1e-4f)) return false;
        const float* s = in_t ? a : b;
        out[k][0] = s[0]; out[k][1] = s[1];
    }
    return true;
}

bool em_build_from_triangles(edit_mode_mesh* mesh, const editor_vertex* verts, uint32_t vc,
                             const uint32_t* idx, uint32_t ic, float eps, bool merge_quads)
{
    if (!mesh || !verts || !idx || vc == 0 || ic < 3) return false;
    em_mesh_clear(mesh);

    uint32_t* remap = (uint32_t*)malloc(sizeof(uint32_t) * vc);
    if (!remap) return false;

    /* weld by position (O(n^2), fine for editor-sized meshes) */
    float e2 = eps * eps;
    for (uint32_t i = 0; i < vc; ++i) {
        editor_vec3 p = { verts[i].x, verts[i].y, verts[i].z };
        uint32_t found = EM_NONE;
        for (uint32_t j = 0; j < mesh->vert_count; ++j) {
            editor_vec3 d = sub3(mesh->verts[j].obj_vert_pos, p);
            if (dot3(d, d) <= e2) { found = j; break; }
        }
        if (found == EM_NONE) {
            float c[4] = { verts[i].r, verts[i].g, verts[i].b, verts[i].a };
            found = em_add_vert(mesh, p, c);
        }
        remap[i] = found;
    }

    uint32_t tc = ic / 3;
    uint32_t* tris = (uint32_t*)malloc(sizeof(uint32_t) * 3 * tc);
    uint8_t*  used = (uint8_t*)calloc(tc, 1);
    if (!tris || !used) { free(remap); free(tris); free(used); return false; }

    for (uint32_t t = 0; t < tc; ++t) {
        for (int k = 0; k < 3; ++k) {
            uint32_t src = idx[t * 3 + k];
            tris[t * 3 + k] = (src < vc) ? remap[src] : 0;
        }
        const uint32_t* a = &tris[t * 3];
        if (a[0] == a[1] || a[1] == a[2] || a[0] == a[2]) used[t] = 1;   /* degenerate after weld */
    }

    for (uint32_t t = 0; t < tc; ++t) {
        if (used[t]) continue;
        used[t] = 1;
        bool merged = false;
        if (merge_quads) {
            for (uint32_t u = t + 1; u < tc; ++u) {
                if (used[u]) continue;
                uint32_t quad[4];
                if (em_try_merge(mesh, &tris[t * 3], &tris[u * 3], quad)) {
                    float quv[4][2];
                    if (!em_quad_uv(&tris[t * 3], &tris[u * 3], verts, vc, idx, t, u, quad, quv)) continue;   /* uv seam: keep them apart */
                    const uint32_t qf = em_add_face(mesh, quad, 4);
                    if (qf != EM_NONE) memcpy(mesh->faces[qf].uv, quv, sizeof(quv));
                    used[u] = 1;
                    merged = true;
                    break;
                }
            }
        }
        if (!merged) {
            const uint32_t tf = em_add_face(mesh, &tris[t * 3], 3);
            if (tf != EM_NONE) for (int k = 0; k < 3; ++k) em_src_uv(verts, vc, idx, t, k, mesh->faces[tf].uv[k]);
        }
    }

    free(remap); free(tris); free(used);
    return mesh->face_count > 0;
}

bool em_triangulate(const edit_mode_mesh* mesh, editor_vertex** out_v, uint32_t* out_vc, uint32_t** out_i, uint32_t* out_ic)
{
    if (!mesh || !out_v || !out_vc || !out_i || !out_ic || mesh->vert_count == 0 || mesh->face_count == 0) return false;

    /* every face owns its verts: a quad = 4 verts + 6 indices, a triangle = 3 verts + 3 indices */
    uint32_t vc = 0, ic = 0;
    for (uint32_t f = 0; f < mesh->face_count; ++f) {
        vc += (uint32_t)mesh->faces[f].face_type;
        ic += (uint32_t)(mesh->faces[f].face_type - 2) * 3;
    }

    editor_vertex* vert_to_edge_array = (editor_vertex*)malloc(sizeof(editor_vertex) * vc);
    uint32_t*      i = (uint32_t*)malloc(sizeof(uint32_t) * ic);
    if (!vert_to_edge_array || !i) { free(vert_to_edge_array); free(i); return false; }

    uint32_t wv = 0, wi = 0;
    for (uint32_t f = 0; f < mesh->face_count; ++f) {
        const edit_mode_face* F = &mesh->faces[f];
        const uint32_t base = wv;

        const editor_vec3 fn = edit_face_normal(mesh, F);

        for (int k = 0; k < F->face_type; ++k) {
            const edit_mode_vert* s = &mesh->verts[F->vert_to_edge_array[k]];
            editor_vertex* o = &vert_to_edge_array[wv++];
            memset(o, 0, sizeof(*o));
            o->x = s->obj_vert_pos.x; o->y = s->obj_vert_pos.y; o->z = s->obj_vert_pos.z;
            o->r = s->obj_vert_color[0]; o->g = s->obj_vert_color[1]; o->b = s->obj_vert_color[2]; o->a = s->obj_vert_color[3];
            set_vertex_attributes(o, F->uv[k][0], F->uv[k][1], fn.x, fn.y, fn.z);
        }
        for (int t = 1; t + 1 < F->face_type; ++t) {            /* fan keeps winding */
            i[wi++] = base; i[wi++] = base + (uint32_t)t; i[wi++] = base + (uint32_t)t + 1u;
        }
    }

    *out_v = vert_to_edge_array; *out_vc = vc; *out_i = i; *out_ic = ic;
    return true;
}

/* ------------------------------------------------------------------------- */
/* selection                                                                 */
/* ------------------------------------------------------------------------- */
void em_clear_active(edit_mode_mesh* mesh)
{
    if (!mesh) return;
    for (uint32_t i = 0; i < mesh->vert_count; ++i) mesh->verts[i].obj_vert_active = 0;
    for (uint32_t i = 0; i < mesh->edge_count; ++i) mesh->edges[i].obj_vert_active = 0;
    for (uint32_t i = 0; i < mesh->face_count; ++i) mesh->faces[i].obj_vert_active = 0;
}

void em_set_active_element(edit_mode_mesh* mesh, enum_mesh_selection kind, uint32_t index)
{
    if (!mesh) return;
    em_clear_active(mesh);
    switch (kind) {
    case select_vert: if (index < mesh->vert_count) mesh->verts[index].obj_vert_active = 1; break;
    case select_edge: if (index < mesh->edge_count) mesh->edges[index].obj_vert_active = 1; break;
    case select_face: if (index < mesh->face_count) mesh->faces[index].obj_vert_active = 1; break;
    }
}

void em_clear_selection(edit_mode_mesh* mesh)
{
    if (!mesh) return;
    em_clear_active(mesh);
    for (uint32_t i = 0; i < mesh->vert_count; ++i) mesh->verts[i].obj_vert_selected = 0;
    for (uint32_t i = 0; i < mesh->edge_count; ++i) mesh->edges[i].obj_vert_selected = 0;
    for (uint32_t i = 0; i < mesh->face_count; ++i) mesh->faces[i].obj_vert_selected = 0;
}

void em_sync_selection(edit_mode_mesh* mesh, enum_mesh_selection basis)
{
    if (!mesh) return;
    switch (basis) {
    case select_vert:
        for (uint32_t i = 0; i < mesh->edge_count; ++i) {
            edit_mode_edge* e = &mesh->edges[i];
            e->obj_vert_selected = mesh->verts[e->vert_to_edge_array[0]].obj_vert_selected && mesh->verts[e->vert_to_edge_array[1]].obj_vert_selected;
        }
        for (uint32_t i = 0; i < mesh->face_count; ++i) {
            edit_mode_face* f = &mesh->faces[i];
            uint8_t all = 1;
            for (int k = 0; k < f->face_type; ++k) all &= mesh->verts[f->vert_to_edge_array[k]].obj_vert_selected;
            f->obj_vert_selected = all;
        }
        break;

    case select_edge:
        for (uint32_t i = 0; i < mesh->vert_count; ++i) mesh->verts[i].obj_vert_selected = 0;
        for (uint32_t i = 0; i < mesh->edge_count; ++i) {
            edit_mode_edge* e = &mesh->edges[i];
            if (e->obj_vert_selected) { mesh->verts[e->vert_to_edge_array[0]].obj_vert_selected = 1; mesh->verts[e->vert_to_edge_array[1]].obj_vert_selected = 1; }
        }
        for (uint32_t i = 0; i < mesh->face_count; ++i) {
            edit_mode_face* f = &mesh->faces[i];
            uint8_t all = 1;
            for (int k = 0; k < f->face_type; ++k) all &= mesh->edges[f->edge_to_face_array[k]].obj_vert_selected;
            f->obj_vert_selected = all;
        }
        break;

    case select_face:
        for (uint32_t i = 0; i < mesh->vert_count; ++i) mesh->verts[i].obj_vert_selected = 0;
        for (uint32_t i = 0; i < mesh->edge_count; ++i) mesh->edges[i].obj_vert_selected = 0;
        for (uint32_t i = 0; i < mesh->face_count; ++i) {
            edit_mode_face* f = &mesh->faces[i];
            if (!f->obj_vert_selected) continue;
            for (int k = 0; k < f->face_type; ++k) {
                mesh->verts[f->vert_to_edge_array[k]].obj_vert_selected = 1;
                mesh->edges[f->edge_to_face_array[k]].obj_vert_selected = 1;
            }
        }
        break;
    }
}

void em_select_element(edit_mode_mesh* mesh, enum_mesh_selection mode, uint32_t index, bool additive)
{
    if (!mesh) return;
    uint8_t* flag = NULL;
    switch (mode) {
    case select_vert: if (index < mesh->vert_count) flag = &mesh->verts[index].obj_vert_selected; break;
    case select_edge: if (index < mesh->edge_count) flag = &mesh->edges[index].obj_vert_selected; break;
    case select_face: if (index < mesh->face_count) flag = &mesh->faces[index].obj_vert_selected; break;
    }
    uint8_t was = flag ? *flag : 0;
    if (!additive) em_clear_selection(mesh);
    if (flag) *flag = additive ? (uint8_t)!was : 1;
    if (flag && *flag) em_set_active_element(mesh, mode, index);   /* last clicked = active */
    else if (flag) em_clear_active(mesh);                           /* toggled off */
    em_sync_selection(mesh, mode);
}

uint32_t em_selection_centroid(const edit_mode_mesh* mesh, editor_vec3* out)
{
    editor_vec3 s = { 0, 0, 0 };
    uint32_t n = 0;
    if (mesh) {
        for (uint32_t i = 0; i < mesh->vert_count; ++i) {
            if (!mesh->verts[i].obj_vert_selected) continue;
            s.x += mesh->verts[i].obj_vert_pos.x; s.y += mesh->verts[i].obj_vert_pos.y; s.z += mesh->verts[i].obj_vert_pos.z;
            ++n;
        }
    }
    if (n) { s.x /= n; s.y /= n; s.z /= n; }
    if (out) *out = s;
    return n;
}

void em_translate_selected(edit_mode_mesh* mesh, editor_vec3 d)
{
    if (!mesh) return;
    for (uint32_t i = 0; i < mesh->vert_count; ++i) {
        if (!mesh->verts[i].obj_vert_selected) continue;
        mesh->verts[i].obj_vert_pos.x += d.x; mesh->verts[i].obj_vert_pos.y += d.y; mesh->verts[i].obj_vert_pos.z += d.z;
    }
}

/* ------------------------------------------------------------------------- */
/* registry                                                                  */
/* ------------------------------------------------------------------------- */
typedef struct { uint32_t id; bool used; edit_mode_mesh mesh; } em_slot;
static em_slot s_slots[max_scene_objects];

static edit_mode_mesh* em_slot_find(uint32_t id)
{
    for (int i = 0; i < max_scene_objects; ++i)
        if (s_slots[i].used && s_slots[i].id == id) return &s_slots[i].mesh;
    return NULL;
}

/* Public lookup. Terrain / region_xx have no slot: their edit mesh is the terrain's control grid, so the
   transform gizmo (which asks this function) finds the same mesh Edit Mode is drawing. */
edit_mode_mesh* em_registry_get(uint32_t id)
{
    edit_mode_mesh* mesh = em_slot_find(id);
    if (mesh) return mesh;
    if (g_terrain.exists && is_terrain_object(find_scene_object_by_id(id))) return terrain_edit_mesh_current();
    return NULL;
}

static em_slot* em_registry_claim(uint32_t id)
{
    edit_mode_mesh* existing = em_slot_find(id);
    if (existing) return (em_slot*)((char*)existing - offsetof(em_slot, mesh));
    for (int i = 0; i < max_scene_objects; ++i) {
        if (!s_slots[i].used) {
            memset(&s_slots[i], 0, sizeof(s_slots[i]));
            s_slots[i].used = true;
            s_slots[i].id = id;
            return &s_slots[i];
        }
    }
    return NULL;
}

bool em_registry_adopt_triangles(uint32_t id, const editor_vertex* vert_to_edge_array, uint32_t vc, const uint32_t* idx, uint32_t ic)
{
    em_slot* s = em_registry_claim(id);
    if (!s) return false;
    return em_build_from_triangles(&s->mesh, vert_to_edge_array, vc, idx, ic, 1e-4f, true);
}

edit_mode_mesh* em_registry_get_or_create(system_scene_object* obj)
{
    if (!obj) return NULL;
    if (is_terrain_object(obj)) {              /* must come first: Edit Mode then calls terrain_edit_mesh_acquire() */
        static unsigned n = 0;
        if (n++ < 10) phoenix_info("EditMesh", "em_registry_get_or_create('%s' id %u): terrain object -> NULL (use terrain_edit_mesh_acquire)", obj->name, (unsigned)obj->id);
        return NULL;
    }
    edit_mode_mesh* mesh = em_slot_find(obj->id);
    if (mesh) return mesh;
    if (obj->type != scene_object_mesh) return NULL;   /* Terrain + region_xx are edited through terrain_edit_mesh_acquire() */

    em_slot* s = em_registry_claim(obj->id);
    if (!s) return NULL;

    /* Preferred: the CPU copy kept in the object database (written at spawn and after every edit commit). */
    {
        const void* dv = NULL; const uint32_t* di = NULL;
        uint32_t dvs = 0, dvc = 0, dic = 0;
        if (get_object_mesh_data(obj->id, &dv, &dvs, &dvc, &di, &dic) && dvs == sizeof(editor_vertex)) {
            if (em_build_from_triangles(&s->mesh, (const editor_vertex*)dv, dvc, di, dic, 1e-4f, true)) return &s->mesh;
            em_mesh_clear(&s->mesh);
        }
    }

    /* Fallback for objects that never stored a mesh: a one cell cube (system_unit_m on every side). */
    em_make_cube(&s->mesh, system_unit_m * 0.5f);
    return &s->mesh;
}

void em_registry_remove(uint32_t id)
{
    object_db_remove_object(id);     /* the object is gone: drop its database blocks too */
    paint_texture_remove(id);        /* and its paint texture */
    for (int i = 0; i < max_scene_objects; ++i) {
        if (s_slots[i].used && s_slots[i].id == id) {
            em_mesh_free(&s_slots[i].mesh);
            s_slots[i].used = false;
            return;
        }
    }
}

void em_registry_shutdown(void)
{
    em_gpu_flush();
    for (int i = 0; i < max_scene_objects; ++i) {
        if (s_slots[i].used) em_mesh_free(&s_slots[i].mesh);
        s_slots[i].used = false;
    }
}

/* ------------------------------------------------------------------------- */
/* GPU upload (old buffers are retired a few frames later, never mid-frame)  */
/* ------------------------------------------------------------------------- */
#define EM_RETIRE_MAX     64
#define EM_RETIRE_FRAMES  3      /* >= frames in flight + 1 */

typedef struct { uint64_t vb, vm, ib, im; int frames; bool used; } em_retired;
static em_retired s_retired[EM_RETIRE_MAX];

static void em_destroy_retired(em_retired* r)
{
    VkDevice dev = render_context.logical_device;
    if (r->vb) vkDestroyBuffer(dev, (VkBuffer)(uintptr_t)r->vb, NULL);
    if (r->vm) vkFreeMemory(dev, (VkDeviceMemory)(uintptr_t)r->vm, NULL);
    if (r->ib) vkDestroyBuffer(dev, (VkBuffer)(uintptr_t)r->ib, NULL);
    if (r->im) vkFreeMemory(dev, (VkDeviceMemory)(uintptr_t)r->im, NULL);
    memset(r, 0, sizeof(*r));
}

/* Call ONCE per frame (system_edit_mode_update does it). */
void em_gpu_tick(void)
{
    for (int i = 0; i < EM_RETIRE_MAX; ++i)
        if (s_retired[i].used && --s_retired[i].frames <= 0) em_destroy_retired(&s_retired[i]);
    skin_gpu_tick();   /* old skin buffers + descriptor sets use the same 3 frame delay */
}

/* Destroy everything now. Call before the Vulkan device is destroyed. */
void em_gpu_flush(void)
{
    if (!render_context.logical_device) return;
    vkDeviceWaitIdle(render_context.logical_device);
    for (int i = 0; i < EM_RETIRE_MAX; ++i)
        if (s_retired[i].used) em_destroy_retired(&s_retired[i]);
    skin_gpu_flush();
}

void em_gpu_retire(uint64_t vb, uint64_t vm, uint64_t ib, uint64_t im)
{
    if (!(vb || vm || ib || im)) return;
    int slot = -1;
    for (int i = 0; i < EM_RETIRE_MAX; ++i) if (!s_retired[i].used) { slot = i; break; }
    if (slot < 0) { em_gpu_flush(); slot = 0; }      /* queue full: stall once */
    s_retired[slot].vb = vb; s_retired[slot].vm = vm;
    s_retired[slot].ib = ib; s_retired[slot].im = im;
    s_retired[slot].frames = EM_RETIRE_FRAMES;
    s_retired[slot].used = true;
}

bool em_commit_to_gpu(edit_mode_mesh* mesh, system_scene_object* obj)
{
    if (!mesh || !obj) { phoenix_error("EditMesh", "em_commit_to_gpu: null mesh or object (mesh=%p obj=%p)", (void*)mesh, (void*)obj); return false; }
    if (is_terrain_object(obj)) {
        phoenix_info("EditMesh", "em_commit_to_gpu('%s'): terrain -> heights written back", obj->name);
        return terrain_commit_edit_mesh(mesh, obj);   /* heights go back into the region blocks */
    }

    editor_vertex* vert_to_edge_array = NULL; uint32_t vc = 0;
    uint32_t* idx = NULL;    uint32_t ic = 0;
    if (!em_triangulate(mesh, &vert_to_edge_array, &vc, &idx, &ic)) { phoenix_error("EditMesh", "em_commit_to_gpu('%s'): triangulate failed (%u verts %u faces)", obj->name, (unsigned)mesh->vert_count, (unsigned)mesh->face_count); return false; }

#if EM_FREE_OLD_BUFFERS
    uint64_t ovb = obj->vertex_buffer, ovm = obj->vertex_memory;
    uint64_t oib = obj->index_buffer,  oim = obj->index_memory;
#endif

    bool ok = upload_scene_object_mesh(obj, render_context.logical_device, render_context.physical_device, vert_to_edge_array, vc, idx, ic);
    {
        static unsigned n = 0;
        n++;
        if (!ok || n <= 30 || (n % 30) == 0)
            phoenix_info("EditMesh", "em_commit_to_gpu #%u '%s' (id %u): %u verts %u indices -> %s", n, obj->name, (unsigned)obj->id, (unsigned)vc, (unsigned)ic, ok ? "ok" : "UPLOAD FAILED");
    }
    if (ok) set_object_mesh_data(obj->id, vert_to_edge_array, (uint32_t)sizeof(editor_vertex), vc, idx, ic);   /* keep the database copy in step */
    free(vert_to_edge_array);
    free(idx);

#if EM_FREE_OLD_BUFFERS
    if (ok) em_gpu_retire(ovb, ovm, oib, oim);
#endif
    return ok;
}

void em_selection_counts(const edit_mode_mesh* mesh, uint32_t* vert_to_edge_array, uint32_t* e, uint32_t* f)
{
    uint32_t a = 0, b = 0, c = 0;
    if (mesh) {
        for (uint32_t i = 0; i < mesh->vert_count; ++i) a += mesh->verts[i].obj_vert_selected;
        for (uint32_t i = 0; i < mesh->edge_count; ++i) b += mesh->edges[i].obj_vert_selected;
        for (uint32_t i = 0; i < mesh->face_count; ++i) c += mesh->faces[i].obj_vert_selected;
    }
    if (vert_to_edge_array) *vert_to_edge_array = a;
    if (e) *e = b;
    if (f) *f = c;
}