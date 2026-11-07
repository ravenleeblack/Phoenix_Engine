#include "system_globals.h"
#include "system_textureshop_skins.h"

/* Skin Maker: the skin editor window of the Texture Shop mode, docked like Tex Editor and Tex Nodes.
   Layout, top to bottom: tab sub menubar (quick buttons), scrolling body (Skins / Layer Stack / Selected Layer / Masks),
   a Create + Export row, a status strip.  The data and the file work live in draw_system_textureshop.c. */

enum {
    sm_button_h = 22, sm_gap = 4, sm_pad = 8, sm_header_h = 20, sm_text_h = 16,
    sm_status_h = 18, sm_bottom_h = 30,
    sm_sub_gap = 2, sm_sub_sep = 7,
    sm_section_count = 4, sm_scroll_step = 60
};

typedef struct { int left, right, top, bottom, x, y, row_h; bool edge_to_face_array; } sm_cursor;

static struct {
    bool ready;
    bool prev_down;
    bool section_open[sm_section_count];
    int  scroll;           /* pixels the body is scrolled down */
    int  content_h;        /* height of the body last frame, so scroll can be clamped */
} g_sm;

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
        snprintf(label, sizeof(label), "%d  %-10s %s%s", i, l->name, letters, l->triplanar ? "  tri" : "");
        if (draw_sm_button(cmd, ui_element, c, label, true, s->selected_layer == i, system_theme.system_edit_mode_active_face_hover)) s->selected_layer = i;
    }
    if (draw_sm_button(cmd, ui_element, c, "+ Layer", false, false, system_theme.system_color_edit_mode_normal_color)) add_texshop_layer(s);
    if (draw_sm_button(cmd, ui_element, c, "- Layer", false, false, system_theme.toolbar_background)) remove_texshop_layer(s);
    if (draw_sm_button(cmd, ui_element, c, "Up",      false, false, system_theme.toolbar_background)) move_texshop_layer(s, 1);
    if (draw_sm_button(cmd, ui_element, c, "Down",    false, false, system_theme.toolbar_background)) move_texshop_layer(s, -1);
}

static const struct { const char* name; float r, g, b; } k_sm_preset[] = {
    { "Grey",  0.55f, 0.55f, 0.55f }, { "White", 0.95f, 0.95f, 0.95f }, { "Black", 0.05f, 0.05f, 0.05f },
    { "Red",   0.80f, 0.15f, 0.12f }, { "Green", 0.25f, 0.62f, 0.20f }, { "Blue",  0.20f, 0.35f, 0.80f },
    { "Sand",  0.76f, 0.66f, 0.42f }, { "Brown", 0.45f, 0.30f, 0.18f }
};

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

    snprintf(text, sizeof(text), "Color %.2f %.2f %.2f", l->color[0], l->color[1], l->color[2]);
    draw_sm_text(cmd, ui_element, c, text, system_theme.system_color_edit_mode_normal_color);
    snprintf(text, sizeof(text), "Opacity %.2f", l->opacity);
    draw_sm_text(cmd, ui_element, c, text, system_theme.system_color_edit_mode_normal_color);

    static const char* const k_step_label[8] = { "R -", "R +", "G -", "G +", "B -", "B +", "Op -", "Op +" };
    for (int p = 0; p < 8; p++) {
        if (draw_sm_button(cmd, ui_element, c, k_step_label[p], false, false, system_theme.toolbar_background)) {
            const float delta = (p & 1) ? 0.05f : -0.05f;
            if (p < 6) l->color[p / 2] = sm_step_value(l->color[p / 2], delta);
            else       l->opacity      = sm_step_value(l->opacity, delta);
        }
    }
    for (int p = 0; p < (int)(sizeof(k_sm_preset) / sizeof(k_sm_preset[0])); p++) {
        if (draw_sm_button(cmd, ui_element, c, k_sm_preset[p].name, false, false, system_theme.system_color_edit_mode_normal_color)) {
            l->color[0] = k_sm_preset[p].r; l->color[1] = k_sm_preset[p].g; l->color[2] = k_sm_preset[p].b;
        }
    }

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
        if (draw_sm_section(cmd, ui_ctx, &c, "Layer Stack", &open[1]))    draw_sm_layers_section(cmd, ui_ctx, &c, skin);
        if (draw_sm_section(cmd, ui_ctx, &c, "Selected Layer", &open[2])) draw_sm_layer_section(cmd, ui_ctx, &c, skin);
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
/* Texture Shop window: the shader profile side. Lists the profiles (skins), assigns them to the obj_vert_selected object, exports.  */
/* It lives in this file only to share the helpers above, it can move out once it has its own widgets.                      */
/* Next things that belong here, not in the Skin Maker: shading model, feature flags, texture inputs, exposed params.       */
/* ------------------------------------------------------------------------- */
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

    draw_sm_text(cmd, ui_ctx, &c, "Shader Profiles", system_theme.system_color_edit_mode_normal_color);

    static bool s_open = true;
    if (draw_sm_section(cmd, ui_ctx, &c, "Profiles", &s_open)) {
        draw_sm_skins_section(cmd, ui_ctx, &c);
        if (draw_sm_button(cmd, ui_ctx, &c, "Export", false, false, system_theme.system_edit_mode_active_face_hover)) export_texshop_skin();
    }

    const texshop_skin* s = get_texshop_skin();
    if (s) {
        char text[64];
        snprintf(text, sizeof(text), "%.16s: %d layer(s)", s->name, s->layer_count);
        draw_sm_text(cmd, ui_ctx, &c, text, system_theme.system_text);
        draw_sm_text(cmd, ui_ctx, &c, "Edit layers in Skin Maker.", system_theme.system_text);
    }

    push_quad_element(cmd, (float)node->x, (float)status_y, (float)node->w, (float)sm_status_h, system_theme.tab_content_window_textureshop_skin);
    draw_sm_text_at(cmd, ui_ctx, g_texshop.status, node->x + 8, status_y + 3, node->w - 12, system_theme.system_color_edit_mode_preview_color);
}