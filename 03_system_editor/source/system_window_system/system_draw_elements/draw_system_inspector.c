#include "system_globals.h"
#include "system_textureshop_skins.h"

/* ========================================================================= */
/* Inspector: the selected object's name and Transform, and for the terrain    */
/* its options (mode, load radius, tint, sheen).               */
/* every number is a draw_number_field: drag to scrub, click to type.           */
/* ========================================================================= */
enum {
    inspector_id_base       = 0x49500000,
    inspector_row_h         = 22,
    inspector_label_w       = 78,
    inspector_field_gap     = 4,
    inspector_button_id     = 0x49510000
};

bool g_inspector_terrain_dirty = false;

void draw_inspector_section(VkCommandBuffer cmd, system_element_context* ui_ctx, const char* title, int x, int y, int w)
{
    draw_text_element(cmd, ui_ctx, title, x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)x, (float)(y + ui_font_h + 4), (float)w, 1.0f, system_theme.system_border);
}

/* three boxes in a row: x, y, z (or r, g, b) */
number_field_result draw_inspector_vec3_row(VkCommandBuffer cmd, system_element_context* ui_ctx, uint32_t id, const char* label,
                                            int x, int y, int w, float values[3], const number_field_opts* opts)
{
    draw_text_element(cmd, ui_ctx, label, x, y + (inspector_row_h - ui_font_h) / 2, system_theme.system_text);
    const int field_w = (w - inspector_label_w - inspector_field_gap * 2) / 3;
    number_field_result result = number_field_none;
    for (int i = 0; i < 3; i++) {
        const int fx = x + inspector_label_w + i * (field_w + inspector_field_gap);
        const number_field_result r = draw_number_field(cmd, ui_ctx, id + (uint32_t)i, fx, y, field_w, inspector_row_h - 2, &values[i], opts);
        if (r != number_field_none) {
            result = r;
        }
    }
    return result;
}

number_field_result draw_inspector_float_row(VkCommandBuffer cmd, system_element_context* ui_ctx, uint32_t id, const char* label,
                                             int x, int y, int w, float* value, const number_field_opts* opts)
{
    draw_text_element(cmd, ui_ctx, label, x, y + (inspector_row_h - ui_font_h) / 2, system_theme.system_text);
    return draw_number_field(cmd, ui_ctx, id, x + inspector_label_w, y, w - inspector_label_w, inspector_row_h - 2, value, opts);
}

bool draw_inspector_button(VkCommandBuffer cmd, system_element_context* ui_ctx, uint32_t id, const char* label,
                           int x, int y, int w, bool selected)
{
    system_color color = system_theme.tab_sub_menubar_button;
    if (selected) {
        color = system_theme.system_accent;
    }
    return draw_button_element(cmd, ui_ctx, id, label, x, y, w, inspector_row_h - 2, color);
}

void draw_inspector_terrain(VkCommandBuffer cmd, system_element_context* ui_ctx, int x, int* io_y, int w, int limit)
{
    int y = *io_y;
    if (y + inspector_row_h * 4 > limit) {
        return;
    }

    draw_inspector_section(cmd, ui_ctx, "TERRAIN", x, y, w);
    y += ui_font_h + 10;

    /* mode */
    draw_text_element(cmd, ui_ctx, "Mode", x, y + (inspector_row_h - ui_font_h) / 2, system_theme.system_text);
    const int mode_w = (w - inspector_label_w - inspector_field_gap) / 2;
    if (draw_inspector_button(cmd, ui_ctx, inspector_button_id + 1, "Open world", x + inspector_label_w, y, mode_w, g_terrain.settings.mode == terrain_mode_openworld)) {
        set_terrain_mode(terrain_mode_openworld);
        set_statusbar_message("%s", "Terrain: open world (smooth surface).");
    }
    if (draw_inspector_button(cmd, ui_ctx, inspector_button_id + 2, "Sandbox", x + inspector_label_w + mode_w + inspector_field_gap, y, mode_w, g_terrain.settings.mode == terrain_mode_sandbox)) {
        set_terrain_mode(terrain_mode_sandbox);
        set_statusbar_message("%s", "Terrain: sandbox (cube columns).");
    }
    y += inspector_row_h + 2;

    /* how many chunks around the camera get a mesh */
    float radius = (float)g_terrain.settings.load_radius;
    const number_field_opts radius_opts = { .step = 0.05f, .quantum = 1.0f, .min = 0.0f, .max = (float)terrain_max_load_radius, .decimals = 0 };
    if (draw_inspector_float_row(cmd, ui_ctx, inspector_id_base + 400, "Load radius", x, y, w, &radius, &radius_opts) != number_field_none) {
        set_terrain_load_radius((int)(radius + 0.5f));
    }
    y += inspector_row_h + 2;

    char info[96];
    snprintf(info, sizeof(info), "Cells %d x %d, cell %.0f m, region %d chunks", g_terrain.dims.cells_x, g_terrain.dims.cells_z,
             g_terrain.settings.cell_size_m, g_terrain.settings.region_chunks);
    draw_text_element(cmd, ui_ctx, info, x, y, get_theme_hint_color());
    y += ui_font_h + 10;

    /* surface: the Terrain skin (a normal Texture Shop skin), shaded by mesh_frag.glsl */
    texshop_skin* skin = get_builtin_terrain_skin();
    if (skin && skin->layer_count >= 1 && y + inspector_row_h * 3 <= limit) {
        draw_inspector_section(cmd, ui_ctx, "TERRAIN SURFACE", x, y, w);
        y += ui_font_h + 10;

        const number_field_opts color_opts = { .step = 0.002f, .quantum = 0.0f, .min = 0.0f, .max = 1.0f, .decimals = 3 };
        bool changed = false;
        if (draw_inspector_vec3_row(cmd, ui_ctx, inspector_id_base + 410, "Tint", x, y, w, skin->layers[0].color, &color_opts) != number_field_none) {
            changed = true;
        }
        y += inspector_row_h + 2;

        const number_field_opts unit_opts = { .step = 0.004f, .quantum = 0.0f, .min = 0.0f, .max = 1.0f, .decimals = 2 };
        if (draw_inspector_float_row(cmd, ui_ctx, inspector_id_base + 430, "Sheen", x, y, w, &skin->specular, &unit_opts) != number_field_none) {
            changed = true;
        }
        y += inspector_row_h + 2;
        if (draw_inspector_float_row(cmd, ui_ctx, inspector_id_base + 431, "Gloss", x, y, w, &skin->gloss, &unit_opts) != number_field_none) {
            changed = true;
        }
        y += inspector_row_h + 2;

        if (changed) {
            texshop_build_skin_gpu(skin->id);      /* the terrain redraws with the new look next frame */
            g_inspector_terrain_dirty = true;
        }
        draw_text_element(cmd, ui_ctx, "Tints the sculpted / painted colour. Full skin: Texture Shop.", x, y, get_theme_hint_color());
        y += ui_font_h + 6;
    }

    if (g_inspector_terrain_dirty && !ui_ctx->input.mouse_down && !is_number_field_active()) {
        save_builtin_terrain_skin();
        g_inspector_terrain_dirty = false;
    }
    *io_y = y;
}

void draw_inspector_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Inspector", node->x, content_y, node->w, content_h, system_theme.tab_content_inspector);

    const int x = node->x + 10;
    const int w = node->w - 20;
    const int limit = content_y + content_h;
    int y = content_y + 34;
    if (w < 160) {
        return;
    }

    system_scene_object* obj = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (!obj || obj == &g_scene_registry.objects[0]) {
        draw_text_element(cmd, ui_ctx, "Nothing selected.", x, y, system_theme.system_text);
        y += ui_font_h + 6;
        draw_text_element(cmd, ui_ctx, "Click an object in the Scene view,", x, y, get_theme_hint_color());
        y += ui_font_h + 4;
        draw_text_element(cmd, ui_ctx, "or pick one in the Scene Archive.", x, y, get_theme_hint_color());
        return;
    }

    draw_text_element(cmd, ui_ctx, obj->name, x, y, system_theme.system_active_text);
    y += ui_font_h + 14;

    /* ---- Transform ---- */
    draw_inspector_section(cmd, ui_ctx, "TRANSFORM", x, y, w);
    y += ui_font_h + 10;

    const uint32_t base = inspector_id_base + (obj->id & 0xFFu) * 0x1000u;   /* one set of ids per object so a typed box never follows the selection */
    const number_field_opts position_opts = { .step = 0.05f, .quantum = 0.0f, .decimals = 3 };
    const number_field_opts rotation_opts = { .step = 0.25f, .quantum = 0.0f, .display_scale = 57.29578f, .decimals = 2 };
    const number_field_opts scale_opts    = { .step = 0.01f, .quantum = 0.0f, .decimals = 3 };

    float position[3] = { obj->position.x, obj->position.y, obj->position.z };
    float rotation[3] = { obj->rotation.x, obj->rotation.y, obj->rotation.z };
    float scale[3]    = { obj->scale.x, obj->scale.y, obj->scale.z };

    if (draw_inspector_vec3_row(cmd, ui_ctx, base + 1, "Position", x, y, w, position, &position_opts) != number_field_none) {
        obj->position = (editor_vec3){ position[0], position[1], position[2] };
    }
    y += inspector_row_h + 2;
    if (draw_inspector_vec3_row(cmd, ui_ctx, base + 4, "Rotation", x, y, w, rotation, &rotation_opts) != number_field_none) {
        obj->rotation = (editor_vec3){ rotation[0], rotation[1], rotation[2] };
    }
    y += inspector_row_h + 2;
    if (draw_inspector_vec3_row(cmd, ui_ctx, base + 7, "Scale", x, y, w, scale, &scale_opts) != number_field_none) {
        obj->scale = (editor_vec3){ scale[0], scale[1], scale[2] };
    }
    y += inspector_row_h + 12;

    /* ---- Terrain (the terrain object and its regions) ---- */
    const bool is_terrain_part = g_terrain.exists &&
        (obj->id == g_terrain.scene_object_id || get_terrain_region_of_object(obj->id) >= 0);
    if (is_terrain_part) {
        draw_inspector_terrain(cmd, ui_ctx, x, &y, w, limit);
    }
}
