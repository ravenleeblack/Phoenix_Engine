#version 450
layout(push_constant) uniform PC { vec4 top; vec4 bottom; } pc;   // 32 bytes, fragment stage only
layout(location = 0) in float v_t;
layout(location = 0) out vec4 out_color;
void main()
{
    out_color = vec4(mix(pc.top.rgb, pc.bottom.rgb, clamp(v_t, 0.0, 1.0)), 1.0);
}