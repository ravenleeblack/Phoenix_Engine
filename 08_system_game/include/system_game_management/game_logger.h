#ifndef phoenix_game_logger_h
#define phoenix_game_logger_h

#include "engine_logger.h"
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

/* the game side of the logger. same console and same level filter as the engine (they live in phoenix_engine.exe).
   output lines are tagged "game:tag" so the module is visible. */

void phoenix_game_fatal(const char* tag, const char* format, ...);
void phoenix_game_error(const char* tag, const char* format, ...);
void phoenix_game_warn (const char* tag, const char* format, ...);
void phoenix_game_info (const char* tag, const char* format, ...);
void phoenix_game_debug(const char* tag, const char* format, ...);
void phoenix_game_trace(const char* tag, const char* format, ...);

#endif