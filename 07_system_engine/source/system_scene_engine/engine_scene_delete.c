#include "engine_globals.h"

/* Children first, then the object itself, so removal order never leaves a dangling parent. */
void collect_scene_subtree(const system_scene_object* o, uint32_t* ids, int* n, int cap)
{
    for (const system_scene_object* c = o->first_child; c; c = c->next_sibling)
        collect_scene_subtree(c, ids, n, cap);
    if (*n < cap) ids[(*n)++] = o->id;
}

void unlink_scene_object(system_scene_object* o)
{
    system_scene_object* p = o->parent;
    if (p) {
        if (p->first_child == o) {
            p->first_child = o->next_sibling;
        } else {
            for (system_scene_object* s = p->first_child; s; s = s->next_sibling) {
                if (s->next_sibling == o) { s->next_sibling = o->next_sibling; break; }
            }
        }
    }
    o->parent = NULL;
    o->next_sibling = NULL;
}

bool remove_scene_object(uint32_t id)
{
    system_scene_registry* registry = &g_scene_registry;
    system_scene_object* base = registry->objects;
    const uint32_t old_count = registry->object_count;

    uint32_t idx = 0;
    for (uint32_t i = 1; i < old_count; ++i) {          /* 0 = root, never removed */
        if (base[i].id == id) { idx = i; break; }
    }
    if (idx == 0) return false;

    system_scene_object* victim = &base[idx];

    /* leftovers (should be none: children were removed first) must not point at a dead parent */
    for (system_scene_object* c = victim->first_child; c; ) {
        system_scene_object* next = c->next_sibling;
        c->parent = NULL; c->next_sibling = NULL;
        c = next;
    }
    unlink_scene_object(victim);

    /* GPU buffers may still be referenced by a command buffer recorded this frame: retire, don't destroy */

    /* ordered removal: shift everything after it down one slot so hierarchy / camera order is preserved */
    system_scene_object* first_moved = &base[idx + 1];
    system_scene_object* end         = &base[old_count];
    memmove(&base[idx], &base[idx + 1], (size_t)(old_count - idx - 1) * sizeof(system_scene_object));
    memset(&base[old_count - 1], 0, sizeof(system_scene_object));
    registry->object_count = old_count - 1;

    /* every pointer that referred to a moved object now needs to refer to the slot before it */
    for (uint32_t k = 0; k < registry->object_count; ++k) {
        system_scene_object* o = &base[k];
        if (o->parent       >= first_moved && o->parent       < end) o->parent--;
        if (o->first_child  >= first_moved && o->first_child  < end) o->first_child--;
        if (o->next_sibling >= first_moved && o->next_sibling < end) o->next_sibling--;
    }
    return true;
}

bool delete_scene_object(uint32_t id)
{
    system_scene_object* o = find_scene_object_by_id(id);
    if (!o || o == &g_scene_registry.objects[0]) return false;

    if (g_terrain.exists && id == g_terrain.scene_object_id) {
        set_statusbar_message("Terrain can't be deleted here - remove it from the Terrain inspector.");
        return false;
    }

    uint32_t ids[max_scene_objects];
    int n = 0;
    collect_scene_subtree(o, ids, &n, max_scene_objects);

    char name[max_object_name];
    snprintf(name, sizeof(name), "%s", o->name);

    for (int i = 0; i < n; ++i) remove_scene_object(ids[i]);

    g_scene_registry.selected_object_id = 0;      /* nothing obj_vert_selected */
    set_statusbar_message("Deleted '%s'.", name);
    return true;
}

void update_scene_delete_hotkey(system_element_context* ui_element, int x, int y, int w, int h)
{
    if (!ui_element || ui_element->active_mode != mode_object) return;     /* in Edit mode Delete will remove faces later */
    if (!validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, x, y, w, h)) return;

    bool pressed = input_is_key_down(phoenix_key_delete) && input_was_key_up(phoenix_key_delete);
    if (!pressed) return;

    uint32_t id = g_scene_registry.selected_object_id;
    if (id != 0) delete_scene_object(id);
}