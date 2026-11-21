#include "engine_globals.h"

/* see engine_hemisphere_light.h */

void set_hemisphere_light_defaults(light_object* light)
{
    light->kind     = light_kind_hemisphere;
    light->enabled  = true;
    light->color[0] = 0.55f; light->color[1] = 0.70f; light->color[2] = 1.00f;
    light->ground_color[0] = 0.22f; light->ground_color[1] = 0.18f; light->ground_color[2] = 0.14f;
    light->intensity = 0.5f;
    light->warmth   = 0.0f;
}
