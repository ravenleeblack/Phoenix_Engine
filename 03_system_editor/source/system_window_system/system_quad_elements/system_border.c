#include "system_globals.h"

/* Link down directly to your core quad packer array system */

void draw_left_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int h, int thickness, system_color base_color) {
    if (!context) return;
    system_color final_color = process_theme_color(base_color, 0.0f);
    push_quad_element(cmd, (float)x, (float)y, (float)thickness, (float)h, final_color);
}

void draw_right_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int h, int thickness, system_color base_color) {
    if (!context) return;
    system_color final_color = process_theme_color(base_color, 0.0f);
    push_quad_element(cmd, (float)(x - thickness), (float)y, (float)thickness, (float)h, final_color);
}

void draw_bottom_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int thickness, system_color base_color) {
    if (!context) return;
    system_color final_color = process_theme_color(base_color, 0.0f);
    push_quad_element(cmd, (float)x, (float)(y - thickness), (float)w, (float)thickness, final_color);
}

void draw_top_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int thickness, system_color base_color) 
{
    if (!context) return;
    system_color final_color = process_theme_color(base_color, 0.0f);
    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)thickness, final_color);
}

void draw_parallel_top_and_bottom(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, int thickness, system_color base_color) 
{
    draw_top_border(cmd, context, x, y, w, thickness, base_color);
    draw_bottom_border(cmd, context, x, y + h, w, thickness, base_color);
}

void draw_parallel_left_and_right(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, int thickness, system_color base_color) 
{
    draw_left_border(cmd, context, x, y, h, thickness, base_color);
    draw_right_border(cmd, context, x + w, y, h, thickness, base_color);
}

void draw_full_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, int thickness, system_color base_color)
{
    draw_top_border(cmd, context, x, y, w, thickness, base_color);
    draw_bottom_border(cmd, context, x, y + h, w, thickness, base_color);
    draw_left_border(cmd, context, x, y, h, thickness, base_color);
    draw_right_border(cmd, context, x + w, y, h, thickness, base_color);
}

void draw_tab_border(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, int thickness, system_color base_color) 
{
    draw_top_border(cmd, context, x, y, w, thickness, base_color);
    draw_left_border(cmd, context, x, y, h, thickness, base_color);
    draw_right_border(cmd, context, x + w, y, h, thickness, base_color);
}