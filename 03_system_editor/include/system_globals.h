#pragma once

#include "system_editor_input.h"

extern char status_bar_text_buffer[256];
extern manage_sculpt_stroke g_sculpt_stroke;
extern bool editor_frame_key_down;
extern bool editor_home_key_down;

/* ========================================================================= */
/* system_globals.h : THE one header. Every editor .c file does              */
/*                                                                           */
/*     #include "system_globals.h"     <- first line, nothing else needed    */
/*                                                                           */
/* ORDER BELOW MATTERS. Each group only needs the groups above it, so the    */
/* include loops (dock <-> mode_state <-> edit_manager) cannot bite.         */
/* If a header ever says "unknown type name X", move that header DOWN        */
/* below the header that defines X. Do not add includes to the other headers */
/* that point back at this file.                                             */
/* ========================================================================= */


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


/*==================================================================================================*/
/*                                  HEADERS (dependency order)                                      */
/*==================================================================================================*/

/* ---- 1. Engine base: platform, vulkan, logging, memory, clock, math, input, render, game state ----
   engine_globals.h pulls in engine_defines.h, engine_logger.h, engine_clock.h, engine_memory.h,
   engine_math.h, engine_game_state.h, engine_input.h, engine_main.h and engine_render.h in the right order,
   and defines system_unit_m. it also declares every engine global (pipelines, dll handles,
   hwnd_window_00, engine_context, render_context, create_game_handle), so none are repeated here. */
#include "engine_globals.h"

/* ---- 2. Editor math, theme, UI elements ---- */
#include "system_math.h"
#include "system_theme.h"
#include "system_element.h"
#include "system_font.h"

/* ---- 3. Rendering ---- */
#include "system_shader_profile.h"
#include "system_scene_offscreen.h"

/* ---- 4. Databases and settings ---- */
#include "system_object_database.h"
#include "system_settings_database.h"
#include "system_preferences.h"
#include "system_import_export.h"
#include "system_undo.h"
#include "system_object_pick.h"

/* ---- 5. Scene objects ---- */
#include "system_scene_objects.h"
#include "system_objects.h"
#include "system_textureshop_manager.h"

/* ---- 6. Modes and edit mesh (mode_state before dock: dock uses system_mode_context) ---- */
#include "system_mode_state.h"
#include "system_edit_manager.h"

/* ---- 7. Terrain and smoothing ---- */
#include "system_scene_terrain.h"
#include "system_smoothing.h"

/* ---- 8. Docking, menus, scene tools ---- */
#include "system_dock.h"
#include "system_menubar.h"
#include "system_sceneview.h"
#include "system_context_menu.h"
#include "system_camera_prefs.h"
#include "system_scene_look.h"
#include "system_scene_delete.h"
#include "system_scene_manipulator.h"
#include "system_transform_gizmo.h"

/* ---- 9. Content systems (lighting needs the terrain types from group 7) ---- */
#include "system_board.h"
#include "engine_direction_light.h"
#include "engine_point_light.h"
#include "engine_spot_light.h"
#include "engine_area_light.h"
#include "engine_hemisphere_light.h"
#include "engine_lighting.h"
#include "system_paint_texture.h"
#include "system_file_archive.h"
#include "engine_parry.h"

/* ---- 10. Loader and program entry (last) ---- */
#include "engine_phoenix_loader.h"
#include "system_main.h"       /* verb-first function pointer types */

#include "system_brush_element.h"
#include "system_skin_gpu.h"
#include "system_light_gpu.h"



/*==================================================================================================*/
/*                                  EDITOR GLOBALS (extern only, defined in system_globals.c)       */
/*==================================================================================================*/

/* ---- text buffers ----
   the status bar text is g_statusbar_text (engine_statusbar.h, extern in engine_globals.h). write it with set_statusbar_message(). */
extern char menu_bar_text_buffer[512];

/* ---- run state ---- */
extern bool is_game_running;
extern bool scene_view_has_focus;

/* ---- editor light list (defined in system_globals.c, used by system_directional_light.c) ---- */
extern light_object editor_lights[light_max_objects];
extern int          editor_light_count;
extern int          editor_light_driver;
extern glyph        editor_sun_glyph;
extern bool is_engine_running;

/* element_context is declared in engine_globals.h. */

/* dock_workspace (manage_docking) is declared in system_dock.h. */
/* the color palette is declared in system_theme.h */

/* ---- mouse and wheel input ---- */
extern int                  g_mouse_prev_x;
extern int                  g_active_id_idle_frames;
extern int                  g_mouse_prev_y;

extern WNDPROC              g_wheel_old_proc;
extern HWND                 g_wheel_hooked;
extern bool                 g_wheel_unicode;
extern volatile LONG        g_wheel_accum;       /* raw wheel_delta units, 120 per notch */
extern int                  g_wheel_logged;

/* ---- paint and sculpt state ---- */
extern editor_vec3          g_paint_last;
extern bool                 g_paint_was_painting;
extern bool                 g_paint_prev_down;
extern unsigned             g_paint_last_frame;

//extern manage_sculpt_stroke g_sculpt_stroke;
extern bool                 g_sculpt_prev_left;
extern int                  g_sculpt_prev_mx;
extern unsigned             g_sculpt_last_frame;

/* g_ui_frame_index and g_statusbar_text are declared in engine_globals.h. */

/*==================================================================================================*/
/*                                  FUNCTION PROTOTYPES                                             */
/*==================================================================================================*/

/* start_modeler_system is declared in engine_objects.h. */

/* ---- rendering ---- */
extern VkShaderModule load_shader_binary_module(VkDevice logical_device, const char* filepath);