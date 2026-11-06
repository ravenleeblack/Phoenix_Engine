#include "system_globals.h"

/* ========================================================================= */
/* registry of light objects. the scene registry stays the light list         */
/* (id, name, transform); only the light specific bits live here.             */
/* ========================================================================= */
static light_object s_lights[LIGHT_MAX_OBJECTS];
static int          s_light_count = 0;
static int          s_driver      = -1;     /* index in s_lights that drives the sun, -1 = nobody */

static const float k_pi = 3.14159265f;

static void remove_light_at(int i)
{
    for (int k = i; k < s_light_count - 1; k++) s_lights[k] = s_lights[k + 1];
    s_light_count--;
}

/* ========================================================================= */
/* glyph: a sun. small octahedron core, four short rays (+-X, +-Y) and a      */
/* long arrow down local -Z. the arrow is the direction the light travels.    */
/* every piece is convex, so triangle winding is fixed automatically by       */
/* pushing the face away from the piece's own centre.                         */
/* ========================================================================= */
static int g_vert(glyph* g, float x, float y, float z, const float c[3])
{
    editor_vertex* p = &g->vertices[g->vertex_count];
    p->x = x; p->y = y; p->z = z;
    p->r = c[0]; p->g = c[1]; p->b = c[2]; p->a = 1.0f;
    return g->vertex_count++;
}

/* one triangle, wound counter-clockwise seen from outside of a convex piece centred on `cen` */
static void g_tri(glyph* g, int a, int b, int c, const float cen[3])
{
    const editor_vertex *A = &g->vertices[a], *B = &g->vertices[b], *C = &g->vertices[c];
    float ux = B->x - A->x, uy = B->y - A->y, uz = B->z - A->z;
    float vx = C->x - A->x, vy = C->y - A->y, vz = C->z - A->z;
    float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    float mx = (A->x + B->x + C->x) / 3.0f - cen[0];
    float my = (A->y + B->y + C->y) / 3.0f - cen[1];
    float mz = (A->z + B->z + C->z) / 3.0f - cen[2];
    if (nx * mx + ny * my + nz * mz < 0.0f) { int t = b; b = c; c = t; }
    g->indices[g->index_count++] = (uint32_t)a;
    g->indices[g->index_count++] = (uint32_t)b;
    g->indices[g->index_count++] = (uint32_t)c;
}

/* square pyramid: base centre bc, base half size h, spread across axes (u,w), tip at bc + dir*len */
static void g_pyramid(glyph* g, const float bc[3], const float u[3], const float w[3], float h,
                      const float tip[3], const float col[3])
{
    int b0 = g_vert(g, bc[0] - u[0]*h - w[0]*h, bc[1] - u[1]*h - w[1]*h, bc[2] - u[2]*h - w[2]*h, col);
    int b1 = g_vert(g, bc[0] + u[0]*h - w[0]*h, bc[1] + u[1]*h - w[1]*h, bc[2] + u[2]*h - w[2]*h, col);
    int b2 = g_vert(g, bc[0] + u[0]*h + w[0]*h, bc[1] + u[1]*h + w[1]*h, bc[2] + u[2]*h + w[2]*h, col);
    int b3 = g_vert(g, bc[0] - u[0]*h + w[0]*h, bc[1] - u[1]*h + w[1]*h, bc[2] - u[2]*h + w[2]*h, col);
    int t  = g_vert(g, tip[0], tip[1], tip[2], col);
    const float cen[3] = { (bc[0] + tip[0]) * 0.5f, (bc[1] + tip[1]) * 0.5f, (bc[2] + tip[2]) * 0.5f };
    g_tri(g, b0, b1, b2, cen);  g_tri(g, b0, b2, b3, cen);
    g_tri(g, b0, b1, t, cen);   g_tri(g, b1, b2, t, cen);
    g_tri(g, b2, b3, t, cen);   g_tri(g, b3, b0, t, cen);
}

static void g_box(glyph* g, float x0, float y0, float z0, float x1, float y1, float z1, const float col[3])
{
    int p[8];
    for (int i = 0; i < 8; i++)
        p[i] = g_vert(g, (i & 1) ? x1 : x0, (i & 2) ? y1 : y0, (i & 4) ? z1 : z0, col);
    const float cen[3] = { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f };
    static const int q[6][4] = { {0,1,3,2}, {4,5,7,6}, {0,1,5,4}, {2,3,7,6}, {0,2,6,4}, {1,3,7,5} };
    for (int f = 0; f < 6; f++) {
        g_tri(g, p[q[f][0]], p[q[f][1]], p[q[f][2]], cen);
        g_tri(g, p[q[f][0]], p[q[f][2]], p[q[f][3]], cen);
    }
}

static void build_sun_glyph(glyph* g, float s)
{
    const float core[3] = { 1.00f, 0.85f, 0.25f };
    const float ray[3]  = { 1.00f, 0.65f, 0.10f };
    const float arrow[3]= { 1.00f, 0.95f, 0.60f };
    const float zero[3] = { 0.0f, 0.0f, 0.0f };
    g->vertex_count = g->index_count = 0;

    /* core: octahedron */
    const float r = 0.28f * s;
    int o0 = g_vert(g,  r, 0, 0, core), o1 = g_vert(g, -r, 0, 0, core);
    int o2 = g_vert(g, 0,  r, 0, core), o3 = g_vert(g, 0, -r, 0, core);
    int o4 = g_vert(g, 0, 0,  r, core), o5 = g_vert(g, 0, 0, -r, core);
    static const int f[8][3] = { {0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5} };
    const int o[6] = { o0, o1, o2, o3, o4, o5 };
    for (int i = 0; i < 8; i++) g_tri(g, o[f[i][0]], o[f[i][1]], o[f[i][2]], zero);

    /* four short rays in the XY plane */
    const float X[3] = { 1, 0, 0 }, Y[3] = { 0, 1, 0 }, Z[3] = { 0, 0, 1 };
    const float rb = 0.34f * s, rl = 0.70f * s, rh = 0.08f * s;
    { float bc[3] = {  rb, 0, 0 }, tip[3] = {  rl, 0, 0 }; g_pyramid(g, bc, Y, Z, rh, tip, ray); }
    { float bc[3] = { -rb, 0, 0 }, tip[3] = { -rl, 0, 0 }; g_pyramid(g, bc, Y, Z, rh, tip, ray); }
    { float bc[3] = { 0,  rb, 0 }, tip[3] = { 0,  rl, 0 }; g_pyramid(g, bc, X, Z, rh, tip, ray); }
    { float bc[3] = { 0, -rb, 0 }, tip[3] = { 0, -rl, 0 }; g_pyramid(g, bc, X, Z, rh, tip, ray); }

    /* arrow down -Z: shaft + head */
    const float sh = 0.05f * s;
    g_box(g, -sh, -sh, -1.50f * s, sh, sh, -0.30f * s, arrow);
    { float bc[3] = { 0, 0, -1.50f * s }, tip[3] = { 0, 0, -2.10f * s }; g_pyramid(g, bc, X, Y, 0.16f * s, tip, arrow); }
}

/* ========================================================================= */
/* create                                                                     */
/* ========================================================================= */
bool create_system_directional_light(editor_vec3 position, float size, const char* name,
                                     VkDevice device, VkPhysicalDevice physical_device,
                                     VkCommandPool pool, VkQueue queue)
{
    (void)pool; (void)queue;

    if (s_light_count >= LIGHT_MAX_OBJECTS) {
        phoenix_warn("Light", "Light registry full (%d). Cannot add another light.", LIGHT_MAX_OBJECTS);
        return false;
    }
    if (g_scene_registry.object_count >= max_scene_objects) {
        phoenix_warn("Light", "%s", "Scene registry capacity full. Cannot allocate light object.");
        return false;
    }

    system_scene_object* obj = spawn_new_scene_object(name ? name : "Directional Light", position);
    if (!obj) {
        phoenix_error("Light", "%s", "Failed to allocate light hierarchy node.");
        return false;
    }

    /* id is assigned by spawn_new_scene_object - do NOT overwrite it here */
    obj->type     = scene_object_light;
    obj->rotation = (editor_vec3){ -0.90f, 0.60f, 0.0f };   /* pitch down ~50 degrees, turned a little: a readable default sun */
    obj->scale    = (editor_vec3){ 1.0f, 1.0f, 1.0f };

    static glyph g;                                          /* ~3 KB, built once per call */
    build_sun_glyph(&g, (size > 0.0f) ? size : 1.0f);

    if (!upload_scene_object_mesh(obj, device, physical_device, g.vertices, (uint32_t)g.vertex_count, g.indices, (uint32_t)g.index_count)) {
        phoenix_error("Light", "%s", "Light GPU upload failed - object will appear in hierarchy only.");
    }

    light_object* l = &s_lights[s_light_count++];
    l->object_id = obj->id;
    l->kind      = LIGHT_KIND_DIRECTIONAL;
    l->enabled   = true;

    phoenix_info("Light", "Directional light '%s' registered.", name ? name : "Directional Light");
    return true;
}

/* ========================================================================= */
/* the light drives the sun                                                   */
/* ========================================================================= */

/* rotation (radians, x = pitch, y = yaw) -> the sun settings.
   direction the light travels = Ry(yaw) * Rx(pitch) * (0,0,-1)   (same -Z the camera lens uses)
   the sun sits on the opposite side. elevation = pitch pointing down, so -0.35 rad (the camera default) = 20 degrees.
   If the shadow side looks mirrored in the Scene view, flip the sign of the atan2 line below: it is the only place that decides it. */
static void sun_from_rotation(editor_vec3 rot, float* az_deg, float* el_deg)
{
    const float cp = cosf(rot.x), sp = sinf(rot.x), cy = cosf(rot.y), sy = sinf(rot.y);
    const float dx = -cp * sy, dy = sp, dz = -cp * cy;       /* travel direction */
    const float sx = -dx, sy_up = -dy, sz = -dz;             /* toward the sun */

    float el = asinf(sy_up < -1.0f ? -1.0f : (sy_up > 1.0f ? 1.0f : sy_up)) * 180.0f / k_pi;
    float az = atan2f(sx, -sz) * 180.0f / k_pi;              /* compass: 0 = -Z, 90 = +X */
    if (az < 0.0f)   az += 360.0f;
    if (az >= 360.f) az -= 360.0f;
    if (el < 5.0f)   el = 5.0f;                              /* same limits as the sun sliders */
    if (el > 90.0f)  el = 90.0f;
    *az_deg = az;
    *el_deg = el;
}

void light_update_from_scene(void)
{
    s_driver = -1;

    for (int i = 0; i < s_light_count; ) {
        system_scene_object* obj = find_scene_object_by_id(s_lights[i].object_id);
        if (!obj) { remove_light_at(i); continue; }          /* deleted in the hierarchy */

        if (s_driver < 0 && s_lights[i].enabled && s_lights[i].kind == LIGHT_KIND_DIRECTIONAL) {
            s_driver = i;
            sun_from_rotation(obj->rotation, &g_scene_sun.azimuth_deg, &g_scene_sun.elevation_deg);
        }
        i++;
    }
}

/* ========================================================================= */
/* access for the Lighting window                                             */
/* ========================================================================= */
int light_object_count(void) { return s_light_count; }

const light_object* light_object_at(int index)
{
    return (index >= 0 && index < s_light_count) ? &s_lights[index] : NULL;
}

const char* light_object_name(int index)
{
    if (index < 0 || index >= s_light_count) return "";
    const system_scene_object* obj = find_scene_object_by_id(s_lights[index].object_id);
    return obj ? obj->name : "";
}

void light_object_set_enabled(int index, bool enabled)
{
    if (index >= 0 && index < s_light_count) s_lights[index].enabled = enabled;
}

void light_object_select(int index)
{
    if (index >= 0 && index < s_light_count) g_scene_registry.selected_object_id = s_lights[index].object_id;
}

bool light_sun_is_driven(const char** name_out)
{
    if (s_driver < 0) { if (name_out) *name_out = ""; return false; }
    if (name_out) *name_out = light_object_name(s_driver);
    return true;
}