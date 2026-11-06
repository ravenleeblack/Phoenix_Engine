#include "system_globals.h"

void draw_menubar_element(VkCommandBuffer cmd, system_element_context* context, int x, int y, int w, int h, system_color base_color) 
{
    (void)context;
    // DIRECT DRAW: Pipes the cmd context straight through to record natively into the pipeline
    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, base_color);
}