#version 450

// Scene meshes (cube, terrain chunks, camera glyphs, helpers).
// Vertex format = editor_vertex, stride 48 bytes:
//   location 0  position  vec3   offset  0
//   location 1  colour    vec4   offset 12
//   location 2  uv        vec2   offset 28
//   location 3  normal    vec3   offset 36   (object space; zero = mesh has no normal)
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_normal;

// 160 bytes: modelview + proj + light + sun colour.  Must match MESH_PC_SIZE in system_scene_draw.c.
layout(push_constant) uniform PC {
    mat4 modelview;
    mat4 proj;
    vec4 light;   // xyz = view-space direction TOWARD the sun, w = ambient. xyz == 0 -> draw the plain vertex colour
    vec4 sun;     // rgb = sun tint * brightness (already scaled by 1 - ambient)
} pc;

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec3 v_view_pos;
layout(location = 2) flat out vec4 v_light;
layout(location = 3) flat out vec4 v_sun;
layout(location = 4) out vec2 v_uv;
layout(location = 5) out vec3 v_normal;   // view space, NOT normalised here. zero = no normal supplied

void main()
{
    vec4 view_pos = pc.modelview * vec4(in_pos, 1.0);
    v_view_pos = view_pos.xyz;
    v_color    = in_color;
    v_uv       = in_uv;
    v_light    = pc.light;
    v_sun      = pc.sun;

    // Normal matrix without an inverse: the cofactor matrix of the upper 3x3 equals
    // det * transpose(inverse(M)), so it stays correct under non-uniform scale.
    // A mirrored (negative) scale flips the sign; the fragment shader turns the normal
    // toward the camera anyway, so that is harmless. A zero input normal stays zero.
    mat3 m = mat3(pc.modelview);
    mat3 normal_matrix = mat3(cross(m[1], m[2]),
                              cross(m[2], m[0]),
                              cross(m[0], m[1]));
    v_normal = normal_matrix * in_normal;

    gl_Position = pc.proj * view_pos;
}