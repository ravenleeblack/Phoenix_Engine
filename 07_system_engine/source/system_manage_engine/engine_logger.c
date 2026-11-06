#include "engine_globals.h"

/* the color table, the level names and the current level live in engine_globals.c */

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

void set_log_level(log_level level)
{
    current_log_level = level;
}

void write_console(const char* message, unsigned char color_index)
{
    if (color_index > 5) color_index = 5;
    printf("%s%s\033[0m", ansi_color_strings[color_index], message);
    fflush(stdout);
}

void write_console_error(const char* message, unsigned char color_index)
{
    if (color_index > 5) color_index = 5;
    fprintf(stderr, "%s%s\033[0m", ansi_color_strings[color_index], message);
    fflush(stderr);
}

void enable_console_color(HANDLE console_handle)
{
    DWORD mode = 0;
    if (console_handle == NULL || console_handle == INVALID_HANDLE_VALUE) return;
    if (!GetConsoleMode(console_handle, &mode)) return;
    SetConsoleMode(console_handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

bool start_logging_system(void)
{
    enable_console_color(GetStdHandle(STD_OUTPUT_HANDLE));
    enable_console_color(GetStdHandle(STD_ERROR_HANDLE));
    return true;
}

void shutdown_logging_system(void)
{
}

void log_output_source_va(log_level level, const char* source, const char* tag, const char* format, va_list args)
{
    if (level > current_log_level) return;

    char out_message[4096];
    vsnprintf(out_message, sizeof(out_message), format, args);

    char final_message[4352];
    snprintf(final_message, sizeof(final_message), "%s [%s:%s]: %s\n",
             level_strings[level], source, tag, out_message);

    if (level < log_level_warn) {
        write_console_error(final_message, (unsigned char)level);
    } else {
        write_console(final_message, (unsigned char)level);
    }
}

void log_output_va(log_level level, const char* tag, const char* format, va_list args)
{
    log_output_source_va(level, "engine", tag, format, args);
}

void log_output(log_level level, const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(level, "engine", tag, format, args);
    va_end(args);
}

void phoenix_engine_fatal(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_fatal, "engine", tag, format, args);
    va_end(args);
}

void phoenix_engine_error(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_error, "engine", tag, format, args);
    va_end(args);
}

void phoenix_engine_warn(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_warn, "engine", tag, format, args);
    va_end(args);
}

void phoenix_engine_info(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_info, "engine", tag, format, args);
    va_end(args);
}

void phoenix_engine_debug(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_debug, "engine", tag, format, args);
    va_end(args);
}

void phoenix_engine_trace(const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_output_source_va(log_level_trace, "engine", tag, format, args);
    va_end(args);
}

void report_assertion_failure(const char* expression, const char* message, const char* file, int line)
{
    phoenix_engine_fatal("assertion", "failure: %s, message: '%s', in file: %s, line: %d",
                         expression, message, file, line);
}