#include "system_globals.h"

/* Two plain primitives every icon / icon button in the editor builds from.
   No theme lookups, no input, no scene knowledge. Callers pass the color. */

/* flat filled rect (icon background, or a pixel / bar of a icon_glyph). ignores empty sizes. */
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

/* a lit sphere in a color: a dark disc, then smaller and lighter discs pushed toward the light (upper left), then a highlight.
   the same look as the skin spheres in Texture Shop. */
void draw_icon_sphere(VkCommandBuffer cmd, float cx, float cy, float radius, const float rgb[3])
{
    const float shade[6]  = { 0.30f, 0.50f, 0.72f, 0.92f, 1.10f, 1.40f };
    const float size[6]   = { 1.00f, 0.92f, 0.78f, 0.60f, 0.40f, 0.16f };
    const float offset[6] = { 0.00f, 0.05f, 0.12f, 0.20f, 0.28f, 0.38f };
    for (int k = 0; k < 6; k++) {
        system_color color;
        color.red   = rgb[0] * shade[k];
        color.green = rgb[1] * shade[k];
        color.blue  = rgb[2] * shade[k];
        color.alpha = 1.0f;
        if (color.red   > 1.0f) color.red   = 1.0f;
        if (color.green > 1.0f) color.green = 1.0f;
        if (color.blue  > 1.0f) color.blue  = 1.0f;
        push_circle_element(cmd, cx - radius * offset[k], cy - radius * offset[k], radius * size[k], 0.0f, color);
    }
}

/* a brush icon: a lit sphere with a icon_glyph on it, so every brush looks different.
   icon_glyph 0..8 = sculpt brushes (clay buildup, clay soften, standard, carve, grab, polish, inflate, smooth, flatten)
   icon_glyph 9..17 = paint brushes (paint, airbrush, eraser, smudge, blur, fill, picker, lighten, darken)
   each icon_glyph is 9 x 9 cells, X = lit cell. */
void draw_icon_brush(VkCommandBuffer cmd, float cx, float cy, float radius, const float rgb[3], int icon_glyph)
{
    const char* const glyphs[18][9] = {
        { ".........", ".........", "..XXXXX..", ".........", ".XXXXXXX.", ".........", "XXXXXXXXX", ".........", "........." },
        { ".........", ".........", ".........", ".XX...XX.", "X..X.X..X", "....X....", ".........", ".........", "........." },
        { ".........", "...XXX...", "..X...X..", ".X.....X.", "X.......X", ".........", "XXXXXXXXX", ".........", "........." },
        { ".........", "X.......X", ".X.....X.", "..X...X..", "...X.X...", "....X....", ".........", ".........", "........." },
        { "....X....", "...XXX...", "....X....", "X...X...X", "XXXXXXXXX", "X...X...X", "....X....", "...XXX...", "....X...." },
        { "......X.X", ".....X.X.", "....X.X..", "...X.X...", "..X.X....", ".X.X.....", "X.X......", ".X.......", "X........" },
        { ".........", "XX.....XX", "X.X...X.X", "...X.X...", "....X....", "...X.X...", "X.X...X.X", "XX.....XX", "........." },
        { ".........", ".XX...XX.", "X..X.X..X", "....X....", ".........", ".XX...XX.", "X..X.X..X", "....X....", "........." },
        { "...XXX...", "....X....", "....X....", ".........", "XXXXXXXXX", "XXXXXXXXX", ".........", ".........", "........." },
        { "......XX.", ".....XXX.", "....XXX..", "...XXX...", "..XXX....", ".XXX.....", "XXX......", "XX.......", "........." },
        { "....X....", "..X...X..", ".....X...", "X...X...X", "...X.X...", ".X.....X.", "....X....", "..X...X..", "X.......X" },
        { ".........", ".XXXXXXX.", ".X..X..X.", ".X..X..X.", ".X..X..X.", ".XXXXXXX.", ".........", ".........", "........." },
        { ".........", "....X....", "X....X...", "XXXXXXX..", "X....X...", "....X....", ".........", ".........", "........." },
        { "....X....", "...X.X...", "..X.X.X..", ".X.XXX.X.", "X.XXXXX.X", ".X.XXX.X.", "..X.X.X..", "...X.X...", "....X...." },
        { "....X....", "....X....", "...XXX...", "..XXXXX..", ".XXXXXXX.", ".XXXXXXX.", "..XXXXX..", ".........", "........." },
        { "......XXX", "......XXX", ".....XX..", "....XX...", "...XX....", "..XX.....", ".XX......", "X........", "........." },
        { "....X....", ".X..X..X.", "..XXXXX..", "..XXXXX..", "XXXXXXXXX", "..XXXXX..", "..XXXXX..", ".X..X..X.", "....X...." },
        { "..XXX....", ".XXX.....", "XXX......", "XXX......", "XXX......", "XXX......", ".XXX.....", "..XXX....", "........." }
    };

    draw_icon_sphere(cmd, cx, cy, radius, rgb);
    if (icon_glyph < 0 || icon_glyph >= 18) {
        return;
    }

    int cell = (int)(radius * 1.2f / 9.0f);
    if (cell < 1) {
        cell = 1;
    }
    const int left = (int)(cx + 0.5f) - (9 * cell) / 2;
    const int top  = (int)(cy + 0.5f) - (9 * cell) / 2;
    const system_color shadow = { 0.0f, 0.0f, 0.0f, 0.55f };
    const system_color light  = { 1.0f, 1.0f, 1.0f, 1.0f };

    for (int pass = 0; pass < 2; pass++) {
        const int offset = (pass == 0) ? 1 : 0;
        const system_color color = (pass == 0) ? shadow : light;
        for (int row = 0; row < 9; row++) {
            for (int column = 0; column < 9; column++) {
                if (glyphs[icon_glyph][row][column] == 'X') {
                    draw_icon_element(cmd, left + column * cell + offset, top + row * cell + offset, cell, cell, color);
                }
            }
        }
    }
}