#include "system_globals.h"
#include "system_object_database.h"

/* sky dome, fog and fog volumes. see system_sky.h. */

float get_sky_linear_color(float c)
{
    if (!g_swapchain_is_srgb) {
        return c;
    }
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return powf((c + 0.055f) / 1.055f, 2.4f);
}

bool is_sky_dome_object(const system_scene_object* obj)
{
    return obj != NULL && g_sky.dome_object_id != 0 && obj->id == g_sky.dome_object_id;
}

/* an octagon dome of radius 1: 4 rings of 8 vertices (below the horizon, horizon, 30, 60 degrees) and the top point */
system_scene_object* create_system_sky_dome(VkDevice device, VkPhysicalDevice physical_device)
{
    system_scene_object* existing = find_scene_object_by_id(g_sky.dome_object_id);
    if (existing) {
        g_scene_registry.selected_object_id = existing->id;
        set_statusbar_message("Sky dome already exists: selected it.");
        return existing;
    }
    if (g_scene_registry.object_count >= max_scene_objects) {
        set_statusbar_message("Scene is full: cannot create the sky dome.");
        return NULL;
    }

    const int ring_count = 4;
    const int side_count = 8;
    const float ring_degrees[4] = { -15.0f, 0.0f, 30.0f, 60.0f };
    const uint32_t vertex_count = (uint32_t)(ring_count * side_count + 1);
    const uint32_t index_count = (uint32_t)((ring_count - 1) * side_count * 6 + side_count * 3);

    editor_vertex vertices[33];
    uint32_t indices[168];
    memset(vertices, 0, sizeof(vertices));

    for (int ring = 0; ring < ring_count; ring++) {
        const float elevation = ring_degrees[ring] * 0.01745329252f;
        for (int side = 0; side < side_count; side++) {
            const float turn = ((float)side / (float)side_count) * 6.28318530718f;
            editor_vertex* v = &vertices[ring * side_count + side];
            v->x = cosf(elevation) * cosf(turn);
            v->y = sinf(elevation);
            v->z = cosf(elevation) * sinf(turn);
            v->r = 1.0f;
            v->g = 1.0f;
            v->b = 1.0f;
            v->a = 1.0f;
            set_vertex_attributes(v, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        }
    }
    editor_vertex* apex = &vertices[ring_count * side_count];
    apex->y = 1.0f;
    apex->r = 1.0f;
    apex->g = 1.0f;
    apex->b = 1.0f;
    apex->a = 1.0f;
    set_vertex_attributes(apex, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    uint32_t used = 0;
    for (int ring = 0; ring < ring_count - 1; ring++) {
        for (int side = 0; side < side_count; side++) {
            const uint32_t a = (uint32_t)(ring * side_count + side);
            const uint32_t b = (uint32_t)(ring * side_count + (side + 1) % side_count);
            const uint32_t c = (uint32_t)((ring + 1) * side_count + side);
            const uint32_t d = (uint32_t)((ring + 1) * side_count + (side + 1) % side_count);
            indices[used++] = a; indices[used++] = b; indices[used++] = c;
            indices[used++] = b; indices[used++] = d; indices[used++] = c;
        }
    }
    for (int side = 0; side < side_count; side++) {
        indices[used++] = (uint32_t)((ring_count - 1) * side_count + side);
        indices[used++] = (uint32_t)((ring_count - 1) * side_count + (side + 1) % side_count);
        indices[used++] = vertex_count - 1;
    }

    system_scene_object* obj = spawn_new_scene_object("Sky Dome", (editor_vec3){ 0.0f, 0.0f, 0.0f });
    if (!obj) {
        return NULL;
    }
    obj->type  = scene_object_mesh;
    obj->scale = (editor_vec3){ 1.0f, 1.0f, 1.0f };

    set_object_mesh_data(obj->id, vertices, (uint32_t)sizeof(editor_vertex), vertex_count, indices, index_count);
    em_registry_adopt_triangles(obj->id, vertices, vertex_count, indices, index_count);
    if (!upload_scene_object_mesh(obj, device, physical_device, vertices, vertex_count, indices, index_count)) {
        set_statusbar_message("Sky dome: GPU upload failed.");
        return NULL;
    }

    g_sky.dome_object_id = obj->id;
    g_sky.scene_sky_on = true;
    g_sky.game_sky_on = true;
    set_statusbar_message("Sky dome created (octagon dome). Colors: Preferences > Sky / Fog.");
    return obj;
}

/* drops volumes whose object was deleted */
void prune_sky_volumes(void)
{
    int kept = 0;
    for (int i = 0; i < g_sky.volume_count; i++) {
        if (find_scene_object_by_id(g_sky.volume_object_ids[i]) == NULL) {
            continue;
        }
        if (kept != i) {
            g_sky.volume_object_ids[kept] = g_sky.volume_object_ids[i];
            g_sky.volume_density[kept] = g_sky.volume_density[i];
            for (int k = 0; k < 3; k++) {
                g_sky.volume_color[kept][k] = g_sky.volume_color[i][k];
            }
        }
        kept++;
    }
    g_sky.volume_count = kept;
}

system_scene_object* create_system_fog_volume(editor_vec3 position)
{
    prune_sky_volumes();
    if (g_sky.volume_count >= sky_max_volumes) {
        set_statusbar_message("Fog volumes: the limit is %d. Delete one first.", (int)sky_max_volumes);
        return NULL;
    }
    system_scene_object* obj = create_system_empty("Fog Volume", position);
    if (!obj) {
        return NULL;
    }
    obj->scale = (editor_vec3){ 20.0f, 20.0f, 20.0f };

    const int slot = g_sky.volume_count;
    g_sky.volume_object_ids[slot] = obj->id;
    g_sky.volume_density[slot] = 0.06f;
    g_sky.volume_color[slot][0] = g_sky.fog_color[0];
    g_sky.volume_color[slot][1] = g_sky.fog_color[1];
    g_sky.volume_color[slot][2] = g_sky.fog_color[2];
    g_sky.volume_count++;
    g_sky.scene_fog_on = true;
    g_sky.game_fog_on = true;
    set_statusbar_message("Fog volume created: position = center, scale = radius (meters). Fog is on.");
    return obj;
}

/* the dome sits on the camera and is 90% of the far clip wide */
void get_sky_dome_matrix(const editor_mat4* view, const system_scene_object* dome, editor_mat4* out_model)
{
    float camera[3];
    for (int c = 0; c < 3; c++) {
        camera[c] = -(view->mesh[c][0] * view->mesh[3][0] + view->mesh[c][1] * view->mesh[3][1] + view->mesh[c][2] * view->mesh[3][2]);
    }
    float radius = g_sky.draw_far * 0.9f;
    if (radius < 10.0f) {
        radius = 10.0f;
    }
    (void)dome;
    *out_model = system_math_model_matrix((editor_vec3){ camera[0], camera[1], camera[2] }, (editor_vec3){ 0.0f, 0.0f, 0.0f },
                                          (editor_vec3){ radius, radius, radius });
}

/* appends the sky and fog data to the light block (floats from light_gpu_block_floats on). see light_block in mesh_frag.glsl */
void fill_sky_fog_block(float* block, const editor_mat4* view)
{
    float* fog = block;
    float camera_y = -(view->mesh[1][0] * view->mesh[3][0] + view->mesh[1][1] * view->mesh[3][1] + view->mesh[1][2] * view->mesh[3][2]);

    fog[0] = get_sky_linear_color(g_sky.fog_color[0]);
    fog[1] = get_sky_linear_color(g_sky.fog_color[1]);
    fog[2] = get_sky_linear_color(g_sky.fog_color[2]);
    fog[3] = g_sky.draw_fog_now ? 1.0f : 0.0f;
    fog[4] = g_sky.fog_density;
    fog[5] = g_sky.fog_height_falloff;
    fog[6] = g_sky.fog_base_height;
    fog[7] = g_sky.fog_start;
    fog[8] = camera_y;
    fog[9] = g_sky.fog_max_opacity;
    fog[10] = g_sky.fog_sun_scatter;
    fog[11] = g_sky.fog_sun_exponent;
    for (int k = 0; k < 3; k++) {
        fog[12 + k] = get_sky_linear_color(g_sky.sky_top[k]);
        fog[16 + k] = get_sky_linear_color(g_sky.sky_horizon[k]);
    }

    /* the sun: direction toward it in view space and its color, so the sky and the fog can glow toward it */
    {
        float sun_light[4];
        float sun_color[4];
        scene_look_get_light_view(view, sun_light);
        get_light_sun_color(sun_light[3], sun_color);
        const bool sun_on = (sun_light[0] * sun_light[0] + sun_light[1] * sun_light[1] + sun_light[2] * sun_light[2]) > 1e-6f;
        fog[20] = sun_light[0]; fog[21] = sun_light[1]; fog[22] = sun_light[2];
        fog[23] = sun_on ? 1.0f : 0.0f;
        fog[24] = sun_color[0]; fog[25] = sun_color[1]; fog[26] = sun_color[2];
        fog[27] = 0.0f;
    }

    prune_sky_volumes();
    for (int i = 0; i < sky_max_volumes; i++) {
        float* center = &fog[28 + i * 4];
        float* color = &fog[44 + i * 4];
        center[0] = 0.0f; center[1] = 0.0f; center[2] = 0.0f; center[3] = 0.0f;
        color[0] = 0.0f; color[1] = 0.0f; color[2] = 0.0f; color[3] = 0.0f;
        if (!g_sky.draw_fog_now || i >= g_sky.volume_count) {
            continue;
        }
        const system_scene_object* obj = find_scene_object_by_id(g_sky.volume_object_ids[i]);
        if (!obj) {
            continue;
        }
        float radius = fabsf(obj->scale.x);
        if (fabsf(obj->scale.y) > radius) {
            radius = fabsf(obj->scale.y);
        }
        if (fabsf(obj->scale.z) > radius) {
            radius = fabsf(obj->scale.z);
        }
        const float world[3] = { obj->position.x, obj->position.y, obj->position.z };
        float view_pos[3];
        convert_to_view_space(view, world, true, view_pos);
        center[0] = view_pos[0]; center[1] = view_pos[1]; center[2] = view_pos[2];
        center[3] = radius;
        color[0] = get_sky_linear_color(g_sky.volume_color[i][0]);
        color[1] = get_sky_linear_color(g_sky.volume_color[i][1]);
        color[2] = get_sky_linear_color(g_sky.volume_color[i][2]);
        color[3] = g_sky.volume_density[i];
    }
}
