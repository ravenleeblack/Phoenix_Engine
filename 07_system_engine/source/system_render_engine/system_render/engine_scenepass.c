#include "engine_globals.h"


void start_system_scenepass(VkCommandBuffer cmd)
{
    if (g_editor.draw_menubar && g_editor.draw_toolbar && g_editor.draw_dock &&
        g_editor.draw_statusbar && g_editor.flush_ui) {
        g_editor.draw_menubar(cmd);
        g_editor.draw_toolbar(cmd);
        g_editor.draw_dock(cmd);
        g_editor.draw_statusbar(cmd);
        g_editor.flush_ui(cmd);
    }
}

void shutdown_system_scenepass(void)
{
}