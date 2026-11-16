#version 450

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec3 v_view_pos;
layout(location = 2) flat in vec4 v_light;
layout(location = 3) flat in vec4 v_sun;
layout(location = 4) in vec2 v_uv;          // only read by the terrain cell-grid wire (light.w < -1.5)

layout(location = 0) out vec4 out_color;

// skin data: one uniform buffer per skin (system_skin_gpu.c). Must match SKIN_UBO layout there: 16 + 16 * 32 bytes.
//   head.x = layer count, head.y = 1 -> no skin: keep the vertex colour (what every object did before skins)
//   layer.color  = rgb + alpha (alpha already multiplied by the layer opacity)
//   layer.params = x blend (0 lerp, 1 multiply, 2 overlay, 3 add), y output bit mask (bit 0 = Color), z triplanar, w unused
struct skin_layer {
    vec4 color;
    vec4 params;
};

layout(set = 0, binding = 0, std140) uniform skin_block {
    uvec4     head;
    skin_layer layers[16];
} skin;

layout(set = 0, binding = 1) uniform sampler2D skin_map;   // base color map. head.z = 1 when the skin has one, otherwise a 1x1 white texture is bound

vec3 blend_layer(vec3 base, vec3 src, float mode, float amount)
{
    vec3 result;
    if (mode < 0.5)      result = src;                                                       // lerp
    else if (mode < 1.5) result = base * src;                                                // multiply
    else if (mode < 2.5) result = mix(2.0 * base * src,                                      // overlay
                                      1.0 - 2.0 * (1.0 - base) * (1.0 - src),
                                      step(vec3(0.5), base));
    else                 result = base + src;                                                // add
    return mix(base, clamp(result, 0.0, 1.0), amount);
}

// Colour of the surface before lighting: the skin's layers evaluated bottom to top, or the vertex colour.
vec4 surface_color()
{
    if (skin.head.y != 0u) return v_color;

    uint  count = min(skin.head.x, 16u);
    vec3  c     = vec3(0.0);
    bool  have  = false;
    vec3  map_color = vec3(1.0);
    if (skin.head.z != 0u) {
        map_color = texture(skin_map, v_uv).rgb;                 // the map multiplies the first color layer (white = the picture as is)
    }
    for (uint i = 0u; i < count; ++i) {
        uint out_mask = uint(skin.layers[i].params.y + 0.5);
        if ((out_mask & 1u) == 0u) continue;                      // this layer does not write Color
        vec3  src    = skin.layers[i].color.rgb;
        float amount = clamp(skin.layers[i].color.a, 0.0, 1.0);
        if (!have) { c = src * map_color; have = true; continue; }   // the first colour layer is the base
        c = blend_layer(c, src, skin.layers[i].params.x, amount);
    }
    if (!have) {
        if (skin.head.z != 0u) return vec4(map_color, v_color.a);
        return v_color;                                           // a skin that writes no colour: keep the vertex colour
    }
    return vec4(c, v_color.a);
}

void main()
{
    // Flat colour override (wireframe lines): light.xyz = 0 and light.w < 0, colour comes in v_sun.rgb
    if (dot(v_light.xyz, v_light.xyz) < 1e-6 && v_light.w < 0.0) {
        // light.w < -1.5: terrain cell grid. The triangles are drawn FILLED and every fragment that is not on a
        // cell border is discarded, so only real cube edges show (no triangle diagonals, no doubled lines).
        // Terrain uvs are whole numbers exactly on cell borders (sandbox 0/1 per face, smooth 0/1 parity, far LOD i,j).
        if (v_light.w < -1.5) {
            vec2 g  = abs(fract(v_uv - 0.5) - 0.5);      // uv distance to the nearest cell border
            vec2 fw = max(fwidth(v_uv), vec2(1e-6));     // uv per pixel
            vec2 px = g / fw;                            // the same distance in pixels
            // Line thickness: ~1 px, measured in screen pixels so it does not grow or shrink with distance.
            // Fade instead of a hard cut: a direction whose cells are under 2 px is gone, over 8 px it is solid, in between it
            // is thinned out with a screen-space dither. The old hard cut (fw < 0.5) switched whole directions off at one
            // distance, so far away only one set of lines survived: the stripes / fan / wedge on the horizon, and a moire
            // where thousands of 4 mesh cells fall inside a few pixels.
            vec2 cell_px = vec2(1.0) / fw;                                   // pixels per cell, each direction on its own
            vec2 vis     = smoothstep(vec2(2.0), vec2(8.0), cell_px);        // 0 .. 1
            float dither = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));   // 0..1, screen-space noise
            bool on_u = (px.x <= 0.75) && (vis.x > dither);  // line of constant u (runs along the cell's v direction)
            bool on_v = (px.y <= 0.75) && (vis.y > dither);  // line of constant v
            if (!on_u && !on_v) discard;
        }
        out_color = vec4(v_sun.rgb, 1.0);
        return;
    }

    vec4 base = surface_color();

    // Unlit (Unl button, camera glyphs, wire helpers): plain surface colour.
    if (dot(v_light.xyz, v_light.xyz) < 1e-6) {
        out_color = base;
        return;
    }

    // Flat face normal from the screen-space derivatives of the view-space position.
    vec3 face = cross(dFdx(v_view_pos), dFdy(v_view_pos));
    float len = length(face);
    if (len < 1e-12) {                       // degenerate (edge-on / line): no usable normal
        out_color = base;
        return;
    }
    vec3 n = face / len;
    if (dot(n, -v_view_pos) < 0.0) n = -n;   // always face the camera: works for any winding / Y flip

    float facing = max(dot(n, normalize(v_light.xyz)), 0.0);

    // ambient (grey fill) + sun (coloured, brightness-scaled), applied once to the skin colour
    vec3 lighting = vec3(v_light.w) + v_sun.rgb * facing;
    out_color = vec4(base.rgb * lighting, base.a);
}