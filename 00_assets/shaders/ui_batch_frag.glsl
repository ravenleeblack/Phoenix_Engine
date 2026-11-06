#version 450
layout(location = 0) flat in vec4  v_color;
layout(location = 1)      in vec2  v_local;
layout(location = 2) flat in float v_half;
layout(location = 0) out vec4 out_color;

// blue >= 2 marks a circle (see push_circle_element in system_element.c):
//   blue  = real blue + 2
//   green = real green + 2 * k        k = thickness in quarter pixels, k = 0 is a filled disc
// alpha is the normal opacity. the edge is computed per pixel from the distance to the center: a true anti aliased circle at any size.
void main()
{
    if (v_color.b < 2.0) { out_color = v_color; return; }

    float k       = floor(v_color.g * 0.5);
    vec3  rgb     = vec3(v_color.r, v_color.g - k * 2.0, v_color.b - 2.0);
    float thick   = k * 0.25;
    float radius  = v_half - 1.0;
    float d       = length(v_local);

    float cover;
    if (thick <= 0.0) {
        cover = clamp(radius + 0.5 - d, 0.0, 1.0);                         // disc
    } else {
        float hw = thick * 0.5;                                            // ring: a band centered half a thickness inside the radius
        cover = clamp(hw + 0.5 - abs(d - (radius - hw)), 0.0, 1.0);
    }
    if (cover <= 0.0) discard;
    out_color = vec4(rgb, v_color.a * cover);
}