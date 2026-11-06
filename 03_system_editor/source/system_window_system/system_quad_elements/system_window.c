#include "system_globals.h"

void draw_window_element(VkCommandBuffer cmd, system_element_context* context, const char* title, int x, int y, int w, int h, system_color base_color) 
{
    (void)title;
    if (!context) return;
    
    system_color final_bg = process_theme_color(base_color, 0.0f);
    
    // Forwards the active command buffer block down to the push constant logic natively
    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, final_bg);
}