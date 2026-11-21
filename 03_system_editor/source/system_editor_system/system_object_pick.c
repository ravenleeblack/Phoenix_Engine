#include "system_globals.h"

/* ========================================================================= */
/* click an object in the Scene view: it becomes the selected object, and the  */
/* Scene Archive highlights its row (its parents are opened so the row shows). */
/* meshes are tested against their real triangles, front most wins. objects    */
/* without mesh data (lights, cameras, empties) by a small circle at their     */
/* origin. the terrain is tested with a ray against its height map: the click  */
/* selects the region under it and remembers the chunk, so Ctrl+F can frame it. */
/* a click on nothing deselects.                                               */
/* ========================================================================= */
bool     g_pick_prev_down = false;
int      g_pick_chunk_x = -1;
int      g_pick_chunk_z = -1;
uint32_t g_pick_chunk_region_object_id = 0;

enum { pick_max_vertices = 600000 };

bool system_transform_gizmo_wants_mouse(void);   /* system_transform_gizmo.c: a gizmo handle is under the mouse or being dragged */

/* ------------------------------------------------------------------------- */
/* meshes                                                                     */
/* ------------------------------------------------------------------------- */
/* the depth of the front most triangle of this object under the cursor, or a negative number when nothing is hit */
float get_object_pick_depth(const system_element_context* ui_ctx, const system_scene_object* obj, float mx, float my)
{
    const void* vertices = NULL;
    uint32_t vertex_size = 0;
    uint32_t vertex_count = 0;
    const uint32_t* indices = NULL;
    uint32_t index_count = 0;

    edit_mode_camera_context cam;
    if (!begin_edit_camera(&cam, ui_ctx, obj)) {
        return -1.0f;
    }

    if (get_object_mesh_data(obj->id, &vertices, &vertex_size, &vertex_count, &indices, &index_count) &&
        vertex_size == sizeof(editor_vertex) && vertex_count > 0 && vertex_count <= pick_max_vertices && index_count >= 3) {
        const editor_vertex* source = (const editor_vertex*)vertices;
        cam_screen* screen = (cam_screen*)malloc(sizeof(cam_screen) * vertex_count);
        if (!screen) {
            return -1.0f;
        }
        for (uint32_t i = 0; i < vertex_count; i++) {
            screen[i] = project_edit_point(&cam, (editor_vec3){ source[i].x, source[i].y, source[i].z });
        }
        float best = -1.0f;
        for (uint32_t t = 0; t + 2 < index_count; t += 3) {
            const uint32_t ia = indices[t];
            const uint32_t ib = indices[t + 1];
            const uint32_t ic = indices[t + 2];
            if (ia >= vertex_count || ib >= vertex_count || ic >= vertex_count) {
                continue;
            }
            if (!screen[ia].ok || !screen[ib].ok || !screen[ic].ok) {
                continue;
            }
            if (system_math_is_point_in_triangle(mx, my, screen[ia], screen[ib], screen[ic])) {
                const float depth = (screen[ia].w + screen[ib].w + screen[ic].w) / 3.0f;
                if (best < 0.0f || depth < best) {
                    best = depth;
                }
            }
        }
        free(screen);
        return best;
    }

    /* no mesh data (lights, cameras, empties, boards ...): a circle around the origin that is as big as the object's bounds on screen */
    const cam_screen centre = project_edit_point(&cam, (editor_vec3){ 0.0f, 0.0f, 0.0f });
    if (!centre.ok) {
        return -1.0f;
    }
    float world_radius = obj->bounds_radius;
    float largest_scale = fabsf(obj->scale.x);
    if (fabsf(obj->scale.y) > largest_scale) {
        largest_scale = fabsf(obj->scale.y);
    }
    if (fabsf(obj->scale.z) > largest_scale) {
        largest_scale = fabsf(obj->scale.z);
    }
    world_radius *= largest_scale;
    float fov = engine_context.world_state.editor_scene_camera.fov;
    if (fov <= 0.0f) {
        fov = 1.0471976f;
    }
    float pixel_radius = 16.0f;
    if (world_radius > 0.0f && centre.w > 0.001f) {
        const float scaled = world_radius * ((float)cam.vh * 0.5f / tanf(fov * 0.5f)) / centre.w;
        if (scaled > pixel_radius) {
            pixel_radius = scaled;
        }
    }
    if (hypotf(centre.x - mx, centre.y - my) <= pixel_radius) {
        return centre.w;
    }
    return -1.0f;
}

/* ------------------------------------------------------------------------- */
/* terrain                                                                    */
/* ------------------------------------------------------------------------- */
/* the world ray under the cursor. the sign of right / up is settled by projecting a point of the ray back to the screen, so it
   always agrees with the real Scene view projection. */
bool get_mouse_world_ray(const system_element_context* ui_ctx, float mx, float my, editor_vec3* out_origin, editor_vec3* out_dir)
{
    const struct_viewport_bounds* b = &ui_ctx->scene_view;
    if (b->width <= 0 || b->height <= 0) {
        return false;
    }
    const world_camera* cam = &engine_context.world_state.editor_scene_camera;
    const float pitch = cam->rotation.x;
    const float yaw = cam->rotation.y;
    const editor_vec3 forward = { sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch) };
    editor_vec3 right = { forward.z * -1.0f, 0.0f, forward.x };         /* cross(forward, world up) */
    const float right_length = sqrtf(right.x * right.x + right.z * right.z);
    if (right_length < 1e-6f) {
        return false;
    }
    right.x /= right_length;
    right.z /= right_length;
    const editor_vec3 up = {
        right.y * forward.z - right.z * forward.y,
        right.z * forward.x - right.x * forward.z,
        right.x * forward.y - right.y * forward.x
    };

    float fov = cam->fov;
    if (fov <= 0.0f) {
        fov = 1.0471976f;
    }
    const float half_tan = tanf(fov * 0.5f);
    const float aspect = (float)b->width / (float)b->height;
    const float ndc_x = ((mx - (float)b->offset_x) / (float)b->width) * 2.0f - 1.0f;
    const float ndc_y = 1.0f - ((my - (float)b->offset_y) / (float)b->height) * 2.0f;

    /* a dummy object at the origin gives a camera context with an identity model matrix */
    system_scene_object dummy;
    memset(&dummy, 0, sizeof(dummy));
    dummy.scale = (editor_vec3){ 1.0f, 1.0f, 1.0f };
    edit_mode_camera_context context;
    if (!begin_edit_camera(&context, ui_ctx, &dummy)) {
        return false;
    }

    float best_error = 1e30f;
    editor_vec3 best_dir = forward;
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            editor_vec3 dir = {
                forward.x + right.x * ndc_x * half_tan * aspect * (float)sx + up.x * ndc_y * half_tan * (float)sy,
                forward.y + right.y * ndc_x * half_tan * aspect * (float)sx + up.y * ndc_y * half_tan * (float)sy,
                forward.z + right.z * ndc_x * half_tan * aspect * (float)sx + up.z * ndc_y * half_tan * (float)sy
            };
            const float length = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            dir.x /= length;
            dir.y /= length;
            dir.z /= length;
            const editor_vec3 sample = { cam->position.x + dir.x * 100.0f, cam->position.y + dir.y * 100.0f, cam->position.z + dir.z * 100.0f };
            const cam_screen s = project_edit_point(&context, sample);
            if (s.ok) {
                const float error = hypotf(s.x - mx, s.y - my);
                if (error < best_error) {
                    best_error = error;
                    best_dir = dir;
                }
            }
        }
    }
    *out_origin = cam->position;
    *out_dir = best_dir;
    return best_error < 4.0f;
}

int get_terrain_region_of_chunk(int cx, int cz)
{
    int rx = cx / g_terrain.dims.region_chunks;
    int rz = cz / g_terrain.dims.region_chunks;
    if (rx > terrain_region_grid - 1) {
        rx = terrain_region_grid - 1;
    }
    if (rz > terrain_region_grid - 1) {
        rz = terrain_region_grid - 1;
    }
    return rz * terrain_region_grid + rx;
}

/* the terrain under the cursor: the world position of the hit and its depth. false when the ray misses the terrain.
   the terrain brush (sculpt / paint) uses this too. */
bool get_terrain_ray_hit(const system_element_context* ui_ctx, float mx, float my, editor_vec3* out_hit, float* out_depth)
{
    if (!g_terrain.exists) {
        return false;
    }
    editor_vec3 origin, dir;
    if (!get_mouse_world_ray(ui_ctx, mx, my, &origin, &dir)) {
        return false;
    }
    editor_vec3 root = { 0.0f, 0.0f, 0.0f };
    terrain_root_position(&root);

    const float cell = g_terrain.settings.cell_size_m;
    const int cells_x = g_terrain.dims.cells_x;
    const int cells_z = g_terrain.dims.cells_z;
    const float far_limit = 6000.0f;

    float t = 0.5f;
    float previous_t = 0.0f;
    bool hit = false;
    while (t < far_limit) {
        const float lx = origin.x + dir.x * t - root.x;
        const float lz = origin.z + dir.z * t - root.z;
        const float gx = lx / cell + (float)g_terrain.dims.origin_x;
        const float gz = lz / cell + (float)g_terrain.dims.origin_z;
        if (gx >= 0.0f && gz >= 0.0f && gx < (float)cells_x && gz < (float)cells_z) {
            const float height = terrain_sample_height_m(lx, lz) + root.y;
            if (origin.y + dir.y * t <= height) {
                hit = true;
                break;
            }
        }
        previous_t = t;
        t += 0.75f + t * 0.01f;
    }
    if (!hit) {
        return false;
    }

    /* tighten the hit between the last step above the ground and the first one below it */
    float low = previous_t;
    float high = t;
    for (int i = 0; i < 14; i++) {
        const float mid = (low + high) * 0.5f;
        const float lx = origin.x + dir.x * mid - root.x;
        const float lz = origin.z + dir.z * mid - root.z;
        const float height = terrain_sample_height_m(lx, lz) + root.y;
        if (origin.y + dir.y * mid <= height) {
            high = mid;
        } else {
            low = mid;
        }
    }

    out_hit->x = origin.x + dir.x * high;
    out_hit->y = origin.y + dir.y * high;
    out_hit->z = origin.z + dir.z * high;

    const world_camera* cam = &engine_context.world_state.editor_scene_camera;
    const float pitch = cam->rotation.x;
    const float yaw = cam->rotation.y;
    const float along = dir.x * sinf(yaw) * cosf(pitch) + dir.y * sinf(pitch) + dir.z * -cosf(yaw) * cosf(pitch);
    *out_depth = high * along;                      /* the same kind of depth the mesh test returns */
    return true;
}

/* the terrain under the cursor: the depth and the chunk. false when the ray misses the terrain. */
bool pick_terrain_chunk(const system_element_context* ui_ctx, float mx, float my, float* out_depth, int* out_cx, int* out_cz)
{
    editor_vec3 hit;
    if (!get_terrain_ray_hit(ui_ctx, mx, my, &hit, out_depth)) {
        return false;
    }
    editor_vec3 root = { 0.0f, 0.0f, 0.0f };
    terrain_root_position(&root);
    const float cell = g_terrain.settings.cell_size_m;
    const float hx = hit.x - root.x;
    const float hz = hit.z - root.z;
    int gx = (int)floorf(hx / cell + (float)g_terrain.dims.origin_x);
    int gz = (int)floorf(hz / cell + (float)g_terrain.dims.origin_z);
    if (gx < 0) {
        gx = 0;
    }
    if (gz < 0) {
        gz = 0;
    }
    int cx = gx / g_terrain.dims.chunk_cells;
    int cz = gz / g_terrain.dims.chunk_cells;
    if (cx > g_terrain.dims.chunks_x - 1) {
        cx = g_terrain.dims.chunks_x - 1;
    }
    if (cz > g_terrain.dims.chunks_z - 1) {
        cz = g_terrain.dims.chunks_z - 1;
    }

    *out_cx = cx;
    *out_cz = cz;
    return true;
}

/* the world position of the middle of a block of chunks and how big it is */
void get_terrain_chunk_block(int cx0, int cz0, int cx1, int cz1, editor_vec3* out_centre, float* out_radius)
{
    editor_vec3 root = { 0.0f, 0.0f, 0.0f };
    terrain_root_position(&root);
    const float cell = g_terrain.settings.cell_size_m;
    const float chunk_m = (float)g_terrain.dims.chunk_cells * cell;
    const float min_x = ((float)(cx0 * g_terrain.dims.chunk_cells) - (float)g_terrain.dims.origin_x) * cell + root.x;
    const float min_z = ((float)(cz0 * g_terrain.dims.chunk_cells) - (float)g_terrain.dims.origin_z) * cell + root.z;
    const float width = (float)(cx1 - cx0 + 1) * chunk_m;
    const float depth = (float)(cz1 - cz0 + 1) * chunk_m;
    const float centre_x = min_x + width * 0.5f;
    const float centre_z = min_z + depth * 0.5f;
    out_centre->x = centre_x;
    out_centre->z = centre_z;
    out_centre->y = terrain_sample_height_m(centre_x - root.x, centre_z - root.z) + root.y;
    float radius = width;
    if (depth > radius) {
        radius = depth;
    }
    *out_radius = radius * 0.5f;
}

bool get_object_world_bounds(const system_scene_object* obj, editor_vec3* out_centre, float* out_radius)
{
    const void* vertices = NULL;
    uint32_t vertex_size = 0;
    uint32_t vertex_count = 0;
    const uint32_t* indices = NULL;
    uint32_t index_count = 0;
    if (!obj || !get_object_mesh_data(obj->id, &vertices, &vertex_size, &vertex_count, &indices, &index_count) ||
        vertex_size != sizeof(editor_vertex) || vertex_count == 0) {
        return false;
    }
    const editor_vertex* v = (const editor_vertex*)vertices;
    float lo[3] = { v[0].x, v[0].y, v[0].z };
    float hi[3] = { v[0].x, v[0].y, v[0].z };
    for (uint32_t i = 1; i < vertex_count; i++) {
        const float p[3] = { v[i].x, v[i].y, v[i].z };
        for (int k = 0; k < 3; k++) {
            if (p[k] < lo[k]) {
                lo[k] = p[k];
            }
            if (p[k] > hi[k]) {
                hi[k] = p[k];
            }
        }
    }
    const float mid[3] = { (lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f };
    const float half[3] = { (hi[0] - lo[0]) * 0.5f, (hi[1] - lo[1]) * 0.5f, (hi[2] - lo[2]) * 0.5f };

    const editor_mat4 model = system_math_model_matrix(obj->position, obj->rotation, obj->scale);
    out_centre->x = model.mesh[0][0] * mid[0] + model.mesh[1][0] * mid[1] + model.mesh[2][0] * mid[2] + model.mesh[3][0];
    out_centre->y = model.mesh[0][1] * mid[0] + model.mesh[1][1] * mid[1] + model.mesh[2][1] * mid[2] + model.mesh[3][1];
    out_centre->z = model.mesh[0][2] * mid[0] + model.mesh[1][2] * mid[1] + model.mesh[2][2] * mid[2] + model.mesh[3][2];

    float largest_scale = fabsf(obj->scale.x);
    if (fabsf(obj->scale.y) > largest_scale) {
        largest_scale = fabsf(obj->scale.y);
    }
    if (fabsf(obj->scale.z) > largest_scale) {
        largest_scale = fabsf(obj->scale.z);
    }
    *out_radius = sqrtf(half[0] * half[0] + half[1] * half[1] + half[2] * half[2]) * largest_scale;
    return true;
}

bool get_terrain_focus_target(uint32_t selected_object_id, editor_vec3* out_target, float* out_radius, float* out_distance_factor)
{
    if (!g_terrain.exists || selected_object_id == 0) {
        return false;
    }
    *out_distance_factor = 2.4f;

    int region = -1;
    for (int r = 0; r < terrain_region_count; r++) {
        if (g_terrain.region_ids[r] == selected_object_id) {
            region = r;
        }
    }

    if (region >= 0) {
        if (g_pick_chunk_x >= 0 && g_pick_chunk_region_object_id == selected_object_id) {
            get_terrain_chunk_block(g_pick_chunk_x, g_pick_chunk_z, g_pick_chunk_x, g_pick_chunk_z, out_target, out_radius);
            return true;
        }
        const int rc = g_terrain.dims.region_chunks;
        const int rx = region % terrain_region_grid;
        const int rz = region / terrain_region_grid;
        int cx1 = rx * rc + rc - 1;
        int cz1 = rz * rc + rc - 1;
        if (rx == terrain_region_grid - 1) {
            cx1 = g_terrain.dims.chunks_x - 1;
        }
        if (rz == terrain_region_grid - 1) {
            cz1 = g_terrain.dims.chunks_z - 1;
        }
        get_terrain_chunk_block(rx * rc, rz * rc, cx1, cz1, out_target, out_radius);
        return true;
    }

    if (selected_object_id == g_terrain.scene_object_id) {
        get_terrain_chunk_block(0, 0, g_terrain.dims.chunks_x - 1, g_terrain.dims.chunks_z - 1, out_target, out_radius);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* selecting                                                                  */
/* ------------------------------------------------------------------------- */
void select_scene_object_and_show(uint32_t object_id)
{
    g_scene_registry.selected_object_id = object_id;
    g_pick_chunk_x = -1;
    g_pick_chunk_z = -1;
    system_scene_object* obj = find_scene_object_by_id(object_id);
    if (!obj) {
        return;
    }
    for (system_scene_object* parent = obj->parent; parent != NULL; parent = parent->parent) {
        parent->is_open = true;                      /* the Scene Archive row must be visible */
    }
}

/* once per frame, after the modal windows had their say about the mouse */
void update_system_object_picking(system_element_context* ui_ctx)
{
    if (!ui_ctx) {
        return;
    }
    const bool pressed = ui_ctx->input.mouse_down && !g_pick_prev_down;
    g_pick_prev_down = ui_ctx->input.mouse_down;
    if (!pressed) {
        return;
    }
    if (!mode_state_is_object() || !is_edit_in_scene_view(ui_ctx)) {
        return;
    }
    /* a gizmo handle (the Y arrow floats over empty space) or the number boxes own this click */
    if (ui_ctx->active_id != 0 || ui_ctx->input.camera_manipulating || is_manip_overlay_using_mouse() || system_transform_gizmo_wants_mouse()) {
        return;
    }
    if (!check_window_focus() || (GetAsyncKeyState(VK_MENU) & 0x8000) != 0) {
        return;                                      /* Alt + click orbits the camera */
    }

    const float mx = (float)ui_ctx->input.mouse_x;
    const float my = (float)ui_ctx->input.mouse_y;
    uint32_t hit_id = 0;
    float hit_depth = -1.0f;
    for (uint32_t i = 1; i < g_scene_registry.object_count; i++) {
        const system_scene_object* obj = &g_scene_registry.objects[i];
        if (is_terrain_object(obj)) {
            continue;                                /* the terrain is tested against its height map below */
        }
        const float depth = get_object_pick_depth(ui_ctx, obj, mx, my);
        if (depth >= 0.0f && (hit_depth < 0.0f || depth < hit_depth)) {
            hit_depth = depth;
            hit_id = obj->id;
        }
    }

    int chunk_x = -1;
    int chunk_z = -1;
    float terrain_depth = -1.0f;
    if (pick_terrain_chunk(ui_ctx, mx, my, &terrain_depth, &chunk_x, &chunk_z)) {
        if (hit_depth < 0.0f || terrain_depth < hit_depth) {
            const int region = get_terrain_region_of_chunk(chunk_x, chunk_z);
            hit_id = g_terrain.region_ids[region];
            hit_depth = terrain_depth;
        } else {
            chunk_x = -1;
        }
    }

    if (hit_id != 0) {
        select_scene_object_and_show(hit_id);
        if (chunk_x >= 0) {
            g_pick_chunk_x = chunk_x;
            g_pick_chunk_z = chunk_z;
            g_pick_chunk_region_object_id = hit_id;
            set_statusbar_message("Selected terrain chunk %d, %d (Ctrl+F frames it).", chunk_x, chunk_z);
        } else {
            const system_scene_object* hit = find_scene_object_by_id(hit_id);
            if (hit) {
                set_statusbar_message("Selected: %s", hit->name);
            }
        }
    } else {
        select_scene_object_and_show(0);
    }
}
