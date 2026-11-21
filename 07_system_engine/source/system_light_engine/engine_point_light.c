#include "engine_globals.h"

/* see engine_point_light.h */

void set_point_light_defaults(light_object* light)
{
    light->kind     = light_kind_point;
    light->enabled  = true;
    light->color[0] = 1.0f; light->color[1] = 0.96f; light->color[2] = 0.88f;
    light->intensity = 1.5f;
    light->warmth   = 0.0f;
    light->range    = 30.0f;
}

int get_point_light_shader_kind(void)
{
    return 1;
}
