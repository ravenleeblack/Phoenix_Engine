#include "engine_globals.h"

#define s_regions_changed g_terrain_regions_changed
#define s_h g_terrain_region_heights
#define s_c g_terrain_region_colors
#define s_built_since_log g_terrain_built_since_log
#define s_edit g_edit_mesh
#define s_edit_have g_edit_mesh_have
#define s_ecx g_edit_chunk_x
#define s_ecz g_edit_chunk_z
#define s_estride g_edit_stride






/* ------------------------------------------------------------------------- */
/* settings / dimensions                                                     */
/* ------------------------------------------------------------------------- */
void terrain_settings_defaults(terrain_settings* s)
{
    memset(s, 0, sizeof(*s));
    s->mode          = terrain_mode_sandbox;    /* cube columns */
    s->region_chunks = terrain_default_region_chunks;
    s->chunk_cells   = terrain_chunk_cells;
    s->cells_x       = s->region_chunks * terrain_region_grid * terrain_chunk_cells;
    s->cells_z       = s->cells_x;
    s->cells_y       = 400;                     /* 400 cubes high */
    s->cell_size_m   = system_unit_m;           /* one cell = one 4 mesh cube */
    s->load_radius   = 3;                       /* chunks around the camera chunk: 3 = 7 x 7 chunks of 100 x 100 cubes = 2.8 km square (49 x 40k = ~2m verts). 0 = camera chunk only */
    s->edit_stride   = 1;                       /* edit mode: one vertex per cell */

    s->smooth_iterations = 2; s->smooth_strength = 0.5f; s->smooth_max_offset = 0.5f;   /* legacy */
    s->hs_max_slope_deg  = 89.0f; s->hs_blur_strength = 0.5f; s->hs_slope_iterations = 12;

    s->sun_azimuth_deg   = 135.0f;
    s->sun_elevation_deg = 40.0f;
    s->shade_amount      = 0.8f;
    s->rock_slope_deg    = 38.0f;
    s->sand_line_m       = -12.0f;
    s->snow_line_m       = 320.0f;
}

terrain_dims terrain_resolve_dims(const terrain_settings* s)
{
    terrain_dims d;
    memset(&d, 0, sizeof(d));
    float cs = system_unit_m;
    if (s->cell_size_m >= 0.0625f) cs = s->cell_size_m;
    d.region_chunks = clampi(s->region_chunks, 1, terrain_max_region_chunks);
    d.chunk_cells   = terrain_chunk_cells;
    d.region_cells  = d.region_chunks * d.chunk_cells;
    d.chunks_x      = d.region_chunks * terrain_region_grid;
    d.chunks_z      = d.chunks_x;
    d.cells_x       = d.chunks_x * d.chunk_cells;
    d.cells_z       = d.cells_x;
    d.cells_y       = imax(16, s->cells_y);
    d.evenly_divisible = true;
    d.origin_x      = d.cells_x / 2;       /* world (0,0) = the middle of region_04 */
    d.origin_z      = d.cells_z / 2;
    d.size_x_m      = d.cells_x * cs;
    d.size_y_m      = d.cells_y * cs;
    d.size_z_m      = d.cells_z * cs;
    return d;
}

const char* get_terrain_layer_name(int layer)
{
    static const char* names[terrain_max_layers] = {
        "1 terrain", "2 trees", "3 grass", "4 buildings", "5 props",
        "6 fog / atmosphere / lighting", "7 underground tunnels",
        "8 tunnel lighting", "9 npcs", "10 npcs"
    };
    if (layer >= 0 && layer < terrain_max_layers) return names[layer];
    return "?";
}

void terrain_region_name(int region, char* out, size_t out_size)
{
    if (!out || out_size == 0) return;
    snprintf(out, out_size, "region_%02d", clampi(region, 0, terrain_region_count - 1));
}

terrain_chunk* chunk_ptr(const system_terrain* t, int layer, int cx, int cz)
{
    const terrain_layer* layer_grid = &t->layers[layer];
    if (!layer_grid->chunks || cx < 0 || cz < 0 || cx >= t->dims.chunks_x || cz >= t->dims.chunks_z) return NULL;
    return &layer_grid->chunks[cz * t->dims.chunks_x + cx];
}

int region_of_chunk(const system_terrain* t, int cx, int cz)
{
    int rx = imin(cx / t->dims.region_chunks, terrain_region_grid - 1);
    int rz = imin(cz / t->dims.region_chunks, terrain_region_grid - 1);
    return rz * terrain_region_grid + rx;
}

/* ------------------------------------------------------------------------- */
/* region on / off. a region that is off gets no chunk meshes at all: nothing    */
/* is built, uploaded or drawn for it, which is the big performance win.         */
/* ------------------------------------------------------------------------- */


bool chunk_region_on(const system_terrain* t, int cx, int cz)
{
    return !t->region_off[region_of_chunk(t, cx, cz)];
}

int get_terrain_region_of_object(uint32_t id)
{
    if (!g_terrain.exists || id == 0) return -1;
    for (int r = 0; r < terrain_region_count; r++) if (g_terrain.region_ids[r] == id) return r;
    return -1;
}

int get_terrain_camera_region(void)
{
    const system_terrain* t = &g_terrain;
    if (!t->exists || !t->region_valid) return -1;
    return region_of_chunk(t, t->center_cx, t->center_cz);
}

bool is_terrain_region_enabled(int r)
{
    return g_terrain.exists && r >= 0 && r < terrain_region_count && !g_terrain.region_off[r];
}

void set_terrain_region_enabled(int r, bool on)
{
    system_terrain* t = &g_terrain;
    if (!t->exists || r < 0 || r >= terrain_region_count) return;
    if (t->region_off[r] == !on) return;
    t->region_off[r] = !on;
    const char* state_text = "off";
    if (on) state_text = "on";
    phoenix_engine_info("terrain", "region %d switched %s", r, state_text);
    t->region_dirty[r] = true;
    s_regions_changed = true;
}

void solo_terrain_region(int r)
{
    for (int k = 0; k < terrain_region_count; k++) set_terrain_region_enabled(k, k == r);
}

void terrain_region_show_all(void)
{
    for (int k = 0; k < terrain_region_count; k++) set_terrain_region_enabled(k, true);
}




void refresh_height_cache(const system_terrain* t)
{
    for (int r = 0; r < terrain_region_count; r++) {
        s_h[r] = NULL; s_c[r] = NULL;
        if (!t->exists || !t->region_ids[r]) continue;
    }
}

float limit_m(const system_terrain* t) { return t->dims.size_y_m * 0.5f - 2.0f * t->settings.cell_size_m; }

float corner_h(const system_terrain* t, int gx, int gz)
{
    gx = clampi(gx, 0, t->dims.cells_x);
    gz = clampi(gz, 0, t->dims.cells_z);
    const int rc = t->dims.region_cells;
    int rx = imin(gx / rc, terrain_region_grid - 1), rz = imin(gz / rc, terrain_region_grid - 1);
    const float* h = s_h[rz * terrain_region_grid + rx];
    if (!h) return 0.0f;
    return h[(size_t)(gz - rz * rc) * (size_t)(rc + 1) + (size_t)(gx - rx * rc)];
}

/* height of the surface over cell (gx,gz): sandbox = the column top (a whole cube), openworld = average of the 4 corners */
float get_cell_surface_height(const system_terrain* t, int gx, int gz)
{
    if (t->settings.mode == terrain_mode_sandbox) return sandbox_snap(corner_h(t, gx, gz), t->settings.cell_size_m);
    return (corner_h(t, gx, gz) + corner_h(t, gx + 1, gz) + corner_h(t, gx, gz + 1) + corner_h(t, gx + 1, gz + 1)) * 0.25f;
}

float terrain_corner_height(int gx, int gz)
{
    if (!g_terrain.exists) return 0.0f;
    return corner_h(&g_terrain, gx, gz);
}

void terrain_set_corner_height(int gx, int gz, float y)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return;
    if (gx < 0 || gz < 0 || gx > t->dims.cells_x || gz > t->dims.cells_z) return;
    const int rc = t->dims.region_cells;
    int rx = imin(gx / rc, terrain_region_grid - 1), rz = imin(gz / rc, terrain_region_grid - 1);
    float* h = s_h[rz * terrain_region_grid + rx];
    if (!h) return;

    if (t->settings.mode == terrain_mode_sandbox) {
        float unit = t->settings.cell_size_m / (float)terrain_substeps;
        y = floorf(y / unit + 0.5f) * unit;
    }
    y = clampf(y, -limit_m(t), limit_m(t));
    h[(size_t)(gz - rz * rc) * (size_t)(rc + 1) + (size_t)(gx - rx * rc)] = y;
}

float terrain_sample_height_m(float lx, float lz)
{
    const system_terrain* t = &g_terrain;
    if (!t->exists) return 0.0f;
    const float cs = t->settings.cell_size_m;
    float fx = lx / cs + (float)t->dims.origin_x, fz = lz / cs + (float)t->dims.origin_z;
    fx = clampf(fx, 0.0f, (float)t->dims.cells_x); fz = clampf(fz, 0.0f, (float)t->dims.cells_z);
    int gx = (int)floorf(fx), gz = (int)floorf(fz);
    if (t->settings.mode == terrain_mode_sandbox)       /* cubes: you stand on the column top, not on a slope */
        return get_cell_surface_height(t, imin(gx, t->dims.cells_x - 1), imin(gz, t->dims.cells_z - 1));
    float tx = fx - (float)gx, tz = fz - (float)gz;
    float a = lerpf(corner_h(t, gx, gz),     corner_h(t, gx + 1, gz),     tx);
    float b = lerpf(corner_h(t, gx, gz + 1), corner_h(t, gx + 1, gz + 1), tx);
    return lerpf(a, b, tz);
}

/* corner rect changed: every chunk whose cells touch it (and its neighbours, for sandbox walls / normals) is dirty */
void terrain_mark_dirty_rect(int gx0, int gz0, int gx1, int gz1)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return;
    const int cc = t->dims.chunk_cells;
    gx0 = imax(0, gx0 - 2); gz0 = imax(0, gz0 - 2);
    gx1 = imin(t->dims.cells_x - 1, gx1 + 1); gz1 = imin(t->dims.cells_z - 1, gz1 + 1);
    for (int cz = gz0 / cc; cz <= gz1 / cc; cz++)
        for (int cx = gx0 / cc; cx <= gx1 / cc; cx++) {
            terrain_chunk* c = chunk_ptr(t, terrain_layer_terrain, cx, cz);
            if (c) { c->dirty = true; c->modified = true; }
        }
}

void terrain_touch(void)
{
    g_terrain.edited = true;
    g_terrain.edit_revision++;
}

bool terrain_root_position(editor_vec3* out)
{
    const system_scene_object* o = NULL;
    if (g_terrain.exists) o = find_scene_object_by_id(g_terrain.scene_object_id);
    if (!o) return false;
    if (out) *out = o->position;
    return true;
}

/* ------------------------------------------------------------------------- */
/* cells for layers 2..10 (light layer etc.). layer 1 is the height grid.    */
/* ------------------------------------------------------------------------- */
size_t chunk_local_index(const system_terrain* t, int lx, int ly, int lz)
{
    return ((size_t)lz * t->dims.cells_y + ly) * t->dims.chunk_cells + lx;
}

uint8_t terrain_get_cell(const system_terrain* t, int layer, int gx, int gy, int gz)
{
    if (!t->exists || layer < 0 || layer >= terrain_max_layers) return terrain_cell_air;
    if (gy < 0)                 return terrain_cell_stone;
    if (gy >= t->dims.cells_y)  return terrain_cell_air;
    gx = clampi(gx, 0, t->dims.cells_x - 1);
    gz = clampi(gz, 0, t->dims.cells_z - 1);

    if (layer == terrain_layer_terrain) {
        const float cs = t->settings.cell_size_m;
        float h = get_cell_surface_height(t, gx, gz);
        float y = ((float)gy + 0.5f) * cs - t->dims.size_y_m * 0.5f;
        if (y >= h)             return terrain_cell_air;
        if (y + cs >= h) {
            if (h < t->settings.sand_line_m) return terrain_cell_sand;
            return terrain_cell_grass;
        }
        if (y + 3.0f * cs >= h) return terrain_cell_dirt;
        return terrain_cell_stone;
    }
    const int cc = t->dims.chunk_cells;
    const terrain_chunk* c = chunk_ptr(t, layer, gx / cc, gz / cc);
    if (!c || !c->cells) return terrain_cell_air;
    return c->cells[chunk_local_index(t, gx % cc, gy, gz % cc)];
}

bool terrain_set_cell(int layer, int gx, int gy, int gz, uint8_t type)
{
    (void)layer; (void)gx; (void)gy; (void)gz; (void)type;
    return false;      /* the surface is edited through heights; other layers own their storage */
}

void terrain_fill_box(int layer, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t type)
{
    for (int z = z0; z < z1; z++)
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++)
                terrain_set_cell(layer, x, y, z, type);
}

/* ------------------------------------------------------------------------- */
/* surface colour (baked: the mesh shader is unlit)                          */
/* ------------------------------------------------------------------------- */
float srgb_to_linear_terrain(float c)
{
    if (!g_swapchain_is_srgb) return c;
    if (c <= 0.04045f) return c / 12.92f;
    return powf((c + 0.055f) / 1.055f, 2.4f);
}



void surface_color(const terrain_settings* s, float h_m, float slope_deg, const float n[3], float out[3])
{
    (void)n;
#if !terrain_bake_skin_color
    (void)s; (void)h_m; (void)slope_deg;
    out[0] = out[1] = out[2] = skin_empty_grey_linear();
    return;
#endif
    static const float grass[3] = {0.30f, 0.52f, 0.25f};
    static const float stone[3] = {0.52f, 0.52f, 0.54f};
    static const float sand[3]  = {0.78f, 0.70f, 0.48f};
    static const float snow[3]  = {0.93f, 0.95f, 0.97f};
    float c[3] = { grass[0], grass[1], grass[2] };

    float dry = smoothstepf(0.0f, 300.0f, h_m) * 0.25f;
    for (int i = 0; i < 3; i++) c[i] *= (1.0f - dry);

    float ts = smoothstepf(s->sand_line_m + 5.0f, s->sand_line_m - 5.0f, h_m);
    for (int i = 0; i < 3; i++) c[i] = lerpf(c[i], sand[i], ts);

    float tr = smoothstepf(s->rock_slope_deg - 6.0f, s->rock_slope_deg + 6.0f, slope_deg);
    for (int i = 0; i < 3; i++) c[i] = lerpf(c[i], stone[i], tr);

    if (s->snow_line_m < 9000.0f) {
        float tw = smoothstepf(s->snow_line_m, s->snow_line_m + 30.0f, h_m)
                 * (1.0f - smoothstepf(s->rock_slope_deg + 4.0f, s->rock_slope_deg + 22.0f, slope_deg));
        for (int i = 0; i < 3; i++) c[i] = lerpf(c[i], snow[i], tw);
    }

    for (int i = 0; i < 3; i++) out[i] = srgb_to_linear_terrain(c[i]);
}

/* ------------------------------------------------------------------------- */
/* paint layer. one rgba8 value per cube column, per region. a = how much of the */
/* painted colour replaces the baked colour. every face of a column (top and     */
/* walls, 4 verts each) takes that colour when its chunk is meshed.              */
/* ------------------------------------------------------------------------- */
void apply_paint(const system_terrain* t, int gx, int gz, float col[3])
{
    gx = clampi(gx, 0, t->dims.cells_x - 1); gz = clampi(gz, 0, t->dims.cells_z - 1);
    const int rc = t->dims.region_cells;
    const int rx = imin(gx / rc, terrain_region_grid - 1), rz = imin(gz / rc, terrain_region_grid - 1);
    const uint8_t* blk = s_c[rz * terrain_region_grid + rx];
    if (!blk) return;
    const uint8_t* p = blk + ((size_t)(gz - rz * rc) * (size_t)rc + (size_t)(gx - rx * rc)) * 4u;
    if (!p[3]) return;
    const float a = (float)p[3] / 255.0f;
    for (int i = 0; i < 3; i++) col[i] = lerpf(col[i], srgb_to_linear_terrain((float)p[i] / 255.0f), a);
}

bool terrain_paint_dab(editor_vec3 c, float radius, const float color[3], float opacity, float hardness, bool erase, int only_region)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) {
        static bool warned_none = false;
        if (!warned_none) { warned_none = true; phoenix_engine_warn("terrainpaint", "terrain_paint_dab called but no terrain exists."); }
        return false;
    }
    refresh_height_cache(t);

    const float cs = t->settings.cell_size_m;
    if (radius < 0.01f) radius = 0.01f;
    hardness = clampf(hardness, 0.0f, 0.99f);
    opacity  = clampf(opacity, 0.0f, 1.0f);

    const float fx = c.x / cs + (float)t->dims.origin_x, fz = c.z / cs + (float)t->dims.origin_z;
    const int gx0 = clampi((int)floorf(fx - radius / cs), 0, t->dims.cells_x - 1);
    const int gx1 = clampi((int)ceilf (fx + radius / cs), 0, t->dims.cells_x - 1);
    const int gz0 = clampi((int)floorf(fz - radius / cs), 0, t->dims.cells_z - 1);
    const int gz1 = clampi((int)ceilf (fz + radius / cs), 0, t->dims.cells_z - 1);
    const int hgx = (int)floorf(fx), hgz = (int)floorf(fz);       /* the column under the cursor */
    const int rc = t->dims.region_cells;
    static bool warned = false;
    bool changed = false;

    for (int gz = gz0; gz <= gz1; gz++)
        for (int gx = gx0; gx <= gx1; gx++) {
            const float dx = ((float)gx + 0.5f - fx) * cs, dz = ((float)gz + 0.5f - fz) * cs;
            const float dist = sqrtf(dx * dx + dz * dz);
            const bool  hit_cell = (gx == hgx && gz == hgz);       /* the column under the cursor always gets the full dab */
            if (!hit_cell && dist >= radius) continue;

            const int rx = imin(gx / rc, terrain_region_grid - 1), rz = imin(gz / rc, terrain_region_grid - 1);
            const int r = rz * terrain_region_grid + rx;
            if (only_region >= 0 && r != only_region) continue;

            float w = 1.0f;
            const float tt = dist / radius;
            if (!hit_cell && tt > hardness) { const float u = (tt - hardness) / (1.0f - hardness); w = 1.0f - u * u * (3.0f - 2.0f * u); }
            const float a = w * opacity;
            if (a <= 0.0f) continue;

            if (!s_c[r]) {                                    /* first paint in this region: zeroed block = nothing painted */
                if (erase) continue;
                const uint32_t bytes = (uint32_t)rc * (uint32_t)rc * 4u;
                if (!warned) { warned = true; phoenix_engine_warn("terrain", "could not allocate the paint layer of region %d.", r); }
                continue;
                refresh_height_cache(t);
                if (!s_c[r]) continue;
                memset(s_c[r], 0, bytes);
                phoenix_engine_info("terrainpaint", "allocated + zeroed paint layer of region %d (%u bytes)", r, (unsigned)bytes);                     /* object_db_set does not guarantee zeroed memory: stale alpha = random paint */
            }

            uint8_t* p = s_c[r] + ((size_t)(gz - rz * rc) * (size_t)rc + (size_t)(gx - rx * rc)) * 4u;
            uint8_t  n[4] = { p[0], p[1], p[2], p[3] };
            const float old_a = (float)p[3] / 255.0f;
            if (erase) {
                n[3] = (uint8_t)(old_a * (1.0f - a) * 255.0f + 0.5f);
            } else {
                const float new_a = old_a + a * (1.0f - old_a);
                float k = 1.0f;
                if (new_a > 0.0f) k = a / new_a;
                for (int i = 0; i < 3; i++) n[i] = (uint8_t)(clampf(lerpf((float)p[i] / 255.0f, color[i], k), 0.0f, 1.0f) * 255.0f + 0.5f);
                n[3] = (uint8_t)(clampf(new_a, 0.0f, 1.0f) * 255.0f + 0.5f);
            }
            if (memcmp(n, p, 4) != 0) { memcpy(p, n, 4); changed = true; }
        }

    if (changed) { terrain_mark_dirty_rect(gx0, gz0, gx1 + 1, gz1 + 1); t->edited = true; }
    {
        static unsigned dab_n = 0;
        dab_n++;
        if (dab_n <= 20 || (dab_n % 60) == 0)
            phoenix_engine_info("terrainpaint", "dab #%u at (%.1f,%.1f,%.1f) r=%.2f op=%.2f hard=%.2f erase=%d only_region=%d cols x %d..%d z %d..%d cursor col (%d,%d) changed=%d",
                         dab_n, c.x, c.y, c.z, radius, opacity, hardness, (int)erase, only_region, gx0, gx1, gz0, gz1, hgx, hgz, (int)changed);
    }
    return changed;
}



bool cell_is_hole(const system_terrain* t, float wx, float wz)
{
    for (int i = 0; i < t->tunnel_count; i++) {
        const terrain_tunnel* k = &t->tunnels[i];
        float r2 = (k->radius * 1.1f) * (k->radius * 1.1f);
        float ax = wx - k->ax, az = wz - k->az, bx = wx - k->bx, bz = wz - k->bz;
        if (ax * ax + az * az < r2 || bx * bx + bz * bz < r2) return true;
    }
    return false;
}

bool tunnel_start_in_chunk(const system_terrain* t, const terrain_tunnel* k, int cx, int cz)
{
    const float cs = t->settings.cell_size_m;
    int gx = clampi((int)floorf(k->ax / cs + (float)t->dims.origin_x), 0, t->dims.cells_x - 1);
    int gz = clampi((int)floorf(k->az / cs + (float)t->dims.origin_z), 0, t->dims.cells_z - 1);
    return gx / t->dims.chunk_cells == cx && gz / t->dims.chunk_cells == cz;
}

int tubes_in_chunk(const system_terrain* t, int cx, int cz)
{
    int n = 0;
    for (int i = 0; i < t->tunnel_count; i++) if (tunnel_start_in_chunk(t, &t->tunnels[i], cx, cz)) n++;
    return n;
}

void emit_tubes(const system_terrain* t, int cx, int cz, editor_vertex* vert_to_edge_array, uint32_t* vi, uint32_t* idx, uint32_t* ii)
{
#if terrain_bake_skin_color
    float col[3] = { srgb_to_linear_terrain(0.24f), srgb_to_linear_terrain(0.21f), srgb_to_linear_terrain(0.19f) };
#else
    const float grey = skin_empty_grey_linear();
    float col[3] = { grey, grey, grey };       /* tunnel tube: no skin yet either */
#endif
    for (int n = 0; n < t->tunnel_count; n++) {
        const terrain_tunnel* k = &t->tunnels[n];
        if (!tunnel_start_in_chunk(t, k, cx, cz)) continue;

        float d[3] = { k->bx - k->ax, k->by - k->ay, k->bz - k->az };
        float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (len < 0.01f) continue;
        d[0] /= len; d[1] /= len; d[2] /= len;
        float up[3] = { 0, 1, 0 };
        if (fabsf(d[1]) > 0.95f) { up[0] = 1; up[1] = 0; }
        float u[3] = { up[1] * d[2] - up[2] * d[1], up[2] * d[0] - up[0] * d[2], up[0] * d[1] - up[1] * d[0] };
        float ul = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        u[0] /= ul; u[1] /= ul; u[2] /= ul;
        float w[3] = { d[1] * u[2] - d[2] * u[1], d[2] * u[0] - d[0] * u[2], d[0] * u[1] - d[1] * u[0] };

        uint32_t base = *vi;
        for (int ring = 0; ring < 2; ring++) {
            const float* p = &k->ax;
            if (ring) p = &k->bx;
            for (int s = 0; s < tube_segments; s++) {
                float a = 6.2831853f * (float)s / (float)tube_segments, ca = cosf(a) * k->radius, sa = sinf(a) * k->radius;
                editor_vertex* o = &vert_to_edge_array[(*vi)++];
                o->x = p[0] + u[0] * ca + w[0] * sa;
                o->y = p[1] + u[1] * ca + w[1] * sa;
                o->z = p[2] + u[2] * ca + w[2] * sa;
                o->r = col[0]; o->g = col[1]; o->b = col[2]; o->a = 1.0f;
                /* round the tube: normal points away from the axis, uv = (around, ring 0/1) */
                set_vertex_attributes(o, (float)s / (float)tube_segments, (float)ring,
                                      (u[0] * ca + w[0] * sa) / k->radius,
                                      (u[1] * ca + w[1] * sa) / k->radius,
                                      (u[2] * ca + w[2] * sa) / k->radius);
            }
        }
        for (int s = 0; s < tube_segments; s++) {      /* double sided: the inside is what you see */
            uint32_t a0 = base + s, a1 = base + (s + 1) % tube_segments;
            uint32_t b0 = a0 + tube_segments, b1 = a1 + tube_segments;
            idx[(*ii)++] = a0; idx[(*ii)++] = a1; idx[(*ii)++] = b1;
            idx[(*ii)++] = b1; idx[(*ii)++] = b0; idx[(*ii)++] = a0;
            idx[(*ii)++] = a0; idx[(*ii)++] = b1; idx[(*ii)++] = a1;
            idx[(*ii)++] = b1; idx[(*ii)++] = a0; idx[(*ii)++] = b0;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* chunk meshes                                                              */
/* ------------------------------------------------------------------------- */
void corner_normal(const system_terrain* t, int gx, int gz, float n[3], float* slope_deg)
{
    const float cs = t->settings.cell_size_m;
    float dhdx = (corner_h(t, gx + 1, gz) - corner_h(t, gx - 1, gz)) / (2.0f * cs);
    float dhdz = (corner_h(t, gx, gz + 1) - corner_h(t, gx, gz - 1)) / (2.0f * cs);
    float inv = 1.0f / sqrtf(dhdx * dhdx + 1.0f + dhdz * dhdz);
    n[0] = -dhdx * inv; n[1] = inv; n[2] = -dhdz * inv;
    *slope_deg = acosf(clampf(n[1], -1.0f, 1.0f)) * 57.2957795f;
}

/* openworld: one smooth vertex per corner */
bool build_smooth_chunk(const system_terrain* t, int cx, int cz,
                               editor_vertex** out_v, uint32_t* out_vc, uint32_t** out_i, uint32_t* out_ic)
{
    *out_v = NULL; *out_vc = 0; *out_i = NULL; *out_ic = 0;
    const terrain_settings* s = &t->settings;
    const int cc = t->dims.chunk_cells;
    const float cs = s->cell_size_m;
    const int x0 = cx * cc, z0 = cz * cc;
    const int wc = imin(cc, t->dims.cells_x - x0), dc = imin(cc, t->dims.cells_z - z0);
    if (wc < 1 || dc < 1) return true;

    const int tubes = tubes_in_chunk(t, cx, cz);
    const size_t vn = (size_t)(wc + 1) * (dc + 1) + (size_t)tubes * 2 * tube_segments;
    const size_t in = (size_t)wc * dc * 6 + (size_t)tubes * tube_segments * 12;
    editor_vertex* vert_to_edge_array = (editor_vertex*)malloc(vn * sizeof(editor_vertex) + 1);
    uint32_t*     ix = (uint32_t*)malloc(in * sizeof(uint32_t) + 1);
    if (!vert_to_edge_array || !ix) { phoenix_engine_error("terrain", "out of memory building chunk %d,%d.", cx, cz); free(vert_to_edge_array); free(ix); return false; }

    const float ox = (float)t->dims.origin_x, oz = (float)t->dims.origin_z;
    uint32_t vi = 0, ii = 0;
    for (int j = 0; j <= dc; j++)
        for (int i = 0; i <= wc; i++) {
            int gx = x0 + i, gz = z0 + j;
            float n[3], slope, col[3], y = corner_h(t, gx, gz);
            corner_normal(t, gx, gz, n, &slope);
            surface_color(s, y, slope, n, col);
            apply_paint(t, gx, gz, col);
            editor_vertex* o = &vert_to_edge_array[vi++];
            o->x = ((float)gx - ox) * cs; o->y = y; o->z = ((float)gz - oz) * cs;
            o->r = col[0]; o->g = col[1]; o->b = col[2]; o->a = 1.0f;
            set_vertex_attributes(o, (float)gx, (float)gz, n[0], n[1], n[2]);            /* whole numbers on cell borders (the grid wire needs that), and the skin maps tile by cell */
        }
    const uint32_t row = (uint32_t)(wc + 1);
    for (int j = 0; j < dc; j++)
        for (int i = 0; i < wc; i++) {
            float wx = ((float)(x0 + i) + 0.5f - ox) * cs, wz = ((float)(z0 + j) + 0.5f - oz) * cs;
            if (t->tunnel_count && cell_is_hole(t, wx, wz)) continue;
            uint32_t a = (uint32_t)j * row + (uint32_t)i;      /* (i,j) (i,j+1) (i+1,j+1) (i+1,j): same winding as the old top face */
            uint32_t b = a + row, c = b + 1, d = a + 1;
            ix[ii++] = a; ix[ii++] = b; ix[ii++] = c;
            ix[ii++] = c; ix[ii++] = d; ix[ii++] = a;
        }
    emit_tubes(t, cx, cz, vert_to_edge_array, &vi, ix, &ii);
    *out_v = vert_to_edge_array; *out_vc = vi; *out_i = ix; *out_ic = ii;
    return true;
}



/* top cube layer of a column whose top is at top_m. layer 0 starts at the bottom of the world. */
int calc_cube_layer(const system_terrain* t, float top_m)
{
    const int layer = (int)floorf((top_m + t->dims.size_y_m * 0.5f) / t->settings.cell_size_m + 0.5f) - 1;
    return clampi(layer, 0, t->dims.cells_y - 1);
}

int read_column_layer(const system_terrain* t, int gx, int gz)
{
    if (gx < 0 || gz < 0 || gx >= t->dims.cells_x || gz >= t->dims.cells_z) return terrain_no_layer;
    return calc_cube_layer(t, sandbox_snap(corner_h(t, gx, gz), t->settings.cell_size_m));
}

void add_chunk_quad(terrain_quad* q, size_t* n, int lx, int lz, int ly, int face, int material)
{
    if (q) {                                   /* q == null on the counting pass */
        terrain_quad* o = &q[*n];
        memset(o, 0, sizeof(*o));              /* sub[] = 0: flat top. reserved[] = 0 */
        o->lx = lx; o->lz = lz; o->ly = ly;
        o->face = face; o->material = material;
    }
    (*n)++;
}

bool build_chunk_quads(const system_terrain* t, int cx, int cz, terrain_quad** out_q, uint32_t* out_n)
{
    *out_q = NULL; *out_n = 0;
    const int cc = t->dims.chunk_cells;
    const int x0 = cx * cc, z0 = cz * cc;
    const int wc = imin(cc, t->dims.cells_x - x0), dc = imin(cc, t->dims.cells_z - z0);
    if (wc < 1 || dc < 1) return true;

    /* top layer of every column in the chunk plus a 1 column halo, so walls match across chunk borders */
    const int tw = wc + 2, td = dc + 2;
    int* layer_grid = (int*)malloc((size_t)tw * td * sizeof(int));
    if (!layer_grid) { phoenix_engine_error("terrain", "out of memory building chunk quads."); return false; }
    for (int j = 0; j < td; j++)
        for (int i = 0; i < tw; i++)
            layer_grid[(size_t)j * tw + i] = read_column_layer(t, x0 + i - 1, z0 + j - 1);

    const float cs = t->settings.cell_size_m;
    const float ox = (float)t->dims.origin_x, oz = (float)t->dims.origin_z;
    static const struct { int face, di, dj; } dirs[4] = {
        { terrain_face_west, -1, 0 }, { terrain_face_east, 1, 0 }, { terrain_face_north, 0, -1 }, { terrain_face_south, 0, 1 }
    };

    terrain_quad* q = NULL;
    size_t n = 0, total = 0;
    for (int pass = 0; pass < 2; pass++) {              /* pass 0 counts, pass 1 fills */
        n = 0;
        for (int j = 0; j < dc; j++)
            for (int i = 0; i < wc; i++) {
                const float wx = ((float)(x0 + i) + 0.5f - ox) * cs, wz = ((float)(z0 + j) + 0.5f - oz) * cs;
                if (t->tunnel_count && cell_is_hole(t, wx, wz)) continue;

                const size_t r = (size_t)(j + 1) * tw + (size_t)(i + 1);
                const int top = layer_grid[r];
                add_chunk_quad(q, &n, i, j, top, terrain_face_top, terrain_cell_grass);
                for (int d = 0; d < 4; d++) {
                    const int ln = layer_grid[(size_t)(j + 1 + dirs[d].dj) * tw + (size_t)(i + 1 + dirs[d].di)];
                    if (ln == terrain_no_layer) continue;
                    for (int ly = ln + 1; ly <= top; ly++)   /* one 4 x 4 mesh face per cube of drop */
                        add_chunk_quad(q, &n, i, j, ly, dirs[d].face, terrain_cell_dirt);
                }
            }
        if (pass == 0) {
            total = n;
            if (total == 0) { free(layer_grid); return true; }
            if (total * 4 > 0xfffffff0ull) { phoenix_engine_error("terrain", "chunk %d,%d has too many cube faces.", cx, cz); free(layer_grid); return false; }
            q = (terrain_quad*)malloc(total * sizeof(terrain_quad));
            if (!q) { phoenix_engine_error("terrain", "out of memory building chunk quads."); free(layer_grid); return false; }
        }
    }
    free(layer_grid);
    *out_q = q; *out_n = (uint32_t)total;
    return true;
}

void set_quad_corner(float p[4][3], int k, float x, float y, float z) { p[k][0] = x; p[k][1] = y; p[k][2] = z; }

/* corners are counter-clockwise seen from outside. walls run bottom, top, top, bottom so vert_to_edge_array points up. */
void get_quad_corners(const system_terrain* t, int cx, int cz, const terrain_quad* q, float p[4][3])
{
    const float cs = t->settings.cell_size_m;
    const int gx = cx * t->dims.chunk_cells + q->lx, gz = cz * t->dims.chunk_cells + q->lz;
    const float x0 = ((float)gx - (float)t->dims.origin_x) * cs, x1 = x0 + cs;
    const float z0 = ((float)gz - (float)t->dims.origin_z) * cs, z1 = z0 + cs;
    const float y0 = (float)q->ly * cs - t->dims.size_y_m * 0.5f, y1 = y0 + cs;

    switch (q->face) {
    case terrain_face_bottom:
        set_quad_corner(p, 0, x0, y0, z0); set_quad_corner(p, 1, x1, y0, z0);
        set_quad_corner(p, 2, x1, y0, z1); set_quad_corner(p, 3, x0, y0, z1); break;
    case terrain_face_west:
        set_quad_corner(p, 0, x0, y0, z1); set_quad_corner(p, 1, x0, y1, z1);
        set_quad_corner(p, 2, x0, y1, z0); set_quad_corner(p, 3, x0, y0, z0); break;
    case terrain_face_east:
        set_quad_corner(p, 0, x1, y0, z0); set_quad_corner(p, 1, x1, y1, z0);
        set_quad_corner(p, 2, x1, y1, z1); set_quad_corner(p, 3, x1, y0, z1); break;
    case terrain_face_north:
        set_quad_corner(p, 0, x0, y0, z0); set_quad_corner(p, 1, x0, y1, z0);
        set_quad_corner(p, 2, x1, y1, z0); set_quad_corner(p, 3, x1, y0, z0); break;
    case terrain_face_south:
        set_quad_corner(p, 0, x1, y0, z1); set_quad_corner(p, 1, x1, y1, z1);
        set_quad_corner(p, 2, x0, y1, z1); set_quad_corner(p, 3, x0, y0, z1); break;
    case terrain_face_top:
    default:
        set_quad_corner(p, 0, x0, y1, z0); set_quad_corner(p, 1, x0, y1, z1);
        set_quad_corner(p, 2, x1, y1, z1); set_quad_corner(p, 3, x1, y1, z0);
        for (int k = 0; k < 4; k++)                 /* sub heights: each top corner can sit up to 16/16 of a cube higher */
            p[k][1] += (float)clampi(q->sub[k], 0, terrain_sub_steps) * cs / (float)terrain_sub_steps;
        break;
    }
}

void terrain_quad_corners(int cx, int cz, const terrain_quad* q, float out[4][3])
{
    if (!q || !out || !g_terrain.exists) return;
    get_quad_corners(&g_terrain, cx, cz, q, out);
}

uint32_t get_terrain_chunk_quads(int cx, int cz, const terrain_quad** out)
{
    if (out) *out = NULL;
    if (!g_terrain.exists) return 0;
    const terrain_chunk* c = chunk_ptr(&g_terrain, terrain_layer_terrain, cx, cz);
    if (!c || !c->quads) return 0;
    if (out) *out = c->quads;
    return c->quad_count;
}

bool build_sandbox_chunk(const system_terrain* t, int cx, int cz,
                                editor_vertex** out_v, uint32_t* out_vc, uint32_t** out_i, uint32_t* out_ic,
                                terrain_quad** out_q, uint32_t* out_qn)
{
    *out_v = NULL; *out_vc = 0; *out_i = NULL; *out_ic = 0; *out_q = NULL; *out_qn = 0;
    static const float face_normal[terrain_face_count][3] = {
        { 0, 1, 0 }, { 0, -1, 0 }, { -1, 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 0, 0, 1 }
    };
    static const float corner_u[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    static const float corner_v[4] = { 0.0f, 1.0f, 1.0f, 0.0f };

    const terrain_settings* s = &t->settings;
    terrain_quad* q = NULL; uint32_t qn = 0;
    if (!build_chunk_quads(t, cx, cz, &q, &qn)) return false;

    const int tubes = tubes_in_chunk(t, cx, cz);
    const size_t vn = (size_t)qn * 4 + (size_t)tubes * 2 * tube_segments;
    const size_t in = (size_t)qn * 6 + (size_t)tubes * tube_segments * 12;
    editor_vertex* vert_to_edge_array  = (editor_vertex*)malloc(vn * sizeof(editor_vertex) + 1);
    uint32_t*      ix = (uint32_t*)malloc(in * sizeof(uint32_t) + 1);
    if (!vert_to_edge_array || !ix) { phoenix_engine_error("terrain", "out of memory building chunk %d,%d.", cx, cz); free(q); free(vert_to_edge_array); free(ix); return false; }

    uint32_t vi = 0, ii = 0;
    for (uint32_t k = 0; k < qn; k++) {
        float p[4][3], col[3];
        get_quad_corners(t, cx, cz, &q[k], p);
        int face = q[k].face;
        if (face >= terrain_face_count) face = terrain_face_top;
        float nrm_top[3];
        const float* nrm = face_normal[face];
        if (face == terrain_face_top) {            /* a sloped top (sub heights) gets its real normal: cross of the two diagonals */
            const float d1[3] = { p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2] };
            const float d2[3] = { p[3][0] - p[1][0], p[3][1] - p[1][1], p[3][2] - p[1][2] };
            float nx = d1[1] * d2[2] - d1[2] * d2[1], ny = d1[2] * d2[0] - d1[0] * d2[2], nz = d1[0] * d2[1] - d1[1] * d2[0];
            const float l = sqrtf(nx * nx + ny * ny + nz * nz);
            if (l > 1e-6f) {
                if (ny < 0.0f) { nx = -nx; ny = -ny; nz = -nz; }
                nrm_top[0] = nx / l; nrm_top[1] = ny / l; nrm_top[2] = nz / l; nrm = nrm_top;
            }
        }
        float face_slope = 90.0f;
        if (face == terrain_face_top) face_slope = 0.0f;
        surface_color(s, p[1][1], face_slope, nrm, col);
        apply_paint(t, cx * t->dims.chunk_cells + q[k].lx, cz * t->dims.chunk_cells + q[k].lz, col);

        const uint32_t base = vi;
        for (int c = 0; c < 4; c++) {                      /* every face owns its 4 verts */
            editor_vertex* o = &vert_to_edge_array[vi++];
            o->x = p[c][0]; o->y = p[c][1]; o->z = p[c][2];
            o->r = col[0]; o->g = col[1]; o->b = col[2]; o->a = 1.0f;
            set_vertex_attributes(o, (float)(cx * t->dims.chunk_cells + q[k].lx) + corner_u[c], (float)(cz * t->dims.chunk_cells + q[k].lz) + corner_v[c], nrm[0], nrm[1], nrm[2]);
        }
        ix[ii++] = base;     ix[ii++] = base + 1; ix[ii++] = base + 2;
        ix[ii++] = base + 2; ix[ii++] = base + 3; ix[ii++] = base;
    }
    emit_tubes(t, cx, cz, vert_to_edge_array, &vi, ix, &ii);
    *out_v = vert_to_edge_array; *out_vc = vi; *out_i = ix; *out_ic = ii;
    *out_q = q; *out_qn = qn;
    return true;
}

/* ------------------------------------------------------------------------- */
/* streaming: the chunks around the camera get meshes; each region object     */
/* holds the merged meshes of its resident chunks.                           */
/* ------------------------------------------------------------------------- */
void camera_local_xz(const system_terrain* t, float* lx, float* lz)
{
    const world_camera* cam = &engine_context.world_state.editor_scene_camera;
    const system_scene_object* obj = find_scene_object_by_id(t->scene_object_id);
    float obj_x = 0.0f, obj_z = 0.0f;
    if (obj) { obj_x = obj->position.x; obj_z = obj->position.z; }
    *lx = cam->position.x - obj_x;
    *lz = cam->position.z - obj_z;
}

void terrain_center_chunk(const system_terrain* t, int* out_cx, int* out_cz)
{
    float lx, lz;
    camera_local_xz(t, &lx, &lz);
    const float cs = t->settings.cell_size_m;
    int gx = clampi((int)floorf(lx / cs + (float)t->dims.origin_x), 0, t->dims.cells_x - 1);
    int gz = clampi((int)floorf(lz / cs + (float)t->dims.origin_z), 0, t->dims.cells_z - 1);
    int cx = gx / t->dims.chunk_cells, cz = gz / t->dims.chunk_cells;

    /* the camera may be over a region that is switched off (solo on another region). then the window moves to the nearest
       enabled region, otherwise nothing at all is meshed and that region cannot be edited, sculpted or painted. */
    if (t->region_off[region_of_chunk(t, cx, cz)]) {
        const int rc = t->dims.region_chunks;
        long best = -1; int bx = cx, bz = cz;
        for (int r = 0; r < terrain_region_count; r++) {
            if (t->region_off[r]) continue;
            const int rx = r % terrain_region_grid, rz = r / terrain_region_grid;
            const int px = clampi(cx, rx * rc, (rx + 1) * rc - 1), pz = clampi(cz, rz * rc, (rz + 1) * rc - 1);
            const long d = (long)(px - cx) * (px - cx) + (long)(pz - cz) * (pz - cz);
            if (best < 0 || d < best) { best = d; bx = px; bz = pz; }
        }
        cx = bx; cz = bz;
    }
    *out_cx = cx;
    *out_cz = cz;
}

void chunk_free_mesh(terrain_chunk* c)
{
    free(c->mesh_v); free(c->mesh_i); free(c->quads);
    c->mesh_v = NULL; c->mesh_i = NULL; c->mesh_vc = 0; c->mesh_ic = 0;
    c->quads = NULL; c->quad_count = 0;
    c->mesh_built = false;
}

bool upload_region(system_terrain* t, int r, int cx0, int cx1, int cz0, int cz1)
{
    system_scene_object* obj = find_scene_object_by_id(t->region_ids[r]);
    if (!obj) { phoenix_engine_error("terrain", "upload_region: region %d has no scene object (id %u).", r, (unsigned)t->region_ids[r]); return false; }
    const int rc = t->dims.region_chunks, rx = r % terrain_region_grid, rz = r / terrain_region_grid;
    const int ax = imax(cx0, rx * rc), bx = imin(cx1, (rx + 1) * rc);
    const int az = imax(cz0, rz * rc), bz = imin(cz1, (rz + 1) * rc);

    uint64_t tv = 0, ti = 0; int n = 0;
    for (int cz = az; cz < bz; cz++)
        for (int cx = ax; cx < bx; cx++) {
            const terrain_chunk* c = chunk_ptr(t, terrain_layer_terrain, cx, cz);
            if (c && c->mesh_built) { tv += c->mesh_vc; ti += c->mesh_ic; n++; }
        }
    t->region_resident[r] = n;
    phoenix_engine_info("terrain", "upload_region %d: %d resident chunk(s), %llu verts, %llu indices", r, n, (unsigned long long)tv, (unsigned long long)ti);
    if (tv > 0xfffffff0ull) { phoenix_engine_error("terrain", "region mesh exceeds 32-bit index range. lower the load radius."); return false; }
    if (tv == 0 || ti == 0) { phoenix_engine_warn("terrain", "upload_region %d: nothing to draw (no built chunks in this region) -> region object is empty", r); obj->index_count = 0; return true; }

    editor_vertex* vert_to_edge_array  = (editor_vertex*)malloc((size_t)tv * sizeof(editor_vertex));
    uint32_t*      ix = (uint32_t*)malloc((size_t)ti * sizeof(uint32_t));
    bool ok = false;
    if (vert_to_edge_array && ix) {
        uint32_t vo = 0, io = 0;
        for (int cz = az; cz < bz; cz++)
            for (int cx = ax; cx < bx; cx++) {
                const terrain_chunk* c = chunk_ptr(t, terrain_layer_terrain, cx, cz);
                if (!c || !c->mesh_built || !c->mesh_vc) continue;
                memcpy(&vert_to_edge_array[vo], c->mesh_v, (size_t)c->mesh_vc * sizeof(editor_vertex));
                for (uint32_t k = 0; k < c->mesh_ic; k++) ix[io + k] = c->mesh_i[k] + vo;
                vo += c->mesh_vc; io += c->mesh_ic;
            }
        ok = upload_scene_object_mesh(obj, t->device, t->physical_device, vert_to_edge_array, (uint32_t)tv, ix, (uint32_t)ti);
        if (!ok) phoenix_engine_error("terrain", "gpu upload failed for region %d.", r);
        else     phoenix_engine_info("terrain", "region %d uploaded: obj '%s' id %u index_count=%u bounds=%.1f", r, obj->name, (unsigned)obj->id, (unsigned)obj->index_count, obj->bounds_radius);
    } else {
        phoenix_engine_error("terrain", "out of memory merging region %d.", r);
    }
    free(vert_to_edge_array); free(ix);
    return ok;
}



bool rebuild_terrain_mesh(void)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return false;
    refresh_height_cache(t);

    /* no lod: every resident chunk is the same full detail mesh, so every cell is the same size everywhere. */
    const int side_chunks = terrain_region_side_chunks;      /* the window is the 3 x 3 chunk region the camera is over (9 chunks), aligned to region borders */
    const int r = side_chunks;
    int ccx, ccz;
    terrain_center_chunk(t, &ccx, &ccz);
    const int cx0 = (ccx / side_chunks) * side_chunks, cx1 = imin(t->dims.chunks_x, cx0 + side_chunks);
    const int cz0 = (ccz / side_chunks) * side_chunks, cz1 = imin(t->dims.chunks_z, cz0 + side_chunks);
    {
        static int lw0 = -1, lw1 = -1, lz0 = -1, lz1 = -1;
        if (cx0 != lw0 || cx1 != lw1 || cz0 != lz0 || cz1 != lz1) {
            lw0 = cx0; lw1 = cx1; lz0 = cz0; lz1 = cz1;
            float lx, lz; camera_local_xz(t, &lx, &lz);
            phoenix_engine_info("terrain", "resident window: chunks x %d..%d z %d..%d (region side %d chunks, camera chunk %d,%d, camera local x=%.1f z=%.1f, camera y=%.1f)",
                         cx0, cx1 - 1, cz0, cz1 - 1, r, ccx, ccz, lx, lz, engine_context.world_state.editor_scene_camera.position.y);
        }
    }

    if (t->region_valid) {                      /* chunks that left the window lose their mesh */
        for (int cz = t->region_cz0; cz < t->region_cz1; cz++)
            for (int cx = t->region_cx0; cx < t->region_cx1; cx++) {
                if (cx >= cx0 && cx < cx1 && cz >= cz0 && cz < cz1) continue;
                terrain_chunk* c = chunk_ptr(t, terrain_layer_terrain, cx, cz);
                if (!c) continue;
                if (c->mesh_built) t->region_dirty[region_of_chunk(t, cx, cz)] = true;
                phoenix_engine_info("terrain", "chunk %d,%d left the window: mesh freed", cx, cz);
                chunk_free_mesh(c);
                c->resident = false;
                c->dirty = true;
            }
    }

    const bool sandbox = (t->settings.mode == terrain_mode_sandbox);
    int built = 0;
    const int pending = 0;

    for (int cz = cz0; cz < cz1; cz++)
        for (int cx = cx0; cx < cx1; cx++) {
            terrain_chunk* c = chunk_ptr(t, terrain_layer_terrain, cx, cz);
            if (!c) continue;
            c->resident = true;
            if (!chunk_region_on(t, cx, cz)) {            /* region is off: no mesh at all */
                phoenix_engine_warn("terrain", "chunk %d,%d skipped: region %d is off", cx, cz, region_of_chunk(t, cx, cz));
                if (c->mesh_built) { chunk_free_mesh(c); t->region_dirty[region_of_chunk(t, cx, cz)] = true; }
                continue;
            }
            if (c->mesh_built && !c->dirty) continue;

            editor_vertex* nv = NULL; uint32_t nvc = 0; uint32_t* ni = NULL; uint32_t nic = 0;
            terrain_quad* nq = NULL; uint32_t nqc = 0;
            bool ok = false;
            if (sandbox) ok = build_sandbox_chunk(t, cx, cz, &nv, &nvc, &ni, &nic, &nq, &nqc);
            else         ok = build_smooth_chunk(t, cx, cz, &nv, &nvc, &ni, &nic);
            if (!ok) { phoenix_engine_error("terrain", "chunk %d,%d mesh build failed (out of memory or too many faces).", cx, cz); return false; }

            chunk_free_mesh(c);                   /* also frees the old quads */
            c->mesh_v = nv; c->mesh_vc = nvc; c->mesh_i = ni; c->mesh_ic = nic;
            c->quads = nq; c->quad_count = nqc;
            apply_terrain_light_to_mesh(t, c->mesh_v, c->mesh_vc);
            c->mesh_built = true;
            c->dirty = false;
            const char* kind_text = "smooth";
            if (sandbox) kind_text = "sandbox";
            phoenix_engine_info("terrain", "built chunk %d,%d (region %d, %s): %u verts, %u indices, %u quads", cx, cz, region_of_chunk(t, cx, cz),
                         kind_text, (unsigned)nvc, (unsigned)nic, (unsigned)nqc);
            t->region_dirty[region_of_chunk(t, cx, cz)] = true;
            built++;
        }
    s_built_since_log += built;
    s_regions_changed = false;

    t->region_cx0 = cx0; t->region_cx1 = cx1; t->region_cz0 = cz0; t->region_cz1 = cz1;
    t->region_valid = true;
    t->center_cx = ccx; t->center_cz = ccz;
    t->resident_chunks = (cx1 - cx0) * (cz1 - cz0);

    /* diagnostic for the missing square: logs once whenever the camera chunk changes */
    {
        static int last_cx = -1, last_cz = -1;
        if (ccx != last_cx || ccz != last_cz) {
            last_cx = ccx; last_cz = ccz;
            const terrain_chunk* cc0 = chunk_ptr(t, terrain_layer_terrain, ccx, ccz);
            const int rg = region_of_chunk(t, ccx, ccz);
            const system_scene_object* ro = find_scene_object_by_id(t->region_ids[rg]);
            int cc0_built = -1, cc0_dirty = -1;
            unsigned cc0_vc = 0u, cc0_ic = 0u, ro_ic = 0u;
            if (cc0) { cc0_built = (int)cc0->mesh_built; cc0_dirty = (int)cc0->dirty; cc0_vc = cc0->mesh_vc; cc0_ic = cc0->mesh_ic; }
            if (ro) ro_ic = (unsigned)ro->index_count;
            phoenix_engine_info("terrain", "camera chunk %d,%d region %d: region_off=%d mesh_built=%d verts=%u idx=%u dirty=%d tunnels=%d region_index_count=%u",
                         ccx, ccz, rg, (int)t->region_off[rg], cc0_built, cc0_vc,
                         cc0_ic, cc0_dirty, t->tunnel_count, ro_ic);
        }
    }

    bool all_ok = true, any = false;
    for (int k = 0; k < terrain_region_count; k++) if (t->region_dirty[k]) any = true;
    if (any) {
        for (int k = 0; k < terrain_region_count; k++)
            if (t->region_dirty[k]) {
                if (upload_region(t, k, cx0, cx1, cz0, cz1)) t->region_dirty[k] = false; else all_ok = false;
            }
        t->last_vertex_count = 0; t->last_index_count = 0;        /* totals for the ui: every resident chunk */
        for (int cz = cz0; cz < cz1; cz++)
            for (int cx = cx0; cx < cx1; cx++) {
                const terrain_chunk* c = chunk_ptr(t, terrain_layer_terrain, cx, cz);
                if (c && c->mesh_built) { t->last_vertex_count += c->mesh_vc; t->last_index_count += c->mesh_ic; }
            }
        /* one line when the window is complete, not one per frame while it fills up */
        if (all_ok && pending == 0 && s_built_since_log > 0) {
            phoenix_engine_info("terrain", "chunks %d..%d x %d..%d resident (%d rebuilt): %u verts, %u indices.",
                         cx0, cx1 - 1, cz0, cz1 - 1, s_built_since_log, t->last_vertex_count, t->last_index_count);
            s_built_since_log = 0;
        }
    }
    return all_ok;
}

void update_terrain_streaming(void)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return;
    refresh_height_cache(t);

    static int retry_cooldown = 0;
    if (retry_cooldown > 0) { retry_cooldown--; return; }

    int ccx, ccz;
    terrain_center_chunk(t, &ccx, &ccz);
    const int side_chunks = terrain_region_side_chunks;
    bool need = !t->region_valid || ccx / side_chunks != t->center_cx / side_chunks || ccz / side_chunks != t->center_cz / side_chunks || s_regions_changed;
    for (int cz = t->region_cz0; cz < t->region_cz1 && !need; cz++)
        for (int cx = t->region_cx0; cx < t->region_cx1; cx++) {
            const terrain_chunk* c = chunk_ptr(t, terrain_layer_terrain, cx, cz);
            if (c && chunk_region_on(t, cx, cz) && (!c->mesh_built || c->dirty)) { need = true; break; }
        }
    if (need && !rebuild_terrain_mesh()) {
        retry_cooldown = 120;
        phoenix_engine_error("terrain", "rebuild_terrain_mesh failed: retrying in 120 frames (terrain will be missing until then).");
    }
}

/* ------------------------------------------------------------------------- */
/* location                                                                  */
/* ------------------------------------------------------------------------- */
static void persist_settings(void)
{
    if (!g_terrain.exists) return;
}

void apply_terrain_location(void)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return;
    const float k = 1.0f / (float)terrain_units_per_meter;
    editor_vec3 p = { (float)t->settings.location.x * k, (float)t->settings.location.y * k, (float)t->settings.location.z * k };
    system_scene_object* root = find_scene_object_by_id(t->scene_object_id);
    if (root) root->position = p;
#if terrain_regions_use_world_position
    for (int r = 0; r < terrain_region_count; r++) {
        system_scene_object* o = find_scene_object_by_id(t->region_ids[r]);
        if (o) o->position = p;
    }
#endif
}

void set_terrain_location(terrain_location_q loc)
{
    if (!g_terrain.exists) return;
    g_terrain.settings.location = loc;
    g_terrain_ui.location       = loc;
    persist_settings();
    apply_terrain_location();
}

/* ------------------------------------------------------------------------- */
/* scene objects: terrain > region_00 .. region_08                           */
/* ------------------------------------------------------------------------- */
void unlink_from_parent(system_scene_object* c)
{
    system_scene_object* p = c->parent;
    if (!p) return;
    if (p->first_child == c) p->first_child = c->next_sibling;
    else for (system_scene_object* s = p->first_child; s; s = s->next_sibling)
        if (s->next_sibling == c) { s->next_sibling = c->next_sibling; break; }
    c->parent = NULL; c->next_sibling = NULL;
}

void link_child(system_scene_object* parent, system_scene_object* child)
{
    unlink_from_parent(child);
    child->parent = parent;
    child->next_sibling = NULL;
    if (!parent->first_child) { parent->first_child = child; return; }
    system_scene_object* s = parent->first_child;
    while (s->next_sibling) s = s->next_sibling;
    s->next_sibling = child;
}

bool is_terrain_object(const system_scene_object* obj)
{
    if (!obj || !g_terrain.exists) return false;
    if (obj->id == g_terrain.scene_object_id) return true;
    for (int r = 0; r < terrain_region_count; r++) if (obj->id == g_terrain.region_ids[r]) return true;
    return false;
}

void bind_terrain_builder(uint32_t object_id)
{
    const system_scene_object* o = find_scene_object_by_id(object_id);
    if (is_terrain_object(o)) g_terrain.builder_bound = true;
}

/* ------------------------------------------------------------------------- */
/* lifetime                                                                  */
/* ------------------------------------------------------------------------- */
void terrain_free_cpu(system_terrain* t)
{
    for (int l = 0; l < terrain_max_layers; l++) {
        terrain_layer* layer_grid = &t->layers[l];
        if (layer_grid->chunks) {
            int n = t->dims.chunks_x * t->dims.chunks_z;
            for (int i = 0; i < n; i++) {
                free(layer_grid->chunks[i].cells); free(layer_grid->chunks[i].heights);
                free(layer_grid->chunks[i].mesh_v); free(layer_grid->chunks[i].mesh_i);
                free(layer_grid->chunks[i].quads);
            }
            free(layer_grid->chunks);
        }
        layer_grid->chunks = NULL;
    }
    t->region_valid = false;
    t->resident_chunks = 0;
}


bool spawn_terrain_system(const terrain_settings* s_in, VkDevice device, VkPhysicalDevice physical_device)
{
    system_terrain* t = &g_terrain;
    terrain_settings s = *s_in;
    s.chunk_cells   = terrain_chunk_cells;
    s.region_chunks = clampi(s.region_chunks, 1, terrain_max_region_chunks);
    if (s.cell_size_m < 0.0625f) s.cell_size_m = system_unit_m;
    s.edit_stride = 1;                          /* one edit vertex per cell: every edit face is one cell */
    terrain_dims d = terrain_resolve_dims(&s);
    s.cells_x = d.cells_x; s.cells_z = d.cells_z; s.cells_y = d.cells_y;

    const bool had = t->exists;
    if (!had && g_scene_registry.object_count + 1 + terrain_region_count > max_scene_objects) {
        phoenix_engine_warn("terrain", "scene registry has no room for the terrain and its 9 regions.");
        return false;
    }

    if (had) terrain_free_cpu(t);               /* old dims on purpose */
    s_edit_have = false;

    t->settings = s; t->dims = d;
    t->device = device; t->physical_device = physical_device;
    t->tunnel_count = 0;
    t->edited = false; t->edit_revision++;
    t->edit_follow = true; t->edit_cx = d.chunks_x / 2; t->edit_cz = d.chunks_z / 2;
    memset(t->region_dirty, 0, sizeof(t->region_dirty));
    memset(t->region_resident, 0, sizeof(t->region_resident));
    t->last_vertex_count = t->last_index_count = 0;

    for (int l = 0; l < terrain_max_layers; l++) { t->layers[l].visible = true; t->layers[l].chunks = NULL; }
    t->layers[terrain_layer_terrain].chunks = (terrain_chunk*)calloc((size_t)d.chunks_x * d.chunks_z, sizeof(terrain_chunk));
    if (!t->layers[terrain_layer_terrain].chunks) { phoenix_engine_error("terrain", "out of memory allocating chunk table."); t->exists = false; return false; }
    t->layers[terrain_light_layer].chunks = (terrain_chunk*)calloc((size_t)d.chunks_x * d.chunks_z, sizeof(terrain_chunk));
    if (!t->layers[terrain_light_layer].chunks) phoenix_engine_warn("terrain", "out of memory allocating the light layer chunk table. lighting disabled.");

    /* scene objects */
    if (!had) {
        editor_vec3 zero = {0, 0, 0};
        system_scene_object* root = spawn_new_scene_object("terrain", zero);
        if (!root) { terrain_free_cpu(t); return false; }
        root->type = scene_object_empty;
        root->scale = (editor_vec3){1.0f, 1.0f, 1.0f};
        t->scene_object_id = root->id;
        for (int r = 0; r < terrain_region_count; r++) {
            char name[16]; terrain_region_name(r, name, sizeof(name));
            system_scene_object* o = spawn_new_scene_object(name, zero);
            if (!o) { phoenix_engine_error("terrain", "could not create %s.", name); terrain_free_cpu(t); return false; }
            o->type = scene_object_mesh;
            o->scale = (editor_vec3){1.0f, 1.0f, 1.0f};
            o->index_count = 0;
            t->region_ids[r] = o->id;
            link_child(find_scene_object_by_id(t->scene_object_id), o);
        }
    } else {
        for (int r = 0; r < terrain_region_count; r++) {
            system_scene_object* o = find_scene_object_by_id(t->region_ids[r]);
            if (o) o->index_count = 0;
        }
    }
    t->exists = true;
    for (int r = 0; r < terrain_region_count; r++) {
        t->region_off[r] = false;                                            /* every region on */
        remove_object_data(t->region_ids[r], object_data_terrain_color);       /* a flat reset also clears the paint */
    }
    s_regions_changed = false;

    /* flat height blocks: a zeroed block is height 0 everywhere */
    const uint32_t side = (uint32_t)d.region_cells + 1u;
    const uint64_t bytes = region_header_size + (uint64_t)side * side * sizeof(float);
    for (int r = 0; r < terrain_region_count; r++) {
        if (bytes > 64ull * 1024 * 1024) {
            phoenix_engine_error("terrain", "object database refused the %u byte block for region %d.", (unsigned)bytes, r);
            t->exists = false; terrain_free_cpu(t);
            return false;
        }
        region_header* h = (region_header*)get_object_data(t->region_ids[r], object_data_terrain_region, NULL);
        if (!h) {
            phoenix_engine_error("terrain", "region %d height block missing right after creation.", r);
            t->exists = false; terrain_free_cpu(t);
            return false;
        }
        /* flat no matter what: zero the whole block ourselves. a block that was loaded from the database file, or one that
           object_db_set reused, can still hold old heights. every height is 0 after creation / reset, always. */
        memset(h, 0, (size_t)bytes);
        h->magic = region_magic; h->version = region_version; h->side = side;
    }
    remove_object_data(t->scene_object_id, object_data_terrain_tunnels);
    persist_settings();
    refresh_height_cache(t);
    apply_terrain_location();

    const char* mode_text = "open-world";
    if (s.mode == terrain_mode_sandbox) mode_text = "sandbox";
    phoenix_engine_info("terrain", "%s terrain, flat: 3x3 regions of %dx%d chunks (%d cubes each side), chunks of %dx%d cubes, %d cubes high, %.0f mesh cubes, %.0f x %.0f mesh.",
                 mode_text, d.region_chunks, d.region_chunks, d.region_cells,
                 d.chunk_cells, d.chunk_cells, d.cells_y, s.cell_size_m, d.size_x_m, d.size_z_m);
    {
        float lx, lz; camera_local_xz(t, &lx, &lz);
        phoenix_engine_info("terrain", "spawn: cells %dx%d, %d regions of %d chunk(s), camera local x=%.1f z=%.1f y=%.1f, terrain object id %u",
                     d.cells_x, d.cells_z, terrain_region_count, d.region_chunks, lx, lz, engine_context.world_state.editor_scene_camera.position.y, (unsigned)t->scene_object_id);
    }
    return rebuild_terrain_mesh();
}

bool create_terrain_system(void)
{
    system_terrain* t = &g_terrain;
    if (t->exists) {                            /* pressing + again must never wipe the user's work */
        g_scene_registry.selected_object_id = t->scene_object_id;
        t->builder_bound = true;
        return true;
    }
    if (g_terrain_ui.cell_size_m <= 0.0f) terrain_settings_defaults(&g_terrain_ui);
    bool ok = spawn_terrain_system(&g_terrain_ui, render_context.logical_device, render_context.physical_device);
    if (ok) { g_scene_registry.selected_object_id = t->scene_object_id; t->builder_bound = true; }
    return ok;
}

bool reset_terrain_to_flat(void)
{
    if (!g_terrain.exists) return false;
    terrain_settings s = g_terrain.settings;
    bool bound = g_terrain.builder_bound;
    bool ok = spawn_terrain_system(&s, g_terrain.device, g_terrain.physical_device);
    g_terrain.builder_bound = bound;
    return ok;
}

void shutdown_terrain_system(void)
{
    if (!g_terrain.exists) return;
    terrain_free_cpu(&g_terrain);
    em_mesh_free(s_edit); s_edit_have = false;
    g_terrain.exists = false;
    g_terrain.builder_bound = false;
}

/* deletes the whole terrain: scene objects (terrain + region_00..08), database blocks, cpu chunk data.
   delete_scene_object() refuses the terrain while it exists, so the terrain is shut down first (exists = false, ids cleared),
   then the scene objects are deleted like any other object. */
bool delete_terrain(void)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return false;

    const uint32_t root = t->scene_object_id;
    uint32_t ids[terrain_region_count];
    memcpy(ids, t->region_ids, sizeof(ids));
    phoenix_engine_info("terrain", "deleting the terrain (object %u and %d regions)", (unsigned)root, terrain_region_count);

    shutdown_terrain_system();                       /* exists = false: nothing counts as "the terrain" any more */
    t->scene_object_id = 0;
    memset(t->region_ids, 0, sizeof(t->region_ids));
    memset(t->region_dirty, 0, sizeof(t->region_dirty));
    memset(t->region_resident, 0, sizeof(t->region_resident));
    t->tunnel_count = 0;
    t->edited = false;
    t->edit_revision++;
    s_regions_changed = false;

    for (int r = 0; r < terrain_region_count; r++) remove_object_data_all(ids[r]);
    remove_object_data_all(root);

    g_scene_registry.selected_object_id = 0;
    for (int r = 0; r < terrain_region_count; r++) delete_scene_object(ids[r]);
    const bool ok = delete_scene_object(root);
    if (!ok || find_scene_object_by_id(root)) {
        phoenix_engine_warn("terrain", "the terrain object is still in the scene: delete_scene_object() refused it. remove its terrain check (see system_scene_delete.h).");
    } else {
        phoenix_engine_info("terrain", "terrain deleted.");
    }
    return ok;
}

/* ------------------------------------------------------------------------- */
/* world builder setters                                                     */
/* ------------------------------------------------------------------------- */
void set_terrain_mode(terrain_mode mode)
{
    system_terrain* t = &g_terrain;
    g_terrain_ui.mode = mode;
    if (!t->exists || t->settings.mode == mode) return;
    t->settings.mode = mode;
    const char* mode_text = "open-world";
    if (mode == terrain_mode_sandbox) mode_text = "sandbox";
    phoenix_engine_info("terrain", "mode changed to %s", mode_text);
    persist_settings();
    const int n = t->dims.chunks_x * t->dims.chunks_z;
    for (int i = 0; i < n; i++) t->layers[terrain_layer_terrain].chunks[i].dirty = true;   /* same heights, other mesher */
    if (mode == terrain_mode_sandbox && t->edited) {   /* snap what exists to whole cubes so edit mode and the mesh agree */
        for (int gz = 0; gz <= t->dims.cells_z; gz++)
            for (int gx = 0; gx <= t->dims.cells_x; gx++)
                terrain_set_corner_height(gx, gz, corner_h(t, gx, gz));
        t->edit_revision++;
    }
}

bool set_terrain_region_chunks(int rc)
{
    system_terrain* t = &g_terrain;
    rc = clampi(rc, 1, terrain_max_region_chunks);
    g_terrain_ui.region_chunks = rc;
    if (!t->exists) return true;
    if (t->edited) return false;                // changing the size re-creates the world: refuse while it holds edits
    if (t->settings.region_chunks == rc) return true;
    terrain_settings s = t->settings; s.region_chunks = rc;
    bool bound = t->builder_bound;
    bool ok = spawn_terrain_system(&s, t->device, t->physical_device);
    t->builder_bound = bound;
    return ok;
}

void set_terrain_load_radius(int radius)
{
    radius = clampi(radius, 0, terrain_max_load_radius);
    g_terrain_ui.load_radius = radius;
    if (!g_terrain.exists) return;
    g_terrain.settings.load_radius = radius;
    persist_settings();
}

bool terrain_add_tunnel(const terrain_tunnel* k)
{
    system_terrain* t = &g_terrain;
    if (!t->exists || !k || t->tunnel_count >= terrain_max_tunnels) return false;
    t->tunnels[t->tunnel_count++] = *k;
    phoenix_engine_info("terrain", "tunnel #%d added: (%.1f,%.1f,%.1f) -> (%.1f,%.1f,%.1f) radius %.1f (cuts a hole in the surface at both ends)",
                 t->tunnel_count, k->ax, k->ay, k->az, k->bx, k->by, k->bz, k->radius);

    const float cs = t->settings.cell_size_m;
    const float xs[2] = { k->ax, k->bx }, zs[2] = { k->az, k->bz };
    for (int e = 0; e < 2; e++) {
        int gx = (int)floorf(xs[e] / cs + (float)t->dims.origin_x), gz = (int)floorf(zs[e] / cs + (float)t->dims.origin_z);
        int mesh = (int)ceilf(k->radius * 1.2f / cs) + 1;
        terrain_mark_dirty_rect(gx - mesh, gz - mesh, gx + mesh, gz + mesh);
    }
    {   /* the tube lives in the chunk that holds its start */
        int gx = clampi((int)floorf(k->ax / cs + (float)t->dims.origin_x), 0, t->dims.cells_x - 1);
        int gz = clampi((int)floorf(k->az / cs + (float)t->dims.origin_z), 0, t->dims.cells_z - 1);
        terrain_chunk* c = chunk_ptr(t, terrain_layer_terrain, gx / t->dims.chunk_cells, gz / t->dims.chunk_cells);
        if (c) { c->dirty = true; c->modified = true; }
    }
    t->edited = true;
    return true;
}

/* ------------------------------------------------------------------------- */
/* edit mode bridge                                                          */
/*                                                                           */
/* edit mode works on one chunk at a time. the chunk is shown as a control    */
/* grid with one vertex per cube column (terrain_chunk_cells x terrain_chunk_cells quads, every face is one   */
/* cell = system_unit_m x system_unit_m in xz). in sandbox mode vertex (i,j)  */
/* is the height of column (i,j). only y is used; x/z drags are put back on   */
/* commit.                                                                   */
/* ------------------------------------------------------------------------- */


/* always one vertex per cell. the stored edit_stride setting is ignored. */
/*
 * the terrain editor implementation lives in the editor DLL.
 */
#if 0
int edit_stride_ok(int st) { (void)st; return 1; }

/* edit mode works on one region: 3 x 3 chunks = 300 x 300 cells, one vertex per cell column. s_ecx / s_ecz = its first chunk. */
int edit_span_cells(const system_terrain* t) { return t->dims.chunk_cells * terrain_region_side_chunks; }

void edit_sync_positions(const system_terrain* t)
{
    const int cc = t->dims.chunk_cells, ec = edit_span_cells(t), st = s_estride, n = ec / st;
    const float cs = t->settings.cell_size_m, ox = (float)t->dims.origin_x, oz = (float)t->dims.origin_z;
    for (int j = 0; j <= n; j++)
        for (int i = 0; i <= n; i++) {
            uint32_t idx = (uint32_t)(j * (n + 1) + i);
            int gx = s_ecx * cc + i * st, gz = s_ecz * cc + j * st;
            float y = corner_h(t, gx, gz);
            if (t->settings.mode == terrain_mode_sandbox) y = sandbox_snap(y, cs);   /* the cube top drawn for this column is snapped: the overlay must sit on it, not on the raw height */
            s_edit.verts[idx].obj_vert_pos.x = ((float)gx - ox) * cs;
            s_edit.verts[idx].obj_vert_pos.y = y;
            s_edit.verts[idx].obj_vert_pos.z = ((float)gz - oz) * cs;
            s_ebase[idx] = y;
        }
}

void set_terrain_edit_chunk(int cx, int cz)
{
    system_terrain* t = &g_terrain;
    if (!t->exists) return;
    t->edit_cx = (clampi(cx, 0, t->dims.chunks_x - 1) / terrain_region_side_chunks) * terrain_region_side_chunks;
    t->edit_cz = (clampi(cz, 0, t->dims.chunks_z - 1) / terrain_region_side_chunks) * terrain_region_side_chunks;
    t->edit_follow = false;
}

void set_terrain_edit_stride(int stride)
{
    g_terrain_ui.edit_stride = edit_stride_ok(stride);
    if (g_terrain.exists) { g_terrain.settings.edit_stride = g_terrain_ui.edit_stride; persist_settings(); }
}

edit_mode_mesh* terrain_edit_mesh_acquire(system_scene_object* obj)
{
    system_terrain* t = &g_terrain;
    if (!is_terrain_object(obj)) return NULL;
    refresh_height_cache(t);

    int tcx = t->edit_cx, tcz = t->edit_cz;
    if (t->edit_follow) {
        uint32_t nv = 0;
        if (s_edit_have) em_selection_counts(&s_edit, &nv, NULL, NULL);
        if (!s_edit_have || nv == 0) { tcx = t->center_cx; tcz = t->center_cz; }
        else { tcx = s_ecx; tcz = s_ecz; }
    }
    tcx = (clampi(tcx, 0, t->dims.chunks_x - 1) / terrain_region_side_chunks) * terrain_region_side_chunks;   /* snap to the region's first chunk */
    tcz = (clampi(tcz, 0, t->dims.chunks_z - 1) / terrain_region_side_chunks) * terrain_region_side_chunks;
    t->edit_cx = tcx; t->edit_cz = tcz;
    const int st = edit_stride_ok(t->settings.edit_stride);

    if (!s_edit_have || tcx != s_ecx || tcz != s_ecz || st != s_estride || t->edit_revision != s_erev) {
        const int n = edit_span_cells(t) / st;
        em_make_grid(&s_edit, (uint32_t)n, (uint32_t)n);
        const uint32_t vc = (uint32_t)(n + 1) * (uint32_t)(n + 1);
        if (vc > s_ebase_cap) {
            float* nb = (float*)realloc(s_ebase, (size_t)vc * sizeof(float));
            if (!nb) return NULL;
            s_ebase = nb; s_ebase_cap = vc;
        }
        s_ecx = tcx; s_ecz = tcz; s_estride = st; s_erev = t->edit_revision;
        edit_sync_positions(t);
        s_edit_have = true;
        phoenix_engine_info("terrain", "edit grid (re)built for chunk %d,%d: %d x %d cells, revision %u", s_ecx, s_ecz, n, n, (unsigned)s_erev);
    }
    return &s_edit;
}

edit_mode_mesh* terrain_edit_mesh_current(void)
{
    if (g_terrain.exists && s_edit_have) return &s_edit;
    return NULL;
}

bool terrain_commit_edit_mesh(edit_mode_mesh* mesh, system_scene_object* obj)
{
    system_terrain* t = &g_terrain;
    (void)obj;
    if (!t->exists || !s_edit_have || mesh != &s_edit) return false;
    refresh_height_cache(t);

    const int cc = t->dims.chunk_cells, ec = edit_span_cells(t), st = s_estride, n = ec / st;
    float maxd = 0.0f;
    for (uint32_t i = 0; i < mesh->vert_count; i++) maxd = fmaxf(maxd, fabsf(mesh->verts[i].obj_vert_pos.y - s_ebase[i]));

    const char* commit_text = "nothing to write";
    if (maxd > 1e-4f) commit_text = "writing heights";
    phoenix_engine_info("terrain", "commit edit mesh: chunk %d,%d max height change %.4f mesh -> %s", s_ecx, s_ecz, maxd, commit_text);
    if (maxd > 1e-4f) {
        const int x0 = s_ecx * cc, z0 = s_ecz * cc;
        float* base = s_ebase;
        for (int gz = 0; gz <= ec; gz++) {
            float fz = (float)gz / (float)st; int j = imin((int)fz, n - 1); float tz = fz - (float)j;
            for (int gx = 0; gx <= ec; gx++) {
                float fx = (float)gx / (float)st; int i = imin((int)fx, n - 1); float tx = fx - (float)i;
                #define delta(ii, jj) (mesh->verts[(jj) * (n + 1) + (ii)].obj_vert_pos.y - base[(jj) * (n + 1) + (ii)])
                float d = lerpf(lerpf(delta(i, j),     delta(i + 1, j),     tx),
                                lerpf(delta(i, j + 1), delta(i + 1, j + 1), tx), tz);
                #undef delta
                if (fabsf(d) > 1e-6f) terrain_set_corner_height(x0 + gx, z0 + gz, corner_h(t, x0 + gx, z0 + gz) + d);
            }
        }
        terrain_mark_dirty_rect(x0, z0, x0 + ec, z0 + ec);
        t->edited = true;
        rebuild_terrain_mesh();
    }
    edit_sync_positions(t);        /* x/z back on the grid, y = what the terrain really holds now (cube snapping included) */
    return true;
}
#endif