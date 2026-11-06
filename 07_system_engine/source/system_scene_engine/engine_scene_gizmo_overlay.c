#include "engine_globals.h"


void draw_gimbal_thick_line(VkCommandBuffer cmd, float x0, float y0, float x1, float y1, float r, float g, float b) 
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float steps = fabsf(dy);
    if (fabsf(dx) > fabsf(dy)) steps = fabsf(dx);
    
    if (steps < 1.0f) {
        push_quad_element(cmd, x0 - 2.0f, y0 - 2.0f, 4.0f, 4.0f, (system_color){ .red = r, .green = g, .blue = b, .alpha = 1.0f });
        return;
    }
    
    float x_increment = dx / steps;
    float y_increment = dy / steps;
    float current_x = x0;
    float current_y = y0;
    
    for (int i = 0; i <= (int)steps; i++) {
        push_quad_element(cmd, current_x - 1.5f, current_y - 1.5f, 3.0f, 3.0f, (system_color){ .red = r, .green = g, .blue = b, .alpha = 1.0f });
        current_x += x_increment;
        current_y += y_increment;
    }
}

void draw_sceneview_gimbal_overlay(VkCommandBuffer cmd, system_element_context* ui_ctx, struct_viewport_bounds bounds, editor_vec3 object_origin)
{
    (void)object_origin; 
    
    camera_component* active_cam = &engine_context.world_state.editor_scene_camera;
    
    float center_x = (float)(bounds.offset_x) + 60.0f;
    float center_y = (float)(bounds.offset_y + bounds.height) - 60.0f;

    float rot_x = active_cam->rotation.x;
    float rot_y = active_cam->rotation.y;

    float rgt_x = cosf(rot_y);
    float rgt_y = 0.0f;
    float rgt_z = sinf(rot_y);

    float fwd_x = sinf(rot_y) * cosf(rot_x);
    float fwd_y = sinf(rot_x);
    float fwd_z = -cosf(rot_y) * cosf(rot_x);

    float up_x = (rgt_y * fwd_z) - (rgt_z * fwd_y);
    float up_y = (rgt_z * fwd_x) - (rgt_x * fwd_z);
    float up_z = (rgt_x * fwd_y) - (rgt_y * fwd_x);

    float line_len = 35.0f; 

    float sx_x = center_x + (rgt_x * 1.0f) * line_len;
    float sx_y = center_y - (up_x  * 1.0f) * line_len;

    float sy_x = center_x + (rgt_y * 1.0f) * line_len;
    float sy_y = center_y - (up_y  * 1.0f) * line_len;

    float sz_x = center_x + (rgt_z * 1.0f) * line_len;
    float sz_y = center_y - (up_z  * 1.0f) * line_len;

    draw_gimbal_thick_line(cmd, center_x, center_y, sx_x, sx_y, 1.0f, 0.1f, 0.1f);
    draw_gimbal_thick_line(cmd, center_x, center_y, sy_x, sy_y, 0.1f, 1.0f, 0.1f);
    draw_gimbal_thick_line(cmd, center_x, center_y, sz_x, sz_y, 0.1f, 0.4f, 1.0f);

    push_quad_element(cmd, center_x - 3.0f, center_y - 3.0f, 6.0f, 6.0f, (system_color){ .red = 0.9f, .green = 0.9f, .blue = 0.9f, .alpha = 1.0f });

    /* top-right tool box + Show Manipulator panel. Hosted here because this already runs once per frame inside the
       Scene view with cmd / ui_element / bounds. Move this call next to the gimbal call if you would rather keep them apart. */
    draw_scene_manipulator_overlay(cmd, ui_ctx, (int)bounds.offset_x, (int)bounds.offset_y, (int)bounds.width, (int)bounds.height);
}