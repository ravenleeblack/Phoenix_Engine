#include "system_globals.h"

paint_state g_paint;
static bool s_paint_started = false;

void start_paint_mode(void)
{
    if (s_paint_started) return;
    s_paint_started = true;

    g_paint.active = false;
    g_paint.color[0] = 1.0f;
    g_paint.color[1] = 1.0f;
    g_paint.color[2] = 1.0f;
    g_paint.radius = 2.0f;
    g_paint.opacity = 1.0f;
    g_paint.hardness = 0.5f;
    g_paint.spacing = 0.10f;
}

void paint_mode_set_active(bool on)
{
    start_paint_mode();
    g_paint.active = on;
    phoenix_info("Paint", "paint_mode_set_active(%d)", (int)on);
}

bool paint_mode_is_active(void)
{
    // Verifies the UI switch flag is true and global editor context is set to Paint
    const bool r = g_paint.active && mode_state_is_paint();
    static int last = -1;
    if (last != (int)r) {
        last = (int)r;
        phoenix_info("Paint", "paint_mode_is_active -> %d (g_paint.active=%d, mode_state_is_paint=%d)", (int)r, (int)g_paint.active, (int)mode_state_is_paint());
    }
    return r; 
}

void paint_clamp_settings(void)
{
    if (g_paint.radius < 0.01f)   g_paint.radius = 0.01f;
    if (g_paint.radius > 400.0f)  g_paint.radius = 400.0f;
    if (g_paint.opacity < 0.0f)   g_paint.opacity = 0.0f;
    if (g_paint.opacity > 1.0f)   g_paint.opacity = 1.0f;
    if (g_paint.hardness < 0.0f)  g_paint.hardness = 0.0f;
    if (g_paint.hardness > 0.99f) g_paint.hardness = 0.99f;
    if (g_paint.spacing < 0.02f)  g_paint.spacing = 0.02f;
    if (g_paint.spacing > 1.0f)   g_paint.spacing = 1.0f;
}

static float paint_falloff(float dist, float radius, float hardness)
{
    if (radius <= 0.0f || dist >= radius) return 0.0f;
    float t = dist / radius;
    if (t <= hardness) return 1.0f;
    float u = (t - hardness) / (1.0f - hardness);
    return 1.0f - u * u * (3.0f - 2.0f * u); // Smoothstep gradient down to rim
}

/* carried colour of the Smudge brush: it forgets it when the stroke stops (no dab for a few frames) */
static float    s_smudge[3];
static bool     s_smudge_valid = false;
static unsigned s_smudge_frame = 0;

static void mix3(float* dst, const float* src, float t)
{
    for (int c = 0; c < 3; c++) dst[c] = dst[c] * (1.0f - t) + src[c] * t;
}

/* brush ids: 0 paint, 1 airbrush, 2 eraser, 3 smudge, 4 blur, 5 fill, 6 picker, 7 lighten, 8 darken */
bool paint_dab(edit_mode_mesh* mesh, editor_vec3 ctr, int brush, bool erase_now)
{
    if (!mesh || mesh->vert_count == 0) {
        static unsigned bad = 0;
        if (bad++ < 10) phoenix_warn("Paint", "paint_dab: no mesh to paint on (mesh=%p verts=%u)", (void*)mesh, mesh ? (unsigned)mesh->vert_count : 0u);
        return false;
    }
    if (brush < 0 || brush > 8) brush = 0;
    if (erase_now) brush = 2;

    const float R  = g_paint.radius;
    const float r2 = R * R;
    const float white[3] = { 1.0f, 1.0f, 1.0f };
    bool changed = false;

    /* Picker: take the colour of the vertex nearest to the dab. Nothing is painted. */
    if (brush == 6) {
        float best = r2;
        int   hit  = -1;
        for (uint32_t i = 0; i < mesh->vert_count; i++) {
            const float dx = mesh->verts[i].obj_vert_pos.x - ctr.x, dy = mesh->verts[i].obj_vert_pos.y - ctr.y, dz = mesh->verts[i].obj_vert_pos.z - ctr.z;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best) { best = d2; hit = (int)i; }
        }
        if (hit >= 0) for (int c = 0; c < 3; c++) g_paint.color[c] = mesh->verts[hit].obj_vert_color[c];
        return false;
    }

    /* Blur and Smudge need the average colour under the dab */
    float avg[3] = { 0.0f, 0.0f, 0.0f };
    if (brush == 3 || brush == 4) {
        float wsum = 0.0f;
        for (uint32_t i = 0; i < mesh->vert_count; i++) {
            const float dx = mesh->verts[i].obj_vert_pos.x - ctr.x, dy = mesh->verts[i].obj_vert_pos.y - ctr.y, dz = mesh->verts[i].obj_vert_pos.z - ctr.z;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 >= r2) continue;
            const float w = paint_falloff(sqrtf(d2), R, g_paint.hardness);
            if (w <= 0.0f) continue;
            for (int c = 0; c < 3; c++) avg[c] += mesh->verts[i].obj_vert_color[c] * w;
            wsum += w;
        }
        if (wsum <= 0.0f) return false;
        for (int c = 0; c < 3; c++) avg[c] /= wsum;

        if (brush == 3) {
            const unsigned now = brush_frame_id();
            if (!s_smudge_valid || now - s_smudge_frame > 3u) { for (int c = 0; c < 3; c++) s_smudge[c] = avg[c]; s_smudge_valid = true; }
            s_smudge_frame = now;
        }
    }

    for (uint32_t i = 0; i < mesh->vert_count; i++) {
        float* col = mesh->verts[i].obj_vert_color;

        if (brush == 5) {                                   /* Fill: the whole mesh, no radius */
            mix3(col, g_paint.color, g_paint.opacity);
            changed = true;
            continue;
        }

        const float dx = mesh->verts[i].obj_vert_pos.x - ctr.x, dy = mesh->verts[i].obj_vert_pos.y - ctr.y, dz = mesh->verts[i].obj_vert_pos.z - ctr.z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= r2) continue;
        const float w = paint_falloff(sqrtf(d2), R, g_paint.hardness);
        if (w <= 0.0f) continue;
        const float influence = w * g_paint.opacity;

        switch (brush) {
        case 2:  mix3(col, white, influence);                          break;   /* Eraser: back to neutral white */
        case 0:  mix3(col, g_paint.color, influence);                  break;   /* Paint */
        case 1:  mix3(col, g_paint.color, influence * 0.30f);          break;   /* Airbrush: builds up slowly */
        case 4:  mix3(col, avg, influence);                            break;   /* Blur */
        case 3:  mix3(col, s_smudge, influence * 0.60f);               break;   /* Smudge: drags the carried colour */
        case 7:  for (int c = 0; c < 3; c++) col[c] += (1.0f - col[c]) * influence * 0.25f; break;   /* Lighten */
        case 8:  for (int c = 0; c < 3; c++) col[c] *= (1.0f - influence * 0.25f);          break;   /* Darken */
        default: continue;
        }
        changed = true;
    }

    if (brush == 3) mix3(s_smudge, avg, 0.35f);             /* pick up a bit of what is under the brush now */
    {
        static unsigned dab_n = 0;
        dab_n++;
        if (dab_n <= 20 || (dab_n % 60) == 0)
            phoenix_info("Paint", "paint_dab #%u: brush %d at (%.2f,%.2f,%.2f) r=%.2f on mesh with %u verts, changed=%d", dab_n, brush, ctr.x, ctr.y, ctr.z, R, (unsigned)mesh->vert_count, (int)changed);
    }
    return changed;
}