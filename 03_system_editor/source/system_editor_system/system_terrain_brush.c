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
float g_terrain_brush_drag_dy = 0.0f;       /* how far the mouse moved up (+) or down (-) the screen this frame, in pixels: the Grab brush uses it */
int   g_terrain_brush_prev_my = 0;

/* system_terrain.c (no header declares these: without a prototype C reads the float result as an int) */
void  terrain_refresh_height_cache(void);
float terrain_corner_height(int gx, int gz);
void  terrain_set_corner_height(int gx, int gz, float y);
void  terrain_mark_dirty_rect(int gx0, int gz0, int gx1, int gz1);
void  terrain_touch(void);
bool  terrain_paint_dab(editor_vec3 centre, float radius, const float color[3], float opacity, float hardness, bool erase, int only_region);
float get_object_pick_depth(const system_element_context* ui_ctx, const system_scene_object* obj, float mx, float my);   /* system_object_pick.c */

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

bool get_mouse_world_ray(const system_element_context* ui_ctx, float mx, float my, editor_vec3* out_origin, editor_vec3* out_dir);   /* system_object_pick.c */

/* ---- Grab: take hold of the ground under the cursor and move it, like picking up a cloth ----
   at the start the heights of the brush area are saved. while the button is down the ground follows the cursor straight up and down
   (the cursor is tracked on a vertical plane through the point you grabbed): every column = its saved height + the move * how close
   it is to the middle of the brush. nothing accumulates, so it can not drift, flip or sink under. */
bool         g_grab_active = false;
float*       g_grab_heights = NULL;
int          g_grab_gx0 = 0, g_grab_gz0 = 0, g_grab_gx1 = 0, g_grab_gz1 = 0;
float        g_grab_local_x = 0.0f, g_grab_local_z = 0.0f, g_grab_radius = 0.0f, g_grab_hardness = 0.0f;
editor_vec3  g_grab_start = { 0.0f, 0.0f, 0.0f };
editor_vec3  g_grab_normal = { 0.0f, 0.0f, 1.0f };

void end_terrain_grab(void)
{
    free(g_grab_heights);
    g_grab_heights = NULL;
    g_grab_active = false;
}

void begin_terrain_grab(float local_x, float local_z, editor_vec3 hit, const sculpt_brush_settings* s, editor_vec3 ray_dir)
{
    end_terrain_grab();
    terrain_refresh_height_cache();
    const float cell = g_terrain.settings.cell_size_m;
    int gx0 = (int)floorf((local_x - s->radius) / cell + (float)g_terrain.dims.origin_x);
    int gx1 = (int)ceilf((local_x + s->radius) / cell + (float)g_terrain.dims.origin_x);
    int gz0 = (int)floorf((local_z - s->radius) / cell + (float)g_terrain.dims.origin_z);
    int gz1 = (int)ceilf((local_z + s->radius) / cell + (float)g_terrain.dims.origin_z);
    if (gx0 < 0) { gx0 = 0; }
    if (gz0 < 0) { gz0 = 0; }
    if (gx1 > g_terrain.dims.cells_x) { gx1 = g_terrain.dims.cells_x; }
    if (gz1 > g_terrain.dims.cells_z) { gz1 = g_terrain.dims.cells_z; }
    if (gx1 < gx0 || gz1 < gz0) {
        return;
    }
    const int width = gx1 - gx0 + 1;
    const int depth = gz1 - gz0 + 1;
    g_grab_heights = (float*)malloc(sizeof(float) * (size_t)width * (size_t)depth);
    if (!g_grab_heights) {
        return;
    }
    for (int gz = gz0; gz <= gz1; gz++) {
        for (int gx = gx0; gx <= gx1; gx++) {
            g_grab_heights[(gz - gz0) * width + (gx - gx0)] = terrain_corner_height(gx, gz);
        }
    }
    g_grab_gx0 = gx0; g_grab_gx1 = gx1; g_grab_gz0 = gz0; g_grab_gz1 = gz1;
    g_grab_local_x = local_x; g_grab_local_z = local_z;
    g_grab_radius = s->radius; g_grab_hardness = s->hardness;
    g_grab_start = hit;
    const float horizontal = sqrtf(ray_dir.x * ray_dir.x + ray_dir.z * ray_dir.z);
    if (horizontal > 1e-4f) {
        g_grab_normal = (editor_vec3){ ray_dir.x / horizontal, 0.0f, ray_dir.z / horizontal };
    } else {
        g_grab_normal = (editor_vec3){ 0.0f, 0.0f, 1.0f };
    }
    g_grab_active = true;
}

void update_terrain_grab(const system_element_context* ui_ctx)
{
    if (!g_grab_active || !g_grab_heights) {
        return;
    }
    editor_vec3 origin, dir;
    if (!get_mouse_world_ray(ui_ctx, (float)ui_ctx->input.mouse_x, (float)ui_ctx->input.mouse_y, &origin, &dir)) {
        return;
    }
    const float denom = dir.x * g_grab_normal.x + dir.y * g_grab_normal.y + dir.z * g_grab_normal.z;
    if (fabsf(denom) < 1e-4f) {
        return;
    }
    const float t = ((g_grab_start.x - origin.x) * g_grab_normal.x + (g_grab_start.y - origin.y) * g_grab_normal.y + (g_grab_start.z - origin.z) * g_grab_normal.z) / denom;
    const float move = (origin.y + dir.y * t) - g_grab_start.y;          /* how far the cursor went up (+) or down (-) from where you grabbed */

    const float cell = g_terrain.settings.cell_size_m;
    const int width = g_grab_gx1 - g_grab_gx0 + 1;
    bool changed = false;
    terrain_refresh_height_cache();
    for (int gz = g_grab_gz0; gz <= g_grab_gz1; gz++) {
        for (int gx = g_grab_gx0; gx <= g_grab_gx1; gx++) {
            const float wx = ((float)(gx - g_terrain.dims.origin_x)) * cell;
            const float wz = ((float)(gz - g_terrain.dims.origin_z)) * cell;
            const float falloff = get_terrain_brush_falloff(hypotf(wx - g_grab_local_x, wz - g_grab_local_z), g_grab_radius, g_grab_hardness);
            if (falloff <= 0.0f) {
                continue;
            }
            const float base = g_grab_heights[(gz - g_grab_gz0) * width + (gx - g_grab_gx0)];
            const float target = base + move * falloff;
            if (target != terrain_corner_height(gx, gz)) {
                terrain_set_corner_height(gx, gz, target);
                changed = true;
            }
        }
    }
    if (changed) {
        terrain_mark_dirty_rect(g_grab_gx0 - 1, g_grab_gz0 - 1, g_grab_gx1 + 1, g_grab_gz1 + 1);
        terrain_touch();
    }
}

/* one sculpt dab at a terrain-local position. every sculpt brush does its own thing on the height grid:
     Standard      raise (Ctrl lowers), soft edge
     Clay Buildup  raise with a flat, wide top (builds up plateaus)
     Clay Soften   smooth, gently
     Carve         lower with a sharp edge (Ctrl raises)
     Grab          drag the ground up and down with the mouse
     Polish        smooth hard (flattens bumps)
     Inflate       raise with a pointed peak
     Smooth        smooth
     Flatten       pull toward the height where the stroke started
     Pinch         pull the ground in toward the middle of the brush (Ctrl spreads it out) */
void apply_terrain_sculpt_dab(float local_x, float local_z, enum_sculpt_brush brush, const sculpt_brush_settings* s, bool invert)
{
    terrain_refresh_height_cache();
    const float cell = g_terrain.settings.cell_size_m;
    const int origin_x = g_terrain.dims.origin_x;
    const int origin_z = g_terrain.dims.origin_z;
    const int cells_x = g_terrain.dims.cells_x;
    const int cells_z = g_terrain.dims.cells_z;
    const bool sandbox = (g_terrain.settings.mode == terrain_mode_sandbox);

    /* sandbox: heights are whole cubes, so a dab every few frames moves the columns in the middle of the brush by exactly one cube.
       open world: a smooth push every frame, strength decides how fast. */
    float amount = s->strength * cell * 0.06f;
    if (sandbox) {
        int interval = 2 + (int)((1.0f - s->strength) * 14.0f);   /* stronger = a cube more often */
        if ((g_terrain_brush_frame % interval) != 0) {
            return;
        }
        amount = cell;
    }
    if (brush == sculpt_brush_carve) {
        invert = !invert;                                  /* carve digs, Ctrl raises */
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

    const bool smoothing = (brush == sculpt_brush_smooth || brush == sculpt_brush_polish || brush == sculpt_brush_clay_soften);
    /* smoothing reads the heights from before the dab (the snapshot) and averages the column with its 4 neighbors. the pull is capped at 0.6 so a
       checkerboard of high and low columns settles instead of flipping every dab (that was the alternating up / down look). */
    float smooth_k = s->strength * 0.5f;
    if (brush == sculpt_brush_polish) {
        smooth_k = s->strength * 0.9f;
    } else if (brush == sculpt_brush_clay_soften) {
        smooth_k = s->strength * 0.3f;
    }
    if (smooth_k > 0.6f) {
        smooth_k = 0.6f;
    }

    bool changed = false;
    for (int gz = gz0; gz <= gz1; gz++) {
        for (int gx = gx0; gx <= gx1; gx++) {
            const float wx = ((float)(gx - origin_x)) * cell;
            const float wz = ((float)(gz - origin_z)) * cell;
            float falloff = get_terrain_brush_falloff(hypotf(wx - local_x, wz - local_z), s->radius, s->hardness);
            if (falloff <= 0.0f) {
                continue;
            }
            const float current = heights[(gz - gz0) * width + (gx - gx0)];
            float target = current;
            if (smoothing) {
                float sum = current;
                int count = 1;
                for (int k = 0; k < 4; k++) {
                    int nx = gx;
                    int nz = gz;
                    if (k == 0) { nx = gx - 1; }
                    if (k == 1) { nx = gx + 1; }
                    if (k == 2) { nz = gz - 1; }
                    if (k == 3) { nz = gz + 1; }
                    if (nx < 0 || nx > cells_x || nz < 0 || nz > cells_z) {
                        continue;
                    }
                    if (nx >= gx0 && nx <= gx1 && nz >= gz0 && nz <= gz1) {
                        sum += heights[(nz - gz0) * width + (nx - gx0)];       /* the snapshot: not the columns this dab already moved */
                    } else {
                        sum += terrain_corner_height(nx, nz);
                    }
                    count++;
                }
                if (sandbox) {
                    if (falloff >= 0.35f) {
                        target = sum / (float)count;                        /* cubes are whole: settle on the neighbours' average (the terrain rounds it) */
                    }
                } else {
                    target = current + (sum / (float)count - current) * falloff * smooth_k;
                }
            } else if (brush == sculpt_brush_pinch) {
                /* pull the ground in toward the middle of the brush: the column takes the height of its neighbor one step closer to the middle
                   (Ctrl: one step further out, so the shape spreads) */
                float step_x = local_x - wx;
                float step_z = local_z - wz;
                if (invert) {
                    step_x = -step_x;
                    step_z = -step_z;
                }
                int nx = gx;
                int nz = gz;
                if (fabsf(step_x) >= fabsf(step_z)) {
                    if (step_x > 0.0f) { nx = gx + 1; } else if (step_x < 0.0f) { nx = gx - 1; }
                } else {
                    if (step_z > 0.0f) { nz = gz + 1; } else if (step_z < 0.0f) { nz = gz - 1; }
                }
                if (nx >= 0 && nx <= cells_x && nz >= 0 && nz <= cells_z && (nx != gx || nz != gz)) {
                    float neighbor;
                    if (nx >= gx0 && nx <= gx1 && nz >= gz0 && nz <= gz1) {
                        neighbor = heights[(nz - gz0) * width + (nx - gx0)];
                    } else {
                        neighbor = terrain_corner_height(nx, nz);
                    }
                    if (sandbox) {
                        if (falloff >= 0.35f) {
                            target = neighbor;
                        }
                    } else {
                        float pull = falloff * s->strength;
                        if (pull > 0.8f) { pull = 0.8f; }
                        target = current + (neighbor - current) * pull;
                    }
                }
            } else if (brush == sculpt_brush_clay_buildup) {
                /* builds layers up to a flat top: the top is the ground where the stroke started plus the layer thickness, and a column
                   never goes past it (Ctrl scrapes down to a flat floor the same way) */
                float thickness = s->strength * s->radius * 0.5f + s->plane_offset * s->radius * 0.2f;
                if (thickness < cell) { thickness = cell; }
                float push = falloff;
                if (sandbox) {
                    push = 0.0f;
                    if (falloff >= 0.2f) { push = 1.0f; }
                }
                if (!invert) {
                    const float top = g_terrain_brush_flatten_height + thickness;
                    if (current < top) {
                        target = current + amount * push;
                        if (target > top) { target = top; }
                    }
                } else {
                    const float floor_height = g_terrain_brush_flatten_height - thickness;
                    if (current > floor_height) {
                        target = current + amount * push;
                        if (target < floor_height) { target = floor_height; }
                    }
                }
            } else if (brush == sculpt_brush_flatten) {
                target = current + (g_terrain_brush_flatten_height - current) * falloff * s->strength;
            } else {
                float shape = falloff;
                if (brush == sculpt_brush_inflate) {
                    shape = falloff * falloff;                  /* pointed peak */
                } else if (brush == sculpt_brush_carve) {
                    shape = falloff * falloff;                  /* sharp trench */
                }
                float push = shape;
                if (sandbox) {
                    push = 0.0f;
                    if (shape >= 0.35f) {
                        push = 1.0f;                            /* a whole cube for the columns inside the brush */
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
/* what a sculpt brush does to the ground, said in words (shown in the status bar when a stroke starts) */
const char* get_terrain_brush_effect(enum_sculpt_brush brush, bool invert)
{
    switch (brush) {
    case sculpt_brush_standard:     if (invert) { return "lowering"; } return "raising";
    case sculpt_brush_clay_buildup: if (invert) { return "scraping down"; } return "building up";
    case sculpt_brush_clay_soften:  return "softening bumps";
    case sculpt_brush_carve:        if (invert) { return "raising"; } return "digging";
    case sculpt_brush_grab:         return "dragging with the mouse";
    case sculpt_brush_polish:       return "polishing flat";
    case sculpt_brush_inflate:      if (invert) { return "deflating"; } return "inflating";
    case sculpt_brush_smooth:       return "smoothing";
    case sculpt_brush_flatten:      return "flattening";
    case sculpt_brush_pinch:        if (invert) { return "spreading out"; } return "pinching in";
    default:                        return "sculpting";
    }
}

const char* get_terrain_tool_name(int tool)
{
    switch (tool) {
    case terrain_tool_off:        return "Select";
    case terrain_tool_raise:      return "Raise";
    case terrain_tool_lower:      return "Lower";
    case terrain_tool_smooth:     return "Smooth";
    case terrain_tool_flatten:    return "Flatten";
    case terrain_tool_set_height: return "Set Height";
    default:                      return "?";
    }
}

/* Edit, Sculpt and Paint on the terrain. called once per frame from brush_ui_frame (draw_system_sceneview.c), after the brush ring
   was cleared for the frame, so the ring set here is the one that gets drawn.
     Edit    the terrain tools of the Modeler window (raise, lower, smooth, flatten, set height). Needs the terrain selected.
     Sculpt  the sculpt brushes (Brushes + Sculpt windows).
     Paint   the paint brush (Brushes + Paint windows).
   a selected mesh that is not terrain keeps its own sculpt / paint input while the mouse is over it. anywhere else the terrain is used. */
void update_terrain_brush(system_element_context* ui_ctx)
{
    if (!ui_ctx) {
        return;
    }
    g_terrain_brush_frame++;
    const bool down = ui_ctx->input.mouse_down;
    const bool press = down && !g_terrain_brush_prev_down;
    g_terrain_brush_prev_down = down;
    if (g_grab_active && !(down && g_terrain_brush_stroke)) {
        end_terrain_grab();                                    /* the button went up (or the stroke was cancelled) */
    }
    g_terrain_brush_drag_dy = (float)(g_terrain_brush_prev_my - ui_ctx->input.mouse_y);
    g_terrain_brush_prev_my = ui_ctx->input.mouse_y;

    const system_scene_object* selected = find_scene_object_by_id(g_scene_registry.selected_object_id);
    const bool terrain_selected = selected != NULL && is_terrain_object(selected);

    const bool sculpt = mode_state_is_sculpt() && sculpt_mode_is_active();
    const bool paint = mode_state_is_paint() && paint_mode_is_active();
    const bool edit = mode_state_is_edit() && terrain_selected && g_terrain_tool.tool != terrain_tool_off;   /* Select = the normal vertex / edge / face editing */
    if ((!sculpt && !paint && !edit) || !g_terrain.exists) {
        g_terrain_brush_stroke = false;
        return;
    }

    if (ui_ctx->input.alt_pressed || ui_ctx->input.camera_manipulating || ui_ctx->input.middle_down || is_context_menu_open() ||
        !is_edit_in_scene_view(ui_ctx) || is_manip_overlay_using_mouse()) {
        g_terrain_brush_stroke = false;
        return;
    }

    /* a selected mesh that is not terrain is sculpted / painted by the normal mesh input while the mouse is over it */
    if (!edit && selected && selected != &g_scene_registry.objects[0] && !terrain_selected && selected->type == scene_object_mesh) {
        if (get_object_pick_depth(ui_ctx, selected, (float)ui_ctx->input.mouse_x, (float)ui_ctx->input.mouse_y) >= 0.0f) {
            g_terrain_brush_stroke = false;
            return;
        }
    }

    editor_vec3 hit;
    float depth = 0.0f;
    if (!get_terrain_ray_hit(ui_ctx, (float)ui_ctx->input.mouse_x, (float)ui_ctx->input.mouse_y, &hit, &depth)) {
        if (!down) {
            g_terrain_brush_stroke = false;
        }
        return;
    }
    editor_vec3 root = { 0.0f, 0.0f, 0.0f };
    terrain_root_position(&root);
    const float local_x = hit.x - root.x;
    const float local_z = hit.z - root.z;
    const float cell_size = g_terrain.settings.cell_size_m;

    bool invert = input_is_key_down(phoenix_key_control) != g_sculpt.invert;
    sculpt_brush_settings terrain_settings = *sculpt_current_settings();
    enum_sculpt_brush brush = sculpt_get_brush();

    if (edit) {
        /* the Modeler window's terrain tools: only Y changes */
        terrain_settings.radius   = g_terrain_tool.radius_cells * cell_size;
        terrain_settings.strength = g_terrain_tool.strength;
        terrain_settings.hardness = g_terrain_tool.hardness;
        invert = input_is_key_down(phoenix_key_control);
        switch (g_terrain_tool.tool) {
        case terrain_tool_lower:
            brush = sculpt_brush_standard;
            invert = !invert;
            break;
        case terrain_tool_smooth:
            brush = sculpt_brush_smooth;
            break;
        case terrain_tool_flatten:
        case terrain_tool_set_height:
            brush = sculpt_brush_flatten;
            break;
        default:
            brush = sculpt_brush_standard;
            break;
        }
    } else {
        /* the Sculpt window's size is in cubes on the terrain (a 4 m cell is the smallest thing the ground has), so 2 means two cubes wide */
        terrain_settings.radius = terrain_settings.radius * cell_size;
    }
    if (terrain_settings.radius < cell_size * 1.5f) {
        terrain_settings.radius = cell_size * 1.5f;
    }
    sculpt_brush_settings* s = &terrain_settings;
    float paint_radius = g_paint.radius * cell_size;     /* paint is stored per cube column: the size is in cubes too */
    if (paint_radius < cell_size * 1.5f) {
        paint_radius = cell_size * 1.5f;
    }

    if (paint) {
        brush_cursor_set(ui_ctx, hit, paint_radius, g_paint.hardness, NULL, g_paint.color);
    } else {
        const float tint[3] = { 1.0f, 0.45f, 0.45f };
        const float* ring_tint = NULL;
        if (invert) {
            ring_tint = tint;
        }
        brush_cursor_set(ui_ctx, hit, s->radius, s->hardness, ring_tint, NULL);
    }

    if (!g_terrain_brush_stroke) {
        if (!press) {
            return;
        }
        g_terrain_brush_stroke = true;
        if (paint) {
            set_statusbar_message("Terrain paint: drag to paint, Ctrl erases.");
        } else if (edit) {
            set_statusbar_message("Terrain %s: %s (Ctrl does the opposite).", get_terrain_tool_name(g_terrain_tool.tool), invert ? "pushing down" : "pushing up");
        } else {
            set_statusbar_message("Terrain %s: %s (Ctrl does the opposite).", sculpt_brush_name(brush), get_terrain_brush_effect(brush, invert));
        }
        g_terrain_brush_flatten_height = hit.y - root.y;
        if (edit && g_terrain_tool.tool == terrain_tool_set_height) {
            g_terrain_brush_flatten_height = g_terrain_tool.height_cells * cell_size;
        }
        if (!paint && !edit && brush == sculpt_brush_grab) {
            editor_vec3 grab_origin, grab_dir;
            if (get_mouse_world_ray(ui_ctx, (float)ui_ctx->input.mouse_x, (float)ui_ctx->input.mouse_y, &grab_origin, &grab_dir)) {
                begin_terrain_grab(local_x, local_z, hit, s, grab_dir);
            }
        }
    }
    if (!down) {
        g_terrain_brush_stroke = false;
        return;
    }

    if (paint) {
        const float color[3] = { g_paint.color[0], g_paint.color[1], g_paint.color[2] };   /* stored as sRGB: the mesher converts it to linear once */
        terrain_paint_dab((editor_vec3){ local_x, hit.y - root.y, local_z }, paint_radius, color, g_paint.opacity * 0.5f,
                          g_paint.hardness, input_is_key_down(phoenix_key_control), -1);
    } else if (!edit && brush == sculpt_brush_grab) {
        update_terrain_grab(ui_ctx);
    } else {
        apply_terrain_sculpt_dab(local_x, local_z, brush, s, invert);
    }
}