#include "system_globals.h"

/*
 * The grid is generated entirely inside grid_vert.glsl from gl_VertexIndex
 * (1001 lines per axis * 2 axes * 2 verts = 4004 vertices), so NO vertex buffer is needed.
 *
 * There is ONE camera: engine_context.world_state.editor_scene_camera.
 * It is the same camera draw_scene_3d_in_rect() (system_element.c), the gimbal overlay
 * and the WASD / Alt-drag input already use, so the grid, meshes and gizmo cannot disagree.
 * Convention: yaw 0 faces -Z, rotation.x is pitch (negative looks down).
 */
#define GRID_VERTEX_COUNT   4004   /* must match grid_vert.glsl */
#define ZOOM_MIN_DISTANCE   1.0f
#define ZOOM_MAX_DISTANCE   50000.0f
#define ZOOM_STEP_FACTOR    0.88f      /* one wheel notch */
#define ZOOM_KEY_STEPS      0.25f      /* per frame while +/- is held */
#define DEFAULT_FOV         1.0471976f /* 60 degrees */

static world_camera* editor_camera(void)
{
    return &engine_context.world_state.editor_scene_camera;
}

static editor_vec3 camera_forward(const world_camera* cam)
{
    float cp = cosf(cam->rotation.x), sp = sinf(cam->rotation.x);
    float cy = cosf(cam->rotation.y), sy = sinf(cam->rotation.y);
    editor_vec3 f = { sy * cp, sp, -cy * cp };
    return f;
}

/* Near is capped so nearby terrain is never clipped away (the old 0.5% of the reference distance reached
   5 mesh+ when looking at a far horizon, which let you see through close terrain). Far is much further out. */

bool is_cursor_in_scene_view(void)
{
    const struct_viewport_bounds* b = &element_context.scene_view;
    if (b->width <= 0 || b->height <= 0) return true;
    return validate_rectangle_hit(element_context.input.mouse_x, element_context.input.mouse_y,
                                  b->offset_x, b->offset_y, b->width, b->height);
}

/* Ctrl+F / Home button: third person view of the selected object (or the world origin), looking down at it from a distance
   that grows with the object's size. Moves the editor's own camera copy, the one the scene view renders from. */
void focus_system_grid_on_selected_object(void)
{
    editor_vec3 target = { 0.0f, 0.0f, 0.0f };
    float radius = 2.0f;

    system_scene_object* selected = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (selected && selected != &g_scene_registry.objects[0]) {
        target = selected->position;
        float largest_scale = fabsf(selected->scale.x);
        if (fabsf(selected->scale.y) > largest_scale) {
            largest_scale = fabsf(selected->scale.y);
        }
        if (fabsf(selected->scale.z) > largest_scale) {
            largest_scale = fabsf(selected->scale.z);
        }
        if (selected->bounds_radius > 0.0f && largest_scale > 0.0f) {
            radius = selected->bounds_radius * largest_scale;
        }
    }

    float distance = radius * 6.0f;
    if (distance < 40.0f) {
        distance = 40.0f;
    }
    if (distance > ZOOM_MAX_DISTANCE) {
        distance = ZOOM_MAX_DISTANCE;
    }

    world_camera* cam = editor_camera();
    cam->rotation.x = -0.5f;
    editor_vec3 forward = camera_forward(cam);
    cam->position.x = target.x - forward.x * distance;
    cam->position.y = target.y - forward.y * distance;
    cam->position.z = target.z - forward.z * distance;
}

/* Point the camera zooms toward: the obj_vert_selected object, or the world origin. */
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
   - a normal object obj_vert_selected: distance to it
   - nothing / root / the huge terrain obj_vert_selected: where the view ray meets the y = 0 plane,
     or, when looking at the horizon / up, the camera height.
   Unlike a fixed target this never gets stuck: you can zoom straight through it. */
float get_zoom_reference_distance(void)
{
    world_camera* cam = editor_camera();
    const system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    bool is_terrain = (g_terrain.exists && o && o->id == g_terrain.scene_object_id);

    float d;
    if (o && o != &g_scene_registry.objects[0] && !is_terrain && o->bounds_radius < 500.0f) {
        d = get_distance_between(cam->position, o->position);
    } else {
        editor_vec3 f = camera_forward(cam);
        if (f.y < -0.02f && cam->position.y > 0.0f) d = cam->position.y / -f.y;
        else                                         d = fmaxf(fabsf(cam->position.y), 10.0f);
    }
    if (d < ZOOM_MIN_DISTANCE * 2.0f) d = ZOOM_MIN_DISTANCE * 2.0f;
    if (d > ZOOM_MAX_DISTANCE)        d = ZOOM_MAX_DISTANCE;
    return d;
}

/* Near/far grow with the distance so zooming far out never clips the grid or terrain. */
/* Fixed clip planes: nearest practical near, very far far. Culling is handled separately later. */
void sync_clip_planes(world_camera* cam, float dist)
{
    const float near_m = 0.05f;
    const float far_m  = 20000.0f;
    (void)dist;
    cam->near_clip = near_m;
    cam->far_clip  = far_m;
}

/* Place the camera at an orbit-style yaw/pitch/distance around the zoom target (kept for old callers). */
void set_system_grid_orbit_camera(float yaw, float pitch, float distance)
{
    if (pitch >  1.5f) pitch =  1.5f;
    if (pitch < -1.5f) pitch = -1.5f;
    if (distance < ZOOM_MIN_DISTANCE) distance = ZOOM_MIN_DISTANCE;
    if (distance > ZOOM_MAX_DISTANCE) distance = ZOOM_MAX_DISTANCE;

    world_camera* cam = editor_camera();
    cam->rotation.x = -pitch;
    cam->rotation.y = -yaw;
    cam->rotation.z = 0.0f;

    editor_vec3 target = get_zoom_target();
    editor_vec3 fwd = camera_forward(cam);
    cam->position.x = target.x - fwd.x * distance;
    cam->position.y = target.y - fwd.y * distance;
    cam->position.z = target.z - fwd.z * distance;
    sync_clip_planes(cam, distance);
}

/* Zoom the Scene view. steps > 0 zooms in, < 0 zooms out (one wheel notch = 1 step).
   Dollies the camera along its view direction by a fraction of the reference distance, so it
   feels the same at 5 mesh and at 5000 mesh and never sticks at a target.
   Only acts when the cursor is over the Scene view. Called from process_system_mouse_wheel().
   Accepts either notches (+-1) or a raw Win32 wheel delta (+-120): a raw delta used to be fed
   into powf() as 120 steps, which snapped the camera to the min / max distance. */
void system_grid_zoom(float steps)
{
    if (steps == 0.0f || !is_cursor_in_scene_view()) return;
    if (fabsf(steps) > 4.0f) steps /= 120.0f;
    if (steps >  4.0f) steps =  4.0f;
    if (steps < -4.0f) steps = -4.0f;

    world_camera* cam = editor_camera();
    editor_vec3 fwd = camera_forward(cam);
    float ref  = get_zoom_reference_distance();
    float move = ref * (1.0f - powf(ZOOM_STEP_FACTOR, steps));      /* > 0 forward, < 0 backward */

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
    if (zoom_out) system_grid_zoom(-ZOOM_KEY_STEPS);
    if (zoom_in)  system_grid_zoom( ZOOM_KEY_STEPS);

    /* Keep clip planes valid even when the camera was moved by WASD / Alt-pan. */
    world_camera* cam = editor_camera();
    sync_clip_planes(cam, get_zoom_reference_distance());

    /* Load / unload the 3x3 terrain chunks around the camera (cheap when nothing changed). */
    update_terrain_streaming();
}

/* View/projection of the Scene camera. Identical maths to draw_scene_3d_in_rect(). */
void build_scene_orbit_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj)
{
    if (aspect <= 0.0f) aspect = 16.0f / 9.0f;

    world_camera* cam = editor_camera();
    editor_vec3 fwd = camera_forward(cam);

    editor_vec3 eye    = cam->position;
    editor_vec3 target = { eye.x + fwd.x, eye.y + fwd.y, eye.z + fwd.z };
    editor_vec3 up     = { 0.0f, 1.0f, 0.0f };

    *out_view = system_math_look_at(eye, target, up);
    *out_proj = system_math_perspective_vk(cam->fov > 0.0f ? cam->fov : DEFAULT_FOV,
                                           aspect,
                                           cam->near_clip > 0.0f ? cam->near_clip : 0.1f,
                                           cam->far_clip  > 0.0f ? cam->far_clip  : 1000.0f);
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
    if (g_scene_view.show_grid) vkCmdDraw(cmd, GRID_VERTEX_COUNT, 1, 0, 0);

    draw_scene_objects(cmd, (const float*)&pc.view, (const float*)&pc.projection);
}