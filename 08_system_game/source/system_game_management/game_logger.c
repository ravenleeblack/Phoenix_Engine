#include "game_logger.h"

void phoenix_game_fatal(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_fatal, "game", tag, format, args);
    va_end(args);
}

void phoenix_game_error(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_error, "game", tag, format, args);
    va_end(args);
}

void phoenix_game_warn(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_warn, "game", tag, format, args);
    va_end(args);
}

void phoenix_game_info(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_info, "game", tag, format, args);
    va_end(args);
}

void phoenix_game_debug(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_debug, "game", tag, format, args);
    va_end(args);
}

void phoenix_game_trace(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_trace, "game", tag, format, args);
    va_end(args);
}