#include "engine_globals.h"

/* the whole boot, in order:
   1 window  2 editor / loader / game dlls  3 editor settings  4 engine + world + game  5 vulkan  6 frame jobs  7 editor  8 show.
   program_stage records how far we got so end_program can unwind exactly that much. */
bool start_program(void* h_instance_ptr)
{
    HINSTANCE instance = (HINSTANCE)h_instance_ptr;

    if (program_iniated || program_stage != program_boot_stage_none)
    {
        phoenix_engine_error("boot", "start_program called more than once.");
        return false;
    }

    start_logging_system();
    program_stage = program_boot_stage_logging;
    phoenix_engine_info("boot", "initializing window, editor module, loader module, game module and vulkan...");

    timeBeginPeriod(1);    /* Sleep(1) really sleeps 1 ms instead of up to ~15.6 ms */

    /* 1. window */
    if (!create_program_window(instance)) {
        end_program();
        return false;
    }
    program_stage = program_boot_stage_window;

    /* 2. plugins. the stage is set first so a half loaded set still gets unloaded. */
    program_stage = program_boot_stage_libraries;
    if (!load_subsystem_libraries()) {
        phoenix_engine_fatal("boot", "dynamic system linkage failure. exiting...");
        end_program();
        return false;
    }

    /* 3. aa / vsync settings: the editor loads its prefs and fills g_render_settings before the renderer builds its renderpass and pipelines */
    if (!g_editor.load_settings()) {
        phoenix_engine_warn("boot", "editor settings not loaded, using defaults.");
    }

    /* 4. engine, world registry, game. start_world_registry resets its own count, so it is safe before start_engine. */
    start_world_registry(&engine_context.world_state);
    if (!start_engine(&engine_context)) {
        phoenix_engine_fatal("boot", "start_engine failed.");
        end_program();
        return false;
    }
    program_stage = program_boot_stage_engine;

    if (!create_game_handle(&start_game_state_instance)) {
        phoenix_engine_fatal("boot", "create_game failed.");
        end_program();
        return false;
    }

    if (!start_game_state_instance.start || !start_game_state_instance.start(&start_game_state_instance)) {
        phoenix_engine_fatal("boot", "game start failed.");
        end_program();
        return false;
    }

    /* 5. vulkan: instance, surface from hwnd_window_00, device, swapchain. calls g_editor.create_offscreen_* on the way. */
    if (!start_render_frontend(program_window_title)) {
        phoenix_engine_fatal("boot", "vulkan render frontend failed to start.");
        end_program();
        return false;
    }
    program_stage = program_boot_stage_render;

    /* 6. job threads: the editor posts tasks, so they must exist before it starts */
    if (!start_frame_jobs()) {
        phoenix_engine_fatal("boot", "frame jobs failed to start.");
        end_program();
        return false;
    }
    program_stage = program_boot_stage_frame_jobs;

    /* 7. editor: dock tree, object database, ui pipelines. needs the device and the job threads. */
    if (!g_editor.start()) {
        phoenix_engine_fatal("boot", "editor failed to start.");
        end_program();
        return false;
    }
    program_stage = program_boot_stage_editor;

    /* 8. everything is up: show the window */
    program_state.is_running   = true;
    program_state.is_suspended = false;
    program_state.render_resume_pending = false;
    program_iniated = true;

    ShowWindow(hwnd_window_00, SW_SHOW);
    UpdateWindow(hwnd_window_00);
    SetForegroundWindow(hwnd_window_00);

    phoenix_engine_info("boot", "window, plugins, vulkan and editor are up.");
    return true;
}


bool run_program(void)
{
    phoenix_engine_trace("program", "%s", "tracing the program 03: starting the run program");
    phoenix_engine_info("running program", "%s", "main entrance into program live");

    start_clock(&main_clock);
    update_clock(&main_clock);

    program_started = true;     /* from now on the window procedure forwards messages to the editor */

    while (program_state.is_running)
    {
        if (!pump_system_messages(&program_state))
        {
            program_state.is_running = false;
        }

        if (!program_state.is_suspended)
        {
            const double t_frame_start = get_time_ms();
            update_clock(&main_clock);
            const float delta_time = (float)main_clock.delta_time;

            start_engine_input(&engine_context, delta_time);   /* wasd fly camera */
            update_engine(&engine_context, delta_time);

            g_editor.update(delta_time);   /* wheel zoom, grid hotkeys, tool cursor */

            if (start_game_state_instance.update) {
                start_game_state_instance.update(&start_game_state_instance, delta_time);
            }

            const double t_draw_start = get_time_ms();
            render_packet packet = {delta_time};
            draw_frontend_render_frame(&packet);
            if (program_state.render_resume_pending) {
                program_state.render_resume_pending = false;
            }

            const double t_end = get_time_ms();
            if (t_end - t_frame_start > 33.0) {
                phoenix_engine_warn("perf", "slow frame %.1f ms (update %.1f ms, draw %.1f ms)",
                             t_end - t_frame_start, t_draw_start - t_frame_start, t_end - t_draw_start);
            }
        }
        Sleep(1);
    }
    program_state.is_running = false;
    /* end_program is called once by winmain */
    phoenix_engine_trace("program", "%s", "tracing the program 03: ending the run program");
    return true;
}

/* unwinds exactly the stages start_program finished, newest first.
   calling it twice does nothing: the stage is none after the first call. */
void end_program(void)
{
    if (program_stage == program_boot_stage_none) return;

    phoenix_engine_trace("program", "tracing the program 04: starting the shutdown program");

    program_state.is_running = false;
    program_started = false;    /* the window procedure stops calling into the editor */

    /* last chance: camera, view buttons, tool, everything in phoenix_editor.pdb. only when the editor really ran,
       so a failed boot never overwrites good prefs with defaults. */
    if (program_stage >= program_boot_stage_editor && g_editor.save_settings) {
        g_editor.save_settings();
    }

    /* the game frees its own state: only it knows the size. this must run before unload_subsystem_libraries
       because these pointers point into the game dll's code. */
    if (start_game_state_instance.shutdown) {
        start_game_state_instance.shutdown(&start_game_state_instance);
    }
    start_game_state_instance = (game_state){0};

    if (program_stage >= program_boot_stage_frame_jobs) {
        shutdown_frame_jobs();   /* drains in flight frames, then joins the job threads (editor tasks included) */
    }

    if (program_stage >= program_boot_stage_render) {
        if (render_context.logical_device) vkDeviceWaitIdle(render_context.logical_device);
        shutdown_render_frontend();   /* the backend calls the editor's offscreen shutdown, so the editor dll must still be loaded */
    }

    if (program_stage >= program_boot_stage_editor && g_editor.shutdown) {
        g_editor.shutdown();   /* object database, dock tree */
    }

    if (program_stage >= program_boot_stage_engine) {
        shutdown_engine(&engine_context);
    }

    if (program_stage >= program_boot_stage_editor) {
        shutdown_clock(&main_clock);
    }

    if (program_stage >= program_boot_stage_libraries) {
        unload_subsystem_libraries();
    }

    if (program_stage >= program_boot_stage_window) {
        destroy_program_window();   /* after vulkan released the surface */
    }

    timeEndPeriod(1);
    program_iniated = false;
    program_stage = program_boot_stage_none;
    phoenix_engine_info("exit", "phoenix architecture closed safely.");
    phoenix_engine_trace("program", "tracing the program 04: ending the shutdown program");
}
