#include "engine_globals.h"

bool validate_rectangle_hit(int mx, int my, int x, int y, int w, int h)
{
    if (mx < x || my < y) return false;
    if (mx >= x + w || my >= y + h) return false;
    return true;
}
