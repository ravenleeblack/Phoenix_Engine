#include "system_globals.h"
#include <stdarg.h>
#include <stdio.h>

/* status text goes to the statusbar (draw_system_statusbar.c reads status_bar_text_buffer). */
static void set_toolbar_status(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), format, args);
    va_end(args);
}


enum { toolbar_btn_w = 80, toolbar_btn_h = 20 };
enum { toolbar_icon_btn_w = 28, toolbar_icon_size = 16 };

enum {
    toolbar_icon_cube,
    toolbar_icon_camera,
    toolbar_icon_terrain,
    toolbar_icon_octagon,
    toolbar_icon_diamond,
    toolbar_icon_half,
    toolbar_icon_quarter,
    toolbar_icon_sun,
    toolbar_icon_point,
    toolbar_icon_spot,
    toolbar_icon_area,
    toolbar_icon_hemisphere
};

static int get_toolbar_icon_abs(int value)
{
    return value < 0 ? -value : value;
}

/* 1 px line between two points. horizontal and vertical lines are a single quad,
   diagonals step one pixel at a time (bresenham). */
static void draw_toolbar_icon_line(VkCommandBuffer cmd, int x0, int y0, int x1, int y1, system_color color)
{
    if (y0 == y1) {
        int left = x0 < x1 ? x0 : x1;
        push_quad_element(cmd, (float)left, (float)y0, (float)(get_toolbar_icon_abs(x1 - x0) + 1), 1.0f, color);
        return;
    }
    if (x0 == x1) {
        int top = y0 < y1 ? y0 : y1;
        push_quad_element(cmd, (float)x0, (float)top, 1.0f, (float)(get_toolbar_icon_abs(y1 - y0) + 1), color);
        return;
    }

    int dx = get_toolbar_icon_abs(x1 - x0);
    int dy = -get_toolbar_icon_abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        push_quad_element(cmd, (float)x0, (float)y0, 1.0f, 1.0f, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* closed outline through a list of x,y pairs. points are relative to the icon origin. */
static void draw_toolbar_icon_polygon(VkCommandBuffer cmd, int x, int y, const int* points, int point_count, system_color color)
{
    for (int i = 0; i < point_count; i++) {
        int next = (i + 1) % point_count;
        draw_toolbar_icon_line(cmd,
                               x + points[i * 2],    y + points[i * 2 + 1],
                               x + points[next * 2], y + points[next * 2 + 1],
                               color);
    }
}

/* open line through a list of x,y pairs. points are relative to the icon origin. */
static void draw_toolbar_icon_polyline(VkCommandBuffer cmd, int x, int y, const int* points, int point_count, system_color color)
{
    for (int i = 0; i + 1 < point_count; i++) {
        draw_toolbar_icon_line(cmd,
                               x + points[i * 2],       y + points[i * 2 + 1],
                               x + points[(i + 1) * 2], y + points[(i + 1) * 2 + 1],
                               color);
    }
}

/* tile box for board pieces that live on a border. the faint outline is the tile,
   the full strength edges (bottom, left) are the board border the piece sits against. */
static void draw_toolbar_icon_box(VkCommandBuffer cmd, int x, int y, int size, int edge_bottom, int edge_left, system_color color)
{
    system_color faint = color;
    faint.alpha = 0.45f;

    /* faint outline */
    push_quad_element(cmd, (float)x,              (float)y,              (float)size, 1.0f, faint);
    push_quad_element(cmd, (float)(x + size - 1), (float)y,              1.0f, (float)size, faint);
    push_quad_element(cmd, (float)x,              (float)(y + size - 1), (float)size, 1.0f, faint);
    push_quad_element(cmd, (float)x,              (float)y,              1.0f, (float)size, faint);

    /* bold border edges */
    if (edge_bottom) {
        push_quad_element(cmd, (float)x, (float)(y + size - 2), (float)size, 2.0f, color);
    }
    if (edge_left) {
        push_quad_element(cmd, (float)x, (float)y, 2.0f, (float)size, color);
    }
}

/* octagon: eight edges, corner cut of 5 on a 16 px icon */
static void draw_toolbar_icon_octagon(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int points[] = {
        5, 0,   10, 0,   15, 5,   15, 10,
        10, 15, 5, 15,   0, 10,   0, 5
    };
    draw_toolbar_icon_polygon(cmd, x, y, points, 8, color);
}

/* diamond: outline plus faint diagonals. the diagonals show where the diamond is cut
   into two halves and four quarters. */
static void draw_toolbar_icon_diamond(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int points[] = {
        8, 1,   15, 8,   8, 15,   1, 8
    };

    system_color faint = color;
    faint.alpha = 0.45f;
    draw_toolbar_icon_line(cmd, x + 8, y + 1, x + 8, y + 15, faint);
    draw_toolbar_icon_line(cmd, x + 1, y + 8, x + 15, y + 8, faint);

    draw_toolbar_icon_polygon(cmd, x, y, points, 4, color);
}

/* half: a diamond cut along one diagonal. flat side on the bottom border, point inward.
   base is 14 wide and height is 7, the same scale as the diamond icon. */
static void draw_toolbar_icon_triangle_half(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    static const int points[] = {
        1, 13,   15, 13,   8, 6
    };
    draw_toolbar_icon_polygon(cmd, x, y, points, 3, color);
    draw_toolbar_icon_box(cmd, x, y, size, 1, 0, color);
}

/* quarter: the half cut again. right angle in the bottom left corner, both legs are 7,
   so it is half the size of the half. */
static void draw_toolbar_icon_triangle_quarter(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    static const int points[] = {
        2, 13,   9, 13,   2, 6
    };
    draw_toolbar_icon_polygon(cmd, x, y, points, 3, color);
    draw_toolbar_icon_box(cmd, x, y, size, 1, 1, color);
}

/* cube: front square, back square, and the four connecting edges.
   the hidden back edges are drawn faint. */
static void draw_toolbar_icon_cube(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int front[] = {
        0, 5,   10, 5,   10, 15,   0, 15
    };

    system_color faint = color;
    faint.alpha = 0.4f;

    /* hidden edges */
    draw_toolbar_icon_line(cmd, x + 5, y + 0,  x + 5,  y + 10, faint);
    draw_toolbar_icon_line(cmd, x + 5, y + 10, x + 15, y + 10, faint);
    draw_toolbar_icon_line(cmd, x + 0, y + 15, x + 5,  y + 10, faint);

    /* back top edge_to_face_array and right edge_to_face_array */
    draw_toolbar_icon_line(cmd, x + 5,  y + 0, x + 15, y + 0,  color);
    draw_toolbar_icon_line(cmd, x + 15, y + 0, x + 15, y + 10, color);

    /* connecting edges */
    draw_toolbar_icon_line(cmd, x + 0,  y + 5,  x + 5,  y + 0,  color);
    draw_toolbar_icon_line(cmd, x + 10, y + 5,  x + 15, y + 0,  color);
    draw_toolbar_icon_line(cmd, x + 10, y + 15, x + 15, y + 10, color);

    /* front face */
    draw_toolbar_icon_polygon(cmd, x, y, front, 4, color);
}

/* camera: body outline, small bump on top, lens as a flared outline on the right */
static void draw_toolbar_icon_camera(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int body[] = {
        0, 4,   10, 4,   10, 14,   0, 14
    };
    static const int bump[] = {
        2, 2,   6, 2,   6, 4,   2, 4
    };
    static const int lens[] = {
        10, 7,   15, 3,   15, 15,   10, 11
    };

    draw_toolbar_icon_polygon(cmd, x, y, body, 4, color);
    draw_toolbar_icon_polygon(cmd, x, y, bump, 4, color);
    draw_toolbar_icon_polygon(cmd, x, y, lens, 4, color);
}

/* terrain: ridge line over a ground line, with faint mesh lines dropping from the peaks */
static void draw_toolbar_icon_terrain(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int ridge[] = {
        0, 13,   5, 3,   9, 9,   11, 6,   15, 13
    };

    system_color faint = color;
    faint.alpha = 0.4f;

    /* faint mesh under the ridge */
    draw_toolbar_icon_line(cmd, x + 5,  y + 3, x + 5,  y + 13, faint);
    draw_toolbar_icon_line(cmd, x + 9,  y + 9, x + 9,  y + 13, faint);
    draw_toolbar_icon_line(cmd, x + 11, y + 6, x + 11, y + 13, faint);
    draw_toolbar_icon_line(cmd, x + 5,  y + 8, x + 9,  y + 13, faint);

    draw_toolbar_icon_polyline(cmd, x, y, ridge, 5, color);
    draw_toolbar_icon_line(cmd, x + 0, y + 13, x + 15, y + 13, color);
}

/* sun (directional light): small octagon core, eight short rays */
static void draw_toolbar_icon_sun(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int core[] = {
        7, 5,   9, 5,   11, 7,   11, 9,
        9, 11,  7, 11,   5, 9,    5, 7
    };
    draw_toolbar_icon_polygon(cmd, x, y, core, 8, color);

    draw_toolbar_icon_line(cmd, x + 8,  y + 0,  x + 8,  y + 2,  color);
    draw_toolbar_icon_line(cmd, x + 8,  y + 14, x + 8,  y + 15, color);
    draw_toolbar_icon_line(cmd, x + 0,  y + 8,  x + 2,  y + 8,  color);
    draw_toolbar_icon_line(cmd, x + 14, y + 8,  x + 15, y + 8,  color);
    draw_toolbar_icon_line(cmd, x + 2,  y + 2,  x + 3,  y + 3,  color);
    draw_toolbar_icon_line(cmd, x + 13, y + 2,  x + 12, y + 3,  color);
    draw_toolbar_icon_line(cmd, x + 2,  y + 13, x + 3,  y + 12, color);
    draw_toolbar_icon_line(cmd, x + 13, y + 13, x + 12, y + 12, color);
}

/* point light: a small bulb with rays */
static void draw_toolbar_icon_point_light(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int bulb[] = { 6, 4,  10, 4,  12, 6,  12, 9,  10, 11,  6, 11,  4, 9,  4, 6 };
    draw_toolbar_icon_polygon(cmd, x, y, bulb, 8, color);
    draw_toolbar_icon_line(cmd, x + 6, y + 13, x + 10, y + 13, color);
    draw_toolbar_icon_line(cmd, x + 8, y + 0,  x + 8,  y + 2,  color);
    draw_toolbar_icon_line(cmd, x + 1, y + 7,  x + 3,  y + 7,  color);
    draw_toolbar_icon_line(cmd, x + 13, y + 7, x + 15, y + 7,  color);
}

/* spot light: a lamp head with a cone */
static void draw_toolbar_icon_spot_light(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int head[] = { 5, 1,  11, 1,  10, 5,  6, 5 };
    draw_toolbar_icon_polygon(cmd, x, y, head, 4, color);
    draw_toolbar_icon_line(cmd, x + 6,  y + 5, x + 1,  y + 14, color);
    draw_toolbar_icon_line(cmd, x + 10, y + 5, x + 15, y + 14, color);
    draw_toolbar_icon_line(cmd, x + 1,  y + 14, x + 15, y + 14, color);
}

/* area light: a panel with light coming off it */
static void draw_toolbar_icon_area_light(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int panel[] = { 2, 2,  14, 2,  14, 6,  2, 6 };
    draw_toolbar_icon_polygon(cmd, x, y, panel, 4, color);
    draw_toolbar_icon_line(cmd, x + 4,  y + 8, x + 3,  y + 14, color);
    draw_toolbar_icon_line(cmd, x + 8,  y + 8, x + 8,  y + 14, color);
    draw_toolbar_icon_line(cmd, x + 12, y + 8, x + 13, y + 14, color);
}

/* hemisphere light: a half dome over a ground line */
static void draw_toolbar_icon_hemisphere_light(VkCommandBuffer cmd, int x, int y, int size, system_color color)
{
    (void)size;
    static const int dome[] = { 1, 11,  2, 7,  4, 4,  8, 2,  12, 4,  14, 7,  15, 11 };
    draw_toolbar_icon_polygon(cmd, x, y, dome, 7, color);
    draw_toolbar_icon_line(cmd, x + 0, y + 11, x + 15, y + 11, color);
    draw_toolbar_icon_line(cmd, x + 3, y + 14, x + 12, y + 14, color);
}

/* picks the right icon function for a kind */
static void draw_toolbar_icon(VkCommandBuffer cmd, int icon_kind, int x, int y, int size, system_color color)
{
    switch (icon_kind) {
        case toolbar_icon_cube:     draw_toolbar_icon_cube(cmd, x, y, size, color);             break;
        case toolbar_icon_camera:   draw_toolbar_icon_camera(cmd, x, y, size, color);           break;
        case toolbar_icon_terrain:  draw_toolbar_icon_terrain(cmd, x, y, size, color);          break;
        case toolbar_icon_octagon:  draw_toolbar_icon_octagon(cmd, x, y, size, color);          break;
        case toolbar_icon_diamond:  draw_toolbar_icon_diamond(cmd, x, y, size, color);          break;
        case toolbar_icon_half:     draw_toolbar_icon_triangle_half(cmd, x, y, size, color);    break;
        case toolbar_icon_quarter:  draw_toolbar_icon_triangle_quarter(cmd, x, y, size, color); break;
        case toolbar_icon_sun:      draw_toolbar_icon_sun(cmd, x, y, size, color);              break;
        case toolbar_icon_point:    draw_toolbar_icon_point_light(cmd, x, y, size, color);      break;
        case toolbar_icon_spot:     draw_toolbar_icon_spot_light(cmd, x, y, size, color);       break;
        case toolbar_icon_area:     draw_toolbar_icon_area_light(cmd, x, y, size, color);       break;
        case toolbar_icon_hemisphere: draw_toolbar_icon_hemisphere_light(cmd, x, y, size, color); break;
        default: break;
    }
}

/* themed icon button with the icon drawn on top. returns the click result of the button. */
static int draw_toolbar_icon_button(VkCommandBuffer cmd, system_element_context* ui_ctx, int id, int icon_kind, int x, int y)
{
    int clicked = draw_button_element(cmd, ui_ctx, id, "", x, y, toolbar_icon_btn_w, toolbar_btn_h, system_theme.toolbar_icon_button);

    int icon_x = x + (toolbar_icon_btn_w - toolbar_icon_size) / 2;
    int icon_y = y + (toolbar_btn_h - toolbar_icon_size) / 2;
    draw_toolbar_icon(cmd, icon_kind, icon_x, icon_y, toolbar_icon_size, system_theme.button_text);

    return clicked;
}

void draw_system_toolbar(VkCommandBuffer cmd, system_element_context* ui_ctx, editor_window_swapchain* swapchain)
{
    int w = (int)swapchain->extent.width;

    /* rows 28..58 background, row 59 border, dock starts at row 60: no gap anywhere */
    draw_toolbar_element(cmd, ui_ctx, 0, toolbar_y, w, toolbar_h - 1, system_theme.toolbar_background);
    push_quad_element(cmd, 0.0f, (float)(dock_top - 1), (float)w, 1.0f, system_theme.system_border);

    int btn_y = toolbar_y + ((toolbar_h - 1) - toolbar_btn_h) / 2;   /* 33 */
    int run_x = (w / 2) - (toolbar_btn_w / 2);

    /* RUN / STOP */
    const char* run_label = is_game_running ? "STOP" : "RUN";
    system_color run_color = is_game_running ? system_theme.toolbar_stop_button : system_theme.toolbar_run_button;
    if (draw_button_element(cmd, ui_ctx, 90001, run_label, run_x, btn_y, toolbar_btn_w, toolbar_btn_h, run_color)) {
        is_game_running = !is_game_running;
        engine_context.is_paused = !is_game_running;
        set_toolbar_status(is_game_running ? "Simulation started." : "Simulation stopped.");
    }

    /* all non run buttons start at the far left, themed, with their icon drawn on top */
    int btn_step = toolbar_icon_btn_w + 6;
    int btn_x = 5;

    /* +CUBE: create_system_cube registers the scene object itself. Do NOT spawn a second one. */
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90002, toolbar_icon_cube, btn_x, btn_y)) {
        const float cube_size = 4.0f;
        editor_vec3 spawn_pos = { cube_size * 0.5f, cube_size * 0.5f, cube_size * 0.5f };   /* fills the 4m grid cell 0..4 */
        create_system_cube(spawn_pos, cube_size, "Cube",
                           render_context.logical_device, render_context.physical_device,
                           render_context.command_pool, render_context.graphics_queue);
        set_toolbar_status("Created: Cube Primitive (4m).");
    }
    btn_x += btn_step;

    /* +CAMERA: create_system_camera registers the scene object itself. Do NOT spawn a second one. */
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90003, toolbar_icon_camera, btn_x, btn_y)) {
        editor_vec3 cam_pos = { 0.0f, 5.0f, 12.0f };
        create_system_camera(cam_pos, 1.0f, "Editor Camera Object",
                             render_context.logical_device, render_context.physical_device,
                             render_context.command_pool, render_context.graphics_queue);
        set_toolbar_status("Created: Editor Camera Object.");
    }
    btn_x += btn_step;

    /* +TERRAIN: one terrain object. Pressing again rebuilds it with the Inspector settings
       (it does not add a second one). */
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90004, toolbar_icon_terrain, btn_x, btn_y)) {
        create_terrain_system();
        set_toolbar_status("Terrain created / rebuilt from the Inspector settings.");
    }
    btn_x += btn_step;

    /* Board pieces (4 mesh edges). Each one registers itself, stores its mesh in the object database and is editable.
       octagon = main tile, diamond = fills the gaps between octagons,
       half = border piece, quarter = corner piece. */
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90005, toolbar_icon_octagon, btn_x, btn_y)) {
        create_system_board_octagon(NULL);
        set_toolbar_status("Created: Board Octagon.");
    }
    btn_x += btn_step;
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90006, toolbar_icon_diamond, btn_x, btn_y)) {
        create_system_board_diamond(NULL);
        set_toolbar_status("Created: Board Diamond.");
    }
    btn_x += btn_step;
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90007, toolbar_icon_half, btn_x, btn_y)) {
        create_system_board_triangle_half(NULL);
        set_toolbar_status("Created: Board Triangle Half.");
    }
    btn_x += btn_step;
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90008, toolbar_icon_quarter, btn_x, btn_y)) {
        create_system_board_triangle_quarter(NULL);
        set_toolbar_status("Created: Board Triangle Quarter.");
    }
    btn_x += btn_step;

    /* +DIRECTIONAL LIGHT: create_system_directional_light registers the scene object itself. Do NOT spawn a second one.
       While it exists it drives the sun (see system_light.c). */
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90009, toolbar_icon_sun, btn_x, btn_y)) {
        editor_vec3 light_pos = { 0.0f, 12.0f, 0.0f };
        create_system_directional_light(light_pos, 1.0f, "Directional Light",
                                        render_context.logical_device, render_context.physical_device,
                                        render_context.command_pool, render_context.graphics_queue);
        set_toolbar_status("Created: Directional Light.");
    }
    btn_x += btn_step;

    /* the other lights, each with its own engine file (engine_point_light.c, engine_spot_light.c, engine_area_light.c, engine_hemisphere_light.c) */
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90010, toolbar_icon_point, btn_x, btn_y)) {
        create_system_light(light_kind_point, (editor_vec3){ 0.0f, 8.0f, 0.0f }, render_context.logical_device, render_context.physical_device);
        set_toolbar_status("Created: Point Light.");
    }
    btn_x += btn_step;
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90011, toolbar_icon_spot, btn_x, btn_y)) {
        create_system_light(light_kind_spot, (editor_vec3){ 0.0f, 12.0f, 0.0f }, render_context.logical_device, render_context.physical_device);
        set_toolbar_status("Created: Spot Light.");
    }
    btn_x += btn_step;
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90012, toolbar_icon_area, btn_x, btn_y)) {
        create_system_light(light_kind_area, (editor_vec3){ 0.0f, 10.0f, 0.0f }, render_context.logical_device, render_context.physical_device);
        set_toolbar_status("Created: Area Light.");
    }
    btn_x += btn_step;
    if (draw_toolbar_icon_button(cmd, ui_ctx, 90013, toolbar_icon_hemisphere, btn_x, btn_y)) {
        create_system_light(light_kind_hemisphere, (editor_vec3){ 0.0f, 16.0f, 0.0f }, render_context.logical_device, render_context.physical_device);
        set_toolbar_status("Created: Hemisphere Light.");
    }
}