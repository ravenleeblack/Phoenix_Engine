#ifndef phoenix_engine_main_h
#define phoenix_engine_main_h

#include "engine_defines.h"
#include "engine_clock.h"
#include "engine_game_state.h"

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

/* phoenix_engine.exe owns the process: window, message loop, vulkan device, job threads, frame chain.
   phoenix_editor.dll, phoenix_loader.dll and system_game.dll are plugins it loads.
   types and prototypes only in this header. every extern lives in engine_globals.h, every definition in engine_globals.c.
   system_engine_context needs manage_game_world: engine_globals.h includes its owner before this header. */

/* ---- boot sequence: start_program walks these in order, end_program unwinds them in reverse ---- */
typedef enum program_boot_stage
{
    program_boot_stage_none = 0,
    program_boot_stage_logging,
    program_boot_stage_window,
    program_boot_stage_libraries,
    program_boot_stage_engine,
    program_boot_stage_render,
    program_boot_stage_frame_jobs,
    program_boot_stage_editor
} program_boot_stage;

/* game plugin: create_game_ptr lives in engine_game_state.h */

/* ---- editor plugin (phoenix_editor.dll): everything the engine calls into the editor ---- */
typedef bool            (*load_editor_settings_ptr)(void);
typedef void            (*save_editor_settings_ptr)(void);
typedef bool            (*start_editor_ptr)(void);
typedef void            (*update_editor_ptr)(float delta_time);
typedef VkCommandBuffer (*begin_editor_frame_pass_ptr)(void);
typedef void            (*draw_editor_ui_ptr)(VkCommandBuffer cmd, float delta_time);
typedef void            (*end_editor_frame_pass_ptr)(VkCommandBuffer cmd);
typedef bool            (*handle_editor_message_ptr)(HWND hwnd, UINT message, WPARAM wp, LPARAM lp, LRESULT* result);
typedef void            (*shutdown_editor_ptr)(void);

/* called by the vulkan backend while it builds the device (the offscreen scene pass belongs to the editor) */
typedef bool (*create_offscreen_renderpass_ptr)(VkFormat color_format, VkFormat depth_format);
typedef bool (*create_offscreen_resources_ptr)(void);
typedef void (*shutdown_offscreen_ptr)(void);
typedef bool (*create_scene_object_pipeline_ptr)(VkDevice device, VkRenderPass offscreen_renderpass, bool enable_depth);

typedef struct editor_plugin
{
    load_editor_settings_ptr         load_settings;
    save_editor_settings_ptr         save_settings;
    start_editor_ptr                 start;
    update_editor_ptr                update;
    begin_editor_frame_pass_ptr      begin_frame_pass;
    draw_editor_ui_ptr               draw_ui;
    end_editor_frame_pass_ptr        end_frame_pass;
    handle_editor_message_ptr        handle_message;
    shutdown_editor_ptr              shutdown;

    create_offscreen_renderpass_ptr  create_offscreen_renderpass;
    create_offscreen_resources_ptr   create_offscreen_resources;
    shutdown_offscreen_ptr           shutdown_offscreen;
    create_scene_object_pipeline_ptr create_scene_object_pipeline;
} editor_plugin;

/* ---- settings the renderer needs before the editor exists. the editor writes them (sync_render_settings in phoenix_editor.dll) ---- */
typedef struct engine_render_settings
{
    bool vsync;
    int  msaa_samples;      /* 1, 2, 4 or 8 */
    bool fxaa;
} engine_render_settings;

VkSampleCountFlagBits get_render_msaa_sample_count(void);
void sync_render_settings(void);        /* defined in the editor: copies g_prefs into g_render_settings */

/* ---- process ---- */
typedef struct monitor_layout
{
    RECT monitor_01_rect;       /* primary monitor work area */
    RECT monitor_02_rect;       /* first non primary monitor work area */
    bool has_primary;
    bool has_secondary;
    int  monitor_count;
} monitor_layout;

typedef struct system_program
{
    bool is_running;
    bool is_suspended;
    bool render_resume_pending;
    int16_t width;
    int16_t height;
    system_clock clock_state;
    double last_time;
} system_program;

typedef struct system_engine_context
{
    double total_simulation_time;
    float time_scale;
    bool is_paused;

    manage_game_world world_state;
} system_engine_context;

/* ---- window ---- */
BOOL CALLBACK collect_monitor_layout(HMONITOR monitor, HDC monitor_dc, LPRECT monitor_rect, LPARAM user_data);
LRESULT CALLBACK handle_window_message(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
bool create_program_window(HINSTANCE instance);
void destroy_program_window(void);

/* ---- plugin loading (system_dll_loader.c) ---- */
FARPROC resolve_system_symbol(HMODULE module, const char* module_name, const char* symbol_name);
bool load_editor_library(void);
bool load_loader_library(void);
bool load_game_library(void);
bool load_subsystem_libraries(void);     /* editor, then loader, then game */
void unload_subsystem_libraries(void);

/* ---- program ---- */
bool start_program(void* h_instance_ptr);
bool run_program(void);
void end_program(void);
bool pump_system_messages(system_program* program);

/* ---- frame chain (engine_frame_jobs.c) ---- */
bool start_frame_jobs(void);
void shutdown_frame_jobs(void);
void submit_frame(float delta_time);

/* ---- engine ---- */
bool start_engine(system_engine_context* context);
void update_engine(system_engine_context* context, float delta_time);
void shutdown_engine(system_engine_context* context);

void start_world_registry(manage_game_world* world);
void start_engine_input(system_engine_context* context, float delta_time);

#endif