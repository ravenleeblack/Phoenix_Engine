#include "engine_globals.h"

void start_clock(system_clock* clock_state)
{
    if (!clock_state) return;

    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    clock_state->frequency  = (uint64_t)frequency.QuadPart;
    clock_state->start_time = (uint64_t)counter.QuadPart;
    clock_state->last_time  = clock_state->start_time;
    clock_state->delta_time = 0.0f;
    clock_state->total_time = 0.0;
    clock_state->elapsed    = 0;
}

void update_clock(system_clock* clock_state)
{
    if (!clock_state) return;
    if (clock_state->frequency == 0) return;    /* start_clock has not run yet */

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    const uint64_t current_time = (uint64_t)counter.QuadPart;
    const double   frequency    = (double)clock_state->frequency;

    const uint64_t elapsed_ticks = current_time - clock_state->last_time;
    const uint64_t total_ticks   = current_time - clock_state->start_time;

    /* divide in double, narrow to float only at the end */
    clock_state->delta_time = (float)((double)elapsed_ticks / frequency);
    clock_state->total_time = (double)total_ticks / frequency;
    clock_state->elapsed    = elapsed_ticks;
    clock_state->last_time  = current_time;
}

void shutdown_clock(system_clock* clock_state)
{
    if (!clock_state) return;
    clock_state->frequency  = 0;
    clock_state->start_time = 0;
    clock_state->last_time  = 0;
    clock_state->delta_time = 0.0f;
    clock_state->total_time = 0.0;
    clock_state->elapsed    = 0;
}

/* replaces ms_now: verb first, and it reads the frequency from main_clock instead of caching it in a static. */
double get_time_ms(void)
{
    if (main_clock.frequency == 0) return 0.0;

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return ((double)counter.QuadPart * 1000.0) / (double)main_clock.frequency;
}