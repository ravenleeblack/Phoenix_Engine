#include "system_globals.h"

extern char statusbar_text_buffer[256];
void draw_game_sub_menu(system_element_context* ui_element, system_dock_node* node, VkCommandBuffer cmd);

void draw_gameview_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer current_cmd)
{
    draw_game_sub_menu(ui_ctx, node, current_cmd);
    draw_window_element(current_cmd, ui_ctx, "File Archive", node->x, content_y, node->w, content_h, system_theme.tab_content_window_gameview_00); 
    
    ui_ctx->game_view.width    = node->w;
    ui_ctx->game_view.height   = content_h;
    ui_ctx->game_view.offset_x = node->x;
    ui_ctx->game_view.offset_y = content_y;

    VkDescriptorSet game_texture = render_context.game_viewport_target.descriptor_set;
    draw_gameview_viewport_00(ui_ctx, current_cmd, node->x, content_y, node->w, content_h, game_texture); 
}