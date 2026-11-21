#include "engine_globals.h"

/* see engine_spot_light.h */

void set_spot_light_defaults(light_object* light)
{
    light->kind     = light_kind_spot;
    light->enabled  = true;
    light->color[0] = 1.0f; light->color[1] = 1.0f; light->color[2] = 1.0f;
    light->intensity = 2.0f;
    light->warmth   = 0.0f;
    light->range    = 40.0f;
    light->cone_deg = 35.0f;
    light->softness = 0.3f;
}

int get_spot_light_shader_kind(void)
{
    return 2;
}
