#include "engine_globals.h"


static volatile LONG g_mouse_x, g_mouse_y, g_mouse_down;

/* The dock tree is owned by the UI thread (collapse() frees nodes), so the main thread never walks it.
   The UI thread publishes the cursor shape: 0 = arrow, 1 = size WE, 2 = size NS. */
static volatile LONG g_cursor_kind = 0;

int frame_jobs_get_cursor_kind(void) { return (int)g_cursor_kind; }

void publish_ui_cursor(int mx, int my)
{
    (void)mx;
    (void)my;
    InterlockedExchange(&g_cursor_kind, 0);
}

void frame_jobs_post_mouse(int x, int y, bool pressed)
{
    InterlockedExchange(&g_mouse_x, x);
    InterlockedExchange(&g_mouse_y, y);
    InterlockedExchange(&g_mouse_down, pressed ? 1 : 0);
}

/* UI thread (called by the engine frame chain): input, layout, draw into the open main pass, flush the quad batch. */
void editor_draw_ui(VkCommandBuffer cmd, float delta_time)
{
    (void)delta_time;
    const int  mouse_x    = (int)g_mouse_x;
    const int  mouse_y    = (int)g_mouse_y;
    const bool mouse_down = g_mouse_down != 0;

    (void)mouse_x;
    (void)mouse_y;
    (void)mouse_down;
    (void)cmd;

    publish_ui_cursor(mouse_x, mouse_y);
}

bool start_frame_jobs(void)
{
    return true;
}

void shutdown_frame_jobs(void)
{
}

void submit_frame(float delta_time)
{
    if (!backend) return;
    render_packet packet = {delta_time};
    draw_frontend_render_frame(&packet);
}