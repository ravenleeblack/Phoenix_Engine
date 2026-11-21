#include "system_globals.h"


render_backend* backend = 0;

// Define the real storage memory locations for the linker to point to
VkPipelineLayout pipeline_layout_ui     = VK_NULL_HANDLE;
VkPipeline       graphics_pipeline_ui   = VK_NULL_HANDLE;

VkPipelineLayout pipeline_layout_grid   = VK_NULL_HANDLE;
VkPipeline       graphics_pipeline_grid = VK_NULL_HANDLE;

HMODULE system_engine_dll = NULL;
HMODULE system_game_dll = NULL;
HWND hwnd_window_00 = NULL;

char status_bar_text_buffer[256] = "System Idle";
char menu_bar_text_buffer[512] = "";

bool is_game_running = false;
bool scene_view_has_focus = false;       /* the last click landed in the scene view: it keeps the keyboard while the cursor is elsewhere */

/* the editor module's own light list (see system_directional_light.c) */
light_object editor_lights[light_max_objects] = {0};
int          editor_light_count               = 0;
int          editor_light_driver              = -1;
glyph        editor_sun_glyph                 = {0};
bool is_engine_running = true;
bool editor_frame_key_down = false;
bool editor_home_key_down = false;

system_clock main_clock;
system_program program_state = {0};
system_element_context element_context = {0};
system_render_context render_context = {0};
system_engine_context engine_context = {0};
manage_docking dock_workspace = {0};
tooltip_state g_tooltip = {0};

sky_state g_sky = {
    .scene_sky_on = true, .scene_fog_on = false, .game_sky_on = true, .game_fog_on = false,
    .sky_top = { 0.25f, 0.45f, 0.85f }, .sky_horizon = { 0.75f, 0.85f, 0.95f },
    .fog_color = { 0.72f, 0.80f, 0.90f },
    .fog_density = 0.008f, .fog_height_falloff = 0.015f, .fog_base_height = 0.0f, .fog_start = 0.0f,
    .fog_max_opacity = 1.0f, .fog_sun_scatter = 0.6f, .fog_sun_exponent = 8.0f,
};

scene_view_state g_scene_view = {
    .show_grid = true, .grid_half_cells = 50, .show_origin = true, .show_cameras = true,
    .shading = scene_shading_textured,
    .wire_color           = { 0.78f, 0.82f, 0.90f },
    .wire_on_shaded_color = { 0.04f, 0.04f, 0.05f },
};

/* the color palette now lives in phoenix_system_theme.c */

int                  g_mouse_prev_x = 0;
int                  g_active_id_idle_frames = 0;   /* frames with no mouse button down (system_editor_mouse.c clears a stale active_id after 3) */
int                  g_mouse_prev_y = 0;
 
WNDPROC              g_wheel_old_proc = 0;
HWND                 g_wheel_hooked   = 0;
bool                 g_wheel_unicode  = false;
volatile LONG        g_wheel_accum    = 0;
int                  g_wheel_logged   = 0;
 
editor_vec3          g_paint_last         = { 1e9f, 0.0f, 0.0f };
bool                 g_paint_was_painting = false;
bool                 g_paint_prev_down    = false;
unsigned             g_paint_last_frame   = 0xFFFFFFFFu;
 
manage_sculpt_stroke g_sculpt_stroke      = {0};
bool                 g_sculpt_prev_left   = false;
int                  g_sculpt_prev_mx     = 0;
unsigned             g_sculpt_last_frame  = 0xFFFFFFFFu;
#include "system_editor_input.h"