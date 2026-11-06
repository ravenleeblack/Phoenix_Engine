#include "system_globals.h"

sculpt_state g_sculpt;
static bool  s_started = false;

/* ========================================================================= */
/* small vector helpers (object space)                                       */
/* ========================================================================= */
static inline editor_vec3 v3(float x, float y, float z)            { editor_vec3 r = { x, y, z }; return r; }
static inline editor_vec3 v3_add(editor_vec3 a, editor_vec3 b)     { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline editor_vec3 v3_sub(editor_vec3 a, editor_vec3 b)     { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline editor_vec3 v3_scale(editor_vec3 a, float s)         { return v3(a.x * s, a.y * s, a.z * s); }
static inline float       v3_dot(editor_vec3 a, editor_vec3 b)     { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float       v3_len(editor_vec3 a)                     { return sqrtf(v3_dot(a, a)); }
static inline editor_vec3 v3_lerp(editor_vec3 a, editor_vec3 b, float t) { return v3_add(a, v3_scale(v3_sub(b, a), t)); }
static inline editor_vec3 v3_norm(editor_vec3 a)
{
    const float l = v3_len(a);
    return l > 1e-8f ? v3_scale(a, 1.0f / l) : v3(0.0f, 0.0f, 0.0f);
}

/* ========================================================================= */
/* brush table                                                               */
/* ========================================================================= */
static const char* const k_brush_names[sculpt_brush_count] = {
    "Clay Buildup", "Clay Soften", "Standard", "Carve", "Grab", "Polish", "Inflate", "Smooth", "Flatten"
};

#define M_R   sculpt_setting_radius
#define M_S   sculpt_setting_strength
#define M_H   sculpt_setting_hardness
#define M_P   sculpt_setting_plane_offset
#define M_N   sculpt_setting_smooth_passes
#define M_SP  sculpt_setting_spacing
static const uint32_t k_brush_masks[sculpt_brush_count] = {
    M_R | M_S | M_H | M_P | M_SP,        /* clay buildup */
    M_R | M_S | M_H | M_N | M_SP,        /* clay soften  */
    M_R | M_S | M_H | M_SP,              /* standard     */
    M_R | M_S | M_H | M_SP,              /* carve        */
    M_R | M_H,                           /* grab         */
    M_R | M_S | M_H | M_N | M_SP,        /* polish       */
    M_R | M_S | M_H | M_SP,              /* inflate      */
    M_R | M_S | M_H | M_N | M_SP,        /* smooth       */
    M_R | M_S | M_H | M_P | M_SP         /* flatten      */
};
#undef M_R
#undef M_S
#undef M_H
#undef M_P
#undef M_N
#undef M_SP

const char* sculpt_brush_name(enum_sculpt_brush b)
{
    return ((int)b >= 0 && b < sculpt_brush_count) ? k_brush_names[b] : "?";
}

uint32_t sculpt_brush_setting_mask(enum_sculpt_brush b)
{
    return ((int)b >= 0 && b < sculpt_brush_count) ? k_brush_masks[b] : 0u;
}

void sculpt_clamp_settings(sculpt_brush_settings* s)
{
    if (!s) return;
    s->radius        = clampf(s->radius,       0.02f, 400.0f);   /* terrain brushes need big radii */
    s->strength      = clampf(s->strength,     0.0f,  1.0f);
    s->hardness      = clampf(s->hardness,     0.0f,  0.99f);
    s->plane_offset  = clampf(s->plane_offset, -1.0f, 1.0f);
    s->spacing       = clampf(s->spacing,      0.02f, 1.0f);
    if (s->smooth_passes < 1) s->smooth_passes = 1;
    if (s->smooth_passes > 8) s->smooth_passes = 8;
}

/* ========================================================================= */
/* state                                                                     */
/* ========================================================================= */
void start_sculpt_mode(void)
{
    if (s_started) return;
    s_started = true;

    for (int i = 0; i < sculpt_brush_count; i++) {
        sculpt_brush_settings* s = &g_sculpt.settings[i];
        s->radius = 2.0f;  s->strength = 0.5f;  s->hardness = 0.35f;
        s->plane_offset = 0.0f;  s->smooth_passes = 1;  s->spacing = 0.10f;
    }
    g_sculpt.settings[sculpt_brush_carve].strength     = 0.6f;
    g_sculpt.settings[sculpt_brush_carve].hardness     = 0.15f;
    g_sculpt.settings[sculpt_brush_clay_buildup].strength = 0.4f;
    g_sculpt.settings[sculpt_brush_smooth].strength    = 0.6f;
    g_sculpt.settings[sculpt_brush_polish].strength    = 0.6f;
    g_sculpt.settings[sculpt_brush_flatten].strength   = 0.7f;
    g_sculpt.settings[sculpt_brush_grab].radius        = 3.0f;
    g_sculpt.settings[sculpt_brush_grab].hardness      = 0.0f;
    g_sculpt.settings[sculpt_brush_grab].spacing       = 0.02f;

    g_sculpt.brush            = sculpt_brush_standard;
    g_sculpt.smooth_subdivide = true;
    g_sculpt.nav              = sculpt_nav_maya;
}

void sculpt_mode_set_active(bool on)  { start_sculpt_mode(); g_sculpt.active = on; }
bool sculpt_mode_is_active(void)      { return g_sculpt.active && mode_state_is_sculpt(); }

void sculpt_set_brush(enum_sculpt_brush b)
{
    start_sculpt_mode();
    if ((int)b >= 0 && b < sculpt_brush_count) g_sculpt.brush = b;
}
enum_sculpt_brush sculpt_get_brush(void)        { start_sculpt_mode(); return g_sculpt.brush; }
sculpt_brush_settings* sculpt_current_settings(void) { start_sculpt_mode(); return &g_sculpt.settings[g_sculpt.brush]; }

/* ========================================================================= */
/* math                                                                      */
/* ========================================================================= */
float sculpt_falloff(float dist, float radius, float hardness)
{
    if (radius <= 0.0f || dist >= radius) return 0.0f;
    const float t = dist / radius;
    const float h = clampf(hardness, 0.0f, 0.99f);
    if (t <= h) return 1.0f;
    const float u = (t - h) / (1.0f - h);
    return 1.0f - u * u * (3.0f - 2.0f * u);          /* smoothstep down to the rim */
}

/* scratch buffers, grown on demand and reused: no allocation per dab once they are big enough */
static editor_vec3* s_nrm = NULL;  static uint32_t s_nrm_cap = 0;
static editor_vec3* s_avg = NULL;  static uint32_t s_avg_cap = 0;
static uint32_t*    s_cnt = NULL;  static uint32_t s_cnt_cap = 0;

static bool ensure_scratch(void** buf, uint32_t* cap, uint32_t need, size_t elem)
{
    if (need <= *cap) return true;
    uint32_t n = *cap ? *cap : 256u;
    while (n < need) n *= 2u;
    void* p = realloc(*buf, (size_t)n * elem);
    if (!p) return false;
    *buf = p;  *cap = n;
    return true;
}

void sculpt_compute_vertex_normals(const edit_mode_mesh* mesh, editor_vec3* out)
{
    if (!mesh || !out) return;
    const uint32_t nv = mesh->vert_count;
    for (uint32_t i = 0; i < nv; i++) out[i] = v3(0.0f, 0.0f, 0.0f);

    for (uint32_t f = 0; f < mesh->face_count; f++) {
        const edit_mode_face* face = &mesh->faces[f];
        const int n = face->face_type;
        editor_vec3 nrm = v3(0.0f, 0.0f, 0.0f);
        for (int i = 0; i < n; i++) {                        /* Newell's method: works for tris and quads */
            const uint32_t a = face->vert_to_edge_array[i], b = face->vert_to_edge_array[(i + 1) % n];
            if (a >= nv || b >= nv) { nrm = v3(0, 0, 0); break; }
            const editor_vec3 p = mesh->verts[a].obj_vert_pos, q = mesh->verts[b].obj_vert_pos;
            nrm.x += (p.y - q.y) * (p.z + q.z);
            nrm.y += (p.z - q.z) * (p.x + q.x);
            nrm.z += (p.x - q.x) * (p.y + q.y);
        }
        for (int i = 0; i < n; i++) if (face->vert_to_edge_array[i] < nv) out[face->vert_to_edge_array[i]] = v3_add(out[face->vert_to_edge_array[i]], nrm);   /* area weighted */
    }
    for (uint32_t i = 0; i < nv; i++) out[i] = v3_norm(out[i]);
}

/* s_avg[i] = average position of the neighbours of vertex i (its own position when it has none) */
static bool compute_neighbor_average(const edit_mode_mesh* mesh)
{
    const uint32_t nv = mesh->vert_count;
    if (!ensure_scratch((void**)&s_avg, &s_avg_cap, nv, sizeof(editor_vec3))) return false;
    if (!ensure_scratch((void**)&s_cnt, &s_cnt_cap, nv, sizeof(uint32_t)))   return false;
    memset(s_avg, 0, sizeof(editor_vec3) * nv);
    memset(s_cnt, 0, sizeof(uint32_t) * nv);

    for (uint32_t e = 0; e < mesh->edge_count; e++) {
        const uint32_t a = mesh->edges[e].vert_to_edge_array[0], b = mesh->edges[e].vert_to_edge_array[1];
        if (a >= nv || b >= nv) continue;
        s_avg[a] = v3_add(s_avg[a], mesh->verts[b].obj_vert_pos);  s_cnt[a]++;
        s_avg[b] = v3_add(s_avg[b], mesh->verts[a].obj_vert_pos);  s_cnt[b]++;
    }
    for (uint32_t i = 0; i < nv; i++)
        s_avg[i] = s_cnt[i] ? v3_scale(s_avg[i], 1.0f / (float)s_cnt[i]) : mesh->verts[i].obj_vert_pos;
    return true;
}

/* the plane the brush works on: weighted average position + weighted average normal under the brush */
static bool area_plane(const edit_mode_mesh* mesh, const editor_vec3* nrm, editor_vec3 c, float radius, float hardness,
                       editor_vec3 hit_n, editor_vec3* out_n, editor_vec3* out_p)
{
    float sum_w = 0.0f;
    editor_vec3 sn = v3(0, 0, 0), sp = v3(0, 0, 0);
    const float r2 = radius * radius;

    for (uint32_t i = 0; i < mesh->vert_count; i++) {
        const editor_vec3 d = v3_sub(mesh->verts[i].obj_vert_pos, c);
        const float d2 = v3_dot(d, d);
        if (d2 >= r2) continue;
        const float w = sculpt_falloff(sqrtf(d2), radius, hardness);
        if (w <= 0.0f) continue;
        sum_w += w;
        sn = v3_add(sn, v3_scale(nrm[i], w));
        sp = v3_add(sp, v3_scale(mesh->verts[i].obj_vert_pos, w));
    }
    if (sum_w <= 1e-6f) return false;

    *out_p = v3_scale(sp, 1.0f / sum_w);
    *out_n = v3_norm(sn);
    if (v3_len(*out_n) < 1e-6f) *out_n = v3_norm(hit_n);
    return true;
}

/* one relax pass: pull toward the neighbour average (k_smooth) and/or onto the work plane (k_flat) */
static void relax_pass(edit_mode_mesh* mesh, editor_vec3 c, float radius, float hardness, float k_smooth, float k_flat,
                       float plane_off, editor_vec3 pn, editor_vec3 pp)
{
    if (!compute_neighbor_average(mesh)) return;
    const editor_vec3 plane_pt = v3_add(pp, v3_scale(pn, plane_off));
    const float r2 = radius * radius;

    for (uint32_t i = 0; i < mesh->vert_count; i++) {
        editor_vec3 p = mesh->verts[i].obj_vert_pos;
        const editor_vec3 d = v3_sub(p, c);
        const float d2 = v3_dot(d, d);
        if (d2 >= r2) continue;
        const float w = sculpt_falloff(sqrtf(d2), radius, hardness);
        if (w <= 0.0f) continue;

        if (k_smooth > 0.0f) p = v3_lerp(p, s_avg[i], clampf(k_smooth * w, 0.0f, 1.0f));
        if (k_flat > 0.0f) {
            const float dist = v3_dot(v3_sub(p, plane_pt), pn);
            p = v3_sub(p, v3_scale(pn, dist * clampf(k_flat * w, 0.0f, 1.0f)));
        }
        mesh->verts[i].obj_vert_pos = p;
    }
}

/* ========================================================================= */
/* strokes                                                                   */
/* ========================================================================= */
typedef struct { uint32_t vert; float w; uint8_t mirror; editor_vec3 start; } grab_item;

static struct {
    bool       active;
    grab_item* items;
    uint32_t   face_type, cap;
} s_stroke = { 0 };

/* mirror masks that are switched on: bit0 = flip X, bit1 = flip Y, bit2 = flip Z. mask 0 (no flip) is always first. */
static int active_mirrors(uint8_t out[8])
{
    int n = 0;
    out[n++] = 0;
    for (uint8_t mask = 1; mask < 8; mask++) {
        if ((mask & 1) && !g_sculpt.sym_x) continue;
        if ((mask & 2) && !g_sculpt.sym_y) continue;
        if ((mask & 4) && !g_sculpt.sym_z) continue;
        out[n++] = mask;
    }
    return n;
}

static editor_vec3 mirror_vec(editor_vec3 vert_to_edge_array, uint8_t mask)
{
    if (mask & 1) vert_to_edge_array.x = -vert_to_edge_array.x;
    if (mask & 2) vert_to_edge_array.y = -vert_to_edge_array.y;
    if (mask & 4) vert_to_edge_array.z = -vert_to_edge_array.z;
    return vert_to_edge_array;
}

static bool stroke_push(grab_item it)
{
    if (s_stroke.face_type == s_stroke.cap) {
        const uint32_t n = s_stroke.cap ? s_stroke.cap * 2u : 1024u;
        grab_item* p = (grab_item*)realloc(s_stroke.items, sizeof(grab_item) * n);
        if (!p) return false;
        s_stroke.items = p;  s_stroke.cap = n;
    }
    s_stroke.items[s_stroke.face_type++] = it;
    return true;
}

void sculpt_stroke_begin(edit_mode_mesh* mesh, editor_vec3 center, editor_vec3 hit_normal)
{
    (void)hit_normal;
    start_sculpt_mode();
    s_stroke.active = true;
    s_stroke.face_type  = 0;
    if (!mesh || g_sculpt.brush != sculpt_brush_grab) return;

    /* Grab: remember which vertices the brush caught and how strongly, so the whole stroke moves ONE set of vertices */
    const sculpt_brush_settings* s = &g_sculpt.settings[sculpt_brush_grab];
    uint8_t* claimed = (uint8_t*)calloc(mesh->vert_count ? mesh->vert_count : 1u, 1);
    if (!claimed) return;

    uint8_t masks[8];
    const int mc = active_mirrors(masks);
    for (int k = 0; k < mc; k++) {
        const editor_vec3 c = mirror_vec(center, masks[k]);
        for (uint32_t i = 0; i < mesh->vert_count; i++) {
            if (claimed[i]) continue;
            const float w = sculpt_falloff(v3_len(v3_sub(mesh->verts[i].obj_vert_pos, c)), s->radius, s->hardness);
            if (w <= 0.0f) continue;
            claimed[i] = 1;
            grab_item it = { i, w, masks[k], mesh->verts[i].obj_vert_pos };
            if (!stroke_push(it)) break;
        }
    }
    free(claimed);
}

void sculpt_stroke_end(void)
{
    s_stroke.active = false;
    s_stroke.face_type  = 0;
    free(s_stroke.items);
    s_stroke.items = NULL;
    s_stroke.cap   = 0;
}

static void apply_brush_at(edit_mode_mesh* mesh, const editor_vec3* nrm, enum_sculpt_brush b, const sculpt_brush_settings* s,
                           editor_vec3 c, editor_vec3 hit_n, float dir)
{
    editor_vec3 pn, pp;
    if (!area_plane(mesh, nrm, c, s->radius, s->hardness, hit_n, &pn, &pp)) return;

    const float amount = s->strength * s->radius * 0.25f;     /* world units moved at full weight per dab */
    const float off    = s->plane_offset * s->radius * 0.2f;
    const int   passes = s->smooth_passes < 1 ? 1 : (s->smooth_passes > 8 ? 8 : s->smooth_passes);

    /* relax family: Smooth = all neighbour average. Polish = mostly flatten. Clay Soften = mostly smooth, a little flatten. */
    if (b == sculpt_brush_smooth || b == sculpt_brush_polish || b == sculpt_brush_clay_soften) {
        float k_smooth = s->strength, k_flat = 0.0f;
        if (b == sculpt_brush_polish)      { k_smooth = s->strength * 0.3f; k_flat = s->strength * 0.7f; }
        if (b == sculpt_brush_clay_soften) { k_smooth = s->strength * 0.7f; k_flat = s->strength * 0.3f; }
        for (int p = 0; p < passes; p++) relax_pass(mesh, c, s->radius, s->hardness, k_smooth, k_flat, 0.0f, pn, pp);
        return;
    }

    const float r2 = s->radius * s->radius;
    for (uint32_t i = 0; i < mesh->vert_count; i++) {
        editor_vec3 p = mesh->verts[i].obj_vert_pos;
        const editor_vec3 d = v3_sub(p, c);
        const float d2 = v3_dot(d, d);
        if (d2 >= r2) continue;
        float w = sculpt_falloff(sqrtf(d2), s->radius, s->hardness);
        if (w <= 0.0f) continue;

        switch (b) {
        case sculpt_brush_standard:                                   /* push along the average normal */
            p = v3_add(p, v3_scale(pn, amount * w * dir));
            break;
        case sculpt_brush_carve:                                      /* cut in, sharper edge_to_face_array than Standard */
            p = v3_sub(p, v3_scale(pn, amount * w * w * dir));
            break;
        case sculpt_brush_inflate:                                    /* along each vertex's own normal */
            p = v3_add(p, v3_scale(nrm[i], amount * w * dir));
            break;
        case sculpt_brush_clay_buildup: {                             /* raise toward a plane above the surface, never dig */
            const float h     = s->strength * s->radius * 0.15f * dir;
            const float delta = v3_dot(v3_sub(v3_add(pp, v3_scale(pn, h + off)), p), pn);
            if ((dir > 0.0f && delta > 0.0f) || (dir < 0.0f && delta < 0.0f))
                p = v3_add(p, v3_scale(pn, delta * w * 0.5f));
            break;
        }
        case sculpt_brush_flatten: {                                  /* everything under the brush onto the work plane */
            const float dist = v3_dot(v3_sub(p, v3_add(pp, v3_scale(pn, off))), pn);
            p = v3_sub(p, v3_scale(pn, dist * clampf(s->strength * w, 0.0f, 1.0f)));
            break;
        }
        default: break;
        }
        mesh->verts[i].obj_vert_pos = p;
    }
}

bool sculpt_stroke_update(edit_mode_mesh* mesh, editor_vec3 center, editor_vec3 hit_normal, editor_vec3 drag_delta, bool invert_now)
{
    start_sculpt_mode();
    if (!mesh || mesh->vert_count == 0) return false;

    const enum_sculpt_brush b = g_sculpt.brush;
    const sculpt_brush_settings* s = &g_sculpt.settings[b];

    if (b == sculpt_brush_grab) {
        for (uint32_t k = 0; k < s_stroke.face_type; k++) {
            const grab_item* it = &s_stroke.items[k];
            if (it->vert >= mesh->vert_count) continue;
            mesh->verts[it->vert].obj_vert_pos = v3_add(it->start, v3_scale(mirror_vec(drag_delta, it->mirror), it->w));
        }
        return s_stroke.face_type > 0;
    }

    if (!ensure_scratch((void**)&s_nrm, &s_nrm_cap, mesh->vert_count, sizeof(editor_vec3))) return false;
    sculpt_compute_vertex_normals(mesh, s_nrm);

    const float dir = (g_sculpt.invert != invert_now) ? -1.0f : 1.0f;
    uint8_t masks[8];
    const int mc = active_mirrors(masks);
    for (int k = 0; k < mc; k++)
        apply_brush_at(mesh, s_nrm, b, s, mirror_vec(center, masks[k]), mirror_vec(hit_normal, masks[k]), dir);
    return true;
}

/* ========================================================================= */
/* subdivision                                                               */
/* ========================================================================= */
static bool em_copy(const edit_mode_mesh* s, edit_mode_mesh* d)
{
    memset(d, 0, sizeof(*d));
    if (s->vert_count) {
        d->verts = (edit_mode_vert*)malloc(sizeof(edit_mode_vert) * s->vert_count);
        if (!d->verts) return false;
        memcpy(d->verts, s->verts, sizeof(edit_mode_vert) * s->vert_count);
        d->vert_count = d->vert_cap = s->vert_count;
    }
    if (s->edge_count) {
        d->edges = (edit_mode_edge*)malloc(sizeof(edit_mode_edge) * s->edge_count);
        if (!d->edges) { free(d->verts); memset(d, 0, sizeof(*d)); return false; }
        memcpy(d->edges, s->edges, sizeof(edit_mode_edge) * s->edge_count);
        d->edge_count = d->edge_cap = s->edge_count;
    }
    if (s->face_count) {
        d->faces = (edit_mode_face*)malloc(sizeof(edit_mode_face) * s->face_count);
        if (!d->faces) { free(d->verts); free(d->edges); memset(d, 0, sizeof(*d)); return false; }
        memcpy(d->faces, s->faces, sizeof(edit_mode_face) * s->face_count);
        d->face_count = d->face_cap = s->face_count;
    }
    return true;
}

static void color_avg(float out[4], const float* a, const float* b, const float* c, const float* d, int n)
{
    const float* in[4] = { a, b, c, d };
    for (int k = 0; k < 4; k++) {
        float sum = 0.0f;
        for (int i = 0; i < n; i++) sum += in[i][k];
        out[k] = sum / (float)n;
    }
}

/* Catmull-Clark topology. The ORIGINAL vertices keep their indices (0 .. nv-1), so a lower level can be rebuilt from the higher one.
   smooth = true also moves the points with the Catmull-Clark rules (boundary vertices stay put).
   out_shift (nv entries, caller free()s): how far each original vertex moved, so undivide can take it back out. */
static bool subdivide_mesh(const edit_mode_mesh* src, edit_mode_mesh* dst, bool smooth, editor_vec3** out_shift)
{
    memset(dst, 0, sizeof(*dst));
    const uint32_t nv = src->vert_count, ne = src->edge_count, nf = src->face_count;
    bool ok = false;

    uint32_t*    edge_mid = (uint32_t*)   calloc(ne ? ne : 1u, sizeof(uint32_t));
    uint32_t*    face_pt  = (uint32_t*)   calloc(nf ? nf : 1u, sizeof(uint32_t));
    editor_vec3* fp       = (editor_vec3*)calloc(nf ? nf : 1u, sizeof(editor_vec3));
    editor_vec3* fsum     = (editor_vec3*)calloc(nv ? nv : 1u, sizeof(editor_vec3));
    editor_vec3* rsum     = (editor_vec3*)calloc(nv ? nv : 1u, sizeof(editor_vec3));
    editor_vec3* newp     = (editor_vec3*)calloc(nv ? nv : 1u, sizeof(editor_vec3));
    editor_vec3* shift    = (editor_vec3*)calloc(nv ? nv : 1u, sizeof(editor_vec3));
    uint32_t*    fcnt     = (uint32_t*)   calloc(nv ? nv : 1u, sizeof(uint32_t));
    uint32_t*    rcnt     = (uint32_t*)   calloc(nv ? nv : 1u, sizeof(uint32_t));
    uint8_t*     bnd      = (uint8_t*)    calloc(nv ? nv : 1u, 1);
    if (!edge_mid || !face_pt || !fp || !fsum || !rsum || !newp || !shift || !fcnt || !rcnt || !bnd) goto done;

    /* face points */
    for (uint32_t f = 0; f < nf; f++) {
        const edit_mode_face* face = &src->faces[f];
        editor_vec3 sum = v3(0, 0, 0);
        for (int i = 0; i < face->face_type; i++) sum = v3_add(sum, src->verts[face->vert_to_edge_array[i]].obj_vert_pos);
        fp[f] = v3_scale(sum, 1.0f / (float)face->face_type);
        for (int i = 0; i < face->face_type; i++) { fsum[face->vert_to_edge_array[i]] = v3_add(fsum[face->vert_to_edge_array[i]], fp[f]); fcnt[face->vert_to_edge_array[i]]++; }
    }
    /* edge_to_face_array midpoints (for the vertex rule) + boundary flags */
    for (uint32_t e = 0; e < ne; e++) {
        const uint32_t a = src->edges[e].vert_to_edge_array[0], b = src->edges[e].vert_to_edge_array[1];
        const editor_vec3 mid = v3_scale(v3_add(src->verts[a].obj_vert_pos, src->verts[b].obj_vert_pos), 0.5f);
        rsum[a] = v3_add(rsum[a], mid);  rcnt[a]++;
        rsum[b] = v3_add(rsum[b], mid);  rcnt[b]++;
        if (src->edges[e].face_count < 2) bnd[a] = bnd[b] = 1;
    }
    /* new positions of the original vertices: (F + 2R + (n-3)P) / n */
    for (uint32_t i = 0; i < nv; i++) {
        const editor_vec3 P = src->verts[i].obj_vert_pos;
        newp[i] = P;
        if (smooth && !bnd[i] && rcnt[i] >= 3 && fcnt[i] > 0) {
            const float n = (float)rcnt[i];
            const editor_vec3 F = v3_scale(fsum[i], 1.0f / (float)fcnt[i]);
            const editor_vec3 R = v3_scale(rsum[i], 1.0f / n);
            newp[i] = v3_scale(v3_add(v3_add(F, v3_scale(R, 2.0f)), v3_scale(P, n - 3.0f)), 1.0f / n);
        }
        shift[i] = v3_sub(newp[i], P);
    }

    /* build: originals first (indices preserved) */
    for (uint32_t i = 0; i < nv; i++)
        if (em_add_vert(dst, newp[i], src->verts[i].obj_vert_color) != i) goto done;

    for (uint32_t e = 0; e < ne; e++) {
        const edit_mode_edge* ed = &src->edges[e];
        const edit_mode_vert *va = &src->verts[ed->vert_to_edge_array[0]], *vb = &src->verts[ed->vert_to_edge_array[1]];
        editor_vec3 p = v3_scale(v3_add(va->obj_vert_pos, vb->obj_vert_pos), 0.5f);
        if (smooth && ed->face_count == 2 && ed->face[0] < nf && ed->face[1] < nf)
            p = v3_scale(v3_add(v3_add(va->obj_vert_pos, vb->obj_vert_pos), v3_add(fp[ed->face[0]], fp[ed->face[1]])), 0.25f);
        float col[4];
        color_avg(col, va->obj_vert_color, vb->obj_vert_color, NULL, NULL, 2);
        edge_mid[e] = em_add_vert(dst, p, col);
    }
    for (uint32_t f = 0; f < nf; f++) {
        const edit_mode_face* face = &src->faces[f];
        float col[4];
        color_avg(col, src->verts[face->vert_to_edge_array[0]].obj_vert_color, src->verts[face->vert_to_edge_array[1]].obj_vert_color, src->verts[face->vert_to_edge_array[2]].obj_vert_color,
                  face->face_type == 4 ? src->verts[face->vert_to_edge_array[3]].obj_vert_color : NULL, face->face_type == 4 ? 4 : 3);
        face_pt[f] = em_add_vert(dst, fp[f], col);
    }
    /* every corner becomes one quad: corner, next edge_to_face_array midpoint, face point, previous edge_to_face_array midpoint (winding is kept) */
    for (uint32_t f = 0; f < nf; f++) {
        const edit_mode_face* face = &src->faces[f];
        const int n = face->face_type;
        for (int i = 0; i < n; i++) {
            const uint32_t q[4] = { face->vert_to_edge_array[i], edge_mid[face->edge_to_face_array[i]], face_pt[f], edge_mid[face->edge_to_face_array[(i + n - 1) % n]] };
            em_add_face(dst, q, 4);
        }
    }
    ok = true;

done:
    free(edge_mid); free(face_pt); free(fp); free(fsum); free(rsum); free(newp);
    free(fcnt); free(rcnt); free(bnd);
    if (ok) *out_shift = shift;
    else { free(shift); em_mesh_free(dst); memset(dst, 0, sizeof(*dst)); }
    return ok;
}

/* level stack of the object being subdivided. base[i] = the mesh BEFORE subdivision i. */
static struct {
    uint32_t     object_id;
    int          depth;
    edit_mode_mesh      base[sculpt_max_levels];
    editor_vec3* shift[sculpt_max_levels];
    uint32_t     high_verts[sculpt_max_levels];     /* vertex face_type right after subdivision i: detects outside topology edits */
} s_levels = { 0 };

static void levels_reset(uint32_t object_id)
{
    for (int i = 0; i < s_levels.depth; i++) { em_mesh_free(&s_levels.base[i]); free(s_levels.shift[i]); s_levels.shift[i] = NULL; }
    s_levels.depth     = 0;
    s_levels.object_id = object_id;
}

static system_scene_object* selected_mesh_object(void)
{
    system_scene_object* obj = find_scene_object_by_id(g_scene_registry.selected_object_id);
    return (obj && obj->type == scene_object_mesh && !is_terrain_object(obj)) ? obj : NULL;   /* terrain regions: edited through Edit Mode / terrain tools only */
}

/* the mesh changed shape outside this system (edit mode, extrude ...): the stored levels no longer fit */
static bool levels_valid(const system_scene_object* obj, const edit_mode_mesh* mesh)
{
    if (s_levels.object_id != obj->id || s_levels.depth == 0) return false;
    return mesh->vert_count == s_levels.high_verts[s_levels.depth - 1];
}

bool sculpt_subdivide_selected(void)
{
    start_sculpt_mode();
    system_scene_object* obj = selected_mesh_object();
    if (!obj) {
        const system_scene_object* s = find_scene_object_by_id(g_scene_registry.selected_object_id);
        phoenix_warn("Sculpt", "subdivide refused: obj_vert_selected '%s' (id %u) is %s", s ? s->name : "nothing", (unsigned)g_scene_registry.selected_object_id,
                     (s && is_terrain_object(s)) ? "terrain (edit it in Edit Mode)" : "not a mesh");
        return false;
    }
    edit_mode_mesh* mesh = em_registry_get_or_create(obj);
    if (!mesh || mesh->face_count == 0) { phoenix_warn("Sculpt", "subdivide refused: '%s' has no edit mesh (mesh=%p faces=%u)", obj->name, (void*)mesh, mesh ? (unsigned)mesh->face_count : 0u); return false; }
    if ((uint64_t)mesh->face_count * 4u > (uint64_t)sculpt_max_faces) { phoenix_warn("Sculpt", "subdivide refused: '%s' would exceed %u faces (now %u)", obj->name, (unsigned)sculpt_max_faces, (unsigned)mesh->face_count); return false; }

    if (s_levels.object_id != obj->id || (s_levels.depth > 0 && !levels_valid(obj, mesh))) levels_reset(obj->id);
    if (s_levels.depth >= sculpt_max_levels) { phoenix_warn("Sculpt", "subdivide refused: '%s' is at the max level (%d)", obj->name, (int)sculpt_max_levels); return false; }

    edit_mode_mesh snapshot, result;
    editor_vec3* shift = NULL;
    if (!em_copy(mesh, &snapshot)) return false;
    if (!subdivide_mesh(mesh, &result, g_sculpt.smooth_subdivide, &shift)) { em_mesh_free(&snapshot); return false; }

    const int d = s_levels.depth++;
    s_levels.base[d]       = snapshot;
    s_levels.shift[d]      = shift;
    s_levels.high_verts[d] = result.vert_count;

    em_mesh_free(mesh);
    *mesh = result;
    {
        const bool ok = em_commit_to_gpu(mesh, obj);
        phoenix_info("Sculpt", "subdivide '%s' (id %u): level %d, %u -> %u faces, %u verts, smooth=%d, gpu commit %s", obj->name, (unsigned)obj->id,
                     s_levels.depth, (unsigned)snapshot.face_count, (unsigned)mesh->face_count, (unsigned)mesh->vert_count, (int)g_sculpt.smooth_subdivide, ok ? "ok" : "FAILED");
    }
    return true;
}

bool sculpt_undivide_selected(void)
{
    start_sculpt_mode();
    system_scene_object* obj = selected_mesh_object();
    if (!obj) return false;
    edit_mode_mesh* mesh = em_registry_get(obj->id);
    if (!mesh) return false;
    if (!levels_valid(obj, mesh)) {
        phoenix_warn("Sculpt", "undivide refused on '%s': no stored level matches the mesh (verts %u, stored depth %d, level owner id %u)", obj->name,
                     (unsigned)mesh->vert_count, s_levels.depth, (unsigned)s_levels.object_id);
        if (s_levels.object_id == obj->id) levels_reset(obj->id);
        return false;
    }

    const int d = s_levels.depth - 1;
    edit_mode_mesh* low = &s_levels.base[d];
    const editor_vec3* shift = s_levels.shift[d];

    /* original vertices kept their indices: take the sculpted position back down, minus what the smoothing added */
    for (uint32_t i = 0; i < low->vert_count && i < mesh->vert_count; i++)
        low->verts[i].obj_vert_pos = v3_sub(mesh->verts[i].obj_vert_pos, shift[i]);

    em_mesh_free(mesh);
    *mesh = *low;
    memset(low, 0, sizeof(*low));
    free(s_levels.shift[d]);
    s_levels.shift[d] = NULL;
    s_levels.depth--;

    {
        const bool ok = em_commit_to_gpu(mesh, obj);
        phoenix_info("Sculpt", "undivide '%s' (id %u): back to level %d, %u faces, gpu commit %s", obj->name, (unsigned)obj->id, s_levels.depth, (unsigned)mesh->face_count, ok ? "ok" : "FAILED");
    }
    return true;
}

int sculpt_subdiv_level(void)
{
    system_scene_object* obj = selected_mesh_object();
    if (!obj || s_levels.object_id != obj->id) return 0;
    return s_levels.depth;
}

bool sculpt_selected_mesh_counts(uint32_t* verts, uint32_t* faces)
{
    system_scene_object* obj = selected_mesh_object();
    if (!obj) return false;
    edit_mode_mesh* mesh = em_registry_get(obj->id);
    if (verts) *verts = mesh ? mesh->vert_count : 0u;
    if (faces) *faces = mesh ? mesh->face_count : 0u;
    return true;
}