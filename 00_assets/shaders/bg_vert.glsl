#version 450
// Fullscreen triangle, no vertex buffer. uv.y: 0 at the top of the viewport, 1 at the bottom.
layout(location = 0) out float v_t;
void main()
{
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));   // (0,0) (2,0) (0,2)
    v_t = p.y;                                                                    // Vulkan: y grows downward
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}