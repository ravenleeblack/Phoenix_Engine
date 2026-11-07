#include "system_globals.h"


void draw_inspector_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Inspector", node->x, content_y, node->w, content_h, system_theme.tab_content_inspector);

    const int x = node->x + 10;
    int y = content_y + 34;

    draw_text_element(cmd, ui_ctx, "Inspector Window Active", x, y, system_theme.system_text);
}