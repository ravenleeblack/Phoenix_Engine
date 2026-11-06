#include "system_globals.h"

/* How many pixels to cut from the left/right edge_to_face_array on a given corner row (row 0 = outermost) */
static int corner_inset(int radius, int row)
{
    float dy = (float)radius - (float)row - 0.5f;
    float dx = sqrtf((float)(radius * radius) - dy * dy);
    int inset = (int)ceilf((float)radius - dx - 0.2f);
    return inset < 0 ? 0 : inset;
}

void push_rounded_quad(VkCommandBuffer cmd, float x, float y, float w, float h,
                       system_color c, int radius, int corners)
{
    if (radius <= 0 || corners == 0 || w < (float)(radius * 2) || h < (float)(radius * 2)) {
        push_quad_element(cmd, x, y, w, h, c);
        return;
    }

    /* solid middle band */
    float mid_h = h - (float)(radius * 2);
    if (mid_h > 0.0f) {
        push_quad_element(cmd, x, y + (float)radius, w, mid_h, c);
    }

    /* top and bottom corner rows */
    for (int i = 0; i < radius; i++) {
        float in = (float)corner_inset(radius, i);

        float tl = (corners & corner_tl) ? in : 0.0f;
        float tr = (corners & corner_tr) ? in : 0.0f;
        push_quad_element(cmd, x + tl, y + (float)i, w - tl - tr, 1.0f, c);

        float bl = (corners & corner_bl) ? in : 0.0f;
        float br = (corners & corner_br) ? in : 0.0f;
        push_quad_element(cmd, x + bl, y + h - 1.0f - (float)i, w - bl - br, 1.0f, c);
    }
}