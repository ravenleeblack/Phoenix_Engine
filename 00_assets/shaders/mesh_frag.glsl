#version 450

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec3 v_view_pos;
layout(location = 2) flat in vec4 v_light;
layout(location = 3) flat in vec4 v_sun;
layout(location = 4) in vec2 v_uv;          // terrain cell-grid wire (light.w < -1.5) and skin maps
layout(location = 5) in vec3 v_normal;      // view space, not normalised. zero = the mesh has no normals. smooth terrain material only

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
    uvec4     head;          // .w = aux map flags: bit 0 specular map, bit 1 height map, bit 2 ambient map
    skin_layer layers[16];
    vec4      surface;       // x specular strength, y gloss, z height (bump) scale, w ambient multiplier
} skin;

layout(set = 0, binding = 1) uniform sampler2D skin_map;   // base color map. head.z = 1 when the skin has one, otherwise a 1x1 white texture is bound
layout(set = 0, binding = 2) uniform sampler2D skin_aux;   // r = specular, g = height, b = ambient occlusion (white when absent)

// scene lights (system_light_gpu.c): one block per draw call, set 1
//   pos_range.xyz  view space position, w range           color_type.rgb  color * intensity, w kind (0 directional, 1 point, 2 spot)
//   dir_cone.xyz   directional: toward the light, spot: direction the spot points, w cos of the outer cone
//   extra.x        cos of the inner cone
struct scene_light {
    vec4 pos_range;
    vec4 color_type;
    vec4 dir_cone;
    vec4 extra;
};

layout(set = 1, binding = 0, std140) uniform light_block {
    vec4        counts;      // x = light count, yzw = world up in view space
    vec4        hemi_sky;    // rgb sky color, w = 1 when a hemisphere light is on
    vec4        hemi_ground;
    scene_light lights[8];
    // sky and fog (system_sky.c), 15 vec4 after the lights
    vec4        fog_a;       // rgb fog color (linear), w = 1 when fog is on in this view
    vec4        fog_b;       // x density per meter, y height falloff per meter, z base height, w start distance
    vec4        fog_c;       // x camera world height, y max opacity, z sun glow strength, w sun glow exponent
    vec4        sky_top;     // rgb sky color overhead
    vec4        sky_horizon; // rgb sky color at the horizon
    vec4        sun_dir;     // xyz view space direction toward the sun, w = 1 when a sun is on
    vec4        sun_color;   // rgb sun color * brightness
    vec4        vol_center[4]; // fog volume spheres: xyz view space center, w radius (0 = unused)
    vec4        vol_color[4];  // rgb color, w density per meter at the middle of the sphere
} scene_lights;

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

// Glow toward the sun: fog and volumes catch the sun like Unreal's directional inscattering.
vec3 fog_inscatter(vec3 fog_rgb, vec3 dir)
{
    float glow = 0.0;
    if (scene_lights.sun_dir.w > 0.5) {
        float facing = max(dot(dir, normalize(scene_lights.sun_dir.xyz)), 0.0);
        glow = pow(facing, max(scene_lights.fog_c.w, 1.0)) * scene_lights.fog_c.z;
    }
    return fog_rgb + scene_lights.sun_color.rgb * glow;
}

// Fog: exponential height fog (density falls off with height, integrated along the whole ray like Unreal's
// Exponential Height Fog), then every fog volume: a sphere that is thickest in the middle and fades to nothing at its edge.
vec3 apply_fog(vec3 color, vec3 view_pos)
{
    if (scene_lights.fog_a.w < 0.5) return color;

    float dist = length(view_pos);
    vec3  dir  = view_pos / max(dist, 1e-4);

    float falloff = max(scene_lights.fog_b.y, 1e-4);
    float dir_y   = dot(dir, scene_lights.counts.yzw);                     // how much the ray climbs
    float low     = clamp(-falloff * (scene_lights.fog_c.x - scene_lights.fog_b.z), -20.0, 20.0);
    float length_in_fog = max(dist - scene_lights.fog_b.w, 0.0);
    float climb   = falloff * dir_y * length_in_fog;
    float depth   = length_in_fog;                                         // integral of exp(-falloff * height) along the ray
    if (abs(climb) > 1e-3) {
        depth = (1.0 - exp(clamp(-climb, -20.0, 20.0))) / (falloff * dir_y);
    }
    float tau     = scene_lights.fog_b.x * exp(low) * depth;
    float amount  = clamp((1.0 - exp(-tau)) * scene_lights.fog_c.y, 0.0, 1.0);
    color = mix(color, fog_inscatter(scene_lights.fog_a.rgb, dir), amount);

    for (int i = 0; i < 4; ++i) {
        vec4 c = scene_lights.vol_center[i];
        if (c.w <= 0.0) continue;
        float b    = dot(c.xyz, dir);
        float h2   = dot(c.xyz, c.xyz) - b * b;                            // squared distance of the ray from the center
        float r2   = c.w * c.w;
        if (h2 >= r2) continue;
        float s  = sqrt(r2 - h2);
        float t0 = max(b - s, 0.0);
        float t1 = min(b + s, dist);
        if (t1 <= t0) continue;
        // density = d * (1 - distance_from_center^2 / radius^2), integrated exactly between t0 and t1
        float a0 = t0 - b;
        float a1 = t1 - b;
        float inside = (1.0 - h2 / r2) * (t1 - t0) - (a1 * a1 * a1 - a0 * a0 * a0) / (3.0 * r2);
        float a = 1.0 - exp(-scene_lights.vol_color[i].w * max(inside, 0.0));
        color = mix(color, fog_inscatter(scene_lights.vol_color[i].rgb, dir), clamp(a, 0.0, 1.0));
    }
    return color;
}

// Sky dome color for a view direction: horizon to zenith gradient, darker below the horizon, a soft glow and a disc at the sun.
vec3 sky_color(vec3 dir)
{
    float up      = dot(dir, scene_lights.counts.yzw);
    vec3  horizon = mix(scene_lights.sky_horizon.rgb, scene_lights.fog_a.rgb, scene_lights.fog_a.w);
    vec3  color   = mix(horizon, scene_lights.sky_top.rgb, pow(clamp(up, 0.0, 1.0), 0.5));
    if (up < 0.0) color = horizon * mix(1.0, 0.6, clamp(-up * 3.0, 0.0, 1.0));
    if (scene_lights.sun_dir.w > 0.5) {
        float facing = max(dot(dir, normalize(scene_lights.sun_dir.xyz)), 0.0);
        color += scene_lights.sun_color.rgb * (pow(facing, 4.0) * 0.10 + pow(facing, 48.0) * 0.30);
        color += scene_lights.sun_color.rgb * smoothstep(0.9990, 0.9997, facing) * 4.0;
    }
    return color;
}

void main()
{
    // Sky dome: light.xyz = 0 and light.w < -2.5.
    if (dot(v_light.xyz, v_light.xyz) < 1e-6 && v_light.w < -2.5) {
        out_color = vec4(sky_color(normalize(v_view_pos)), 1.0);
        return;
    }

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
        out_color = vec4(apply_fog(base.rgb, v_view_pos), base.a);
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

    vec3 view_dir = normalize(-v_view_pos);
    bool  has_skin = (skin.head.y == 0u);
    uint  flags    = skin.head.w;

    // terrain material (skin named Terrain, flag bit 3): the surface keeps its vertex colour (what sculpting and painting write),
    // tinted by layer 0, and is shaded with the smooth vertex normals so the ground has no faceting and carries a soft sheen.
    bool terrain_material = has_skin && (flags & 8u) != 0u;
    if (terrain_material) {
        if (dot(v_normal, v_normal) > 1e-8) {
            n = normalize(v_normal);
            if (dot(n, -v_view_pos) < 0.0) n = -n;
        }
        base = vec4(v_color.rgb * skin.layers[0].color.rgb, v_color.a);
    }
    vec3  aux      = vec3(1.0);
    if (has_skin && (flags & 7u) != 0u) {
        aux = texture(skin_aux, v_uv).rgb;
    }

    // height map -> bump: perturb the normal with the screen space gradient of the height (no tangents needed)
    if (has_skin && (flags & 2u) != 0u && skin.surface.z > 0.0) {
        float h   = aux.g;
        vec3  dpx = dFdx(v_view_pos);
        vec3  dpy = dFdy(v_view_pos);
        vec3  r1  = cross(dpy, n);
        vec3  r2  = cross(n, dpx);
        float det = dot(dpx, r1);
        vec3  grad = sign(det) * (dFdx(h) * r1 + dFdy(h) * r2);
        n = normalize(abs(det) * n - skin.surface.z * 4.0 * grad);
    }

    float spec_strength = 0.0;
    float shininess     = 32.0;
    float ambient_k     = 1.0;
    float ao            = 1.0;
    if (has_skin) {
        spec_strength = skin.surface.x;
        if ((flags & 1u) != 0u) spec_strength *= aux.r;
        shininess = exp2(skin.surface.y * 7.0 + 1.0);
        ambient_k = skin.surface.w;
        if ((flags & 4u) != 0u) ao = aux.b;
    }

    // ambient (grey fill) + hemisphere light, scaled by the skin's ambient value and ambient map
    // the ambient value is a soft sky / ground gradient (surfaces facing up get more, facing down less) instead of one flat grey
    float up_t = dot(n, scene_lights.counts.yzw) * 0.5 + 0.5;
    vec3 amb = vec3(v_light.w) * mix(vec3(0.66, 0.64, 0.62), vec3(1.0, 1.0, 1.04), up_t);
    if (scene_lights.hemi_sky.w > 0.5) {
        float t = dot(n, scene_lights.counts.yzw) * 0.5 + 0.5;
        amb += mix(scene_lights.hemi_ground.rgb, scene_lights.hemi_sky.rgb, t);
    }
    vec3 diffuse  = amb * ambient_k * ao;
    vec3 specular = vec3(0.0);

    // the sun (coloured, brightness-scaled)
    vec3  sun_l  = normalize(v_light.xyz);
    float facing = max(dot(n, sun_l), 0.0);
    diffuse += v_sun.rgb * facing;
    if (facing > 0.0 && spec_strength > 0.0) {
        specular += v_sun.rgb * pow(max(dot(n, normalize(sun_l + view_dir)), 0.0), shininess) * spec_strength;
    }

    // directional / point / spot light objects
    int count = int(scene_lights.counts.x + 0.5);
    for (int i = 0; i < count && i < 8; ++i) {
        scene_light lt   = scene_lights.lights[i];
        int         kind = int(lt.color_type.w + 0.5);
        vec3  l   = normalize(lt.dir_cone.xyz);
        float att = 1.0;
        if (kind != 0) {
            vec3  to_light = lt.pos_range.xyz - v_view_pos;
            float dist     = length(to_light);
            float ratio    = dist / max(lt.pos_range.w, 0.001);
            float falloff  = clamp(1.0 - ratio * ratio, 0.0, 1.0);
            l   = to_light / max(dist, 1e-4);
            att = falloff * falloff;
            if (kind == 3) {
                att = falloff;                    // area light: a soft panel, no hot spot
            }
            if (kind == 2) {
                float c = dot(-l, normalize(lt.dir_cone.xyz));
                att *= smoothstep(lt.dir_cone.w, lt.extra.x, c);
            }
        }
        float nl = max(dot(n, l), 0.0);
        if (kind == 3) {
            nl = clamp((dot(n, l) + 0.5) / 1.5, 0.0, 1.0);   // wraps around the surface instead of a hard terminator
        }
        vec3  lc = lt.color_type.rgb * att;
        diffuse += lc * nl;
        if (nl > 0.0 && spec_strength > 0.0) {
            specular += lc * pow(max(dot(n, normalize(l + view_dir)), 0.0), shininess) * spec_strength;
        }
    }

    // natural sheen on the terrain: a faint sky reflection that grows toward grazing angles, so slopes catch light like Unity / Unreal ground
    if (terrain_material) {
        float fres = pow(1.0 - clamp(dot(n, view_dir), 0.0, 1.0), 4.0);
        vec3  sky  = vec3(0.55, 0.65, 0.80) * clamp(v_light.w + 0.25, 0.0, 1.0);
        specular += sky * fres * spec_strength * 0.9;
    }

    out_color = vec4(apply_fog(base.rgb * diffuse + specular, v_view_pos), base.a);
}