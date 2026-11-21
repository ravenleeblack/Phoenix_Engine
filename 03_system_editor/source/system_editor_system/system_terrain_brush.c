#include "system_globals.h"

/* ========================================================================= */
/* terrain brush: Sculpt and Paint mode on the terrain, like Unity / Unreal.    */
/* a ring follows the cursor on the ground, hold the left button and drag.      */
/*   Sculpt  raise (Ctrl lowers), smooth, flatten. size / strength / hardness   */
/*           come from the Brushes + Sculpt windows (the sculpt settings).      */
/*   Paint   colour, size, opacity, hardness from the Paint window. Ctrl erases. */
/* the paint is stored per cube column across the whole terrain (not a tiling   */
/* picture), so the Terrain skin only tints / shines it.                        */
/* every other mesh keeps the old sculpt / paint input.                         */
/* ========================================================================= */
bool  g_terrain_brush_stroke = false;
bool  g_terrain_brush_prev_down = false;
float g_terrain_brush_flatten_height = 0.0f;
int   g_terrain_brush_frame = 0;

/* system_terrain.c (no header declares these: without a prototype C reads the float result as an int) */
void  terrain_refresh_height_cache(void);
float terrain_corner_height(int gx, int gz);
void  terrain_set_corner_height(int gx, int gz, float y);
void  terrain_mark_dirty_rect(int gx0, int gz0, int gx1, int gz1);
void  terrain_touch(void);
bool  terrain_paint_dab(editor_vec3 centre, float radius, const float color[3], float opacity, float hardness, bool erase, int only_region);

float get_terrain_brush_falloff(float distance, float radius, float hardness)
{
    if (radius <= 0.0f || distance >= radius) {
        return 0.0f;
    }
    const float t = distance / radius;
    if (t <= hardness) {
        return 1.0f;
    }
    float f = 1.0f - (t - hardness) / (1.0f - hardness);
    if (f < 0.0f) {
        f = 0.0f;
    }
    return f * f * (3.0f - 2.0f * f);
}

float convert_terrain_brush_color(float srgb)
{
    if (!g_swapchain_is_srgb) {
        return srgb;
    }
    if (srgb <= 0.04045f) {
        return srgb / 12.92f;
    }
    return powf((srgb + 0.055f) / 1.055f, 2.4f);
}

/* one sculpt dab at a terrain-local position */
void apply_terrain_sculpt_dab(float local_x, float local_z, enum_sculpt_brush brush, const sculpt_brush_settings* s, bool invert)
{
    terrain_refresh_height_cache();
    const float cell = g_terrain.settings.cell_size_m;
    const int origin_x = g_terrain.dims.origin_x;
    const int origin_z = g_terrain.dims.origin_z;
    const int cells_x = g_terrain.dims.cells_x;
    const int cells_z = g_terrain.dims.cells_z;
    const bool sandbox = (g_terrain.settings.mode == terrain_mode_sandbox);

    /* open world: a smooth push every frame. sandbox: heights are whole cubes, so a dab every few frames moves the columns in the
       middle of the brush by exactly one cube. */
    float amount = s->strength * 0.12f;
    if (sandbox) {
        if ((g_terrain_brush_frame % 10) != 0) {
            return;
        }
        amount = cell;
    }
    if (invert) {
        amount = -amount;
    }

    int gx0 = (int)floorf((local_x - s->radius) / cell + (float)origin_x);
    int gx1 = (int)ceilf((local_x + s->radius) / cell + (float)origin_x);
    int gz0 = (int)floorf((local_z - s->radius) / cell + (float)origin_z);
    int gz1 = (int)ceilf((local_z + s->radius) / cell + (float)origin_z);
    if (gx0 < 0) { gx0 = 0; }
    if (gz0 < 0) { gz0 = 0; }
    if (gx1 > cells_x) { gx1 = cells_x; }
    if (gz1 > cells_z) { gz1 = cells_z; }
    if (gx1 < gx0 || gz1 < gz0) {
        return;
    }

    const int width = gx1 - gx0 + 1;
    const int depth = gz1 - gz0 + 1;
    float* heights = (float*)malloc(sizeof(float) * (size_t)width * (size_t)depth);
    if (!heights) {
        return;
    }
    for (int gz = gz0; gz <= gz1; gz++) {
        for (int gx = gx0; gx <= gx1; gx++) {
            heights[(gz - gz0) * width + (gx - gx0)] = terrain_corner_height(gx, gz);
        }
    }

    bool changed = false;
    for (int gz = gz0; gz <= gz1; gz++) {
        for (int gx = gx0; gx <= gx1; gx++) {
            const float wx = ((float)(gx - origin_x)) * cell;
            const float wz = ((float)(gz - origin_z)) * cell;
            const float falloff = get_terrain_brush_falloff(hypotf(wx - local_x, wz - local_z), s->radius, s->hardness);
            if (falloff <= 0.0f) {
                continue;
            }
            const float current = heights[(gz - gz0) * width + (gx - gx0)];
            float target = current;
            if (brush == sculpt_brush_smooth || brush == sculpt_brush_polish || brush == sculpt_brush_clay_soften) {
                float sum = 0.0f;
                int count = 0;
                for (int k = 0; k < 4; k++) {
                    const int nx = gx + ((k == 0) ? -1 : ((k == 1) ? 1 : 0));
                    const int nz = gz + ((k == 2) ? -1 : ((k == 3) ? 1 : 0));
                    if (nx >= 0 && nx <= cells_x && nz >= 0 && nz <= cells_z) {
                        sum += terrain_corner_height(nx, nz);
                        count++;
                    }
                }
                if (count > 0) {
                    target = current + (sum / (float)count - current) * falloff * s->strength;
                }
            } else if (brush == sculpt_brush_flatten) {
                target = current + (g_terrain_brush_flatten_height - current) * falloff * s->strength;
            } else {
                float push = falloff;
                if (sandbox) {
                    push = 0.0f;
                    if (falloff >= 0.5f) {
                        push = 1.0f;                  /* a whole cube for the columns in the middle of the brush */
                    }
                }
                target = current + amount * push;
            }
            if (target != current) {
                terrain_set_corner_height(gx, gz, target);
                changed = true;
            }
        }
    }
    free(heights);
    if (changed) {
        terrain_mark_dirty_rect(gx0 - 1, gz0 - 1, gx1 + 1, gz1 + 1);
        terrain_touch();
    }
}

void update_terrain_brush(system_element_context* ui_ctx)
{
    if (!ui_ctx) {
        return;
    }
    g_terrain_brush_frame++;
    const bool down = ui_ctx->input.mouse_down;
    const bool press = down && !g_terrain_brush_prev_down;
    g_terrain_brush_prev_down = down;

    const bool sculpt = mode_state_is_sculpt() && sculpt_mode_is_active();
    const bool paint = mode_state_is_paint() && paint_mode_is_active();
    if ((!sculpt && !paint) || !g_terrain.exists) {
        g_terrain_brush_stroke = false;
        return;
    }

    /* a selected non-terrain mesh is sculpted / painted by the normal mesh input */
    const system_scene_object* selected = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (selected && selected != &g_scene_registry.objects[0] && !is_terrain_object(selected)) {
        g_terrain_brush_stroke = false;
        return;
    }
    if (ui_ctx->input.alt_pressed || ui_ctx->input.camera_manipulating || ui_ctx->input.middle_down || is_context_menu_open() ||
        !is_edit_in_scene_view(ui_ctx) || is_manip_overlay_using_mouse()) {
        g_terrain_brush_stroke = false;
        return;
    }

    editor_vec3 hit;
    float depth = 0.0f;
    if (!get_terrain_ray_hit(ui_ctx, (float)ui_ctx->input.mouse_x, (float)ui_ctx->input.mouse_y, &hit, &depth)) {
        return;
    }
    editor_vec3 root = { 0.0f, 0.0f, 0.0f };
    terrain_root_position(&root);
    const float local_x = hit.x - root.x;
    const float local_z = hit.z - root.z;

    const bool invert = input_is_key_down(phoenix_key_control) != g_sculpt.invert;
    /* the Sculpt window's size is in cubes on the terrain (a 4 m cell is the smallest thing the ground has), so 2 means two cubes wide */
    sculpt_brush_settings terrain_settings = *sculpt_current_settings();
    const float cell_size = g_terrain.settings.cell_size_m;
    terrain_settings.radius = terrain_settings.radius * cell_size;
    if (terrain_settings.radius < cell_size * 1.5f) {
        terrain_settings.radius = cell_size * 1.5f;
    }
    sculpt_brush_settings* s = &terrain_settings;
    float paint_radius = g_paint.radius * cell_size;     /* paint is stored per cube column: the size is in cubes too */
    if (paint_radius < cell_size * 1.5f) {
        paint_radius = cell_size * 1.5f;
    }
    if (sculpt) {
        const float tint[3] = { 1.0f, 0.45f, 0.45f };
        const float* ring_tint = NULL;
        if (invert) {
            ring_tint = tint;
        }
        brush_cursor_set(ui_ctx, hit, s->radius, s->hardness, ring_tint, NULL);
    } else {
        brush_cursor_set(ui_ctx, hit, paint_radius, g_paint.hardness, NULL, g_paint.color);
    }

    if (!g_terrain_brush_stroke) {
        if (!press || ui_ctx->active_id != 0) {
            return;
        }
        g_terrain_brush_stroke = true;
        g_terrain_brush_flatten_height = hit.y - root.y;
    }
    if (!down) {
        g_terrain_brush_stroke = false;
        return;
    }

    if (sculpt) {
        apply_terrain_sculpt_dab(local_x, local_z, sculpt_get_brush(), s, invert);
    } else {
        const float color[3] = {
            convert_terrain_brush_color(g_paint.color[0]),
            convert_terrain_brush_color(g_paint.color[1]),
            convert_terrain_brush_color(g_paint.color[2])
        };
        terrain_paint_dab((editor_vec3){ local_x, hit.y - root.y, local_z }, paint_radius, color, g_paint.opacity * 0.5f,
                          g_paint.hardness, input_is_key_down(phoenix_key_control), -1);
    }
}
