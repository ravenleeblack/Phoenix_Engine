#include "engine_globals.h"

/* keyboard half of the input system. the mouse half is in engine_mouse.c.
   the state arrays live in engine_globals.c: g_key_states, g_key_prev_down, g_input_profile. */

bool check_window_focus(void)
{
    if (!hwnd_window_00) return true;
    return GetForegroundWindow() == hwnd_window_00;
}

bool check_key_state_down(unsigned char state)
{
    return state == key_state_pressed || state == key_state_held;
}

/* poll once per frame (update_input_system in engine_mouse.c calls this) */
void update_system_keyboard_states(void)
{
    bool focused = check_window_focus();

    for (int i = 0; i < 256; i++)
    {
        g_key_prev_down[i] = check_key_state_down(g_key_states[i]);

        bool down = focused && ((GetAsyncKeyState(i) & 0x8000) != 0);

        if (down) {
            if (g_key_states[i] == key_state_up || g_key_states[i] == key_state_released) {
                g_key_states[i] = key_state_pressed;
            } else {
                g_key_states[i] = key_state_held;
            }
        } else {
            if (g_key_states[i] == key_state_pressed || g_key_states[i] == key_state_held) {
                g_key_states[i] = key_state_released;
            } else {
                g_key_states[i] = key_state_up;
            }
        }
    }
}

void apply_input_profile_mappings(enum_input_profile_type profile)
{
    if ((unsigned)profile < (unsigned)input_profile_max) {
        g_input_profile = profile;
    }
}

/* ---- keyboard queries ---- */
bool input_is_key_down(keys key)  { return (unsigned)key < 256 &&  check_key_state_down(g_key_states[key]); }
bool input_is_key_up(keys key)    { return (unsigned)key < 256 && !check_key_state_down(g_key_states[key]); }
bool input_was_key_down(keys key) { return (unsigned)key < 256 &&  g_key_prev_down[key]; }
bool input_was_key_up(keys key)   { return (unsigned)key < 256 && !g_key_prev_down[key]; }

/* optional event hook: forces a key state immediately (polling corrects it next frame) */
void process_system_keys(keys key, bool pressed)
{
    if ((unsigned)key >= 256) return;

    bool was_down = check_key_state_down(g_key_states[key]);

    if (pressed) {
        if (was_down) {
            g_key_states[key] = key_state_held;
        } else {
            g_key_states[key] = key_state_pressed;
        }
    } else {
        if (was_down) {
            g_key_states[key] = key_state_released;
        } else {
            g_key_states[key] = key_state_up;
        }
    }
}