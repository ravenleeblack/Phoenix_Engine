#include "system_theme.h"
#include "system_globals.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Texture Shop Nodes: the node canvas that starts the Substance-style side of the Texture Shop.
   Docked as a tab next to the Tex Editor (the UV editor). Same UI style as the UV editor: quads + the 8x8 font,
   everything clipped to the canvas by hand. The add / delete / zoom buttons live in the tab sub menubar (same as the Scene tab).

   WHAT IT DOES TODAY: add nodes, drag them, link output -> input, pan, zoom, delete. Nothing is evaluated yet.
   WHAT IT IS FOR: a graph makes a texture or a mask (noise, gradient, curvature, paint map, levels, blend).
   A skin layer can then use "node graph output" as a source. */

enum {
    texnode_status_h = 18,
    texnode_sub_gap = 2, texnode_sub_sep = 7,
    texnode_max = 64, texnode_max_links = 128,
    texnode_port_hit = 7
};

typedef enum {
    texnode_kind_output = 0,    /* the graph result: feeds a skin layer */
    texnode_kind_color,
    texnode_kind_noise,
    texnode_kind_gradient,
    texnode_kind_texture,       /* a texture from the skin's pool */
    texnode_kind_paint,         /* an object paint map (what paint mode wrote) */
    texnode_kind_curvature,     /* baked curvature of the mesh */
    texnode_kind_levels,
    texnode_kind_blend,
    texnode_kind_count
} enum_texnode_kind;

typedef struct {
    const char* title;
    int         inputs;
    int         outputs;
    const char* in_name[2];
    const char* out_name;
} texnode_def;

static const texnode_def k_def[texnode_kind_count] = {
    { "Output",    1, 0, { "Layer", NULL }, NULL   },
    { "Color",     0, 1, { NULL, NULL },    "RGBA" },
    { "Noise",     0, 1, { NULL, NULL },    "Mask" },
    { "Gradient",  0, 1, { NULL, NULL },    "Mask" },
    { "Texture",   0, 1, { NULL, NULL },    "RGBA" },
    { "Paint Map", 0, 1, { NULL, NULL },    "RGBA" },
    { "Curvature", 0, 1, { NULL, NULL },    "Mask" },
    { "Levels",    1, 1, { "In", NULL },    "Out"  },
    { "Blend",     2, 1, { "A", "B" },      "Out"  }
};

typedef struct { bool used; int kind; float x, y; } texnode;
typedef struct { bool used; int from_node; int to_node; int to_port; } texlink;   /* every node has at most one output */
typedef struct { int x, y, w, h; } texnode_rect;
typedef struct { float x, y, w, h, head, row; } texnode_box;

typedef struct {
    bool    ready;
    float   pan_x, pan_y;           /* graph point at the middle of the canvas */
    float   zoom;
    texnode nodes[texnode_max];
    texlink links[texnode_max_links];
    int     obj_vert_selected;
    int     drag_node;
    float   drag_off_x, drag_off_y;
    int     link_from;              /* output being dragged out of, -1 = none */
    bool    prev_down;
    bool    panning;
    int     prev_mouse_x, prev_mouse_y;
    int     spawn_count;
    char    status[96];
} texnode_state;

static texnode_state g_nodes = { .zoom = 1.0f, .obj_vert_selected = -1, .drag_node = -1, .link_from = -1 };

static const system_color k_bg      = { 0.17f, 0.17f, 0.17f, 1.0f };
static const system_color k_minor   = { 0.20f, 0.20f, 0.20f, 1.0f };
static const system_color k_major   = { 0.25f, 0.25f, 0.25f, 1.0f };
static const system_color k_body    = { 0.26f, 0.26f, 0.27f, 1.0f };
static const system_color k_outline = { 0.62f, 0.62f, 0.62f, 1.0f };
static const system_color k_port    = { 0.85f, 0.85f, 0.85f, 1.0f };

static void set_status(const char* text) { snprintf(g_nodes.status, sizeof(g_nodes.status), "%s", text); }

/* ------------------------------------------------------------------------- */
/* graph <-> screen                                                          */
/* ------------------------------------------------------------------------- */
static float to_sx(const texnode_rect* vp, float gx) { return (float)vp->x + (float)vp->w * 0.5f + (gx - g_nodes.pan_x) * g_nodes.zoom; }
static float to_sy(const texnode_rect* vp, float gy) { return (float)vp->y + (float)vp->h * 0.5f + (gy - g_nodes.pan_y) * g_nodes.zoom; }
static float to_gx(const texnode_rect* vp, float sx) { return g_nodes.pan_x + (sx - ((float)vp->x + (float)vp->w * 0.5f)) / g_nodes.zoom; }
static float to_gy(const texnode_rect* vp, float sy) { return g_nodes.pan_y + (sy - ((float)vp->y + (float)vp->h * 0.5f)) / g_nodes.zoom; }

static texnode_box box_of(const texnode_rect* vp, const texnode* n)
{
    const texnode_def* d = &k_def[n->kind];
    int rows = d->inputs > d->outputs ? d->inputs : d->outputs;
    if (rows < 1) rows = 1;
    texnode_box b;
    b.x    = to_sx(vp, n->x);
    b.y    = to_sy(vp, n->y);
    b.w    = 140.0f * g_nodes.zoom;
    b.head = 18.0f  * g_nodes.zoom;
    b.row  = 16.0f  * g_nodes.zoom;
    b.h    = b.head + (float)rows * b.row + 6.0f * g_nodes.zoom;
    return b;
}

static void port_pos(const texnode_box* b, bool is_output, int port, float* px, float* py)
{
    *px = is_output ? b->x + b->w : b->x;
    *py = b->y + b->head + b->row * ((float)port + 0.5f);
}

static bool near_point(int mx, int my, float px, float py)
{
    const float dx = (float)mx - px, dy = (float)my - py;
    return dx * dx + dy * dy <= (float)(texnode_port_hit * texnode_port_hit);
}

/* ------------------------------------------------------------------------- */
/* clipped drawing                                                           */
/* ------------------------------------------------------------------------- */
static void fill_clipped(VkCommandBuffer cmd, const texnode_rect* vp, float x, float y, float w, float h, system_color c)
{
    const float x0 = fmaxf(x, (float)vp->x),               y0 = fmaxf(y, (float)vp->y);
    const float x1 = fminf(x + w, (float)(vp->x + vp->w)), y1 = fminf(y + h, (float)(vp->y + vp->h));
    if (x1 <= x0 || y1 <= y0) return;
    push_quad_element(cmd, x0, y0, x1 - x0, y1 - y0, c);
}

static void text_clipped(VkCommandBuffer cmd, system_element_context* ui_element, const texnode_rect* vp, const char* text, int x, int y, system_color c)
{
    const int len = (int)strlen(text) * ui_font_w;
    if (x < vp->x + 1 || y < vp->y || x + len > vp->x + vp->w - 1 || y + ui_font_h > vp->y + vp->h) return;
    draw_text_element(cmd, ui_element, text, x, y, c);
}

static void hline(VkCommandBuffer cmd, const texnode_rect* vp, float xa, float xb, float y, system_color c)
{
    fill_clipped(cmd, vp, fminf(xa, xb), y - 1.0f, fabsf(xb - xa) + 2.0f, 2.0f, c);
}

static void vline(VkCommandBuffer cmd, const texnode_rect* vp, float x, float ya, float yb, system_color c)
{
    fill_clipped(cmd, vp, x - 1.0f, fminf(ya, yb), 2.0f, fabsf(yb - ya) + 2.0f, c);
}

/* elbow link: horizontal, vertical, horizontal */
static void draw_link(VkCommandBuffer cmd, const texnode_rect* vp, float x0, float y0, float x1, float y1, system_color c)
{
    const float mid = (x0 + x1) * 0.5f;
    hline(cmd, vp, x0, mid, y0, c);
    vline(cmd, vp, mid, y0, y1, c);
    hline(cmd, vp, mid, x1, y1, c);
}

/* ------------------------------------------------------------------------- */
/* graph editing                                                             */
/* ------------------------------------------------------------------------- */
static system_color header_color(int kind)
{
    switch (kind) {
    case texnode_kind_output:    return system_theme.system_color_edit_mode_invalid_color;
    case texnode_kind_color:     return system_theme.system_color_edit_mode_preview_color;
    case texnode_kind_noise:     return system_theme.system_edit_mode_active_face_hover;
    case texnode_kind_gradient:  return system_theme.system_color_edit_mode_normal_color;
    case texnode_kind_texture:   return system_theme.system_color_edit_mode_face_active_color;
    case texnode_kind_paint:     return system_theme.system_color_edit_mode_face_selection_color;
    case texnode_kind_curvature: return system_theme.system_color_edit_mode_extrude_face_color;
    case texnode_kind_levels:    return system_theme.system_color_edit_mode_hover_color;
    default:                     return system_theme.system_border;
    }
}

static int add_node(int kind, float gx, float gy)
{
    for (int i = 0; i < texnode_max; i++) {
        if (g_nodes.nodes[i].used) continue;
        g_nodes.nodes[i].used = true;
        g_nodes.nodes[i].kind = kind;
        g_nodes.nodes[i].x    = gx;
        g_nodes.nodes[i].y    = gy;
        g_nodes.obj_vert_selected      = i;
        return i;
    }
    set_status("The graph is full (64 nodes).");
    return -1;
}

static void add_node_at_view_center(int kind)
{
    const float off = (float)((g_nodes.spawn_count++ % 6) * 24);
    if (add_node(kind, g_nodes.pan_x - 70.0f + off, g_nodes.pan_y - 30.0f + off) >= 0) set_status(k_def[kind].title);
}

static void remove_links_of(int node_index)
{
    for (int i = 0; i < texnode_max_links; i++)
        if (g_nodes.links[i].used && (g_nodes.links[i].from_node == node_index || g_nodes.links[i].to_node == node_index))
            g_nodes.links[i].used = false;
}

static void delete_selected(void)
{
    const int s = g_nodes.obj_vert_selected;
    if (s < 0 || !g_nodes.nodes[s].used) { set_status("Nothing obj_vert_selected."); return; }
    remove_links_of(s);
    g_nodes.nodes[s].used = false;
    g_nodes.obj_vert_selected = -1;
    if (g_nodes.link_from == s) g_nodes.link_from = -1;
    set_status("Node deleted.");
}

static void clear_graph(void)
{
    memset(g_nodes.nodes, 0, sizeof(g_nodes.nodes));
    memset(g_nodes.links, 0, sizeof(g_nodes.links));
    g_nodes.obj_vert_selected = -1; g_nodes.drag_node = -1; g_nodes.link_from = -1;
    add_node(texnode_kind_output, 200.0f, 0.0f);
    g_nodes.obj_vert_selected = -1;
    set_status("Graph cleared.");
}

static void fit_graph(void)
{
    float sx = 0.0f, sy = 0.0f; int n = 0;
    for (int i = 0; i < texnode_max; i++) if (g_nodes.nodes[i].used) { sx += g_nodes.nodes[i].x + 70.0f; sy += g_nodes.nodes[i].y + 20.0f; n++; }
    g_nodes.pan_x = n ? sx / (float)n : 0.0f;
    g_nodes.pan_y = n ? sy / (float)n : 0.0f;
    g_nodes.zoom  = 1.0f;
}

/* would linking from -> to close a loop?  yes when `to` already reaches `from` */
static bool reaches(int from, int target, bool* seen)
{
    if (from == target) return true;
    if (seen[from]) return false;
    seen[from] = true;
    for (int i = 0; i < texnode_max_links; i++)
        if (g_nodes.links[i].used && g_nodes.links[i].from_node == from && reaches(g_nodes.links[i].to_node, target, seen)) return true;
    return false;
}

static void try_link(int from, int to, int to_port)
{
    bool seen[texnode_max] = { false };
    if (from == to)                    { set_status("A node cannot feed itself."); g_nodes.link_from = -1; return; }
    if (reaches(to, from, seen))       { set_status("That link would make a loop."); g_nodes.link_from = -1; return; }

    for (int i = 0; i < texnode_max_links; i++)       /* one link per input */
        if (g_nodes.links[i].used && g_nodes.links[i].to_node == to && g_nodes.links[i].to_port == to_port) g_nodes.links[i].used = false;

    for (int i = 0; i < texnode_max_links; i++) {
        if (g_nodes.links[i].used) continue;
        g_nodes.links[i].used = true;
        g_nodes.links[i].from_node = from;
        g_nodes.links[i].to_node   = to;
        g_nodes.links[i].to_port   = to_port;
        set_status("Linked.");
        g_nodes.link_from = -1;
        return;
    }
    set_status("Too many links.");
    g_nodes.link_from = -1;
}

/* a left click inside the canvas */
static void click_canvas(const texnode_rect* vp, int mx, int my)
{
    for (int i = texnode_max - 1; i >= 0; i--) {         /* topmost node first */
        if (!g_nodes.nodes[i].used) continue;
        const texnode_def* d = &k_def[g_nodes.nodes[i].kind];
        const texnode_box  b = box_of(vp, &g_nodes.nodes[i]);
        float px, py;

        if (d->outputs > 0) {
            port_pos(&b, true, 0, &px, &py);
            if (near_point(mx, my, px, py)) {
                g_nodes.link_from = i; g_nodes.obj_vert_selected = i;
                set_status("Click an input port to finish the link.");
                return;
            }
        }
        for (int p = 0; p < d->inputs; p++) {
            port_pos(&b, false, p, &px, &py);
            if (!near_point(mx, my, px, py)) continue;
            if (g_nodes.link_from >= 0) {
                try_link(g_nodes.link_from, i, p);
            } else {                                      /* pick the link up again, like Substance */
                for (int l = 0; l < texnode_max_links; l++)
                    if (g_nodes.links[l].used && g_nodes.links[l].to_node == i && g_nodes.links[l].to_port == p) {
                        g_nodes.link_from = g_nodes.links[l].from_node;
                        g_nodes.links[l].used = false;
                        set_status("Link picked up. Click an input port to drop it.");
                        break;
                    }
            }
            return;
        }
        if ((float)mx >= b.x && (float)mx < b.x + b.w && (float)my >= b.y && (float)my < b.y + b.h) {
            g_nodes.link_from  = -1;
            g_nodes.obj_vert_selected   = i;
            g_nodes.drag_node  = i;
            g_nodes.drag_off_x = to_gx(vp, (float)mx) - g_nodes.nodes[i].x;
            g_nodes.drag_off_y = to_gy(vp, (float)my) - g_nodes.nodes[i].y;
            set_status(d->title);
            return;
        }
    }
    g_nodes.link_from = -1;
    g_nodes.obj_vert_selected  = -1;
}

/* ------------------------------------------------------------------------- */
/* drawing                                                                   */
/* ------------------------------------------------------------------------- */
static void draw_grid(VkCommandBuffer cmd, const texnode_rect* vp)
{
    const float step = (g_nodes.zoom < 0.6f) ? 40.0f : 20.0f;
    const float left  = to_gx(vp, (float)vp->x),            right  = to_gx(vp, (float)(vp->x + vp->w));
    const float top   = to_gy(vp, (float)vp->y),            bottom = to_gy(vp, (float)(vp->y + vp->h));

    for (int i = (int)floorf(left / step); (float)i * step <= right; i++) {
        const float gx = (float)i * step;
        vline(cmd, vp, floorf(to_sx(vp, gx)), (float)vp->y, (float)(vp->y + vp->h), (i % 5 == 0) ? k_major : k_minor);
    }
    for (int i = (int)floorf(top / step); (float)i * step <= bottom; i++) {
        const float gy = (float)i * step;
        hline(cmd, vp, (float)vp->x, (float)(vp->x + vp->w), floorf(to_sy(vp, gy)), (i % 5 == 0) ? k_major : k_minor);
    }
}

static void draw_node(VkCommandBuffer cmd, system_element_context* ui_element, const texnode_rect* vp, int index)
{
    const texnode*     n = &g_nodes.nodes[index];
    const texnode_def* d = &k_def[n->kind];
    const texnode_box  b = box_of(vp, n);
    const bool obj_vert_selected  = (g_nodes.obj_vert_selected == index);

    if (obj_vert_selected) fill_clipped(cmd, vp, b.x - 2.0f, b.y - 2.0f, b.w + 4.0f, b.h + 4.0f, system_theme.system_color_edit_mode_preview_color);
    else          fill_clipped(cmd, vp, b.x - 1.0f, b.y - 1.0f, b.w + 2.0f, b.h + 2.0f, k_outline);
    fill_clipped(cmd, vp, b.x, b.y, b.w, b.h, k_body);
    fill_clipped(cmd, vp, b.x, b.y, b.w, b.head, header_color(n->kind));

    if (g_nodes.zoom >= 0.8f) {
        text_clipped(cmd, ui_element, vp, d->title, (int)b.x + 6, (int)(b.y + (b.head - (float)ui_font_h) * 0.5f), system_theme.system_text);
        for (int p = 0; p < d->inputs; p++)
            if (d->in_name[p]) text_clipped(cmd, ui_element, vp, d->in_name[p], (int)b.x + 10, (int)(b.y + b.head + b.row * (float)p + (b.row - (float)ui_font_h) * 0.5f), system_theme.system_text);
        if (d->outputs > 0 && d->out_name) {
            const int tw = (int)strlen(d->out_name) * ui_font_w;
            text_clipped(cmd, ui_element, vp, d->out_name, (int)(b.x + b.w) - tw - 10, (int)(b.y + b.head + (b.row - (float)ui_font_h) * 0.5f), system_theme.system_text);
        }
    }

    float px, py;
    for (int p = 0; p < d->inputs; p++)  { port_pos(&b, false, p, &px, &py); fill_clipped(cmd, vp, px - 4.0f, py - 4.0f, 8.0f, 8.0f, k_port); }
    if (d->outputs > 0) {
        port_pos(&b, true, 0, &px, &py);
        if (g_nodes.link_from == index) {
            fill_clipped(cmd, vp, px - 4.0f, py - 4.0f, 8.0f, 8.0f, system_theme.system_color_edit_mode_preview_color);
        } else {
            fill_clipped(cmd, vp, px - 4.0f, py - 4.0f, 8.0f, 8.0f, k_port);
        }
    }
}

static void draw_graph(VkCommandBuffer cmd, system_element_context* ui_element, const texnode_rect* vp)
{
    fill_clipped(cmd, vp, (float)vp->x, (float)vp->y, (float)vp->w, (float)vp->h, k_bg);
    draw_grid(cmd, vp);

    for (int i = 0; i < texnode_max_links; i++) {
        const texlink* l = &g_nodes.links[i];
        if (!l->used || !g_nodes.nodes[l->from_node].used || !g_nodes.nodes[l->to_node].used) continue;
        const texnode_box a = box_of(vp, &g_nodes.nodes[l->from_node]);
        const texnode_box c = box_of(vp, &g_nodes.nodes[l->to_node]);
        float x0, y0, x1, y1;
        port_pos(&a, true, 0, &x0, &y0);
        port_pos(&c, false, l->to_port, &x1, &y1);
        draw_link(cmd, vp, x0, y0, x1, y1, system_theme.system_text);
    }

    if (g_nodes.link_from >= 0 && g_nodes.nodes[g_nodes.link_from].used) {
        const texnode_box a = box_of(vp, &g_nodes.nodes[g_nodes.link_from]);
        float x0, y0;
        port_pos(&a, true, 0, &x0, &y0);
        draw_link(cmd, vp, x0, y0, (float)ui_element->input.mouse_x, (float)ui_element->input.mouse_y, system_theme.system_color_edit_mode_preview_color);
    }

    for (int i = 0; i < texnode_max; i++) if (g_nodes.nodes[i].used) draw_node(cmd, ui_element, vp, i);
}

/* ------------------------------------------------------------------------- */
/* tab sub menubar (same look as the Scene tab)                              */
/* ------------------------------------------------------------------------- */
static bool draw_node_sub_button(VkCommandBuffer cmd, system_element_context* ui_element, const char* label, const char* name,
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

static void draw_node_sub_menu(VkCommandBuffer cmd, system_element_context* ui_element, const system_dock_node* node, bool edge_to_face_array)
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

    #define NODE_BUTTON(label, name, action) \
        do { const int bw = (int)strlen(label) * ui_font_w + 12; \
             if (bx + bw <= right_limit) { \
                 if (draw_node_sub_button(cmd, ui_element, label, name, bx, by, bw, s, edge_to_face_array, &hover_name)) { action; } \
                 bx += bw + texnode_sub_gap; } } while (0)
    #define NODE_SEPARATOR() \
        do { if (bx + texnode_sub_sep < right_limit) { \
                 draw_icon_element(cmd, bx + texnode_sub_sep / 2, by + 2, 1, s - 4, system_theme.tab_sub_menubar_divider); \
                 bx += texnode_sub_sep; } } while (0)

    NODE_BUTTON("Out", "Add Output node",    add_node_at_view_center(texnode_kind_output));
    NODE_BUTTON("Col", "Add Color node",     add_node_at_view_center(texnode_kind_color));
    NODE_BUTTON("Noi", "Add Noise node",     add_node_at_view_center(texnode_kind_noise));
    NODE_BUTTON("Grd", "Add Gradient node",  add_node_at_view_center(texnode_kind_gradient));
    NODE_BUTTON("Tex", "Add Texture node",   add_node_at_view_center(texnode_kind_texture));
    NODE_BUTTON("Pnt", "Add Paint Map node", add_node_at_view_center(texnode_kind_paint));
    NODE_BUTTON("Crv", "Add Curvature node", add_node_at_view_center(texnode_kind_curvature));
    NODE_BUTTON("Lvl", "Add Levels node",    add_node_at_view_center(texnode_kind_levels));
    NODE_BUTTON("Bld", "Add Blend node",     add_node_at_view_center(texnode_kind_blend));
    NODE_SEPARATOR();
    NODE_BUTTON("Del", "Delete obj_vert_selected node", delete_selected());
    NODE_BUTTON("Clr", "Clear graph",          clear_graph());
    NODE_SEPARATOR();
    NODE_BUTTON("+",   "Zoom In",  g_nodes.zoom = fminf(g_nodes.zoom * 1.25f, 2.0f));
    NODE_BUTTON("-",   "Zoom Out", g_nodes.zoom = fmaxf(g_nodes.zoom * 0.8f, 0.4f));
    NODE_BUTTON("Fit", "Fit",      fit_graph());
    #undef NODE_BUTTON
    #undef NODE_SEPARATOR

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
void draw_textureshop_node_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    const int x = node->x, w = node->w;

    /* the sub menubar sits under the tab bar; if the dock already left room for it, content_y is already below it */
    const int sub_bottom = node->y + tab_bar_h + tab_menu_h;
    const int top        = (content_y > sub_bottom) ? content_y : sub_bottom;
    if (w <= 0 || content_y + content_h - top <= texnode_status_h + 8) return;

    const bool edge_to_face_array = ui_ctx->input.mouse_down && !g_nodes.prev_down;
    g_nodes.prev_down = ui_ctx->input.mouse_down;

    const int status_y = content_y + content_h - texnode_status_h;
    const texnode_rect vp = { x, top, w, status_y - top };

    if (!g_nodes.ready) {
        g_nodes.ready = true;
        g_nodes.zoom  = 1.0f;
        clear_graph();
        set_status("Middle mouse or Alt + left mouse pans. Click an output port, then an input port.");
    }

    /* ---- tab sub menubar: add node / delete / zoom ---- */
    draw_node_sub_menu(cmd, ui_ctx, node, edge_to_face_array);

    /* ---- pan: middle mouse, or alt + left mouse (same as the UV editor) ---- */
    const int mx = ui_ctx->input.mouse_x, my = ui_ctx->input.mouse_y;
    const bool inside = validate_rectangle_hit(mx, my, vp.x, vp.y, vp.w, vp.h);
    {
        const bool drag = ui_ctx->input.middle_down || (ui_ctx->input.alt_pressed && ui_ctx->input.mouse_down);
        if (drag && (g_nodes.panning || inside)) {
            if (g_nodes.panning) {
                g_nodes.pan_x -= (float)(mx - g_nodes.prev_mouse_x) / g_nodes.zoom;
                g_nodes.pan_y -= (float)(my - g_nodes.prev_mouse_y) / g_nodes.zoom;
            }
            g_nodes.panning = true;
        } else {
            g_nodes.panning = false;
        }
        g_nodes.prev_mouse_x = mx;
        g_nodes.prev_mouse_y = my;
    }

    /* ---- clicks and node dragging ---- */
    if (!g_nodes.panning) {
        if (edge_to_face_array && inside && !ui_ctx->input.alt_pressed) click_canvas(&vp, mx, my);
        if (!ui_ctx->input.mouse_down) g_nodes.drag_node = -1;
        if (g_nodes.drag_node >= 0 && g_nodes.nodes[g_nodes.drag_node].used) {
            g_nodes.nodes[g_nodes.drag_node].x = to_gx(&vp, (float)mx) - g_nodes.drag_off_x;
            g_nodes.nodes[g_nodes.drag_node].y = to_gy(&vp, (float)my) - g_nodes.drag_off_y;
        }
    }

    draw_graph(cmd, ui_ctx, &vp);

    /* right click menu (Skin: new / duplicate / delete), drawn on top later by draw_context_menu */
    context_menu_textureshop_window(ui_ctx, vp.x, vp.y, vp.w, vp.h);

    /* ---- status strip ---- */
    push_quad_element(cmd, (float)x, (float)status_y, (float)w, (float)texnode_status_h, system_theme.tab_content_window_textureshop_node);
    {
        int node_count = 0, link_count = 0;
        for (int i = 0; i < texnode_max; i++)       if (g_nodes.nodes[i].used) node_count++;
        for (int i = 0; i < texnode_max_links; i++) if (g_nodes.links[i].used) link_count++;
        char text[200];
        snprintf(text, sizeof(text), "Nodes: %d   Links: %d   Zoom: %.0f%%   %s", node_count, link_count, (double)(g_nodes.zoom * 100.0f), g_nodes.status);
        draw_text_element(cmd, ui_ctx, text, x + 8, status_y + 3, system_theme.system_text);
    }
}