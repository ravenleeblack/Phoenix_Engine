#version 450

// 128 bytes. Must match grid_camera_push_constant in system_grid.c. Vertex stage only.
layout(push_constant) uniform camera_block {
    mat4 view;
    mat4 projection;
} camera_transforms;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;   // keeps the interface identical to grid_frag.glsl

const int cell_size_m  = 4;
const int main_every_m = 16;
const int chunk_size_m = 2000;
const int chunk_border_m = 1000;
const float depth_bias = 1.0e-6;

const vec3 color_sub    = vec3(0.36, 0.36, 0.39);
const vec3 color_main   = vec3(0.52, 0.52, 0.56);
const vec3 color_chunk  = vec3(0.80, 0.80, 0.85);
const vec3 color_axis_x = vec3(0.85, 0.25, 0.25);
const vec3 color_axis_z = vec3(0.28, 0.46, 0.90);

vec3 pick_line_color(int coord_m, vec3 axis_color) {
    int c = abs(coord_m);
    if (c == 0) return axis_color;
    if (c % chunk_size_m == chunk_border_m) return color_chunk;
    if (c % main_every_m == 0) return color_main;
    return color_sub;
}

void main()
{
    // the cpu passes the grid half size in cells as firstInstance: gl_InstanceIndex = cells per half side
    int half_extent_m = max(gl_InstanceIndex, 1) * cell_size_m;
    int lines_per_axis = (half_extent_m * 2) / cell_size_m + 1;
    int verts_per_axis = lines_per_axis * 2;

    vec3 pos = vec3(0.0);
    vec3 line_color;

    if (gl_VertexIndex < verts_per_axis) {
        int line_idx = gl_VertexIndex / 2;
        int vert_side = gl_VertexIndex % 2;
        int z_m = -half_extent_m + line_idx * cell_size_m;
        pos.x = (vert_side == 0) ? -float(half_extent_m) : float(half_extent_m);
        pos.z = float(z_m);
        line_color = pick_line_color(z_m, color_axis_x);
    } else {
        int adjusted = gl_VertexIndex - verts_per_axis;
        int line_idx = adjusted / 2;
        int vert_side = adjusted % 2;
        int x_m = -half_extent_m + line_idx * cell_size_m;
        pos.x = float(x_m);
        pos.z = (vert_side == 0) ? -float(half_extent_m) : float(half_extent_m);
        line_color = pick_line_color(x_m, color_axis_z);
    }

    gl_Position = camera_transforms.projection * camera_transforms.view * vec4(pos, 1.0);
    gl_Position.z -= depth_bias * gl_Position.w;
    out_color = vec4(line_color, 1.0);
    out_uv = vec2(0.0);
}
