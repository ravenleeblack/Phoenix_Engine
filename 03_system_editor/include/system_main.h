#ifndef phoenix_system_main_h
#define phoenix_system_main_h

#include "engine_defines.h"
#include "engine_main.h"

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

/* EVERYTHING that used to be here now lives in engine_main.h and engine_globals.h (one definition each):
     editor_plugin, g_editor, engine_render_settings, g_render_settings, system_program, program_state,
     pump_system_messages, start_program, run_program, end_program, the frame chain, load_subsystem_libraries.
   create_game_ptr and create_game_handle: engine_game_state.h / engine_globals.h.
   renamed in the engine (update every caller):
     MonitorLayout     -> monitor_layout
     MonitorEnumProc   -> collect_monitor_layout
     system_proc       -> handle_window_message
   Editor only declarations go below this line. */

#endif