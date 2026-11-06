#include "engine_globals.h"
#include "system_globals.h"
#include "editor_input.h"

/* editor half of the mouse: ui hit testing and the maya camera (alt + lmb orbit, alt + mmb pan).
   the raw mouse state comes from the engine (input_is_button_down, get_mouse_position ...).
   state lives in the editor globals: g_mouse_prev_x, g_mouse_prev_y. */

/* precise axis aligned bounding box check for windows and panels */
bool validate_rectangle_hit(int mx, int my, int rx, int ry, int rw, int rh)
{
    return (mx >= rx && mx < (rx + rw) && my >= ry && my < (ry + rh));
}

/* pixel precise circular distance check for radial dials and icon buttons */
bool validate_circle_hit(int mx, int my, int cx, int cy, int radius)
{
    int dx = mx - cx;
    int dy = my - cy;
    return ((dx * dx) + (dy * dy)) <= (radius * radius);
}

/* ray casting (even odd) point in polygon test, locks mouse clicks into custom shapes */
bool validate_polygon_hit(int mx, int my, const struct_polygon_shape* polygon)
{
    if (!polygon || polygon->vertex_count < 3) return false;

    bool  inside = false;
    float px = (float)mx;
    float py = (float)my;

    for (int i = 0, j = polygon->vertex_count - 1; i < polygon->vertex_count; j = i++) {
        float vi_x = polygon->vertices[i].x;
        float vi_y = polygon->vertices[i].y;
        float vj_x = polygon->vertices[j].x;
        float vj_y = polygon->vertices[j].y;

        bool intersect = ((vi_y > py) != (vj_y > py)) &&
                         (px < (vj_x - vi_x) * (py - vi_y) / (vj_y - vi_y + 0.00001f) + vi_x);

        if (intersect) inside = !inside;
    }
    return inside;
}

/* trapezoid hit for the sloped, angled tab header buttons */
bool validate_sloped_tab_hit(int mx, int my, int tx, int ty, int tw, int th, int slope_padding)
{
    if (!validate_rectangle_hit(mx, my, tx, ty, tw, th)) return false;

    struct_polygon_shape tab_poly;
    tab_poly.vertex_count = 4;

    tab_poly.vertices[0].x = (float)tx;
    tab_poly.vertices[0].y = (float)(ty + th);                /* bottom left */

    tab_poly.vertices[1].x = (float)(tx + slope_padding);
    tab_poly.vertices[1].y = (float)ty;                       /* top left, sloped inward */

    tab_poly.vertices[2].x = (float)(tx + tw - slope_padding);
    tab_poly.vertices[2].y = (float)ty;                       /* top right, sloped inward */

    tab_poly.vertices[3].x = (float)(tx + tw);
    tab_poly.vertices[3].y = (float)(ty + th);                /* bottom right */

    return validate_polygon_hit(mx, my, &tab_poly);
}

/* immediate mode lock guard: hot and active ids */
void process_element_hit_state(system_element_context* context, unsigned int id, bool is_hovered)
{
    if (!context) return;

    if (is_hovered) {
        /* only grant the hot state if no other widget has captured an active interaction lock */
        if (context->active_id == 0) {
            context->hot_id = id;
        }

        if (context->input.mouse_down && context->active_id == 0) {
            context->active_id = id;
        }
    } else {
        /* drop the hot highlight as soon as the pointer leaves */
        if (context->hot_id == id) {
            context->hot_id = 0;
        }
    }
}

void update_element_input(system_element_context* context, int mouse_x, int mouse_y, bool mouse_down)
{
    if (!context) return;

    camera_component* cam = &engine_context.world_state.editor_scene_camera;

    int dx = mouse_x - g_mouse_prev_x;
    int dy = mouse_y - g_mouse_prev_y;

    g_mouse_prev_x = mouse_x;
    g_mouse_prev_y = mouse_y;

    context->input.mouse_x    = mouse_x;
    context->input.mouse_y    = mouse_y;
    context->input.mouse_down = mouse_down;

    /* physical key states from windows */
    context->input.alt_pressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    context->input.middle_down = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

    /* rule 1: hard intercept for the menu bar (top 30 pixels). stop any viewport camera manipulation up here. */
    if (mouse_y <= 30) {
        context->input.camera_manipulating = false;
        return;
    }

    /* rule 2: check the mouse against the inner scene view canvas */
    bool inside_viewport = (mouse_x >= context->scene_view.offset_x &&
                            mouse_x <= (context->scene_view.offset_x + context->scene_view.width) &&
                            mouse_y >= context->scene_view.offset_y &&
                            mouse_y <= (context->scene_view.offset_y + context->scene_view.height));

    /* rule 3: break ui lock contention while navigating the camera */
    if (inside_viewport && context->input.alt_pressed && (mouse_down || context->input.middle_down)) {
        context->input.camera_manipulating = true;
        context->active_id = 999999;     /* dummy id: blocks background elements from overriding this drag frame */
    } else if (!mouse_down && !context->input.middle_down) {
        if (context->input.camera_manipulating) {
            context->input.camera_manipulating = false;
            context->active_id = 0;      /* release focus */
        }
    }

    /* not manipulating the camera, or outside the canvas: no navigation */
    if (!inside_viewport || !context->input.camera_manipulating) return;

    /* maya camera */
    if (context->input.alt_pressed) {
        float sensitivity_orbit = 0.005f;
        float sensitivity_pan   = 0.02f * g_camera_prefs.pan_speed;   /* preferences > camera */

        /* orbit: alt + left click */
        if (mouse_down && !context->input.middle_down) {
            cam->rotation.y += (float)dx * sensitivity_orbit;
            cam->rotation.x -= (float)dy * sensitivity_orbit;      /* subtract dy: up / down look is inverted on purpose */

            if (cam->rotation.x > 1.4f)  cam->rotation.x = 1.4f;
            if (cam->rotation.x < -1.4f) cam->rotation.x = -1.4f;
        }

        /* pan: alt + middle click */
        if (context->input.middle_down) {
            float scale_x = cosf(cam->rotation.y);
            float scale_z = sinf(cam->rotation.y);

            cam->position.x -= (scale_x * (float)dx) * sensitivity_pan;
            cam->position.z -= (scale_z * (float)dx) * sensitivity_pan;
            cam->position.y += (float)dy * sensitivity_pan;
        }
    }
}