#include "engine_globals.h"

/* mouse half of the input system. the keys are in engine_keyboard.c.
   the state lives in engine_globals.c: g_mouse_current, g_mouse_previous, g_mouse_wheel_notches.
   nothing here knows about the editor. camera moves, hit tests and tools are in editor_mouse.c and the other editor input files. */

unsigned char read_mouse_button(int virtual_key, bool focused)
{
    if (!focused) return 0;
    if (GetAsyncKeyState(virtual_key) & 0x8000) return 1;
    return 0;
}

void start_input_system(void)
{
    memset(&g_mouse_current,  0, sizeof(g_mouse_current));
    memset(&g_mouse_previous, 0, sizeof(g_mouse_previous));
    g_mouse_wheel_notches = 0.0f;
    update_system_keyboard_states();
}

void shutdown_input_system(void)
{
    memset(&g_mouse_current,  0, sizeof(g_mouse_current));
    memset(&g_mouse_previous, 0, sizeof(g_mouse_previous));
    g_mouse_wheel_notches = 0.0f;
}

/* call once per frame, before anything that reads input */
void update_input_system(double delta_time)
{
    (void)delta_time;

    g_mouse_previous = g_mouse_current;
    update_system_keyboard_states();

    bool focused = check_window_focus();
    g_mouse_current.buttons[button_left]   = read_mouse_button(VK_LBUTTON, focused);
    g_mouse_current.buttons[button_right]  = read_mouse_button(VK_RBUTTON, focused);
    g_mouse_current.buttons[button_middle] = read_mouse_button(VK_MBUTTON, focused);

    if (hwnd_window_00) {
        POINT p;
        if (GetCursorPos(&p) && ScreenToClient(hwnd_window_00, &p)) {
            g_mouse_current.x = (short)p.x;
            g_mouse_current.y = (short)p.y;
        }
    }
}

/* ---- mouse queries ---- */
bool input_is_button_down(buttons button)  { return (unsigned)button < button_max_buttons &&  g_mouse_current.buttons[button]; }
bool input_is_button_up(buttons button)    { return (unsigned)button < button_max_buttons && !g_mouse_current.buttons[button]; }
bool input_was_button_down(buttons button) { return (unsigned)button < button_max_buttons &&  g_mouse_previous.buttons[button]; }
bool input_was_button_up(buttons button)   { return (unsigned)button < button_max_buttons && !g_mouse_previous.buttons[button]; }

void get_mouse_position(int* x, int* y)
{
    if (x) *x = g_mouse_current.x;
    if (y) *y = g_mouse_current.y;
}

void get_previous_mouse_position(int* x, int* y)
{
    if (x) *x = g_mouse_previous.x;
    if (y) *y = g_mouse_previous.y;
}

/* ---- optional event hooks (polling covers it if the window procedure does not forward these) ---- */
void process_system_button(buttons button, bool pressed)
{
    if ((unsigned)button >= button_max_buttons) return;

    g_mouse_current.buttons[button] = 0;
    if (pressed) g_mouse_current.buttons[button] = 1;
}

void process_system_mouse_move(short x, short y)
{
    g_mouse_current.x = x;
    g_mouse_current.y = y;
}

/* z_delta: wheel notches (+1 / -1) or the raw win32 value (+/-120), which is normalised to notches.
   the engine only collects the wheel. the editor reads it with consume_mouse_wheel and decides what it means (grid zoom, dolly, loop rows). */
void process_system_mouse_wheel(short z_delta)
{
    if (z_delta == 0) return;

    float steps = (float)z_delta;
    if (z_delta >= 100 || z_delta <= -100) {
        steps = (float)z_delta / 120.0f;
    }
    g_mouse_wheel_notches += steps;
}

/* returns the notches collected since the last call and clears them */
float consume_mouse_wheel(void)
{
    float notches = g_mouse_wheel_notches;
    g_mouse_wheel_notches = 0.0f;
    return notches;
}