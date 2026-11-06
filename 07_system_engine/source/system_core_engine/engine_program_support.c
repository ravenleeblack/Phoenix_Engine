#include "engine_globals.h"

bool start_engine(system_engine_context* context)
{
    if (!context) {
        return false;
    }
    return true;
}

void update_engine(system_engine_context* context, float delta_time)
{
    (void)context;
    (void)delta_time;
}

void shutdown_engine(system_engine_context* context)
{
    (void)context;
}

void start_engine_input(system_engine_context* context, float delta_time)
{
    (void)context;
    (void)delta_time;
}

bool create_program_window(HINSTANCE instance)
{
    monitor_layout layout = {0};
    EnumDisplayMonitors(NULL, NULL, collect_monitor_layout, (LPARAM)&layout);

    RECT window_rect = layout.monitor_01_rect;
    if (!layout.has_primary) {
        window_rect.left = 0;
        window_rect.top = 0;
        window_rect.right = GetSystemMetrics(SM_CXSCREEN);
        window_rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = handle_window_message;
    wc.hInstance = instance;
    wc.lpszClassName = program_window_class_name;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.style = CS_DBLCLKS;

    if (!RegisterClassA(&wc)) {
        phoenix_engine_fatal("window", "win32 window class registration failure (error %lu).", GetLastError());
        return false;
    }

    int window_width = window_rect.right - window_rect.left;
    int window_height = window_rect.bottom - window_rect.top;
    hwnd_window_00 = CreateWindowExA(0, program_window_class_name, program_window_title,
        WS_POPUP, window_rect.left, window_rect.top, window_width, window_height,
        NULL, NULL, instance, NULL);

    if (!hwnd_window_00) {
        phoenix_engine_fatal("window", "window creation failed (error %lu).", GetLastError());
        UnregisterClassA(program_window_class_name, instance);
        return false;
    }

    program_state.width = (int16_t)window_width;
    program_state.height = (int16_t)window_height;
    phoenix_engine_info("window", "window created %d x %d.", window_width, window_height);
    return true;
}

void destroy_program_window(void)
{
    if (hwnd_window_00) {
        DestroyWindow(hwnd_window_00);
        hwnd_window_00 = NULL;
    }
    UnregisterClassA(program_window_class_name, GetModuleHandleA(NULL));
}

bool pump_system_messages(system_program* program)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            if (program) {
                program->is_running = false;
            }
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return true;
}

void start_world_registry(manage_game_world* world)
{
    if (!world) {
        return;
    }
    world->object_count = 0;

    world->editor_scene_camera.position = (editor_vec3){0.0f, 15.0f, 35.0f};
    world->editor_scene_camera.rotation = (editor_vec3){-0.4f, 0.0f, 0.0f};
    world->editor_scene_camera.fov = 1.047f;
    world->editor_scene_camera.near_clip = 0.1f;
    world->editor_scene_camera.far_clip = 1000.0f;
    world->editor_scene_camera.is_orthographic = false;

    system_world_object game_cam = {0};
    game_cam.entity_id = 5001;
    snprintf(game_cam.name, sizeof(game_cam.name), "main_game_camera");
    game_cam.object_type = object_type_camera;
    game_cam.position = (editor_vec3){0.0f, 2.0f, 5.0f};
    game_cam.scale = (editor_vec3){1.0f, 1.0f, 1.0f};
    game_cam.data.camera.fov = 0.785f;
    game_cam.data.camera.near_clip = 0.1f;
    game_cam.data.camera.far_clip = 500.0f;
    game_cam.data.camera.is_orthographic = false;

    world->object_registry[world->object_count++] = game_cam;
    world->active_game_camera_index = 0;
}
