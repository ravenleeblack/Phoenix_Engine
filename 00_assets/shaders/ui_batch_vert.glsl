#version 450
// One instance = one UI quad. 4 vertices per instance (triangle strip), no vertex buffer data per vertex.
layout(location = 0) in vec2 in_pos;     // pixels, top-left
layout(location = 1) in vec2 in_size;    // pixels
layout(location = 2) in vec4 in_color;   // already linear if the swapchain is sRGB (done on the CPU)

layout(push_constant) uniform PC { vec2 resolution; } pc;

layout(location = 0) flat out vec4  v_color;   // flat: the alpha channel carries a circle code, it must not be interpolated
layout(location = 1)      out vec2  v_local;   // pixels from the quad center
layout(location = 2) flat out float v_half;    // half the quad size (circles: radius + 1px for the soft edge)

void main()
{
    vec2 corner = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));
    vec2 p      = in_pos + corner * in_size;
    vec2 ndc    = vec2(p.x / pc.resolution.x * 2.0 - 1.0,
                       1.0 - p.y / pc.resolution.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color     = in_color;
    v_local     = (corner - 0.5) * in_size;
    v_half      = in_size.x * 0.5;
}