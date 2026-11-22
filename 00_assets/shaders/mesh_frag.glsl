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
layout(set = 0, binding = 3) uniform sampler2D skin_normal; // tangent space normal map (flag bit 4)
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
    // sky and fog (system_sky.c), 27 vec4 after the lights
    vec4        fog_a;       // rgb fog color (linear, used when sky_a.z = 0), w = 1 when fog is on in this view
    vec4        fog_b;       // x density per meter, y height falloff per meter, z base height, w start distance
    vec4        fog_c;       // x camera world height, y max opacity, z sun glow strength, w sun glow exponent
    vec4        sky_a;       // x sky brightness, y haze, z = 1 when the fog takes the sky's horizon color, w time in seconds
    vec4        sun_dir;     // xyz view space direction toward the sun, w = 1 when a sun is on
    vec4        sun_color;   // rgb sun color * brightness * (1 - ambient), w = the scene ambient
    vec4        sun_tint;    // rgb the sun color with its warm / cool tint, w = the sun intensity (no ambient split)
    vec4        vol_color[4];  // fog volume: rgb color, w density per meter
    vec4        vol_params[4]; // x = 1 when used, y type (0 cheap, 1 good), z edge softness, w noise cells across the box
    vec4        vol_rows[12];  // per volume 3 rows (xyz, w): view space point -> the box's own space where the box is -1..1
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

// ---------------------------------------------------------------------------------------------------------------
// The directional light is the source. Everything below is made FROM its color, intensity and direction:
//   sky   the sun's light scattered by the air (blue overhead, pale and orange toward the horizon, dark at night)
//   fog   a medium that takes light OUT of the view (extinction) and reflects the sun and the sky light BACK toward the
//         camera (in-scattering, with a forward-scattering phase function). fog color is only its albedo.
// Nothing here dims or tints the scene: surfaces are lit by the lights, the fog only adds what it reflects.
// ---------------------------------------------------------------------------------------------------------------
float sky_sun_height()                      // sine of the sun's elevation: 1 = overhead, 0 = on the horizon, < 0 = night
{
    if (scene_lights.sun_dir.w > 0.5) return dot(normalize(scene_lights.sun_dir.xyz), scene_lights.counts.yzw);
    return 0.8;
}

vec3 sky_sun_dir()
{
    if (scene_lights.sun_dir.w > 0.5) return normalize(scene_lights.sun_dir.xyz);
    return scene_lights.counts.yzw;
}

// the sun's own color (with its warm / cool tint) times its intensity
vec3 sun_full_color()
{
    return scene_lights.sun_tint.rgb * scene_lights.sun_tint.w;
}

// the sunlight that reaches the air and the ground: reddened and dimmed by the long path through the atmosphere when the sun is low, gone at night
vec3 sun_light_at_ground()
{
    float sun_h = sky_sun_height();
    float air   = 1.0 / (max(sun_h, 0.0) + 0.08);
    vec3  atmosphere = exp(-vec3(0.045, 0.10, 0.24) * air);
    float rise  = smoothstep(-0.08, 0.12, sun_h);
    if (scene_lights.sun_dir.w < 0.5) rise = 1.0;
    return sun_full_color() * atmosphere * rise;
}

// Rayleigh scattering: short (blue) wavelengths scatter most. the longer the path through the air (toward the horizon) the paler it gets.
vec3 sky_scatter(float up, float mu, vec3 sunlight, float haze)
{
    float airmass = 1.0 / (max(up, 0.0) + 0.10);
    vec3  beta    = vec3(0.20, 0.45, 1.00);
    vec3  scatter = (vec3(1.0) - exp(-beta * airmass)) * 1.25;
    float rayleigh_phase = 0.75 * (1.0 + mu * mu);
    vec3  sky = sunlight * scatter * rayleigh_phase;
    sky += sunlight * haze * 0.30 * exp(-max(up, 0.0) * 2.5);          // haze: a whiter, brighter horizon
    return sky;
}

// the light of the sky in a direction. with_sun = false leaves out the glow around the sun (used as the fog's ambient light)
vec3 sky_radiance(vec3 dir, bool with_sun)
{
    float up    = dot(dir, scene_lights.counts.yzw);
    float sun_h = sky_sun_height();
    float mu    = dot(dir, sky_sun_dir());
    float haze  = scene_lights.sky_a.y;
    vec3  sunlight = sun_light_at_ground();

    vec3 sky     = sky_scatter(up, mu, sunlight, haze);
    vec3 horizon = sky_scatter(0.0, mu, sunlight, haze);
    if (up < 0.0) sky = mix(sky, horizon * 0.7, clamp(-up * 1.5, 0.0, 1.0));      // below the horizon the haze fades slowly, so the far ground meets the sky without a dark band

    if (with_sun) {
        sky += sunlight * pow(max(mu, 0.0), mix(24.0, 6.0, haze)) * mix(0.25, 0.6, haze);          // the glow around the sun
        if (scene_lights.sun_dir.w > 0.5) sky += sunlight * smoothstep(0.9993, 0.9997, mu) * 5.0;  // the sun itself
    }
    float night = 1.0 - smoothstep(-0.12, 0.25, sun_h);
    sky += vec3(0.004, 0.007, 0.018) * night;                                                      // faint starlight
    return sky * scene_lights.sky_a.x;
}

vec3 sky_color(vec3 dir)
{
    vec3 sky = sky_radiance(dir, true);

    // stars at night
    float up    = dot(dir, scene_lights.counts.yzw);
    float night = 1.0 - smoothstep(-0.12, 0.25, sky_sun_height());
    vec3  cell  = floor(dir * 220.0);
    float star  = step(0.9965, fract(sin(dot(cell, vec3(12.9898, 78.233, 37.719))) * 43758.5453));
    sky += vec3(star) * night * smoothstep(0.05, 0.3, up) * 0.8 * scene_lights.sky_a.x;
    return sky;
}

// ---------------------------------------------------------------------------------------------------------------
// Fog
// ---------------------------------------------------------------------------------------------------------------
// the light that falls on fog and gets reflected toward the camera: the sky's light, plus the sun scattered with a forward-biased
// phase function (fog glows when you look toward the sun, and is much dimmer looking away from it)
vec3 fog_lighting(vec3 dir)
{
    float up       = dot(dir, scene_lights.counts.yzw);
    vec3  flat_dir = normalize(dir - scene_lights.counts.yzw * up + scene_lights.counts.yzw * 0.03);
    vec3  ambient  = sky_radiance(flat_dir, false);

    vec3 sun = vec3(0.0);
    if (scene_lights.sun_dir.w > 0.5) {
        float g  = clamp(scene_lights.fog_c.w, 0.0, 0.95);
        float mu = dot(dir, sky_sun_dir());
        float hg = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * mu, 1.5);       // average 1 over the sphere
        sun = sun_light_at_ground() * mix(1.0, hg, 0.7) * scene_lights.fog_c.z;
    }
    return ambient + sun;
}

float hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float value_noise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash13(i),                    hash13(i + vec3(1.0, 0.0, 0.0)), f.x),
                   mix(hash13(i + vec3(0.0, 1.0, 0.0)), hash13(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
               mix(mix(hash13(i + vec3(0.0, 0.0, 1.0)), hash13(i + vec3(1.0, 0.0, 1.0)), f.x),
                   mix(hash13(i + vec3(0.0, 1.0, 1.0)), hash13(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);
}

float fog_noise(vec3 p)
{
    return value_noise(p) * 0.65 + value_noise(p * 2.07 + vec3(7.3, 1.7, 4.1)) * 0.35;
}

// Fog volumes: boxes (any size and rotation) filled with fog. The fog takes light out of the view and reflects light back.
//   cheap: one straight pass through the box, soft edge.
//   good:  ray marched through the box with moving noise, thicker toward the bottom, with a little self shadow.
vec3 apply_volumes(vec3 color, vec3 dir, float dist)
{
    vec3 lighting = fog_lighting(dir);
    for (int i = 0; i < 4; ++i) {
        vec4 params = scene_lights.vol_params[i];
        if (params.x < 0.5) continue;

        vec4 r0 = scene_lights.vol_rows[i * 3];
        vec4 r1 = scene_lights.vol_rows[i * 3 + 1];
        vec4 r2 = scene_lights.vol_rows[i * 3 + 2];
        vec3 o  = vec3(r0.w, r1.w, r2.w);
        vec3 d  = vec3(dot(r0.xyz, dir), dot(r1.xyz, dir), dot(r2.xyz, dir));

        vec3 side = step(vec3(0.0), d) * 2.0 - 1.0;
        vec3 inv  = 1.0 / (d + side * 1e-6);
        vec3 t0   = (vec3(-1.0) - o) * inv;
        vec3 t1   = (vec3(1.0) - o) * inv;
        vec3 tn   = min(t0, t1);
        vec3 tf   = max(t0, t1);
        float enter = max(max(tn.x, tn.y), max(tn.z, 0.0));
        float leave = min(min(tf.x, tf.y), min(tf.z, dist));
        if (leave <= enter) continue;

        vec3  reflected = scene_lights.vol_color[i].rgb * lighting;        // albedo * the light that falls on it
        float density   = scene_lights.vol_color[i].w;
        float soft      = max(params.z, 0.02);

        if (params.y < 0.5) {
            vec3  mid  = o + d * (0.5 * (enter + leave));
            float edge = 1.0 - max(max(abs(mid.x), abs(mid.y)), abs(mid.z));
            float tau  = density * smoothstep(0.0, soft, edge) * (leave - enter);
            color = mix(color, reflected, 1.0 - exp(-tau));
        } else {
            const int steps = 24;
            float dt     = (leave - enter) / float(steps);
            float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
            float trans  = 1.0;
            vec3  scatter = vec3(0.0);
            vec3  drift   = vec3(scene_lights.sky_a.w * 0.04, 0.0, scene_lights.sky_a.w * 0.025);
            for (int s = 0; s < steps; ++s) {
                vec3  p     = o + d * (enter + dt * (float(s) + jitter));
                float edge  = 1.0 - max(max(abs(p.x), abs(p.y)), abs(p.z));
                float fade  = smoothstep(0.0, soft, edge);
                float low   = mix(1.0, 0.35, p.y * 0.5 + 0.5);
                float n     = fog_noise(p * params.w + drift);
                float here  = density * fade * low * (0.25 + 1.5 * n);
                float a     = 1.0 - exp(-here * dt);
                scatter    += trans * a * reflected * mix(1.0, 0.65, clamp(here * dt * 4.0, 0.0, 1.0));
                trans      *= 1.0 - a;
            }
            color = color * trans + scatter;
        }
    }
    return color;
}

// Fog: exponential height fog (density falls off with height, integrated along the whole ray like Unreal's
// Exponential Height Fog), then the fog volumes.
// extinction: how much of the scene is hidden. in-scattering: the sun and sky light the fog reflects toward the camera.
// Aerial perspective: with the sky on, far surfaces take on the color of the sky at the horizon, so the end of the ground melts into the sky
// instead of cutting a hard line. It grows with distance and is strongest looking along the ground.
vec3 apply_horizon(vec3 color, vec3 view_pos)
{
    float dist = length(view_pos);
    vec3  dir  = view_pos / max(dist, 1e-4);
    float up   = dot(dir, scene_lights.counts.yzw);
    vec3  flat_dir = normalize(dir - scene_lights.counts.yzw * up + scene_lights.counts.yzw * 0.02);
    vec3  haze = sky_radiance(flat_dir, false);
    float amount = 1.0 - exp(-pow(dist * 0.0011, 1.6));
    amount *= 1.0 - smoothstep(0.15, 0.75, abs(up));                 // looking down at the ground right below: no haze
    return mix(color, haze, clamp(amount, 0.0, 0.92));
}

vec3 apply_fog(vec3 color, vec3 view_pos)
{
    if (scene_lights.fog_a.w >= 1.5) {
        color = apply_horizon(color, view_pos);
    }
    if (mod(scene_lights.fog_a.w, 2.0) < 0.5) return color;

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
    float tau          = scene_lights.fog_b.x * exp(low) * depth;
    float amount       = clamp((1.0 - exp(-tau)) * scene_lights.fog_c.y, 0.0, 1.0);
    vec3  reflected    = scene_lights.fog_a.rgb * fog_lighting(dir);        // albedo * the light that falls on the fog
    color = color * (1.0 - amount) + reflected * amount;

    return apply_volumes(color, dir, dist);
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
    // terrain vertices carry the cell coordinate as uv (one unit per cell): the maps repeat every 8 cells (32 m)
    vec2 tex_uv = v_uv;
    if (terrain_material) {
        tex_uv = v_uv * 0.125;
        if (skin.head.z != 0u) {
            base.rgb *= texture(skin_map, tex_uv).rgb * 1.6;       // the picture tints the painted ground (mid grey = unchanged)
        }
    }
    vec4  aux4     = vec4(1.0);
    vec3  aux      = vec3(1.0);
    if (has_skin && (flags & 39u) != 0u) {
        aux4 = texture(skin_aux, tex_uv);
        aux  = aux4.rgb;
    }

    // normal map: a tangent frame from the screen space derivatives of the position and the uv (no tangents in the mesh)
    if (has_skin && (flags & 16u) != 0u) {
        vec3 tn  = texture(skin_normal, tex_uv).xyz * 2.0 - 1.0;
        tn.xy   *= max(skin.surface.z * 2.0, 0.25);                // Height value is also the normal map strength
        vec3 dp1 = dFdx(v_view_pos);
        vec3 dp2 = dFdy(v_view_pos);
        vec2 du1 = dFdx(tex_uv);
        vec2 du2 = dFdy(tex_uv);
        vec3 dp2perp = cross(dp2, n);
        vec3 dp1perp = cross(n, dp1);
        vec3 tangent   = dp2perp * du1.x + dp1perp * du2.x;
        vec3 bitangent = dp2perp * du1.y + dp1perp * du2.y;
        float inv = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-12));
        n = normalize(tangent * (tn.x * inv) + bitangent * (-tn.y * inv) + n * tn.z);
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
        if ((flags & 32u) != 0u) {
            ao *= aux4.a;                                         // the occlusion map darkens ambient light ...
            spec_strength *= mix(0.4, 1.0, aux4.a);               // ... and the highlights in the cracks
        }
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