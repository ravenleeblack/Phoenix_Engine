#ifndef phoenix_engine_logger_h
#define phoenix_engine_logger_h

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

/* one console writer and one level filter, owned by the engine exe.
   the engine calls phoenix_engine_*, the editor dll calls phoenix_editor_* (system_logger.h),
   the game dll calls phoenix_game_* (game_logger.h). all three end up in log_output_source_va. */

typedef enum log_level
{
    log_level_fatal = 0,
    log_level_error = 1,
    log_level_warn  = 2,
    log_level_info  = 3,
    log_level_debug = 4,
    log_level_trace = 5
} log_level;

bool start_logging_system(void);
void shutdown_logging_system(void);
void enable_console_color(HANDLE console_handle);

/* runtime filter: anything more verbose than this level is dropped */
void set_log_level(log_level level);

void log_output(log_level level, const char* tag, const char* format, ...);
void log_output_va(log_level level, const char* tag, const char* format, va_list args);
void log_output_source_va(log_level level, const char* source, const char* tag, const char* format, va_list args);

void phoenix_engine_fatal(const char* tag, const char* format, ...);
void phoenix_engine_error(const char* tag, const char* format, ...);
void phoenix_engine_warn (const char* tag, const char* format, ...);
void phoenix_engine_info (const char* tag, const char* format, ...);
void phoenix_engine_debug(const char* tag, const char* format, ...);
void phoenix_engine_trace(const char* tag, const char* format, ...);

#define phoenix_error phoenix_engine_error
#define phoenix_fatal phoenix_engine_fatal
#define phoenix_warn phoenix_engine_warn
#define phoenix_info phoenix_engine_info
#define phoenix_debug phoenix_engine_debug
#define phoenix_trace phoenix_engine_trace

void write_console(const char* message, unsigned char color_index);
void write_console_error(const char* message, unsigned char color_index);
void report_assertion_failure(const char* expression, const char* message, const char* file, int line);



#endif