#ifndef phoenix_engine_globals_h
#define phoenix_engine_globals_h


#define system_unit_m 4.0f

#include "engine_defines.h"


/*
RULES THAT MUST BE FOLLOWED.
uncomment as we start getting things fixed: 
Must use lowercase seperated by underscores. Even structs names, enum names, etc.
Must use verbs before nouns when naming anything
Must not use ternerys. i fucking hate ternery operators.
Must use braced control-flow headers: every if, else if, and else must use braces.
Must call extern in engine_globals.h and init in engine_globals.c
Must not have statics in any file. I fucking hate statics.
No static structs, No static enums, No static anything.
Math functions must go in math file and header.
Drawing stuff must go into drawing stuff and headers.
ALl information gets routed into statusbar.

NO CAPITAL LETTERS UNLESS WINDOWS OR VULKAN DEMANDS IT.
DO NOT REMOVE THIS COMMENT EVER. THESE ARE THE RULES.
IF SOMETHING IS BREAKING THESE RULES CHANGE IT THEN WE CHANGE EVERYTHING CONNECTED AS WE GO.
THIS INCLUDES INSIDE THE SOURCE FILES NOT JUST HEADERS
*/

/* include order is dependency order: base services, then the types engine_main.h needs, then the render types.
   THE ENGINE NEVER INCLUDES AN EDITOR HEADER (system_*.h). the editor includes this file, not the other way round.
   enabled for the boot path. the rest stay off until the files that need them are fixed. */
#include "engine_logger.h"
#include "engine_statusbar.h"      /* statusbar_text_size + set_statusbar_message */
#include "engine_clock.h"
#include "engine_memory.h"
#include "engine_math.h"
#include "engine_game_state.h"   /* moved up: engine_main.h needs manage_game_world */
#include "engine_input.h"
#include "engine_main.h"
#include "engine_phoenix_loader.h"
#include "engine_render.h"
#include "engine_shader_profile.h"
#include "engine_paint_map.h"
#include "engine_skin_gpu_runtime.h"
#include "engine_terrain.h"      /* terrain types moved here from the editor so engine lighting can use them */
#include "engine_lighting.h"
#include "engine_job_system.h"
#include "engine_skin_gpu.h"
#include "engine_parry.h"
#include "engine_task.h"

#include "engine_camera.h"             /* camera_prefs */
#include "engine_scene_objects.h"
#include "engine_object_database.h"
#include "engine_objects.h"      /* modeler types + object size api. world types are NOT in it: they live in engine_game_state.h */
#include "engine_scene_delete.h"
#include "engine_scene_look.h"
#include "engine_scene_offscreen.h"
#include "engine_direction_light.h"   /* needs system_scene_object + editor_vertex: after engine_scene_objects.h */
#include "engine_point_light.h"
#include "engine_spot_light.h"
#include "engine_area_light.h"
#include "engine_hemisphere_light.h"
#include "engine_grid.h"              /* needs world_camera + editor_mat4 */
#include "engine_element.h"
#include "engine_dock.h"
#include "engine_scene_manipulator.h"
#include "engine_edit_manager.h"         /* the edit manager: edit mesh data + the tools it manages. after engine_element.h */
#include "engine_transform_gizmo.h" 



/* everything the engine exe owns. defined in engine_globals.c and nowhere else. */

/* ---- logger state (used by engine_logger.c) ---- */
extern log_level   current_log_level;
extern const char* ansi_color_strings[6];
extern const char* level_strings[6];

/* ---- boot state ---- */
extern bool               program_iniated;     /* window, plugins, engine, vulkan and editor are all up */
extern bool               program_started;     /* the main loop is running: window messages may go to the editor */
extern program_boot_stage program_stage;       /* how far start_program got, end_program unwinds from here */

extern game_state start_game_state_instance;

/* ---- input state (used by engine_keyboard.c and engine_mouse.c) ---- */
extern unsigned char           g_key_states[256];       /* key_state_up / pressed / held / released */
extern bool                    g_key_prev_down[256];    /* "down" as of the previous update */
extern enum_input_profile_type g_input_profile;
extern mouse_state             g_mouse_current;
extern mouse_state             g_mouse_previous;
extern float                   g_mouse_wheel_notches;   /* collected by process_system_mouse_wheel, cleared by consume_mouse_wheel */

/* ---- rendering ---- */
extern render_backend*   backend;

extern VkPipelineLayout pipeline_layout_ui;
extern VkPipeline       graphics_pipeline_ui;

extern VkPipelineLayout pipeline_layout_grid;
extern VkPipeline       graphics_pipeline_grid;

extern engine_render_settings g_render_settings;

/* ---- vulkan backend state (engine_vulkan_backend.c), were file statics ---- */
extern unsigned int          g_cached_framebuffer_width;
extern unsigned int          g_cached_framebuffer_height;
extern unsigned long long    g_framebuffer_size_generation;
extern unsigned long long    g_framebuffer_size_last_generation;
extern bool                  g_recreating_swapchain;
extern uint32_t              g_current_inflight_cpu_frame;   /* cpu pacing ring index, independent of the swapchain image index */
extern VkDebugUtilsMessengerEXT g_debug_messenger;
extern vulkan_system_context g_vulkan_runtime_context;       /* was a function static in start_vulkan_render_backend */

/* ---- fallback ui texture (engine_vulkan_backend.c), were file statics ---- */
extern VkImage          g_ui_fallback_image;
extern VkDeviceMemory   g_ui_fallback_memory;
extern VkImageView      g_ui_fallback_view;
extern VkSampler        g_ui_fallback_sampler;
extern VkDescriptorPool g_ui_fallback_pool;
extern VkDescriptorSet  g_ui_fallback_set;

/* ---- swapchain targets (engine_vulkan_swapchain.c), were file statics ---- */
extern bool           g_swapchain_is_srgb;
extern vulkan_image   g_swapchain_depth_image;
extern VkImage        g_msaa_image;
extern VkDeviceMemory g_msaa_memory;
extern VkImageView    g_msaa_view;

/* ---- shader search folders (engine_vulkan_pipeline.c), was a file static ---- */
extern const char* const g_shader_dirs[shader_directory_count];

/* ---- modeler (engine_objects.h) ---- */
extern system_object_database object_database;       /* modeler objects database. need an object? get it from the editor. */
extern manage_modeler         edit_object_workspace;  /* global instance handle for the modeler workspace data */

/* ---- lighting and terrain ---- */
extern light_settings  g_light;
extern system_terrain  g_terrain;

/* ---- directional lights (engine_direction_light.c) ---- */
extern light_object    g_lights[light_max_objects];
extern int             g_light_count;
extern int             g_light_driver;      /* index in g_lights that drives the sun, -1 = nobody */
extern glyph           g_sun_glyph;

/* ---- scene view (engine_grid.c) ---- */
extern bool            g_scene_cursor_in_view;   /* written by the editor once per frame: cursor is over the Scene view */

/* ---- windows: dll modules and main window ---- */
extern HMODULE     system_editor_dll;
extern HMODULE     system_loader_dll;
extern HMODULE     system_game_dll;
extern HWND        hwnd_window_00;
extern const char* program_window_class_name;
extern const char* program_window_title;

/* ---- contexts ---- */
extern system_clock          main_clock;
extern system_program        program_state;
extern system_render_context render_context;
system_render_context* get_engine_render_context(void);
system_program* get_engine_program_state(void);
VkPipeline get_engine_grid_pipeline(void);
extern int g_scene_msaa_samples;
void set_engine_scene_msaa(int samples);                /* the editor calls it when it decides the scene pass sample count */
VkSampleCountFlagBits get_scene_msaa_sample_count(void);
VkPipelineLayout get_engine_grid_pipeline_layout(void);
extern system_engine_context engine_context;

void em_gpu_retire(uint64_t vertex_buffer, uint64_t vertex_memory,
                   uint64_t index_buffer, uint64_t index_memory);
void em_mesh_free(edit_mode_mesh* mesh);
extern system_element_context element_context;


/* ---- plugins ---- */
extern editor_plugin   g_editor;
extern loader_plugin   g_loader;
extern create_game_ptr create_game_handle;

/* the editor owns element_context, the status bar and menu bar buffers, the manipulator (g_manip),
   the scene view settings and the modeler. they are declared in the editor's system_globals.h, not here. */


/* ---- scene registry (engine_scene_objects.c) ---- */
extern system_scene_registry g_scene_registry;
 
/* ---- scene look (engine_scene_look.c) ---- */
extern scene_background  g_scene_bg;
extern scene_sun         g_scene_sun;
extern const scene_look_preset g_scene_look_presets[scene_look_preset_count];
extern VkPipeline        g_scene_bg_pipeline;
extern VkPipelineLayout  g_scene_bg_layout;
 
/* ---- manipulator (engine_scene_manipulator.c) ---- */
extern manip_state               g_manip;
extern manip_frame_state         g_manip_frame;
extern const number_field_opts   g_manip_pos_opts;
extern const number_field_opts   g_manip_rot_opts;
extern const number_field_opts   g_manip_scale_opts;
extern const number_field_opts   g_manip_size_opts;
extern const char*               g_manip_row_labels[3];
extern const number_field_opts*  g_manip_row_opts[3];

extern camera_prefs g_camera_prefs;

/* ---- ui frame counter ---- */
extern uint32_t g_ui_frame_index;

/* ---- statusbar (engine_statusbar.c) ---- */
extern char g_statusbar_text[statusbar_text_size];

/* ---- terrain ui + scene view ---- */
extern terrain_settings g_terrain_ui;
extern scene_view_state g_scene_view;

/* ---- terrain regions (were file statics s_regions_changed, s_h, s_c, s_built_since_log) ---- */
extern bool     g_terrain_regions_changed;
extern float*   g_terrain_region_heights[terrain_region_count];   /* pointers into the database blocks, refreshed every frame */
extern uint8_t* g_terrain_region_colors[terrain_region_count];    /* paint layer of each region (rgba8 per cell), null until painted */
extern int      g_terrain_built_since_log;

/* ---- edit mode bridge (were file statics s_edit, s_edit_have, s_ecx, s_ecz, s_estride, s_erev, s_ebase, s_ebase_cap) ---- */
extern edit_mode_mesh* g_edit_mesh;
extern bool           g_edit_mesh_have;
extern int            g_edit_chunk_x;
extern int            g_edit_chunk_z;
extern int            g_edit_stride;
extern uint32_t       g_edit_revision;
extern float*         g_edit_base;
extern uint32_t       g_edit_base_capacity;


/* ---- transform gizmo (engine_transform_gizmo.c) (were file statics g, s_mesh, s_snap, s_snap_cap, s_model, s_inv, prev_down, AXES) ---- */
extern gizmo_state        g_gizmo;
extern edit_mode_mesh*    g_gizmo_mesh;
extern editor_vec3*       g_gizmo_snap;        /* vertex positions at drag start (object space) */
extern uint32_t           g_gizmo_snap_cap;
extern editor_mat4        g_gizmo_model;
extern editor_mat4        g_gizmo_inverse;
extern bool               g_gizmo_prev_down;
extern const editor_vec3  g_gizmo_axes[3];

#endif