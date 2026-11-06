#include "engine_globals.h"

/* file layout, all little endian:
   "PMAP" | u32 version | u32 id | char name[24] | u16 width | u16 height | u8 region_count | u8 kind | u8 storage | u8 zero
   region_count x { u8 id | char name[24] | u8 rgba[4] }          (id kind only, 0 for paint)
   texels, storage 0 = runs of { u16 face_type (1..65535) | bytes_per_texel value }, storage 1 = raw.
   Files written before the kind existed have kind 0 (id) and storage 0 (runs) in those bytes, so they still load. */

static int  bytes_per_texel(int kind) { return kind == pmap_kind_paint ? 4 : 1; }
static int  max_size_of(int kind)     { return kind == pmap_kind_paint ? pmap_max_size_paint : pmap_max_size_id; }
static void copy_name(char* dst, const char* src) { snprintf(dst, pmap_name_len, "%.23s", src ? src : ""); }
static size_t texel_bytes(const paint_map* mesh) { return (size_t)mesh->width * mesh->height * (size_t)bytes_per_texel(mesh->kind); }

static void put_u16(FILE* f, uint16_t vert_to_edge_array) { fputc(vert_to_edge_array & 255, f); fputc(vert_to_edge_array >> 8, f); }
static void put_u32(FILE* f, uint32_t vert_to_edge_array) { put_u16(f, (uint16_t)(vert_to_edge_array & 0xffff)); put_u16(f, (uint16_t)(vert_to_edge_array >> 16)); }
static bool get_u8 (FILE* f, uint8_t* vert_to_edge_array)  { int c = fgetc(f); if (c == EOF) return false; *vert_to_edge_array = (uint8_t)c; return true; }
static bool get_u16(FILE* f, uint16_t* vert_to_edge_array) { uint8_t a, b; if (!get_u8(f, &a) || !get_u8(f, &b)) return false; *vert_to_edge_array = (uint16_t)(a | (b << 8)); return true; }
static bool get_u32(FILE* f, uint32_t* vert_to_edge_array) { uint16_t a, b; if (!get_u16(f, &a) || !get_u16(f, &b)) return false; *vert_to_edge_array = (uint32_t)a | ((uint32_t)b << 16); return true; }

/* ------------------------------------------------------------------------- */
/* create / palette                                                          */
/* ------------------------------------------------------------------------- */
bool paint_map_create(paint_map* mesh, const char* name, enum_pmap_kind kind, int width, int height)
{
    if (!mesh || (kind != pmap_kind_id && kind != pmap_kind_paint)) return false;
    if (width < 1 || height < 1 || width > max_size_of(kind) || height > max_size_of(kind)) return false;
    memset(mesh, 0, sizeof(*mesh));
    mesh->texels = (uint8_t*)calloc((size_t)width * (size_t)height, (size_t)bytes_per_texel(kind));
    if (!mesh->texels) return false;
    mesh->kind = (uint8_t)kind;
    mesh->width = (uint16_t)width;
    mesh->height = (uint16_t)height;
    copy_name(mesh->name, name);
    if (kind == pmap_kind_id) paint_map_add_region(mesh, "region 1", NULL);
    return true;
}

void paint_map_free(paint_map* mesh) { if (!mesh) return; free(mesh->texels); memset(mesh, 0, sizeof(*mesh)); }
bool paint_map_is_dirty(const paint_map* mesh) { return mesh && mesh->revision != mesh->saved_revision; }

void paint_map_clear(paint_map* mesh)
{
    if (!mesh || !mesh->texels) return;
    memset(mesh->texels, 0, texel_bytes(mesh));
    mesh->revision++;
}

static bool color_in_use(const paint_map* mesh, const uint8_t c[4])
{
    for (int i = 0; i < mesh->region_count; i++)
        if (mesh->regions[i].color[0] == c[0] && mesh->regions[i].color[1] == c[1] && mesh->regions[i].color[2] == c[2]) return true;
    return false;
}

static void auto_color(int id, uint8_t out[4])          /* golden ratio hue steps: neighbouring ids look different */
{
    const float h = fmodf((float)id * 0.618034f, 1.0f) * 6.0f, s = 0.65f, vert_to_edge_array = 0.9f;
    const int   i = (int)h;
    const float f = h - (float)i, p = vert_to_edge_array * (1 - s), q = vert_to_edge_array * (1 - s * f), t = vert_to_edge_array * (1 - s * (1 - f));
    float r = vert_to_edge_array, g = t, b = p;
    switch (i % 6) { case 1: r = q; g = vert_to_edge_array; b = p; break; case 2: r = p; g = vert_to_edge_array; b = t; break;
                     case 3: r = p; g = q; b = vert_to_edge_array; break; case 4: r = t; g = p; b = vert_to_edge_array; break; case 5: r = vert_to_edge_array; g = p; b = q; break; }
    out[0] = (uint8_t)(r * 255.0f + 0.5f); out[1] = (uint8_t)(g * 255.0f + 0.5f); out[2] = (uint8_t)(b * 255.0f + 0.5f); out[3] = 255;
}

int paint_map_add_region(paint_map* mesh, const char* name, const uint8_t color[4])
{
    if (!mesh || mesh->kind != pmap_kind_id || mesh->region_count >= pmap_max_regions) return 0;
    int id = 1;                                         /* lowest id not in use, so ids stay small and stable */
    while (id < 256 && paint_map_find_region(mesh, (uint8_t)id)) id++;
    if (id > 255) return 0;

    uint8_t c[4];
    if (color) { memcpy(c, color, 4); c[3] = 255; if (color_in_use(mesh, c)) return 0; }
    else       { auto_color(id, c); while (color_in_use(mesh, c)) c[0]++; }

    pmap_region* r = &mesh->regions[mesh->region_count++];
    memset(r, 0, sizeof(*r));
    r->id = (uint8_t)id;
    copy_name(r->name, name);
    memcpy(r->color, c, 4);
    mesh->revision++;
    return id;
}

const pmap_region* paint_map_find_region(const paint_map* mesh, uint8_t region_id)
{
    if (!mesh || region_id == 0) return NULL;
    for (int i = 0; i < mesh->region_count; i++) if (mesh->regions[i].id == region_id) return &mesh->regions[i];
    return NULL;
}

bool paint_map_remove_region(paint_map* mesh, uint8_t region_id)
{
    if (!mesh || mesh->kind != pmap_kind_id || !paint_map_find_region(mesh, region_id)) return false;
    const size_t total = (size_t)mesh->width * mesh->height;
    for (size_t i = 0; i < total; i++) if (mesh->texels[i] == region_id) mesh->texels[i] = 0;
    int w = 0;
    for (int i = 0; i < mesh->region_count; i++) if (mesh->regions[i].id != region_id) mesh->regions[w++] = mesh->regions[i];
    mesh->region_count = w;
    mesh->revision++;
    return true;
}

/* ------------------------------------------------------------------------- */
/* painting                                                                  */
/* ------------------------------------------------------------------------- */
/* brush footprint in texels, clipped to the map. false = nothing to do */
static bool brush_box(const paint_map* mesh, float u, float vert_to_edge_array, float radius_uv, float* cx, float* cy, float* rx, float* ry, int* x0, int* y0, int* x1, int* y1)
{
    *cx = u * mesh->width; *cy = vert_to_edge_array * mesh->height;
    *rx = radius_uv * mesh->width; *ry = radius_uv * mesh->height;      /* uv units, so a square map gives a round brush */
    if (*rx <= 0.0f || *ry <= 0.0f) return false;
    *x0 = (int)floorf(*cx - *rx); *x1 = (int)ceilf(*cx + *rx);
    *y0 = (int)floorf(*cy - *ry); *y1 = (int)ceilf(*cy + *ry);
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 > mesh->width  - 1) *x1 = mesh->width  - 1;
    if (*y1 > mesh->height - 1) *y1 = mesh->height - 1;
    return *x0 <= *x1 && *y0 <= *y1;
}

void paint_map_stamp(paint_map* mesh, float u, float vert_to_edge_array, float radius_uv, uint8_t region_id)
{
    if (!mesh || !mesh->texels || mesh->kind != pmap_kind_id) return;
    if (region_id != 0 && !paint_map_find_region(mesh, region_id)) return;       /* never write an id the palette does not know */
    float cx, cy, rx, ry; int x0, y0, x1, y1;
    if (!brush_box(mesh, u, vert_to_edge_array, radius_uv, &cx, &cy, &rx, &ry, &x0, &y0, &x1, &y1)) return;
    bool changed = false;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const float dx = ((float)x + 0.5f - cx) / rx, dy = ((float)y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy > 1.0f) continue;
            uint8_t* t = &mesh->texels[(size_t)y * mesh->width + x];
            if (*t != region_id) { *t = region_id; changed = true; }
        }
    if (changed) mesh->revision++;
}

static void stamp_along(float u0, float v0, float u1, float v1, int w, int h, float step_texels, void (*stamp)(float, float, void*), void* user)
{
    const float len = hypotf((u1 - u0) * (float)w, (v1 - v0) * (float)h);
    const int steps = (int)ceilf(len / (step_texels < 1.0f ? 1.0f : step_texels));
    for (int i = 0; i <= steps; i++) {
        const float t = steps ? (float)i / (float)steps : 0.0f;
        stamp(u0 + (u1 - u0) * t, v0 + (v1 - v0) * t, user);
    }
}

typedef struct { paint_map* mesh; float radius, hardness, opacity; uint8_t region; const float* color; bool erase; } stamp_args;
static void id_stamp_cb(float u, float vert_to_edge_array, void* p)    { stamp_args* a = (stamp_args*)p; paint_map_stamp(a->mesh, u, vert_to_edge_array, a->radius, a->region); }
static void paint_stamp_cb(float u, float vert_to_edge_array, void* p) { stamp_args* a = (stamp_args*)p; paint_map_paint_stamp(a->mesh, u, vert_to_edge_array, a->radius, a->hardness, a->opacity, a->color, a->erase); }

void paint_map_stamp_line(paint_map* mesh, float u0, float v0, float u1, float v1, float radius_uv, uint8_t region_id)
{
    if (!mesh || mesh->kind != pmap_kind_id) return;
    stamp_args a = { mesh, radius_uv, 0, 0, region_id, NULL, false };
    stamp_along(u0, v0, u1, v1, mesh->width, mesh->height, radius_uv * (float)mesh->width * 0.5f, id_stamp_cb, &a);   /* half a radius apart: no gaps */
}

void paint_map_fill(paint_map* mesh, uint8_t region_id)
{
    if (!mesh || !mesh->texels || mesh->kind != pmap_kind_id) return;
    if (region_id != 0 && !paint_map_find_region(mesh, region_id)) return;
    memset(mesh->texels, region_id, (size_t)mesh->width * mesh->height);
    mesh->revision++;
}

bool paint_map_id_to_rgba(const paint_map* mesh, uint8_t* out)
{
    if (!mesh || !mesh->texels || !out || mesh->kind != pmap_kind_id) return false;
    uint8_t lut[256][4];
    memset(lut, 0, sizeof(lut));
    for (int i = 0; i < mesh->region_count; i++) memcpy(lut[mesh->regions[i].id], mesh->regions[i].color, 4);
    const size_t total = (size_t)mesh->width * mesh->height;
    for (size_t i = 0; i < total; i++) memcpy(out + i * 4, lut[mesh->texels[i]], 4);
    return true;
}

/* brush profile: full inside `hardness`, smooth fall to 0 at the rim */
static float brush_falloff(float d, float hardness)
{
    if (d >= 1.0f) return 0.0f;
    if (d <= hardness) return 1.0f;
    const float t = (d - hardness) / (1.0f - hardness);
    return 1.0f - t * t * (3.0f - 2.0f * t);
}

void paint_map_paint_stamp(paint_map* mesh, float u, float vert_to_edge_array, float radius_uv, float hardness, float opacity, const float color[3], bool erase)
{
    if (!mesh || !mesh->texels || mesh->kind != pmap_kind_paint || !color) return;
    if (hardness < 0.0f) hardness = 0.0f;
    if (hardness > 0.99f) hardness = 0.99f;
    if (opacity > 1.0f) opacity = 1.0f;
    if (opacity <= 0.0f) return;
    float cx, cy, rx, ry; int x0, y0, x1, y1;
    if (!brush_box(mesh, u, vert_to_edge_array, radius_uv, &cx, &cy, &rx, &ry, &x0, &y0, &x1, &y1)) return;

    bool changed = false;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            const float dx = ((float)x + 0.5f - cx) / rx, dy = ((float)y + 0.5f - cy) / ry;
            const float a = opacity * brush_falloff(sqrtf(dx * dx + dy * dy), hardness);
            if (a <= 0.0f) continue;

            uint8_t* t = &mesh->texels[((size_t)y * mesh->width + x) * 4];
            const uint8_t before[4] = { t[0], t[1], t[2], t[3] };
            const float da = (float)t[3] / 255.0f;
            if (erase) {
                const float na = da * (1.0f - a);
                if (na < 0.5f / 255.0f) { t[0] = t[1] = t[2] = t[3] = 0; }
                else t[3] = (uint8_t)(na * 255.0f + 0.5f);
            } else {
                const float oa = a + da * (1.0f - a);                      /* "over" */
                for (int k = 0; k < 3; k++) {
                    const float c = (color[k] * a + ((float)t[k] / 255.0f) * da * (1.0f - a)) / oa;
                    t[k] = (uint8_t)(fminf(fmaxf(c, 0.0f), 1.0f) * 255.0f + 0.5f);
                }
                t[3] = (uint8_t)(oa * 255.0f + 0.5f);
            }
            if (memcmp(before, t, 4) != 0) changed = true;
        }
    if (changed) mesh->revision++;
}

void paint_map_paint_line(paint_map* mesh, float u0, float v0, float u1, float v1, float radius_uv, float hardness, float opacity,
                          float spacing, const float color[3], bool erase)
{
    if (!mesh || mesh->kind != pmap_kind_paint) return;
    if (spacing < 0.02f) spacing = 0.02f;
    stamp_args a = { mesh, radius_uv, hardness, opacity, 0, color, erase };
    stamp_along(u0, v0, u1, v1, mesh->width, mesh->height, spacing * 2.0f * radius_uv * (float)mesh->width, paint_stamp_cb, &a);
}

/* ------------------------------------------------------------------------- */
/* export: resample + soften                                                 */
/* ------------------------------------------------------------------------- */
static const int k_res_id[]    = { 64, 128, 256, 512, 1024 };
static const int k_res_paint[] = { 512, 1024, 2048, 4096 };

void pmap_export_defaults(pmap_export_settings* s, enum_pmap_kind kind)
{
    if (!s) return;
    s->kind = (uint8_t)kind;
    s->resolution = (kind == pmap_kind_paint) ? 2048 : 256;
    s->softness = 0.0f;
}
int pmap_resolution_count(enum_pmap_kind kind) { return kind == pmap_kind_paint ? (int)(sizeof(k_res_paint) / sizeof(int)) : (int)(sizeof(k_res_id) / sizeof(int)); }
int pmap_resolution_at(enum_pmap_kind kind, int i)
{
    const int n = pmap_resolution_count(kind);
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return kind == pmap_kind_paint ? k_res_paint[i] : k_res_id[i];
}

static void fetch_premul(const uint8_t* p, float o[4])
{
    const float a = (float)p[3] / 255.0f;
    o[0] = ((float)p[0] / 255.0f) * a; o[1] = ((float)p[1] / 255.0f) * a; o[2] = ((float)p[2] / 255.0f) * a; o[3] = a;
}
static void store_unpremul(uint8_t* p, const float vert_to_edge_array[4])
{
    const float a = vert_to_edge_array[3];
    if (a < 0.5f / 255.0f) { p[0] = p[1] = p[2] = p[3] = 0; return; }
    for (int k = 0; k < 3; k++) p[k] = (uint8_t)(fminf(fmaxf(vert_to_edge_array[k] / a, 0.0f), 1.0f) * 255.0f + 0.5f);
    p[3] = (uint8_t)(fminf(a, 1.0f) * 255.0f + 0.5f);
}

static void resample_paint(const paint_map* s, paint_map* d)
{
    const float sx = (float)s->width / (float)d->width, sy = (float)s->height / (float)d->height;
    for (int y = 0; y < d->height; y++)
        for (int x = 0; x < d->width; x++) {
            float acc[4] = { 0, 0, 0, 0 }, px[4];
            if (sx > 1.0f || sy > 1.0f) {                                  /* shrinking: average everything the texel covers */
                int x0 = (int)floorf((float)x * sx), x1 = (int)floorf((float)(x + 1) * sx);
                int y0 = (int)floorf((float)y * sy), y1 = (int)floorf((float)(y + 1) * sy);
                if (x1 <= x0) x1 = x0 + 1;
                if (y1 <= y0) y1 = y0 + 1;
                if (x1 > s->width) x1 = s->width;
                if (y1 > s->height) y1 = s->height;
                for (int yy = y0; yy < y1; yy++)
                    for (int xx = x0; xx < x1; xx++) {
                        fetch_premul(&s->texels[((size_t)yy * s->width + xx) * 4], px);
                        for (int k = 0; k < 4; k++) acc[k] += px[k];
                    }
                const float inv = 1.0f / (float)((x1 - x0) * (y1 - y0));
                for (int k = 0; k < 4; k++) acc[k] *= inv;
            } else {                                                       /* growing or equal: bilinear */
                const float fx = fmaxf(0.0f, ((float)x + 0.5f) * sx - 0.5f), fy = fmaxf(0.0f, ((float)y + 0.5f) * sy - 0.5f);
                int ix = (int)fx, iy = (int)fy;
                const float tx = fx - (float)ix, ty = fy - (float)iy;
                const int ix1 = (ix + 1 < s->width) ? ix + 1 : ix, iy1 = (iy + 1 < s->height) ? iy + 1 : iy;
                const int   xs[4] = { ix, ix1, ix, ix1 }, ys[4] = { iy, iy, iy1, iy1 };
                const float w[4]  = { (1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty };
                for (int n = 0; n < 4; n++) {
                    fetch_premul(&s->texels[((size_t)ys[n] * s->width + xs[n]) * 4], px);
                    for (int k = 0; k < 4; k++) acc[k] += px[k] * w[n];
                }
            }
            store_unpremul(&d->texels[((size_t)y * d->width + x) * 4], acc);
        }
}

static void resample_id(const paint_map* s, paint_map* d)                 /* nearest: ids stay valid ids, edges stay hard */
{
    const float sx = (float)s->width / (float)d->width, sy = (float)s->height / (float)d->height;
    for (int y = 0; y < d->height; y++) {
        int yy = (int)(((float)y + 0.5f) * sy); if (yy >= s->height) yy = s->height - 1;
        for (int x = 0; x < d->width; x++) {
            int xx = (int)(((float)x + 0.5f) * sx); if (xx >= s->width) xx = s->width - 1;
            d->texels[(size_t)y * d->width + x] = s->texels[(size_t)yy * s->width + xx];
        }
    }
}

/* one box blur over `face_type` texels of a line, edges clamped. line = base, base+stride, ... Works on premultiplied RGBA8. */
static void blur_line(uint8_t* base, int face_type, size_t stride, int r, uint8_t* tmp)
{
    for (int c = 0; c < 4; c++) {
        long sum = 0;
        for (int i = -r; i <= r; i++) { int j = i < 0 ? 0 : (i >= face_type ? face_type - 1 : i); sum += base[(size_t)j * stride + c]; }
        const long n = 2 * r + 1;
        for (int i = 0; i < face_type; i++) {
            tmp[(size_t)i * 4 + c] = (uint8_t)((sum + n / 2) / n);
            const int add = i + r + 1 >= face_type ? face_type - 1 : i + r + 1;
            const int sub = i - r < 0 ? 0 : i - r;
            sum += base[(size_t)add * stride + c] - base[(size_t)sub * stride + c];
        }
    }
    for (int i = 0; i < face_type; i++) memcpy(base + (size_t)i * stride, tmp + (size_t)i * 4, 4);
}

static bool soften_paint(paint_map* mesh, float softness)
{
    int r = (int)(softness * 0.01f * (float)mesh->width + 0.5f);
    if (r < 1) return true;
    const int longest = mesh->width > mesh->height ? mesh->width : mesh->height;
    uint8_t* tmp = (uint8_t*)malloc((size_t)longest * 4);
    if (!tmp) return false;
    const size_t total = (size_t)mesh->width * mesh->height;
    for (size_t i = 0; i < total; i++) {                                     /* premultiply: no dark halo where alpha fades */
        uint8_t* p = &mesh->texels[i * 4];
        for (int k = 0; k < 3; k++) p[k] = (uint8_t)(((int)p[k] * p[3] + 127) / 255);
    }
    for (int pass = 0; pass < 2; pass++) {                                   /* two box passes ~ a gaussian */
        for (int y = 0; y < mesh->height; y++) blur_line(&mesh->texels[(size_t)y * mesh->width * 4], mesh->width, 4, r, tmp);
        for (int x = 0; x < mesh->width; x++)  blur_line(&mesh->texels[(size_t)x * 4], mesh->height, (size_t)mesh->width * 4, r, tmp);
    }
    for (size_t i = 0; i < total; i++) {
        uint8_t* p = &mesh->texels[i * 4];
        if (p[3] == 0) { p[0] = p[1] = p[2] = 0; continue; }
        for (int k = 0; k < 3; k++) { int vert_to_edge_array = ((int)p[k] * 255 + p[3] / 2) / p[3]; p[k] = (uint8_t)(vert_to_edge_array > 255 ? 255 : vert_to_edge_array); }
    }
    free(tmp);
    return true;
}

bool paint_map_export(const paint_map* src, const pmap_export_settings* s, paint_map* out)
{
    if (!src || !src->texels || !s || !out || src->kind != s->kind) return false;
    if (s->resolution < 16 || s->resolution > max_size_of(src->kind)) return false;
    int w = s->resolution;
    int h = (int)((long)w * src->height / src->width);
    if (h < 1) h = 1;
    if (h > max_size_of(src->kind)) return false;

    paint_map d;
    if (!paint_map_create(&d, src->name, (enum_pmap_kind)src->kind, w, h)) return false;
    d.id = src->id;
    if (src->kind == pmap_kind_id) {
        d.region_count = src->region_count;                                  /* same palette: ids and colours must not change */
        memcpy(d.regions, src->regions, sizeof(d.regions));
        resample_id(src, &d);
    } else {
        resample_paint(src, &d);
        if (s->softness > 0.0f && !soften_paint(&d, fminf(s->softness, 1.0f))) { paint_map_free(&d); return false; }
    }
    d.revision = 1;
    *out = d;
    return true;
}

bool paint_map_export_to_file(const char* path, paint_map* src, const pmap_export_settings* s)
{
    paint_map out;
    if (!paint_map_export(src, s, &out)) return false;
    const bool ok = paint_map_save(path, &out);
    paint_map_free(&out);
    if (ok) src->saved_revision = src->revision;
    return ok;
}

/* ------------------------------------------------------------------------- */
/* file                                                                      */
/* ------------------------------------------------------------------------- */
bool paint_map_save(const char* path, paint_map* mesh)
{
    if (!path || !mesh || !mesh->texels) return false;
    const size_t bpp = (size_t)bytes_per_texel(mesh->kind), total = (size_t)mesh->width * mesh->height;

    size_t rle_bytes = 0;                                                    /* pick whichever is smaller: painted detail does not compress */
    for (size_t i = 0; i < total;) {
        size_t run = 1;
        while (i + run < total && run < 65535 && memcmp(&mesh->texels[i * bpp], &mesh->texels[(i + run) * bpp], bpp) == 0) run++;
        rle_bytes += 2 + bpp;
        i += run;
    }
    const bool use_rle = rle_bytes < total * bpp;

    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite("PMAP", 1, 4, f);
    put_u32(f, PMAP_FILE_VERSION);
    put_u32(f, mesh->id);
    fwrite(mesh->name, 1, pmap_name_len, f);
    put_u16(f, mesh->width);
    put_u16(f, mesh->height);
    fputc(mesh->kind == pmap_kind_id ? mesh->region_count : 0, f);
    fputc(mesh->kind, f);
    fputc(use_rle ? 0 : 1, f);
    fputc(0, f);
    for (int i = 0; mesh->kind == pmap_kind_id && i < mesh->region_count; i++) {
        fputc(mesh->regions[i].id, f);
        fwrite(mesh->regions[i].name, 1, pmap_name_len, f);
        fwrite(mesh->regions[i].color, 1, 4, f);
    }
    if (use_rle) {
        for (size_t i = 0; i < total;) {
            size_t run = 1;
            while (i + run < total && run < 65535 && memcmp(&mesh->texels[i * bpp], &mesh->texels[(i + run) * bpp], bpp) == 0) run++;
            put_u16(f, (uint16_t)run);
            fwrite(&mesh->texels[i * bpp], 1, bpp, f);
            i += run;
        }
    } else {
        fwrite(mesh->texels, 1, total * bpp, f);
    }

    const bool ok = (fflush(f) == 0) && !ferror(f);
    fclose(f);
    if (!ok) { remove(path); return false; }            /* no half written file left behind to be loaded as a broken map */
    mesh->saved_revision = mesh->revision;
    return true;
}

bool paint_map_load(const char* path, paint_map* out)
{
    if (!path || !out) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    paint_map mesh;
    memset(&mesh, 0, sizeof(mesh));
    char magic[4];
    uint32_t version = 0;
    uint16_t w = 0, h = 0;
    uint8_t face_type = 0, kind = 0, storage = 0, skip;
    bool ok = fread(magic, 1, 4, f) == 4 && memcmp(magic, "PMAP", 4) == 0
           && get_u32(f, &version) && version >= 1 && version <= PMAP_FILE_VERSION
           && get_u32(f, &mesh.id)
           && fread(mesh.name, 1, pmap_name_len, f) == pmap_name_len
           && get_u16(f, &w) && get_u16(f, &h) && get_u8(f, &face_type) && get_u8(f, &kind) && get_u8(f, &storage) && get_u8(f, &skip)
           && (kind == pmap_kind_id || kind == pmap_kind_paint) && storage <= 1
           && w >= 1 && h >= 1 && w <= max_size_of(kind) && h <= max_size_of(kind) && face_type <= pmap_max_regions
           && (kind == pmap_kind_id || face_type == 0);
    mesh.name[pmap_name_len - 1] = 0;
    mesh.kind = kind;

    for (int i = 0; ok && i < face_type; i++) {
        pmap_region* r = &mesh.regions[i];
        ok = get_u8(f, &r->id) && r->id != 0 && !paint_map_find_region(&mesh, r->id)
          && fread(r->name, 1, pmap_name_len, f) == pmap_name_len
          && fread(r->color, 1, 4, f) == 4;
        r->name[pmap_name_len - 1] = 0;
        mesh.region_count = i + 1;
    }

    const size_t bpp = (size_t)bytes_per_texel(kind), total = (size_t)w * h;
    if (ok) { mesh.texels = (uint8_t*)malloc(total * bpp); ok = mesh.texels != NULL; }
    if (ok && storage == 1) ok = fread(mesh.texels, 1, total * bpp, f) == total * bpp;
    size_t filled = 0;
    while (ok && storage == 0 && filled < total) {
        uint16_t run; uint8_t value[4];
        ok = get_u16(f, &run) && fread(value, 1, bpp, f) == bpp && run > 0 && filled + run <= total;
        if (ok && kind == pmap_kind_id && value[0] != 0 && !paint_map_find_region(&mesh, value[0])) ok = false;   /* texel with no palette entry = corrupt */
        for (size_t i = 0; ok && i < run; i++) memcpy(&mesh.texels[(filled + i) * bpp], value, bpp);
        if (ok) filled += run;
    }
    if (ok && storage == 1 && kind == pmap_kind_id)
        for (size_t i = 0; ok && i < total; i++) if (mesh.texels[i] != 0 && !paint_map_find_region(&mesh, mesh.texels[i])) ok = false;
    fclose(f);

    if (!ok) { free(mesh.texels); return false; }
    mesh.width = w;
    mesh.height = h;
    mesh.revision = mesh.saved_revision = 1;                  /* loaded = clean */
    *out = mesh;
    return true;
}