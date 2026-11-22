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
            g_sky.volume_type[kept] = g_sky.volume_type[i];
            g_sky.volume_density[kept] = g_sky.volume_density[i];
            g_sky.volume_softness[kept] = g_sky.volume_softness[i];
            g_sky.volume_noise_scale[kept] = g_sky.volume_noise_scale[i];
            for (int k = 0; k < 3; k++) {
                g_sky.volume_color[kept][k] = g_sky.volume_color[i][k];
            }
        }
        kept++;
    }
    g_sky.volume_count = kept;
}

/* a box of fog: position = center, scale = full size in meters (resize it with the Scale tool). the type decides how it is filled */
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
    obj->scale = (editor_vec3){ 40.0f, 16.0f, 40.0f };

    const int slot = g_sky.volume_count;
    g_sky.volume_object_ids[slot] = obj->id;
    g_sky.volume_type[slot] = fog_volume_good;
    g_sky.volume_density[slot] = 0.05f;
    g_sky.volume_softness[slot] = 0.45f;
    g_sky.volume_noise_scale[slot] = 4.0f;
    g_sky.volume_color[slot][0] = 0.85f;
    g_sky.volume_color[slot][1] = 0.88f;
    g_sky.volume_color[slot][2] = 0.92f;
    g_sky.volume_count++;
    g_sky.scene_fog_on = true;
    g_sky.game_fog_on = true;
    set_statusbar_message("Fog volume created and filled (good fog). Resize the box with the Scale tool. Type: Preferences > Sky / Fog.");
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

/* three rows that turn a view space point into the box's own space, where the box is -1..1 on every axis:
   local = S^-1 * Rm^T * (Rv^T * (p - tv) - tm). rows[r] = { A[r][0], A[r][1], A[r][2], b[r] } */
void get_volume_local_rows(const editor_mat4* view, const system_scene_object* obj, float rows[3][4])
{
    const editor_mat4 model = system_math_model_matrix(obj->position, obj->rotation, (editor_vec3){ 1.0f, 1.0f, 1.0f });
    const float scale[3] = { fabsf(obj->scale.x) * 0.5f, fabsf(obj->scale.y) * 0.5f, fabsf(obj->scale.z) * 0.5f };

    float rv[3][3];
    float rm[3][3];
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            rv[r][c] = view->mesh[c][r];
            rm[r][c] = model.mesh[c][r];
        }
    }
    const float tv[3] = { view->mesh[3][0], view->mesh[3][1], view->mesh[3][2] };
    const float tm[3] = { obj->position.x, obj->position.y, obj->position.z };

    /* world = Rv^T * (p - tv)  ->  world = Rv^T * p - Rv^T * tv */
    float a_world[3][3];
    float b_world[3];
    for (int r = 0; r < 3; r++) {
        b_world[r] = 0.0f;
        for (int c = 0; c < 3; c++) {
            a_world[r][c] = rv[c][r];
            b_world[r] -= rv[c][r] * tv[c];
        }
    }
    /* local = S^-1 * Rm^T * (world - tm) */
    for (int r = 0; r < 3; r++) {
        float inverse_scale = 1.0f;
        if (scale[r] > 1e-4f) {
            inverse_scale = 1.0f / scale[r];
        }
        rows[r][3] = 0.0f;
        for (int c = 0; c < 3; c++) {
            float sum = 0.0f;
            for (int k = 0; k < 3; k++) {
                sum += rm[k][r] * a_world[k][c];
            }
            rows[r][c] = sum * inverse_scale;
        }
        float offset = 0.0f;
        for (int k = 0; k < 3; k++) {
            offset += rm[k][r] * (b_world[k] - tm[k]);
        }
        rows[r][3] = offset * inverse_scale;
    }
}

/* appends the sky and fog data to the light block (floats from light_gpu_block_floats on). see light_block in mesh_frag.glsl */
void fill_sky_fog_block(float* block, const editor_mat4* view)
{
    float* fog = block;
    float camera_y = -(view->mesh[1][0] * view->mesh[3][0] + view->mesh[1][1] * view->mesh[3][1] + view->mesh[1][2] * view->mesh[3][2]);

    fog[0] = get_sky_linear_color(g_sky.fog_color[0]);
    fog[1] = get_sky_linear_color(g_sky.fog_color[1]);
    fog[2] = get_sky_linear_color(g_sky.fog_color[2]);
    fog[3] = 0.0f;
    if (g_sky.draw_fog_now) {
        fog[3] += 1.0f;
    }
    if (g_sky.draw_sky_now) {
        fog[3] += 2.0f;                               /* the shader also blends far surfaces into the sky's horizon when the sky is on */
    }
    fog[4] = g_sky.fog_density;
    fog[5] = g_sky.fog_height_falloff;
    fog[6] = g_sky.fog_base_height;
    fog[7] = g_sky.fog_start;
    fog[8] = camera_y;
    fog[9] = g_sky.fog_max_opacity;
    fog[10] = g_sky.fog_sun_scatter;
    fog[11] = g_sky.fog_sun_exponent;
    fog[12] = g_sky.sky_brightness;
    fog[13] = g_sky.sky_haze;
    fog[14] = g_sky.fog_sky_color ? 1.0f : 0.0f;
    fog[15] = g_sky.draw_time;

    /* the sun: direction toward it in view space and its color, so the sky and the fog can follow it */
    {
        float sun_light[4];
        float sun_color[4];
        scene_look_get_light_view(view, sun_light);
        get_light_sun_color(sun_light[3], sun_color);
        const bool sun_on = (sun_light[0] * sun_light[0] + sun_light[1] * sun_light[1] + sun_light[2] * sun_light[2]) > 1e-6f;
        fog[16] = sun_light[0]; fog[17] = sun_light[1]; fog[18] = sun_light[2];
        fog[19] = sun_on ? 1.0f : 0.0f;
        fog[20] = sun_color[0]; fog[21] = sun_color[1]; fog[22] = sun_color[2];
        fog[23] = sun_light[3];                      /* the scene's ambient: the fog is lit by it too */

        float sun_tint[4];
        get_light_sun_tint(sun_tint);
        fog[24] = sun_tint[0]; fog[25] = sun_tint[1]; fog[26] = sun_tint[2];
        fog[27] = sun_tint[3];                       /* intensity */
    }

    prune_sky_volumes();
    for (int i = 0; i < sky_max_volumes; i++) {
        float* color = &fog[28 + i * 4];
        float* params = &fog[44 + i * 4];
        float* rows = &fog[60 + i * 12];
        for (int k = 0; k < 4; k++) {
            color[k] = 0.0f;
            params[k] = 0.0f;
        }
        for (int k = 0; k < 12; k++) {
            rows[k] = 0.0f;
        }
        if (!g_sky.draw_fog_now || i >= g_sky.volume_count) {
            continue;
        }
        const system_scene_object* obj = find_scene_object_by_id(g_sky.volume_object_ids[i]);
        if (!obj) {
            continue;
        }
        float local[3][4];
        get_volume_local_rows(view, obj, local);
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                rows[r * 4 + c] = local[r][c];
            }
        }
        color[0] = get_sky_linear_color(g_sky.volume_color[i][0]);
        color[1] = get_sky_linear_color(g_sky.volume_color[i][1]);
        color[2] = get_sky_linear_color(g_sky.volume_color[i][2]);
        color[3] = g_sky.volume_density[i];
        params[0] = 1.0f;                                    /* this slot is used */
        params[1] = (float)g_sky.volume_type[i];
        params[2] = g_sky.volume_softness[i];
        params[3] = g_sky.volume_noise_scale[i];
    }
}