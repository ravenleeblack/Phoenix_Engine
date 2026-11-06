#include "system_globals.h"

/* system_element.c: one instanced quad, the fragment shader (ui_batch_frag.glsl) draws the circle from its distance field */
void push_circle_element(VkCommandBuffer cmd, float cx, float cy, float radius, float thickness, system_color color);

/* filled disc */
void draw_circle_element(VkCommandBuffer cmd, float cx, float cy, float radius, system_color color)
{
    if (radius <= 0.0f) return;
    push_circle_element(cmd, cx, cy, radius, 0.0f, color);
}

/* a ring: outer edge at radius, thickness going inward */
void draw_circle_border(VkCommandBuffer cmd, float cx, float cy, float radius, float thickness, system_color color)
{
    if (radius <= 0.0f || thickness <= 0.0f) return;
    if (thickness > radius) thickness = radius;
    push_circle_element(cmd, cx, cy, radius, thickness, color);
}