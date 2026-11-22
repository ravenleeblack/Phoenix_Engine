#include "system_globals.h"

bool create_editor_scene_object_pipeline(VkPhysicalDevice physical_device, VkDevice device, VkRenderPass renderpass, bool enable_depth);
#include "system_dock.h"
#include "system_preferences.h"
#include "system_scene_offscreen.h"

extern void record_editor_offscreen_frame(VkCommandBuffer cmd);
int get_dock_cursor_kind(const manage_docking* manager, int mx, int my);   /* system_dock.c */
extern bool create_editor_offscreen_resources(void);
extern void shutdown_editor_offscreen(void);

void update_editor_camera_hotkeys(void)
{
    /* hotkeys only while this window has the keyboard: Ctrl+F or Home pressed in another app must not move the camera */
    const bool window_focused = check_window_focus();
    bool frame_down = window_focused && ((GetAsyncKeyState(0x46) & 0x8000) != 0) && ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
    bool home_down = window_focused && ((GetAsyncKeyState(VK_HOME) & 0x8000) != 0);

    if (frame_down && !editor_frame_key_down) {
        focus_system_grid_on_selected_object();
    }
    if (home_down && !editor_home_key_down) {
        world_camera* camera = &engine_context.world_state.editor_scene_camera;
        camera->position = (editor_vec3){0.0f, 15.0f, 35.0f};
        camera->rotation = (editor_vec3){-0.4f, 0.0f, 0.0f};
    }

    editor_frame_key_down = frame_down;
    editor_home_key_down = home_down;
}
__declspec(dllexport) void editor_draw_menubar(VkCommandBuffer cmd);
__declspec(dllexport) void editor_draw_toolbar(VkCommandBuffer cmd);
__declspec(dllexport) void editor_draw_dock(VkCommandBuffer cmd);
__declspec(dllexport) void editor_draw_statusbar(VkCommandBuffer cmd);
__declspec(dllexport) void editor_flush_ui(VkCommandBuffer cmd);

__declspec(dllexport) bool editor_load_settings(void)
{
    prefs_load();
    return true;
}

__declspec(dllexport) void editor_apply_settings(void)
{
    update_system_preference();
}

__declspec(dllexport) void editor_save_settings(void)
{
    prefs_save();
}

__declspec(dllexport) bool editor_start(void)
{
    system_render_context* engine_render_context = get_engine_render_context();
    if (engine_render_context) {
        render_context = *engine_render_context;
        graphics_pipeline_grid = get_engine_grid_pipeline();
        pipeline_layout_grid = get_engine_grid_pipeline_layout();
    }
    system_program* engine_program_state = get_engine_program_state();
    if (engine_program_state) {
        program_state = *engine_program_state;
    }
    start_editor_elements(&element_context);
    start_object_database();

    /* the editor module has its own engine_context copy: a camera still at the origin (never set, or saved there) sits inside the origin marker */
    world_camera* start_camera = &engine_context.world_state.editor_scene_camera;
    if (start_camera->position.x * start_camera->position.x + start_camera->position.y * start_camera->position.y +
        start_camera->position.z * start_camera->position.z < 9.0f) {
        start_camera->position = (editor_vec3){0.0f, 15.0f, 35.0f};
        start_camera->rotation = (editor_vec3){-0.4f, 0.0f, 0.0f};
        phoenix_info("EditorCamera", "camera was at the origin; restored the default start position.");
    }
    return true;
}

__declspec(dllexport) void editor_update(float delta_time)
{
    (void)delta_time;
    update_editor_camera_hotkeys();
    system_render_context* engine_render_context = get_engine_render_context();
    if (engine_render_context) {
        render_context = *engine_render_context;
    }
    system_program* engine_program_state = get_engine_program_state();
    if (engine_program_state) {
        program_state.width = engine_program_state->width;
        program_state.height = engine_program_state->height;
    }
    update_element_input(&element_context,
                         element_context.input.mouse_x,
                         element_context.input.mouse_y,
                         element_context.input.mouse_down);
    system_wheel_hook_update();
    preferences_begin_frame(&element_context);
    export_begin_frame(&element_context);
    update_system_object_picking(&element_context);   /* click an object in the Scene view */
    update_terrain_streaming();                       /* rebuilds the terrain chunks that were edited, sculpted or painted, and loads / frees them around the camera */
    update_system_undo(&element_context);             /* Ctrl+Z / Ctrl+Y and Edit > Undo / Redo */
}

__declspec(dllexport) VkCommandBuffer editor_begin_frame_pass(void)
{
    return begin_frontend_main_pass();
}

__declspec(dllexport) void editor_draw_ui(VkCommandBuffer cmd, float delta_time)
{
    (void)delta_time;
    editor_draw_toolbar(cmd);
    editor_draw_dock(cmd);
    editor_draw_statusbar(cmd);
    editor_draw_menubar(cmd);
    editor_flush_ui(cmd);
    preferences_end_frame(&element_context);
    export_end_frame(&element_context);
}

__declspec(dllexport) void editor_draw_menubar(VkCommandBuffer cmd)
{
    draw_system_menubar(cmd, &element_context, &render_context.swapchain_main);
}

__declspec(dllexport) void editor_draw_toolbar(VkCommandBuffer cmd)
{
    draw_system_toolbar(cmd, &element_context, &render_context.swapchain_main);
}

__declspec(dllexport) void editor_draw_dock(VkCommandBuffer cmd)
{
    start_dock_system(cmd, &dock_workspace, &element_context);
}

__declspec(dllexport) void editor_draw_statusbar(VkCommandBuffer cmd)
{
    draw_system_statusbar(cmd, &element_context, &render_context.swapchain_main, is_game_running);
}

__declspec(dllexport) void editor_flush_ui(VkCommandBuffer cmd)
{
    draw_preferences_overlay(cmd, &element_context, &render_context.swapchain_main);
    draw_tooltip_overlay(cmd, &element_context, (int)render_context.swapchain_main.extent.width, (int)render_context.swapchain_main.extent.height);
    render_element_commands(cmd,
                            render_context.swapchain_main.extent,
                            render_context.pipeline_layout);
    preferences_end_frame(&element_context);
    export_end_frame(&element_context);
}

__declspec(dllexport) void editor_end_frame_pass(VkCommandBuffer cmd)
{
    end_frontend_main_pass(cmd);
}

/* the editor lost the mouse or the keyboard (another window took it, a dialog opened): nothing may stay held down */
void clear_editor_input_state(void)
{
    element_context.input.mouse_down = false;
    element_context.input.mouse_pressed = false;
    element_context.input.middle_down = false;
    element_context.input.camera_manipulating = false;
    element_context.active_id = 0;
}

__declspec(dllexport) bool editor_handle_message(HWND hwnd, UINT message, WPARAM wp, LPARAM lp, LRESULT* result)
{
    (void)wp;
    (void)lp;
    if (result) *result = 0;

    switch (message) {
        case WM_MOUSEMOVE:
            element_context.input.mouse_x = (int)(short)LOWORD(lp);
            element_context.input.mouse_y = (int)(short)HIWORD(lp);
            break;
        case WM_LBUTTONDOWN: {
            element_context.input.mouse_x = (int)(short)LOWORD(lp);
            element_context.input.mouse_y = (int)(short)HIWORD(lp);
            element_context.input.mouse_down = true;
            element_context.input.mouse_pressed = true;

            /* the panel that was clicked keeps the keyboard until another one is clicked */
            const struct_viewport_bounds* view = &element_context.scene_view;
            scene_view_has_focus = view->width > 0 && view->height > 0 &&
                validate_rectangle_hit(element_context.input.mouse_x, element_context.input.mouse_y,
                                       view->offset_x, view->offset_y, view->width, view->height);

            SetCapture(hwnd);                                  /* the button up is delivered here even when the cursor left the window */
            phoenix_trace("EditorInput", "left button down at (%d,%d) scene_focus=%d", element_context.input.mouse_x, element_context.input.mouse_y, (int)scene_view_has_focus);
            if (result) *result = 0;
            return true;
        }
        case WM_LBUTTONUP:
            element_context.input.mouse_x = (int)(short)LOWORD(lp);
            element_context.input.mouse_y = (int)(short)HIWORD(lp);
            element_context.input.mouse_down = false;
            if (GetCapture() == hwnd) {
                ReleaseCapture();
            }
            if (result) *result = 0;
            return true;
        case WM_RBUTTONDOWN:
            element_context.input.mouse_x = (int)(short)LOWORD(lp);
            element_context.input.mouse_y = (int)(short)HIWORD(lp);
            context_menu_notify_right_button(element_context.input.mouse_x, element_context.input.mouse_y);
            if (result) *result = 0;
            return true;
        case WM_SETCURSOR: {
            /* windows asks before every mouse move which cursor to show: a resize arrow over a dock divider, otherwise the normal one */
            if (LOWORD(lp) == HTCLIENT) {
                POINT cursor_point;
                if (GetCursorPos(&cursor_point) && ScreenToClient(hwnd, &cursor_point)) {
                    const int cursor_kind = get_dock_cursor_kind(&dock_workspace, cursor_point.x, cursor_point.y);
                    if (cursor_kind == 1) {
                        SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                        if (result) *result = TRUE;
                        return true;
                    }
                    if (cursor_kind == 2) {
                        SetCursor(LoadCursor(NULL, IDC_SIZENS));
                        if (result) *result = TRUE;
                        return true;
                    }
                }
            }
            return false;
        }
        case WM_CAPTURECHANGED:
            if (lp != 0 && (HWND)lp != hwnd) {
                clear_editor_input_state();                    /* another window took the mouse: the button up will not come to us */
            }
            return false;
        case WM_KILLFOCUS:
            clear_editor_input_state();
            return false;
        case WM_ACTIVATEAPP:
            if (wp == FALSE) {
                clear_editor_input_state();                    /* alt tab away: nothing stays held. the scene view keeps its keyboard focus for the way back */
            }
            return false;
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

__declspec(dllexport) bool offscreen_create_renderpass(VkDevice device, VkFormat color_format, VkFormat depth_format)
{
    render_context.logical_device = device;
    /* the sample count is decided from the physical device. Without it get_msaa_sample_count() answers 1 here and 4 later, and the pipelines no longer match this pass. */
    system_render_context* engine_render_context = get_engine_render_context();
    if (engine_render_context) {
        render_context.physical_device = engine_render_context->physical_device;
    }
    return create_editor_offscreen_renderpass(device, color_format, depth_format);
}

__declspec(dllexport) VkRenderPass offscreen_get_renderpass(void)
{
    return editor_get_offscreen_renderpass();
}

__declspec(dllexport) bool offscreen_create_resources(void)
{
    system_render_context* engine_render_context = get_engine_render_context();
    if (engine_render_context) {
        render_context.logical_device = engine_render_context->logical_device;
        render_context.allocator = engine_render_context->allocator;
        render_context.offscreen_renderpass = engine_render_context->offscreen_renderpass;
        render_context.descriptor_layout = engine_render_context->descriptor_layout;
    }
    return create_editor_offscreen_resources();
}

__declspec(dllexport) void offscreen_shutdown(void)
{
    shutdown_editor_offscreen();
}

__declspec(dllexport) bool create_scene_object_pipeline(VkPhysicalDevice physical_device, VkDevice device, VkRenderPass renderpass, bool enable_depth)
{
    render_context.physical_device = physical_device;
    render_context.logical_device = device;
    render_context.offscreen_renderpass = renderpass;
    return create_editor_scene_object_pipeline(physical_device, device, renderpass, enable_depth);
}

__declspec(dllexport) void offscreen_record_frame(VkCommandBuffer cmd)
{
    record_editor_offscreen_frame(cmd);
}
