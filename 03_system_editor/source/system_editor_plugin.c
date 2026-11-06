#include "system_globals.h"
#include "system_dock.h"
#include "system_preferences.h"

#ifdef offscreen_create_renderpass
#undef offscreen_create_renderpass
#endif
#ifdef offscreen_create_resources
#undef offscreen_create_resources
#endif
#ifdef offscreen_shutdown
#undef offscreen_shutdown
#endif

__declspec(dllexport) bool editor_load_settings(void)
{
    prefs_load();
    return true;
}

__declspec(dllexport) void editor_save_settings(void)
{
    prefs_save();
    editor_state_write_to_db();
    settings_db_save();
}

__declspec(dllexport) bool editor_start(void)
{
    start_editor_elements(&element_context);
    start_object_database();
    return true;
}

__declspec(dllexport) void editor_update(float delta_time)
{
    (void)delta_time;
    update_element_input(&element_context,
                         element_context.input.mouse_x,
                         element_context.input.mouse_y,
                         element_context.input.mouse_down);
    system_wheel_hook_update();
}

__declspec(dllexport) VkCommandBuffer editor_begin_frame_pass(void)
{
    return begin_frontend_main_pass();
}

__declspec(dllexport) void editor_draw_ui(VkCommandBuffer cmd, float delta_time)
{
    (void)delta_time;
    draw_system_menubar(cmd, &element_context, &render_context.swapchain_main);
    draw_system_toolbar(cmd, &element_context, &render_context.swapchain_main);
    start_dock_system(cmd, &dock_workspace, &element_context);
    draw_system_statusbar(cmd, &element_context, &render_context.swapchain_main, is_game_running);
    render_element_commands(cmd,
                            render_context.swapchain_main.extent,
                            render_context.pipeline_layout);
}

__declspec(dllexport) void editor_end_frame_pass(VkCommandBuffer cmd)
{
    end_frontend_main_pass(cmd);
}

__declspec(dllexport) bool editor_handle_message(HWND hwnd, UINT message, WPARAM wp, LPARAM lp, LRESULT* result)
{
    (void)hwnd;
    (void)wp;
    (void)lp;
    if (result) *result = 0;

    switch (message) {
        case WM_MOUSEMOVE:
            element_context.input.mouse_x = (int)(short)LOWORD(lp);
            element_context.input.mouse_y = (int)(short)HIWORD(lp);
            break;
        case WM_LBUTTONDOWN:
            element_context.input.mouse_x = (int)(short)LOWORD(lp);
            element_context.input.mouse_y = (int)(short)HIWORD(lp);
            element_context.input.mouse_down = true;
            if (result) *result = 0;
            return true;
        case WM_LBUTTONUP:
            element_context.input.mouse_x = (int)(short)LOWORD(lp);
            element_context.input.mouse_y = (int)(short)HIWORD(lp);
            element_context.input.mouse_down = false;
            if (result) *result = 0;
            return true;
        case WM_MBUTTONDOWN:
            element_context.input.middle_down = true;
            if (result) *result = 0;
            return true;
        case WM_MBUTTONUP:
            element_context.input.middle_down = false;
            if (result) *result = 0;
            return true;
        case WM_MOUSEWHEEL:
            system_camera_dolly((float)(short)HIWORD(wp) / (float)WHEEL_DELTA);
            system_grid_zoom((float)(short)HIWORD(wp) / (float)WHEEL_DELTA);
            if (result) *result = 0;
            return true;
    }
    return false;
}

__declspec(dllexport) void editor_shutdown(void)
{
    editor_save_settings();
    shutdown_dock_system(&dock_workspace);
    shutdown_object_database();
}

__declspec(dllexport) bool offscreen_create_renderpass(VkFormat color_format, VkFormat depth_format)
{
    return create_offscreen_renderpass(color_format, depth_format);
}

__declspec(dllexport) bool offscreen_create_resources(void)
{
    return create_offscreen_resources();
}

__declspec(dllexport) void offscreen_shutdown(void)
{
    shutdown_offscreen();
}
