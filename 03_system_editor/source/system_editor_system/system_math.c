#include "system_globals.h"



editor_mat4 system_math_mat4_identity(void) {
    editor_mat4 r = {0};
    r.mesh[0][0] = r.mesh[1][1] = r.mesh[2][2] = r.mesh[3][3] = 1.0f;
    return r;
}

/* r = a * b (column-major, mesh[col][row]) */
editor_mat4 system_math_mat4_multiply(editor_mat4 a, editor_mat4 b) {
    editor_mat4 r;
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += a.mesh[k][row] * b.mesh[c][k];
            r.mesh[c][row] = s;
        }
    }
    return r;
}

/* M = T * Ry * Rx * Rz * S. Rotation is radians: x = pitch, y = yaw, z = roll. */
editor_mat4 system_math_model_matrix(editor_vec3 position, editor_vec3 rotation, editor_vec3 scale) {
    float cx = cosf(rotation.x), sx = sinf(rotation.x);
    float cy = cosf(rotation.y), sy = sinf(rotation.y);
    float cz = cosf(rotation.z), sz = sinf(rotation.z);

    editor_mat4 rx = system_math_mat4_identity();
    rx.mesh[1][1] = cx; rx.mesh[1][2] = sx; rx.mesh[2][1] = -sx; rx.mesh[2][2] = cx;

    editor_mat4 ry = system_math_mat4_identity();
    ry.mesh[0][0] = cy; ry.mesh[0][2] = -sy; ry.mesh[2][0] = sy; ry.mesh[2][2] = cy;

    editor_mat4 rz = system_math_mat4_identity();
    rz.mesh[0][0] = cz; rz.mesh[0][1] = sz; rz.mesh[1][0] = -sz; rz.mesh[1][1] = cz;

    editor_mat4 r = system_math_mat4_multiply(system_math_mat4_multiply(ry, rx), rz);

    for (int i = 0; i < 3; i++) {
        r.mesh[0][i] *= scale.x;
        r.mesh[1][i] *= scale.y;
        r.mesh[2][i] *= scale.z;
    }
    r.mesh[3][0] = position.x;
    r.mesh[3][1] = position.y;
    r.mesh[3][2] = position.z;
    return r;
}

editor_mat4 system_math_look_at(editor_vec3 eye, editor_vec3 target, editor_vec3 up) {
    editor_mat4 res = {0};
    
    float fx = target.x - eye.x;
    float fy = target.y - eye.y;
    float fz = target.z - eye.z;
    float r_f = 1.0f / sqrtf(fx*fx + fy*fy + fz*fz + 0.00001f);
    fx *= r_f; fy *= r_f; fz *= r_f;

    float rx = fy * up.z - fz * up.y;
    float ry = fz * up.x - fx * up.z;
    float rz = fx * up.y - fy * up.x;
    float r_r = 1.0f / sqrtf(rx*rx + ry*ry + rz*rz + 0.00001f);
    rx *= r_r; ry *= r_r; rz *= r_r;

    float ux = ry * fz - rz * fy;
    float uy = rz * fx - rx * fz;
    float uz = rx * fy - ry * fx;

    res.mesh[0][0] = rx;  res.mesh[1][0] = ry;  res.mesh[2][0] = rz;  res.mesh[3][0] = -(rx*eye.x + ry*eye.y + rz*eye.z);
    res.mesh[0][1] = ux;  res.mesh[1][1] = uy;  res.mesh[2][1] = uz;  res.mesh[3][1] = -(ux*eye.x + uy*eye.y + uz*eye.z);
    res.mesh[0][2] = -fx; res.mesh[1][2] = -fy; res.mesh[2][2] = -fz; res.mesh[3][2] = (fx*eye.x + fy*eye.y + fz*eye.z);
    res.mesh[3][3] = 1.0f;

    return res;
}

editor_mat4 system_math_perspective(float fov_radians, float aspect, float near_z, float far_z) {
    editor_mat4 res = {0};
    float g = 1.0f / tanf(fov_radians * 0.5f);
    
    res.mesh[0][0] = g / aspect;
    res.mesh[1][1] = g;
    res.mesh[2][2] = far_z / (near_z - far_z);
    res.mesh[2][3] = -1.0f;
    res.mesh[3][2] = (near_z * far_z) / (near_z - far_z);
    
    return res;
}

/* Same as system_math_perspective, but with the Vulkan Y flip applied.
   Vulkan clip space has +Y pointing DOWN, so without this the scene renders upside down. */
editor_mat4 system_math_perspective_vk(float fov_radians, float aspect, float near_z, float far_z) {
    editor_mat4 res = system_math_perspective(fov_radians, aspect, near_z, far_z);
    res.mesh[1][1] = -res.mesh[1][1];
    return res;
}

editor_vec3 system_math_project_to_screen(editor_vec3 world_pos, float camera_pos[3], float camera_rot[3], float fov, float aspect, int pw, int ph, int px, int py) {
    editor_vec3 out = { -1.0f, -1.0f, 0.0f };
    if (pw <= 0 || ph <= 0) return out;

    float cos_yaw = cosf(camera_rot[1]);
    float sin_yaw = sinf(camera_rot[1]);
    float cos_pitch = cosf(camera_rot[0]);
    float sin_pitch = sinf(camera_rot[0]);

    float dx = world_pos.x - camera_pos[0];
    float dy = world_pos.y - camera_pos[1];
    float dz = world_pos.z - camera_pos[2];

    float cx = cos_yaw * dx + sin_yaw * dz;
    float cy = sin_pitch * (sin_yaw * dx - cos_yaw * dz) + cos_pitch * dy;
    float cz = cos_pitch * (sin_yaw * dx - cos_yaw * dz) - sin_pitch * dy;

    if (cz >= -0.1f) return out; // Frustum near-plane clipping check

    float fov_scale = 1.0f / tanf(fov * 0.5f);
    float sx = (cx * fov_scale) / (-cz * aspect);
    float sy = (cy * fov_scale) / (-cz);

    out.x = (float)px + ((sx + 1.0f) * 0.5f * (float)pw);
    out.y = (float)py + ((1.0f - sy) * 0.5f * (float)ph);
    out.z = -cz; // Pass raw depth tracking down to grid checks
    return out;
}
#define EDIT_OPS_FLIP_Y 0         /* set to 1 if clicks land mirrored top to bottom on your viewport */

editor_vec3 system_math_compute_face_normal(editor_vec3 p0, editor_vec3 p1, editor_vec3 p2, editor_vec3 p3, int face_type)
{
    if (face_type == 4) return norm3(cross3(sub3(p2, p0), sub3(p3, p1)));   /* diagonals: right for bent quads too */
    return norm3(cross3(sub3(p1, p0), sub3(p2, p0)));
}

cam_screen system_math_project_point(const edit_mode_camera_context* c, editor_vec3 p)
{
    const float (*M)[4] = c->mvp.mesh;                      /* M[col][row] */
    const float cx = M[0][0] * p.x + M[1][0] * p.y + M[2][0] * p.z + M[3][0];
    const float cy = M[0][1] * p.x + M[1][1] * p.y + M[2][1] * p.z + M[3][1];
    const float cw = M[0][3] * p.x + M[1][3] * p.y + M[2][3] * p.z + M[3][3];
    cam_screen s; s.x = s.y = 0.0f; s.w = cw; s.ok = cw > 1e-4f;
    if (s.ok) {
        float ny = cy / cw;
#if EDIT_OPS_FLIP_Y
        ny = -ny;
#endif
        s.x = (float)c->vx + (cx / cw * 0.5f + 0.5f) * (float)c->vw;
        s.y = (float)c->vy + (ny * 0.5f + 0.5f) * (float)c->vh;
    }
    return s;
}

float system_math_compute_segment_distance(float px, float py, float ax, float ay, float bx, float by)
{
    const float dx = bx - ax, dy = by - ay;
    const float l2 = dx * dx + dy * dy;
    float t = (l2 > 1e-6f) ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const float qx = ax + dx * t - px, qy = ay + dy * t - py;
    return sqrtf(qx * qx + qy * qy);
}

bool system_math_is_point_in_triangle(float px, float py, cam_screen a, cam_screen b, cam_screen c)
{
    const float d1 = (px - b.x) * (a.y - b.y) - (a.x - b.x) * (py - b.y);
    const float d2 = (px - c.x) * (b.y - c.y) - (b.x - c.x) * (py - c.y);
    const float d3 = (px - a.x) * (c.y - a.y) - (c.x - a.x) * (py - a.y);
    const bool has_neg = d1 < 0 || d2 < 0 || d3 < 0, has_pos = d1 > 0 || d2 > 0 || d3 > 0;
    return !(has_neg && has_pos);
}

float system_math_compute_loop_t(int j, int face_type, float slide)
{
    if (j <= 0) return 0.0f;
    if (j >= face_type + 1) return 1.0f;
    return ((float)j + slide) / (float)(face_type + 1);
}

float system_math_compute_cut_percent(int j, int count)
{
    if (count < 1) count = 1;
    return 100.0f * (float)j / (float)(count + 1);
}

void system_math_compute_label_offset(float ax, float ay, float bx, float by, float dist, float* out_x, float* out_y)
{
    const float dx = bx - ax, dy = by - ay;
    const float l = sqrtf(dx * dx + dy * dy);
    if (l < 1e-4f) { *out_x = 0.0f; *out_y = -dist; return; }
    *out_x = dx / l * dist;
    *out_y = dy / l * dist;
}

void system_math_compute_arrow_head(float ax, float ay, float bx, float by, float size, float* out_lx, float* out_ly, float* out_rx, float* out_ry)
{
    float dx = bx - ax, dy = by - ay;
    const float l = sqrtf(dx * dx + dy * dy);
    if (l < 1e-4f) { dx = 1.0f; dy = 0.0f; } else { dx /= l; dy /= l; }
    const float baseX = bx - dx * size, baseY = by - dy * size;
    *out_lx = baseX - dy * size * 0.5f;  *out_ly = baseY + dx * size * 0.5f;
    *out_rx = baseX + dy * size * 0.5f;  *out_ry = baseY - dx * size * 0.5f;
}


/* .c */
float round_value_to_step(float value, float step)
{
    return roundf(value / step) * step;
}
 
float get_vec3_axis(editor_vec3 v, int axis)
{
    if (axis == 0) return v.x;
    if (axis == 1) return v.y;
    return v.z;
}
 
float get_vec3_distance_squared(editor_vec3 a, editor_vec3 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}