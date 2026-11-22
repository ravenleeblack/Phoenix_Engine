#include "system_globals.h"


void draw_gimbal_thick_line(VkCommandBuffer cmd, float x0, float y0, float x1, float y1, float r, float g, float b) 
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float steps = (fabsf(dx) > fabsf(dy)) ? fabsf(dx) : fabsf(dy);
    
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
    
    world_camera* active_cam = &engine_context.world_state.editor_scene_camera;
    
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

    /* the orientation gizmo, like Unity's: a soft disc, three axis arms with a lettered ball on the + end (X red, Y green, Z blue)
       and a small dim ball on each - end. the arms nearest the viewer are drawn last, so they sit in front. */
    const float line_len = 34.0f;
    const float axis_world[3][3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
    const char* axis_letter[3] = { "X", "Y", "Z" };
    const system_color axis_rgb[3] = {
        { 0.86f, 0.25f, 0.20f, 1.0f }, { 0.56f, 0.88f, 0.28f, 1.0f }, { 0.24f, 0.50f, 0.96f, 1.0f }
    };

    push_circle_element(cmd, center_x, center_y, 50.0f, 0.0f, (system_color){ 0.10f, 0.10f, 0.12f, 0.35f });

    float arm_x[6], arm_y[6], arm_depth[6];
    int arm_axis[6];
    bool arm_positive[6];
    for (int i = 0; i < 6; i++) {
        const int axis = i % 3;
        const float sign = (i < 3) ? 1.0f : -1.0f;
        const float ax = axis_world[axis][0] * sign;
        const float ay = axis_world[axis][1] * sign;
        const float az = axis_world[axis][2] * sign;
        arm_x[i] = center_x + (rgt_x * ax + rgt_y * ay + rgt_z * az) * line_len;
        arm_y[i] = center_y - (up_x * ax + up_y * ay + up_z * az) * line_len;
        arm_depth[i] = fwd_x * ax + fwd_y * ay + fwd_z * az;       /* bigger = further from the viewer */
        arm_axis[i] = axis;
        arm_positive[i] = (i < 3);
    }

    int order[6] = { 0, 1, 2, 3, 4, 5 };
    for (int i = 0; i < 5; i++) {
        for (int k = i + 1; k < 6; k++) {
            if (arm_depth[order[k]] > arm_depth[order[i]]) {
                const int swap = order[i];
                order[i] = order[k];
                order[k] = swap;
            }
        }
    }

    for (int n = 0; n < 6; n++) {
        const int i = order[n];
        const system_color base = axis_rgb[arm_axis[i]];
        if (arm_positive[i]) {
            draw_gimbal_thick_line(cmd, center_x, center_y, arm_x[i], arm_y[i], base.red, base.green, base.blue);
            push_circle_element(cmd, arm_x[i], arm_y[i], 10.0f, 0.0f, base);
            const int letter_x = (int)(arm_x[i] + 0.5f) - ui_font_w / 2;
            const int letter_y = (int)(arm_y[i] + 0.5f) - ui_font_h / 2;
            draw_text_element(cmd, ui_ctx, axis_letter[arm_axis[i]], letter_x, letter_y, (system_color){ 0.05f, 0.05f, 0.05f, 1.0f });
        } else {
            push_circle_element(cmd, arm_x[i], arm_y[i], 6.0f, 0.0f, (system_color){ base.red * 0.55f, base.green * 0.55f, base.blue * 0.55f, 0.9f });
        }
    }
    /* top-right tool box + Show Manipulator panel. Hosted here because this already runs once per frame inside the
       Scene view with cmd / ui_element / bounds. Move this call next to the gimbal call if you would rather keep them apart. */
    draw_scene_manipulator_overlay(cmd, ui_ctx, (int)bounds.offset_x, (int)bounds.offset_y, (int)bounds.width, (int)bounds.height);
}