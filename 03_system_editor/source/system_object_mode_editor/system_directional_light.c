#include "system_globals.h"

/* the editor module draws its own copy of the scene, so its lights live in its own list.
   the glyph builder and the rotation -> sun math are pure and come from the engine (engine_direction_light.h).
   the list, the glyph buffer and the driver index are extern in system_globals.h. */

void remove_editor_light_at(int index)
{
    for (int k = index; k < editor_light_count - 1; k++) {
        editor_lights[k] = editor_lights[k + 1];
    }
    editor_light_count--;
}

bool create_system_directional_light(editor_vec3 position, float size, const char* name,
                                     VkDevice device, VkPhysicalDevice physical_device,
                                     VkCommandPool pool, VkQueue queue)
{
    (void)pool;
    (void)queue;

    const char* light_name = "Directional Light";
    if (name) {
        light_name = name;
    }

    float glyph_scale = 1.0f;
    if (size > 0.0f) {
        glyph_scale = size;
    }

    if (editor_light_count >= light_max_objects) {
        phoenix_warn("Light", "Light registry full (%d). Cannot add another light.", light_max_objects);
        return false;
    }
    if (g_scene_registry.object_count >= max_scene_objects) {
        phoenix_warn("Light", "%s", "Scene registry capacity full. Cannot allocate light object.");
        return false;
    }

    system_scene_object* obj = spawn_new_scene_object(light_name, position);
    if (!obj) {
        phoenix_error("Light", "%s", "Failed to allocate light hierarchy node.");
        return false;
    }

    /* id is assigned by spawn_new_scene_object - do NOT overwrite it here */
    obj->type     = scene_object_light;
    obj->rotation = (editor_vec3){ -0.90f, 0.60f, 0.0f };   /* pitch down ~50 degrees, turned a little: a readable default sun */
    obj->scale    = (editor_vec3){ 1.0f, 1.0f, 1.0f };

    build_sun_glyph(&editor_sun_glyph, glyph_scale);

    if (!upload_scene_object_mesh(obj, device, physical_device, editor_sun_glyph.vertices, (uint32_t)editor_sun_glyph.vertex_count,
                                  editor_sun_glyph.indices, (uint32_t)editor_sun_glyph.index_count)) {
        phoenix_error("Light", "%s", "Light GPU upload failed - object will appear in hierarchy only.");
    }

    light_object* l = &editor_lights[editor_light_count];
    editor_light_count++;
    l->object_id = obj->id;
    l->kind      = light_kind_directional;
    l->enabled   = true;

    phoenix_info("Light", "Directional light '%s' registered.", light_name);
    return true;
}

/* the first enabled directional light drives the sun in the editor's scene view */
void update_light_from_scene(void)
{
    editor_light_driver = -1;

    for (int i = 0; i < editor_light_count; ) {
        system_scene_object* obj = find_scene_object_by_id(editor_lights[i].object_id);
        if (!obj) {
            remove_editor_light_at(i);                       /* deleted in the hierarchy */
            continue;
        }

        if (editor_light_driver < 0 && editor_lights[i].enabled && editor_lights[i].kind == light_kind_directional) {
            editor_light_driver = i;
            get_sun_from_rotation(obj->rotation, &g_scene_sun.azimuth_deg, &g_scene_sun.elevation_deg);
        }
        i++;
    }
}

int get_light_object_count(void)
{
    return editor_light_count;
}

const light_object* get_light_object_at(int index)
{
    if (index >= 0 && index < editor_light_count) {
        return &editor_lights[index];
    }
    return NULL;
}

const char* get_light_object_name(int index)
{
    if (index < 0 || index >= editor_light_count) {
        return "";
    }
    const system_scene_object* obj = find_scene_object_by_id(editor_lights[index].object_id);
    if (obj) {
        return obj->name;
    }
    return "";
}

void set_light_object_enabled(int index, bool enabled)
{
    if (index >= 0 && index < editor_light_count) {
        editor_lights[index].enabled = enabled;
    }
}

void select_light_object(int index)
{
    if (index >= 0 && index < editor_light_count) {
        g_scene_registry.selected_object_id = editor_lights[index].object_id;
    }
}

bool is_light_sun_driven(const char** out_name)
{
    if (editor_light_driver < 0) {
        if (out_name) {
            *out_name = "";
        }
        return false;
    }
    if (out_name) {
        *out_name = get_light_object_name(editor_light_driver);
    }
    return true;
}
