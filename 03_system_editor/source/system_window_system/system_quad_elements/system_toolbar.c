#include "system_globals.h"

void draw_toolbar_element(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, system_color color) 
{
    if (!context) return;

    system_color final_color = process_theme_color(color, 0.0f);
    
    // DIRECT DRAW: Pipes the cmd context straight through to record natively into the pipeline
    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, 
                      final_color);
}