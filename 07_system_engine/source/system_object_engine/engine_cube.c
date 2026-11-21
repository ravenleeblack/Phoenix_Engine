#include "engine_globals.h"

/* Maya default shaded grey (sRGB 0.55). The mesh shader writes vertex colour as-is, so convert to linear
   when the swapchain is an sRGB format (same rule push_quad_element uses for UI colors). */
#define CUBE_MAYA_GREY_SRGB 0.62f

static float convert_srgb_to_linear(float c)
{
    if (!g_swapchain_is_srgb) return c;
    return (c <= 0.04045f) ? (c / 12.92f) : powf((c + 0.055f) / 1.055f, 2.4f);
}

/* the grey of every object without a skin: the cube, the terrain, anything else untextured */
float skin_empty_grey_linear(void)
{
    return convert_srgb_to_linear(CUBE_MAYA_GREY_SRGB);
}

/* size is a multiple of the system unit: 1.0f = one 4 mesh cube */
void create_system_cube(editor_vec3 position, float size, const char* name, VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue)
{
    (void)pool; (void)queue;

    if (g_scene_registry.object_count >= max_scene_objects) {
        phoenix_warn("ModelerObjects", "Database full. Cannot allocate primitive cube.");
        return;
    }

    system_scene_object* obj = spawn_new_scene_object(name, position);
    if (!obj) return;

    /* id is assigned by spawn_new_scene_object - do NOT overwrite it here */
    obj->type  = scene_object_mesh;
    obj->scale = (editor_vec3){1.0f, 1.0f, 1.0f};

    const float h = size * system_unit_m * 0.5f;
    const float sgn[8][3] = {
        {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
        {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}
    };

    /* 4 corners per face, counter-clockwise seen from outside: +Z, -Z, -X, +X, +Y, -Y */
    const int face_corners[6][4] = {
        {4, 5, 6, 7},
        {1, 0, 3, 2},
        {0, 4, 7, 3},
        {5, 1, 2, 6},
        {3, 7, 6, 2},
        {0, 1, 5, 4}
    };

    /* outward normal per face, same order as face_corners: +Z, -Z, -X, +X, +Y, -Y */
    const float face_normal[6][3] = {
        { 0, 0, 1}, { 0, 0,-1}, {-1, 0, 0}, { 1, 0, 0}, { 0, 1, 0}, { 0,-1, 0}
    };
    /* 6 faces in a 3 x 3 atlas (3 cells empty so texels stay square; corner order = face_corners order): no two faces share texels, so painting one face never shows on another */
    const float corner_uv[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

    /* Convert standard base grey to linear space once */
    const float base_shade = skin_empty_grey_linear();     /* same grey as every other object without a skin */

    /* 24 verts: every face owns its 4 corners so each face can get its own uv and normal */
    editor_vertex vertices[24];
    memset(vertices, 0, sizeof(vertices));
    uint32_t indices[36];
    for (int f = 0; f < 6; f++) {
        for (int c = 0; c < 4; c++) {
            editor_vertex* o = &vertices[f * 4 + c];
            const int k = face_corners[f][c];
            o->x = sgn[k][0] * h;
            o->y = sgn[k][1] * h;
            o->z = sgn[k][2] * h;
            o->r = base_shade;
            o->g = base_shade;
            o->b = base_shade;
            o->a = 1.0f;
            set_vertex_attributes(o, ((float)(f % 3) + corner_uv[c][0]) / 3.0f, ((float)(f / 3) + corner_uv[c][1]) / 3.0f,
                                  face_normal[f][0], face_normal[f][1], face_normal[f][2]);
        }
        const uint32_t b = (uint32_t)(f * 4);
        const uint32_t i = (uint32_t)(f * 6);
        indices[i + 0] = b + 0; indices[i + 1] = b + 1; indices[i + 2] = b + 2;
        indices[i + 3] = b + 2; indices[i + 4] = b + 3; indices[i + 5] = b + 0;
    }

    /* object database + edit mode first, so the cube is editable even if the GPU upload fails */

    if (!upload_scene_object_mesh(obj, device, physical_device, vertices, 24, indices, 36)) {
        phoenix_error("ModelerObjects", "Cube GPU upload failed - object will appear in hierarchy only.");
        return;
    }

    phoenix_info("ModelerObjects", "Cube '%s' uploaded: %.1f mesh edge_to_face_array, 24 verts, 36 indices.", name ? name : "Cube", size * system_unit_m);
}