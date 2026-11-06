#include "engine_globals.h"
 
void set_statusbar_message(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_statusbar_text, sizeof(g_statusbar_text), fmt, args);
    va_end(args);
}