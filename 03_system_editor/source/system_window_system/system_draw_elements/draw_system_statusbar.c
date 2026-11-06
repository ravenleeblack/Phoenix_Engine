#include "system_globals.h"

/* prototypes for the two helpers below belong in the editor's statusbar draw header (or system_globals.h):
   const char* get_select_type_name(int select_type);
   const char* get_statusbar_tool_name(int tool_index);                                                    */

/* name of the edit mode component type, shown in the status text while editing */
const char* get_select_type_name(int select_type)
{
    if (select_type == select_vert) return "Vertex";
    if (select_type == select_edge) return "Edge";
    if (select_type == select_face) return "Face";
    return "None";
}

const char* get_statusbar_tool_name(int tool_index)
{
    if (tool_index == 1) return "Translate";
    if (tool_index == 2) return "Rotate";
    if (tool_index == 3) return "Scale";
    return "Select";
}

void draw_system_statusbar(VkCommandBuffer cmd, system_element_context* ui_ctx, editor_window_swapchain* swapchain, bool is_running)
{
    const int w = (int)swapchain->extent.width;
    const int status_y = (int)swapchain->extent.height - statusbar_h;

    system_color status_bg   = system_theme.statusbar_stop_background;
    system_color status_text = system_theme.statusbar_stop_text;
    if (is_running) {
        status_bg   = system_theme.statusbar_run_background;
        status_text = system_theme.statusbar_run_text;
    }

    draw_statusbar_element(cmd, ui_ctx, 0, status_y, w, statusbar_h, status_bg);

    world_camera* active_cam = &engine_context.world_state.editor_scene_camera;

    /* Keep the mode state fresh even if no other window ran this frame. */
    mode_state_sync_from_ui(ui_ctx);

    int tool_i = (int)ui_ctx->active_tool;
    if (tool_i < 0 || tool_i > 3) tool_i = 0;

    const system_scene_object* sel = find_scene_object_by_id(g_scene_registry.selected_object_id);

    const char* sel_name = "none";
    if (sel) sel_name = sel->name;

    /* First field is the real editor mode (Object / Edit / UV / Model / Sculpt / Paint / Animation).
       It only reads ACTIVE SIMULATION while the game is running. */
    const char* mode_label = mode_context_name(g_mode_state.context);
    if (is_running) mode_label = "ACTIVE SIMULATION";

    /* the mode menu (menubar dynamic zone) owns the component type: show it here while editing */
    char select_text[40] = "";
    if (!is_running && g_mode_state.context == mode_context_edit) {
        snprintf(select_text, sizeof(select_text), " | Select: %s", get_select_type_name((int)ui_ctx->active_select_type));
    }

    const char* no_component_text = "";
    if (ui_ctx->active_mode == mode_edit_mesh && tool_i != 0) no_component_text = " (no component editing yet)";

    /* every engine message (set_statusbar_message) shows up in the %s that reads g_statusbar_text */
    snprintf(ui_ctx->text_buffers.statusbar_text_buffer, sizeof(ui_ctx->text_buffers.statusbar_text_buffer),
             "[%s] | Tool: %s%s%s | Selected: %s | %s | Camera Position: [%.1f, %.1f, %.1f] | Grid Unit: 4.0m",
             mode_label,
             get_statusbar_tool_name(tool_i),
             no_component_text,
             select_text,
             sel_name,
             g_statusbar_text,
             active_cam->position.x, active_cam->position.y, active_cam->position.z);

    /* 1px top border, then text centred in the remaining rows */
    const int text_y = status_y + 1 + (statusbar_h - 1 - ui_font_h) / 2;
    draw_text_element(cmd, ui_ctx, ui_ctx->text_buffers.statusbar_text_buffer, 15, text_y, status_text);

    push_quad_element(cmd, 0.0f, (float)status_y, (float)w, 1.0f, system_theme.system_border);
}