#include "engine_game_state.h"
#include "engine_memory.h"
#include "game_logger.h"
#include "game_main.h"

bool start_game(game_state* game)
{
    manage_game_data* state = (manage_game_data*)kallocate(sizeof(manage_game_data), MEMORY_TAG_GAME);
    if (!state) {
        phoenix_game_error("scene", "game state allocation failed.");
        return false;
    }

    state->running_time = 0.0f;
    state->active_score = 0;

    game->state = state;
    phoenix_game_info("scene", "game state allocated.");
    return true;
}

void update_game(game_state* game, float delta_time)
{
    manage_game_data* state = (manage_game_data*)game->state;
    if (!state) return;
    state->running_time += delta_time;
}

void shutdown_game(game_state* game)
{
    if (game->state) {
        kfree(game->state, sizeof(manage_game_data), MEMORY_TAG_GAME);
        game->state = 0;
    }
}

bool create_game(game_state* out_game)
{
    out_game->state    = 0;
    out_game->start    = start_game;
    out_game->update   = update_game;
    out_game->shutdown = shutdown_game;
    return true;
}