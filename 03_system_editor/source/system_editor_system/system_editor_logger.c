#include "system_logger.h"

void phoenix_editor_fatal(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_fatal, "editor", tag, format, args);
    va_end(args);
}

void phoenix_editor_error(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_error, "editor", tag, format, args);
    va_end(args);
}

void phoenix_editor_warn(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_warn, "editor", tag, format, args);
    va_end(args);
}

void phoenix_editor_info(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_info, "editor", tag, format, args);
    va_end(args);
}

void phoenix_editor_debug(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_debug, "editor", tag, format, args);
    va_end(args);
}

void phoenix_editor_trace(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_trace, "editor", tag, format, args);
    va_end(args);
}