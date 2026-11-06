#include "engine_globals.h"


/* ========================================================================= */
/* registry of light objects. state lives in engine_globals.c                 */
/* ========================================================================= */
void remove_light_at(int index)
{
    for (int k = index; k < g_light_count - 1; k++) g_lights[k] = g_lights[k + 1];
    g_light_count--;
}

/* ========================================================================= */
/* glyph: every piece is convex, so triangle winding is fixed automatically   */
/* by pushing the face away from the piece's own centre.                      */
/* ========================================================================= */
int add_glyph_vertex(glyph* g, float x, float y, float z, const float color[3])
{
    editor_vertex* p = &g->vertices[g->vertex_count];
    p->x = x; p->y = y; p->z = z;
    p->r = color[0]; p->g = color[1]; p->b = color[2]; p->a = 1.0f;
    g->vertex_count++;
    return g->vertex_count - 1;
}

/* one triangle, wound counter-clockwise seen from outside of a convex piece centred on `center` */
void add_glyph_triangle(glyph* g, int a, int b, int c, const float center[3])
{
    const editor_vertex* va = &g->vertices[a];
    const editor_vertex* vb = &g->vertices[b];
    const editor_vertex* vc = &g->vertices[c];
    const float ux = vb->x - va->x, uy = vb->y - va->y, uz = vb->z - va->z;
    const float vx = vc->x - va->x, vy = vc->y - va->y, vz = vc->z - va->z;
    const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    const float mx = (va->x + vb->x + vc->x) / 3.0f - center[0];
    const float my = (va->y + vb->y + vc->y) / 3.0f - center[1];
    const float mz = (va->z + vb->z + vc->z) / 3.0f - center[2];
    if (nx * mx + ny * my + nz * mz < 0.0f) { int t = b; b = c; c = t; }
    g->indices[g->index_count++] = (uint32_t)a;
    g->indices[g->index_count++] = (uint32_t)b;
    g->indices[g->index_count++] = (uint32_t)c;
}

/* square pyramid: base centre, base half size, spread across axes (u,w), tip given */
void add_glyph_pyramid(glyph* g, const float base_center[3], const float axis_u[3], const float axis_w[3], float half_size,
                       const float tip[3], const float color[3])
{
    const float* bc = base_center;
    const float  h  = half_size;
    int b0 = add_glyph_vertex(g, bc[0] - axis_u[0]*h - axis_w[0]*h, bc[1] - axis_u[1]*h - axis_w[1]*h, bc[2] - axis_u[2]*h - axis_w[2]*h, color);
    int b1 = add_glyph_vertex(g, bc[0] + axis_u[0]*h - axis_w[0]*h, bc[1] + axis_u[1]*h - axis_w[1]*h, bc[2] + axis_u[2]*h - axis_w[2]*h, color);
    int b2 = add_glyph_vertex(g, bc[0] + axis_u[0]*h + axis_w[0]*h, bc[1] + axis_u[1]*h + axis_w[1]*h, bc[2] + axis_u[2]*h + axis_w[2]*h, color);
    int b3 = add_glyph_vertex(g, bc[0] - axis_u[0]*h + axis_w[0]*h, bc[1] - axis_u[1]*h + axis_w[1]*h, bc[2] - axis_u[2]*h + axis_w[2]*h, color);
    int t  = add_glyph_vertex(g, tip[0], tip[1], tip[2], color);
    const float center[3] = { (bc[0] + tip[0]) * 0.5f, (bc[1] + tip[1]) * 0.5f, (bc[2] + tip[2]) * 0.5f };
    add_glyph_triangle(g, b0, b1, b2, center);  add_glyph_triangle(g, b0, b2, b3, center);
    add_glyph_triangle(g, b0, b1, t, center);   add_glyph_triangle(g, b1, b2, t, center);
    add_glyph_triangle(g, b2, b3, t, center);   add_glyph_triangle(g, b3, b0, t, center);
}

void add_glyph_box(glyph* g, float x0, float y0, float z0, float x1, float y1, float z1, const float color[3])
{
    int p[8];
    for (int i = 0; i < 8; i++) {
        float px = x0;
        float py = y0;
        float pz = z0;
        if (i & 1) px = x1;
        if (i & 2) py = y1;
        if (i & 4) pz = z1;
        p[i] = add_glyph_vertex(g, px, py, pz, color);
    }
    const float center[3] = { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f };
    const int q[6][4] = { {0,1,3,2}, {4,5,7,6}, {0,1,5,4}, {2,3,7,6}, {0,2,6,4}, {1,3,7,5} };
    for (int f = 0; f < 6; f++) {
        add_glyph_triangle(g, p[q[f][0]], p[q[f][1]], p[q[f][2]], center);
        add_glyph_triangle(g, p[q[f][0]], p[q[f][2]], p[q[f][3]], center);
    }
}

void build_sun_glyph(glyph* g, float scale)
{
    const float s = scale;
    const float core_color[3]  = { 1.00f, 0.85f, 0.25f };
    const float ray_color[3]   = { 1.00f, 0.65f, 0.10f };
    const float arrow_color[3] = { 1.00f, 0.95f, 0.60f };
    const float zero[3]        = { 0.0f, 0.0f, 0.0f };
    g->vertex_count = 0;
    g->index_count  = 0;

    /* core: octahedron */
    const float r = 0.28f * s;
    int o0 = add_glyph_vertex(g,  r, 0, 0, core_color), o1 = add_glyph_vertex(g, -r, 0, 0, core_color);
    int o2 = add_glyph_vertex(g, 0,  r, 0, core_color), o3 = add_glyph_vertex(g, 0, -r, 0, core_color);
    int o4 = add_glyph_vertex(g, 0, 0,  r, core_color), o5 = add_glyph_vertex(g, 0, 0, -r, core_color);
    const int faces[8][3] = { {0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5} };
    const int corners[6]  = { o0, o1, o2, o3, o4, o5 };
    for (int i = 0; i < 8; i++) add_glyph_triangle(g, corners[faces[i][0]], corners[faces[i][1]], corners[faces[i][2]], zero);

    /* four short rays in the XY plane */
    const float axis_x[3] = { 1, 0, 0 }, axis_y[3] = { 0, 1, 0 }, axis_z[3] = { 0, 0, 1 };
    const float ray_base = 0.34f * s, ray_len = 0.70f * s, ray_half = 0.08f * s;
    { float bc[3] = {  ray_base, 0, 0 }, tip[3] = {  ray_len, 0, 0 }; add_glyph_pyramid(g, bc, axis_y, axis_z, ray_half, tip, ray_color); }
    { float bc[3] = { -ray_base, 0, 0 }, tip[3] = { -ray_len, 0, 0 }; add_glyph_pyramid(g, bc, axis_y, axis_z, ray_half, tip, ray_color); }
    { float bc[3] = { 0,  ray_base, 0 }, tip[3] = { 0,  ray_len, 0 }; add_glyph_pyramid(g, bc, axis_x, axis_z, ray_half, tip, ray_color); }
    { float bc[3] = { 0, -ray_base, 0 }, tip[3] = { 0, -ray_len, 0 }; add_glyph_pyramid(g, bc, axis_x, axis_z, ray_half, tip, ray_color); }

    /* arrow down -Z: shaft + head */
    const float shaft = 0.05f * s;
    add_glyph_box(g, -shaft, -shaft, -1.50f * s, shaft, shaft, -0.30f * s, arrow_color);
    { float bc[3] = { 0, 0, -1.50f * s }, tip[3] = { 0, 0, -2.10f * s }; add_glyph_pyramid(g, bc, axis_x, axis_y, 0.16f * s, tip, arrow_color); }
}

/* ========================================================================= */
/* create                                                                     */
/* ========================================================================= */
bool create_system_directional_light(editor_vec3 position, float size, const char* name,
                                     VkDevice device, VkPhysicalDevice physical_device,
                                     VkCommandPool pool, VkQueue queue)
{
    (void)pool; (void)queue;

    const char* light_name = "Directional Light";
    if (name) light_name = name;

    float glyph_scale = 1.0f;
    if (size > 0.0f) glyph_scale = size;

    if (g_light_count >= light_max_objects) {
        phoenix_engine_warn("Light", "Light registry full (%d). Cannot add another light.", light_max_objects);
        return false;
    }
    if (g_scene_registry.object_count >= max_scene_objects) {
        phoenix_engine_warn("Light", "%s", "Scene registry capacity full. Cannot allocate light object.");
        return false;
    }

    system_scene_object* obj = spawn_new_scene_object(light_name, position);
    if (!obj) {
        phoenix_engine_error("Light", "%s", "Failed to allocate light hierarchy node.");
        return false;
    }

    /* id is assigned by spawn_new_scene_object - do NOT overwrite it here */
    obj->type     = scene_object_light;
    obj->rotation = (editor_vec3){ -0.90f, 0.60f, 0.0f };   /* pitch down ~50 degrees, turned a little: a readable default sun */
    obj->scale    = (editor_vec3){ 1.0f, 1.0f, 1.0f };

    build_sun_glyph(&g_sun_glyph, glyph_scale);

    if (!upload_scene_object_mesh(obj, device, physical_device, g_sun_glyph.vertices, (uint32_t)g_sun_glyph.vertex_count,
                                  g_sun_glyph.indices, (uint32_t)g_sun_glyph.index_count)) {
        phoenix_engine_error("Light", "%s", "Light GPU upload failed - object will appear in hierarchy only.");
    }

    light_object* l = &g_lights[g_light_count++];
    l->object_id = obj->id;
    l->kind      = light_kind_directional;
    l->enabled   = true;

    phoenix_engine_info("Light", "Directional light '%s' registered.", light_name);
    return true;
}

/* ========================================================================= */
/* the light drives the sun                                                   */
/* ========================================================================= */

/* rotation (radians, x = pitch, y = yaw) -> the sun settings.
   direction the light travels = Ry(yaw) * Rx(pitch) * (0,0,-1)   (same -Z the camera lens uses)
   the sun sits on the opposite side. If the shadow side looks mirrored, flip the sign of the atan2 line. */
void get_sun_from_rotation(editor_vec3 rotation, float* out_azimuth_deg, float* out_elevation_deg)
{
    const float cp = cosf(rotation.x), sp = sinf(rotation.x);
    const float cy = cosf(rotation.y), sy = sinf(rotation.y);
    const float travel_x = -cp * sy, travel_y = sp, travel_z = -cp * cy;
    const float sun_x = -travel_x, sun_y = -travel_y, sun_z = -travel_z;     /* toward the sun */

    float up = sun_y;
    if (up < -1.0f) up = -1.0f;
    if (up >  1.0f) up =  1.0f;

    float el = asinf(up) * 180.0f / phoenix_pi;
    float az = atan2f(sun_x, -sun_z) * 180.0f / phoenix_pi;                  /* compass: 0 = -Z, 90 = +X */
    if (az < 0.0f)    az += 360.0f;
    if (az >= 360.0f) az -= 360.0f;
    if (el < 5.0f)    el = 5.0f;                                             /* same limits as the sun sliders */
    if (el > 90.0f)   el = 90.0f;
    *out_azimuth_deg   = az;
    *out_elevation_deg = el;
}

void update_light_from_scene(void)
{
    g_light_driver = -1;

    for (int i = 0; i < g_light_count; ) {
        system_scene_object* obj = find_scene_object_by_id(g_lights[i].object_id);
        if (!obj) { remove_light_at(i); continue; }          /* deleted in the hierarchy */

        if (g_light_driver < 0 && g_lights[i].enabled && g_lights[i].kind == light_kind_directional) {
            g_light_driver = i;
            get_sun_from_rotation(obj->rotation, &g_scene_sun.azimuth_deg, &g_scene_sun.elevation_deg);
        }
        i++;
    }
}

/* ========================================================================= */
/* access for the Lighting window                                             */
/* ========================================================================= */
int get_light_object_count(void) { return g_light_count; }

const light_object* get_light_object_at(int index)
{
    if (index >= 0 && index < g_light_count) return &g_lights[index];
    return NULL;
}

const char* get_light_object_name(int index)
{
    if (index < 0 || index >= g_light_count) return "";
    const system_scene_object* obj = find_scene_object_by_id(g_lights[index].object_id);
    if (obj) return obj->name;
    return "";
}

void set_light_object_enabled(int index, bool enabled)
{
    if (index >= 0 && index < g_light_count) g_lights[index].enabled = enabled;
}

void select_light_object(int index)
{
    if (index >= 0 && index < g_light_count) g_scene_registry.selected_object_id = g_lights[index].object_id;
}

bool is_light_sun_driven(const char** out_name)
{
    if (g_light_driver < 0) {
        if (out_name) *out_name = "";
        return false;
    }
    if (out_name) *out_name = get_light_object_name(g_light_driver);
    return true;
}