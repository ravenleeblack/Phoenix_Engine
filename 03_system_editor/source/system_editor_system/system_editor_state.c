#include "system_globals.h"

/* ========================================================================= */
/* Editor state that is remembered between runs. Same file as the prefs:       */
/* phoenix_editor.pdb next to the exe (system_settings_database.c).             */
/* One key per thing, plain text, so you can open the file and edit it by hand. */
/*                                                                               */
/*   view.grid / view.origin / view.cameras / view.wire_on_shaded / view.xray  0 or 1 */
/*   view.shading            0 wire, 1 shaded, 2 textured                        */
/*   view.wire_color         r,g,b  (0..1)  Wireframe mode lines                 */
/*   view.wire_on_shaded_color  r,g,b       Wireframe on Shaded lines            */
/*   view.locked             0 or 1, view.lock_pos / view.lock_rot x,y,z          */
/*   view.camera_pos / view.camera_rot  x,y,z   the editor camera                */
/*   mode.tool               0 select, 1 translate, 2 rotate, 3 scale            */
/*   mode.select_type        0 vert, 1 edge_to_face_array, 2 face                              */
/*   mode.edit_xray          0 or 1                                              */
/*   mode.snap               enabled, translate mesh, rotate deg, scale             */
/*                                                                               */
/* Not saved on purpose: Edit mode / sculpt / paint (there is no object in edit   */
/* mode at boot, so the editor always starts in Object mode), obj_vert_selected object     */
/* (ids are not stable until the scene itself is saved), dock layout.             */
/* To add something: one get in editor_state_apply_from_db, one set in             */
/* editor_state_write_to_db, same key.                                             */
/* ========================================================================= */

static bool get_bool(const char* key, bool* out)
{
    int vert_to_edge_array;
    if (!settings_db_get_int(key, &vert_to_edge_array)) return false;
    *out = (vert_to_edge_array != 0);
    return true;
}

static void set_bool(const char* key, bool vert_to_edge_array) { settings_db_set_int(key, vert_to_edge_array ? 1 : 0); }

static void get_color3(const char* key, float out[3])
{
    float f[3];
    if (!settings_db_get_floats(key, f, 3)) return;
    for (int i = 0; i < 3; i++) out[i] = f[i] < 0.0f ? 0.0f : (f[i] > 1.0f ? 1.0f : f[i]);
}

static void get_vec3(const char* key, editor_vec3* out)
{
    float f[3];
    if (!settings_db_get_floats(key, f, 3)) return;
    out->x = f[0]; out->y = f[1]; out->z = f[2];
}

static void set_vec3(const char* key, editor_vec3 vert_to_edge_array)
{
    float f[3] = { vert_to_edge_array.x, vert_to_edge_array.y, vert_to_edge_array.z };
    settings_db_set_floats(key, f, 3);
}

void editor_state_apply_from_db(void)
{
    int vert_to_edge_array;

    /* ---- Scene tab buttons ---- */
    get_bool("view.grid",           &g_scene_view.show_grid);
    get_bool("view.origin",         &g_scene_view.show_origin);
    get_bool("view.cameras",        &g_scene_view.show_cameras);
    get_bool("view.wire_on_shaded", &g_scene_view.wire_on_shaded);
    get_bool("view.xray",           &g_scene_view.xray);
    if (settings_db_get_int("view.shading", &vert_to_edge_array) && vert_to_edge_array >= scene_shading_wire && vert_to_edge_array <= scene_shading_textured)
        g_scene_view.shading = (enum_scene_shading)vert_to_edge_array;

    get_color3("view.wire_color",            g_scene_view.wire_color);
    get_color3("view.wire_on_shaded_color",  g_scene_view.wire_on_shaded_color);

    /* ---- editor camera (and its lock) ---- */
    world_camera* cam = &engine_context.world_state.editor_scene_camera;
    get_vec3("view.camera_pos", &cam->position);
    get_vec3("view.camera_rot", &cam->rotation);
    get_vec3("view.lock_pos",   &g_scene_view.lock_position);
    get_vec3("view.lock_rot",   &g_scene_view.lock_rotation);
    get_bool("view.locked",     &g_scene_view.camera_locked);

    /* ---- tool / select type / snap ---- */
    start_mode_state();
    if (settings_db_get_int("mode.tool", &vert_to_edge_array) && vert_to_edge_array >= tool_select && vert_to_edge_array <= tool_scale)
        mode_state_set_tool(&element_context, (enum_dcc_tool)vert_to_edge_array);
    if (settings_db_get_int("mode.select_type", &vert_to_edge_array) && vert_to_edge_array >= select_vert && vert_to_edge_array <= select_face) {
        g_mode_state.edit.select_type       = (enum_mesh_selection)vert_to_edge_array;
        element_context.active_select_type = (enum_mesh_selection)vert_to_edge_array;
    }
    get_bool("mode.edit_xray", &g_mode_state.edit.xray);

    float s[4];
    if (settings_db_get_floats("mode.snap", s, 4)) {
        g_mode_state.object.snap_enabled     = (s[0] != 0.0f);
        g_mode_state.object.snap_translate_m = s[1];
        g_mode_state.object.snap_rotate_deg  = s[2];
        g_mode_state.object.snap_scale       = s[3];
    }
}

void editor_state_write_to_db(void)
{
    set_bool("view.grid",           g_scene_view.show_grid);
    set_bool("view.origin",         g_scene_view.show_origin);
    set_bool("view.cameras",        g_scene_view.show_cameras);
    set_bool("view.wire_on_shaded", g_scene_view.wire_on_shaded);
    set_bool("view.xray",           g_scene_view.xray);
    settings_db_set_int("view.shading", (int)g_scene_view.shading);
    settings_db_set_floats("view.wire_color",           g_scene_view.wire_color, 3);
    settings_db_set_floats("view.wire_on_shaded_color", g_scene_view.wire_on_shaded_color, 3);

    world_camera* cam = &engine_context.world_state.editor_scene_camera;
    set_vec3("view.camera_pos", cam->position);
    set_vec3("view.camera_rot", cam->rotation);
    set_vec3("view.lock_pos",   g_scene_view.lock_position);
    set_vec3("view.lock_rot",   g_scene_view.lock_rotation);
    set_bool("view.locked",     g_scene_view.camera_locked);

    settings_db_set_int("mode.tool",        (int)g_mode_state.object.tool);
    settings_db_set_int("mode.select_type", (int)g_mode_state.edit.select_type);
    set_bool("mode.edit_xray", g_mode_state.edit.xray);
    float s[4] = { g_mode_state.object.snap_enabled ? 1.0f : 0.0f, g_mode_state.object.snap_translate_m,
                   g_mode_state.object.snap_rotate_deg, g_mode_state.object.snap_scale };
    settings_db_set_floats("mode.snap", s, 4);
}