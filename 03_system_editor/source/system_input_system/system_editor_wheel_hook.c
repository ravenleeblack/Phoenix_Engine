#include "engine_globals.h"
#include "system_globals.h"
#include "editor_input.h"

/* why this exists: the mouse wheel cannot be polled with GetAsyncKeyState. it only exists as WM_MOUSEWHEEL,
   so something has to see that message. this subclasses hwnd_window_00 (no edits to the engine's window procedure)
   and turns the accumulated wheel delta into a camera dolly once per frame.
   state lives in the editor globals: g_wheel_old_proc, g_wheel_hooked, g_wheel_unicode, g_wheel_accum, g_wheel_logged. */

LRESULT CALLBACK handle_wheel_hook_message(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_SETCURSOR && LOWORD(lp) == HTCLIENT && brush_cursor_is_on()) {   /* brush ring replaces the arrow */
        SetCursor(0);
        return TRUE;
    }
    if (msg == WM_MOUSEWHEEL) {
        InterlockedExchangeAdd(&g_wheel_accum, (LONG)(short)HIWORD(wp));
    }

    if (g_wheel_unicode) {
        return CallWindowProcW(g_wheel_old_proc, h, msg, wp, lp);
    }
    return CallWindowProcA(g_wheel_old_proc, h, msg, wp, lp);
}

void install_wheel_hook(void)
{
    if (g_wheel_old_proc || !hwnd_window_00) return;

    g_wheel_unicode = IsWindowUnicode(hwnd_window_00) != 0;

    LONG_PTR prev = 0;
    if (g_wheel_unicode) {
        prev = SetWindowLongPtrW(hwnd_window_00, GWLP_WNDPROC, (LONG_PTR)handle_wheel_hook_message);
    } else {
        prev = SetWindowLongPtrA(hwnd_window_00, GWLP_WNDPROC, (LONG_PTR)handle_wheel_hook_message);
    }

    if (!prev) {
        phoenix_engine_error("input", "mouse wheel hook failed to install (error %lu).", (unsigned long)GetLastError());
        return;
    }
    g_wheel_old_proc = (WNDPROC)prev;
    g_wheel_hooked   = hwnd_window_00;
    phoenix_engine_info("input", "mouse wheel hook installed.");
}

bool system_wheel_hook_active(void)
{
    return g_wheel_old_proc != 0;
}

/* free fly dolly: the wheel moves the camera along its view direction.
   same camera and same convention as wasd / alt orbit, so it works while you are flying around.
   no minimum distance, so it can never get stuck. */
void system_camera_dolly(float notches)
{
    if (notches == 0.0f) return;

    const struct_viewport_bounds* b = &element_context.scene_view;
    if (b->width > 0 && b->height > 0 &&
        !validate_rectangle_hit(element_context.input.mouse_x, element_context.input.mouse_y,
                                b->offset_x, b->offset_y, b->width, b->height)) return;

    world_camera* cam = &engine_context.world_state.editor_scene_camera;

    float cp = cosf(cam->rotation.x), sp = sinf(cam->rotation.x);
    float cy = cosf(cam->rotation.y), sy = sinf(cam->rotation.y);
    float fx = sy * cp, fy = sp, fz = -cy * cp;

    /* the step scales with how far you are from the selected object / origin, but never below half a metre */
    float tx = 0.0f, ty = 0.0f, tz = 0.0f;
    const system_scene_object* sel = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (sel && sel != &g_scene_registry.objects[0]) {
        tx = sel->position.x;
        ty = sel->position.y;
        tz = sel->position.z;
    }
    float dx = cam->position.x - tx, dy = cam->position.y - ty, dz = cam->position.z - tz;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);

    float per_notch = dist * 0.15f;
    if (per_notch < 0.5f)   per_notch = 0.5f;
    if (per_notch > 500.0f) per_notch = 500.0f;

    float move = notches * per_notch;               /* + = forward (zoom in), - = back (zoom out) */
    cam->position.x += fx * move;
    cam->position.y += fy * move;
    cam->position.z += fz * move;
}

/* call once per frame from editor_update. the engine used to call this from update_input_system. */
void system_wheel_hook_update(void)
{
    install_wheel_hook();

    /* wheel messages the engine's window procedure forwarded to process_system_mouse_wheel: grid zoom, as before */
    if (!g_wheel_old_proc) return;

    LONG raw = InterlockedExchange(&g_wheel_accum, 0);
    if (raw == 0) return;

    /* insert loop uses the wheel for "how many rows". when it takes the wheel the camera must not dolly. */
    if (consume_edit_ops_wheel((float)raw / 120.0f)) return;

    if (g_wheel_logged < 3) {
        phoenix_engine_info("input", "mouse wheel event received (raw delta %ld).", (long)raw);
        g_wheel_logged++;
    }

    system_camera_dolly((float)raw / 120.0f);
}