#include "system_globals.h"

/* Camera glyph: a small body box plus a flared lens frustum pointing down local -Z. */
void create_system_camera(editor_vec3 position, float size, const char* name, VkDevice device, VkPhysicalDevice physical_device, VkCommandPool pool, VkQueue queue)
{
    (void)pool; (void)queue;

    if (g_scene_registry.object_count >= max_scene_objects) {
        phoenix_warn("ModelerObjects", "Scene registry capacity full. Cannot allocate camera object.");
        return;
    }

    system_scene_object* obj = spawn_new_scene_object(name, position);
    if (!obj) {
        phoenix_error("ModelerObjects", "Failed to allocate camera hierarchy node.");
        return;
    }

    /* id is assigned by spawn_new_scene_object - do NOT overwrite it here */
    obj->type     = scene_object_camera;
    obj->rotation = (editor_vec3){-0.35f, 0.0f, 0.0f};   /* pitch nose down toward origin */
    obj->scale    = (editor_vec3){1.0f, 1.0f, 1.0f};

    const float s = (size > 0.0f) ? size : 1.0f;
    const float bx = 0.30f * s, by = 0.20f * s, bz = 0.40f * s;   /* body half extents */
    const float lx = 0.45f * s, ly = 0.30f * s, lz = 1.00f * s;   /* lens end half extents / depth */

    const float body_col[4] = { 0.90f, 0.55f, 0.10f, 1.0f };
    const float lens_col[4] = { 1.00f, 0.85f, 0.45f, 1.0f };

    editor_vertex vert_to_edge_array[12];
    const float p[12][3] = {
        /* body, front face (z = -bz) first so indices 0..3 are the lens base */
        {-bx,-by,-bz}, { bx,-by,-bz}, { bx, by,-bz}, {-bx, by,-bz},
        {-bx,-by, bz}, { bx,-by, bz}, { bx, by, bz}, {-bx, by, bz},
        /* lens end */
        {-lx,-ly,-lz}, { lx,-ly,-lz}, { lx, ly,-lz}, {-lx, ly,-lz}
    };
    for (int i = 0; i < 12; i++) {
        const float* c = (i < 8) ? body_col : lens_col;
        float shade = (i < 8) ? (0.75f + 0.25f * (p[i][1] > 0 ? 1.0f : 0.0f)) : 1.0f;
        vert_to_edge_array[i].x = p[i][0]; vert_to_edge_array[i].y = p[i][1]; vert_to_edge_array[i].z = p[i][2];
        vert_to_edge_array[i].r = c[0] * shade; vert_to_edge_array[i].g = c[1] * shade; vert_to_edge_array[i].b = c[2] * shade; vert_to_edge_array[i].a = 1.0f;
    }

    const uint32_t idx[] = {
        /* body */
        4,5,6, 6,7,4,   1,0,3, 3,2,1,   0,4,7, 7,3,0,
        5,1,2, 2,6,5,   3,7,6, 6,2,3,   0,1,5, 5,4,0,
        /* lens sides (outward winding) */
        0,9,1,  9,0,8,    1,10,2, 10,1,9,
        2,11,3, 11,2,10,  3,8,0,  8,3,11,
        /* lens end cap */
        8,10,9, 10,8,11
    };
    const uint32_t idx_count = (uint32_t)(sizeof(idx) / sizeof(idx[0]));

    if (!upload_scene_object_mesh(obj, device, physical_device, vert_to_edge_array, 12, idx, idx_count)) {
        phoenix_error("ModelerObjects", "Camera GPU upload failed - object will appear in hierarchy only.");
        return;
    }

    phoenix_info("ModelerObjects", "Camera '%s' registered with glyph mesh.", name ? name : "Camera");
}