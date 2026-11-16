#include "engine_globals.h"

/* everything the engine exe owns. the editor, loader and game dlls reach these through the import library. */

log_level   current_log_level = log_level_trace;
const char* ansi_color_strings[6] = {
    "\033[1;41m",  /* fatal */
    "\033[1;31m",  /* error */
    "\033[1;33m",  /* warn */
    "\033[1;32m",  /* info */
    "\033[1;34m",  /* debug */
    "\033[1;37m"   /* trace */
};
const char* level_strings[6] = {
    "[fatal]", "[error]", "[warn] ", "[info] ", "[debug]", "[trace]"
};

bool               program_iniated = false;
bool               program_started = false;
program_boot_stage program_stage   = program_boot_stage_none;

game_state start_game_state_instance = {0};

unsigned char           g_key_states[256]    = {0};
bool                    g_key_prev_down[256] = {0};
enum_input_profile_type g_input_profile      = input_profile_maya;
mouse_state             g_mouse_current      = {0};
mouse_state             g_mouse_previous     = {0};
float                   g_mouse_wheel_notches = 0.0f;

render_backend* backend = 0;

VkPipelineLayout pipeline_layout_ui     = VK_NULL_HANDLE;
VkPipeline       graphics_pipeline_ui   = VK_NULL_HANDLE;

VkPipelineLayout pipeline_layout_grid   = VK_NULL_HANDLE;
VkPipeline       graphics_pipeline_grid = VK_NULL_HANDLE;

/* ---- vulkan backend state (were file statics in engine_vulkan_backend.c) ---- */
unsigned int             g_cached_framebuffer_width         = 0;
unsigned int             g_cached_framebuffer_height        = 0;
unsigned long long       g_framebuffer_size_generation      = 0;
unsigned long long       g_framebuffer_size_last_generation = 0;
bool                     g_recreating_swapchain             = false;
uint32_t                 g_current_inflight_cpu_frame       = 0;
VkDebugUtilsMessengerEXT g_debug_messenger                  = VK_NULL_HANDLE;
vulkan_system_context    g_vulkan_runtime_context           = {0};

/* ---- fallback ui texture (were file statics in engine_vulkan_backend.c) ---- */
VkImage          g_ui_fallback_image   = VK_NULL_HANDLE;
VkDeviceMemory   g_ui_fallback_memory  = VK_NULL_HANDLE;
VkImageView      g_ui_fallback_view    = VK_NULL_HANDLE;
VkSampler        g_ui_fallback_sampler = VK_NULL_HANDLE;
VkDescriptorPool g_ui_fallback_pool    = VK_NULL_HANDLE;
VkDescriptorSet  g_ui_fallback_set     = VK_NULL_HANDLE;

/* ---- swapchain targets (were file statics in engine_vulkan_swapchain.c) ---- */
vulkan_image   g_swapchain_depth_image = {0};
VkImage        g_msaa_image            = VK_NULL_HANDLE;
VkDeviceMemory g_msaa_memory           = VK_NULL_HANDLE;
VkImageView    g_msaa_view             = VK_NULL_HANDLE;

/* ---- shader search folders (was a file static in engine_vulkan_pipeline.c) ---- */
const char* const g_shader_dirs[shader_directory_count] = { "", "shaders/", "../shaders/", "../../shaders/", "../" };

HMODULE     system_editor_dll         = NULL;
HMODULE     system_loader_dll         = NULL;
HMODULE     system_game_dll           = NULL;
loader_plugin g_loader                = {0};
HWND        hwnd_window_00            = NULL;
const char* program_window_class_name = "phoenix_engine_window_class";
const char* program_window_title      = "phoenix engine";

system_clock          main_clock     = {0};
system_program        program_state  = {0};
system_render_context render_context = {0};

__declspec(dllexport) system_render_context* get_engine_render_context(void)
{
    return &render_context;
}

__declspec(dllexport) system_program* get_engine_program_state(void)
{
    return &program_state;
}

__declspec(dllexport) VkPipeline get_engine_grid_pipeline(void)
{
    return graphics_pipeline_grid;
}

__declspec(dllexport) VkPipelineLayout get_engine_grid_pipeline_layout(void)
{
    return pipeline_layout_grid;
}
system_engine_context engine_context = {0};
system_element_context element_context = {0};

editor_plugin          g_editor           = {0};
create_game_ptr        create_game_handle = NULL;
engine_render_settings g_render_settings  = { true, 1, false };
light_object g_lights[light_max_objects] = {0};
int g_light_count = 0;
int g_light_driver = -1;
glyph g_sun_glyph = {0};
bool g_scene_cursor_in_view = false;
bool g_swapchain_is_srgb = false;

VkSampleCountFlagBits get_render_msaa_sample_count(void)
{
    switch (g_render_settings.msaa_samples) {
        case 2:  return VK_SAMPLE_COUNT_2_BIT;
        case 4:  return VK_SAMPLE_COUNT_4_BIT;
        case 8:  return VK_SAMPLE_COUNT_8_BIT;
        default: return VK_SAMPLE_COUNT_1_BIT;
    }
}

light_settings   g_light      = { 0.20f, true, terrain_layer_atmosphere, 1.5f, 0.0f };
system_terrain   g_terrain    = {0};
terrain_settings g_terrain_ui = {0};

system_scene_registry g_scene_registry = {0};
 
/* Maya-like defaults: lighter blue-grey at the top fading to a dark grey at the bottom. */
scene_background g_scene_bg  = { { 0.43f, 0.49f, 0.57f }, { 0.16f, 0.17f, 0.19f } };
scene_sun        g_scene_sun = { 40.0f, 50.0f, 0.60f };
 
const scene_look_preset g_scene_look_presets[scene_look_preset_count] = {
    { "Maya", { 0.43f, 0.49f, 0.57f }, { 0.16f, 0.17f, 0.19f } },
    { "Dark", { 0.20f, 0.21f, 0.23f }, { 0.08f, 0.08f, 0.09f } },
    { "Sky",  { 0.30f, 0.50f, 0.80f }, { 0.76f, 0.83f, 0.91f } },
    { "Flat", { 0.27f, 0.27f, 0.28f }, { 0.27f, 0.27f, 0.28f } }
};
VkPipeline       g_scene_bg_pipeline = VK_NULL_HANDLE;
VkPipelineLayout g_scene_bg_layout   = VK_NULL_HANDLE;
 
manip_state g_manip = {
    .show_manipulator  = false,
    .snap              = manip_snap_off,
    .grid_size         = 1.0f,
    .point_snap_radius = 0.75f,
    .edit_tool         = manip_edit_none,
};
manip_frame_state g_manip_frame = { .frame = 0xFFFFFFFFu };
 
/* drag step per pixel / rounding step. 0.0625 = 1/16. Shift = fine, no rounding. */
const number_field_opts g_manip_pos_opts   = { .step = 0.03125f, .quantum = 0.0625f };
const number_field_opts g_manip_rot_opts   = { .step = 0.5f,     .quantum = 0.5f, .display_scale = 57.29578f };   /* radians stored, degrees shown */
const number_field_opts g_manip_scale_opts = { .step = 0.03125f, .quantum = 0.0625f };
const number_field_opts g_manip_size_opts  = { .step = 0.03125f, .quantum = 0.0625f, .min = 0.001f, .max = 1000000.0f };
const char*              g_manip_row_labels[3] = { "Pos", "Rot", "Scl" };
const number_field_opts* g_manip_row_opts[3]   = { &g_manip_pos_opts, &g_manip_rot_opts, &g_manip_scale_opts };
 
camera_prefs g_camera_prefs = { .near_clip = 0.1f, .far_clip = 5000.0f, .pan_speed = 1.0f };

uint32_t g_ui_frame_index = 0;

scene_view_state g_scene_view = {0};

char g_statusbar_text[statusbar_text_size] = {0};

bool     g_terrain_regions_changed = false;
float*   g_terrain_region_heights[terrain_region_count] = {0};
uint8_t* g_terrain_region_colors[terrain_region_count]  = {0};
int      g_terrain_built_since_log = 0;

edit_mode_mesh* g_edit_mesh = NULL;
bool           g_edit_mesh_have = false;
int            g_edit_chunk_x       = 0;
int            g_edit_chunk_z       = 0;
int            g_edit_stride        = 0;
uint32_t       g_edit_revision      = 0;
float*         g_edit_base          = NULL;
uint32_t       g_edit_base_capacity = 0;



/* ---- transform gizmo (engine_transform_gizmo.c) (were file statics g, s_mesh, s_snap, s_snap_cap, s_model, s_inv, prev_down, AXES) ---- */
gizmo_state       g_gizmo = { .dragging = false, .edit = false, .axis = gizmo_axis_none, .hover = gizmo_axis_none };
edit_mode_mesh*   g_gizmo_mesh = NULL;
editor_vec3*      g_gizmo_snap = NULL;
uint32_t          g_gizmo_snap_cap = 0;
editor_mat4       g_gizmo_model;
editor_mat4       g_gizmo_inverse;
bool              g_gizmo_prev_down = false;
const editor_vec3 g_gizmo_axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };