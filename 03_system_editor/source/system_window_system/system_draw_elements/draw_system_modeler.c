#include "system_globals.h"
#include <stdarg.h>
#include <stdio.h>

/* status text goes to the statusbar (draw_system_statusbar.c reads status_bar_text_buffer). */
static void set_modeler_status(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), format, args);
    va_end(args);
}

/* ------------------------------------------------------------------------- */
/* The Modeler window shows tools for whatever the mode menu last obj_vert_selected:   */
/*   Obj -> object tools, Edt -> edit tools, UV / Mdl / Scp / Pnt / Anm.      */
/* Bright buttons work. Dim buttons are listed but not implemented yet; they  */
/* print a note in the status bar when clicked.                               */
/* ------------------------------------------------------------------------- */

enum { scene_root_id = 1000 };

static bool s_pressed = false;     /* rising edge_to_face_array of left mouse this frame */

typedef struct { int x, y, start_x, max_x, row_h; } flow_layout;

/* Dock-safe button: tests raw mouse state (the dock consumes input before content windows run).
   base is a theme color. hover / press use the shared theme steps, so the face never gets brighter than the theme allows. */
static bool ui_button_text(VkCommandBuffer cmd, system_element_context* ui_element, const char* label,
                           int x, int y, int w, int h, system_color base, system_color text_color)
{
    int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    bool hover = (mx >= x && mx < x + w && my >= y && my < y + h);
    system_color face = base;
    if (hover) face = process_theme_color(base, ui_element->input.mouse_down ? system_theme.button_press_delta : system_theme.button_hover_delta);
    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, face);

    int tx = x + (w - (int)strlen(label) * 8) / 2;
    if (tx < x + 2) tx = x + 2;
    draw_text_element(cmd, ui_element, label, tx, y + (h - 8) / 2, text_color);
    return hover && s_pressed;
}

static bool ui_button(VkCommandBuffer cmd, system_element_context* ui_element, const char* label,
                      int x, int y, int w, int h, system_color base)
{
    return ui_button_text(cmd, ui_element, label, x, y, w, h, base, system_theme.button_text);
}

static void flow_newline(flow_layout* f)
{
    if (f->x != f->start_x) { f->x = f->start_x; f->y += f->row_h + 4; }
}

static bool flow_button_text(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f,
                             const char* label, system_color color, system_color text_color)
{
    int w = (int)strlen(label) * 8 + 16;
    if (f->x + w > f->max_x && f->x != f->start_x) { f->x = f->start_x; f->y += f->row_h + 4; }
    bool clicked = ui_button_text(cmd, ui_element, label, f->x, f->y, w, f->row_h, color, text_color);
    f->x += w + 4;
    return clicked;
}

static bool flow_button(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f,
                        const char* label, system_color color)
{
    return flow_button_text(cmd, ui_element, f, label, color, system_theme.button_text);
}

static void flow_text(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f,
                      const char* text, system_color color)
{
    flow_newline(f);
    f->y += 6;
    draw_text_element(cmd, ui_element, text, f->x, f->y, color);
    f->y += 18;
}

/* A toggle: accent when on. status_name is what the statusbar calls it (the shown label can carry the state, like "Snap: ON"). */
static bool flow_toggle_named(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f,
                              const char* label, const char* status_name, bool* value)
{
    if (flow_button(cmd, ui_element, f, label, *value ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) {
        *value = !*value;
        set_modeler_status("%s: %s.", status_name, *value ? "on" : "off");
        return true;
    }
    return false;
}

static bool flow_toggle(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f,
                        const char* label, bool* value)
{
    return flow_toggle_named(cmd, ui_element, f, label, label, value);
}

/* A tool that exists in the layout but is not wired to anything yet. */
static void flow_todo(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f, const char* label)
{
    if (flow_button_text(cmd, ui_element, f, label, system_theme.tab_item, get_theme_hint_color())) {
        set_modeler_status("%s: not implemented yet.", label);
    }
}

/* A modeling tool that edit_ops_run really does. Interactive ones (Target Weld, Insert Loop) light up while on. */
static void flow_edit_op(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f,
                         const char* label, system_edit_tools tool)
{
    const bool on = (tool == edit_target_weld      && g_manip.edit_tool == manip_edit_target_weld) ||
                    (tool == edit_insert_edge_loop && g_manip.edit_tool == manip_edit_insert_loop);
    if (flow_button(cmd, ui_element, f, label, on ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) {
        run_edit_ops(ui_element, tool);               /* the reason for a failure is shown at the bottom of the Scene view */
        set_modeler_status("%s.", label);
    }
}

#define clampf draw_modeler_clampf
static float draw_modeler_clampf(float vert_to_edge_array, float lo, float hi) { return vert_to_edge_array < lo ? lo : (vert_to_edge_array > hi ? hi : vert_to_edge_array); }

/* label  [-] value [+] */
static void flow_stepper_f(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f,
                           const char* label, float* vert_to_edge_array, float step, float lo, float hi)
{
    char val[32];
    flow_newline(f);
    draw_text_element(cmd, ui_element, label, f->x, f->y + 6, system_theme.system_text);
    if (ui_button(cmd, ui_element, "-", f->x + 130, f->y, 24, f->row_h, system_theme.tab_sub_menubar_button)) *vert_to_edge_array = clampf(*vert_to_edge_array - step, lo, hi);
    snprintf(val, sizeof(val), "%.4f", *vert_to_edge_array);
    draw_text_element(cmd, ui_element, val, f->x + 160, f->y + 6, system_theme.system_active_text);
    if (ui_button(cmd, ui_element, "+", f->x + 236, f->y, 24, f->row_h, system_theme.tab_sub_menubar_button)) *vert_to_edge_array = clampf(*vert_to_edge_array + step, lo, hi);
    f->y += f->row_h + 4;
}

static void flow_stepper_i(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f,
                           const char* label, int* vert_to_edge_array, int step, int lo, int hi)
{
    float tmp = (float)*vert_to_edge_array;
    char val[32];
    flow_newline(f);
    draw_text_element(cmd, ui_element, label, f->x, f->y + 6, system_theme.system_text);
    if (ui_button(cmd, ui_element, "-", f->x + 130, f->y, 24, f->row_h, system_theme.tab_sub_menubar_button)) tmp = clampf(tmp - (float)step, (float)lo, (float)hi);
    snprintf(val, sizeof(val), "%d", *vert_to_edge_array);
    draw_text_element(cmd, ui_element, val, f->x + 160, f->y + 6, system_theme.system_active_text);
    if (ui_button(cmd, ui_element, "+", f->x + 236, f->y, 24, f->row_h, system_theme.tab_sub_menubar_button)) tmp = clampf(tmp + (float)step, (float)lo, (float)hi);
    *vert_to_edge_array = (int)tmp;
    f->y += f->row_h + 4;
}

/* ---------------------------------------------------------------- OBJECT */

static void reset_selected(system_scene_object* o, int what)   /* 0 obj_vert_pos, 1 rot, 2 scale */
{
    if (what == 0) {
        o->position = (editor_vec3){ 0, 0, 0 };
        /* only the terrain object owns an object_data_terrain block in the object database */
        if (object_db_get(o->id, object_data_terrain, NULL)) {
            terrain_location_q zero = { 0, 0, 0 };
            set_terrain_location(zero);
        }
    } else if (what == 1) {
        o->rotation = (editor_vec3){ 0, 0, 0 };
    } else {
        o->scale = (editor_vec3){ 1, 1, 1 };
    }
}

static void tools_object(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f)
{
    object_mode_state* os = &g_mode_state.object;

    flow_text(cmd, ui_element, f, "Transform tool", system_theme.system_text);
    if (flow_button(cmd, ui_element, f, "Select",    ui_element->active_tool == tool_select    ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) { mode_state_set_tool(ui_element, tool_select); set_modeler_status("Tool: Select."); }
    if (flow_button(cmd, ui_element, f, "Translate", ui_element->active_tool == tool_translate ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) { mode_state_set_tool(ui_element, tool_translate); set_modeler_status("Tool: Translate."); }
    if (flow_button(cmd, ui_element, f, "Rotate",    ui_element->active_tool == tool_rotate    ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) { mode_state_set_tool(ui_element, tool_rotate); set_modeler_status("Tool: Rotate."); }
    if (flow_button(cmd, ui_element, f, "Scale",     ui_element->active_tool == tool_scale     ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) { mode_state_set_tool(ui_element, tool_scale); set_modeler_status("Tool: Scale."); }

    flow_text(cmd, ui_element, f, "Snapping", system_theme.system_text);
    flow_toggle_named(cmd, ui_element, f, os->snap_enabled ? "Snap: ON" : "Snap: OFF", "Snap", &os->snap_enabled);
    flow_stepper_f(cmd, ui_element, f, "Move (mesh)",   &os->snap_translate_m, 0.25f, 0.0625f, 100.0f);
    flow_stepper_f(cmd, ui_element, f, "Rotate (deg)", &os->snap_rotate_deg, 5.0f, 1.0f, 90.0f);
    flow_stepper_f(cmd, ui_element, f, "Scale",      &os->snap_scale, 0.05f, 0.01f, 1.0f);

    system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    flow_text(cmd, ui_element, f, (o && o->id != scene_root_id) ? o->name : "Select an object to use these", system_theme.system_active_text);
    if (o && o->id != scene_root_id) {
        if (flow_button(cmd, ui_element, f, "Reset Pos",   system_theme.tab_sub_menubar_button)) { reset_selected(o, 0); set_modeler_status("Reset position: %s.", o->name); }
        if (flow_button(cmd, ui_element, f, "Reset Rot",   system_theme.tab_sub_menubar_button)) { reset_selected(o, 1); set_modeler_status("Reset rotation: %s.", o->name); }
        if (flow_button(cmd, ui_element, f, "Reset Scale", system_theme.tab_sub_menubar_button)) { reset_selected(o, 2); set_modeler_status("Reset scale: %s.", o->name); }
        if (flow_button(cmd, ui_element, f, "Frame",       system_theme.tab_sub_menubar_button)) { focus_system_grid_on_selected_object(); set_modeler_status("Framed: %s.", o->name); }
    }
}

/* ---------------------------------------------------------------- TERRAIN */

/* Edit mode with the terrain selected. Select = the normal Vertex / Edge / Face editing of the control grid (the gizmo moves Y only).
   The other tools are brushes like Unity and Unreal: drag on the ground in the Scene view, only Y changes. */
static void tools_terrain(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f)
{
    flow_text(cmd, ui_element, f, "Terrain: Y tools", system_theme.system_text);
    for (int tool = 0; tool < terrain_tool_count; tool++) {
        const bool on = (g_terrain_tool.tool == tool);
        if (flow_button(cmd, ui_element, f, get_terrain_tool_name(tool), on ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) {
            g_terrain_tool.tool = tool;
            set_modeler_status("Terrain tool: %s.", get_terrain_tool_name(tool));
        }
    }

    if (g_terrain_tool.tool != terrain_tool_off) {
        flow_text(cmd, ui_element, f, "Brush", system_theme.system_text);
        flow_stepper_f(cmd, ui_element, f, "Size (cubes)", &g_terrain_tool.radius_cells, 1.0f, 1.0f, 60.0f);
        flow_stepper_f(cmd, ui_element, f, "Strength", &g_terrain_tool.strength, 0.05f, 0.05f, 1.0f);
        flow_stepper_f(cmd, ui_element, f, "Hardness", &g_terrain_tool.hardness, 0.05f, 0.0f, 0.95f);
        if (g_terrain_tool.tool == terrain_tool_set_height) {
            flow_stepper_f(cmd, ui_element, f, "Height (cubes)", &g_terrain_tool.height_cells, 1.0f, 0.0f, 400.0f);
        }
        flow_text(cmd, ui_element, f, "Drag on the ground. Ctrl does the opposite (raise <-> lower).", get_theme_hint_color());
    } else {
        flow_text(cmd, ui_element, f, "Pick vertices, edges or faces below and move them with the Y arrow.", get_theme_hint_color());
    }
}

/* ---------------------------------------------------------------- EDIT */

static void tools_edit(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f)
{
    const system_scene_object* selected_terrain = find_scene_object_by_id(g_scene_registry.selected_object_id);
    const bool terrain_selected = (selected_terrain != NULL && is_terrain_object(selected_terrain));
    if (terrain_selected) {
        tools_terrain(cmd, ui_element, f);
    }

    edit_mode_state* es = &g_mode_state.edit;

    flow_text(cmd, ui_element, f, "Component", system_theme.system_text);
    if (flow_button(cmd, ui_element, f, "Vertex", ui_element->active_select_type == select_vert ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) { ui_element->active_select_type = select_vert; es->select_type = select_vert; set_modeler_status("Select type: Vertex."); }
    if (flow_button(cmd, ui_element, f, "Edge",   ui_element->active_select_type == select_edge ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) { ui_element->active_select_type = select_edge; es->select_type = select_edge; set_modeler_status("Select type: Edge."); }
    if (flow_button(cmd, ui_element, f, "Face",   ui_element->active_select_type == select_face ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button)) { ui_element->active_select_type = select_face; es->select_type = select_face; set_modeler_status("Select type: Face."); }

    flow_text(cmd, ui_element, f, "Selection", system_theme.system_text);
    flow_toggle(cmd, ui_element, f, "Soft Select", &es->soft_select);
    flow_toggle(cmd, ui_element, f, "X-Ray", &es->xray);
    flow_stepper_f(cmd, ui_element, f, "Soft radius", &es->soft_select_radius, 0.25f, 0.05f, 100.0f);

    flow_text(cmd, ui_element, f, "Mesh operators", system_theme.system_text);
    {
        float dist = get_edit_ops_extrude_distance();
        flow_stepper_f(cmd, ui_element, f, "Extrude dist", &dist, 0.25f, 0.0f, 100.0f);
        set_edit_ops_extrude_distance(dist);
    }
    flow_edit_op(cmd, ui_element, f, "Extrude",      edit_extrude);
    flow_edit_op(cmd, ui_element, f, "Bridge",       edit_bridge);
    flow_edit_op(cmd, ui_element, f, "Merge",        edit_merge_vertices);
    flow_edit_op(cmd, ui_element, f, "Delete",       edit_delete_component);
    flow_edit_op(cmd, ui_element, f, "Flip Normals", edit_flip_normals);
    flow_edit_op(cmd, ui_element, f, "Target Weld",  edit_target_weld);
    flow_edit_op(cmd, ui_element, f, "Insert Loop",  edit_insert_edge_loop);
    flow_todo(cmd, ui_element, f, "Bevel");
    flow_todo(cmd, ui_element, f, "Cut");
    flow_todo(cmd, ui_element, f, "Split");
}

/* ---------------------------------------------------------------- UV */

static void tools_uv(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f)
{
    uv_mode_state* us = &g_mode_state.uv;
    char label[32];

    flow_text(cmd, ui_element, f, "Display", system_theme.system_text);
    flow_toggle(cmd, ui_element, f, "Checker", &us->show_checker);
    flow_toggle(cmd, ui_element, f, "Pixel Snap", &us->pixel_snap);
    snprintf(label, sizeof(label), "Texture: %d", us->texture_size);
    if (flow_button(cmd, ui_element, f, label, system_theme.tab_sub_menubar_button)) {
        us->texture_size = (us->texture_size >= 4096) ? 256 : us->texture_size * 2;
        set_modeler_status("UV texture size: %d.", us->texture_size);
    }
    flow_stepper_i(cmd, ui_element, f, "UV set", &us->active_uv_set, 1, 0, 7);

    flow_text(cmd, ui_element, f, "Projection", system_theme.system_text);
    flow_todo(cmd, ui_element, f, "Unwrap");
    flow_todo(cmd, ui_element, f, "Planar");
    flow_todo(cmd, ui_element, f, "Cylindrical");
    flow_todo(cmd, ui_element, f, "Spherical");
    flow_todo(cmd, ui_element, f, "Layout");
    flow_todo(cmd, ui_element, f, "Pack");
}

/* ---------------------------------------------------------------- MODEL */

static void tools_model(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f)
{
    model_mode_state* ms = &g_mode_state.model;

    flow_text(cmd, ui_element, f, "Operator settings", system_theme.system_text);
    flow_stepper_f(cmd, ui_element, f, "Extrude dist", &ms->extrude_distance, 0.25f, 0.0f, 100.0f);
    flow_stepper_f(cmd, ui_element, f, "Bevel width",  &ms->bevel_width, 0.05f, 0.0f, 10.0f);
    flow_stepper_i(cmd, ui_element, f, "Bevel segs",   &ms->bevel_segments, 1, 1, 16);
    flow_stepper_f(cmd, ui_element, f, "Merge dist",   &ms->merge_threshold, 0.001f, 0.0f, 1.0f);

    flow_text(cmd, ui_element, f, "Modeling tools", system_theme.system_text);
    flow_todo(cmd, ui_element, f, "Extrude");
    flow_todo(cmd, ui_element, f, "Bevel");
    flow_todo(cmd, ui_element, f, "Merge");
    flow_todo(cmd, ui_element, f, "Bridge");
    flow_todo(cmd, ui_element, f, "Cut");
    flow_todo(cmd, ui_element, f, "Mirror");
}

/* ---------------------------------------------------------------- SCULPT */

static void tools_sculpt(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f)
{
    sculpt_mode_state* ss = &g_mode_state.sculpt;

    flow_text(cmd, ui_element, f, "Brush", system_theme.system_text);
    flow_stepper_f(cmd, ui_element, f, "Radius",   &ss->brush_radius,   0.25f, 0.05f, 100.0f);
    flow_stepper_f(cmd, ui_element, f, "Strength", &ss->brush_strength, 0.05f, 0.0f, 1.0f);
    flow_stepper_f(cmd, ui_element, f, "Falloff",  &ss->brush_falloff,  0.05f, 0.0f, 1.0f);
    flow_toggle(cmd, ui_element, f, "Symmetry X", &ss->symmetry_x);
    flow_toggle(cmd, ui_element, f, "Invert", &ss->invert);

    flow_text(cmd, ui_element, f, "Brushes", system_theme.system_text);
    flow_todo(cmd, ui_element, f, "Grab");
    flow_todo(cmd, ui_element, f, "Smooth");
    flow_todo(cmd, ui_element, f, "Inflate");
    flow_todo(cmd, ui_element, f, "Pinch");
    flow_todo(cmd, ui_element, f, "Flatten");
}

/* ---------------------------------------------------------------- PAINT */

static void tools_paint(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f)
{
    paint_mode_state* ps = &g_mode_state.paint;
    float size = ps->brush_size;

    flow_text(cmd, ui_element, f, "Brush", system_theme.system_text);
    flow_stepper_f(cmd, ui_element, f, "Size",     &size,         1.0f, 1.0f, 256.0f);
    ps->brush_size = size;
    flow_stepper_f(cmd, ui_element, f, "Opacity",  &ps->opacity,  0.05f, 0.0f, 1.0f);
    flow_stepper_f(cmd, ui_element, f, "Hardness", &ps->hardness, 0.05f, 0.0f, 1.0f);

    flow_text(cmd, ui_element, f, "Color", system_theme.system_text);
    const system_color swatches[8] = {
        system_theme.system_color_edit_mode_face_active_color,
        system_theme.system_color_edit_mode_target_weld,
        system_theme.system_color_edit_mode_preview_color,
        system_theme.system_color_edit_mode_normal_color,
        system_theme.system_color_edit_mode_normal_color,
        system_theme.system_edit_mode_active_face_hover,
        system_theme.system_color_edit_mode_face_selection_color,
        system_theme.system_color_edit_mode_hover_color
    };
    for (int i = 0; i < 8; i++) {
        if (flow_button(cmd, ui_element, f, "  ", swatches[i])) {
            ps->color[0] = swatches[i].red; ps->color[1] = swatches[i].green;
            ps->color[2] = swatches[i].blue; ps->color[3] = 1.0f;
            set_modeler_status("Paint color set.");
        }
    }
    flow_newline(f);
    draw_text_element(cmd, ui_element, "Current", f->x, f->y + 6, system_theme.system_text);
    push_quad_element(cmd, (float)(f->x + 70), (float)f->y, 40.0f, (float)f->row_h,
                      (system_color){ .red = ps->color[0], .green = ps->color[1], .blue = ps->color[2], .alpha = 1.0f });
    f->y += f->row_h + 8;

    flow_text(cmd, ui_element, f, "Paint tools", system_theme.system_text);
    flow_todo(cmd, ui_element, f, "Brush");
    flow_todo(cmd, ui_element, f, "Eraser");
    flow_todo(cmd, ui_element, f, "Fill");
    flow_todo(cmd, ui_element, f, "Smear");
    flow_todo(cmd, ui_element, f, "Clone");
}

/* ---------------------------------------------------------------- ANIMATION */

static void tools_animation(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f)
{
    flow_text(cmd, ui_element, f, "Keyframes", system_theme.system_text);
    flow_todo(cmd, ui_element, f, "Set Key");
    flow_todo(cmd, ui_element, f, "Delete Key");
    flow_todo(cmd, ui_element, f, "Play");
    flow_todo(cmd, ui_element, f, "Stop");
}

/* ---------------------------------------------------------------- Terrain Tools: the steps of the picked tool */
static void flow_choice(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f, const char* label, int* value, int this_value)
{
    system_color color = system_theme.tab_sub_menubar_button;
    if (*value == this_value) {
        color = system_theme.tab_sub_menubar_button_active;
    }
    if (flow_button(cmd, ui_element, f, label, color)) {
        *value = this_value;
    }
}

/* the body of one step (only the open step shows its body; every step has its header, in order, top to bottom) */
static void draw_terrain_step_body(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f, terrain_tools_state* s, int step)
{
    const bool radial = is_terrain_tools_radial(s->tool);
    const char* width_label = "Half width (m)";
    const char* depth_label = "Depth (m)";
    if (radial) { width_label = "Radius (m)"; }
    if (s->tool == tt_tool_mountain) { depth_label = "Height (m)"; }
    if (s->tool == tt_tool_ramp) { depth_label = "Rise (m)"; }
    if (s->tool == tt_tool_tunnel) { width_label = "Radius (m)"; depth_label = "Under the ground (m)"; }
    if (s->tool == tt_tool_overhang) { depth_label = "Height over ground (m)"; }

    switch (step) {
    case 1:
        flow_text(cmd, ui_element, f, "Edit mode, edge select is on.", system_theme.system_text);
        if (radial) {
            flow_text(cmd, ui_element, f, "Click an edge of the grid for the middle.", system_theme.system_text);
        } else if (s->picks == 0) {
            flow_text(cmd, ui_element, f, "Click an edge for the START.", system_theme.system_text);
        } else if (s->picks == 1) {
            flow_text(cmd, ui_element, f, "Start set. Pan to the END, click its edge.", system_theme.system_text);
        } else {
            flow_text(cmd, ui_element, f, "Both edges picked.", system_theme.system_text);
        }
        flow_text(cmd, ui_element, f, "Pick the region in the Scene Archive, Ctrl+F centers it.", get_theme_hint_color());
        flow_text(cmd, ui_element, f, "Click anywhere on the ground, any chunk. Blue = regions,", get_theme_hint_color());
        flow_text(cmd, ui_element, f, "light yellow = chunks. The cyan preview follows the mouse.", get_theme_hint_color());
        flow_newline(f);
        if (flow_button(cmd, ui_element, f, "Follow camera", system_theme.tab_sub_menubar_button)) {
            g_terrain.edit_follow = true;
        }
        if (flow_button(cmd, ui_element, f, "Chunk -X", system_theme.tab_sub_menubar_button)) {
            set_terrain_edit_chunk(g_terrain.edit_cx - 1, g_terrain.edit_cz);
        }
        if (flow_button(cmd, ui_element, f, "Chunk +X", system_theme.tab_sub_menubar_button)) {
            set_terrain_edit_chunk(g_terrain.edit_cx + 1, g_terrain.edit_cz);
        }
        if (flow_button(cmd, ui_element, f, "Chunk -Z", system_theme.tab_sub_menubar_button)) {
            set_terrain_edit_chunk(g_terrain.edit_cx, g_terrain.edit_cz - 1);
        }
        if (flow_button(cmd, ui_element, f, "Chunk +Z", system_theme.tab_sub_menubar_button)) {
            set_terrain_edit_chunk(g_terrain.edit_cx, g_terrain.edit_cz + 1);
        }
        flow_newline(f);
        if (flow_button(cmd, ui_element, f, "Reset picks", system_theme.tab_sub_menubar_button)) {
            edit_mode_mesh* mesh = terrain_edit_mesh_current();
            if (mesh) {
                em_clear_selection(mesh);
            }
            reset_terrain_tool_picks();
        }
        break;

    case 2: {
        const int old_segments = s->segments;
        const float old_bend = s->bend;
        const int old_curve = s->curve;
        const char* segment_label = "Segments";
        if (radial) {
            segment_label = "Outline parts";
        }
        flow_stepper_i(cmd, ui_element, f, segment_label, &s->segments, 1, 2, tt_max_segments);
        if (!radial) {
            flow_text(cmd, ui_element, f, "Curve", system_theme.system_text);
            flow_choice(cmd, ui_element, f, "S shape", &s->curve, tt_curve_s);
            flow_choice(cmd, ui_element, f, "Arc", &s->curve, tt_curve_arc);
            flow_newline(f);
            flow_stepper_f(cmd, ui_element, f, "Curve bend", &s->bend, 0.05f, -1.0f, 1.0f);
        }
        if (s->segments != old_segments || s->bend != old_bend || s->curve != old_curve) {
            if (!radial) {
                if (s->nodes_edited) {
                    set_modeler_status("Segments / bend changed: the moved vertices were reset.");
                }
                build_terrain_tool_nodes();
            }
        }
        flow_text(cmd, ui_element, f, "Click a square to select it (Shift adds).", get_theme_hint_color());
        flow_text(cmd, ui_element, f, "Move them with the translate gizmo.", get_theme_hint_color());
        if (flow_button(cmd, ui_element, f, "Select all vertices", system_theme.tab_sub_menubar_button)) {
            select_all_terrain_tool_nodes();
        }
        flow_newline(f);
        flow_text(cmd, ui_element, f, "Side pattern (the edge of the cut)", system_theme.system_text);
        flow_choice(cmd, ui_element, f, "Smooth", &s->pattern, tt_pattern_smooth);
        flow_choice(cmd, ui_element, f, "Steps", &s->pattern, tt_pattern_steps);
        flow_choice(cmd, ui_element, f, "Zigzag", &s->pattern, tt_pattern_zigzag);
        flow_newline(f);
        flow_stepper_f(cmd, ui_element, f, "Side offset", &s->pattern_offset, 0.05f, 0.0f, 0.8f);
        break;
    }

    case 3:
        flow_stepper_f(cmd, ui_element, f, width_label, &s->width, 1.0f, 1.0f, 200.0f);
        flow_stepper_f(cmd, ui_element, f, depth_label, &s->depth, 1.0f, 0.0f, 200.0f);
        if (s->tool != tt_tool_tunnel) {
            flow_stepper_f(cmd, ui_element, f, "Bank slope (m)", &s->bank, 1.0f, 0.0f, 100.0f);
        }
        if (s->tool == tt_tool_river || s->tool == tt_tool_canyon || s->tool == tt_tool_lake || s->tool == tt_tool_mountain) {
            flow_text(cmd, ui_element, f, "Walls (this is a 3D height map)", system_theme.system_text);
            flow_choice(cmd, ui_element, f, "Straight up and down", &s->walls, tt_walls_vertical);
            flow_choice(cmd, ui_element, f, "Sloped", &s->walls, tt_walls_sloped);
            flow_newline(f);
        }
        if (s->tool != tt_tool_ramp && s->tool != tt_tool_tunnel) {
            flow_text(cmd, ui_element, f, "Bottom shape", system_theme.system_text);
            for (int i = 0; i < tt_shape_count; i++) {
                flow_choice(cmd, ui_element, f, get_terrain_tools_shape_name(i), &s->shape, i);
            }
            flow_newline(f);
        }
        break;

    case 4:
        if (s->tool == tt_tool_tunnel) {
            flow_text(cmd, ui_element, f, "A tunnel has no ridges.", system_theme.system_text);
            break;
        }
        flow_text(cmd, ui_element, f, "The rim along the top of the cut.", system_theme.system_text);
        flow_stepper_f(cmd, ui_element, f, "Ridge height (m)", &s->ridge_height, 0.5f, 0.0f, 60.0f);
        flow_stepper_f(cmd, ui_element, f, "Ridge width (m)", &s->ridge_width, 1.0f, 0.0f, 80.0f);
        flow_stepper_f(cmd, ui_element, f, "Lip over the cut (m)", &s->lip, 0.5f, 0.0f, 40.0f);
        flow_text(cmd, ui_element, f, "A lip hangs over the cut: it shows in the preview", get_theme_hint_color());
        flow_text(cmd, ui_element, f, "and is built as a separate object.", get_theme_hint_color());
        break;

    default:
        if (s->tool == tt_tool_tunnel) {
            flow_text(cmd, ui_element, f, "Tunnels become part of the terrain.", get_theme_hint_color());
        } else if (s->tool == tt_tool_overhang) {
            flow_text(cmd, ui_element, f, "An overhang is always its own object.", get_theme_hint_color());
        } else {
            flow_text(cmd, ui_element, f, "Keep it as", system_theme.system_text);
            int separate = 0;
            if (s->separate) { separate = 1; }
            flow_choice(cmd, ui_element, f, "Single (terrain)", &separate, 0);
            flow_choice(cmd, ui_element, f, "Separate object", &separate, 1);
            s->separate = (separate == 1);
            flow_newline(f);
        }
        f->y += 6;
        if (flow_button(cmd, ui_element, f, "Accept and merge", system_theme.tab_sub_menubar_button_active)) {
            accept_terrain_tool();
        }
        flow_newline(f);
        break;
    }
}

/* the tool name on top, then the five steps one under the other: a header for each, the open one shows its controls.
   every step draws its live preview in the Scene view. */
static void tools_terrain_steps(VkCommandBuffer cmd, system_element_context* ui_element, flow_layout* f)
{
    terrain_tools_state* s = &g_terrain_tools;
    flow_text(cmd, ui_element, f, get_terrain_tools_name(s->tool), system_theme.menu_active_text);
    f->y -= 4;

    for (int step = 1; step <= tt_step_count; step++) {
        char label[96];
        snprintf(label, sizeof(label), "%d  %s", step, get_terrain_tools_step_name(s->tool, step));
        system_color color = system_theme.tab_sub_menubar_button;
        if (s->step == step) {
            color = system_theme.tab_sub_menubar_button_active;
        }
        flow_newline(f);
        const int header_w = f->max_x - f->start_x;
        if (ui_button(cmd, ui_element, label, f->start_x, f->y, header_w, f->row_h + 2, color)) {
            set_terrain_tool_step(step);
        }
        f->x = f->start_x;
        f->y += f->row_h + 6;
        if (s->step == step) {
            draw_terrain_step_body(cmd, ui_element, f, s, step);
            flow_newline(f);
            f->y += 4;
        }
    }

    flow_newline(f);
    f->y += 4;
    if (flow_button(cmd, ui_element, f, "Back", system_theme.tab_sub_menubar_button)) {
        set_terrain_tool_step(s->step - 1);
    }
    if (flow_button(cmd, ui_element, f, "Next", system_theme.tab_sub_menubar_button)) {
        set_terrain_tool_step(s->step + 1);
    }
    if (flow_button(cmd, ui_element, f, "Close tool", system_theme.tab_sub_menubar_button)) {
        close_terrain_tool();
    }
}
/* ---------------------------------------------------------------- window */

void draw_modeler_window(system_element_context* ui_ctx, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    mode_state_sync_from_ui(ui_ctx);

    {   /* one rising-edge_to_face_array sample per frame for every button below */
        static bool prev_down = false;
        s_pressed = ui_ctx->input.mouse_down && !prev_down;
        prev_down = ui_ctx->input.mouse_down;
    }

    draw_window_element(cmd, ui_ctx, "Modeler", node->x, content_y, node->w, content_h, system_theme.tab_content_window_modeler);

    if (g_terrain_tools.tool != tt_tool_none) {
        flow_layout tf;
        tf.start_x = node->x + 15;
        tf.x       = tf.start_x;
        tf.y       = content_y + 12;
        tf.max_x   = node->x + node->w - 15;
        tf.row_h   = 22;
        tools_terrain_steps(cmd, ui_ctx, &tf);
        return;
    }

    system_mode_context ctx = g_mode_state.context;

    const system_color header = system_theme.menu_active_text;   /* one themed header color for every mode */

    char title[64];
    snprintf(title, sizeof(title), "%s Tools", mode_context_name(ctx));
    draw_text_element(cmd, ui_ctx, title, node->x + 15, content_y + 15, header);
    draw_text_element(cmd, ui_ctx, "Dim buttons are not implemented yet.", node->x + 15, content_y + 33, get_theme_hint_color());

    flow_layout f;
    f.start_x = node->x + 15;
    f.x       = f.start_x;
    f.y       = content_y + 54;
    f.max_x   = node->x + node->w - 15;
    f.row_h   = 22;

    switch (ctx) {
        case mode_context_object:    tools_object(cmd, ui_ctx, &f);    break;
        case mode_context_edit:      tools_edit(cmd, ui_ctx, &f);      break;
        case mode_context_uv:        tools_uv(cmd, ui_ctx, &f);        break;
        case mode_context_model:     tools_model(cmd, ui_ctx, &f);     break;
        case mode_context_sculpt:    tools_sculpt(cmd, ui_ctx, &f);    break;
        case mode_context_paint:     tools_paint(cmd, ui_ctx, &f);     break;
        case mode_context_animation: tools_animation(cmd, ui_ctx, &f); break;
    }
}