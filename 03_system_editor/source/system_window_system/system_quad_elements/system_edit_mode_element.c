#include "system_globals.h"

/* ========================================================================= */
/* 2D overlay drawing                                                        */
/* ========================================================================= */
void edit_line2d(VkCommandBuffer cmd, float x0, float y0, float x1, float y1, float th, system_color c)
{
    const float dx = x1 - x0, dy = y1 - y0;
    float steps = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
    if (steps > 4000.0f) return;
    steps /= 2.0f;                                           /* a dot every 2 px keeps the quad face_type low */
    if (steps < 1.0f) { push_quad_element(cmd, x0 - th * 0.5f, y0 - th * 0.5f, th, th, c); return; }
    const float sx = dx / steps, sy = dy / steps;
    for (int i = 0; i <= (int)steps; ++i)
        push_quad_element(cmd, x0 + sx * (float)i - th * 0.5f, y0 + sy * (float)i - th * 0.5f, th, th, c);
}

void edit_box2d(VkCommandBuffer cmd, float x, float y, float r, system_color c)
{
    push_quad_element(cmd, x - r, y - r, r * 2.0f, r * 2.0f, c);
}