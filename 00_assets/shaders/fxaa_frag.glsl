#version 450

// FXAA (compact luma-edge version). Reads the resolved scene image, writes the smoothed image.
layout(set = 0, binding = 0) uniform sampler2D u_scene;
layout(push_constant) uniform PC { vec2 inv_res; } pc;   // 1/width, 1/height of the scene image

layout(location = 0) in  vec2 v_uv;
layout(location = 0) out vec4 out_color;

const float FXAA_REDUCE_MIN = 1.0 / 128.0;
const float FXAA_REDUCE_MUL = 1.0 / 8.0;
const float FXAA_SPAN_MAX   = 8.0;

void main()
{
    vec2 px = pc.inv_res;
    vec3 luma_w = vec3(0.299, 0.587, 0.114);

    vec3 rgb_nw = texture(u_scene, v_uv + vec2(-1.0, -1.0) * px).rgb;
    vec3 rgb_ne = texture(u_scene, v_uv + vec2( 1.0, -1.0) * px).rgb;
    vec3 rgb_sw = texture(u_scene, v_uv + vec2(-1.0,  1.0) * px).rgb;
    vec3 rgb_se = texture(u_scene, v_uv + vec2( 1.0,  1.0) * px).rgb;
    vec4 center = texture(u_scene, v_uv);
    vec3 rgb_m  = center.rgb;

    float l_nw = dot(rgb_nw, luma_w);
    float l_ne = dot(rgb_ne, luma_w);
    float l_sw = dot(rgb_sw, luma_w);
    float l_se = dot(rgb_se, luma_w);
    float l_m  = dot(rgb_m,  luma_w);

    float l_min = min(l_m, min(min(l_nw, l_ne), min(l_sw, l_se)));
    float l_max = max(l_m, max(max(l_nw, l_ne), max(l_sw, l_se)));

    // edge direction
    vec2 dir;
    dir.x = -((l_nw + l_ne) - (l_sw + l_se));
    dir.y =  ((l_nw + l_sw) - (l_ne + l_se));

    float dir_reduce = max((l_nw + l_ne + l_sw + l_se) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcp_dir_min = 1.0 / (min(abs(dir.x), abs(dir.y)) + dir_reduce);
    dir = min(vec2(FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX), dir * rcp_dir_min)) * px;

    vec3 rgb_a = 0.5 * (texture(u_scene, v_uv + dir * (1.0 / 3.0 - 0.5)).rgb +
                        texture(u_scene, v_uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgb_b = rgb_a * 0.5 + 0.25 * (texture(u_scene, v_uv + dir * -0.5).rgb +
                                       texture(u_scene, v_uv + dir *  0.5).rgb);

    float l_b = dot(rgb_b, luma_w);
    vec3 result = (l_b < l_min || l_b > l_max) ? rgb_a : rgb_b;
    out_color = vec4(result, 1.0);
}