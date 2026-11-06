#include "system_globals.h"

object_state g_object;
static bool s_object_started = false;

void start_object_mode(void)
{
    if (s_object_started) return;
    s_object_started = true;
    g_object.active = true;                   /* default mode */
}

void object_mode_set_active(bool on)
{
    start_object_mode();
    g_object.active = on;
}

bool object_mode_is_active(void)
{
    start_object_mode();
    return g_object.active && mode_state_is_object();
}

static float snap_to(float vert_to_edge_array, float step)
{
    if (!g_mode_state.object.snap_enabled || step <= 0.0f) return vert_to_edge_array;
    return roundf(vert_to_edge_array / step) * step;
}

float object_snap_translate(float vert_to_edge_array) { return snap_to(vert_to_edge_array, g_mode_state.object.snap_translate_m); }
float object_snap_rotate(float deg)  { return snap_to(deg, g_mode_state.object.snap_rotate_deg); }
float object_snap_scale(float vert_to_edge_array)     { return snap_to(vert_to_edge_array, g_mode_state.object.snap_scale); }