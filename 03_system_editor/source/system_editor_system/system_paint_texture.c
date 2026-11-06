#include "system_globals.h"

/* default colour of a fresh texture: the same grey as an unpainted mesh (sRGB 0.55) */
#define PAINT_BASE_GREY 140

static paint_texture* s_tex[max_scene_objects];

/* ------------------------------------------------------------------------- */
/* storage                                                                   */
/* ------------------------------------------------------------------------- */
paint_texture* paint_texture_get(uint32_t object_id, bool create)
{
    int free_slot = -1;
    for (int i = 0; i < max_scene_objects; ++i) {
        if (s_tex[i] && s_tex[i]->object_id == object_id) return s_tex[i];
        if (!s_tex[i] && free_slot < 0) free_slot = i;
    }
    if (!create || free_slot < 0) return NULL;

    paint_texture* t = (paint_texture*)calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->width = t->height = PAINT_TEXTURE_SIZE;
    t->pixels = (uint8_t*)malloc((size_t)t->width * (size_t)t->height * 4u);
    if (!t->pixels) { free(t); return NULL; }
    for (size_t i = 0; i < (size_t)t->width * (size_t)t->height; ++i) {
        t->pixels[i * 4 + 0] = PAINT_BASE_GREY;
        t->pixels[i * 4 + 1] = PAINT_BASE_GREY;
        t->pixels[i * 4 + 2] = PAINT_BASE_GREY;
        t->pixels[i * 4 + 3] = 255;
    }
    t->object_id = object_id;
    t->dirty = true;
    s_tex[free_slot] = t;
    return t;
}

void paint_texture_remove(uint32_t object_id)
{
    for (int i = 0; i < max_scene_objects; ++i) {
        if (s_tex[i] && s_tex[i]->object_id == object_id) {
            free(s_tex[i]->pixels);
            free(s_tex[i]);
            s_tex[i] = NULL;
        }
    }
}

void paint_texture_shutdown(void)
{
    for (int i = 0; i < max_scene_objects; ++i) {
        if (s_tex[i]) { free(s_tex[i]->pixels); free(s_tex[i]); s_tex[i] = NULL; }
    }
}

/* ------------------------------------------------------------------------- */
/* brush                                                                     */
/* ------------------------------------------------------------------------- */
static float clamp01(float vert_to_edge_array) { return vert_to_edge_array < 0.0f ? 0.0f : (vert_to_edge_array > 1.0f ? 1.0f : vert_to_edge_array); }
static uint8_t to_byte(float vert_to_edge_array) { return (uint8_t)(clamp01(vert_to_edge_array) * 255.0f + 0.5f); }

bool paint_texture_dab(uint32_t object_id, const float uv[2], const float uv_min[2], const float uv_max[2],
                       float uv_per_m, int brush, bool erase_now)
{
    if (!uv || !uv_min || !uv_max || uv_per_m <= 0.0f) return false;
    paint_texture* t = paint_texture_get(object_id, true);
    if (!t) return false;

    if (brush == 2) erase_now = true;
    if (!erase_now && brush != 0 && brush != 1 && brush != 7 && brush != 8) return false;   /* smudge / blur / fill: not on textures yet */

    const float w = (float)t->width, h = (float)t->height;
    const float cx = uv[0] * w, cy = uv[1] * h;
    float r = g_paint.radius * uv_per_m * w;               /* texel radius (the atlas is square, so one number is enough) */
    if (r < 0.75f) r = 0.75f;                              /* never smaller than about one texel */

    /* the dab is clipped to the face it started on */
    int x0 = (int)floorf(cx - r), x1 = (int)ceilf(cx + r);
    int y0 = (int)floorf(cy - r), y1 = (int)ceilf(cy + r);
    const int fx0 = (int)floorf(uv_min[0] * w), fx1 = (int)ceilf(uv_max[0] * w) - 1;
    const int fy0 = (int)floorf(uv_min[1] * h), fy1 = (int)ceilf(uv_max[1] * h) - 1;
    if (x0 < fx0) x0 = fx0;
    if (x1 > fx1) x1 = fx1;
    if (y0 < fy0) y0 = fy0;
    if (y1 > fy1) y1 = fy1;
    if (x0 < 0) x0 = 0;
    if (x1 > t->width  - 1) x1 = t->width  - 1;
    if (y0 < 0) y0 = 0;
    if (y1 > t->height - 1) y1 = t->height - 1;

    const float hard = g_paint.hardness;                   /* 0..0.99: share of the radius that is full strength */
    float strength = g_paint.opacity;
    if (brush == 1) strength *= 0.25f;                     /* airbrush: soft build up */

    const uint8_t pr = to_byte(g_paint.color[0]), pg = to_byte(g_paint.color[1]), pb = to_byte(g_paint.color[2]);
    bool changed = false;

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = ((float)x + 0.5f) - cx, dy = ((float)y + 0.5f) - cy;
            const float d = sqrtf(dx * dx + dy * dy) / r;
            if (d > 1.0f) continue;

            float falloff = 1.0f;
            if (d > hard) {
                const float k = (d - hard) / (1.0f - hard);
                falloff = 1.0f - k * k * (3.0f - 2.0f * k);        /* smoothstep */
            }
            const float a = clamp01(falloff * strength);
            if (a <= 0.0f) continue;

            uint8_t* p = &t->pixels[((size_t)y * (size_t)t->width + (size_t)x) * 4u];
            if (brush == 7) {                                      /* lighten */
                for (int c = 0; c < 3; ++c) p[c] = (uint8_t)(p[c] + (255 - p[c]) * a * 0.3f + 0.5f);
            } else if (brush == 8) {                               /* darken */
                for (int c = 0; c < 3; ++c) p[c] = (uint8_t)(p[c] - p[c] * a * 0.3f + 0.5f);
            } else if (erase_now) {                                /* back to the base grey */
                p[0] = (uint8_t)(p[0] + (PAINT_BASE_GREY - p[0]) * a + 0.5f);
                p[1] = (uint8_t)(p[1] + (PAINT_BASE_GREY - p[1]) * a + 0.5f);
                p[2] = (uint8_t)(p[2] + (PAINT_BASE_GREY - p[2]) * a + 0.5f);
            } else {                                               /* paint / airbrush */
                p[0] = (uint8_t)(p[0] + (pr - p[0]) * a + 0.5f);
                p[1] = (uint8_t)(p[1] + (pg - p[1]) * a + 0.5f);
                p[2] = (uint8_t)(p[2] + (pb - p[2]) * a + 0.5f);
            }
            changed = true;
        }
    }

    if (changed) { t->dirty = true; t->revision++; }
    return changed;
}

bool paint_texture_pick(uint32_t object_id, const float uv[2], float rgb[3])
{
    paint_texture* t = paint_texture_get(object_id, false);
    if (!t || !uv || !rgb) return false;
    int x = (int)(uv[0] * (float)t->width), y = (int)(uv[1] * (float)t->height);
    if (x < 0) x = 0;
    if (x > t->width  - 1) x = t->width  - 1;
    if (y < 0) y = 0;
    if (y > t->height - 1) y = t->height - 1;
    const uint8_t* p = &t->pixels[((size_t)y * (size_t)t->width + (size_t)x) * 4u];
    rgb[0] = (float)p[0] / 255.0f; rgb[1] = (float)p[1] / 255.0f; rgb[2] = (float)p[2] / 255.0f;
    return true;
}

/* ------------------------------------------------------------------------- */
/* PNG writer: signature, IHDR, sRGB, IDAT (zlib "stored" blocks), IEND       */
/* ------------------------------------------------------------------------- */
static uint32_t s_crc_table[256];
static bool     s_crc_ready = false;

static uint32_t crc32_update(uint32_t crc, const uint8_t* buf, size_t len)
{
    if (!s_crc_ready) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            s_crc_table[n] = c;
        }
        s_crc_ready = true;
    }
    uint32_t c = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) c = s_crc_table[(c ^ buf[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void put32(FILE* f, uint32_t vert_to_edge_array)
{
    const uint8_t b[4] = { (uint8_t)(vert_to_edge_array >> 24), (uint8_t)(vert_to_edge_array >> 16), (uint8_t)(vert_to_edge_array >> 8), (uint8_t)vert_to_edge_array };
    fwrite(b, 1, 4, f);
}

static void write_chunk(FILE* f, const char type[4], const uint8_t* data, uint32_t len)
{
    put32(f, len);
    fwrite(type, 1, 4, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t crc = crc32_update(0, (const uint8_t*)type, 4);
    if (len) crc = crc32_update(crc, data, len);
    put32(f, crc);
}

bool paint_texture_export_png(uint32_t object_id, const char* path)
{
    const paint_texture* t = paint_texture_get(object_id, false);
    if (!t || !path) return false;

    const size_t row  = 1u + (size_t)t->width * 4u;                 /* filter byte + RGBA */
    const size_t raw  = row * (size_t)t->height;
    const size_t blks = (raw + 65534u) / 65535u;
    const size_t zlen = 2u + blks * 5u + raw + 4u;                  /* zlib header + block headers + data + adler32 */

    uint8_t* rawbuf = (uint8_t*)malloc(raw);
    uint8_t* z      = (uint8_t*)malloc(zlen);
    if (!rawbuf || !z) { free(rawbuf); free(z); return false; }

    for (int y = 0; y < t->height; ++y) {
        uint8_t* dst = rawbuf + (size_t)y * row;
        dst[0] = 0;                                                  /* filter: none */
        memcpy(dst + 1, &t->pixels[(size_t)y * (size_t)t->width * 4u], (size_t)t->width * 4u);
    }

    size_t zp = 0;
    z[zp++] = 0x78; z[zp++] = 0x01;
    for (size_t off = 0; off < raw; ) {
        const size_t n = (raw - off > 65535u) ? 65535u : (raw - off);
        z[zp++] = (off + n >= raw) ? 1 : 0;                          /* final block flag, stored */
        z[zp++] = (uint8_t)(n & 0xFF);          z[zp++] = (uint8_t)(n >> 8);
        z[zp++] = (uint8_t)(~n & 0xFF);         z[zp++] = (uint8_t)((~n >> 8) & 0xFF);
        memcpy(z + zp, rawbuf + off, n);
        zp += n; off += n;
    }
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; ++i) { a = (a + rawbuf[i]) % 65521u; b = (b + a) % 65521u; }
    const uint32_t adler = (b << 16) | a;
    z[zp++] = (uint8_t)(adler >> 24); z[zp++] = (uint8_t)(adler >> 16); z[zp++] = (uint8_t)(adler >> 8); z[zp++] = (uint8_t)adler;
    free(rawbuf);

    FILE* f = fopen(path, "wb");
    if (!f) { free(z); return false; }

    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    fwrite(sig, 1, 8, f);

    uint8_t ihdr[13] = {
        (uint8_t)((uint32_t)t->width >> 24),  (uint8_t)((uint32_t)t->width >> 16),  (uint8_t)((uint32_t)t->width >> 8),  (uint8_t)t->width,
        (uint8_t)((uint32_t)t->height >> 24), (uint8_t)((uint32_t)t->height >> 16), (uint8_t)((uint32_t)t->height >> 8), (uint8_t)t->height,
        8, 6, 0, 0, 0                                                /* 8 bit, RGBA, deflate, adaptive filter, no interlace */
    };
    write_chunk(f, "IHDR", ihdr, 13);
    const uint8_t srgb = 0;                                          /* perceptual: the pixels are sRGB */
    write_chunk(f, "sRGB", &srgb, 1);
    write_chunk(f, "IDAT", z, (uint32_t)zp);
    write_chunk(f, "IEND", NULL, 0);

    const bool ok = (fflush(f) == 0) && (ferror(f) == 0);
    fclose(f);
    free(z);
    return ok;
}

bool paint_texture_export_for_object(uint32_t object_id, const char* object_name, char* out_path, size_t out_size)
{
    if (!out_path || out_size < 16) return false;
    char clean[64];
    size_t n = 0;
    for (const char* s = object_name ? object_name : "object"; *s && n < sizeof(clean) - 1; ++s) {
        const char c = *s;
        clean[n++] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') ? c : '_';
    }
    clean[n] = 0;
    if (n == 0) snprintf(clean, sizeof(clean), "object");
    snprintf(out_path, out_size, "paint_%s.png", clean);
    return paint_texture_export_png(object_id, out_path);
}