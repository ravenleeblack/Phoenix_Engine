#version 450

layout(push_constant) uniform ui_block {
    vec2 position;
    vec2 size;
    vec4 color;
    vec2 resolution;
} push_constants;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;

void main() {
    vec2 vertices[4] = vec2[](
        vec2(0.0, 0.0), // Top-Left
        vec2(1.0, 0.0), // Top-Right
        vec2(0.0, 1.0), // Bottom-Left
        vec2(1.0, 1.0)  // Bottom-Right
    );

    vec2 local_pos = vertices[gl_VertexIndex];
    vec2 pixel_pos = push_constants.position + (local_pos * push_constants.size);

    vec2 clip_space = vec2(
        pixel_pos.x / push_constants.resolution.x * 2.0 - 1.0,
        pixel_pos.y / push_constants.resolution.y * 2.0 - 1.0
    );

    gl_Position = vec4(clip_space, 0.0, 1.0);

    out_color = push_constants.color;
    out_uv    = local_pos;
}