#include "engine_globals.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    (void)previous_instance;
    (void)command_line;
    (void)show_command;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    /* window, then editor / loader / game dlls, then engine, vulkan, frame jobs, editor.
       start_program unwinds itself when a stage fails. */
    if (!start_program(instance))
    {
        return 1;
    }

    phoenix_engine_info("main", "tracing the program 01: starting the winmain");
    run_program();      /* message pump, input, editor update, game update, draw */

    end_program(); /* unwinds every boot stage in reverse */
    phoenix_engine_trace("main", "tracing the program 01: ending the winmain");
    return 0;
}