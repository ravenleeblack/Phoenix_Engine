#include "system_globals.h"

/* ========================================================================= */
/* the export options window: what to export, which format, then Export...    */
/* modal like the preferences window: while it is open the dock, toolbar and   */
/* menubar see no mouse button. it is drawn in the overlay stage, on top.      */
/* ========================================================================= */
bool export_prev_down    = false;
bool export_real_down    = false;
bool export_pressed      = false;
bool export_input_hidden = false;
bool export_prev_escape  = false;

int get_export_text_width(const char* text)
{
    return (int)strlen(text) * ui_font_w;
}

bool is_export_point_inside(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < (x + w) && py >= y && py < (y + h);
}

/* same look as the preferences buttons. returns true on the frame the button was pressed. */
bool draw_export_button(VkCommandBuffer cmd, system_element_context* ui_element, const char* label,
                        int x, int y, int w, int h, bool selected, bool enabled)
{
    bool hover = enabled && is_export_point_inside(ui_element->input.mouse_x, ui_element->input.mouse_y, x, y, w, h);

    system_color background;
    system_color text_color;
    if (!enabled) {
        background = system_theme.tab_item;
        text_color = get_theme_disabled_color();
    } else {
        background = system_theme.tab_sub_menubar_button;
        if (selected) {
            background = system_theme.tab_sub_menubar_button_active;
        }
        if (hover) {
            background = process_theme_color(background, system_theme.system_hover_delta);
        }
        text_color = system_theme.button_text;
    }

    push_rounded_quad(cmd, (float)x, (float)y, (float)w, (float)h, background, 3, corner_all);
    draw_text_element(cmd, ui_element, label, x + (w - get_export_text_width(label)) / 2, y + (h - ui_font_h) / 2, text_color);
    return hover && export_pressed;
}

void export_begin_frame(system_element_context* ui_ctx)
{
    if (!export_window_open || !ui_ctx) {
        return;
    }
    export_real_down = ui_ctx->input.mouse_down;
    export_pressed = export_real_down && !export_prev_down;
    export_prev_down = export_real_down;
    ui_ctx->input.mouse_down = false;
    export_input_hidden = true;
}

void export_end_frame(system_element_context* ui_ctx)
{
    if (ui_ctx && export_input_hidden) {
        ui_ctx->input.mouse_down = export_real_down;
        export_input_hidden = false;
    }
}

/* what the current source points at, for the line under the source buttons */
void describe_export_source(char* out, size_t out_size, bool* out_valid)
{
    *out_valid = false;
    if (export_source == export_source_scene_object) {
        const system_scene_object* obj = find_scene_object_by_id(g_scene_registry.selected_object_id);
        if (!obj || obj == &g_scene_registry.objects[0]) {
            snprintf(out, out_size, "%s", "Nothing selected in the scene.");
        } else if (obj->type != scene_object_mesh) {
            snprintf(out, out_size, "'%.30s' is not a mesh.", obj->name);
        } else {
            snprintf(out, out_size, "Object: %.40s", obj->name);
            *out_valid = true;
        }
        return;
    }

    const file_archive_node* node = file_archive_node_at(file_archive_selected());
    if (!node || node->kind != file_node_file) {
        snprintf(out, out_size, "%s", "No file selected in the File Archive.");
    } else if (!check_path_extension(node->name, ".fbx") && !check_path_extension(node->name, ".obj") && !check_path_extension(node->name, ".png")) {
        snprintf(out, out_size, "'%.30s' is not an .fbx, .obj or .png.", node->name);
    } else {
        snprintf(out, out_size, "File: %.40s", node->name);
        *out_valid = true;
    }
}

void draw_export_window(VkCommandBuffer cmd, system_element_context* ui_element, int screen_w, int screen_h)
{
    if (!export_window_open || !ui_element) {
        return;
    }

    const int W = 460;
    const int H = 330;
    const int X = (screen_w - W) / 2;
    const int Y = (screen_h - H) / 2;
    const int pad = 18;
    const int row_h = 24;

    const bool escape = check_window_focus() && ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0);
    if (escape && !export_prev_escape) {
        export_window_open = false;
    }
    export_prev_escape = escape;

    push_rounded_quad(cmd, (float)(X - 1), (float)(Y - 1), (float)(W + 2), (float)(H + 2), system_theme.system_border, 6, corner_all);
    push_rounded_quad(cmd, (float)X, (float)Y, (float)W, (float)H, system_theme.tab_content_window, 6, corner_all);
    push_rounded_quad(cmd, (float)X, (float)Y, (float)W, 28.0f, system_theme.tab_sub_menubar, 6, corner_top);
    draw_text_element(cmd, ui_element, "Export", X + pad, Y + (28 - ui_font_h) / 2, system_theme.system_active_text);
    if (draw_export_button(cmd, ui_element, "x", X + W - 28, Y + 4, 20, 20, false, true)) {
        export_window_open = false;
    }

    system_color separator = system_theme.menu_hort_separator;
    separator.alpha = 1.0f;

    int y = Y + 28 + pad;
    const int label_x = X + pad;

    /* ---- source ---- */
    draw_text_element(cmd, ui_element, "EXPORT WHAT", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, separator);
    y += ui_font_h + 14;

    if (draw_export_button(cmd, ui_element, "Selected object", label_x, y, 200, row_h, export_source == export_source_scene_object, true)) {
        export_source = export_source_scene_object;
        if (export_format == export_format_png) {
            export_format = export_format_fbx_binary;
        }
    }
    if (draw_export_button(cmd, ui_element, "Selected archive file", label_x + 210, y, 210, row_h, export_source == export_source_archive_file, true)) {
        export_source = export_source_archive_file;
    }
    y += row_h + 8;

    char description[96];
    bool source_valid = false;
    describe_export_source(description, sizeof(description), &source_valid);
    system_color description_color = get_theme_hint_color();
    if (source_valid) {
        description_color = system_theme.system_active_text;
    }
    draw_text_element(cmd, ui_element, description, label_x, y, description_color);
    y += ui_font_h + 18;

    /* ---- format ---- */
    draw_text_element(cmd, ui_element, "FORMAT", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, separator);
    y += ui_font_h + 14;

    const bool png_allowed = (export_source == export_source_archive_file);
    if (draw_export_button(cmd, ui_element, "FBX binary", label_x, y, 100, row_h, export_format == export_format_fbx_binary, true)) {
        export_format = export_format_fbx_binary;
    }
    if (draw_export_button(cmd, ui_element, "FBX ASCII", label_x + 108, y, 100, row_h, export_format == export_format_fbx_ascii, true)) {
        export_format = export_format_fbx_ascii;
    }
    if (draw_export_button(cmd, ui_element, "OBJ", label_x + 216, y, 100, row_h, export_format == export_format_obj, true)) {
        export_format = export_format_obj;
    }
    if (draw_export_button(cmd, ui_element, "PNG", label_x + 324, y, 100, row_h, export_format == export_format_png, png_allowed)) {
        export_format = export_format_png;
    }
    y += row_h + 10;

    char units_line[96];
    snprintf(units_line, sizeof(units_line), "Object space, %s units (change in Preferences > Import / Export).", get_unit_profile_name(g_unit_prefs.export_profile));
    draw_text_element(cmd, ui_element, units_line, label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "PNG exports an image from the File Archive.", label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "Export... opens the save dialog.", label_x, y, get_theme_hint_color());

    /* ---- footer ---- */
    const int footer_y = Y + H - pad - 28;
    if (draw_export_button(cmd, ui_element, "Export...", X + W - pad - 200, footer_y, 100, 28, false, source_valid)) {
        export_current_selection();
    }
    if (draw_export_button(cmd, ui_element, "Close", X + W - pad - 90, footer_y, 90, 28, false, true)) {
        export_window_open = false;
    }
}
