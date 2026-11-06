#include "system_globals.h"

system_scene_registry g_scene_registry = {0};

void start_scene_registry(void)
{
    if (g_scene_registry.object_count > 0) return;

    memset(&g_scene_registry, 0, sizeof(g_scene_registry));

    system_scene_object* root = &g_scene_registry.objects[0];
    root->id = 1000;
    strncpy(root->name, "World Space Root", sizeof(root->name) - 1);
    root->is_open = true;
    root->type = scene_object_empty;
    root->scale = (editor_vec3){1.0f, 1.0f, 1.0f};

    g_scene_registry.object_count      = 1;
    g_scene_registry.selected_object_id = 1000;
    g_scene_registry.next_object_id    = 2000;
}

system_scene_object* spawn_new_scene_object(const char* name, editor_vec3 obj_vert_pos)
{
    if (g_scene_registry.object_count == 0) {
        start_scene_registry();
    }

    /* FIXED: was hardcoded 512 while the array holds MAX_SCENE_OBJECTS (256) -> overflow */
    if (g_scene_registry.object_count >= max_scene_objects) { phoenix_error("Scene", "spawn_new_scene_object('%s'): registry full (%d objects)", name ? name : "Object", (int)max_scene_objects); return NULL; }

    system_scene_object* root_node = &g_scene_registry.objects[0];
    system_scene_object* new_node  = &g_scene_registry.objects[g_scene_registry.object_count];

    memset(new_node, 0, sizeof(*new_node));
    new_node->id = g_scene_registry.next_object_id++;      /* ids assigned in ONE place only */
    strncpy(new_node->name, name ? name : "Object", sizeof(new_node->name) - 1);
    new_node->is_open  = true;
    new_node->type     = scene_object_empty;
    new_node->position = obj_vert_pos;
    new_node->rotation = (editor_vec3){0.0f, 0.0f, 0.0f};
    new_node->scale    = (editor_vec3){1.0f, 1.0f, 1.0f};
    new_node->parent   = root_node;

    if (root_node->first_child == NULL) {
        root_node->first_child = new_node;
    } else {
        system_scene_object* curr = root_node->first_child;
        while (curr->next_sibling != NULL) curr = curr->next_sibling;
        curr->next_sibling = new_node;
    }

    g_scene_registry.object_count++;
    g_scene_registry.selected_object_id = new_node->id;

    phoenix_info("Scene", "spawned '%s' id %u at (%.1f,%.1f,%.1f), object face_type %u", new_node->name, (unsigned)new_node->id, obj_vert_pos.x, obj_vert_pos.y, obj_vert_pos.z, (unsigned)g_scene_registry.object_count);
    return new_node;
}

system_scene_object* find_scene_object_by_id(uint32_t id)
{
    for (uint32_t i = 0; i < g_scene_registry.object_count; i++) {
        if (g_scene_registry.objects[i].id == id) return &g_scene_registry.objects[i];
    }
    return NULL;
}