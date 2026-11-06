#include "system_globals.h"

/* Two plain primitives every icon / icon button in the editor builds from.
   No theme lookups, no input, no scene knowledge. Callers pass the color. */

/* flat filled rect (icon background, or a pixel / bar of a glyph). ignores empty sizes. */
void draw_icon_element(VkCommandBuffer cmd, int x, int y, int w, int h, system_color c)
{
    if (w > 0 && h > 0) push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, c);
}

/* 1px frame drawn INSIDE x, y, w, h. the four edges never overlap, so it is safe with alpha. */
void draw_icon_border(VkCommandBuffer cmd, int x, int y, int w, int h, system_color c)
{
    if (w < 2 || h < 2) { draw_icon_element(cmd, x, y, w, h, c); return; }
    draw_icon_element(cmd, x,         y,         w,     1,     c);   /* top    */
    draw_icon_element(cmd, x,         y + h - 1, w,     1,     c);   /* bottom */
    draw_icon_element(cmd, x,         y + 1,     1,     h - 2, c);   /* left   */
    draw_icon_element(cmd, x + w - 1, y + 1,     1,     h - 2, c);   /* right  */
}