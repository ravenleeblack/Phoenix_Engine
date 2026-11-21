#include "engine_globals.h"



world_camera* get_editor_camera(void)
{
    return &engine_context.world_state.editor_scene_camera;
}

editor_vec3 get_camera_forward(const world_camera* cam)
{
    float cp = cosf(cam->rotation.x), sp = sinf(cam->rotation.x);
    float cy = cosf(cam->rotation.y), sy = sinf(cam->rotation.y);
    editor_vec3 f = { sy * cp, sp, -cy * cp };
    return f;
}

/* The engine does not know the editor's panels. The editor writes g_scene_cursor_in_view once per frame. */
bool is_cursor_in_scene_view(void)
{
    return g_scene_cursor_in_view;
}

/* Point the camera zooms toward: the selected object, or the world origin. */
editor_vec3 get_zoom_target(void)
{
    const system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (o && o != &g_scene_registry.objects[0]) return o->position;
    editor_vec3 origin = { 0.0f, 0.0f, 0.0f };
    return origin;
}

float get_distance_between(editor_vec3 a, editor_vec3 b)
{
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* How far "the thing you are looking at" is. Zoom moves a fraction of THIS distance per notch.
   - a normal object selected: distance to it
   - nothing / root / the huge terrain selected: where the view ray meets the y = 0 plane,
     or, when looking at the horizon / up, the camera height.
   Unlike a fixed target this never gets stuck: you can zoom straight through it. */
float get_zoom_reference_distance(void)
{
    world_camera* cam = get_editor_camera();
    const system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    bool is_terrain = (g_terrain.exists && o && o->id == g_terrain.scene_object_id);

    float d;
    if (o && o != &g_scene_registry.objects[0] && !is_terrain && o->bounds_radius < 500.0f) {
        d = get_distance_between(cam->position, o->position);
    } else {
        editor_vec3 f = get_camera_forward(cam);
        if (f.y < -0.02f && cam->position.y > 0.0f) {
            d = cam->position.y / -f.y;
        } else {
            d = fmaxf(fabsf(cam->position.y), 10.0f);
        }
    }
    if (d < zoom_min_distance * 2.0f) d = zoom_min_distance * 2.0f;
    if (d > zoom_max_distance)        d = zoom_max_distance;
    return d;
}

/* Fixed clip planes: nearest practical near, very far far. Culling is handled separately later. */
void sync_clip_planes(world_camera* cam, float distance)
{
    const float near_m = 0.05f;
    const float far_m  = 20000.0f;
    (void)distance;
    cam->near_clip = near_m;
    cam->far_clip  = far_m;
}

/* Place the camera at an orbit-style yaw/pitch/distance around the zoom target (kept for old callers). */
void set_system_grid_orbit_camera(float yaw, float pitch, float distance)
{
    if (pitch >  1.5f) pitch =  1.5f;
    if (pitch < -1.5f) pitch = -1.5f;
    if (distance < zoom_min_distance) distance = zoom_min_distance;
    if (distance > zoom_max_distance) distance = zoom_max_distance;

    world_camera* cam = get_editor_camera();
    cam->rotation.x = -pitch;
    cam->rotation.y = -yaw;
    cam->rotation.z = 0.0f;

    editor_vec3 target = get_zoom_target();
    editor_vec3 fwd = get_camera_forward(cam);
    cam->position.x = target.x - fwd.x * distance;
    cam->position.y = target.y - fwd.y * distance;
    cam->position.z = target.z - fwd.z * distance;
    sync_clip_planes(cam, distance);
}

/* Zoom the Scene view. steps > 0 zooms in, < 0 zooms out (one wheel notch = 1 step).
   Dollies the camera along its view direction by a fraction of the reference distance.
   Only acts when the editor says the cursor is over the Scene view.
   Accepts either notches (+-1) or a raw Win32 wheel delta (+-120). */
void zoom_system_grid(float steps)
{
    if (steps == 0.0f || !is_cursor_in_scene_view()) return;
    if (fabsf(steps) > 4.0f) steps /= 120.0f;
    if (steps >  4.0f) steps =  4.0f;
    if (steps < -4.0f) steps = -4.0f;

    world_camera* cam = get_editor_camera();
    editor_vec3 fwd = get_camera_forward(cam);
    float ref  = get_zoom_reference_distance();
    float move = ref * (1.0f - powf(zoom_step_factor, steps));      /* > 0 forward, < 0 backward */

    cam->position.x += fwd.x * move;
    cam->position.y += fwd.y * move;
    cam->position.z += fwd.z * move;

    /* keep float precision sane */
    if (cam->position.x >  200000.0f) cam->position.x =  200000.0f;
    if (cam->position.x < -200000.0f) cam->position.x = -200000.0f;
    if (cam->position.z >  200000.0f) cam->position.z =  200000.0f;
    if (cam->position.z < -200000.0f) cam->position.z = -200000.0f;
    if (cam->position.y >  100000.0f) cam->position.y =  100000.0f;
    if (cam->position.y < -100000.0f) cam->position.y = -100000.0f;

    sync_clip_planes(cam, get_zoom_reference_distance());
}

/* Call once per frame AFTER update_input_system(). +/- = keyboard zoom. (Ctrl+F lives in system_proc.c) */
void update_system_grid_hotkeys(void)
{
    bool zoom_out = input_is_key_down(phoenix_key_minus) || input_is_key_down(phoenix_key_subtract);
    bool zoom_in  = input_is_key_down(phoenix_key_plus)  || input_is_key_down(phoenix_key_add);
    if (zoom_out) zoom_system_grid(-zoom_key_steps);
    if (zoom_in)  zoom_system_grid( zoom_key_steps);

    /* Keep clip planes valid even when the camera was moved by WASD / Alt-pan. */
    world_camera* cam = get_editor_camera();
    sync_clip_planes(cam, get_zoom_reference_distance());

    /* Load / unload the 3x3 terrain chunks around the camera (cheap when nothing changed). */
    update_terrain_streaming();
}

/* View/projection of the Scene camera. */
void build_scene_orbit_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj)
{
    if (aspect <= 0.0f) aspect = 16.0f / 9.0f;

    world_camera* cam = get_editor_camera();
    editor_vec3 fwd = get_camera_forward(cam);

    editor_vec3 eye    = cam->position;
    editor_vec3 target = { eye.x + fwd.x, eye.y + fwd.y, eye.z + fwd.z };
    editor_vec3 up     = { 0.0f, 1.0f, 0.0f };

    float fov      = cam->fov;
    float near_m   = cam->near_clip;
    float far_m    = cam->far_clip;
    if (fov    <= 0.0f) fov    = grid_default_fov;
    if (near_m <= 0.0f) near_m = 0.1f;
    if (far_m  <= 0.0f) far_m  = 1000.0f;

    *out_view = system_math_look_at(eye, target, up);
    *out_proj = system_math_perspective_vk(fov, aspect, near_m, far_m);
}

/*
 * Draws the grid, then the scene objects using the SAME view/projection.
 * `layout_3d` must have a push constant range { VK_SHADER_STAGE_VERTEX_BIT, 0, 128 }.
 * Do NOT also call draw_scene_objects from the scene pass.
 */
void draw_system_grid_axis_stream(VkCommandBuffer cmd, VkPipelineLayout layout_3d, VkShaderStageFlags stages, float aspect)
{
    grid_camera_push_constant pc;
    build_scene_orbit_camera(aspect, &pc.view, &pc.projection);

    vkCmdPushConstants(cmd, layout_3d, stages, 0, (uint32_t)sizeof(pc), &pc);
    if (g_scene_view.show_grid) {
        uint32_t grid_half = (uint32_t)g_scene_view.grid_half_cells;
        if (grid_half < 1u) {
            grid_half = 50u;
        }
        vkCmdDraw(cmd, (grid_half * 2u + 1u) * 4u, 1, 0, grid_half);   /* firstInstance carries the grid size to grid_vert.glsl */
    }

    draw_scene_objects(cmd, (const float*)&pc.view, (const float*)&pc.projection);
}