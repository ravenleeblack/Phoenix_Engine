#ifndef PHOENIX_SYSTEM_MATH_H
#define PHOENIX_SYSTEM_MATH_H
#include <math.h>
#include <stdbool.h>
/*
RULES THAT MUST BE FOLLOWED.
uncomment as we start getting things fixed: 
Must use lowercase seperated by underscores. Even structs names, enum names, etc.
Must use verbs before nouns when naming anything
Must not use ternerys. i fucking hate ternery operators.
Must use braced control-flow headers: every if, else if, and else must use braces.
Must call extern in engine_globals.h and init in engine_globals.c
Must not have statics in any file. I fucking hate statics.
No static structs, No static enums, No static anything.
Math functions must go in math file and header.
Drawing stuff must go into drawing stuff and headers.
ALl information gets routed into statusbar.

NO CAPITAL LETTERS UNLESS WINDOWS OR VULKAN DEMANDS IT.
DO NOT REMOVE THIS COMMENT EVER. THESE ARE THE RULES.
IF SOMETHING IS BREAKING THESE RULES CHANGE IT THEN WE CHANGE EVERYTHING CONNECTED AS WE GO.
THIS INCLUDES INSIDE THE SOURCE FILES NOT JUST HEADERS
*/



typedef struct {
    float x;
    float y;
    float z;
} editor_vec3;

/* Vertex layout shared by the CPU meshes, the GPU vertex buffers and mesh_vert.glsl. Stride = 48 bytes.
   New fields are appended so old {x,y,z,r,g,b,a} initialisers still compile (u, vert_to_edge_array, nx, ny, nz become 0).
   A zero normal means "no normal": the fragment shader then lights the face from screen derivatives. */
typedef struct {
    float x, y, z;        /* offset  0: position            */
    float r, g, b, a;     /* offset 12: vertex colour       */
    float u, vert_to_edge_array;           /* offset 28: texture / paint map */
    float nx, ny, nz;     /* offset 36: object-space normal */
} editor_vertex;

void set_vertex_attributes(editor_vertex* vertex, float u, float v,
                           float nx, float ny, float nz);
float round_value_to_step(float value, float step);
float get_vec3_axis(editor_vec3 value, int axis);
float get_vec3_distance_squared(editor_vec3 a, editor_vec3 b);

typedef struct {
    float mesh[4][4];   /* mesh[col][row], column-major, same memory layout the shaders consume */
} editor_mat4;

/* Vector cross products and linear perspective matrix generators */
editor_mat4 system_math_look_at(editor_vec3 eye, editor_vec3 target, editor_vec3 up);
editor_mat4 system_math_perspective(float fov_radians, float aspect, float near_z, float far_z);
/* Vulkan-correct perspective: Y flipped (clip Y points down), depth 0..1, right-handed. Use for the 3D scene pass. */
editor_mat4 system_math_perspective_vk(float fov_radians, float aspect, float near_z, float far_z);
editor_vec3 system_math_project_to_screen(editor_vec3 world_pos, float camera_pos[3], float camera_rot[3], float fov, float aspect, int pw, int ph, int px, int py);

/* scalar helpers */
int clamp_int_value(int value, int min_value, int max_value);

/* Matrix helpers */
editor_mat4 system_math_mat4_identity(void);
editor_mat4 system_math_mat4_multiply(editor_mat4 a, editor_mat4 b);   /* a * b */
editor_mat4 system_math_model_matrix(editor_vec3 position, editor_vec3 rotation, editor_vec3 scale); /* T * Ry * Rx * Rz * S */


static inline editor_vec3 sub3(editor_vec3 a, editor_vec3 b) { editor_vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z }; return r; }
static inline editor_vec3 add3(editor_vec3 a, editor_vec3 b) { editor_vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z }; return r; }
static inline editor_vec3 mul3(editor_vec3 a, float s)       { editor_vec3 r = { a.x * s, a.y * s, a.z * s }; return r; }
static inline float       dot3(editor_vec3 a, editor_vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline editor_vec3 cross3(editor_vec3 a, editor_vec3 b)
{
    editor_vec3 r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    return r;
}
static inline editor_vec3 norm3(editor_vec3 a)
{
    float l = sqrtf(dot3(a, a));
    if (l < 1e-12f) { editor_vec3 z = { 0, 0, 0 }; return z; }
    return mul3(a, 1.0f / l);
}
static inline editor_vec3 lerp3(editor_vec3 a, editor_vec3 b, float t) { return add3(mul3(a, 1.0f - t), mul3(b, t)); }

/* screen-space projection result + the camera it was projected with */
typedef struct { float x, y, w; bool ok; } cam_screen;
typedef struct { editor_mat4 mvp; int vx, vy, vw, vh; } edit_mode_camera_context;

/* face / projection / picking / loop math */
editor_vec3 system_math_compute_face_normal(editor_vec3 p0, editor_vec3 p1, editor_vec3 p2, editor_vec3 p3, int face_type);
cam_screen  system_math_project_point(const edit_mode_camera_context* c, editor_vec3 p);
float       system_math_compute_segment_distance(float px, float py, float ax, float ay, float bx, float by);
bool        system_math_is_point_in_triangle(float px, float py, cam_screen a, cam_screen b, cam_screen c);
float       system_math_compute_loop_t(int j, int face_type, float slide);
/* percent along the cut edge of cut line j (1..count) when `count` lines are spread evenly: count 1 -> 50, count 3 -> 25 / 50 / 75 */
float       system_math_compute_cut_percent(int j, int count);
/* offset of a label: `dist` pixels from a towards b (screen space) */
void        system_math_compute_label_offset(float ax, float ay, float bx, float by, float dist, float* out_x, float* out_y);
/* the two back corners of an arrow head whose tip is b, for a line a -> b */
void        system_math_compute_arrow_head(float ax, float ay, float bx, float by, float size, float* out_lx, float* out_ly, float* out_rx, float* out_ry);


int   imax(int a, int b);
int   imin(int a, int b);
int   clampi(int value, int lo, int hi);
float clampf(float value, float lo, float hi);
float lerpf(float a, float b, float t);
float smoothstepf(float e0, float e1, float x);
float sandbox_snap(float h, float unit);

void multiply_gizmo_vec4(const editor_mat4* m, float x, float y, float z, float w, float out[4]);
editor_vec3 rotate_vec3_about_axis(editor_vec3 v, editor_vec3 a, float ang);


#endif