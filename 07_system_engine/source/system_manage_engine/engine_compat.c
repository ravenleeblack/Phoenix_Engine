#include "engine_globals.h"

VkSampleCountFlagBits get_msaa_sample_count(void)
{
    return VK_SAMPLE_COUNT_1_BIT;
}

void em_gpu_retire(uint64_t vertex_buffer, uint64_t vertex_memory,
                    uint64_t index_buffer, uint64_t index_memory)
{
    (void)vertex_buffer;
    (void)vertex_memory;
    (void)index_buffer;
    (void)index_memory;
}

void em_mesh_free(edit_mode_mesh* mesh)
{
    if (!mesh) return;
    free(mesh->verts);
    free(mesh->edges);
    free(mesh->faces);
    memset(mesh, 0, sizeof(*mesh));
}

bool create_system_board_octagon(const editor_vec3* position)
{
    (void)position;
    return false;
}

bool create_system_board_diamond(const editor_vec3* position)
{
    (void)position;
    return false;
}

bool create_system_board_triangle_half(const editor_vec3* position)
{
    (void)position;
    return false;
}

bool create_system_board_triangle_quarter(const editor_vec3* position)
{
    (void)position;
    return false;
}

void light_update_from_scene(void)
{
    update_light_from_scene();
}

int light_object_count(void)
{
    return get_light_object_count();
}

const light_object* light_object_at(int index)
{
    return get_light_object_at(index);
}

const char* light_object_name(int index)
{
    return get_light_object_name(index);
}

void light_object_set_enabled(int index, bool enabled)
{
    set_light_object_enabled(index, enabled);
}

void light_object_select(int index)
{
    select_light_object(index);
}

bool light_sun_is_driven(const char** name_out)
{
    return is_light_sun_driven(name_out);
}

void focus_system_grid_on_selected_object(void)
{
    editor_vec3 target = { 0.0f, 0.0f, 0.0f };
    system_scene_object* selected = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (selected) target = selected->position;

    world_camera* camera = get_editor_camera();
    if (!camera) return;
    camera->position = target;
    camera->position.x += 12.0f;
    camera->position.y += 8.0f;
    camera->position.z += 12.0f;
    camera->rotation.x = -0.45f;
    camera->rotation.y = -0.78539816339f;
}
