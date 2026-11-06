#ifndef phoenix_engine_input_h
#define phoenix_engine_input_h

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

/* engine_input.h: raw keyboard and mouse state only. nothing here may know about the editor or its ui types.
   the editor includes this file, never the other way round. */

typedef enum buttons {
    button_left,
    button_right,
    button_middle,
    button_max_buttons
} buttons;

/* the phoenix_key_ prefix keeps these clear of the win32 macros (delete, next, ...) */
#define define_key(name, code) phoenix_key_##name = code

typedef enum keys {
    define_key(backspace, 0x08),
    define_key(enter, 0x0d),
    define_key(tab, 0x09),
    define_key(shift, 0x10),
    define_key(control, 0x11),
    define_key(pause, 0x13),
    define_key(capital, 0x14),
    define_key(escape, 0x1b),
    define_key(convert, 0x1c),
    define_key(nonconvert, 0x1d),
    define_key(accept, 0x1e),
    define_key(modechange, 0x1f),
    define_key(space, 0x20),
    define_key(prior, 0x21),
    define_key(next, 0x22),
    define_key(end, 0x23),
    define_key(home, 0x24),
    define_key(left, 0x25),
    define_key(up, 0x26),
    define_key(right, 0x27),
    define_key(down, 0x28),
    define_key(select, 0x29),
    define_key(print, 0x2a),
    define_key(execute, 0x2b),
    define_key(snapshot, 0x2c),
    define_key(insert, 0x2d),
    define_key(delete, 0x2e),
    define_key(help, 0x2f),
    define_key(a, 0x41),
    define_key(b, 0x42),
    define_key(c, 0x43),
    define_key(d, 0x44),
    define_key(e, 0x45),
    define_key(f, 0x46),
    define_key(g, 0x47),
    define_key(h, 0x48),
    define_key(i, 0x49),
    define_key(j, 0x4a),
    define_key(k, 0x4b),
    define_key(l, 0x4c),
    define_key(m, 0x4d),
    define_key(n, 0x4e),
    define_key(o, 0x4f),
    define_key(p, 0x50),
    define_key(q, 0x51),
    define_key(r, 0x52),
    define_key(s, 0x53),
    define_key(t, 0x54),
    define_key(u, 0x55),
    define_key(v, 0x56),
    define_key(w, 0x57),
    define_key(x, 0x58),
    define_key(y, 0x59),
    define_key(z, 0x5a),
    define_key(lwin, 0x5b),
    define_key(rwin, 0x5c),
    define_key(apps, 0x5d),
    define_key(sleep, 0x5f),
    define_key(numpad0, 0x60),
    define_key(numpad1, 0x61),
    define_key(numpad2, 0x62),
    define_key(numpad3, 0x63),
    define_key(numpad4, 0x64),
    define_key(numpad5, 0x65),
    define_key(numpad6, 0x66),
    define_key(numpad7, 0x67),
    define_key(numpad8, 0x68),
    define_key(numpad9, 0x69),
    define_key(multiply, 0x6a),
    define_key(add, 0x6b),
    define_key(separator, 0x6c),
    define_key(subtract, 0x6d),
    define_key(decimal, 0x6e),
    define_key(divide, 0x6f),
    define_key(f1, 0x70),
    define_key(f2, 0x71),
    define_key(f3, 0x72),
    define_key(f4, 0x73),
    define_key(f5, 0x74),
    define_key(f6, 0x75),
    define_key(f7, 0x76),
    define_key(f8, 0x77),
    define_key(f9, 0x78),
    define_key(f10, 0x79),
    define_key(f11, 0x7a),
    define_key(f12, 0x7b),
    define_key(f13, 0x7c),
    define_key(f14, 0x7d),
    define_key(f15, 0x7e),
    define_key(f16, 0x7f),
    define_key(f17, 0x80),
    define_key(f18, 0x81),
    define_key(f19, 0x82),
    define_key(f20, 0x83),
    define_key(f21, 0x84),
    define_key(f22, 0x85),
    define_key(f23, 0x86),
    define_key(f24, 0x87),
    define_key(numlock, 0x90),
    define_key(scroll, 0x91),
    define_key(numpad_equal, 0x92),
    define_key(lshift, 0xa0),
    define_key(rshift, 0xa1),
    define_key(lcontrol, 0xa2),
    define_key(rcontrol, 0xa3),
    define_key(lmenu, 0xa4),
    define_key(rmenu, 0xa5),
    define_key(semicolon, 0xba),
    define_key(plus, 0xbb),
    define_key(comma, 0xbc),
    define_key(minus, 0xbd),
    define_key(period, 0xbe),
    define_key(slash, 0xbf),
    define_key(grave, 0xc0),

    keys_max_keys
} keys;

typedef enum {
    key_state_up,
    key_state_pressed,
    key_state_held,
    key_state_released
} enum_key_state;

typedef enum {
    input_profile_maya,
    input_profile_blender,
    input_profile_max
} enum_input_profile_type;

typedef struct keyboard_state {
    bool keys[256];
} keyboard_state;

typedef struct mouse_state {
    short x;
    short y;
    unsigned char buttons[button_max_buttons];
} mouse_state;

typedef struct system_input_state {
    keyboard_state keyboard_current;
    keyboard_state keyboard_previous;
    mouse_state    mouse_current;
    mouse_state    mouse_previous;
} system_input_state;

void update_system_keyboard_states(void);
void apply_input_profile_mappings(enum_input_profile_type profile);

void start_input_system(void);
void shutdown_input_system(void);
void update_input_system(double delta_time);
bool check_window_focus(void);

/* keyboard input: exported so the editor and game dlls can read it through phoenix_engine.lib */
phoenix_engine_api bool input_is_key_down(keys key);
phoenix_engine_api bool input_is_key_up(keys key);
phoenix_engine_api bool input_was_key_down(keys key);
phoenix_engine_api bool input_was_key_up(keys key);

/* mouse input */
phoenix_engine_api bool input_is_button_down(buttons button);
phoenix_engine_api bool input_is_button_up(buttons button);
phoenix_engine_api bool input_was_button_down(buttons button);
phoenix_engine_api bool input_was_button_up(buttons button);
phoenix_engine_api void get_mouse_position(int* x, int* y);
phoenix_engine_api void get_previous_mouse_position(int* x, int* y);

/* fed by the window procedure */
void process_system_keys(keys key, bool pressed);
void process_system_button(buttons button, bool pressed);
void process_system_mouse_move(short x, short y);
void process_system_mouse_wheel(short z_delta);

#endif
#define PHOENIX_KEY_DELETE 0x2E