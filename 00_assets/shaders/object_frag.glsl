#version 450

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;

layout(binding = 0) uniform sampler2D ui_sampler;

layout(location = 0) out vec4 out_color;

void main() {
    // Alpha > 1.5 is the viewport sentinel pushed by system_viewport.c:
    // sample the offscreen 3D scene texture instead of using a flat colour.
    if (in_color.a > 1.5) {
        out_color = vec4(texture(ui_sampler, in_uv).rgb, 1.0);
    } else {
        out_color = in_color;
    }
}