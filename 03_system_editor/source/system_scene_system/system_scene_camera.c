#include "system_globals.h"

/* ------------------------------------------------------------------ */
/* Two different cameras live in the editor. Do not mix them up:       */
/*                                                                     */
/*  1. SYSTEM camera  - the editor fly camera (WASD). Not an object,   */
/*                      never in the hierarchy. Drives the Scene view. */
/*                      build_scene_view_camera()                      */
/*                                                                     */
/*  2. GAME camera    - a camera OBJECT created into the hierarchy     */
/*                      (Objects > Create Camera / +CAMERA). The first */
/*                      one in the hierarchy drives the Game view.     */
/*                      find_scene_game_camera()                       */
/*                      build_game_view_camera()                       */
/* ------------------------------------------------------------------ */


/* Clip planes that follow the camera height, so you can fly high and still see large sections cleanly.
   Preferences > CAMERA near / far are the values used near the ground. Going up:
     far  grows to 10 x your height (at 3 km up you see 30 km), never above 50 km
     near grows to far / 20000, because the depth buffer error grows with distance squared and only shrinks as near grows
   (near 0.5 mesh / far 5 km = 3 mesh of depth error at 5 km. near 1.25 mesh / far 25 km = ~30 mesh at 25 km, the same 0.1 %). */
static void scene_clip_planes(editor_vec3 eye, float* out_near, float* out_far)
{
    const float alt = fabsf(eye.y);
    float far_m = g_camera_prefs.far_clip;
    if (alt * 10.0f > far_m) far_m = alt * 10.0f;
    if (far_m > 50000.0f)    far_m = 50000.0f;

    /* Height above the ground under the camera (the terrain when there is one, else the y = 0 plane).
       Close to the ground the near plane comes down with you, so the cell under your feet is never clipped.
       High up it goes back to the depth-precision rule (far / 20000). */
    float ground = 0.0f;
    editor_vec3 root = { 0.0f, 0.0f, 0.0f };
    if (g_terrain.exists && terrain_root_position(&root))
        ground = root.y + terrain_sample_height_m(eye.x - root.x, eye.z - root.z);
    const float agl = fabsf(eye.y - ground);
    const float low = fmaxf(0.01f, agl * 0.25f);

    float near_m = g_camera_prefs.near_clip;
    if (near_m > low) near_m = low;
    const float min_near = fminf(far_m / 20000.0f, low);
    if (near_m < min_near) near_m = min_near;

    *out_near = near_m;
    *out_far  = far_m;
}

/* System camera: reads the editor camera that the engine's WASD input writes to.
   Convention: yaw 0 faces -Z, rotation.x is pitch (negative looks down). */
void build_scene_view_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj)
{
    world_camera* cam = &engine_context.world_state.editor_scene_camera;
    float cp = cosf(cam->rotation.x), sp = sinf(cam->rotation.x);
    float cy = cosf(cam->rotation.y), sy = sinf(cam->rotation.y);

    editor_vec3 eye    = cam->position;
    editor_vec3 target = { eye.x + sy * cp, eye.y + sp, eye.z - cy * cp };
    editor_vec3 up     = { 0.0f, 1.0f, 0.0f };

    *out_view = system_math_look_at(eye, target, up);
    /* clip planes: Preferences > CAMERA near / far near the ground, stretched with height (scene_clip_planes) */
    float near_m, far_m;
    scene_clip_planes(eye, &near_m, &far_m);
    *out_proj = system_math_perspective_vk(cam->fov > 0.0f ? cam->fov : 1.0472f,
                                           aspect,
                                           near_m,
                                           far_m);
}

/* The camera object that drives the Game view: the first camera in the hierarchy. NULL if none. */
system_scene_object* find_scene_game_camera(void)
{
    for (uint32_t i = 1; i < g_scene_registry.object_count; i++) {   /* 0 = root */
        if (g_scene_registry.objects[i].type == scene_object_camera) {
            return &g_scene_registry.objects[i];
        }
    }
    return NULL;
}

/* Builds view/proj from the hierarchy camera object, using the same transform the glyph is drawn with
   (the glyph's lens points down local -Z). Returns false when there is no camera object. */
bool build_game_view_camera(float aspect, editor_mat4* out_view, editor_mat4* out_proj)
{
    system_scene_object* cam = find_scene_game_camera();
    if (!cam) return false;

    editor_vec3 unit_scale = { 1.0f, 1.0f, 1.0f };
    editor_mat4 model = system_math_model_matrix(cam->position, cam->rotation, unit_scale);

    editor_vec3 eye     = cam->position;
    editor_vec3 forward = { -model.mesh[2][0], -model.mesh[2][1], -model.mesh[2][2] };   /* local -Z in world */
    editor_vec3 up      = {  model.mesh[1][0],  model.mesh[1][1],  model.mesh[1][2] };   /* local +Y in world */
    editor_vec3 target  = { eye.x + forward.x, eye.y + forward.y, eye.z + forward.z };

    *out_view = system_math_look_at(eye, target, up);
    *out_proj = system_math_perspective_vk(game_camera_fov, aspect, game_camera_near, game_camera_far);
    return true;
}