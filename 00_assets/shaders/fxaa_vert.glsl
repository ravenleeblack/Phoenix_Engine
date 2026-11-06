#version 450

// Fullscreen triangle generated from gl_VertexIndex: no vertex buffer. Draw with vkCmdDraw(cmd, 3, 1, 0, 0).
layout(location = 0) out vec2 v_uv;

void main()
{
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));   // (0,0) (2,0) (0,2)
    v_uv = p;                                                                      // (0,0) = top-left texel
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}