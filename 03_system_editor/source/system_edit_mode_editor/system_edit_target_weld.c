#include "system_globals.h"
#include "system_edit_manager.h"


/* ========================================================================= */
/* TARGET WELD: press a vertex, drag onto another, release                   */
/* ========================================================================= */
static bool     s_weld_drag  = false;
static uint32_t s_weld_src   = EM_NONE;
static uint32_t s_weld_hover = EM_NONE;

/* src becomes dst. dst keeps its place and stays obj_vert_selected. */
static bool op_weld_pair(edit_mode_mesh* mesh, uint32_t src, uint32_t dst, const char** err)
{
    if (src == dst || src >= mesh->vert_count || dst >= mesh->vert_count) { *err = "pick two different vertices"; return false; }
    uint32_t* map = (uint32_t*)malloc(sizeof(uint32_t) * mesh->vert_count);
    if (!map) { *err = "out of memory"; return false; }
    for (uint32_t i = 0; i < mesh->vert_count; ++i) map[i] = i;
    map[src] = dst;
    const bool ok = edit_weld_with_map(mesh, map, dst);
    free(map);
    if (!ok) *err = "weld failed (it would delete the whole mesh)";
    return ok;
}

bool edit_target_weld_dragging(void) { return s_weld_drag; }

void edit_target_weld_cancel(system_element_context* ui_element)
{
    if (s_weld_drag && ui_element && ui_element->active_id == edit_ops_lock_id) ui_element->active_id = 0;
    s_weld_drag = false;
    s_weld_src = s_weld_hover = EM_NONE;
}

bool edit_target_weld_toggle(system_element_context* ui_element)
{
    if (g_mode_state.context != mode_context_edit) { edit_msg("Target Weld: switch to Edit mode first"); return false; }
    if (is_terrain_object(find_scene_object_by_id(g_scene_registry.selected_object_id))) { edit_msg("Target Weld: not on terrain (fixed height grid)"); return false; }
    toggle_manip_edit_tool(manip_edit_target_weld);
    edit_target_weld_cancel(ui_element);
    edit_multicut_cancel(ui_element);
    edit_insert_loop_cancel(ui_element);
    if (g_manip.edit_tool == manip_edit_target_weld) edit_msg("Target Weld: press a vertex, drag onto another, release");
    return true;
}

void update_edit_target_weld(const edit_frame* f)
{
    system_element_context* ui_element = f->ui_element;
    edit_mode_mesh* mesh = f->mesh;

    if (!s_weld_drag) {
        s_weld_hover = f->inside ? edit_pick_vert(mesh, f->mx, f->my, EM_NONE) : EM_NONE;
        if (f->press && f->inside && s_weld_hover != EM_NONE && ui_element->active_id == 0) {
            s_weld_drag = true; s_weld_src = s_weld_hover; ui_element->active_id = edit_ops_lock_id;
        }
        return;
    }

    s_weld_hover = edit_pick_vert(mesh, f->mx, f->my, s_weld_src);
    if (!f->down) {                                            /* release */
        const uint32_t src = s_weld_src, dst = s_weld_hover;
        edit_target_weld_cancel(ui_element);
        if (dst != EM_NONE) {
            const char* err = NULL;
            if (op_weld_pair(mesh, src, dst, &err) && em_commit_to_gpu(mesh, f->obj)) edit_msg("Target Weld done");
            else edit_msg("Target Weld: %s", err ? err : "failed");
        } else edit_msg("Target Weld: release on another vertex");
    }
}

void edit_target_weld_draw(VkCommandBuffer cmd, system_element_context* ui_element, const edit_mode_mesh* mesh)
{
    if (g_manip.edit_tool != manip_edit_target_weld) return;
    const cam_screen* S = edit_screen_points();

    if (s_weld_drag && s_weld_src < mesh->vert_count && S[s_weld_src].ok) {
        const cam_screen a = S[s_weld_src];
        const bool snap = (s_weld_hover != EM_NONE && S[s_weld_hover].ok);
        const float ex = snap ? S[s_weld_hover].x : (float)ui_element->input.mouse_x;
        const float ey = snap ? S[s_weld_hover].y : (float)ui_element->input.mouse_y;
        edit_line2d(cmd, a.x, a.y, ex, ey, 2.0f, system_theme.system_color_edit_mode_target_weld);
        float hlx, hly, hrx, hry;                              /* the gizmo is hidden for this tool: the arrow is the guide */
        system_math_compute_arrow_head(a.x, a.y, ex, ey, 14.0f, &hlx, &hly, &hrx, &hry);
        edit_line2d(cmd, ex, ey, hlx, hly, 2.0f, system_theme.system_color_edit_mode_target_weld);
        edit_line2d(cmd, ex, ey, hrx, hry, 2.0f, system_theme.system_color_edit_mode_target_weld);
        edit_box2d(cmd, a.x, a.y, 5.0f, system_theme.system_color_edit_mode_vert_active_color);   /* the vert being dragged = active */
        if (s_weld_hover != EM_NONE) edit_box2d(cmd, ex, ey, 6.0f, system_theme.system_color_edit_mode_hover_color);
    } else if (s_weld_hover != EM_NONE && S[s_weld_hover].ok) {
        edit_box2d(cmd, S[s_weld_hover].x, S[s_weld_hover].y, 5.0f, system_theme.system_color_edit_mode_hover_color);
    }
}