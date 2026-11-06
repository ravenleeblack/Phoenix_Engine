#include "system_globals.h"

#define N SBX_CHUNK_CELLS

/* ========================================================================= */
/* the case table, built once at start.                                       */
/* corner i is set in the case index when its density is solid. for each of    */
/* the 256 cases the surface is found the same way on every face:             */
/*   - a face edge_to_face_array whose two corners differ is crossed                        */
/*   - 2 crossings on a face = one segment, 4 crossings = the solid corners   */
/*     are cut off from each other (same rule on both cubes that share the     */
/*     face, so the surface never cracks)                                      */
/* the segments join into loops, each loop becomes a triangle fan.             */
/* ========================================================================= */
static const int k_corner[8][3]  = { {0,0,0},{1,0,0},{1,0,1},{0,0,1},{0,1,0},{1,1,0},{1,1,1},{0,1,1} };
static const int k_edge[12][2]   = { {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7} };
static const int k_face_c[6][4]  = { {0,1,2,3},{4,5,6,7},{0,1,5,4},{3,2,6,7},{0,3,7,4},{1,2,6,5} };
static const int k_face_e[6][4]  = { {0,1,2,3},{4,5,6,7},{0,9,4,8},{2,10,6,11},{3,11,7,8},{1,10,5,9} };

#define MAX_TRI_INDICES 48
static int8_t   s_tri[256][MAX_TRI_INDICES];     /* edge_to_face_array numbers, 3 per triangle, -1 ends the list */
static bool     s_table_ready = false;
static float    s_cell = 1.0f;

static void edge_mid(int e, float p[3])
{
    for (int k = 0; k < 3; k++) p[k] = 0.5f * (float)(k_corner[k_edge[e][0]][k] + k_corner[k_edge[e][1]][k]);
}

static void build_table(void)
{
    for (int c = 0; c < 256; c++) {
        int n = 0;
        int adj[12][2], deg[12];
        bool inside[8];
        for (int i = 0; i < 8; i++) inside[i] = (c >> i) & 1;
        for (int e = 0; e < 12; e++) { deg[e] = 0; adj[e][0] = adj[e][1] = -1; }

        for (int f = 0; f < 6; f++) {
            int cross[4], cc = 0;
            for (int j = 0; j < 4; j++)
                if (inside[k_face_c[f][j]] != inside[k_face_c[f][(j + 1) % 4]]) cross[cc++] = j;
            int seg[4][2], sc = 0;
            if (cc == 2) {
                seg[sc][0] = k_face_e[f][cross[0]]; seg[sc][1] = k_face_e[f][cross[1]]; sc++;
            } else if (cc == 4) {
                for (int j = 0; j < 4; j++)
                    if (inside[k_face_c[f][j]]) { seg[sc][0] = k_face_e[f][(j + 3) % 4]; seg[sc][1] = k_face_e[f][j]; sc++; }
            }
            for (int s = 0; s < sc; s++) {
                int a = seg[s][0], b = seg[s][1];
                if (deg[a] < 2) adj[a][deg[a]++] = b;
                if (deg[b] < 2) adj[b][deg[b]++] = a;
            }
        }

        bool visited[12] = {0};
        for (int e0 = 0; e0 < 12; e0++) {
            if (visited[e0] || deg[e0] != 2) continue;
            int loop[12], ln = 0, prev = -1, cur = e0;
            for (int guard = 0; guard < 13; guard++) {
                loop[ln++] = cur; visited[cur] = true;
                int next = (adj[cur][0] != prev) ? adj[cur][0] : adj[cur][1];
                if (prev == -1) next = adj[cur][0];
                prev = cur; cur = next;
                if (cur == e0 || cur < 0) break;
            }
            if (ln < 3) continue;

            /* polygon normal (Newell) and centroid */
            float P[12][3], cen[3] = {0,0,0}, nrm[3] = {0,0,0};
            for (int k = 0; k < ln; k++) { edge_mid(loop[k], P[k]); for (int a = 0; a < 3; a++) cen[a] += P[k][a] / (float)ln; }
            for (int k = 0; k < ln; k++) {
                const float* p = P[k]; const float* q = P[(k + 1) % ln];
                nrm[0] += (p[1] - q[1]) * (p[2] + q[2]);
                nrm[1] += (p[2] - q[2]) * (p[0] + q[0]);
                nrm[2] += (p[0] - q[0]) * (p[1] + q[1]);
            }

            /* outward = toward air = against the density gradient (trilinear gradient of the 0/1 corners at the centroid) */
            float g[3] = {0,0,0};
            for (int i = 0; i < 8; i++) {
                if (!inside[i]) continue;
                float w[3], s[3];
                for (int a = 0; a < 3; a++) { w[a] = k_corner[i][a] ? cen[a] : 1.0f - cen[a]; s[a] = k_corner[i][a] ? 1.0f : -1.0f; }
                g[0] += s[0] * w[1] * w[2]; g[1] += w[0] * s[1] * w[2]; g[2] += w[0] * w[1] * s[2];
            }
            float d = nrm[0] * g[0] + nrm[1] * g[1] + nrm[2] * g[2];
            if (fabsf(d) < 1e-6f) {                         /* gradient cancels: look toward the solid corners instead */
                float ic[3] = {0,0,0}; int cnt = 0;
                for (int i = 0; i < 8; i++) if (inside[i]) { for (int a = 0; a < 3; a++) ic[a] += (float)k_corner[i][a]; cnt++; }
                d = 0;
                for (int a = 0; a < 3; a++) d += nrm[a] * (ic[a] / (float)(cnt ? cnt : 1) - cen[a]);
            }
            if (d > 0.0f) for (int a = 0, b = ln - 1; a < b; a++, b--) { int t = loop[a]; loop[a] = loop[b]; loop[b] = t; }

            for (int k = 1; k + 1 < ln && n + 3 < MAX_TRI_INDICES; k++) {
                s_tri[c][n++] = (int8_t)loop[0]; s_tri[c][n++] = (int8_t)loop[k]; s_tri[c][n++] = (int8_t)loop[k + 1];
            }
        }
        for (int i = n; i < MAX_TRI_INDICES; i++) s_tri[c][i] = -1;
    }
    s_table_ready = true;
}

/* ========================================================================= */
/* chunk storage: hash table of 32^3 corner blocks                            */
/* ========================================================================= */
typedef struct { int cx, cy, cz; bool dirty; uint8_t d[N * N * N]; } sbx_chunk;

static sbx_chunk** s_tab = NULL;
static size_t      s_cap = 0, s_count = 0;

static int fdiv(int a, int n) { int q = a / n; return (a % n != 0 && ((a < 0) != (n < 0))) ? q - 1 : q; }
static size_t hash3(int x, int y, int z)
{
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    return (size_t)h;
}

static sbx_chunk* find_slot(int cx, int cy, int cz, size_t* out_slot)
{
    if (!s_cap) { if (out_slot) *out_slot = 0; return NULL; }
    size_t i = hash3(cx, cy, cz) & (s_cap - 1);
    while (s_tab[i]) {
        if (s_tab[i]->cx == cx && s_tab[i]->cy == cy && s_tab[i]->cz == cz) { if (out_slot) *out_slot = i; return s_tab[i]; }
        i = (i + 1) & (s_cap - 1);
    }
    if (out_slot) *out_slot = i;
    return NULL;
}

static void grow(void)
{
    size_t ncap = s_cap ? s_cap * 2 : 64;
    sbx_chunk** nt = (sbx_chunk**)calloc(ncap, sizeof(*nt));
    if (!nt) return;
    for (size_t i = 0; i < s_cap; i++) {
        if (!s_tab[i]) continue;
        size_t j = hash3(s_tab[i]->cx, s_tab[i]->cy, s_tab[i]->cz) & (ncap - 1);
        while (nt[j]) j = (j + 1) & (ncap - 1);
        nt[j] = s_tab[i];
    }
    free(s_tab); s_tab = nt; s_cap = ncap;
}

static sbx_chunk* get_chunk(int cx, int cy, int cz, bool create)
{
    size_t slot;
    sbx_chunk* c = find_slot(cx, cy, cz, &slot);
    if (c || !create) return c;
    if ((s_count + 1) * 2 > s_cap) { grow(); find_slot(cx, cy, cz, &slot); }
    c = (sbx_chunk*)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->cx = cx; c->cy = cy; c->cz = cz;
    s_tab[slot] = c; s_count++;
    return c;
}

void sbx_clear(void)
{
    for (size_t i = 0; i < s_cap; i++) { free(s_tab[i]); s_tab[i] = NULL; }
    s_count = 0;
}

void sbx_start(float cell_size_m)
{
    s_cell = cell_size_m > 0.0f ? cell_size_m : 1.0f;
    sbx_clear();
    if (!s_table_ready) build_table();
}

void  sbx_shutdown(void) { sbx_clear(); free(s_tab); s_tab = NULL; s_cap = 0; }
float sbx_cell_size(void) { return s_cell; }
int   sbx_chunk_count(void) { return (int)s_count; }

static inline int cidx(int lx, int ly, int lz) { return (lz * N + ly) * N + lx; }

uint8_t sbx_get(int gx, int gy, int gz)
{
    const sbx_chunk* c = get_chunk(fdiv(gx, N), fdiv(gy, N), fdiv(gz, N), false);
    return c ? c->d[cidx(gx - c->cx * N, gy - c->cy * N, gz - c->cz * N)] : 0;
}

void sbx_set(int gx, int gy, int gz, uint8_t vert_to_edge_array)
{
    if (vert_to_edge_array > SBX_MAX_DENSITY) vert_to_edge_array = SBX_MAX_DENSITY;
    sbx_chunk* c = get_chunk(fdiv(gx, N), fdiv(gy, N), fdiv(gz, N), true);
    if (!c) return;
    uint8_t* p = &c->d[cidx(gx - c->cx * N, gy - c->cy * N, gz - c->cz * N)];
    if (*p == vert_to_edge_array) return;
    *p = vert_to_edge_array;
    /* every cube that uses this corner: cells gx-1..gx etc. they can sit in up to 8 chunks */
    for (int dz = -1; dz <= 0; dz++) for (int dy = -1; dy <= 0; dy++) for (int dx = -1; dx <= 0; dx++) {
        sbx_chunk* mesh = get_chunk(fdiv(gx + dx, N), fdiv(gy + dy, N), fdiv(gz + dz, N), true);
        if (mesh) mesh->dirty = true;
    }
}

bool sbx_next_dirty_chunk(int* cx, int* cy, int* cz)
{
    for (size_t i = 0; i < s_cap; i++) {
        if (s_tab[i] && s_tab[i]->dirty) {
            s_tab[i]->dirty = false;
            *cx = s_tab[i]->cx; *cy = s_tab[i]->cy; *cz = s_tab[i]->cz;
            return true;
        }
    }
    return false;
}

/* ========================================================================= */
/* filling and the digger brush                                               */
/* ========================================================================= */
static uint8_t level_from(float vert_to_edge_array)     /* 0..15 */
{
    int l = (int)floorf(vert_to_edge_array + 0.5f);
    return (uint8_t)(l < 0 ? 0 : (l > SBX_MAX_DENSITY ? SBX_MAX_DENSITY : l));
}

void sbx_fill_from_heights(sbx_height_fn height_m, void* user, int gx0, int gz0, int gx1, int gz1, int gy_min, int gy_max)
{
    if (!height_m) return;
    for (int gz = gz0; gz <= gz1; gz++)
        for (int gx = gx0; gx <= gx1; gx++) {
            const float h = height_m(gx, gz, user);
            for (int gy = gy_min; gy <= gy_max; gy++) {
                const float y = (float)gy * s_cell;
                /* the surface sits where the density passes 7.5, one cell wide transition */
                uint8_t d = (gy == gy_min) ? SBX_MAX_DENSITY : level_from(SBX_ISO + (h - y) / s_cell * 15.0f);
                sbx_set(gx, gy, gz, d);
            }
        }
}

void sbx_sphere(float x, float y, float z, float radius, sbx_brush_mode mode)
{
    const float inv = 1.0f / s_cell;
    const int x0 = (int)floorf((x - radius) * inv) - 1, x1 = (int)ceilf((x + radius) * inv) + 1;
    const int y0 = (int)floorf((y - radius) * inv) - 1, y1 = (int)ceilf((y + radius) * inv) + 1;
    const int z0 = (int)floorf((z - radius) * inv) - 1, z1 = (int)ceilf((z + radius) * inv) + 1;

    for (int gz = z0; gz <= z1; gz++) for (int gy = y0; gy <= y1; gy++) for (int gx = x0; gx <= x1; gx++) {
        const float dx = (float)gx * s_cell - x, dy = (float)gy * s_cell - y, dz = (float)gz * s_cell - z;
        const float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        const float cur = (float)sbx_get(gx, gy, gz);
        /* signed distance mapped to density: 7.5 on the sphere, 15 half a cell outside, 0 half a cell inside */
        const float outside_level = SBX_ISO + (dist - radius) * inv * 15.0f;
        if (mode == SBX_DIG) { if (outside_level < cur) sbx_set(gx, gy, gz, level_from(outside_level)); }
        else                 { const float inside_level = SBX_ISO - (dist - radius) * inv * 15.0f;
                               if (inside_level > cur) sbx_set(gx, gy, gz, level_from(inside_level)); }
    }
}

/* ========================================================================= */
/* meshing                                                                    */
/* ========================================================================= */
void sbx_mesh_free(sbx_mesh* mesh) { if (!mesh) return; free(mesh->vert_to_edge_array); free(mesh->i); mesh->vert_to_edge_array = NULL; mesh->i = NULL; mesh->vc = mesh->ic = 0; }

/* One vertex per crossed EDGE, shared by every triangle (and every cube) that touches it.
   Before: 3 vertices per triangle, nothing shared. Now a smooth patch costs about 1 vertex per 2 triangles.
   Vertices slide along the edge_to_face_array in 1/8 steps (SBX_EDGE_STEPS), so with a 4 mesh cell a surface lands on 0.5 mesh steps. */
#define SBX_EDGE_STEPS 8

typedef struct { uint32_t cap; } sbx_build;

static void density_gradient(int gx, int gy, int gz, float g[3])
{
    g[0] = (float)sbx_get(gx + 1, gy, gz) - (float)sbx_get(gx - 1, gy, gz);
    g[1] = (float)sbx_get(gx, gy + 1, gz) - (float)sbx_get(gx, gy - 1, gz);
    g[2] = (float)sbx_get(gx, gy, gz + 1) - (float)sbx_get(gx, gy, gz - 1);
}

static bool push_vertex(sbx_mesh* mesh, sbx_build* bld, const float p[3], const float n[3], uint32_t* out_index)
{
    if (mesh->vc + 1 > bld->cap) {
        uint32_t nc = bld->cap ? bld->cap * 2 : 2048;
        editor_vertex* nv = (editor_vertex*)realloc(mesh->vert_to_edge_array, sizeof(editor_vertex) * nc);
        if (!nv) return false;
        mesh->vert_to_edge_array = nv; bld->cap = nc;
    }
    /* top faces grass, steep faces rock, in between dirt. n is the density gradient, so it is the same on both sides of a chunk border */
    float r, g, b;
    if      (n[1] > 0.75f) { r = 0.30f; g = 0.55f; b = 0.20f; }
    else if (n[1] > 0.40f) { r = 0.45f; g = 0.35f; b = 0.22f; }
    else                   { r = 0.45f; g = 0.45f; b = 0.47f; }
    editor_vertex* vert_to_edge_array = &mesh->vert_to_edge_array[mesh->vc];
    vert_to_edge_array->x = p[0]; vert_to_edge_array->y = p[1]; vert_to_edge_array->z = p[2];
    vert_to_edge_array->r = r; vert_to_edge_array->g = g; vert_to_edge_array->b = b; vert_to_edge_array->a = 1.0f;
    *out_index = mesh->vc++;
    return true;
}

static bool push_index(sbx_mesh* mesh, uint32_t* icap, uint32_t idx)
{
    if (mesh->ic + 1 > *icap) {
        uint32_t nc = *icap ? *icap * 2 : 6144;
        uint32_t* ni = (uint32_t*)realloc(mesh->i, sizeof(uint32_t) * nc);
        if (!ni) return false;
        mesh->i = ni; *icap = nc;
    }
    mesh->i[mesh->ic++] = idx;
    return true;
}

bool sbx_mesh_chunk(int cx, int cy, int cz, float ox, float oy, float oz, sbx_mesh* out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!s_table_ready) build_table();

    /* corners of the whole chunk plus the one layer shared with the next chunk */
    static uint8_t pad[N + 1][N + 1][N + 1];
    /* edge_to_face_array -> vertex index. each corner owns the 3 edges that leave it in +x, +y, +z. -1 = not made yet */
    static int32_t edge_vertex[3][N + 1][N + 1][N + 1];
    const int bx = cx * N, by = cy * N, bz = cz * N;
    for (int z = 0; z <= N; z++) for (int y = 0; y <= N; y++) for (int x = 0; x <= N; x++)
        pad[z][y][x] = sbx_get(bx + x, by + y, bz + z);
    memset(edge_vertex, 0xFF, sizeof(edge_vertex));

    sbx_build bld = {0};
    uint32_t icap = 0;

    for (int z = 0; z < N; z++) for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        float val[8];
        int c = 0;
        for (int i = 0; i < 8; i++) {
            val[i] = (float)pad[z + k_corner[i][2]][y + k_corner[i][1]][x + k_corner[i][0]];
            if (val[i] >= 8.0f) c |= 1 << i;
        }
        if (c == 0 || c == 255) continue;

        uint32_t ev[12];
        bool     have[12] = {0};
        for (int t = 0; t < MAX_TRI_INDICES && s_tri[c][t] >= 0; t += 3) {
            for (int k = 0; k < 3; k++) {
                const int e = s_tri[c][t + k];
                if (!have[e]) {
                    int a = k_edge[e][0], b = k_edge[e][1];
                    int q = 0;                                           /* axis this edge_to_face_array runs along */
                    for (int ax = 0; ax < 3; ax++) if (k_corner[a][ax] != k_corner[b][ax]) q = ax;
                    if (k_corner[a][q] > k_corner[b][q]) { int tmp = a; a = b; b = tmp; }   /* a = low end, so both cubes get the same number */

                    const int lx = x + k_corner[a][0], ly = y + k_corner[a][1], lz = z + k_corner[a][2];
                    int32_t* slot = &edge_vertex[q][lz][ly][lx];
                    if (*slot < 0) {
                        const float va = val[a], vb = val[b];
                        float tt = (SBX_ISO - va) / (vb - va);
                        tt = floorf(tt * (float)SBX_EDGE_STEPS + 0.5f) / (float)SBX_EDGE_STEPS;
                        if (tt < 0.0f) tt = 0.0f; else if (tt > 1.0f) tt = 1.0f;

                        float p[3], n[3], ga[3], gb[3];
                        const int gax = bx + lx, gay = by + ly, gaz = bz + lz;
                        const int gbx = gax + (q == 0), gby = gay + (q == 1), gbz = gaz + (q == 2);
                        p[0] = ((float)gax + (q == 0 ? tt : 0.0f)) * s_cell + ox;
                        p[1] = ((float)gay + (q == 1 ? tt : 0.0f)) * s_cell + oy;
                        p[2] = ((float)gaz + (q == 2 ? tt : 0.0f)) * s_cell + oz;
                        density_gradient(gax, gay, gaz, ga);
                        density_gradient(gbx, gby, gbz, gb);
                        for (int ax = 0; ax < 3; ax++) n[ax] = -(ga[ax] + (gb[ax] - ga[ax]) * tt);   /* toward air */
                        const float l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                        if (l > 1e-6f) { n[0] /= l; n[1] /= l; n[2] /= l; } else { n[0] = 0; n[1] = 1; n[2] = 0; }

                        uint32_t idx;
                        if (!push_vertex(out, &bld, p, n, &idx)) { sbx_mesh_free(out); return false; }
                        *slot = (int32_t)idx;
                    }
                    ev[e] = (uint32_t)*slot; have[e] = true;
                }
                if (!push_index(out, &icap, ev[e])) { sbx_mesh_free(out); return false; }
            }
        }
    }
    return out->ic > 0;
}