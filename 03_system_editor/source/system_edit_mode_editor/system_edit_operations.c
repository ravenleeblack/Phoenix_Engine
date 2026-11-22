#include "system_globals.h"



/* ========================================================================= */
/* DISPATCHER ONLY. Each tool lives in its own system_edit_<tool>.c          */
/* (see system_edit_internal.h for the list). This file:                     */
/*   - edit_ops_run()      maps a system_edit_tools value to a tool          */
/*   - edit_ops_update()   once per frame input, routed to the active tool   */
/*   - edit_ops_draw()     overlays + the bottom message                     */
/* ========================================================================= */

extern uint32_t g_ui_frame_index;                           /* system_element.c */

static bool     s_prev_down  = false;
static uint32_t s_frame_seen = 0xFFFFFFFFu;

static void cancel_all(system_element_context* ui_element)
{
    edit_target_weld_cancel(ui_element);
    edit_multicut_cancel(ui_element);
    edit_insert_loop_cancel(ui_element);
}

/* ------------------------------------------------------------------------- */
/* mouse / wheel ownership                                                   */
/* ------------------------------------------------------------------------- */
bool is_edit_ops_using_mouse(void)
{
    if (g_mode_state.context != mode_context_edit) return false;
    if (edit_target_weld_dragging() || edit_insert_loop_dragging()) return true;
    return (g_manip.edit_tool == manip_edit_target_weld || g_manip.edit_tool == manip_edit_insert_loop || g_manip.edit_tool == manip_edit_multicut) &&
           is_edit_in_scene_view(&element_context) && !is_manip_overlay_using_mouse();
}

bool consume_edit_ops_wheel(float notches)
{
    if (g_mode_state.context != mode_context_edit) return false;
    if (!is_edit_in_scene_view(&element_context)) return false;
    if (g_manip.edit_tool == manip_edit_multicut)    return consume_edit_multicut_wheel(notches);
    if (g_manip.edit_tool == manip_edit_insert_loop) return consume_edit_insert_loop_wheel(notches);
    return false;
}

/* tools that draw their own guide: the transform gizmo hides while they are on */
bool is_edit_ops_hiding_gizmo(void)
{
    if (g_mode_state.context != mode_context_edit) return false;
    return g_manip.edit_tool == manip_edit_target_weld || g_manip.edit_tool == manip_edit_multicut || g_manip.edit_tool == manip_edit_insert_loop;
}

/* ------------------------------------------------------------------------- */
/* per frame                                                                 */
/* ------------------------------------------------------------------------- */
void update_edit_ops(system_element_context* ui_element)
{
    if (!ui_element) return;
    if (s_frame_seen == g_ui_frame_index) return;           /* once per frame, however many Scene views draw */
    s_frame_seen = g_ui_frame_index;

    const bool down  = ui_element->input.mouse_down;
    const bool press = down && !s_prev_down;
    s_prev_down = down;

    system_scene_object* obj = NULL; edit_mode_mesh* mesh = NULL;
    const bool have = get_edit_target(&obj, &mesh);
    if (!have || g_manip.edit_tool == manip_edit_none) cancel_all(ui_element);
    if (!have) return;
    if (ui_element->input.camera_manipulating || ui_element->input.alt_pressed || is_context_menu_open()) { if (!down) cancel_all(ui_element); return; }

    edit_mode_camera_context cam;
    if (!begin_edit_camera(&cam, ui_element, obj) || !project_edit_points_all(&cam, mesh)) return;

    edit_frame f;
    f.ui_element = ui_element; f.obj = obj; f.mesh = mesh; f.cam = &cam;
    f.mx = (float)ui_element->input.mouse_x; f.my = (float)ui_element->input.mouse_y;
    f.inside = is_edit_in_scene_view(ui_element) && !is_manip_overlay_using_mouse();
    f.press = press; f.down = down;

    switch (g_manip.edit_tool) {
    case manip_edit_target_weld: update_edit_target_weld(&f);  break;
    case manip_edit_insert_loop: update_edit_insert_loop(&f);  break;
    case manip_edit_multicut:    update_edit_multicut(&f);     break;
    default:                     update_edit_row_select(&f);   break;      /* no tool: double click selects a row */
    }
}

/* the active vert / edge / face drawn on top of the normal selection drawing */
static void draw_active_elements(VkCommandBuffer cmd, const edit_mode_mesh* mesh)
{
    const cam_screen* S = edit_screen_points();
    const system_color fc = color_with_alpha(system_theme.system_color_edit_mode_vert_active_color, 1.0f);   /* outline stays solid, the fill alpha is for the shader */
    for (uint32_t i = 0; i < mesh->face_count; ++i) {
        const edit_mode_face* F = &mesh->faces[i];
        if (!F->obj_vert_active) continue;
        for (int k = 0; k < F->face_type; ++k) {
            const cam_screen a = S[F->vert_to_edge_array[k]], b = S[F->vert_to_edge_array[(k + 1) % F->face_type]];
            if (a.ok && b.ok) edit_line2d(cmd, a.x, a.y, b.x, b.y, 2.0f, fc);
        }
    }
    for (uint32_t i = 0; i < mesh->edge_count; ++i) {
        if (!mesh->edges[i].obj_vert_active) continue;
        const cam_screen a = S[mesh->edges[i].vert_to_edge_array[0]], b = S[mesh->edges[i].vert_to_edge_array[1]];
        if (a.ok && b.ok) edit_line2d(cmd, a.x, a.y, b.x, b.y, 3.0f, system_theme.system_color_edit_mode_vert_active_color);
    }
    for (uint32_t i = 0; i < mesh->vert_count; ++i)
    {
        if (mesh->verts[i].obj_vert_active && S[i].ok) 
        {
            edit_box2d(cmd, S[i].x, S[i].y, 5.0f, system_theme.system_color_edit_mode_vert_active_color);
        }
    }
}

void draw_edit_ops(VkCommandBuffer cmd, system_element_context* ui_element)
{
    if (!ui_element) return;

    system_scene_object* obj = NULL; edit_mode_mesh* mesh = NULL;
    if (get_edit_target(&obj, &mesh)) {
        edit_mode_camera_context cam;
        if (begin_edit_camera(&cam, ui_element, obj) && project_edit_points_all(&cam, mesh)) {
            draw_active_elements(cmd, mesh);
            edit_target_weld_draw(cmd, ui_element, mesh);
            edit_insert_loop_draw(cmd, &cam, mesh);
            edit_multicut_draw(cmd, ui_element, &cam, mesh);
        }
    }
    edit_msg_draw(cmd, ui_element);
}

/* ------------------------------------------------------------------------- */
/* run a tool (Modeler window buttons + right click menu)                    */
/* ------------------------------------------------------------------------- */
typedef bool (*op_fn)(edit_mode_mesh*, const char**);

static bool run_instant(const char* name, op_fn fn)
{
    system_scene_object* obj = NULL; edit_mode_mesh* mesh = NULL;
    if (!get_edit_target(&obj, &mesh)) { edit_msg("%s: switch to Edit mode with an object selected", name); return false; }
    if (is_terrain_object(obj)) {
        if (fn != edit_op_extrude) {
            edit_msg("%s: the terrain is one height per column, so only Extrude (pull the selection up) works on it. Move vertices with the gizmo instead", name);
            return false;
        }
        fn = edit_op_terrain_extrude;                      /* extrude on the terrain pulls the selected ground up */
    }
    const char* err = NULL;
    if (!fn(mesh, &err))                { edit_msg("%s: %s", name, err ? err : "failed"); return false; }
    if (!em_commit_to_gpu(mesh, obj))   { edit_msg("%s: GPU upload failed", name); return false; }
    edit_msg("%s done", name);
    return true;
}

bool edit_ops_tool_is_wired(system_edit_tools tool)
{
    switch (tool) {
    case edit_extrude: case edit_bridge: case edit_target_weld: case edit_insert_edge_loop:
    case edit_multicut: case edit_merge_vertices: case edit_collapse: case edit_delete_component: case edit_flip_normals:
        return true;
    default: return false;
    }
}

bool run_edit_ops(system_element_context* ui_element, system_edit_tools tool)
{
    switch (tool) {
    case edit_extrude:           return run_instant("Extrude",      edit_op_extrude);
    case edit_bridge:            return run_instant("Bridge",       edit_op_bridge);
    case edit_merge_vertices:
    case edit_collapse:          return run_instant("Merge",        edit_op_merge_selected);
    case edit_delete_component:  return run_instant("Delete",       edit_op_delete_faces);
    case edit_flip_normals:      return run_instant("Flip Normals", edit_op_flip_normals);

    case edit_target_weld:       return edit_target_weld_toggle(ui_element);
    case edit_insert_edge_loop:  return edit_insert_loop_toggle(ui_element);
    case edit_multicut:          return edit_multicut_toggle(ui_element);

    default:
        edit_msg("That modeling tool is not wired up yet");
        return false;
    }
}