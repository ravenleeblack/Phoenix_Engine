#include "engine_globals.h"

/* g_light is defined in engine_globals.c:
   light_settings g_light = { 0.20f, true, terrain_layer_atmosphere, 1.5f, 0.0f };
   clamp_int_value() lives in engine_math.c / engine_math.h. */

/* warm = low sun / sunset orange, cool = overcast / moonlight blue */
void apply_light_warmth(float rgb[3], float warmth)
{
    const float warm[3] = { 1.00f, 0.72f, 0.42f };
    const float cool[3] = { 0.60f, 0.78f, 1.00f };

    float w = warmth;
    if (w < -1.0f) w = -1.0f;
    if (w >  1.0f) w =  1.0f;

    const float* target = warm;
    float t = w;
    if (w < 0.0f) {
        target = cool;
        t = -w;
    }
    for (int i = 0; i < 3; i++) rgb[i] *= 1.0f + (target[i] - 1.0f) * t;
}

/* the sun's color before the ambient split: its own color with the warm / cool tint, and the intensity in out[3].
   the sky and the fog use this so they follow the directional light's color, warmth and intensity. */
void get_light_sun_tint(float out[4])
{
    float rgb[3] = { 1.0f, 1.0f, 1.0f };
    float inten  = g_light.sun_intensity;
    float warmth = g_light.sun_warmth;
    float limit  = 3.0f;

    if (g_light.sun_driven) {
        rgb[0] = g_light.sun_driver_color[0];
        rgb[1] = g_light.sun_driver_color[1];
        rgb[2] = g_light.sun_driver_color[2];
        inten  = g_light.sun_driver_intensity;
        warmth = g_light.sun_driver_warmth;
        limit  = 4.0f;
    }
    if (inten < 0.0f) inten = 0.0f;
    if (inten > limit) inten = limit;
    apply_light_warmth(rgb, warmth);
    out[0] = rgb[0];
    out[1] = rgb[1];
    out[2] = rgb[2];
    out[3] = inten;
}

/* the sun colour the mesh shader gets. a directional light object (sun_driven) supplies its own colour, intensity and warmth,
   otherwise the Lighting window's Brightness and Warm / Cool sliders do. */
void get_light_sun_color(float ambient, float out[4])
{
    float rgb[3] = { 1.0f, 1.0f, 1.0f };
    float inten  = g_light.sun_intensity;
    float warmth = g_light.sun_warmth;
    float limit  = 3.0f;

    if (g_light.sun_driven) {
        rgb[0] = g_light.sun_driver_color[0];
        rgb[1] = g_light.sun_driver_color[1];
        rgb[2] = g_light.sun_driver_color[2];
        inten  = g_light.sun_driver_intensity;
        warmth = g_light.sun_driver_warmth;
        limit  = 4.0f;
    }
    if (inten < 0.0f) inten = 0.0f;
    if (inten > limit) inten = limit;
    apply_light_warmth(rgb, warmth);

    float fill = 1.0f - ambient;            /* the old formula was ambient + (1 - ambient) * facing */
    if (fill < 0.0f) fill = 0.0f;

    for (int i = 0; i < 3; i++) out[i] = rgb[i] * inten * fill;
    out[3] = 0.0f;
}

unsigned char pack_light_value(int sky, int block)
{
    sky   = clamp_int_value(sky, 0, 15);
    block = clamp_int_value(block, 0, 15);
    return (unsigned char)((block << 4) | (15 - sky));
}

terrain_chunk* get_terrain_light_chunk(const system_terrain* t, int cx, int cz)
{
    const terrain_layer* layer = &t->layers[terrain_light_layer];
    if (!layer->chunks || cx < 0 || cz < 0 || cx >= t->dims.chunks_x || cz >= t->dims.chunks_z) return NULL;
    return &layer->chunks[cz * t->dims.chunks_x + cx];
}

/* must match chunk_local_index() in the terrain source */
size_t get_terrain_light_index(const system_terrain* t, int lx, int ly, int lz)
{
    return ((size_t)lz * t->dims.cells_y + ly) * t->dims.chunk_cells + lx;
}

bool get_terrain_lighting_enabled(void)
{
    return g_light.enabled;
}

/* update_terrain_streaming() rebuilds the resident chunks next frame */
void rebuild_terrain_light_all(void)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return;
    terrain_chunk* tc = t->layers[terrain_layer_terrain].chunks;
    if (!tc) return;

    const int n = t->dims.chunks_x * t->dims.chunks_z;
    for (int i = 0; i < n; i++) tc[i].dirty = true;
}

int get_terrain_light_layer(void)
{
    return g_light.layer;
}

void set_terrain_light_layer(int layer)
{
    if (layer < 1 || layer >= terrain_max_layers) return;     /* layer 0 is the height grid */
    if (g_light.layer == layer) return;
    g_light.layer = layer;
    rebuild_terrain_light_all();                               /* a different layer is a different set of cells */
}

void clear_terrain_light_layer(void)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return;
    terrain_chunk* lc = t->layers[g_light.layer].chunks;
    if (!lc) return;

    const int n = t->dims.chunks_x * t->dims.chunks_z;
    for (int i = 0; i < n; i++) {
        if (lc[i].cells) { free(lc[i].cells); lc[i].cells = NULL; }
        lc[i].modified = true;
    }
    rebuild_terrain_light_all();
}

/* Meshes bake the light into vertex colour, so every terrain chunk has to be rebuilt when the mode flips. */
void set_terrain_lighting_enabled(bool enabled)
{
    if (g_light.enabled == enabled) return;
    g_light.enabled = enabled;
    rebuild_terrain_light_all();
}

void get_terrain_light(const system_terrain* t, int gx, int gy, int gz, int* out_sky, int* out_block)
{
    int sky = 15, block = 0;
    if (t && t->exists && gx >= 0 && gy >= 0 && gz >= 0 &&
        gx < t->dims.cells_x && gy < t->dims.cells_y && gz < t->dims.cells_z) {
        const int cc = t->dims.chunk_cells;
        const terrain_chunk* c = get_terrain_light_chunk(t, gx / cc, gz / cc);
        if (c && c->cells) {
            uint8_t packed = c->cells[get_terrain_light_index(t, gx % cc, gy, gz % cc)];
            sky   = 15 - (packed & 15);
            block = packed >> 4;
        }
    }
    if (out_sky)   *out_sky   = sky;
    if (out_block) *out_block = block;
}

bool set_terrain_light_raw(system_terrain* t, int gx, int gy, int gz, int sky, int block)
{
    if (gx < 0 || gy < 0 || gz < 0 || gx >= t->dims.cells_x || gy >= t->dims.cells_y || gz >= t->dims.cells_z) return false;

    const int cc = t->dims.chunk_cells;
    terrain_chunk* c = get_terrain_light_chunk(t, gx / cc, gz / cc);
    if (!c) return false;

    if (!c->cells) {
        c->cells = (uint8_t*)calloc((size_t)cc * cc * t->dims.cells_y, 1);
        if (!c->cells) {
            phoenix_engine_error("Lighting", "%s", "Out of memory allocating light cells for a chunk.");
            return false;
        }
    }
    c->cells[get_terrain_light_index(t, gx % cc, gy, gz % cc)] = pack_light_value(sky, block);
    c->modified = true;
    return true;
}

/* Terrain meshes read light from the neighbouring cell, so rebuild the chunks around the change. */
void mark_terrain_light_dirty(system_terrain* t, int gx0, int gz0, int gx1, int gz1)
{
    terrain_chunk* tc = t->layers[terrain_layer_terrain].chunks;
    if (!tc) return;
    const int cc = t->dims.chunk_cells;
    gx0 = clamp_int_value(gx0 - 1, 0, t->dims.cells_x - 1);
    gz0 = clamp_int_value(gz0 - 1, 0, t->dims.cells_z - 1);
    gx1 = clamp_int_value(gx1 + 1, 0, t->dims.cells_x - 1);
    gz1 = clamp_int_value(gz1 + 1, 0, t->dims.cells_z - 1);
    for (int cz = gz0 / cc; cz <= gz1 / cc; cz++) {
        for (int cx = gx0 / cc; cx <= gx1 / cc; cx++) {
            tc[cz * t->dims.chunks_x + cx].dirty = true;
        }
    }
}

bool set_terrain_light(int gx, int gy, int gz, int sky, int block)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return false;
    if (!set_terrain_light_raw(t, gx, gy, gz, sky, block)) return false;
    mark_terrain_light_dirty(t, gx, gz, gx, gz);
    return true;
}

void fill_terrain_light_box(int x0, int y0, int z0, int x1, int y1, int z1, int sky, int block)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return;
    for (int z = z0; z < z1; z++) {
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                set_terrain_light_raw(t, x, y, z, sky, block);
            }
        }
    }
    mark_terrain_light_dirty(t, x0, z0, x1 - 1, z1 - 1);
}

void apply_terrain_light_to_mesh(const system_terrain* t, editor_vertex* verts, uint32_t vertex_count)
{
    if (!g_light.enabled) return;
    if (!t || !t->exists || !verts) return;
    const terrain_chunk* light_chunks = t->layers[terrain_light_layer].chunks;
    if (!light_chunks) return;

    /* nothing stored anywhere = everything is fully lit, skip the whole pass */
    bool any = false;
    const int n = t->dims.chunks_x * t->dims.chunks_z;
    for (int i = 0; i < n; i++) {
        if (light_chunks[i].cells) { any = true; break; }
    }
    if (!any) return;

    const float cs     = t->settings.cell_size_m;
    const float half_y = (float)t->dims.cells_y * 0.5f;

    for (uint32_t q = 0; q + 3 < vertex_count; q += 4) {
        editor_vertex* quad = &verts[q];

        /* outward face normal (faces are counter-clockwise from outside) */
        float ax = quad[1].x - quad[0].x, ay = quad[1].y - quad[0].y, az = quad[1].z - quad[0].z;
        float bx = quad[2].x - quad[0].x, by = quad[2].y - quad[0].y, bz = quad[2].z - quad[0].z;
        float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        float len = sqrtf(nx * nx + ny * ny + nz * nz);
        if (len < 1e-6f) continue;
        nx /= len; ny /= len; nz /= len;

        /* sample the air cell just in front of the face centre */
        float px = (quad[0].x + quad[1].x + quad[2].x + quad[3].x) * 0.25f + nx * cs * 0.5f;
        float py = (quad[0].y + quad[1].y + quad[2].y + quad[3].y) * 0.25f + ny * cs * 0.5f;
        float pz = (quad[0].z + quad[1].z + quad[2].z + quad[3].z) * 0.25f + nz * cs * 0.5f;
        int gx = (int)floorf(px / cs + (float)t->dims.origin_x);
        int gy = (int)floorf(py / cs + half_y);
        int gz = (int)floorf(pz / cs + (float)t->dims.origin_z);

        int sky, block;
        get_terrain_light(t, gx, gy, gz, &sky, &block);
        int level = sky;
        if (block > level) level = block;
        if (level >= 15) continue;

        float scale = g_light.min_light + (1.0f - g_light.min_light) * ((float)level / 15.0f);
        for (int k = 0; k < 4; k++) {
            quad[k].r *= scale;
            quad[k].g *= scale;
            quad[k].b *= scale;
        }
    }
}