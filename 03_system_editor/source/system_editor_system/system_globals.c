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
bool is_engine_running = true;
bool editor_frame_key_down = false;
bool editor_home_key_down = false;

system_clock main_clock;
system_program program_state = {0};
system_element_context element_context = {0};
system_render_context render_context = {0};
system_engine_context engine_context = {0};
manage_docking dock_workspace = {0};
scene_view_state g_scene_view = {
    .show_grid = true, .show_origin = true, .show_cameras = true,
    .shading = scene_shading_textured,
    .wire_color           = { 0.78f, 0.82f, 0.90f },
    .wire_on_shaded_color = { 0.04f, 0.04f, 0.05f },
};

/* the color palette now lives in phoenix_system_theme.c */

int                  g_mouse_prev_x = 0;
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