#include "system_globals.h"
#include "system_object_database.h"
#include <math.h>

/* Terrain Tools: see system_terrain_tools.h. Heights live in terrain local meters (the same numbers terrain_sample_height_m returns). */

bool  project_world_to_viewport(editor_vec3 world_pos, editor_mat4 view, editor_mat4 proj, struct_viewport_bounds view_bounds, float* out_x, float* out_y);
bool  terrain_add_tunnel(const terrain_tunnel* k);
bool  system_transform_gizmo_wants_mouse(void);
void  terrain_refresh_height_cache(void);
float terrain_corner_height(int gx, int gz);
void  terrain_set_corner_height(int gx, int gz, float y);
void  terrain_mark_dirty_rect(int gx0, int gz0, int gx1, int gz1);
void  terrain_touch(void);

enum { tt_grid_cols = 13, tt_grid_max_rings = 100, tt_radial_sectors = 32 };

terrain_tools_state g_terrain_tools = {
    .tool = tt_tool_none, .step = 1, .segments = 8, .start_edge = -1,
    .width = 10.0f, .depth = 5.0f, .bank = 8.0f, .ridge_height = 1.5f, .ridge_width = 6.0f
};

/* ------------------------------------------------------------------------- */
/* names                                                                      */
/* ------------------------------------------------------------------------- */
const char* get_terrain_tools_name(int tool)
{
    switch (tool) {
    case tt_tool_river:    return "River Tool";
    case tt_tool_tunnel:   return "Tunnel Tool";
    case tt_tool_mountain: return "Mountain Tool";
    case tt_tool_lake:     return "Lake Tool";
    case tt_tool_ramp:     return "Ramp Tool";
    case tt_tool_overhang: return "Overhang Tool";
    case tt_tool_canyon:   return "Canyon Tool";
    default:               return "No tool";
    }
}

bool is_terrain_tools_radial(int tool)
{
    return tool == tt_tool_mountain || tool == tt_tool_lake;
}

const char* get_terrain_tools_step_name(int tool, int step)
{
    const bool radial = is_terrain_tools_radial(tool);
    switch (step) {
    case 1:
        if (radial) { return "Pick the middle"; }
        return "Pick start and end";
    case 2:
        if (radial) { return "Segments and outline"; }
        return "Segments and sides";
    case 3:
        if (tool == tt_tool_mountain) { return "Height and shape"; }
        if (tool == tt_tool_ramp) { return "Rise and width"; }
        if (tool == tt_tool_tunnel) { return "Radius and depth"; }
        return "Depth and bottom shape";
    case 4:
        if (tool == tt_tool_tunnel) { return "Nothing to set"; }
        return "Ridges and overhang";
    default:
        return "Finalize";
    }
}

const char* get_terrain_tools_shape_name(int shape)
{
    switch (shape) {
    case tt_shape_square:  return "Square";
    case tt_shape_octagon: return "Octagon";
    default:               return "Rounded";
    }
}

const char* get_terrain_tools_pattern_name(int pattern)
{
    switch (pattern) {
    case tt_pattern_steps:  return "Steps";
    case tt_pattern_zigzag: return "Zigzag";
    default:                return "Smooth";
    }
}

bool is_terrain_tools_wanting_mouse(void)
{
    return g_terrain_tools.tool != tt_tool_none;
}

/* ------------------------------------------------------------------------- */
/* small math                                                                 */
/* ------------------------------------------------------------------------- */
float get_tt_clamp(float v, float lo, float hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

float get_tt_smooth(float t)
{
    t = get_tt_clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float get_tt_lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/* a polyline lookup: points are (t, value) sorted by t */
float get_tt_polyline(const float* pts, int count, float t)
{
    if (t <= pts[0]) { return pts[1]; }
    for (int i = 0; i + 1 < count; i++) {
        const float t0 = pts[i * 2];
        const float t1 = pts[(i + 1) * 2];
        if (t <= t1) {
            if (t1 - t0 < 1e-5f) { return pts[(i + 1) * 2 + 1]; }
            return get_tt_lerp(pts[i * 2 + 1], pts[(i + 1) * 2 + 1], (t - t0) / (t1 - t0));
        }
    }
    return pts[(count - 1) * 2 + 1];
}

/* the cut's cross section: 1 in the middle (the bottom), 0 at the edge. t = distance from the middle / half width */
float get_tt_profile(int shape, float t)
{
    t = get_tt_clamp(t, 0.0f, 1.0f);
    if (shape == tt_shape_square) {
        const float pts[] = { 0.0f, 1.0f, 0.88f, 1.0f, 1.0f, 0.0f };
        return get_tt_polyline(pts, 3, t);
    }
    if (shape == tt_shape_octagon) {
        const float pts[] = { 0.0f, 1.0f, 0.30f, 1.0f, 0.65f, 0.70f, 0.66f, 0.30f, 1.0f, 0.0f };
        return get_tt_polyline(pts, 5, t);
    }
    return sqrtf(1.0f - t * t);
}

/* a mountain's side: 1 in the middle, 0 at the foot */
float get_tt_mountain_profile(int shape, float t)
{
    t = get_tt_clamp(t, 0.0f, 1.0f);
    if (shape == tt_shape_square) {
        return 1.0f - get_tt_smooth((t - 0.55f) / 0.45f);
    }
    if (shape == tt_shape_octagon) {
        const float pts[] = { 0.0f, 1.0f, 0.30f, 1.0f, 0.65f, 0.62f, 1.0f, 0.0f };
        return get_tt_polyline(pts, 4, t);
    }
    return 0.5f + 0.5f * cosf(3.14159265f * t);
}

float get_tt_pattern_sign(int pattern, float position)
{
    const int block = (int)floorf(position);
    if (pattern == tt_pattern_steps) {
        if (((block / 2) % 2) == 0) { return 1.0f; }
        return -1.0f;
    }
    if (pattern == tt_pattern_zigzag) {
        if ((block % 2) == 0) { return 1.0f; }
        return -1.0f;
    }
    return 0.0f;
}

float get_tt_node_width(int node_index)
{
    const terrain_tools_state* s = &g_terrain_tools;
    return s->width * (1.0f + s->pattern_offset * get_tt_pattern_sign(s->pattern, (float)node_index));
}

/* a mountain / lake's radius in one direction: the outline pattern goes around the circle in `segments` parts */
float get_tt_radius_at(float angle)
{
    const terrain_tools_state* s = &g_terrain_tools;
    float turn = angle / 6.28318530718f;
    turn -= floorf(turn);
    const float position = turn * (float)s->segments;
    float sign = 0.0f;
    if (s->pattern == tt_pattern_steps) {
        sign = get_tt_pattern_sign(tt_pattern_steps, position);
    } else if (s->pattern == tt_pattern_zigzag) {
        const float f = position - floorf(position);
        sign = fabsf(f - 0.5f) * 4.0f - 1.0f;
    }
    return s->width * (1.0f + s->pattern_offset * sign);
}

/* ------------------------------------------------------------------------- */
/* tool state                                                                 */
/* ------------------------------------------------------------------------- */
void reset_terrain_tool_picks(void)
{
    terrain_tools_state* s = &g_terrain_tools;
    s->picks = 0;
    s->step = 1;
    s->node_count = 0;
    s->nodes_edited = false;
    s->start_edge = -1;
    memset(s->node_selected, 0, sizeof(s->node_selected));
    s->dense_count = 0;
}

void select_terrain_tool(int tool)
{
    terrain_tools_state* s = &g_terrain_tools;
    s->tool = tool;
    s->separate = false;
    s->bend = 0.0f;
    s->curve = tt_curve_s;
    s->segments = 8;
    s->pattern = tt_pattern_smooth;
    s->pattern_offset = 0.0f;
    s->shape = tt_shape_rounded;
    s->lip = 0.0f;
    s->walls = tt_walls_sloped;
    s->ridge_height = 1.5f;
    s->ridge_width = 6.0f;
    s->bank = 8.0f;
    s->width = 10.0f;
    s->depth = 5.0f;
    switch (tool) {
    case tt_tool_canyon:
        s->bend = 0.2f;
        s->walls = tt_walls_vertical;
        s->width = 20.0f; s->depth = 24.0f; s->bank = 6.0f; s->shape = tt_shape_square;
        s->pattern = tt_pattern_steps; s->pattern_offset = 0.25f; s->ridge_height = 3.0f;
        break;
    case tt_tool_lake:
        s->width = 40.0f; s->depth = 8.0f; s->bank = 12.0f; s->ridge_height = 1.0f; s->segments = 12;
        break;
    case tt_tool_mountain:
        s->width = 50.0f; s->depth = 40.0f; s->bank = 0.0f; s->ridge_height = 6.0f; s->ridge_width = 0.0f; s->segments = 8;
        break;
    case tt_tool_ramp:
        s->width = 10.0f; s->depth = 12.0f; s->bank = 8.0f; s->ridge_height = 1.0f;
        break;
    case tt_tool_tunnel:
        s->width = 5.0f; s->depth = 10.0f; s->segments = 6; s->ridge_height = 0.0f;
        break;
    case tt_tool_overhang:
        s->width = 12.0f; s->depth = 6.0f; s->separate = true; s->lip = 3.0f; s->ridge_height = 2.0f;
        break;
    default:
        break;
    }
    if (tool == tt_tool_river) { s->bend = 0.25f; }
    if (tool == tt_tool_tunnel) { s->bend = 0.2f; }
    reset_terrain_tool_picks();
    if (tool != tt_tool_none) {
        const char* first_pick = "start";
        if (is_terrain_tools_radial(tool)) {
            first_pick = "middle";
        }
        set_statusbar_message("%s: click the terrain to pick the %s.", get_terrain_tools_name(tool), first_pick);
    }
}

void close_terrain_tool(void)
{
    g_terrain_tools.tool = tt_tool_none;
    reset_terrain_tool_picks();
}

void set_terrain_tool_step(int step)
{
    terrain_tools_state* s = &g_terrain_tools;
    const bool radial = is_terrain_tools_radial(s->tool);
    int needed = 2;
    if (radial) { needed = 1; }
    if (step < 1) { step = 1; }
    if (step > tt_step_count) { step = tt_step_count; }
    if (step > 1 && s->picks < needed) {
        set_statusbar_message("%s: finish step 1 first (click the terrain).", get_terrain_tools_name(s->tool));
        return;
    }
    s->step = step;
}

/* start / end / bend -> a quadratic curve cut into `segments` pieces */
void build_terrain_tool_nodes(void)
{
    terrain_tools_state* s = &g_terrain_tools;
    if (s->segments < 2) { s->segments = 2; }
    if (s->segments > tt_max_segments) { s->segments = tt_max_segments; }
    if (is_terrain_tools_radial(s->tool)) {
        s->node_count = 1;
        s->node[0][0] = s->start[0];
        s->node[0][1] = s->start[1];
        s->nodes_edited = false;
        select_all_terrain_tool_nodes();
        return;
    }
    const float dx = s->end[0] - s->start[0];
    const float dz = s->end[1] - s->start[1];
    float length = sqrtf(dx * dx + dz * dz);
    if (length < 1.0f) { length = 1.0f; }
    const float perp_x = -dz / length;
    const float perp_z = dx / length;
    /* a cubic curve. S: the two control points bow to opposite sides (a river that winds). Arc: both bow to the same side. */
    float first_side = s->bend * length;
    float second_side = -s->bend * length;
    if (s->curve == tt_curve_arc) {
        first_side = s->bend * length * 1.33f;
        second_side = first_side;
    }
    const float c1x = s->start[0] + dx / 3.0f + perp_x * first_side;
    const float c1z = s->start[1] + dz / 3.0f + perp_z * first_side;
    const float c2x = s->start[0] + dx * 2.0f / 3.0f + perp_x * second_side;
    const float c2z = s->start[1] + dz * 2.0f / 3.0f + perp_z * second_side;
    for (int i = 0; i <= s->segments; i++) {
        const float u = (float)i / (float)s->segments;
        const float k0 = (1.0f - u) * (1.0f - u) * (1.0f - u);
        const float k1 = 3.0f * (1.0f - u) * (1.0f - u) * u;
        const float k2 = 3.0f * (1.0f - u) * u * u;
        const float k3 = u * u * u;
        s->node[i][0] = k0 * s->start[0] + k1 * c1x + k2 * c2x + k3 * s->end[0];
        s->node[i][1] = k0 * s->start[1] + k1 * c1z + k2 * c2z + k3 * s->end[1];
    }    s->node_count = s->segments + 1;
    s->nodes_edited = false;
    select_all_terrain_tool_nodes();
}

/* the smooth curve through the vertices (Catmull-Rom), with a half width and the ground along it */
void prepare_terrain_tool(void)
{
    terrain_tools_state* s = &g_terrain_tools;
    s->dense_count = 0;
    if (!g_terrain.exists) { return; }
    if (is_terrain_tools_radial(s->tool)) {
        if (s->picks >= 1) { s->center_ground = terrain_sample_height_m(s->start[0], s->start[1]); }
        return;
    }
    if (s->node_count < 2) { return; }

    const int n = s->node_count - 1;
    int count = 0;
    for (int k = 0; k < n; k++) {
        int before = k - 1;
        int after = k + 2;
        if (before < 0) { before = 0; }
        if (after > n) { after = n; }
        const float* p0 = s->node[before];
        const float* p1 = s->node[k];
        const float* p2 = s->node[k + 1];
        const float* p3 = s->node[after];
        float w_a = get_tt_node_width(k);
        float w_b = get_tt_node_width(k + 1);
        if (s->pattern == tt_pattern_steps) {
            w_a = get_tt_node_width((k / 2) * 2);
            w_b = w_a;                                           /* the side holds for two segments, then jumps: a staircase */
        }
        for (int j = 0; j < tt_dense_per_segment && count < tt_max_dense - 1; j++) {
            const float t = (float)j / (float)tt_dense_per_segment;
            const float t2 = t * t;
            const float t3 = t2 * t;
            for (int axis = 0; axis < 2; axis++) {
                s->dense[count][axis] = 0.5f * ((2.0f * p1[axis]) + (-p0[axis] + p2[axis]) * t +
                                                (2.0f * p0[axis] - 5.0f * p1[axis] + 4.0f * p2[axis] - p3[axis]) * t2 +
                                                (-p0[axis] + 3.0f * p1[axis] - 3.0f * p2[axis] + p3[axis]) * t3);
            }
            s->dense[count][3] = get_tt_lerp(w_a, w_b, t);
            count++;
        }
    }
    s->dense[count][0] = s->node[n][0];
    s->dense[count][1] = s->node[n][1];
    s->dense[count][3] = get_tt_node_width(n);
    if (s->pattern == tt_pattern_steps) {
        s->dense[count][3] = get_tt_node_width(((n - 1) / 2) * 2);
    }
    count++;
    s->dense_count = count;

    float total = 0.0f;
    s->dense[0][2] = 0.0f;
    for (int i = 1; i < count; i++) {
        const float ex = s->dense[i][0] - s->dense[i - 1][0];
        const float ez = s->dense[i][1] - s->dense[i - 1][1];
        total += sqrtf(ex * ex + ez * ez);
        s->dense[i][2] = total;
    }
    s->path_length = total;
    if (total < 1e-3f) { total = 1e-3f; }
    for (int i = 0; i < count; i++) { s->dense[i][2] /= total; }

    float raw[tt_max_dense];
    for (int i = 0; i < count; i++) { raw[i] = terrain_sample_height_m(s->dense[i][0], s->dense[i][1]); }
    s->ramp_start_height = raw[0];
    s->ramp_end_height = raw[count - 1];
    for (int i = 0; i < count; i++) {                            /* the bed follows a smoothed ground: a river does not climb every little bump */
        float sum = 0.0f;
        int taken = 0;
        for (int k = i - 8; k <= i + 8; k++) {
            if (k >= 0 && k < count) { sum += raw[k]; taken++; }
        }
        s->dense_ground[i] = sum / (float)taken;
    }
}

/* nearest point of the curve: distance, half width there, the smoothed ground, how far along (0..1) */
bool query_terrain_tool_path(float x, float z, float* out_d, float* out_w, float* out_ground, float* out_arc)
{
    const terrain_tools_state* s = &g_terrain_tools;
    if (s->dense_count < 2) { return false; }
    float best = 1e30f;
    int best_i = 0;
    float best_t = 0.0f;
    float best_raw = 0.0f;
    for (int i = 0; i + 1 < s->dense_count; i++) {
        const float ax = s->dense[i][0], az = s->dense[i][1];
        const float bx = s->dense[i + 1][0], bz = s->dense[i + 1][1];
        const float ex = bx - ax, ez = bz - az;
        const float len2 = ex * ex + ez * ez;
        float t = 0.0f;
        float raw = 0.0f;
        if (len2 > 1e-8f) {
            raw = ((x - ax) * ex + (z - az) * ez) / len2;
            t = get_tt_clamp(raw, 0.0f, 1.0f);
        }
        const float px = ax + ex * t - x;
        const float pz = az + ez * t - z;
        const float d2 = px * px + pz * pz;
        if (d2 < best) { best = d2; best_i = i; best_t = t; best_raw = raw; }
    }
    /* the piece ends flush at the start and end edges: nothing is cut or raised beyond them (no cap, no ridge around the ends) */
    if ((best_i == 0 && best_raw < 0.0f) || (best_i == s->dense_count - 2 && best_raw > 1.0f)) {
        return false;
    }
    *out_d = sqrtf(best);
    *out_w = get_tt_lerp(s->dense[best_i][3], s->dense[best_i + 1][3], best_t);
    *out_ground = get_tt_lerp(s->dense_ground[best_i], s->dense_ground[best_i + 1], best_t);
    *out_arc = get_tt_lerp(s->dense[best_i][2], s->dense[best_i + 1][2], best_t);
    return true;
}

/* the height the ground gets at (x, z) when the tool is applied. returns `ground` when the tool does not touch it */
float get_terrain_tool_height(float x, float z, float ground)
{
    const terrain_tools_state* s = &g_terrain_tools;
    float d = 0.0f, w = 1.0f, cg = 0.0f, arc = 0.0f;

    if (is_terrain_tools_radial(s->tool)) {
        if (s->picks < 1) { return ground; }
        const float rx = x - s->start[0];
        const float rz = z - s->start[1];
        d = sqrtf(rx * rx + rz * rz);
        w = get_tt_radius_at(atan2f(rz, rx));
        cg = s->center_ground;
        if (w < 0.5f) { w = 0.5f; }
        if (s->tool == tt_tool_mountain) {
            const float t = d / w;
            if (t >= 1.0f) { return ground; }
            float prof = get_tt_mountain_profile(s->shape, t);
            if (s->walls == tt_walls_vertical) {
                prof = 1.0f;                                 /* a mesa: a flat top and a cliff all around */
            }
            const float lobes = cosf(atan2f(rz, rx) * (float)s->segments);
            const float ridge = s->ridge_height * prof * (0.5f + 0.5f * lobes);
            const float top = cg + s->depth * prof + ridge;
            if (top > ground) { return top; }
            return ground;
        }
    } else {
        if (!query_terrain_tool_path(x, z, &d, &w, &cg, &arc)) { return ground; }
        if (w < 0.5f) { w = 0.5f; }
    }

    if (s->tool == tt_tool_ramp) {
        const float ramp_height = get_tt_lerp(s->ramp_start_height, s->ramp_end_height + s->depth, arc);
        float result = ground;
        if (d <= w) {
            result = ramp_height;
        } else if (d < w + s->bank) {
            result = get_tt_lerp(ramp_height, ground, get_tt_smooth((d - w) / s->bank));
        }
        if (s->ridge_height > 0.01f && s->ridge_width > 0.01f && d > w && d < w + s->ridge_width) {
            const float rim = ramp_height + s->ridge_height * sinf(3.14159265f * (d - w) / s->ridge_width);
            if (rim > result) { result = rim; }
        }
        return result;
    }

    if (s->tool == tt_tool_overhang || s->tool == tt_tool_tunnel) {
        return ground;                                           /* these never change the heights */
    }

    /* river, canyon, lake: cut down, stitch the edge back into the ground with a slope, and raise the rim.
       vertical walls: the side drops straight down (this is a 3D height map) and the shape only rounds the floor */
    float result = ground;
    const bool vertical = (s->walls == tt_walls_vertical);
    if (d <= w) {
        float floor_fraction = get_tt_profile(s->shape, d / w);
        if (vertical) {
            floor_fraction = 1.0f - (1.0f - floor_fraction) * 0.35f;
        }
        const float target = cg - s->depth * floor_fraction;
        if (target < ground) { result = target; }
    } else if (!vertical && s->bank > 0.01f && d < w + s->bank) {
        const float flat = fminf(ground, cg);
        result = get_tt_lerp(flat, ground, get_tt_smooth((d - w) / s->bank));
    }
    if (s->ridge_height > 0.01f && s->ridge_width > 0.01f && d > w && d < w + s->ridge_width) {
        const float rim = cg + s->ridge_height * sinf(3.14159265f * (d - w) / s->ridge_width);
        if (rim > result) { result = rim; }
    }
    return result;
}

/* ------------------------------------------------------------------------- */
/* the grid of points the preview draws and a separate object is built from    */
/* ------------------------------------------------------------------------- */
/* out: rings * cols points (x, y, z) in terrain local meters. returns false when there is nothing to show yet.
   path pieces: a ring is one cross section along the curve. round pieces: a ring is one direction from the middle. */
bool build_terrain_tool_grid(float* out, int max_rings, int* out_rings, int* out_cols)
{
    const terrain_tools_state* s = &g_terrain_tools;
    const bool radial = is_terrain_tools_radial(s->tool);
    int rings = 0;
    const int cols = tt_grid_cols;
    if (radial) {
        if (s->picks < 1) { return false; }
        for (int j = 0; j <= tt_radial_sectors && rings < max_rings; j++, rings++) {
            const float angle = 6.28318530718f * (float)(j % tt_radial_sectors) / (float)tt_radial_sectors;
            const float radius_here = get_tt_radius_at(angle);
            const float extent = radius_here + s->ridge_width + s->bank;
            for (int k = 0; k < cols; k++) {
                float r = extent * (float)k / (float)(cols - 1);
                if (s->walls == tt_walls_vertical) {
                    if (k <= 9) {
                        r = (radius_here - 0.02f) * (float)k / 9.0f;           /* the floor, up to the wall */
                    } else if (k == 10) {
                        r = radius_here + 0.02f;                              /* just outside the wall: the top of the cliff */
                    } else if (k == 11) {
                        r = radius_here + (extent - radius_here) * 0.5f;
                    } else {
                        r = extent;
                    }
                }
                const float x = s->start[0] + cosf(angle) * r;
                const float z = s->start[1] + sinf(angle) * r;
                const float ground = terrain_sample_height_m(x, z);
                float* o = &out[(rings * cols + k) * 3];
                o[0] = x; o[1] = get_terrain_tool_height(x, z, ground); o[2] = z;
            }
        }
        *out_rings = rings;
        *out_cols = cols;
        return true;
    }

    if (s->dense_count < 2) { return false; }
    const int stride = tt_dense_per_segment / 2;
    for (int i = 0; i < s->dense_count && rings < max_rings; i += stride, rings++) {
        int ring_index = i;
        if (i + stride >= s->dense_count) {
            ring_index = s->dense_count - 1;                     /* the last ring sits on the end */
        }
        int i0 = ring_index - 1;
        int i1 = ring_index + 1;
        if (i0 < 0) { i0 = 0; }
        if (i1 > s->dense_count - 1) { i1 = s->dense_count - 1; }
        float tx = s->dense[i1][0] - s->dense[i0][0];
        float tz = s->dense[i1][1] - s->dense[i0][1];
        const float tl = sqrtf(tx * tx + tz * tz);
        if (tl > 1e-6f) { tx /= tl; tz /= tl; }
        const float nx = -tz;
        const float nz = tx;
        const float w = s->dense[ring_index][3];
        const float extent = w + s->ridge_width + s->bank;
        for (int k = 0; k < cols; k++) {
            float u = -extent + 2.0f * extent * (float)k / (float)(cols - 1);
            if (s->walls == tt_walls_vertical) {
                const float outer = (extent - w) * 0.5f;
                const float wall_u[13] = { -extent, -(w + outer), -(w + 0.02f), -(w - 0.02f), -w * 0.66f, -w * 0.33f, 0.0f,
                                           w * 0.33f, w * 0.66f, w - 0.02f, w + 0.02f, w + outer, extent };
                u = wall_u[k];                                                  /* two samples a hair apart on each wall: it drops straight */
            }
            const float x = s->dense[ring_index][0] + nx * u;
            const float z = s->dense[ring_index][1] + nz * u;
            const float ground = terrain_sample_height_m(x, z);
            float y = get_terrain_tool_height(x, z, ground);
            const float cg = s->dense_ground[ring_index];
            const float abs_u = fabsf(u);
            if (s->tool == tt_tool_overhang) {
                const float t = get_tt_clamp(abs_u / w, 0.0f, 1.0f);
                y = cg + s->depth * (0.6f + 0.4f * get_tt_profile(s->shape, t));
                if (abs_u > w) {
                    y = cg + s->depth * 0.6f * (1.0f - get_tt_smooth((abs_u - w) / (s->bank + 0.01f)));
                }
            }
            float shift = 0.0f;
            if (s->lip > 0.01f && s->ridge_width > 0.01f && abs_u > w && abs_u < w + s->ridge_width) {
                shift = s->lip * sinf(3.14159265f * (abs_u - w) / s->ridge_width);   /* the rim reaches out over the cut */
            }
            if (u > 0.0f) { u -= shift; } else { u += shift; }
            float* o = &out[(rings * cols + k) * 3];
            o[0] = s->dense[ring_index][0] + nx * u;
            o[1] = y;
            o[2] = s->dense[ring_index][1] + nz * u;
        }
    }
    *out_rings = rings;
    *out_cols = cols;
    return true;
}

/* ------------------------------------------------------------------------- */
/* finalize                                                                   */
/* ------------------------------------------------------------------------- */
void get_terrain_tool_bounds(float* min_x, float* min_z, float* max_x, float* max_z)
{
    const terrain_tools_state* s = &g_terrain_tools;
    const float margin = s->width * (1.0f + s->pattern_offset) + s->ridge_width + s->bank + g_terrain.settings.cell_size_m * 2.0f;
    if (is_terrain_tools_radial(s->tool)) {
        *min_x = s->start[0] - margin; *max_x = s->start[0] + margin;
        *min_z = s->start[1] - margin; *max_z = s->start[1] + margin;
        return;
    }
    *min_x = 1e30f; *min_z = 1e30f; *max_x = -1e30f; *max_z = -1e30f;
    for (int i = 0; i < s->dense_count; i++) {
        if (s->dense[i][0] < *min_x) { *min_x = s->dense[i][0]; }
        if (s->dense[i][0] > *max_x) { *max_x = s->dense[i][0]; }
        if (s->dense[i][1] < *min_z) { *min_z = s->dense[i][1]; }
        if (s->dense[i][1] > *max_z) { *max_z = s->dense[i][1]; }
    }
    *min_x -= margin; *max_x += margin; *min_z -= margin; *max_z += margin;
}

/* the heights: every ground column near the piece gets the tool's height, and the bank slope stitches the edge into the terrain */
int carve_terrain_tool(void)
{
    const float cs = g_terrain.settings.cell_size_m;
    float min_x, min_z, max_x, max_z;
    get_terrain_tool_bounds(&min_x, &min_z, &max_x, &max_z);
    int gx0 = (int)floorf(min_x / cs + (float)g_terrain.dims.origin_x);
    int gx1 = (int)ceilf(max_x / cs + (float)g_terrain.dims.origin_x);
    int gz0 = (int)floorf(min_z / cs + (float)g_terrain.dims.origin_z);
    int gz1 = (int)ceilf(max_z / cs + (float)g_terrain.dims.origin_z);
    if (gx0 < 0) { gx0 = 0; }
    if (gz0 < 0) { gz0 = 0; }
    if (gx1 > g_terrain.dims.cells_x) { gx1 = g_terrain.dims.cells_x; }
    if (gz1 > g_terrain.dims.cells_z) { gz1 = g_terrain.dims.cells_z; }

    terrain_refresh_height_cache();
    int changed = 0;
    for (int gz = gz0; gz <= gz1; gz++) {
        for (int gx = gx0; gx <= gx1; gx++) {
            const float x = ((float)(gx - g_terrain.dims.origin_x)) * cs;
            const float z = ((float)(gz - g_terrain.dims.origin_z)) * cs;
            const float ground = terrain_corner_height(gx, gz);
            const float result = get_terrain_tool_height(x, z, ground);
            if (fabsf(result - ground) > 0.001f) {
                terrain_set_corner_height(gx, gz, result);
                changed++;
            }
        }
    }
    if (changed > 0) {
        terrain_mark_dirty_rect(gx0 - 1, gz0 - 1, gx1 + 1, gz1 + 1);
        terrain_touch();
    }
    return changed;
}

/* the piece as its own object: the surface of the grid (both sides) */
bool create_terrain_tool_object(void)
{
    const terrain_tools_state* s = &g_terrain_tools;
    float* grid = (float*)malloc(sizeof(float) * 3u * (size_t)tt_grid_cols * (size_t)tt_grid_max_rings);
    if (!grid) { return false; }
    int rings = 0, cols = 0;
    if (!build_terrain_tool_grid(grid, tt_grid_max_rings, &rings, &cols) || rings < 2) {
        free(grid);
        return false;
    }
    editor_vec3 root = { 0.0f, 0.0f, 0.0f };
    terrain_root_position(&root);

    const uint32_t vertex_count = (uint32_t)(rings * cols);
    const uint32_t index_count = (uint32_t)((rings - 1) * (cols - 1) * 12);
    editor_vertex* vertices = (editor_vertex*)calloc(vertex_count, sizeof(editor_vertex));
    uint32_t* indices = (uint32_t*)malloc(sizeof(uint32_t) * index_count);
    if (!vertices || !indices) {
        free(grid); free(vertices); free(indices);
        return false;
    }
    float color[3] = { 0.45f, 0.38f, 0.30f };
    if (s->tool == tt_tool_lake) { color[0] = 0.30f; color[1] = 0.42f; color[2] = 0.55f; }
    if (s->tool == tt_tool_canyon) { color[0] = 0.62f; color[1] = 0.36f; color[2] = 0.24f; }
    if (s->tool == tt_tool_ramp) { color[0] = 0.55f; color[1] = 0.55f; color[2] = 0.58f; }
    if (s->tool == tt_tool_mountain || s->tool == tt_tool_overhang) { color[0] = 0.50f; color[1] = 0.48f; color[2] = 0.46f; }
    for (int i = 0; i < rings * cols; i++) {
        editor_vertex* v = &vertices[i];
        v->x = grid[i * 3]; v->y = grid[i * 3 + 1] + 0.12f; v->z = grid[i * 3 + 2];
        v->r = color[0]; v->g = color[1]; v->b = color[2]; v->a = 1.0f;
        set_vertex_attributes(v, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    uint32_t used = 0;
    for (int r = 0; r + 1 < rings; r++) {
        for (int c = 0; c + 1 < cols; c++) {
            const uint32_t a = (uint32_t)(r * cols + c), b = a + 1, d = (uint32_t)((r + 1) * cols + c), e = d + 1;
            indices[used++] = a; indices[used++] = d; indices[used++] = b;
            indices[used++] = b; indices[used++] = d; indices[used++] = e;
            indices[used++] = a; indices[used++] = b; indices[used++] = d;
            indices[used++] = b; indices[used++] = e; indices[used++] = d;
        }
    }
    free(grid);

    system_scene_object* obj = spawn_new_scene_object(get_terrain_tools_name(s->tool), root);
    bool ok = false;
    if (obj) {
        obj->type = scene_object_mesh;
        obj->scale = (editor_vec3){ 1.0f, 1.0f, 1.0f };
        set_object_mesh_data(obj->id, vertices, (uint32_t)sizeof(editor_vertex), vertex_count, indices, used);
        em_registry_adopt_triangles(obj->id, vertices, vertex_count, indices, used);
        ok = upload_scene_object_mesh(obj, render_context.logical_device, render_context.physical_device, vertices, vertex_count, indices, used);
    }
    free(vertices);
    free(indices);
    return ok;
}

bool accept_terrain_tool(void)
{
    terrain_tools_state* s = &g_terrain_tools;
    if (!g_terrain.exists) {
        set_statusbar_message("Terrain Tools: there is no terrain. Create one first.");
        return false;
    }
    const bool radial = is_terrain_tools_radial(s->tool);
    if ((radial && s->picks < 1) || (!radial && s->picks < 2)) {
        set_statusbar_message("%s: finish step 1 first (click the terrain).", get_terrain_tools_name(s->tool));
        return false;
    }
    prepare_terrain_tool();

    if (s->tool == tt_tool_tunnel) {
        int added = 0;
        for (int i = 0; i + 1 < s->node_count; i++) {
            terrain_tunnel k;
            memset(&k, 0, sizeof(k));
            k.ax = s->node[i][0];     k.az = s->node[i][1];
            k.bx = s->node[i + 1][0]; k.bz = s->node[i + 1][1];
            k.ay = terrain_sample_height_m(k.ax, k.az) - s->depth;
            k.by = terrain_sample_height_m(k.bx, k.bz) - s->depth;
            k.radius = s->width;
            if (!terrain_add_tunnel(&k)) { break; }
            added++;
        }
        set_statusbar_message("Tunnel Tool: %d tunnel piece(s) added to the terrain.", added);
        reset_terrain_tool_picks();
        return added > 0;
    }

    int changed = 0;
    if (s->tool != tt_tool_overhang) {
        changed = carve_terrain_tool();
    }
    bool made_object = false;
    if (s->separate || s->tool == tt_tool_overhang || s->lip > 0.01f) {
        made_object = create_terrain_tool_object();      /* a height map cannot hang over the cut, so the lip is always built as a mesh set into the cut */
    }
    if (made_object) {
        set_statusbar_message("%s: %d ground columns changed and the piece is its own object.", get_terrain_tools_name(s->tool), changed);
    } else {
        set_statusbar_message("%s: %d ground columns changed and stitched into the terrain.", get_terrain_tools_name(s->tool), changed);
    }
    reset_terrain_tool_picks();
    return true;
}

/* ------------------------------------------------------------------------- */
/* Scene view: input and the wireframe preview                                 */
/* ------------------------------------------------------------------------- */
void draw_terrain_tools_line(VkCommandBuffer cmd, const struct_viewport_bounds* b, float x0, float y0, float x1, float y1, system_color color, float size)
{
    const float dx = x1 - x0, dy = y1 - y0;
    const float length = sqrtf(dx * dx + dy * dy);
    int steps = (int)(length / (size * 0.8f)) + 1;
    if (steps > 400) { steps = 400; }
    for (int i = 0; i <= steps; i++) {
        const float t = (float)i / (float)steps;
        const float x = x0 + dx * t - size * 0.5f;
        const float y = y0 + dy * t - size * 0.5f;
        if (x < (float)b->offset_x || y < (float)b->offset_y || x + size > (float)(b->offset_x + b->width) || y + size > (float)(b->offset_y + b->height)) {
            continue;
        }
        push_quad_element(cmd, x, y, size, size, color);
    }
}

editor_mat4 g_tt_view_proj;      /* projection * view of the Scene view this frame (set in update_and_draw_terrain_tools) */

/* terrain local meters -> Scene view pixels. same maths as the transform gizmo: clip = view_proj * world, Vulkan y is already down */
bool get_terrain_tools_screen(const editor_mat4* view, const editor_mat4* proj, const struct_viewport_bounds* b, editor_vec3 root, float lx, float ly, float lz, float* sx, float* sy)
{
    (void)view;
    (void)proj;
    const float x = root.x + lx, y = root.y + ly, z = root.z + lz;
    float o[4];
    for (int r = 0; r < 4; r++) {
        o[r] = g_tt_view_proj.mesh[0][r] * x + g_tt_view_proj.mesh[1][r] * y + g_tt_view_proj.mesh[2][r] * z + g_tt_view_proj.mesh[3][r];
    }
    if (o[3] <= 0.01f) { return false; }
    *sx = (float)b->offset_x + (o[0] / o[3] * 0.5f + 0.5f) * (float)b->width;
    *sy = (float)b->offset_y + (o[1] / o[3] * 0.5f + 0.5f) * (float)b->height;
    return true;
}

/* a line over the ground from (x0, z0) to (x1, z1) in terrain local meters, drawn as a chain of short pieces that follow the heights */
void draw_terrain_tools_ground_line(VkCommandBuffer cmd, const struct_viewport_bounds* b, editor_vec3 root, float x0, float z0, float x1, float z1,
                                    float sample_m, system_color color, float size)
{
    const float dx = x1 - x0, dz = z1 - z0;
    const float length = sqrtf(dx * dx + dz * dz);
    int pieces = (int)(length / sample_m) + 1;
    if (pieces > 400) { pieces = 400; }
    float px = 0.0f, py = 0.0f;
    bool have_previous = false;
    for (int i = 0; i <= pieces; i++) {
        const float t = (float)i / (float)pieces;
        const float x = x0 + dx * t, z = z0 + dz * t;
        float sx, sy;
        if (get_terrain_tools_screen(NULL, NULL, b, root, x, terrain_sample_height_m(x, z) + 0.6f, z, &sx, &sy)) {
            if (have_previous) { draw_terrain_tools_line(cmd, b, px, py, sx, sy, color, size); }
            px = sx; py = sy;
            have_previous = true;
        } else {
            have_previous = false;
        }
    }
}

/* the 3 x 3 regions with a blue outline (the one picked in the Scene Archive brighter) and the chunks around the camera in light yellow */
void draw_terrain_tools_outlines(VkCommandBuffer cmd, const struct_viewport_bounds* b, editor_vec3 root)
{
    const float cs = g_terrain.settings.cell_size_m;
    const int region_cells = g_terrain.dims.region_cells;
    const int chunk_cells = g_terrain.dims.chunk_cells;
    const float ox = (float)g_terrain.dims.origin_x, oz = (float)g_terrain.dims.origin_z;
    const float end_x = ((float)g_terrain.dims.cells_x - ox) * cs, end_z = ((float)g_terrain.dims.cells_z - oz) * cs;
    const float begin_x = -ox * cs, begin_z = -oz * cs;

    int picked_region = -1;
    for (int r = 0; r < terrain_region_count; r++) {
        if (g_terrain.region_ids[r] == g_scene_registry.selected_object_id) { picked_region = r; }
    }

    /* chunks: a window of 11 x 11 around the chunk under the camera, limited to the picked region when there is one */
    int cx0 = g_terrain.center_cx - 5, cx1 = g_terrain.center_cx + 6;
    int cz0 = g_terrain.center_cz - 5, cz1 = g_terrain.center_cz + 6;
    if (picked_region >= 0) {
        const int rc = g_terrain.dims.region_chunks;
        const int rx = picked_region % terrain_region_grid, rz = picked_region / terrain_region_grid;
        if (cx0 < rx * rc) { cx0 = rx * rc; }
        if (cx1 > (rx + 1) * rc) { cx1 = (rx + 1) * rc; }
        if (cz0 < rz * rc) { cz0 = rz * rc; }
        if (cz1 > (rz + 1) * rc) { cz1 = (rz + 1) * rc; }
    }
    if (cx0 < 0) { cx0 = 0; }
    if (cz0 < 0) { cz0 = 0; }
    if (cx1 > g_terrain.dims.chunks_x) { cx1 = g_terrain.dims.chunks_x; }
    if (cz1 > g_terrain.dims.chunks_z) { cz1 = g_terrain.dims.chunks_z; }
    const system_color chunk_color = { 1.0f, 0.95f, 0.60f, 1.0f };
    const float x_from = ((float)(cx0 * chunk_cells) - ox) * cs, x_to = ((float)(cx1 * chunk_cells) - ox) * cs;
    const float z_from = ((float)(cz0 * chunk_cells) - oz) * cs, z_to = ((float)(cz1 * chunk_cells) - oz) * cs;
    for (int cx = cx0; cx <= cx1; cx++) {
        const float x = ((float)(cx * chunk_cells) - ox) * cs;
        draw_terrain_tools_ground_line(cmd, b, root, x, z_from, x, z_to, (float)chunk_cells * cs * 0.1f, chunk_color, 2.0f);
    }
    for (int cz = cz0; cz <= cz1; cz++) {
        const float z = ((float)(cz * chunk_cells) - oz) * cs;
        draw_terrain_tools_ground_line(cmd, b, root, x_from, z, x_to, z, (float)chunk_cells * cs * 0.1f, chunk_color, 2.0f);
    }

    /* regions: four lines each way, blue */
    const system_color region_color = { 0.20f, 0.50f, 1.0f, 1.0f };
    const system_color picked_color = { 0.45f, 0.80f, 1.0f, 1.0f };
    for (int k = 0; k <= terrain_region_grid; k++) {
        const float x = ((float)(k * region_cells) - ox) * cs;
        const float z = ((float)(k * region_cells) - oz) * cs;
        draw_terrain_tools_ground_line(cmd, b, root, x, begin_z, x, end_z, (float)chunk_cells * cs * 0.25f, region_color, 4.0f);
        draw_terrain_tools_ground_line(cmd, b, root, begin_x, z, end_x, z, (float)chunk_cells * cs * 0.25f, region_color, 4.0f);
    }
    if (picked_region >= 0) {
        const int rx = picked_region % terrain_region_grid, rz = picked_region / terrain_region_grid;
        const float x_a = ((float)(rx * region_cells) - ox) * cs, x_b = ((float)((rx + 1) * region_cells) - ox) * cs;
        const float z_a = ((float)(rz * region_cells) - oz) * cs, z_b = ((float)((rz + 1) * region_cells) - oz) * cs;
        const float step = (float)chunk_cells * cs * 0.25f;
        draw_terrain_tools_ground_line(cmd, b, root, x_a, z_a, x_b, z_a, step, picked_color, 5.0f);
        draw_terrain_tools_ground_line(cmd, b, root, x_a, z_b, x_b, z_b, step, picked_color, 5.0f);
        draw_terrain_tools_ground_line(cmd, b, root, x_a, z_a, x_a, z_b, step, picked_color, 5.0f);
        draw_terrain_tools_ground_line(cmd, b, root, x_b, z_a, x_b, z_b, step, picked_color, 5.0f);
    }
}
void get_tt_edge_midpoint(const edit_mode_mesh* mesh, uint32_t edge, float* out_xz)
{
    const editor_vec3 a = mesh->verts[mesh->edges[edge].vert_to_edge_array[0]].obj_vert_pos;
    const editor_vec3 c = mesh->verts[mesh->edges[edge].vert_to_edge_array[1]].obj_vert_pos;
    out_xz[0] = (a.x + c.x) * 0.5f;
    out_xz[1] = (a.z + c.z) * 0.5f;
}

void select_all_terrain_tool_nodes(void)
{
    terrain_tools_state* s = &g_terrain_tools;
    memset(s->node_selected, 0, sizeof(s->node_selected));
    for (int i = 0; i < s->node_count; i++) { s->node_selected[i] = true; }
}

bool is_terrain_tools_blocking_edit_picks(void)
{
    return g_terrain_tools.tool != tt_tool_none && g_terrain_tools.step >= 2;
}

void enter_terrain_tools_edit_mode(system_element_context* ui_element)
{
    if (!g_terrain.exists) {
        set_statusbar_message("Terrain Tools: there is no terrain. Create one first.");
        return;
    }
    const system_scene_object* picked = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (!picked || !is_terrain_object(picked)) {
        g_scene_registry.selected_object_id = g_terrain.scene_object_id;       /* a region picked in the Scene Archive stays the one to work in */
    }
    mode_menu_enter_mode(ui_element, mode_context_edit, true);
    ui_element->active_select_type = select_edge;
    ui_element->active_tool = tool_translate;
}

bool get_terrain_tools_gizmo_pivot(editor_vec3* out_world)
{
    const terrain_tools_state* s = &g_terrain_tools;
    if (s->tool == tt_tool_none || s->step < 2 || s->node_count < 1 || !g_terrain.exists) { return false; }
    float cx = 0.0f, cz = 0.0f;
    int count = 0;
    for (int i = 0; i < s->node_count; i++) {
        if (!s->node_selected[i]) { continue; }
        cx += s->node[i][0]; cz += s->node[i][1]; count++;
    }
    if (count == 0) { return false; }
    cx /= (float)count; cz /= (float)count;
    editor_vec3 root = { 0.0f, 0.0f, 0.0f };
    terrain_root_position(&root);
    out_world->x = root.x + cx;
    out_world->y = root.y + terrain_sample_height_m(cx, cz) + 0.3f;
    out_world->z = root.z + cz;
    return true;
}

void begin_terrain_tools_gizmo_drag(void)
{
    terrain_tools_state* s = &g_terrain_tools;
    for (int i = 0; i < s->node_count; i++) {
        s->node_snap[i][0] = s->node[i][0];
        s->node_snap[i][1] = s->node[i][1];
    }
}

/* the gizmo moved the selected vertices `distance` meters along axis 0 (x) or 2 (z) from where the drag began */
void move_terrain_tools_gizmo_nodes(int axis, float distance)
{
    terrain_tools_state* s = &g_terrain_tools;
    for (int i = 0; i < s->node_count; i++) {
        if (!s->node_selected[i]) { continue; }
        s->node[i][0] = s->node_snap[i][0];
        s->node[i][1] = s->node_snap[i][1];
        if (axis == 0) { s->node[i][0] += distance; }
        if (axis == 2) { s->node[i][1] += distance; }
    }
    s->start[0] = s->node[0][0];
    s->start[1] = s->node[0][1];
    if (!is_terrain_tools_radial(s->tool)) {
        s->end[0] = s->node[s->node_count - 1][0];
        s->end[1] = s->node[s->node_count - 1][1];
    }
    s->nodes_edited = true;
}
void update_and_draw_terrain_tools(VkCommandBuffer cmd, system_element_context* ui)
{
    terrain_tools_state* s = &g_terrain_tools;
    const bool down = ui->input.mouse_down;
    const bool press = down && !s->prev_down;
    s->prev_down = down;
    s->wants_mouse = (s->tool != tt_tool_none);
    if (s->tool == tt_tool_none || !g_terrain.exists) {
        return;
    }
    const struct_viewport_bounds b = ui->scene_view;
    if (b.width <= 0 || b.height <= 0) { return; }

    editor_mat4 view, proj;
    build_scene_orbit_camera((float)b.width / (float)b.height, &view, &proj);
    g_tt_view_proj = system_math_mat4_multiply(proj, view);
    editor_vec3 root = { 0.0f, 0.0f, 0.0f };
    terrain_root_position(&root);
    draw_terrain_tools_outlines(cmd, &b, root);

    const float mx = (float)ui->input.mouse_x, my = (float)ui->input.mouse_y;
    const bool in_view = mx >= (float)b.offset_x && my >= (float)b.offset_y && mx < (float)(b.offset_x + b.width) && my < (float)(b.offset_y + b.height);
    const bool free_mouse = in_view && !ui->input.alt_pressed && !ui->input.middle_down && !ui->input.camera_manipulating && !is_context_menu_open() &&
                            is_edit_in_scene_view(ui);
    const bool radial = is_terrain_tools_radial(s->tool);

    const int picks_before = s->picks;
    /* ---- step 1: Edit mode, edge select. the first edge you pick is the start, the next one is the end (a mountain or lake: one edge, its middle) ---- */
    if (s->step == 1) {
        edit_mode_mesh* mesh = terrain_edit_mesh_current();
        if (mesh) {
            if (s->picks == 0) {
                for (uint32_t e = 0; e < mesh->edge_count; e++) {
                    if (!mesh->edges[e].obj_vert_selected) { continue; }
                    get_tt_edge_midpoint(mesh, e, s->start);
                    s->start_edge = (int)e;
                    s->picks = 1;
                    if (radial) {
                        build_terrain_tool_nodes();
                        s->step = 2;
                        em_clear_selection(mesh);
                        set_statusbar_message("%s: the middle is set. Move it with the translate gizmo.", get_terrain_tools_name(s->tool));
                    } else {
                        em_clear_selection(mesh);      /* the grid is free to follow the camera to another chunk for the end */
                        set_statusbar_message("%s: start edge picked. Pan to the END and click its edge (any chunk).", get_terrain_tools_name(s->tool));
                    }
                    break;
                }
            } else if (!radial && s->picks == 1) {
                for (uint32_t e = 0; e < mesh->edge_count; e++) {
                    if (!mesh->edges[e].obj_vert_selected) { continue; }
                    float candidate[2];
                    get_tt_edge_midpoint(mesh, e, candidate);
                    if (fabsf(candidate[0] - s->start[0]) < 0.5f && fabsf(candidate[1] - s->start[1]) < 0.5f) { continue; }
                    s->end[0] = candidate[0];
                    s->end[1] = candidate[1];
                    s->picks = 2;
                    build_terrain_tool_nodes();
                    s->step = 2;
                    em_clear_selection(mesh);
                    set_statusbar_message("%s: click a square to select it, move with the translate gizmo.", get_terrain_tools_name(s->tool));
                    break;
                }
            }
        }
    }

    /* ---- step 1 anywhere on the terrain, any chunk: a click snaps to the nearest cell edge (the edit grid only shows one chunk) ---- */
    if (s->step == 1 && press && free_mouse && s->picks == picks_before) {
        editor_vec3 click_hit = { 0.0f, 0.0f, 0.0f };
        float click_depth = 0.0f;
        if (get_terrain_ray_hit(ui, mx, my, &click_hit, &click_depth)) {
            const float cs = g_terrain.settings.cell_size_m;
            const float fx = (click_hit.x - root.x) / cs;
            const float fz = (click_hit.z - root.z) / cs;
            float snapped[2];
            if (fabsf(fx - roundf(fx)) < fabsf(fz - roundf(fz))) {
                snapped[0] = roundf(fx) * cs;
                snapped[1] = (floorf(fz) + 0.5f) * cs;
            } else {
                snapped[0] = (floorf(fx) + 0.5f) * cs;
                snapped[1] = roundf(fz) * cs;
            }
            edit_mode_mesh* grid_mesh = terrain_edit_mesh_current();
            if (grid_mesh) {
                em_clear_selection(grid_mesh);                /* the same click also selected an edge of the grid: do not count it twice */
            }
            if (s->picks == 0) {
                s->start[0] = snapped[0]; s->start[1] = snapped[1];
                s->picks = 1;
                if (radial) {
                    build_terrain_tool_nodes();
                    s->step = 2;
                    set_statusbar_message("%s: the middle is set. Move it with the translate gizmo.", get_terrain_tools_name(s->tool));
                } else {
                    set_statusbar_message("%s: start set. Click the END anywhere on the terrain.", get_terrain_tools_name(s->tool));
                }
            } else if (!radial && s->picks == 1) {
                s->end[0] = snapped[0]; s->end[1] = snapped[1];
                s->picks = 2;
                build_terrain_tool_nodes();
                s->step = 2;
                set_statusbar_message("%s: click a square to select it, move with the translate gizmo.", get_terrain_tools_name(s->tool));
            }
        }
    }
    prepare_terrain_tool();

    /* ---- steps 2..5: click a square to select it (Shift adds); the translate gizmo (system_transform_gizmo.c) moves the selected ones ---- */
    if (s->step >= 2 && s->node_count > 0 && press && free_mouse && !system_transform_gizmo_wants_mouse()) {
        float best = 16.0f;
        int picked = -1;
        for (int i = 0; i < s->node_count; i++) {
            float sx, sy;
            const float ground = terrain_sample_height_m(s->node[i][0], s->node[i][1]);
            if (!get_terrain_tools_screen(&view, &proj, &b, root, s->node[i][0], ground, s->node[i][1], &sx, &sy)) { continue; }
            const float d = sqrtf((sx - mx) * (sx - mx) + (sy - my) * (sy - my));
            if (d < best) { best = d; picked = i; }
        }
        const bool additive = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
        if (picked >= 0) {
            if (!additive) {
                memset(s->node_selected, 0, sizeof(s->node_selected));
            }
            s->node_selected[picked] = true;
        } else if (!additive) {
            memset(s->node_selected, 0, sizeof(s->node_selected));
        }
    }
    /* ---- live preview in step 1: before the end is picked the piece follows the mouse over the terrain ---- */
    const int saved_picks = s->picks;
    const int saved_nodes = s->node_count;
    bool provisional = false;
    if (s->step == 1 && free_mouse) {
        editor_vec3 hover_hit = { 0.0f, 0.0f, 0.0f };
        float hover_depth = 0.0f;
        if (get_terrain_ray_hit(ui, mx, my, &hover_hit, &hover_depth)) {
            float cursor_x, cursor_y;
            if (get_terrain_tools_screen(&view, &proj, &b, root, hover_hit.x - root.x, hover_hit.y - root.y + 0.5f, hover_hit.z - root.z, &cursor_x, &cursor_y)) {
                const system_color cyan = { 0.2f, 0.95f, 1.0f, 1.0f };
                push_quad_element(cmd, cursor_x - 9.0f, cursor_y - 1.0f, 18.0f, 2.0f, cyan);
                push_quad_element(cmd, cursor_x - 1.0f, cursor_y - 9.0f, 2.0f, 18.0f, cyan);
            }            const float hover_x = hover_hit.x - root.x;
            const float hover_z = hover_hit.z - root.z;
            if (radial && s->picks == 0) {
                s->start[0] = hover_x; s->start[1] = hover_z;
                s->picks = 1;
                build_terrain_tool_nodes();
                provisional = true;
            } else if (!radial && s->picks == 1) {
                s->end[0] = hover_x; s->end[1] = hover_z;
                s->picks = 2;
                build_terrain_tool_nodes();
                provisional = true;
            }
            if (provisional) {
                prepare_terrain_tool();
            }
        }
    }
    /* ---- the preview ---- */
    const system_color grid_color = { 0.15f, 0.80f, 0.95f, 1.0f };
    const system_color cross_color = { 0.40f, 0.95f, 1.00f, 1.0f };
    const system_color path_color = { 0.85f, 1.0f, 1.0f, 1.0f };

    if (s->picks >= 1 && s->tool != tt_tool_tunnel) {
        float grid[tt_grid_max_rings * tt_grid_cols * 3];
        int rings = 0, cols = 0;
        if (build_terrain_tool_grid(grid, tt_grid_max_rings, &rings, &cols)) {
            for (int r = 0; r < rings; r++) {
                for (int c = 0; c < cols; c++) {
                    float x0, y0;
                    const float* p = &grid[(r * cols + c) * 3];
                    if (!get_terrain_tools_screen(&view, &proj, &b, root, p[0], p[1], p[2], &x0, &y0)) { continue; }
                    if (c + 1 < cols) {
                        float x1, y1;
                        const float* q = &grid[(r * cols + c + 1) * 3];
                        if (get_terrain_tools_screen(&view, &proj, &b, root, q[0], q[1], q[2], &x1, &y1)) {
                            draw_terrain_tools_line(cmd, &b, x0, y0, x1, y1, cross_color, 2.0f);
                        }
                    }
                    if (r + 1 < rings) {
                        float x1, y1;
                        const float* q = &grid[((r + 1) * cols + c) * 3];
                        if (get_terrain_tools_screen(&view, &proj, &b, root, q[0], q[1], q[2], &x1, &y1)) {
                            draw_terrain_tools_line(cmd, &b, x0, y0, x1, y1, grid_color, 2.0f);
                        }
                    }
                }
            }
        }
    }
    if (s->tool == tt_tool_tunnel && s->node_count >= 2) {
        for (int i = 0; i < s->node_count; i++) {
            const int segments = 12;
            const float cx = s->node[i][0], cz = s->node[i][1];
            const float cy = terrain_sample_height_m(cx, cz) - s->depth;
            int i0 = i - 1, i1 = i + 1;
            if (i0 < 0) { i0 = 0; }
            if (i1 >= s->node_count) { i1 = s->node_count - 1; }
            float tx = s->node[i1][0] - s->node[i0][0], tz = s->node[i1][1] - s->node[i0][1];
            const float tl = sqrtf(tx * tx + tz * tz);
            if (tl > 1e-6f) { tx /= tl; tz /= tl; }
            float px = 0.0f, py = 0.0f;
            for (int k = 0; k <= segments; k++) {
                const float a = 6.28318530718f * (float)k / (float)segments;
                const float side = cosf(a) * s->width;
                const float up = sinf(a) * s->width;
                float x, y;
                if (get_terrain_tools_screen(&view, &proj, &b, root, cx - tz * side, cy + up, cz + tx * side, &x, &y)) {
                    if (k > 0 && px > 0.0f) { draw_terrain_tools_line(cmd, &b, px, py, x, y, cross_color, 2.0f); }
                    px = x; py = y;
                } else {
                    px = 0.0f;
                }
            }
        }
    }
    if (s->dense_count >= 2) {
        for (int i = 0; i + 1 < s->dense_count; i++) {
            float x0, y0, x1, y1;
            const float g0 = terrain_sample_height_m(s->dense[i][0], s->dense[i][1]);
            const float g1 = terrain_sample_height_m(s->dense[i + 1][0], s->dense[i + 1][1]);
            if (get_terrain_tools_screen(&view, &proj, &b, root, s->dense[i][0], g0 + 0.3f, s->dense[i][1], &x0, &y0) &&
                get_terrain_tools_screen(&view, &proj, &b, root, s->dense[i + 1][0], g1 + 0.3f, s->dense[i + 1][1], &x1, &y1)) {
                draw_terrain_tools_line(cmd, &b, x0, y0, x1, y1, path_color, 3.0f);
            }
        }
    }

    if (s->dense_count >= 2 && !radial) {
        const system_color bank_color = { 0.55f, 1.0f, 1.0f, 1.0f };
        float left_px = 0.0f, left_py = 0.0f, right_px = 0.0f, right_py = 0.0f;
        bool have_left = false, have_right = false;
        for (int i = 0; i < s->dense_count; i++) {
            int i0 = i - 1, i1 = i + 1;
            if (i0 < 0) { i0 = 0; }
            if (i1 > s->dense_count - 1) { i1 = s->dense_count - 1; }
            float tx = s->dense[i1][0] - s->dense[i0][0], tz = s->dense[i1][1] - s->dense[i0][1];
            const float tl = sqrtf(tx * tx + tz * tz);
            if (tl > 1e-6f) { tx /= tl; tz /= tl; }
            const float w = s->dense[i][3];
            const float lx = s->dense[i][0] - tz * w, lz = s->dense[i][1] + tx * w;
            const float rx = s->dense[i][0] + tz * w, rz = s->dense[i][1] - tx * w;
            float lsx, lsy, rsx, rsy;
            const bool left_ok = get_terrain_tools_screen(&view, &proj, &b, root, lx, terrain_sample_height_m(lx, lz) + 0.5f, lz, &lsx, &lsy);
            const bool right_ok = get_terrain_tools_screen(&view, &proj, &b, root, rx, terrain_sample_height_m(rx, rz) + 0.5f, rz, &rsx, &rsy);
            if (left_ok && have_left) { draw_terrain_tools_line(cmd, &b, left_px, left_py, lsx, lsy, bank_color, 3.0f); }
            if (right_ok && have_right) { draw_terrain_tools_line(cmd, &b, right_px, right_py, rsx, rsy, bank_color, 3.0f); }
            if (left_ok && right_ok && (i == 0 || i == s->dense_count - 1)) {
                draw_terrain_tools_line(cmd, &b, lsx, lsy, rsx, rsy, bank_color, 3.0f);     /* the start edge and the end edge */
            }
            have_left = left_ok; left_px = lsx; left_py = lsy;
            have_right = right_ok; right_px = rsx; right_py = rsy;
        }
    }
    /* the picks and the vertex handles */
    const system_color white = { 1.0f, 1.0f, 1.0f, 1.0f };
    const system_color start_color = { 0.2f, 1.0f, 0.3f, 1.0f };
    const system_color end_color = { 1.0f, 0.3f, 0.3f, 1.0f };
    const system_color active_color = { 1.0f, 0.9f, 0.1f, 1.0f };
    if (s->node_count > 0 && s->step >= 2) {
        for (int i = 0; i < s->node_count; i++) {
            float x, y;
            const float ground = terrain_sample_height_m(s->node[i][0], s->node[i][1]);
            if (!get_terrain_tools_screen(&view, &proj, &b, root, s->node[i][0], ground + 0.3f, s->node[i][1], &x, &y)) { continue; }
            system_color color = white;
            float size = 9.0f;
            if (i == 0) { color = start_color; size = 13.0f; }
            if (i == s->node_count - 1 && !radial) { color = end_color; size = 13.0f; }
            if (s->node_selected[i]) { color = active_color; }
            push_quad_element(cmd, x - size * 0.5f - 1.0f, y - size * 0.5f - 1.0f, size + 2.0f, size + 2.0f, (system_color){ 0.0f, 0.0f, 0.0f, 1.0f });
            push_quad_element(cmd, x - size * 0.5f, y - size * 0.5f, size, size, color);
        }
    } else if (s->picks >= 1) {
        float x, y;
        const float ground = terrain_sample_height_m(s->start[0], s->start[1]);
        if (get_terrain_tools_screen(&view, &proj, &b, root, s->start[0], ground + 0.3f, s->start[1], &x, &y)) {
            push_quad_element(cmd, x - 7.0f, y - 7.0f, 14.0f, 14.0f, start_color);
        }
    }

    if (provisional) {
        s->picks = saved_picks;
        s->node_count = saved_nodes;
        s->dense_count = 0;
    }
}
