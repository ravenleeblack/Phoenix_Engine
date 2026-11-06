#version 450

// 128 bytes. Must match grid_camera_push_constant in system_grid.c. Vertex stage only.
layout(push_constant) uniform camera_block {
    mat4 view;
    mat4 projection;
} camera_transforms;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;   // keeps the interface identical to grid_frag.glsl

// ---- World measurements. 1 unit = 1 meter. ----
const int cell_size_m  = 4;     // sub grid: one cube / one plane is exactly one cell
const int main_every_m = 16;    // main lines: every 4 cells
const int chunk_size_m = 2000;  // terrain chunk: 500 cubes x 4 m = 2000 m x 2000 m
const int chunk_border_m = 1000;// world (0,0) is the MIDDLE of a chunk, so chunk borders sit at +-1000, +-3000 ... (odd multiples of 1000)
const float depth_bias = 1.0e-6;// pulls the grid toward the camera in depth so it draws ON TOP of terrain that is also at y = 0

// If you change the values above, update GRID_VERTEX_COUNT in system_grid.c:
//   lines_per_axis = (half_extent_m * 2) / cell_size_m + 1
//   GRID_VERTEX_COUNT = lines_per_axis * 2 * 2
// (also defined in system_scene_offscreen.c: that is the one the Scene view draws with)
const int half_extent_m = 2000;   // +-2000 m (one chunk each side of the middle) -> 1001 lines per axis -> 4004 vertices

// Colors below are written the way they should look on screen (sRGB).
// Set to false if the swapchain is NOT an sRGB format (lines will look too dark otherwise).
const bool swapchain_is_srgb = false;

// Maya-like palette
// The scene background is a gradient (about 0.16 at the bottom to 0.43+ at the top), so the 4m lines have to sit
// clearly above it or only the 16m lines show and the grid looks like 16m squares.
const vec3 color_sub    = vec3(0.36, 0.36, 0.39);   // every 4m  (one cell = one cube)
const vec3 color_main   = vec3(0.52, 0.52, 0.56);   // every 16m (4 cells)
const vec3 color_chunk  = vec3(0.80, 0.80, 0.85);   // chunk borders (+-1000, +-3000 ...)
const vec3 color_axis_x = vec3(0.85, 0.25, 0.25);   // the X axis line (z = 0)
const vec3 color_axis_z = vec3(0.28, 0.46, 0.90);   // the Z axis line (x = 0)

vec3 convert_to_display_color(vec3 c) {
    return swapchain_is_srgb ? pow(c, vec3(2.2)) : c;
}

// coord_m = the fixed world coordinate (meters) this line sits on.
vec3 pick_line_color(int coord_m, vec3 axis_color) {
    int c = abs(coord_m);
    if (c == 0)                return axis_color;
    if (c % chunk_size_m == chunk_border_m) return color_chunk;
    if (c % main_every_m == 0) return color_main;
    return color_sub;
}

void main() 
{
    int lines_per_axis = (half_extent_m * 2) / cell_size_m + 1;   // 1001
    int verts_per_axis = lines_per_axis * 2;                      // 2002

    vec3 pos = vec3(0.0);
    vec3 line_color;

    if (gl_VertexIndex < verts_per_axis) {
        // Lines running along X, stepping through Z
        int line_idx  = gl_VertexIndex / 2;
        int vert_side = gl_VertexIndex % 2;
        int z_m = -half_extent_m + line_idx * cell_size_m;
        pos.x = (vert_side == 0) ? -float(half_extent_m) : float(half_extent_m);
        pos.z = float(z_m);
        line_color = pick_line_color(z_m, color_axis_x);   // the line at z = 0 IS the X axis
    } else {
        // Lines running along Z, stepping through X
        int adjusted  = gl_VertexIndex - verts_per_axis;
        int line_idx  = adjusted / 2;
        int vert_side = adjusted % 2;
        int x_m = -half_extent_m + line_idx * cell_size_m;
        pos.x = float(x_m);
        pos.z = (vert_side == 0) ? -float(half_extent_m) : float(half_extent_m);
        line_color = pick_line_color(x_m, color_axis_z);   // the line at x = 0 IS the Z axis
    }

    gl_Position = camera_transforms.projection * camera_transforms.view * vec4(pos, 1.0);
    gl_Position.z -= depth_bias * gl_Position.w;   // constant offset in depth for the whole line (clip space is linear), not a lift in height
    out_color   = vec4(convert_to_display_color(line_color), 1.0);
    out_uv      = vec2(0.0);
}