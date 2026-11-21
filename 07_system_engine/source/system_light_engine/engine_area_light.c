#include "engine_globals.h"

/* see engine_area_light.h */

void set_area_light_defaults(light_object* light)
{
    light->kind     = light_kind_area;
    light->enabled  = true;
    light->color[0] = 1.0f; light->color[1] = 1.0f; light->color[2] = 1.0f;
    light->intensity = 1.2f;
    light->warmth   = 0.0f;
    light->range    = 45.0f;
}

int get_area_light_shader_kind(void)
{
    return 3;
}
