#include "engine_globals.h"


void start_system_scenepass(VkCommandBuffer cmd, system_render_context* context, system_element_context* system_ui, float delta_time)
{
    (void)context;
    (void)system_ui;
    if (g_editor.draw_ui) {
        g_editor.draw_ui(cmd, delta_time);
    }
}

void shutdown_system_scenepass(void)
{
}