#include "system_globals.h"
#include "system_scene_offscreen.h"
#include <stdarg.h>
#include <stdio.h>

/* status text goes to the statusbar (draw_system_statusbar.c reads status_bar_text_buffer). */
void set_sceneview_status(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), format, args);
    va_end(args);
}

/* Scene tab sub menubar, Maya panel toolbar style: flat square icon buttons, thin separators between groups,
   toggled-on buttons get the accent, the name of the hovered button shows at the right end.
   State lives in g_scene_view (system_element.h / system_globals.c). Lit = g_light.enabled, AA = g_prefs.fxaa.

   Everything below the glyphs is built from draw_icon_element / draw_icon_border (system_icon_element.c).
   To add a button: add an enum value, a glyph case, a row in k_buttons, and a case in scene_button_on / scene_button_click. */

const scene_button scene_view_buttons[] = {
    { scene_icon_lock,           "Camera Lock",          false },
    { scene_icon_home,           "Home (reset view)",    false },

    { scene_icon_grid,           "Grid",                 true  },
    { scene_icon_origin,         "Origin Marker",        false },
    { scene_icon_camera,         "Cameras + Frustum",    false },

    { scene_icon_wire,           "Wireframe",            true  },
    { scene_icon_shaded,         "Smooth Shade All",     false },
    { scene_icon_textured,       "Textured",             false },
    { scene_icon_wire_on_shaded, "Wireframe on Shaded",  false },
    { scene_icon_xray,           "X-Ray",                false },

    { scene_icon_lit,            "Use Lights (Lit)",     true  },
    { scene_icon_aa,             "Anti-aliasing (FXAA)", false },
};
system_color scaled_alpha(system_color c, float k) { c.alpha *= k; return c; }

/* ------------------------------------------------------------------ glyphs */

/* cube: front face + top + right side. d = depth offset, n = front face size */
void draw_cube_faces(VkCommandBuffer cmd, int ix, int iy, int d, int n, system_color front, system_color top, system_color side)
{
    draw_icon_element(cmd, ix, iy + d, n, n, front);
    for (int r = 0; r < d; r++)  draw_icon_element(cmd, ix + d - r, iy + r, n, 1, top);
    for (int k = 0; k < d; k++)  draw_icon_element(cmd, ix + n + k, iy + d - k, 1, n, side);
}

void draw_cube_wire(VkCommandBuffer cmd, int ix, int iy, int d, int n, system_color c)
{
    draw_icon_border(cmd, ix,     iy + d, n, n, c);          /* front */
    draw_icon_border(cmd, ix + d, iy,     n, n, c);          /* back  */
    for (int i = 0; i <= d; i++) {                           /* four connecting edges, front corner -> back corner */
        draw_icon_element(cmd, ix + i,         iy + d - i,         1, 1, c);   /* top left     */
        draw_icon_element(cmd, ix + n - 1 + i, iy + d - i,         1, 1, c);   /* top right    */
        draw_icon_element(cmd, ix + n - 1 + i, iy + d + n - 1 - i, 1, 1, c);   /* bottom right */
        draw_icon_element(cmd, ix + i,         iy + d + n - 1 - i, 1, 1, c);   /* bottom left  */
    }
}

/* s x s box at ix, iy */
void draw_scene_icon(VkCommandBuffer cmd, system_element_context* ui_element, enum_scene_icon icon, bool on, int ix, int iy, int s, system_color c)
{
    int d = s / 4; if (d < 2) d = 2;
    const int n = s - d;

    switch (icon) {
    case scene_icon_lock: {                           /* padlock: shackle open when unlocked */
        const int body_y = iy + s / 2 - 1;
        draw_icon_element(cmd, ix + 1, body_y, s - 2, s - (body_y - iy), c);
        draw_icon_element(cmd, ix + 3, iy, s - 6, 1, c);
        draw_icon_element(cmd, ix + 3, iy, 1, body_y - iy, c);
        if (on) draw_icon_element(cmd, ix + s - 4, iy, 1, body_y - iy, c);
        else    draw_icon_element(cmd, ix + s - 4, iy, 1, 3, c);
        break;
    }

    case scene_icon_home: {                           /* house */
        const int roof = s / 2;
        for (int i = 0; i < roof; i++) draw_icon_element(cmd, ix + s / 2 - i, iy + i, i * 2 + 1, 1, c);
        draw_icon_element(cmd, ix + 2, iy + roof, s - 4, s - roof - 1, c);
        break;
    }

    case scene_icon_grid:                             /* # : 4 lines each way */
        for (int i = 0; i < 4; i++) {
            const int o = i * (s - 1) / 3;
            draw_icon_element(cmd, ix, iy + o, s, 1, c);
            draw_icon_element(cmd, ix + o, iy, 1, s, c);
        }
        break;

    case scene_icon_origin: {                         /* axes from a corner: X red, Y green, Z blue dot */
        const system_color rx = { 1.0f,  0.25f, 0.25f, 1.0f };
        const system_color gy = { 0.35f, 1.0f,  0.35f, 1.0f };
        const system_color bz = { 0.3f,  0.55f, 1.0f,  1.0f };
        const int ox = ix + 2, oy = iy + s - 3;
        draw_icon_element(cmd, ox, oy, s - 4, 2, rx);
        draw_icon_element(cmd, ox, oy - (s - 5), 2, s - 5, gy);
        draw_icon_element(cmd, ox - 1 + (s - 6) / 3, oy - (s - 6) / 3 - 1, 3, 3, bz);
        break;
    }

    case scene_icon_camera: {                         /* body + lens wedge */
        const int bw = s - 6, bh = s - 7, bx = ix, by = iy + 3;
        draw_icon_border(cmd, bx, by, bw, bh, c);
        for (int i = 0; i < 3; i++) draw_icon_element(cmd, bx + bw + i, by + 2 - i + (bh - 5) / 2, 1, 1 + i * 2 + 1, c);
        draw_icon_element(cmd, bx + 2, by + 2, 2, 2, c);
        break;
    }

    case scene_icon_wire:                             /* cube, edges only */
        draw_cube_wire(cmd, ix, iy, d, n, c);
        break;

    case scene_icon_shaded:                           /* cube, solid faces, three brightness steps */
        draw_cube_faces(cmd, ix, iy, d, n, c, scaled_alpha(c, 0.70f), scaled_alpha(c, 0.40f));
        break;

    case scene_icon_textured: {                       /* cube, checker on the front face */
        const int q = (n / 3 > 1) ? n / 3 : 1;
        draw_cube_faces(cmd, ix, iy, d, n, scaled_alpha(c, 0.35f), scaled_alpha(c, 0.50f), scaled_alpha(c, 0.25f));
        for (int cy = 0; cy < 3; cy++)
            for (int cx = 0; cx < 3; cx++)
                if (((cx + cy) & 1) == 0) draw_icon_element(cmd, ix + cx * q, iy + d + cy * q, q, q, c);
        break;
    }

    case scene_icon_wire_on_shaded:                   /* dim faces, bright edges */
        draw_cube_faces(cmd, ix, iy, d, n, scaled_alpha(c, 0.35f), scaled_alpha(c, 0.25f), scaled_alpha(c, 0.15f));
        draw_cube_wire(cmd, ix, iy, d, n, c);
        break;

    case scene_icon_xray:                             /* edges all the way through, faint front fill */
        draw_icon_element(cmd, ix + 1, iy + d + 1, n - 2, n - 2, scaled_alpha(c, 0.25f));
        draw_cube_wire(cmd, ix, iy, d, n, c);
        break;

    case scene_icon_lit: {                            /* shaded sphere: outline + filled right half */
        const int r = s / 2 - 1, cx = ix + s / 2, cy = iy + s / 2;
        for (int dy = -r; dy <= r; dy++) {
            const int half = (int)(sqrtf((float)(r * r - dy * dy)) + 0.5f);
            if (dy == -r || dy == r) draw_icon_element(cmd, cx - half, cy + dy, half * 2 + 1, 1, c);
            else {
                draw_icon_element(cmd, cx - half, cy + dy, 2, 1, c);
                draw_icon_element(cmd, cx + half - 1, cy + dy, 2, 1, c);
            }
            draw_icon_element(cmd, cx, cy + dy, half, 1, c);
        }
        break;
    }

    case scene_icon_aa: {                             /* text, centred */
        const int tw = 2 * ui_font_w;
        draw_text_element(cmd, ui_element, "AA", ix + (s - tw) / 2, iy + (s - ui_font_h) / 2, c);
        break;
    }
    }
}

/* ------------------------------------------------------------ button state */

bool scene_button_on(enum_scene_icon icon)
{
    switch (icon) {
    case scene_icon_lock:           return g_scene_view.camera_locked;
    case scene_icon_grid:           return g_scene_view.show_grid;
    case scene_icon_origin:         return g_scene_view.show_origin;
    case scene_icon_camera:         return g_scene_view.show_cameras;
    case scene_icon_wire:           return g_scene_view.shading == scene_shading_wire;
    case scene_icon_shaded:         return g_scene_view.shading == scene_shading_shaded;
    case scene_icon_textured:       return g_scene_view.shading == scene_shading_textured;
    case scene_icon_wire_on_shaded: return g_scene_view.wire_on_shaded;
    case scene_icon_xray:           return g_scene_view.xray;
    case scene_icon_lit:            return get_terrain_lighting_enabled();
    case scene_icon_aa:             return g_prefs.fxaa;
    default:                        return false;     /* home is an action, never "on" */
    }
}

void scene_button_click(enum_scene_icon icon, world_camera* cam)
{
    switch (icon) {
    case scene_icon_lock:
        g_scene_view.camera_locked = !g_scene_view.camera_locked;
        if (g_scene_view.camera_locked) { g_scene_view.lock_position = cam->position; g_scene_view.lock_rotation = cam->rotation; }
        break;
    case scene_icon_home:
        focus_system_grid_on_selected_object();
        if (g_scene_view.camera_locked) { g_scene_view.lock_position = cam->position; g_scene_view.lock_rotation = cam->rotation; }
        break;
    case scene_icon_grid:           g_scene_view.show_grid      = !g_scene_view.show_grid;      break;
    case scene_icon_origin:         g_scene_view.show_origin    = !g_scene_view.show_origin;    break;
    case scene_icon_camera:         g_scene_view.show_cameras   = !g_scene_view.show_cameras;   break;
    case scene_icon_wire:           g_scene_view.shading = scene_shading_wire;                  break;
    case scene_icon_shaded:         g_scene_view.shading = scene_shading_shaded;                break;
    case scene_icon_textured:       g_scene_view.shading = scene_shading_textured;              break;
    case scene_icon_wire_on_shaded: g_scene_view.wire_on_shaded = !g_scene_view.wire_on_shaded; break;
    case scene_icon_xray:           g_scene_view.xray           = !g_scene_view.xray;           break;
    case scene_icon_lit:            set_terrain_lighting_enabled(!get_terrain_lighting_enabled()); break;   /* also rebuilds the terrain meshes: light is baked into their vertex colour */
    case scene_icon_aa:             g_prefs.fxaa    = !g_prefs.fxaa;                            break;
    }
}

/* tells the statusbar what a click just did. toggles say on / off, shading modes say which one is active. */
void report_scene_button(const scene_button* b)
{
    switch (b->icon) {
    case scene_icon_home:
        set_sceneview_status("Scene view: camera reset to home.");
        break;
    case scene_icon_wire:
    case scene_icon_shaded:
    case scene_icon_textured:
        set_sceneview_status("Shading: %s.", b->name);
        break;
    default:
        set_sceneview_status("%s: %s.", b->name, scene_button_on(b->icon) ? "on" : "off");
        break;
    }
}

/* ------------------------------------------------------------------ button */

/* one flat square button. returns true on click (press and release on it). */
bool draw_scene_icon_button(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id, enum_scene_icon icon,
                                   int x, int y, int s, bool on, const char* name, const char** hover_name)
{
    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    const bool hovered = mx >= x && mx < x + s && my >= y && my < y + s;
    if (hovered) *hover_name = name;

    if (on) {
        draw_icon_element(cmd, x, y, s, s, color_with_alpha(system_theme.system_accent, 0.60f));
        draw_icon_border (cmd, x, y, s, s, system_theme.system_accent);
    } else if (hovered) {
        draw_icon_element(cmd, x, y, s, s, process_theme_color(system_theme.tab_sub_menubar, system_theme.tab_hover_delta));
    }

    const int isz = (s - 6 > 8) ? s - 6 : 8;
    draw_scene_icon(cmd, ui_element, icon, on, x + (s - isz) / 2, y + (s - isz) / 2, isz,
                    (on || hovered) ? system_theme.tab_item_text_active : system_theme.tab_item_text);

    bool clicked = false;
    if (ui_element->input.mouse_down && hovered && ui_element->active_id == 0 && !ui_element->input.camera_manipulating) ui_element->active_id = id;
    if (!ui_element->input.mouse_down && ui_element->active_id == id) {
        ui_element->active_id = 0;
        clicked = hovered;
    }
    return clicked;
}

/* draws the Scene tab sub menubar, then the buttons on it */
void draw_scene_sub_menu(system_element_context* ui_element, system_dock_node* node, VkCommandBuffer cmd)
{
    const int x = node->x, y = node->y + tab_bar_h, w = node->w, h = tab_menu_h;

    draw_tab_sub_menubar(cmd, ui_element, x, y, w, h);

    world_camera* cam = &engine_context.world_state.editor_scene_camera;
    if (g_scene_view.camera_locked) { cam->position = g_scene_view.lock_position; cam->rotation = g_scene_view.lock_rotation; }

    int s = h - 5;                      /* h includes the 1px divider on the bottom row */
    if (s > 22) s = 22;
    if (s < 12) return;

    const int by = y + (h - 1 - s) / 2;
    const int gap = 2, sep = 7;
    const int right_limit = x + w - 6;
    int bx = x + 6;
    const char* hover_name = NULL;

    for (int i = 0; i < scene_button_count; i++) {
        const scene_button* b = &scene_view_buttons[i];

        if (b->separator_before) {
            if (bx + sep + s > right_limit) break;
            draw_icon_element(cmd, bx + sep / 2, by + 2, 1, s - 4, system_theme.tab_sub_menubar_divider);
            bx += sep;
        }
        if (bx + s > right_limit) break;          /* narrow window: drop what does not fit */

        if (draw_scene_icon_button(cmd, ui_element, dock_tab_id + 800u + (uint32_t)i, b->icon, bx, by, s,
                                   scene_button_on(b->icon), b->name, &hover_name))
        { scene_button_click(b->icon, cam); prefs_save(); report_scene_button(b); }   /* the view options are saved with the other editor options */
        bx += s + gap;
    }

    /* name of the hovered button, right aligned, only when it fits */
    if (hover_name) {
        const int tw = (int)strlen(hover_name) * ui_font_w;
        const int tx = right_limit - tw;
        if (tx > bx + 8) draw_text_element(cmd, ui_element, hover_name, tx, by + (s - ui_font_h) / 2, system_theme.tab_item_text);
    }
}

void draw_sceneview_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer current_cmd)
{
    /* this window's own tab sub menubar + buttons */
    draw_scene_sub_menu(ui_ctx, node, current_cmd);

    /* no backplate: the viewport quad IS the panel. Anything drawn here would show around it. */
    ui_ctx->scene_view.width    = node->w;
    ui_ctx->scene_view.height   = content_h;
    ui_ctx->scene_view.offset_x = node->x;
    ui_ctx->scene_view.offset_y = content_y;

    offscreen_request_size(offscreen_scene, (double)node->w, (double)content_h);

    VkDescriptorSet texture_set = render_context.scene_viewport_target.descriptor_set;

    // 1. Submit offscreen viewport image target presentation regions to queue commands
    draw_sceneview_viewport(ui_ctx, current_cmd, node->x, content_y, node->w, content_h, texture_set);

    // 2. Axis gimbal in the corner
    editor_vec3 selected_object_pos = { 0.0f, 0.0f, 0.0f };
    draw_sceneview_gimbal_overlay(current_cmd, ui_ctx, ui_ctx->scene_view, selected_object_pos);

    // 3. Selection: Object mode = orange outline, Edit mode = vert / edge_to_face_array / face picking + highlight.
    //    (The transform gizmo is still drawn afterwards from traverse_dock_tree, so it sits on top.)
    brush_ui_frame(ui_ctx);                  /* terrain brush + sculpt input + paint input, once per frame */
    system_edit_mode_update(ui_ctx);
    update_edit_ops(ui_ctx);                 /* modeling tools + double click row select. AFTER edit mode so its selection wins this frame */
    system_edit_mode_draw(current_cmd, ui_ctx);
    draw_edit_ops(current_cmd, ui_ctx);      /* loop preview, weld rubber band, status line */

    // 4. Delete key removes the obj_vert_selected object (and its children) while the mouse is over the Scene view
    system_scene_delete_hotkey(ui_ctx, node->x, content_y, node->w, content_h);

    // 4b. Brush ring (sculpt / paint), drawn over the overlays
    draw_brush_cursor(current_cmd, ui_ctx);

    // 5. Right click menu for this window (drawn on top later by draw_context_menu)
    context_menu_scene_window(ui_ctx, node->x, content_y, node->w, content_h);
}