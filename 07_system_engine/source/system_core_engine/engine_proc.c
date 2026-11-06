#include "engine_globals.h"

/* the engine window procedure. the editor gets every message first (once the main loop runs),
   anything it does not take lands here. */
LRESULT CALLBACK handle_window_message(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    if (program_started && g_editor.handle_message)
    {
        LRESULT editor_result = 0;
        if (g_editor.handle_message(hwnd, message, wp, lp, &editor_result))
        {
            return editor_result;
        }
    }

    switch (message)
    {
        case WM_ERASEBKGND:
        {
            return 1;   /* vulkan paints every pixel, a gdi erase only flickers */
        }

        case WM_SIZE:
        {
            const int width  = (int)LOWORD(lp);
            const int height = (int)HIWORD(lp);

            if (wp == SIZE_MINIMIZED)
            {
                program_state.is_suspended = true;
                program_state.render_resume_pending = false;
                return 0;
            }

            program_state.is_suspended = false;
            program_state.width  = (int16_t)width;
            program_state.height = (int16_t)height;
            program_state.render_resume_pending = true;

            if (program_started && backend && backend->resized)
            {
                backend->resized(backend, width, height);
            }
            return 0;
        }

        case WM_CLOSE:
        {
            /* end_program destroys the window, after vulkan let go of the surface */
            program_state.is_running = false;
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, message, wp, lp);
}