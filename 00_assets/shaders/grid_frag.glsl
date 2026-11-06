#version 450

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;   // kept to match the vertex output at location 1

layout(location = 0) out vec4 out_color;

void main() {
    out_color = in_color;
}