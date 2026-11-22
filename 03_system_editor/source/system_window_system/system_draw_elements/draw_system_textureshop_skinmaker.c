#include "system_globals.h"
#include "system_textureshop_skins.h"

/* Skin Maker: the skin editor window of the Texture Shop mode, docked like Tex Editor and Tex Nodes.
   Layout, top to bottom: tab sub menubar (quick buttons), scrolling body (Skins / Layer Stack / Selected Layer / Masks),
   a Create + Export row, a status strip.  The data and the file work live in draw_system_textureshop.c. */

enum {
    sm_button_h = 22, sm_gap = 4, sm_pad = 8, sm_header_h = 20, sm_text_h = 16,
    sm_status_h = 18, sm_bottom_h = 30,
    sm_sub_gap = 2, sm_sub_sep = 7,
    sm_section_count = 6, sm_scroll_step = 60,
    sm_slot_w = 76, sm_slot_h = 92, sm_thumb_px = 64, sm_thumb_cells = 16, sm_thumb_max = 24
};

typedef struct { int left, right, top, bottom, x, y, row_h; bool edge_to_face_array; } sm_cursor;

static struct {
    bool ready;
    bool prev_down;
    bool section_open[sm_section_count];
    int  scroll;           /* pixels the body is scrolled down */
    int  content_h;        /* height of the body last frame, so scroll can be clamped */
} g_sm;

/* small pictures of the maps (16 x 16), made once per file so the slots can show what is in them */
typedef struct { char name[32]; bool used; bool failed; uint8_t rgb[sm_thumb_cells * sm_thumb_cells * 3]; } sm_thumb;
static sm_thumb g_thumbs[sm_thumb_max];
static int      g_thumb_next = 0;

extern load_image_asset_ptr   editor_load_image;
extern remove_image_asset_ptr editor_remove_image;
bool check_loader_ready(void);
void set_tooltip_text(const char* text);

/* ------------------------------------------------------------------------- */
/* small drawing helpers                                                     */
/* ------------------------------------------------------------------------- */
/* text cut to the width it is given: the 8x8 font does not clip, a long line would run into the next window */
static void draw_sm_text_at(VkCommandBuffer cmd, system_element_context* ui_element, const char* text, int x, int y, int max_w, system_color color)
{
    char buf[96];
    int max_chars = max_w / ui_font_w;
    if (max_chars < 1) return;
    if (max_chars > (int)sizeof(buf) - 1) max_chars = (int)sizeof(buf) - 1;
    snprintf(buf, sizeof(buf), "%.*s", max_chars, text);
    draw_text_element(cmd, ui_element, buf, x, y, color);
}

/* the dock can swallow clicks, so buttons use the raw rising edge_to_face_array of the mouse (edge_to_face_array) */
static bool draw_sm_flat_button(VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, int w, int h, const char* label,
                                bool center, bool active, system_color active_color, bool edge_to_face_array)
{
    const bool hovered = validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, x, y, w, h);
    system_color body;
    if (active) {
        body = active_color;
    } else if (hovered) {
        body = system_theme.system_text;
    } else {
        body = system_theme.toolbar_background;
    }
    if (active && hovered) body = process_theme_color(body, system_theme.tab_hover_delta);
    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, body);

    int tx = center ? x + (w - (int)strlen(label) * ui_font_w) / 2 : x + 6;
    if (tx < x + 2) tx = x + 2;
    draw_sm_text_at(cmd, ui_element, label, tx, y + (h - ui_font_h) / 2, w - (tx - x) - 2, system_theme.system_text);
    return hovered && edge_to_face_array;
}

/* ------------------------------------------------------------------------- */
/* flow layout: buttons wrap to the next line, sections collapse, rows clip to the body */
/* ------------------------------------------------------------------------- */
static bool row_visible(const sm_cursor* c, int y, int h) { return y >= c->top && y + h <= c->bottom; }

static void finish_sm_row(sm_cursor* c)
{
    if (c->row_h > 0) c->y += c->row_h + sm_gap;
    c->x = c->left;
    c->row_h = 0;
}

static bool draw_sm_button(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c, const char* label,
                           bool full_row, bool active, system_color active_color)
{
    int w = full_row ? (c->right - c->left) : ((int)strlen(label) * ui_font_w + 14);
    if (full_row || (c->x + w > c->right && c->x > c->left)) finish_sm_row(c);
    if (w > c->right - c->x) w = c->right - c->x;

    const int bx = c->x, by = c->y;
    c->x += w + sm_gap;
    c->row_h = sm_button_h;
    if (w < 8 || !row_visible(c, by, sm_button_h)) return false;
    return draw_sm_flat_button(cmd, ui_element, bx, by, w, sm_button_h, label, !full_row, active, active_color, c->edge_to_face_array);
}

static void draw_sm_text(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c, const char* text, system_color color)
{
    finish_sm_row(c);
    if (row_visible(c, c->y, sm_text_h)) draw_sm_text_at(cmd, ui_element, text, c->left, c->y + 2, c->right - c->left, color);
    c->y += sm_text_h;
}

static bool draw_sm_section(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c, const char* label, bool* open)
{
    finish_sm_row(c);
    c->y += 2;
    const int hx = c->left, hw = c->right - c->left, hy = c->y;
    if (row_visible(c, hy, sm_header_h)) {
        const bool hovered = validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, hx, hy, hw, sm_header_h);
        system_color header_color = system_theme.system_color_edit_mode_preview_color;
        if (hovered) {
            header_color = system_theme.system_text;
        }
        push_quad_element(cmd, (float)hx, (float)hy, (float)hw, (float)sm_header_h, header_color);
        char text[64];
        snprintf(text, sizeof(text), "%s  %s", *open ? "-" : "+", label);
        draw_sm_text_at(cmd, ui_element, text, hx + 6, hy + (sm_header_h - ui_font_h) / 2, hw - 8, system_theme.system_text);
        if (hovered && c->edge_to_face_array) *open = !*open;
    }
    c->y += sm_header_h + sm_gap;
    return *open;
}

/* ------------------------------------------------------------------------- */
/* sections                                                                  */
/* ------------------------------------------------------------------------- */
static void draw_sm_skins_section(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c)
{
    if (g_texshop.skin_count == 0) draw_sm_text(cmd, ui_element, c, "No skin yet. Press Create.", system_theme.system_text);

    for (int i = 0; i < g_texshop.skin_count; i++) {
        const texshop_skin* sk = &g_texshop.skins[i];
        char label[48];
        snprintf(label, sizeof(label), "%d  %.20s%s", i, sk->name, texshop_skin_is_dirty(sk) ? " *" : "");
        if (draw_sm_button(cmd, ui_element, c, label, true, g_texshop.selected_skin == i, system_theme.system_edit_mode_active_face_hover)) g_texshop.selected_skin = i;
    }
    if (draw_sm_button(cmd, ui_element, c, "+ Skin", false, false, system_theme.system_color_edit_mode_normal_color)) create_texshop_skin();
    if (draw_sm_button(cmd, ui_element, c, "Copy",   false, false, system_theme.toolbar_background)) duplicate_texshop_skin();
    if (draw_sm_button(cmd, ui_element, c, "- Skin", false, false, system_theme.toolbar_background)) delete_texshop_skin();

    /* assign: the obj_vert_selected skin goes onto the object obj_vert_selected in the Hierarchy */
    if (draw_sm_button(cmd, ui_element, c, "Assign", false, false, system_theme.system_edit_mode_active_face_hover)) assign_texshop_skin_to_selected();
    if (draw_sm_button(cmd, ui_element, c, "Clear",  false, false, system_theme.toolbar_background)) clear_texshop_skin_on_selected();

    char line[64];
    texshop_describe_target(line, sizeof(line));
    draw_sm_text(cmd, ui_element, c, line, system_theme.system_color_edit_mode_normal_color);
    draw_sm_text(cmd, ui_element, c, "* = not exported yet", system_theme.system_text);
}

static void draw_sm_layers_section(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c, texshop_skin* s)
{
    draw_sm_text(cmd, ui_element, c, "Bottom to top.", system_theme.system_text);
    for (int i = s->layer_count - 1; i >= 0; i--) {                    /* top layer first, like a layer panel */
        const texshop_layer* l = &s->layers[i];
        char letters[texshop_out_count + 1];
        int n = 0;
        for (int o = 0; o < texshop_out_count; o++) if (l->channel_mask & texshop_bit(o)) letters[n++] = k_texshop_output_letter[o];
        letters[n] = '\0';

        char label[80];
        snprintf(label, sizeof(label), "%d  %-9.9s %s%s", i, l->name, letters, l->triplanar ? " tri" : "");
        finish_sm_row(c);
        const int row_y = c->y;
        const int row_w = c->right - c->left;
        if (row_visible(c, row_y, sm_button_h)) {
            if (draw_sm_flat_button(cmd, ui_element, c->left, row_y, row_w - 30, sm_button_h, label, false, s->selected_layer == i,
                                    system_theme.system_edit_mode_active_face_hover, c->edge_to_face_array)) {
                s->selected_layer = i;
            }
            const system_color swatch = { .red = l->color[0], .green = l->color[1], .blue = l->color[2], .alpha = 1.0f };
            push_rounded_quad(cmd, (float)(c->left + row_w - 26), (float)(row_y + 2), 26.0f, (float)(sm_button_h - 4), swatch, 5, corner_all);
        }
        c->y += sm_button_h + sm_gap;
    }
    if (draw_sm_button(cmd, ui_element, c, "+ Layer", false, false, system_theme.system_color_edit_mode_normal_color)) add_texshop_layer(s);
    if (draw_sm_button(cmd, ui_element, c, "- Layer", false, false, system_theme.toolbar_background)) remove_texshop_layer(s);
    if (draw_sm_button(cmd, ui_element, c, "Up",      false, false, system_theme.toolbar_background)) move_texshop_layer(s, 1);
    if (draw_sm_button(cmd, ui_element, c, "Down",    false, false, system_theme.toolbar_background)) move_texshop_layer(s, -1);
}

static float sm_step_value(float vert_to_edge_array, float delta)
{
    vert_to_edge_array += delta;
    if (vert_to_edge_array < 0.0f) vert_to_edge_array = 0.0f;
    if (vert_to_edge_array > 1.0f) vert_to_edge_array = 1.0f;
    return floorf(vert_to_edge_array * 100.0f + 0.5f) / 100.0f;           /* keep it on 0.01 so repeated steps do not drift */
}

static void draw_sm_layer_section(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c, texshop_skin* s)
{
    texshop_layer* l = get_texshop_layer(s);
    char text[64];
    snprintf(text, sizeof(text), "Layer %d: %s", s->selected_layer, l->name);
    draw_sm_text(cmd, ui_element, c, text, system_theme.system_text);

    /* color: the same picker and palette as the Paint window (hue bar, saturation / value square, hex, Save and Del for your own saved colors, 60 preset colors) */
    {
        finish_sm_row(c);
        const int picker_w = c->right - c->left;
        const int picker_h = brush_color_picker_height();
        const int palette_h = brush_color_palette_height(picker_w);
        draw_sm_text(cmd, ui_element, c, "Color", system_theme.system_color_edit_mode_normal_color);
        if (c->y >= c->top && c->y + picker_h <= c->bottom) {
            brush_color_picker(cmd, ui_element, c->left, c->y, picker_w, picker_h, l->color);
        }
        c->y += picker_h + 6;
        if (c->y >= c->top && c->y + palette_h <= c->bottom) {
            brush_color_palette(cmd, ui_element, c->left, c->y, picker_w, palette_h, l->color);
        }
        c->y += palette_h + 6;
    }
    snprintf(text, sizeof(text), "Opacity %.2f", l->opacity);
    draw_sm_text(cmd, ui_element, c, text, system_theme.system_color_edit_mode_normal_color);
    if (draw_sm_button(cmd, ui_element, c, "Op -", false, false, system_theme.toolbar_background)) l->opacity = sm_step_value(l->opacity, -0.05f);
    if (draw_sm_button(cmd, ui_element, c, "Op +", false, false, system_theme.toolbar_background)) l->opacity = sm_step_value(l->opacity, 0.05f);
    draw_sm_text(cmd, ui_element, c, "Writes", system_theme.system_text);
    for (int o = 0; o < texshop_out_count; o++) {
        const bool on = (l->channel_mask & texshop_bit(o)) != 0;
        if (draw_sm_button(cmd, ui_element, c, k_texshop_output_name[o], false, on, system_theme.system_edit_mode_active_face_hover)) l->channel_mask ^= texshop_bit(o);
    }

    draw_sm_text(cmd, ui_element, c, "Blend", system_theme.system_text);
    for (int mesh = 0; mesh < texshop_blend_count; mesh++)
        if (draw_sm_button(cmd, ui_element, c, k_texshop_blend_name[mesh], false, l->blend == (enum_texshop_blend)mesh, system_theme.system_edit_mode_active_face_hover))
            l->blend = (enum_texshop_blend)mesh;

    draw_sm_text(cmd, ui_element, c, "Projection", system_theme.system_text);
    if (draw_sm_button(cmd, ui_element, c, "UV",        false, !l->triplanar, system_theme.system_edit_mode_active_face_hover)) l->triplanar = false;
    if (draw_sm_button(cmd, ui_element, c, "Triplanar", false,  l->triplanar, system_theme.system_edit_mode_active_face_hover)) l->triplanar = true;
}

static float sm_step_range(float value, float delta, float lo, float hi)
{
    value += delta;
    if (value < lo) value = lo;
    if (value > hi) value = hi;
    return floorf(value * 100.0f + 0.5f) / 100.0f;
}

/* one value row: "Label 0.25" then - and + buttons on the same row */
static void draw_sm_value_row(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c, const char* label,
                              float* value, float lo, float hi)
{
    char text[40];
    snprintf(text, sizeof(text), "%s %.2f", label, *value);
    draw_sm_text(cmd, ui_element, c, text, system_theme.system_color_edit_mode_normal_color);
    char minus[24], plus[24];
    snprintf(minus, sizeof(minus), "%.4s -", label);
    snprintf(plus,  sizeof(plus),  "%.4s +", label);
    if (draw_sm_button(cmd, ui_element, c, minus, false, false, system_theme.toolbar_background)) *value = sm_step_range(*value, -0.05f, lo, hi);
    if (draw_sm_button(cmd, ui_element, c, plus,  false, false, system_theme.toolbar_background)) *value = sm_step_range(*value,  0.05f, lo, hi);
}

/* the png picked in the File Archive becomes the map. it must be in assets/textures (a PNG import puts it there). */
void set_skin_map_from_archive(char* map_file, size_t cap)
{
    const file_archive_node* node = file_archive_node_at(file_archive_selected());
    if (!node || node->kind != file_node_file) {
        set_texshop_status("Select a PNG in the Files window first.");
        return;
    }
    const char* dot = strrchr(node->name, '.');
    if (!dot || _stricmp(dot, ".png") != 0) {
        set_texshop_status("The selected file is not a PNG.");
        return;
    }
    if (strlen(node->name) >= cap) {
        set_texshop_status("That file name is too long (31 characters max).");
        return;
    }
    snprintf(map_file, cap, "%s", node->name);
    set_texshop_status("Map set from the Files window.");
}

static const sm_thumb* get_sm_thumb(const char* file)
{
    for (int i = 0; i < sm_thumb_max; i++) {
        if (g_thumbs[i].used && strcmp(g_thumbs[i].name, file) == 0) {
            return &g_thumbs[i];
        }
    }
    sm_thumb* t = &g_thumbs[g_thumb_next];
    g_thumb_next = (g_thumb_next + 1) % sm_thumb_max;
    memset(t, 0, sizeof(*t));
    snprintf(t->name, sizeof(t->name), "%s", file);
    t->used = true;
    t->failed = true;
    if (!check_loader_ready()) {
        return t;
    }
    char path[260];
    snprintf(path, sizeof(path), FILE_ARCHIVE_ROOT_FOLDER "\\textures\\%s", file);
    loader_image_data image;
    memset(&image, 0, sizeof(image));
    if (!editor_load_image(path, &image) || !image.pixels || image.width <= 0 || image.height <= 0) {
        return t;
    }
    for (int cy = 0; cy < sm_thumb_cells; cy++) {
        for (int cx = 0; cx < sm_thumb_cells; cx++) {
            const int sx = (cx * image.width + image.width / 2) / sm_thumb_cells;
            const int sy = (cy * image.height + image.height / 2) / sm_thumb_cells;
            const uint8_t* p = &image.pixels[((size_t)sy * (size_t)image.width + (size_t)sx) * 4u];
            uint8_t* o = &t->rgb[(cy * sm_thumb_cells + cx) * 3];
            o[0] = p[0]; o[1] = p[1]; o[2] = p[2];
        }
    }
    t->failed = false;
    editor_remove_image(&image);
    return t;
}

/* one map slot: a little box with the picture, the name under it. click the box = take the PNG picked in the Files window, x = empty the slot */
static void draw_sm_slot_box(VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, const char* label, char* file, size_t cap, const char* tip, bool edge)
{
    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    const bool hovered = validate_rectangle_hit(mx, my, x, y, sm_slot_w, sm_slot_h);
    const int bx = x + (sm_slot_w - sm_thumb_px) / 2;
    const int by = y + 2;

    system_color frame = system_theme.system_border;
    if (hovered) {
        frame = system_theme.system_text;
    } else if (file[0]) {
        frame = system_theme.system_edit_mode_active_face_hover;
    }
    push_rounded_quad(cmd, (float)(bx - 2), (float)(by - 2), (float)(sm_thumb_px + 4), (float)(sm_thumb_px + 4), frame, 6, corner_all);
    const system_color back = { 0.12f, 0.12f, 0.13f, 1.0f };
    push_rounded_quad(cmd, (float)bx, (float)by, (float)sm_thumb_px, (float)sm_thumb_px, back, 5, corner_all);

    if (file[0]) {
        const sm_thumb* t = get_sm_thumb(file);
        if (!t->failed) {
            const int cell = sm_thumb_px / sm_thumb_cells;
            for (int cy = 0; cy < sm_thumb_cells; cy++) {
                for (int cx = 0; cx < sm_thumb_cells; cx++) {
                    const uint8_t* p = &t->rgb[(cy * sm_thumb_cells + cx) * 3];
                    const system_color c = { (float)p[0] / 255.0f, (float)p[1] / 255.0f, (float)p[2] / 255.0f, 1.0f };
                    push_quad_element(cmd, (float)(bx + cx * cell), (float)(by + cy * cell), (float)cell, (float)cell, c);
                }
            }
        } else {
            draw_sm_text_at(cmd, ui_element, "missing", bx + 6, by + sm_thumb_px / 2 - 4, sm_thumb_px - 8, system_theme.system_color_edit_mode_invalid_color);
        }
    } else {
        draw_sm_text_at(cmd, ui_element, "empty", bx + 14, by + sm_thumb_px / 2 - 4, sm_thumb_px - 8, system_theme.system_text);
    }

    /* x: empty the slot */
    bool clear_hover = false;
    if (file[0]) {
        const int cx0 = bx + sm_thumb_px - 14, cy0 = by + 2;
        clear_hover = validate_rectangle_hit(mx, my, cx0, cy0, 12, 12);
        push_circle_element(cmd, (float)(cx0 + 6), (float)(cy0 + 6), 7.0f, 7.0f, clear_hover ? system_theme.system_color_edit_mode_invalid_color : system_theme.toolbar_background);
        draw_sm_text_at(cmd, ui_element, "x", cx0 + 2, cy0 + 2, 10, system_theme.system_text);
    }

    int tx = x + (sm_slot_w - (int)strlen(label) * ui_font_w) / 2;
    if (tx < x) {
        tx = x;
    }
    draw_sm_text_at(cmd, ui_element, label, tx, by + sm_thumb_px + 6, sm_slot_w, system_theme.system_text);

    if (hovered) {
        set_tooltip_text(tip);
        if (edge) {
            if (clear_hover) {
                file[0] = '\0';
                set_texshop_status("Map emptied.");
            } else {
                set_skin_map_from_archive(file, cap);
            }
        }
    }
}

/* the skin as a lit sphere (what the Texture Shop window shows), with its layer color and the maps it carries */
static void draw_sm_preview_section(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c, texshop_skin* s)
{
    finish_sm_row(c);
    const int box_h = 112;
    if (row_visible(c, c->y, box_h)) {
        const system_color back = { 0.10f, 0.10f, 0.11f, 1.0f };
        push_rounded_quad(cmd, (float)c->left, (float)c->y, (float)(c->right - c->left), (float)box_h, back, 8, corner_all);
        float rgb[3];
        texshop_get_preview_color(s, rgb);
        const float cx = (float)c->left + 60.0f;
        const float cy = (float)c->y + (float)box_h * 0.5f;
        draw_icon_sphere(cmd, cx, cy, 46.0f, rgb);
        int maps = 0;
        maps += s->map_file[0] != '\0';
        maps += s->height_file[0] != '\0';
        maps += s->normal_file[0] != '\0';
        maps += s->ambient_file[0] != '\0';
        maps += s->occlusion_file[0] != '\0';
        maps += s->spec_file[0] != '\0';
        char line[48];
        const int tx = c->left + 120;
        const int tw = c->right - tx - 4;
        draw_sm_text_at(cmd, ui_element, s->name, tx, c->y + 14, tw, system_theme.system_text);
        snprintf(line, sizeof(line), "%d layer(s)", s->layer_count);
        draw_sm_text_at(cmd, ui_element, line, tx, c->y + 34, tw, system_theme.system_color_edit_mode_normal_color);
        snprintf(line, sizeof(line), "%d of 6 maps", maps);
        draw_sm_text_at(cmd, ui_element, line, tx, c->y + 52, tw, system_theme.system_color_edit_mode_normal_color);
        snprintf(line, sizeof(line), "spec %.2f gloss %.2f", s->specular, s->gloss);
        draw_sm_text_at(cmd, ui_element, line, tx, c->y + 70, tw, system_theme.system_text);
    }
    c->y += box_h + sm_gap;
}

static void draw_sm_surface_section(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c, texshop_skin* s)
{
    draw_sm_text(cmd, ui_element, c, "Maps: click a box to take the PNG picked in Files", system_theme.system_text);

    /* the boxes, as many per row as fit */
    finish_sm_row(c);
    const char* const labels[6] = { "Color", "Height", "Normal", "Ambient", "Occlusion", "Specular" };
    char* const files[6] = { s->map_file, s->height_file, s->normal_file, s->ambient_file, s->occlusion_file, s->spec_file };
    const char* const tips[6] = {
        "Color map: the picture of the surface",
        "Height map: bumps (grey, white = high)",
        "Normal map: fine surface detail (tangent space)",
        "Ambient map: how much ambient light each spot gets",
        "Occlusion map: darkens ambient light and highlights in cracks",
        "Specular map: where the surface shines"
    };
    int per_row = (c->right - c->left + sm_gap) / (sm_slot_w + sm_gap);
    if (per_row < 1) {
        per_row = 1;
    }
    for (int i = 0; i < 6; i++) {
        const int sx = c->left + (i % per_row) * (sm_slot_w + sm_gap);
        const int sy = c->y + (i / per_row) * (sm_slot_h + sm_gap);
        if (row_visible(c, sy, sm_slot_h)) {
            draw_sm_slot_box(cmd, ui_element, sx, sy, labels[i], files[i], 32, tips[i], c->edge_to_face_array);
        }
    }
    c->y += ((6 + per_row - 1) / per_row) * (sm_slot_h + sm_gap);

    draw_sm_text(cmd, ui_element, c, "Surface values", system_theme.system_text);
    draw_sm_value_row(cmd, ui_element, c, "Specular", &s->specular, 0.0f, 1.0f);
    draw_sm_value_row(cmd, ui_element, c, "Gloss", &s->gloss, 0.0f, 1.0f);
    draw_sm_value_row(cmd, ui_element, c, "Height", &s->height_scale, 0.0f, 1.0f);
    draw_sm_value_row(cmd, ui_element, c, "Ambient", &s->ambient, 0.0f, 2.0f);
}
static void draw_sm_masks_section(VkCommandBuffer cmd, system_element_context* ui_element, sm_cursor* c, texshop_skin* s)
{
    texshop_layer* l = get_texshop_layer(s);
    draw_sm_text(cmd, ui_element, c, "Weight = masks x each other", system_theme.system_text);

    if (l->mask_count == 0) draw_sm_text(cmd, ui_element, c, "No masks: applies everywhere", system_theme.system_text);
    for (int i = 0; i < l->mask_count; i++) {
        char label[48];
        snprintf(label, sizeof(label), "x  %s", k_texshop_mask_name[l->masks[i]]);
        if (draw_sm_button(cmd, ui_element, c, label, false, true, system_theme.system_color_edit_mode_preview_color)) { remove_texshop_mask(l, i); break; }
    }

    draw_sm_text(cmd, ui_element, c, "Add mask (max 4)", system_theme.system_text);
    for (int mesh = 0; mesh < texshop_mask_count; mesh++)
        if (draw_sm_button(cmd, ui_element, c, k_texshop_mask_name[mesh], false, false, system_theme.system_color_edit_mode_normal_color)) {
            if (l->mask_count >= texshop_max_masks) set_texshop_status("Mask limit is 4 per layer.");
            else add_texshop_mask(l, (enum_texshop_mask)mesh);
        }
}

/* ------------------------------------------------------------------------- */
/* tab sub menubar (same look as Tex Editor / Tex Nodes)                     */
/* ------------------------------------------------------------------------- */
static bool draw_sm_sub_button(VkCommandBuffer cmd, system_element_context* ui_element, const char* label, const char* name,
                               int x, int y, int w, int h, bool edge_to_face_array, const char** hover_name)
{
    const bool hovered = validate_rectangle_hit(ui_element->input.mouse_x, ui_element->input.mouse_y, x, y, w, h);
    if (hovered) *hover_name = name;
    if (hovered) draw_icon_element(cmd, x, y, w, h, process_theme_color(system_theme.tab_sub_menubar, system_theme.tab_hover_delta));

    const int tw = (int)strlen(label) * ui_font_w;
    draw_text_element(cmd, ui_element, label, x + (w - tw) / 2, y + (h - ui_font_h) / 2,
                      hovered ? system_theme.tab_item_text_active : system_theme.tab_item_text);
    return hovered && edge_to_face_array;
}

static void act_add_layer(void)    { texshop_skin* s = get_texshop_skin(); if (s) add_texshop_layer(s); }
static void act_remove_layer(void) { texshop_skin* s = get_texshop_skin(); if (s) remove_texshop_layer(s); }
static void act_layer_up(void)     { texshop_skin* s = get_texshop_skin(); if (s) move_texshop_layer(s, 1); }
static void act_layer_down(void)   { texshop_skin* s = get_texshop_skin(); if (s) move_texshop_layer(s, -1); }

/* most useful first: whatever does not fit the width is left out (the same actions are in the body) */
static void draw_sm_sub_menu(VkCommandBuffer cmd, system_element_context* ui_element, const system_dock_node* node, bool edge_to_face_array)
{
    const int x = node->x, y = node->y + tab_bar_h, w = node->w, h = tab_menu_h;
    draw_tab_sub_menubar(cmd, ui_element, x, y, w, h);

    int s = h - 5;
    if (s > 22) s = 22;
    if (s < 12) return;

    const int by = y + (h - 1 - s) / 2;
    const int right_limit = x + w - 6;
    int bx = x + 6;
    const char* hover_name = NULL;

    #define SM_BUTTON(label, name, action) \
        do { const int bw = (int)strlen(label) * ui_font_w + 12; \
             if (bx + bw <= right_limit) { \
                 if (draw_sm_sub_button(cmd, ui_element, label, name, bx, by, bw, s, edge_to_face_array, &hover_name)) { action; } \
                 bx += bw + sm_sub_gap; } } while (0)
    #define SM_SEPARATOR() \
        do { if (bx + sm_sub_sep < right_limit) { \
                 draw_icon_element(cmd, bx + sm_sub_sep / 2, by + 2, 1, s - 4, system_theme.tab_sub_menubar_divider); \
                 bx += sm_sub_sep; } } while (0)

    SM_BUTTON("New", "New skin",                   create_texshop_skin());
    SM_BUTTON("Exp", "Export skin to assets/skins", export_texshop_skin());
    SM_BUTTON("Asg", "Assign to obj_vert_selected object",  assign_texshop_skin_to_selected());
    SM_BUTTON("Clr", "Clear skin on object",       clear_texshop_skin_on_selected());
    SM_SEPARATOR();
    SM_BUTTON("Cpy", "Duplicate skin",             duplicate_texshop_skin());
    SM_BUTTON("Del", "Delete skin",                delete_texshop_skin());
    SM_BUTTON("<",   "Previous skin",              step_texshop_skin(-1));
    SM_BUTTON(">",   "Next skin",                  step_texshop_skin(1));
    SM_SEPARATOR();
    SM_BUTTON("+L",  "Add layer",                  act_add_layer());
    SM_BUTTON("-L",  "Remove layer",               act_remove_layer());
    SM_BUTTON("Up",  "Move layer up",              act_layer_up());
    SM_BUTTON("Dn",  "Move layer down",            act_layer_down());
    SM_SEPARATOR();
    SM_BUTTON("^",   "Scroll up",                  g_sm.scroll -= sm_scroll_step);
    SM_BUTTON("vert_to_edge_array",   "Scroll down",                g_sm.scroll += sm_scroll_step);
    #undef SM_BUTTON
    #undef SM_SEPARATOR

    /* name of the hovered button, right aligned, only when it fits */
    if (hover_name) {
        const int tw = (int)strlen(hover_name) * ui_font_w;
        const int tx = right_limit - tw;
        if (tx > bx + 8) draw_text_element(cmd, ui_element, hover_name, tx, by + (s - ui_font_h) / 2, system_theme.tab_item_text);
    }
}

/* ------------------------------------------------------------------------- */
/* the window                                                                */
/* ------------------------------------------------------------------------- */
void draw_textureshop_skinmaker_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Skin Maker", node->x, content_y, node->w, content_h, system_theme.tab_content_window_textureshop_editor);

    const int x = node->x, w = node->w;
    /* the sub menubar sits under the tab bar; if the dock already left room for it, content_y is already below it */
    const int sub_bottom = node->y + tab_bar_h + tab_menu_h;
    const int top        = (content_y > sub_bottom) ? content_y : sub_bottom;
    const int bottom     = content_y + content_h;
    if (w < 60 || bottom - top < sm_status_h + sm_bottom_h + 40) return;

    check_texshop_ready();
    if (!g_sm.ready) {
        for (int i = 0; i < sm_section_count; i++) g_sm.section_open[i] = true;
        g_sm.section_open[3] = false;                       /* Masks start closed, the panel is narrow */
        g_sm.ready = true;
    }

    const bool edge_to_face_array = ui_ctx->input.mouse_down && !g_sm.prev_down;
    g_sm.prev_down = ui_ctx->input.mouse_down;

    draw_sm_sub_menu(cmd, ui_ctx, node, edge_to_face_array);

    const int status_y    = bottom - sm_status_h;
    const int button_y    = status_y - sm_bottom_h;
    const int body_bottom = button_y - 2;
    const int view_h      = body_bottom - top;

    int max_scroll = g_sm.content_h - view_h;
    if (max_scroll < 0) max_scroll = 0;
    if (g_sm.scroll > max_scroll) g_sm.scroll = max_scroll;
    if (g_sm.scroll < 0) g_sm.scroll = 0;

    context_menu_textureshop_window(ui_ctx, x, top, w, view_h);          /* right click: Skin menu */

    /* ---- body ---- */
    sm_cursor c;
    c.left = x + sm_pad;
    c.right = x + w - sm_pad;
    c.top = top;
    c.bottom = body_bottom;
    c.x = c.left;
    c.y = top + sm_pad - g_sm.scroll;
    c.row_h = 0;
    c.edge_to_face_array = edge_to_face_array;
    const int start_y = c.y;

    bool* open = g_sm.section_open;
    if (draw_sm_section(cmd, ui_ctx, &c, "Skins", &open[0])) draw_sm_skins_section(cmd, ui_ctx, &c);

    texshop_skin* skin = get_texshop_skin();
    if (skin) {
        if (draw_sm_section(cmd, ui_ctx, &c, "Preview", &open[5]))        draw_sm_preview_section(cmd, ui_ctx, &c, skin);
        if (draw_sm_section(cmd, ui_ctx, &c, "Layer Stack", &open[1]))    draw_sm_layers_section(cmd, ui_ctx, &c, skin);
        if (draw_sm_section(cmd, ui_ctx, &c, "Selected Layer", &open[2])) draw_sm_layer_section(cmd, ui_ctx, &c, skin);
        if (draw_sm_section(cmd, ui_ctx, &c, "Maps and Surface", &open[4]))        draw_sm_surface_section(cmd, ui_ctx, &c, skin);
        if (draw_sm_section(cmd, ui_ctx, &c, "Masks", &open[3]))          draw_sm_masks_section(cmd, ui_ctx, &c, skin);
    }
    finish_sm_row(&c);
    g_sm.content_h = c.y - start_y + 2 * sm_pad;

    /* ---- bottom: Create, then Export ---- */
    push_quad_element(cmd, (float)x, (float)button_y, (float)w, (float)sm_bottom_h, system_theme.toolbar_background);
    {
        const int bw = (w - 3 * sm_gap) / 2, bh = sm_bottom_h - 8, by = button_y + 4;
        if (draw_sm_flat_button(cmd, ui_ctx, x + sm_gap, by, bw, bh, "Create", true, true, system_theme.system_color_edit_mode_normal_color, edge_to_face_array))
            create_texshop_skin();
        if (draw_sm_flat_button(cmd, ui_ctx, x + 2 * sm_gap + bw, by, bw, bh, "Export", true, true, system_theme.system_edit_mode_active_face_hover, edge_to_face_array))
            export_texshop_skin();
    }

    /* ---- status strip ---- */
    push_quad_element(cmd, (float)x, (float)status_y, (float)w, (float)sm_status_h, system_theme.toolbar_background);
    draw_sm_text_at(cmd, ui_ctx, g_texshop.status, x + 8, status_y + 3, w - 12, system_theme.system_color_edit_mode_preview_color);

    texshop_sync_skin_gpu();                            /* an edit above changed the obj_vert_selected skin: rebuild its GPU copy */
}

/* ------------------------------------------------------------------------- */
/* Texture Shop window: the skin library. Every skin is a lit sphere, like the material balls of a sculpting tool.          */
/* Click one to pick it, Assign / Clear put it on the selected object. Making and editing skins is the Skin Maker's job.   */
/* Next things that belong here: shading model, feature flags, texture inputs, exposed params, the render preview.          */
/* ------------------------------------------------------------------------- */
enum { ts_sphere_radius = 30, ts_cell_w = 92, ts_cell_h = 98 };

void draw_textureshop_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    draw_window_element(cmd, ui_ctx, "Texture Shop", node->x, content_y, node->w, content_h, system_theme.tab_content_window_textureshop);
    if (node->w < 60 || content_h < 40 + sm_status_h) return;

    check_texshop_ready();
    context_menu_textureshop_window(ui_ctx, node->x, content_y, node->w, content_h);   /* right click: Skin menu */

    static bool prev_down = false;                      /* own edge_to_face_array: the Skin Maker keeps its own */
    const bool edge_to_face_array = ui_ctx->input.mouse_down && !prev_down;
    prev_down = ui_ctx->input.mouse_down;

    const int status_y = content_y + content_h - sm_status_h;

    sm_cursor c;
    c.left = node->x + sm_pad;
    c.right = node->x + node->w - sm_pad;
    c.top = content_y;
    c.bottom = status_y - 2;
    c.x = c.left;
    c.y = content_y + sm_pad;
    c.row_h = 0;
    c.edge_to_face_array = edge_to_face_array;

    draw_sm_text(cmd, ui_ctx, &c, "Skins", system_theme.system_color_edit_mode_normal_color);

    /* the sphere grid */
    if (g_texshop.skin_count == 0) {
        draw_sm_text(cmd, ui_ctx, &c, "No skin yet. Make one in the Skin Maker.", system_theme.system_text);
    }
    int columns = (c.right - c.left) / ts_cell_w;
    if (columns < 1) columns = 1;
    const int grid_top = c.y + 4;
    for (int i = 0; i < g_texshop.skin_count; i++) {
        const int cell_x = c.left + (i % columns) * ts_cell_w;
        const int cell_y = grid_top + (i / columns) * ts_cell_h;
        if (cell_y + ts_cell_h > c.bottom) {
            break;                                      /* the rest would run over the buttons below */
        }
        texshop_skin* skin = &g_texshop.skins[i];
        float rgb[3];
        texshop_get_preview_color(skin, rgb);

        const float cx = (float)(cell_x + ts_cell_w / 2);
        const float cy = (float)(cell_y + ts_sphere_radius + 4);
        const float dx = (float)ui_ctx->input.mouse_x - cx;
        const float dy = (float)ui_ctx->input.mouse_y - cy;
        const bool hovered = (dx * dx + dy * dy) <= (float)((ts_sphere_radius + 3) * (ts_sphere_radius + 3));
        if (hovered && edge_to_face_array) {
            g_texshop.selected_skin = i;
        }

        if (g_texshop.selected_skin == i) {
            push_circle_element(cmd, cx, cy, (float)(ts_sphere_radius + 5), 2.0f, system_theme.system_edit_mode_active_face_hover);
        } else if (hovered) {
            push_circle_element(cmd, cx, cy, (float)(ts_sphere_radius + 5), 1.0f, system_theme.system_text);
        }
        draw_icon_sphere(cmd, cx, cy, (float)ts_sphere_radius, rgb);

        char label[32];
        snprintf(label, sizeof(label), "%.10s", skin->name);
        draw_sm_text_at(cmd, ui_ctx, label, cell_x + (ts_cell_w - (int)strlen(label) * ui_font_w) / 2,
                        cell_y + 2 * ts_sphere_radius + 12, ts_cell_w - 4, system_theme.system_text);
    }
    if (g_texshop.skin_count > 0) {
        const int rows = (g_texshop.skin_count + columns - 1) / columns;
        c.y = grid_top + rows * ts_cell_h;
        c.x = c.left;
        c.row_h = 0;
    }

    /* use the picked skin */
    if (draw_sm_button(cmd, ui_ctx, &c, "Assign", false, false, system_theme.system_edit_mode_active_face_hover)) assign_texshop_skin_to_selected();
    if (draw_sm_button(cmd, ui_ctx, &c, "Clear",  false, false, system_theme.toolbar_background)) clear_texshop_skin_on_selected();

    char line[64];
    texshop_describe_target(line, sizeof(line));
    draw_sm_text(cmd, ui_ctx, &c, line, system_theme.system_color_edit_mode_normal_color);

    const texshop_skin* s = get_texshop_skin();
    if (s) {
        char text[64];
        snprintf(text, sizeof(text), "%.16s: %d layer(s)", s->name, s->layer_count);
        draw_sm_text(cmd, ui_ctx, &c, text, system_theme.system_text);
        if (s->map_file[0]) {
            snprintf(text, sizeof(text), "map: %.24s", s->map_file);
            draw_sm_text(cmd, ui_ctx, &c, text, system_theme.system_text);
        }
        draw_sm_text(cmd, ui_ctx, &c, "Edit the skin in Skin Maker.", system_theme.system_text);
    }

    push_quad_element(cmd, (float)node->x, (float)status_y, (float)node->w, (float)sm_status_h, system_theme.tab_content_window_textureshop_skin);
    draw_sm_text_at(cmd, ui_ctx, g_texshop.status, node->x + 8, status_y + 3, node->w - 12, system_theme.system_color_edit_mode_preview_color);
}