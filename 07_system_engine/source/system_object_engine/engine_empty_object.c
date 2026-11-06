#include "engine_globals.h"

/* ========================================================================= */
/* Empty game object (Unity: GameObject > Create Empty).                      */
/* A scene object with a transform and NO mesh. The transform gizmo works on  */
/* it because system_gizmo.c only needs the obj_vert_selected object's position.       */
/* Use it as a pivot / parent: drag other objects under it in the hierarchy.  */
/* ========================================================================= */

static bool name_taken(const char* n)
{
    for (uint32_t i = 0; i < g_scene_registry.object_count; i++)
        if (strcmp(g_scene_registry.objects[i].name, n) == 0) return true;
    return false;
}

/* "Empty", "Empty (1)", "Empty (2)" ... like Unity's "GameObject (1)" */
static void make_unique_name(const char* base, char* out, size_t out_size)
{
    snprintf(out, out_size, "%s", base);
    for (int n = 1; name_taken(out) && n < 1000; n++)
        snprintf(out, out_size, "%s (%d)", base, n);
}

/* Registers and selects the new object. name may be NULL. Returns it, or NULL when the scene is full.
   Do NOT call spawn_new_scene_object around this. */
system_scene_object* create_system_empty(const char* name, editor_vec3 position)
{
    if (g_scene_registry.object_count >= max_scene_objects) {
        phoenix_warn("Empty", "%s", "Scene registry capacity full. Cannot create an empty object.");
        return NULL;
    }

    char unique[max_object_name];
    make_unique_name((name && name[0]) ? name : "Empty", unique, sizeof(unique));

    system_scene_object* obj = spawn_new_scene_object(unique, position);
    if (!obj) {
        phoenix_error("Empty", "%s", "Failed to allocate the empty object.");
        return NULL;
    }

    /* id is assigned by spawn_new_scene_object and the object is obj_vert_selected there. no mesh is uploaded on purpose. */
    obj->type     = scene_object_empty;
    obj->rotation = (editor_vec3){ 0.0f, 0.0f, 0.0f };
    obj->scale    = (editor_vec3){ 1.0f, 1.0f, 1.0f };

    phoenix_info("Empty", "Empty object '%s' created.", unique);
    return obj;
}