#include "system_globals.h"

/* see system_undo.h */

undo_snapshot g_undo_stack[undo_max_steps];
int           g_undo_count = 0;
undo_snapshot g_redo_stack[undo_max_steps];
int           g_redo_count = 0;
undo_snapshot g_undo_settled = { 0, NULL };
bool          g_undo_ready = false;
bool          g_undo_key_z_prev = false;
bool          g_undo_key_y_prev = false;

edit_undo_snapshot g_edit_undo_stack[undo_edit_max_steps];
int                g_edit_undo_count = 0;
edit_undo_snapshot g_edit_redo_stack[undo_edit_max_steps];
int                g_edit_redo_count = 0;
edit_undo_snapshot g_edit_settled;
bool               g_edit_undo_ready = false;

void free_undo_snapshot(undo_snapshot* snapshot)
{
    free(snapshot->objects);
    snapshot->objects = NULL;
    snapshot->count = 0;
}

/* every scene object except the root */
bool capture_undo_snapshot(undo_snapshot* out)
{
    out->count = 0;
    out->objects = NULL;
    if (g_scene_registry.object_count <= 1) {
        return true;
    }
    const uint32_t count = g_scene_registry.object_count - 1;
    undo_object_state* objects = (undo_object_state*)malloc(sizeof(undo_object_state) * count);
    if (!objects) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        const system_scene_object* obj = &g_scene_registry.objects[i + 1];
        objects[i].id = obj->id;
        objects[i].position = obj->position;
        objects[i].rotation = obj->rotation;
        objects[i].scale = obj->scale;
    }
    out->count = count;
    out->objects = objects;
    return true;
}

bool is_undo_same_objects(const undo_snapshot* a, const undo_snapshot* b)
{
    if (a->count != b->count) {
        return false;
    }
    for (uint32_t i = 0; i < a->count; i++) {
        if (a->objects[i].id != b->objects[i].id) {
            return false;
        }
    }
    return true;
}

bool is_undo_same_transforms(const undo_snapshot* a, const undo_snapshot* b)
{
    for (uint32_t i = 0; i < a->count; i++) {
        if (memcmp(&a->objects[i].position, &b->objects[i].position, sizeof(editor_vec3)) != 0 ||
            memcmp(&a->objects[i].rotation, &b->objects[i].rotation, sizeof(editor_vec3)) != 0 ||
            memcmp(&a->objects[i].scale, &b->objects[i].scale, sizeof(editor_vec3)) != 0) {
            return false;
        }
    }
    return true;
}

void clear_undo_stack(undo_snapshot* stack, int* count)
{
    for (int i = 0; i < *count; i++) {
        free_undo_snapshot(&stack[i]);
    }
    *count = 0;
}

/* the oldest step falls off when the stack is full */
void push_undo_snapshot(undo_snapshot* stack, int* count, const undo_snapshot* snapshot)
{
    if (*count >= undo_max_steps) {
        free_undo_snapshot(&stack[0]);
        for (int i = 1; i < *count; i++) {
            stack[i - 1] = stack[i];
        }
        (*count)--;
    }
    stack[*count] = *snapshot;
    (*count)++;
}

void apply_undo_snapshot(const undo_snapshot* snapshot)
{
    for (uint32_t i = 0; i < snapshot->count; i++) {
        system_scene_object* obj = find_scene_object_by_id(snapshot->objects[i].id);
        if (obj) {
            obj->position = snapshot->objects[i].position;
            obj->rotation = snapshot->objects[i].rotation;
            obj->scale = snapshot->objects[i].scale;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* edit / sculpt / paint history                                              */
/* ------------------------------------------------------------------------- */
bool is_edit_undo_mode(void)
{
    return mode_state_is_edit() || mode_state_is_sculpt() || mode_state_is_paint();
}

void free_edit_undo_snapshot(edit_undo_snapshot* snapshot)
{
    free(snapshot->verts);
    free(snapshot->edges);
    free(snapshot->faces);
    memset(snapshot, 0, sizeof(*snapshot));
}

bool capture_edit_undo_snapshot(const system_scene_object* obj, const edit_mode_mesh* mesh, edit_undo_snapshot* out)
{
    memset(out, 0, sizeof(*out));
    out->object_id = obj->id;
    out->vert_count = mesh->vert_count;
    out->edge_count = mesh->edge_count;
    out->face_count = mesh->face_count;
    const size_t vert_bytes = sizeof(edit_mode_vert) * mesh->vert_count;
    const size_t edge_bytes = sizeof(edit_mode_edge) * mesh->edge_count;
    const size_t face_bytes = sizeof(edit_mode_face) * mesh->face_count;
    out->verts = (edit_mode_vert*)malloc(vert_bytes + 1);
    out->edges = (edit_mode_edge*)malloc(edge_bytes + 1);
    out->faces = (edit_mode_face*)malloc(face_bytes + 1);
    if (!out->verts || !out->edges || !out->faces) {
        free_edit_undo_snapshot(out);
        return false;
    }
    memcpy(out->verts, mesh->verts, vert_bytes);
    memcpy(out->edges, mesh->edges, edge_bytes);
    memcpy(out->faces, mesh->faces, face_bytes);
    out->bytes = vert_bytes + edge_bytes + face_bytes;
    return true;
}

/* the live mesh against a snapshot, without copying anything */
bool is_edit_mesh_same(const edit_mode_mesh* mesh, const edit_undo_snapshot* snapshot)
{
    if (mesh->vert_count != snapshot->vert_count || mesh->edge_count != snapshot->edge_count || mesh->face_count != snapshot->face_count) {
        return false;
    }
    return memcmp(mesh->verts, snapshot->verts, sizeof(edit_mode_vert) * mesh->vert_count) == 0 &&
           memcmp(mesh->edges, snapshot->edges, sizeof(edit_mode_edge) * mesh->edge_count) == 0 &&
           memcmp(mesh->faces, snapshot->faces, sizeof(edit_mode_face) * mesh->face_count) == 0;
}

size_t get_edit_stack_bytes(const edit_undo_snapshot* stack, int count)
{
    size_t total = 0;
    for (int i = 0; i < count; i++) {
        total += stack[i].bytes;
    }
    return total;
}

void push_edit_undo_snapshot(edit_undo_snapshot* stack, int* count, const edit_undo_snapshot* snapshot)
{
    /* the oldest steps fall off when there are too many or they take too much memory */
    while (*count > 0 && (*count >= undo_edit_max_steps || get_edit_stack_bytes(stack, *count) + snapshot->bytes > undo_edit_max_bytes)) {
        free_edit_undo_snapshot(&stack[0]);
        for (int i = 1; i < *count; i++) {
            stack[i - 1] = stack[i];
        }
        (*count)--;
    }
    stack[*count] = *snapshot;
    (*count)++;
}

void clear_edit_undo_stack(edit_undo_snapshot* stack, int* count)
{
    for (int i = 0; i < *count; i++) {
        free_edit_undo_snapshot(&stack[i]);
    }
    *count = 0;
}

void clear_edit_undo_all(void)
{
    clear_edit_undo_stack(g_edit_undo_stack, &g_edit_undo_count);
    clear_edit_undo_stack(g_edit_redo_stack, &g_edit_redo_count);
    if (g_edit_undo_ready) {
        free_edit_undo_snapshot(&g_edit_settled);
    }
    g_edit_undo_ready = false;
}

/* puts a snapshot back into the live edit mesh and uploads it */
bool apply_edit_undo_snapshot(const edit_undo_snapshot* snapshot)
{
    system_scene_object* obj = NULL;
    edit_mode_mesh* mesh = NULL;
    if (!get_edit_target(&obj, &mesh) || !obj || !mesh || obj->id != snapshot->object_id) {
        return false;
    }
    if (mesh->vert_cap < snapshot->vert_count) {
        edit_mode_vert* grown = (edit_mode_vert*)realloc(mesh->verts, sizeof(edit_mode_vert) * snapshot->vert_count);
        if (!grown) {
            return false;
        }
        mesh->verts = grown;
        mesh->vert_cap = snapshot->vert_count;
    }
    if (mesh->edge_cap < snapshot->edge_count) {
        edit_mode_edge* grown = (edit_mode_edge*)realloc(mesh->edges, sizeof(edit_mode_edge) * snapshot->edge_count);
        if (!grown) {
            return false;
        }
        mesh->edges = grown;
        mesh->edge_cap = snapshot->edge_count;
    }
    if (mesh->face_cap < snapshot->face_count) {
        edit_mode_face* grown = (edit_mode_face*)realloc(mesh->faces, sizeof(edit_mode_face) * snapshot->face_count);
        if (!grown) {
            return false;
        }
        mesh->faces = grown;
        mesh->face_cap = snapshot->face_count;
    }
    memcpy(mesh->verts, snapshot->verts, sizeof(edit_mode_vert) * snapshot->vert_count);
    memcpy(mesh->edges, snapshot->edges, sizeof(edit_mode_edge) * snapshot->edge_count);
    memcpy(mesh->faces, snapshot->faces, sizeof(edit_mode_face) * snapshot->face_count);
    mesh->vert_count = snapshot->vert_count;
    mesh->edge_count = snapshot->edge_count;
    mesh->face_count = snapshot->face_count;
    em_commit_to_gpu(mesh, obj);
    return true;
}

/* steps one way through the edit history. from = the stack to take a step from, to = the stack the state we leave goes on. */
bool step_edit_undo(edit_undo_snapshot* from, int* from_count, edit_undo_snapshot* to, int* to_count, const char* name)
{
    if (*from_count <= 0 || !g_edit_undo_ready) {
        set_statusbar_message("Nothing to %s.", name);
        return false;
    }
    edit_undo_snapshot target = from[*from_count - 1];
    if (!apply_edit_undo_snapshot(&target)) {
        return false;
    }
    (*from_count)--;
    push_edit_undo_snapshot(to, to_count, &g_edit_settled);   /* the state we leave */
    g_edit_settled = target;                                  /* the restored state is the new settled one */
    set_statusbar_message("%s (%d more).", name, *from_count);
    return true;
}

bool can_system_undo(void)
{
    if (is_edit_undo_mode() && g_edit_undo_ready) {
        return g_edit_undo_count > 0;
    }
    return g_undo_count > 0;
}

bool can_system_redo(void)
{
    if (is_edit_undo_mode() && g_edit_undo_ready) {
        return g_edit_redo_count > 0;
    }
    return g_redo_count > 0;
}

bool perform_system_undo(void)
{
    if (is_edit_undo_mode() && g_edit_undo_ready) {
        return step_edit_undo(g_edit_undo_stack, &g_edit_undo_count, g_edit_redo_stack, &g_edit_redo_count, "Undo");
    }
    if (g_undo_count <= 0 || !g_undo_ready) {
        set_statusbar_message("%s", "Nothing to undo.");
        return false;
    }
    undo_snapshot target = g_undo_stack[g_undo_count - 1];
    g_undo_count--;
    push_undo_snapshot(g_redo_stack, &g_redo_count, &g_undo_settled);   /* the state we leave becomes the redo step */
    apply_undo_snapshot(&target);
    g_undo_settled = target;
    set_statusbar_message("Undo (%d more).", g_undo_count);
    return true;
}

bool perform_system_redo(void)
{
    if (is_edit_undo_mode() && g_edit_undo_ready) {
        return step_edit_undo(g_edit_redo_stack, &g_edit_redo_count, g_edit_undo_stack, &g_edit_undo_count, "Redo");
    }
    if (g_redo_count <= 0 || !g_undo_ready) {
        set_statusbar_message("%s", "Nothing to redo.");
        return false;
    }
    undo_snapshot target = g_redo_stack[g_redo_count - 1];
    g_redo_count--;
    push_undo_snapshot(g_undo_stack, &g_undo_count, &g_undo_settled);
    apply_undo_snapshot(&target);
    g_undo_settled = target;
    set_statusbar_message("Redo (%d more).", g_redo_count);
    return true;
}

void update_system_undo(system_element_context* ui_ctx)
{
    if (!ui_ctx) {
        return;
    }

    /* keys: only while this window has the keyboard and no number box is taking typed text */
    const bool focused = check_window_focus() && !is_number_field_editing();
    const bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool z_down = focused && ctrl && ((GetAsyncKeyState('Z') & 0x8000) != 0);
    const bool y_down = focused && ctrl && ((GetAsyncKeyState('Y') & 0x8000) != 0);
    const bool z_edge = z_down && !g_undo_key_z_prev;
    const bool y_edge = y_down && !g_undo_key_y_prev;
    g_undo_key_z_prev = z_down;
    g_undo_key_y_prev = y_down;

    /* edit / sculpt / paint: the edit mesh of the object being worked on */
    if (!ui_ctx->input.mouse_down && !is_number_field_active()) {
        system_scene_object* edit_obj = NULL;
        edit_mode_mesh* edit_mesh = NULL;
        const bool editing = is_edit_undo_mode() && get_edit_target(&edit_obj, &edit_mesh) && edit_obj && edit_mesh && edit_mesh->verts;
        if (!editing) {
            if (g_edit_undo_ready) {
                clear_edit_undo_all();                 /* left the modes: the history goes with them */
            }
        } else if (!g_edit_undo_ready || g_edit_settled.object_id != edit_obj->id) {
            clear_edit_undo_all();                     /* a new object: start a new history */
            if (capture_edit_undo_snapshot(edit_obj, edit_mesh, &g_edit_settled)) {
                g_edit_undo_ready = true;
            }
        } else if (!is_edit_mesh_same(edit_mesh, &g_edit_settled)) {
            edit_undo_snapshot now_edit;
            if (capture_edit_undo_snapshot(edit_obj, edit_mesh, &now_edit)) {
                push_edit_undo_snapshot(g_edit_undo_stack, &g_edit_undo_count, &g_edit_settled);
                clear_edit_undo_stack(g_edit_redo_stack, &g_edit_redo_count);
                g_edit_settled = now_edit;
            }
        }
    }

    /* what changed is judged while nothing is being dragged or typed */
    if (!ui_ctx->input.mouse_down && !is_number_field_active()) {
        undo_snapshot now;
        if (capture_undo_snapshot(&now)) {
            if (!g_undo_ready) {
                g_undo_settled = now;
                g_undo_ready = true;
            } else if (!is_undo_same_objects(&now, &g_undo_settled)) {
                /* an object was created or deleted: the old steps no longer match the scene */
                clear_undo_stack(g_undo_stack, &g_undo_count);
                clear_undo_stack(g_redo_stack, &g_redo_count);
                free_undo_snapshot(&g_undo_settled);
                g_undo_settled = now;
            } else if (!is_undo_same_transforms(&now, &g_undo_settled)) {
                push_undo_snapshot(g_undo_stack, &g_undo_count, &g_undo_settled);
                clear_undo_stack(g_redo_stack, &g_redo_count);
                g_undo_settled = now;
            } else {
                free_undo_snapshot(&now);
            }
        }
    }

    if (z_edge) {
        if (shift) {
            perform_system_redo();
        } else {
            perform_system_undo();
        }
    } else if (y_edge) {
        perform_system_redo();
    }
}
