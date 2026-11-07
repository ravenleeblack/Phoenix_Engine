#include "system_globals.h"


void draw_timeline_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Animation Timeline Track", node->x, content_y, node->w, content_h, system_theme.tab_content_inspector);
    draw_text_element(cmd, ui_ctx, "[00f ----- 30f ----- 60f ----- 90f ----- 120f]", node->x + 15, content_y + 12, system_theme.system_color_edit_mode_preview_color);
}