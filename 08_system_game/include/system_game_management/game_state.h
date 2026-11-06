/* engine_game_state.h (engine owns it, game includes it) */
#ifndef phoenix_engine_game_state_h
#define phoenix_engine_game_state_h

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

typedef struct game_state game_state;

typedef bool (*start_game_ptr)   (game_state* game);
typedef void (*update_game_ptr)  (game_state* game, float delta_time);
typedef void (*shutdown_game_ptr)(game_state* game);

struct game_state
{
    void*             state;      /* opaque: only the game knows what is inside */
    start_game_ptr    start;
    update_game_ptr   update;
    shutdown_game_ptr shutdown;
};

/* the one symbol the game dll exports */
typedef bool (*create_game_ptr)(game_state* out_game);

#endif