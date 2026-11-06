#include "system_globals.h"
#include "system_context_menu.h"


/* ========================================================================= */
/* menu data. to add or change a menu you only edit this block + run_action.  */
/*   see the slot_* enum below. a slot left out of a menu is empty.                      */
/*   a slot with ONE item runs it on click. several items open a dropdown.     */
/*   action_none = not wired yet: shown dimmed, logs when clicked.             */
/*                                                                             */
/*   the scene view menu follows the ONE active mode (Object / Edit / Sculpt /  */
/*   Paint / Texture Shop), so every mode shows its own tools.                 */
/* ========================================================================= */
typedef enum {
    action_none = 0,

    /* modes: exactly one is on */
    action_set_object, action_set_edit, action_set_sculpt, action_set_paint, action_set_textureshop,

    /* object tools */
    action_tool_select, action_tool_translate, action_tool_rotate, action_tool_scale,
    action_tool_show_manip,
    /* snapping */
    action_snap_off, action_snap_point, action_snap_grid, action_snap_grid_up, action_snap_grid_down,
    /* edit mode modeling tools */
    action_edit_target_weld, action_edit_multicut,
    action_edit_extrude, action_edit_bridge, action_edit_merge, action_edit_delete, action_edit_flip, action_edit_insert_loop,
    /* edit mode component type: the selection is converted when you switch (vertex <-> edge_to_face_array <-> face) */
    action_edit_sel_vert, action_edit_sel_edge, action_edit_sel_face,
    action_open_brushes, action_open_sculpt_win, action_open_paint_win,
    action_mesh_densify,
    action_work_anim,
    action_open_board, action_open_card,
    action_open_texshop, action_open_files,

    /* texture shop: skins */
    action_texshop_new_skin, action_texshop_copy_skin, action_texshop_delete_skin,
    action_texshop_next_skin, action_texshop_prev_skin,
    action_texshop_assign_skin, action_texshop_clear_skin,

    /* sculpt */
    action_sculpt_radius_up, action_sculpt_radius_down, action_sculpt_strength_up, action_sculpt_strength_down,
    action_sculpt_invert,
    /* the nine brushes: keep these in the order of enum_sculpt_brush (run_action maps by offset) */
    action_sculpt_brush_0, action_sculpt_brush_1, action_sculpt_brush_2, action_sculpt_brush_3, action_sculpt_brush_4,
    action_sculpt_brush_5, action_sculpt_brush_6, action_sculpt_brush_7, action_sculpt_brush_8,
    action_sculpt_sym_x, action_sculpt_sym_y, action_sculpt_sym_z,
    action_sculpt_subdivide, action_sculpt_undivide,
    action_sculpt_radius_big, action_sculpt_radius_small,
    action_sculpt_hardness_up, action_sculpt_hardness_down,
    action_sculpt_plane_up, action_sculpt_plane_down,
    action_sculpt_smooth_up, action_sculpt_smooth_down,
    action_sculpt_spacing_up, action_sculpt_spacing_down,
    action_sculpt_smooth_subdiv, action_sculpt_nav,

    /* paint */
    action_paint_size_up, action_paint_size_down, action_paint_opacity_up, action_paint_opacity_down,
    action_paint_size_big, action_paint_size_small,
    action_paint_opacity_full, action_paint_opacity_half, action_paint_opacity_quarter,
    action_paint_hardness_up, action_paint_hardness_down,
    /* the nine paint brushes: keep these in the order of brush_paint_kind in draw_system_brush_window.c */
    action_paint_brush_0, action_paint_brush_1, action_paint_brush_2, action_paint_brush_3, action_paint_brush_4,
    action_paint_brush_5, action_paint_brush_6, action_paint_brush_7, action_paint_brush_8,
    /* quick colours (run_action has the table), brightness, saved colours */
    action_paint_color_0, action_paint_color_1, action_paint_color_2, action_paint_color_3, action_paint_color_4,
    action_paint_color_5, action_paint_color_6, action_paint_color_7, action_paint_color_8, action_paint_color_9,
    action_paint_color_lighter, action_paint_color_darker,
    action_paint_color_save, action_paint_color_next, action_paint_color_prev,

    /* hierarchy */
    action_create_empty, action_create_camera,
    action_deselect, action_delete_selected,
    action_expand_all, action_collapse_all,

    /* file archive */
    action_file_new_folder, action_file_new_text, action_file_new_script, action_file_new_scene,
    action_file_deselect, action_file_delete,
    action_file_expand_all, action_file_collapse_all, action_file_refresh,
    action_file_reveal,
} menu_action;

typedef struct { const char* label; menu_action action; } menu_item;
typedef struct { const char* label; const menu_item* items; int face_type; } menu_slot;
/* the scene view radial. one layout for every mode:

                    [ header: mode specific ]                [ right 0 ]
        Object                                               [ right 1 ]
        Edit                                                 [ right 2 ]
        Sculpt                   ( X )                       [ right 3 ]
        Paint                                                [ right 4 ]
        Textureshop        Grab Move Rotate Scale            [ right 5 ]

   LEFT column: the five modes, same in every scene mode. one click, the radial stays open and redraws as the new mode.
   BOTTOM row:  Grab Move Rotate Scale, same in every scene mode.
   HEADER row:  Edit = Vertex Edge Face, Sculpt = Radius Strength, Paint = the quick colors.
   RIGHT column: the tools of the mode.
   the current mode / tool / component is lit. a slot a menu does not fill is empty (label NULL). */
enum {
    slot_left_0 = 0, slot_left_1, slot_left_2, slot_left_3, slot_left_4,                       /* rows next to the modes */
    slot_right_0, slot_right_1, slot_right_2, slot_right_3, slot_right_4, slot_right_5,        /* right_0 sits on the header row, right_1.. on the mode rows */
    slot_head_0, slot_head_1, slot_head_2,                                                      /* centered above the circle */
    slot_bot_0, slot_bot_1, slot_bot_2, slot_bot_3,                                             /* centered below the circle */
    menu_slot_count
};

typedef struct { const char* title; menu_slot slots[menu_slot_count]; int swatches; } menu_def;   /* title shows in the center circle: 5 characters fit. swatches = 1: the quick colors fill the header row */

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define SLOT(label, items) { label, items, COUNT(items) }      /* slots are filled by name: [slot_right_1] = SLOT("Brush", sc_brush) */

/* ---- left column: the modes. each slot is ONE item, so a click runs it ---- */
static const menu_item it_mode_object[]      = { {"Object",      action_set_object}      };
static const menu_item it_mode_edit[]        = { {"Edit",        action_set_edit}        };
static const menu_item it_mode_sculpt[]      = { {"Sculpt",      action_set_sculpt}      };
static const menu_item it_mode_paint[]       = { {"Paint",       action_set_paint}       };
static const menu_item it_mode_textureshop[] = { {"Textureshop", action_set_textureshop} };

#define MODE_COLUMN_SLOTS \
    [slot_left_0] = SLOT("Object",      it_mode_object),      \
    [slot_left_1] = SLOT("Edit",        it_mode_edit),        \
    [slot_left_2] = SLOT("Sculpt",      it_mode_sculpt),      \
    [slot_left_3] = SLOT("Paint",       it_mode_paint),       \
    [slot_left_4] = SLOT("Textureshop", it_mode_textureshop)

/* ---- bottom row: transforms ---- */
static const menu_item it_grab[]    = { {"Grab",   action_tool_select}    };
static const menu_item it_move[]    = { {"Move",   action_tool_translate} };
static const menu_item it_rotate[]  = { {"Rotate", action_tool_rotate}    };
static const menu_item it_scale[]   = { {"Scale",  action_tool_scale}     };

#define TRANSFORM_ROW_SLOTS \
    [slot_bot_0] = SLOT("Grab",   it_grab),   \
    [slot_bot_1] = SLOT("Move",   it_move),   \
    [slot_bot_2] = SLOT("Rotate", it_rotate), \
    [slot_bot_3] = SLOT("Scale",  it_scale)

/* ---- snapping ---- */
static const menu_item it_snap[]      = { {"Snap Off", action_snap_off}, {"Point to Point", action_snap_point}, {"Point to Grid", action_snap_grid},
                                          {"Grid Size +", action_snap_grid_up}, {"Grid Size -", action_snap_grid_down} };
static const menu_item it_snap_mode[] = { {"Snap Off", action_snap_off}, {"Point to Point", action_snap_point}, {"Point to Grid", action_snap_grid} };
static const menu_item it_snap_grid[] = { {"Grid Size +", action_snap_grid_up}, {"Grid Size -", action_snap_grid_down} };

/* not wired yet: shown dimmed, logs when clicked */
static const menu_item it_mask[]      = { {"Mask", action_none} };

/* ---- scene view: object mode ---- */
static const menu_def k_scene_object = { "Obj", {
    MODE_COLUMN_SLOTS,
    TRANSFORM_ROW_SLOTS,
    [slot_right_1] = SLOT("Snap", it_snap),
}, 0 };

/* ---- scene view: edit mode ---- */
static const menu_item edit_vert[]    = { {"Vertex", action_edit_sel_vert} };
static const menu_item edit_edge[]    = { {"Edge",   action_edit_sel_edge} };
static const menu_item edit_face[]    = { {"Face",   action_edit_sel_face} };
static const menu_item edit_build[]   = { {"Extrude", action_edit_extrude}, {"Bridge", action_edit_bridge}, {"Insert Loop", action_edit_insert_loop}, {"Multicut", action_edit_multicut} };
static const menu_item edit_cleanup[] = { {"Merge", action_edit_merge}, {"Target Weld", action_edit_target_weld}, {"Delete", action_edit_delete}, {"Flip Normals", action_edit_flip} };

/* ---- sculpt (also used by edit for symmetry and by paint for subdivide) ---- */
static const menu_item sc_radius[]   = { {"Radius +", action_sculpt_radius_up}, {"Radius -", action_sculpt_radius_down},
                                        {"Radius x2", action_sculpt_radius_big}, {"Radius /2", action_sculpt_radius_small} };
static const menu_item sc_strength[] = { {"Strength +", action_sculpt_strength_up}, {"Strength -", action_sculpt_strength_down} };
static const menu_item sc_brush[]    = {
    {"Clay Buildup", action_sculpt_brush_0}, {"Clay Soften", action_sculpt_brush_1}, {"Standard", action_sculpt_brush_2},
    {"Carve", action_sculpt_brush_3},        {"Grab", action_sculpt_brush_4},        {"Polish", action_sculpt_brush_5},
    {"Inflate", action_sculpt_brush_6},      {"Smooth", action_sculpt_brush_7},      {"Flatten", action_sculpt_brush_8} };
static const menu_item sc_invert[]   = { {"Invert", action_sculpt_invert} };
static const menu_item sc_sym[]      = { {"Symmetry X", action_sculpt_sym_x}, {"Symmetry Y", action_sculpt_sym_y}, {"Symmetry Z", action_sculpt_sym_z} };
static const menu_item sc_subdiv[]   = { {"Subdivide", action_sculpt_subdivide}, {"Undivide", action_sculpt_undivide}, {"Densify (many levels)", action_mesh_densify} };
static const menu_item sc_settings[] = { {"Hardness +", action_sculpt_hardness_up}, {"Hardness -", action_sculpt_hardness_down},
                                        {"Plane Offset +", action_sculpt_plane_up}, {"Plane Offset -", action_sculpt_plane_down},
                                        {"Smooth Passes +", action_sculpt_smooth_up}, {"Smooth Passes -", action_sculpt_smooth_down},
                                        {"Spacing +", action_sculpt_spacing_up}, {"Spacing -", action_sculpt_spacing_down},
                                        {"Smooth Subdivision", action_sculpt_smooth_subdiv}, {"Navigation", action_sculpt_nav} };

static const menu_def k_scene_edit = { "Edit", {
    MODE_COLUMN_SLOTS,
    TRANSFORM_ROW_SLOTS,
    [slot_head_0]  = SLOT("Vertex",    edit_vert),
    [slot_head_1]  = SLOT("Edge",      edit_edge),
    [slot_head_2]  = SLOT("Face",      edit_face),
    [slot_right_0] = SLOT("Snap",      it_snap_mode),
    [slot_right_1] = SLOT("Grid Size", it_snap_grid),
    [slot_right_2] = SLOT("Build",     edit_build),
    [slot_right_3] = SLOT("Cleanup",   edit_cleanup),
    [slot_right_4] = SLOT("Symmetry",  sc_sym),
    [slot_right_5] = SLOT("Mask",      it_mask),
}, 0 };

/* ---- scene view: sculpt mode ---- */
static const menu_def k_scene_sculpt = { "Scpt", {
    MODE_COLUMN_SLOTS,
    TRANSFORM_ROW_SLOTS,
    [slot_head_0]  = SLOT("Radius",   sc_radius),
    [slot_head_1]  = SLOT("Strength", sc_strength),
    [slot_right_0] = SLOT("Subdiv",   sc_subdiv),
    [slot_right_1] = SLOT("Brush",    sc_brush),
    [slot_right_2] = SLOT("Invert",   sc_invert),
    [slot_right_3] = SLOT("Symmetry", sc_sym),
    [slot_right_4] = SLOT("Mask",     it_mask),
    [slot_right_5] = SLOT("Settings", sc_settings),
}, 0 };

/* ---- scene view: paint mode. the header row is the quick colors (swatches = 1) ---- */
static const menu_item pt_size[]     = { {"Size +", action_paint_size_up}, {"Size -", action_paint_size_down},
                                         {"Size x2", action_paint_size_big}, {"Size /2", action_paint_size_small} };
static const menu_item pt_opacity[]  = { {"Opacity +", action_paint_opacity_up}, {"Opacity -", action_paint_opacity_down},
                                         {"Opacity 100%", action_paint_opacity_full}, {"Opacity 50%", action_paint_opacity_half}, {"Opacity 25%", action_paint_opacity_quarter} };
static const menu_item pt_hardness[] = { {"Hardness +", action_paint_hardness_up}, {"Hardness -", action_paint_hardness_down} };
/* the nine paint brushes, same order as brush_paint_kind */
static const menu_item pt_brush[]    = {
    {"Paint", action_paint_brush_0},  {"Airbrush", action_paint_brush_1}, {"Eraser", action_paint_brush_2},
    {"Smudge", action_paint_brush_3}, {"Blur", action_paint_brush_4},     {"Fill", action_paint_brush_5},
    {"Picker", action_paint_brush_6}, {"Lighten", action_paint_brush_7},  {"Darken", action_paint_brush_8} };
static const menu_item pt_colors[]   = { {"Lighter", action_paint_color_lighter}, {"Darker", action_paint_color_darker},
                                         {"Save Color", action_paint_color_save}, {"Next Saved Color", action_paint_color_next}, {"Previous Saved Color", action_paint_color_prev} };

static const menu_def k_scene_paint = { "Paint", {
    MODE_COLUMN_SLOTS,
    TRANSFORM_ROW_SLOTS,
    [slot_right_0] = SLOT("Subdiv",   sc_subdiv),        /* paint needs vertices, same items as Sculpt */
    [slot_right_1] = SLOT("Brush",    pt_brush),
    [slot_right_2] = SLOT("Size",     pt_size),
    [slot_right_3] = SLOT("Opacity",  pt_opacity),
    [slot_right_4] = SLOT("Hardness", pt_hardness),
    [slot_right_5] = SLOT("Colors",   pt_colors),
}, 1 };

/* ---- texture shop mode: skins. Used by the Scene view in Texture Shop mode AND by the three Texture Shop windows ---- */
static const menu_item ts_create[] = { {"New Skin", action_texshop_new_skin}, {"Duplicate Skin", action_texshop_copy_skin} };
static const menu_item ts_select[] = { {"Next Skin", action_texshop_next_skin}, {"Previous Skin", action_texshop_prev_skin} };
static const menu_item ts_delete[] = { {"Delete Skin", action_texshop_delete_skin} };
static const menu_item ts_assign[] = { {"Assign Skin", action_texshop_assign_skin}, {"Clear Skin", action_texshop_clear_skin} };   /* onto the object obj_vert_selected in the Hierarchy */

static const menu_def k_scene_textureshop = { "Skin", {
    MODE_COLUMN_SLOTS,
    TRANSFORM_ROW_SLOTS,
    [slot_right_1] = SLOT("Create", ts_create),
    [slot_right_2] = SLOT("Select", ts_select),
    [slot_right_3] = SLOT("Assign", ts_assign),
    [slot_right_4] = SLOT("Delete", ts_delete),
}, 0 };

/* ---- hierarchy (scene archive): add and delete objects ---- */
static const menu_item hier_create[]   = { {"Empty", action_create_empty}, {"Camera", action_create_camera} };
static const menu_item hier_deselect[] = { {"Deselect", action_deselect} };
static const menu_item hier_delete[]   = { {"Delete", action_delete_selected} };
static const menu_item hier_view[]     = { {"Expand All", action_expand_all}, {"Collapse All", action_collapse_all} };

static const menu_def k_hier_menu = { "Tree", {
    [slot_right_1] = SLOT("Create",   hier_create),
    [slot_right_2] = SLOT("Deselect", hier_deselect),
    [slot_right_3] = SLOT("Delete",   hier_delete),
    [slot_right_4] = SLOT("View",     hier_view),
}, 0 };

/* ---- file archive: add and delete folders and files ---- */
static const menu_item files_create[]   = { {"Folder", action_file_new_folder}, {"Text File", action_file_new_text},
                                            {"Script", action_file_new_script}, {"Scene", action_file_new_scene} };
static const menu_item files_deselect[] = { {"Deselect", action_file_deselect} };
static const menu_item files_delete[]   = { {"Delete", action_file_delete} };               /* to the recycle bin */
static const menu_item files_explorer[] = { {"Explorer", action_file_reveal} };             /* shows the selection in Windows Explorer */
static const menu_item files_view[]     = { {"Expand All", action_file_expand_all}, {"Collapse All", action_file_collapse_all}, {"Refresh", action_file_refresh} };

static const menu_def k_files_menu = { "Files", {
    [slot_right_1] = SLOT("Create",   files_create),
    [slot_right_2] = SLOT("Deselect", files_deselect),
    [slot_right_3] = SLOT("Delete",   files_delete),
    [slot_right_4] = SLOT("View",     files_view),
    [slot_right_5] = SLOT("Explorer", files_explorer),
}, 0 };


/* ========================================================================= */
/* what each action does                                                      */
/* ========================================================================= */
#define clampf context_menu_clampf
static float context_menu_clampf(float vert_to_edge_array, float lo, float hi) { return vert_to_edge_array < lo ? lo : (vert_to_edge_array > hi ? hi : vert_to_edge_array); }

static void enter_mode(system_element_context* ui_element, system_mode_context ctx)
{
    mode_menu_enter_mode(ui_element, ctx, true);      /* same switch as the mode menu: one mode on, the rest off */
}

/* Texture Shop is a real mode now: flags, windows (Texture Shop + UV Editor + Nodes) and the Skin radial all follow it */
static void enter_textureshop_mode(system_element_context* ui_element)
{
    mode_menu_enter_mode(ui_element, mode_context_textureshop, true);
}

/* Vertex / Edge / Face. The selection follows you: what is obj_vert_selected as vertices becomes the edges / faces it fully covers,
   and what is obj_vert_selected as faces becomes their vertices. Both the UI field and the mode state are set, mode_state_sync_from_ui
   copies the UI field every frame, so they agree. */
static void set_select_type(system_element_context* ui_element, enum_mesh_selection t)
{
    system_scene_object* obj = NULL;
    edit_mode_mesh* mesh = NULL;
    if (ui_element->active_select_type != t && edit_get_target(&obj, &mesh) && mesh)
        em_sync_selection(mesh, ui_element->active_select_type);   /* make verts / edges / faces agree under the OLD type first */
    ui_element->active_select_type = t;
    g_mode_state.edit.select_type = t;
    edit_msg("Select: %s", t == select_vert ? "Vertex" : (t == select_edge ? "Edge" : "Face"));   /* on screen proof the click landed */
}

static const float k_quick_colors[10][3] = {
    { 1.00f, 1.00f, 1.00f }, { 0.00f, 0.00f, 0.00f }, { 0.90f, 0.15f, 0.15f }, { 1.00f, 0.55f, 0.10f }, { 1.00f, 0.90f, 0.20f },
    { 0.20f, 0.75f, 0.30f }, { 0.20f, 0.85f, 0.90f }, { 0.20f, 0.35f, 0.90f }, { 0.60f, 0.30f, 0.85f }, { 1.00f, 0.50f, 0.70f }
};

static void run_action(system_element_context* ui_element, menu_action a)
{
    manage_docking* dock = &dock_workspace;

    switch (a) {
    case action_set_object:      enter_mode(ui_element, mode_context_object); break;
    case action_set_edit:        enter_mode(ui_element, mode_context_edit);   break;
    case action_set_sculpt:      enter_mode(ui_element, mode_context_sculpt); break;
    case action_set_paint:       enter_mode(ui_element, mode_context_paint);  break;
    case action_set_textureshop: enter_textureshop_mode(ui_element);         break;

    /* picking a normal tool turns Show Manipulator off: only one manipulator at a time */
    case action_tool_select:     manip_set_show(ui_element, false); ui_element->active_tool = tool_select;    break;
    case action_tool_translate:  manip_set_show(ui_element, false); ui_element->active_tool = tool_translate; break;
    case action_tool_rotate:     manip_set_show(ui_element, false); ui_element->active_tool = tool_rotate;    break;
    case action_tool_scale:      manip_set_show(ui_element, false); ui_element->active_tool = tool_scale;     break;
    case action_tool_show_manip: manip_set_show(ui_element, !g_manip.show_manipulator);               break;

    case action_snap_off:        manip_set_snap(manip_snap_off);            break;
    case action_snap_point:      manip_set_snap(manip_snap_point_to_point); break;
    case action_snap_grid:       manip_set_snap(manip_snap_point_to_grid);  break;
    case action_snap_grid_up:    manip_scale_grid(2.0f);                    break;
    case action_snap_grid_down:  manip_scale_grid(0.5f);                    break;

    case action_edit_target_weld: edit_ops_run(ui_element, edit_target_weld);          break;
    case action_edit_insert_loop: edit_ops_run(ui_element, edit_insert_edge_loop);     break;
    case action_edit_extrude:     edit_ops_run(ui_element, edit_extrude);              break;
    case action_edit_bridge:      edit_ops_run(ui_element, edit_bridge);               break;
    case action_edit_merge:       edit_ops_run(ui_element, edit_merge_vertices);       break;
    case action_edit_delete:      edit_ops_run(ui_element, edit_delete_component);     break;
    case action_edit_flip:        edit_ops_run(ui_element, edit_flip_normals);         break;
    case action_edit_multicut:    manip_toggle_edit_tool(manip_edit_multicut);    break;
    case action_edit_sel_vert:    set_select_type(ui_element, select_vert); break;
    case action_edit_sel_edge:    set_select_type(ui_element, select_edge); break;
    case action_edit_sel_face:    set_select_type(ui_element, select_face); break;

    case action_work_anim:
        mode_state_set_exclusive(ui_element, mode_context_animation);
        spawn_dock_view_beside(dock, view_type_scene, view_type_game, view_type_scene);
        break;
    case action_open_texshop:    enter_textureshop_mode(ui_element);           break;
    case action_open_board:      mode_menu_open_board_builder(ui_element);     break;
    case action_open_card:       mode_menu_open_card_builder(ui_element);      break;
    case action_open_files:      open_file_archive_window(dock);       break;
    case action_open_brushes:    brush_open_window();                  break;
    case action_open_sculpt_win: spawn_dock_view_beside(dock, view_type_light, view_type_inspector, view_type_sculpt); break;
    case action_open_paint_win:  spawn_dock_view_beside(dock, view_type_light, view_type_inspector, view_type_paint);  break;
    case action_mesh_densify:    brush_densify_selected(20000u);       break;

    /* texture shop: the skin list lives in draw_system_textureshop.c */
    case action_texshop_new_skin:    create_texshop_skin();      break;
    case action_texshop_copy_skin:   duplicate_texshop_skin();   break;
    case action_texshop_delete_skin: delete_texshop_skin();      break;
    case action_texshop_next_skin:   step_texshop_skin(1);       break;
    case action_texshop_prev_skin:   step_texshop_skin(-1);      break;
    case action_texshop_assign_skin: assign_texshop_skin_to_selected(); break;
    case action_texshop_clear_skin:  clear_texshop_skin_on_selected();  break;

    /* sculpt: g_sculpt is the source of truth (g_mode_state.sculpt is mirrored from it every frame) */
    case action_sculpt_radius_up:     { sculpt_brush_settings* s = sculpt_current_settings(); s->radius   *= 1.25f; sculpt_clamp_settings(s); } break;
    case action_sculpt_radius_down:   { sculpt_brush_settings* s = sculpt_current_settings(); s->radius   *= 0.80f; sculpt_clamp_settings(s); } break;
    case action_sculpt_strength_up:   { sculpt_brush_settings* s = sculpt_current_settings(); s->strength += 0.1f;  sculpt_clamp_settings(s); } break;
    case action_sculpt_strength_down: { sculpt_brush_settings* s = sculpt_current_settings(); s->strength -= 0.1f;  sculpt_clamp_settings(s); } break;
    case action_sculpt_invert:        g_sculpt.invert = !g_sculpt.invert; break;
    case action_sculpt_sym_x:         g_sculpt.sym_x = !g_sculpt.sym_x; break;
    case action_sculpt_sym_y:         g_sculpt.sym_y = !g_sculpt.sym_y; break;
    case action_sculpt_sym_z:         g_sculpt.sym_z = !g_sculpt.sym_z; break;
    case action_sculpt_radius_big:    { sculpt_brush_settings* s = sculpt_current_settings(); s->radius   *= 2.0f;  sculpt_clamp_settings(s); } break;
    case action_sculpt_radius_small:  { sculpt_brush_settings* s = sculpt_current_settings(); s->radius   *= 0.5f;  sculpt_clamp_settings(s); } break;
    case action_sculpt_hardness_up:   { sculpt_brush_settings* s = sculpt_current_settings(); s->hardness += 0.1f;  sculpt_clamp_settings(s); } break;
    case action_sculpt_hardness_down: { sculpt_brush_settings* s = sculpt_current_settings(); s->hardness -= 0.1f;  sculpt_clamp_settings(s); } break;
    case action_sculpt_plane_up:      { sculpt_brush_settings* s = sculpt_current_settings(); s->plane_offset += 0.1f; sculpt_clamp_settings(s); } break;
    case action_sculpt_plane_down:    { sculpt_brush_settings* s = sculpt_current_settings(); s->plane_offset -= 0.1f; sculpt_clamp_settings(s); } break;
    case action_sculpt_smooth_up:     { sculpt_brush_settings* s = sculpt_current_settings(); s->smooth_passes += 1;   sculpt_clamp_settings(s); } break;
    case action_sculpt_smooth_down:   { sculpt_brush_settings* s = sculpt_current_settings(); s->smooth_passes -= 1;   sculpt_clamp_settings(s); } break;
    case action_sculpt_spacing_up:    { sculpt_brush_settings* s = sculpt_current_settings(); s->spacing  += 0.05f; sculpt_clamp_settings(s); } break;
    case action_sculpt_spacing_down:  { sculpt_brush_settings* s = sculpt_current_settings(); s->spacing  -= 0.05f; sculpt_clamp_settings(s); } break;
    case action_sculpt_smooth_subdiv: g_sculpt.smooth_subdivide = !g_sculpt.smooth_subdivide; break;
    case action_sculpt_nav:           g_sculpt.nav = (g_sculpt.nav == sculpt_nav_maya) ? sculpt_nav_zbrush : sculpt_nav_maya; break;
    case action_sculpt_subdivide:     sculpt_subdivide_selected(); break;
    case action_sculpt_undivide:      sculpt_undivide_selected();  break;
    case action_sculpt_brush_0: case action_sculpt_brush_1: case action_sculpt_brush_2:
    case action_sculpt_brush_3: case action_sculpt_brush_4: case action_sculpt_brush_5:
    case action_sculpt_brush_6: case action_sculpt_brush_7: case action_sculpt_brush_8:
        sculpt_set_brush((enum_sculpt_brush)(a - action_sculpt_brush_0));
        break;

    /* paint: g_paint is the source of truth (paint_dab reads it, g_mode_state.paint is not used by the brush) */
    case action_paint_size_up:        g_paint.radius  *= 1.25f; paint_clamp_settings(); break;
    case action_paint_size_down:      g_paint.radius  *= 0.80f; paint_clamp_settings(); break;
    case action_paint_opacity_up:     g_paint.opacity += 0.1f;  paint_clamp_settings(); break;
    case action_paint_opacity_down:   g_paint.opacity -= 0.1f;  paint_clamp_settings(); break;
    case action_paint_size_big:       g_paint.radius  *= 2.0f;  paint_clamp_settings(); break;
    case action_paint_size_small:     g_paint.radius  *= 0.5f;  paint_clamp_settings(); break;
    case action_paint_opacity_full:   g_paint.opacity = 1.0f;   paint_clamp_settings(); break;
    case action_paint_opacity_half:   g_paint.opacity = 0.5f;   paint_clamp_settings(); break;
    case action_paint_opacity_quarter:g_paint.opacity = 0.25f;  paint_clamp_settings(); break;
    case action_paint_hardness_up:    g_paint.hardness += 0.1f; paint_clamp_settings(); break;
    case action_paint_hardness_down:  g_paint.hardness -= 0.1f; paint_clamp_settings(); break;
    case action_paint_brush_0: case action_paint_brush_1: case action_paint_brush_2:
    case action_paint_brush_3: case action_paint_brush_4: case action_paint_brush_5:
    case action_paint_brush_6: case action_paint_brush_7: case action_paint_brush_8:
        brush_window_set_paint_brush((int)(a - action_paint_brush_0));
        break;
    case action_paint_color_0: case action_paint_color_1: case action_paint_color_2: case action_paint_color_3: case action_paint_color_4:
    case action_paint_color_5: case action_paint_color_6: case action_paint_color_7: case action_paint_color_8: case action_paint_color_9: {
        const int ci = (int)(a - action_paint_color_0);
        for (int c = 0; c < 3; c++) g_paint.color[c] = k_quick_colors[ci][c];
        break;
    }
    case action_paint_color_lighter:  brush_color_shift(g_paint.color, +0.10f); break;
    case action_paint_color_darker:   brush_color_shift(g_paint.color, -0.10f); break;
    case action_paint_color_save:     brush_saved_color_add(g_paint.color);     break;
    case action_paint_color_next:     brush_saved_color_cycle(+1, g_paint.color); break;
    case action_paint_color_prev:     brush_saved_color_cycle(-1, g_paint.color); break;

    case action_create_empty:
        spawn_new_scene_object("Empty", (editor_vec3){ 0.0f, 0.0f, 0.0f });
        break;
    case action_create_camera: {
        system_scene_object* o = spawn_new_scene_object("Camera", (editor_vec3){ 0.0f, 0.0f, 0.0f });
        if (o) o->type = scene_object_camera;
        break;
    }
    case action_deselect:
        g_scene_registry.selected_object_id = 0;
        break;
    case action_delete_selected:
        if (g_scene_registry.selected_object_id != 0) {
            /* the Terrain and its region_xx belong together: choosing any of them removes the whole terrain */
            if (is_terrain_object(find_scene_object_by_id(g_scene_registry.selected_object_id))) delete_terrain();
            else delete_scene_object(g_scene_registry.selected_object_id);
        }
        break;
    case action_expand_all:
    case action_collapse_all:
        for (uint32_t i = 1; i < g_scene_registry.object_count; i++)      /* 0 = root, stays open */
            g_scene_registry.objects[i].is_open = (a == action_expand_all);
        break;

    /* file archive: the model (system_file_archive.c) does the work on the real folder */
    case action_file_new_folder:     file_archive_create_folder();                                         break;
    case action_file_new_text:       file_archive_create_file("New Text",   FILE_ARCHIVE_EXT_TEXT);       break;
    case action_file_new_script:     file_archive_create_file("New Script", FILE_ARCHIVE_EXT_SCRIPT);     break;
    case action_file_new_scene:      file_archive_create_file("New Scene",  FILE_ARCHIVE_EXT_SCENE);      break;
    case action_file_deselect:       file_archive_select(-1);                                              break;
    case action_file_delete:         file_archive_delete_selected();                                       break;
    case action_file_expand_all:     file_archive_set_all_open(true);                                      break;
    case action_file_collapse_all:   file_archive_set_all_open(false);                                     break;
    case action_file_refresh:        file_archive_refresh();                                               break;
    case action_file_reveal:         file_archive_reveal_selected();                                       break;

    case action_none:
    default: break;
    }
}


/* ========================================================================= */
/* layout                                                                     */
/* ========================================================================= */
enum {
    center_radius = 26,     /* the circle button */
    center_ring   = 1,      /* thickness of its ring in pixels */
    slot_w        = 104,    /* the side column buttons */
    slot_h        = 24,
    row_pitch     = 30,     /* distance between rows: 24 high + 6 empty */
    head_w        = 76,     /* header row buttons */
    bot_w         = 62,     /* transform row buttons */
    btn_gap       = 6,      /* empty space between buttons in a row */
    swatch_size   = 20,     /* quick color buttons */
    swatch_gap    = 4,
    col_x         = (4 * bot_w + 3 * btn_gap) / 2 + 14 + slot_w / 2,     /* side columns sit clear of the transform row */
    panel_pad     = 12,
    panel_hw      = col_x + slot_w / 2 + panel_pad,          /* half width / height of the whole menu (only used to keep it on screen) */
    panel_hh      = 3 * row_pitch + slot_h / 2 + panel_pad,
    drop_pad      = 4,
    drop_min_w    = 120,
    clamp_x       = panel_hw + 8,    /* keeps the whole menu on screen */
    clamp_y       = panel_hh + 8,
};

/* where a slot sits, relative to the menu center. row 0 is the header row, rows 1..5 are the mode rows, the circle is on row 3. */
typedef struct { int cx, cy, w; } slot_rect;

static slot_rect get_slot_rect(const menu_def* def, int slot)
{
    slot_rect r = { 0, 0, slot_w };
    if (slot <= slot_left_4) {
        r.cx = -col_x;  r.cy = (slot - slot_left_0 - 2) * row_pitch;
    } else if (slot <= slot_right_5) {
        r.cx =  col_x;  r.cy = (slot - slot_right_0 - 3) * row_pitch;
    } else if (slot <= slot_head_2) {                       /* the filled header buttons are centered as a group */
        int n = 0, before = 0;
        for (int i = slot_head_0; i <= slot_head_2; i++) if (def->slots[i].label) { if (i < slot) before++; n++; }
        const int step = head_w + btn_gap;
        r.w  = head_w;
        r.cx = before * step - (n - 1) * step / 2;
        r.cy = -3 * row_pitch;
    } else {
        const int step = bot_w + btn_gap;
        r.w  = bot_w;
        r.cx = (slot - slot_bot_0) * step - 3 * step / 2;
        r.cy = 2 * row_pitch;
    }
    return r;
}

/* ========================================================================= */
/* state                                                                      */
/* ========================================================================= */
typedef enum { ctx_scene, ctx_hierarchy, ctx_files, ctx_texshop } ctx_kind;

/* while the menu is open, ui_element->active_id holds this value. every widget in your ui_element only claims
   the mouse when active_id == 0, so nothing underneath reacts. same trick the camera drag uses. */
static const uint32_t k_lock_id = 0x7C7C7C01u;

static bool     s_open      = false;
static ctx_kind s_kind      = ctx_scene;
static int      s_cx = 0, s_cy = 0;
static int      s_slot_open = -1;      /* slot whose dropdown is showing */
static bool     s_closing   = false;   /* closed, but the left button is still held: keep the lock until it is let go */
static bool     s_opened_this_frame = false;
static bool     s_drop_valid = false;                      /* the open dropdown's rect, from the last frame */
static int      s_drop_x = 0, s_drop_y = 0, s_drop_w = 0, s_drop_h = 0;

/* edges are tracked here (once per frame in draw_context_menu), not through the input system */
static bool     s_right_event = false; /* a WM_RBUTTONDOWN arrived since the last draw_context_menu */
static int      s_right_x = 0, s_right_y = 0;
static bool     s_prev_left   = false;
static bool     s_prev_esc    = false;
static uint32_t s_last_frame  = 0xFFFFFFFFu;

extern uint32_t g_ui_frame_index;      /* system_element.c */

bool is_context_menu_open(void) { return s_open; }
static bool s_real_left = false;   /* the real left button, saved before it is masked from the widgets */
static bool s_begun     = false;

/* Called at the START of the dock draw. While the menu is open (or the click that closed it is still held) the widgets
   under it must not see the mouse at all, otherwise a click on a menu slot also hits the tab close buttons / buttons
   below it. That is what removed buttons and windows. */
void context_menu_begin_frame(system_element_context* ui_element)
{
    if (!ui_element) return;
    s_real_left = ui_element->input.mouse_down;
    s_begun     = true;
    if (s_open || s_closing) {
        ui_element->input.mouse_down = false;
        ui_element->active_id        = k_lock_id;
    }
}

void context_menu_notify_right_button(int x, int y)
{
    s_right_event = true;
    s_right_x = x;
    s_right_y = y;
    phoenix_info("ContextMenu", "right button message received at %d,%d", x, y);
}

static bool in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < (x + w) && py >= y && py < (y + h);
}

bool context_menu_right_click_in(int x, int y, int w, int h)
{
    return s_right_event && in_rect(s_right_x, s_right_y, x, y, w, h);
}

static void close_menu(void)
{
    s_open      = false;
    s_slot_open = -1;
    s_closing   = true;    /* the click that closed us must not land on whatever is under the cursor */
}

static void open_menu(system_element_context* ui_element, ctx_kind kind)
{
    /* only refuse while a real drag is going on. a stale active_id (the scene archive leaves object ids
       >= 2000 behind, its clear-range is 1000..1299) used to make this return silently = "menu never opens". */
    if (ui_element->input.mouse_down || ui_element->input.camera_manipulating) {
        phoenix_info("ContextMenu", "right click ignored: left button / camera drag is active");
        return;
    }
    ui_element->active_id = 0;
    s_open      = true;
    s_closing   = false;
    s_kind      = kind;
    s_cx        = s_right_x;           /* where the click landed */
    s_cy        = s_right_y;
    s_slot_open = -1;
    s_drop_valid = false;
    s_opened_this_frame = true;
}

/* ------------------------------------------------------------------------- */
/* hooks: called from each window's own draw function                         */
/* ------------------------------------------------------------------------- */
static void window_hook(system_element_context* ui_element, ctx_kind kind, int x, int y, int w, int h)
{
    if (!ui_element || is_preferences_window_open()) return;
    if (!context_menu_right_click_in(x, y, w, h)) return;
    open_menu(ui_element, kind);
}

void context_menu_scene_window       (system_element_context* ui_element, int x, int y, int w, int h) { window_hook(ui_element, ctx_scene,     x, y, w, h); }
void context_menu_hierarchy_window   (system_element_context* ui_element, int x, int y, int w, int h) { window_hook(ui_element, ctx_hierarchy, x, y, w, h); }
void context_menu_files_window       (system_element_context* ui_element, int x, int y, int w, int h) { window_hook(ui_element, ctx_files,     x, y, w, h); }
void context_menu_textureshop_window (system_element_context* ui_element, int x, int y, int w, int h) { window_hook(ui_element, ctx_texshop,   x, y, w, h); }

/* ------------------------------------------------------------------------- */
/* draw + interaction                                                         */
/* ------------------------------------------------------------------------- */
/* picking a mode does not close the radial: the menu stays where it is and redraws as the new mode's menu */
static bool action_keeps_menu_open(menu_action a)
{
    switch (a) {                                    /* mode, transform and component picks keep the radial open: mode, transform, then your tool */
    case action_set_object: case action_set_edit: case action_set_sculpt: case action_set_paint: case action_set_textureshop:
    case action_tool_select: case action_tool_translate: case action_tool_rotate: case action_tool_scale:
    case action_edit_sel_vert: case action_edit_sel_edge: case action_edit_sel_face:
        return true;
    default:
        return false;
    }
}

/* tells the statusbar what a click just did. a mode jump shows the mode: the menubar's dynamic zone follows the same mode. */
static void report_item_status(const menu_item* it)
{
    system_mode_context mode = mode_context_object;
    switch (it->action) {
    case action_set_object:      mode = mode_context_object;      break;
    case action_set_edit:        mode = mode_context_edit;        break;
    case action_set_sculpt:      mode = mode_context_sculpt;      break;
    case action_set_paint:       mode = mode_context_paint;       break;
    case action_set_textureshop: mode = mode_context_textureshop; break;
    default:
        snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Context menu: %s.", it->label);
        return;
    }
    snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Mode: %s.", mode_context_name(mode));
}

/* lit: the mode you are in, the transform tool you have, and the component type in edit mode */
static bool is_active_action(system_element_context* ui_element, menu_action a)
{
    switch (a) {
    case action_set_object:      return g_mode_state.context == mode_context_object;
    case action_set_edit:        return g_mode_state.context == mode_context_edit;
    case action_set_sculpt:      return g_mode_state.context == mode_context_sculpt;
    case action_set_paint:       return g_mode_state.context == mode_context_paint;
    case action_set_textureshop: return g_mode_state.context == mode_context_textureshop;
    case action_tool_select:     return ui_element->active_tool == tool_select;
    case action_tool_translate:  return ui_element->active_tool == tool_translate;
    case action_tool_rotate:     return ui_element->active_tool == tool_rotate;
    case action_tool_scale:      return ui_element->active_tool == tool_scale;
    case action_edit_sel_vert:   return g_mode_state.edit.select_type == select_vert;
    case action_edit_sel_edge:   return g_mode_state.edit.select_type == select_edge;
    case action_edit_sel_face:   return g_mode_state.edit.select_type == select_face;
    default:                     return false;
    }
}

static void run_item(system_element_context* ui_element, const menu_item* it)
{
    if (it->action == action_none) {
        phoenix_info("ContextMenu", "'%s' is not wired up yet", it->label);
        snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "%s: not implemented yet.", it->label);
        return;
    }
    run_action(ui_element, it->action);
    report_item_status(it);
}

static void draw_context_menu_impl(VkCommandBuffer cmd, system_element_context* ui_element, int screen_w, int screen_h)
{
    if (!ui_element) return;

    /* once per UI frame, whoever calls it first (the dock tree does, and so may the renderer). a second call in the
       same frame would eat the click edges. g_ui_frame_index ticks in render_element_commands(). */
    if (s_last_frame == g_ui_frame_index) return;
    s_last_frame = g_ui_frame_index;

    static bool s_logged_first = false;
    if (!s_logged_first) { phoenix_info("ContextMenu", "draw_context_menu is running"); s_logged_first = true; }

    /* per-frame edges. this runs every frame, open or not, so "previous" is always last frame's value */
    const bool left_now   = s_begun ? s_real_left : ui_element->input.mouse_down;
    const bool left_edge  = left_now && !s_prev_left;
    s_prev_left = left_now;

    const bool esc_now    = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    const bool esc_edge   = esc_now && !s_prev_esc;
    s_prev_esc = esc_now;

    const bool right_now  = s_right_event;      /* the windows' hooks already looked at it this frame */
    s_right_event = false;

    const bool opened_now = s_opened_this_frame;
    s_opened_this_frame = false;

    /* closed: release the lock once the left button is up (until then it keeps the click away from the ui_element) */
    if (!s_open) {
        if (s_closing) {
            if (!left_now) { s_closing = false; if (ui_element->active_id == k_lock_id) ui_element->active_id = 0; }
            else if (ui_element->active_id == 0) ui_element->active_id = k_lock_id;
        }
        return;
    }

    if (is_preferences_window_open()) { close_menu(); return; }
    ui_element->active_id = k_lock_id;                                       /* take the mouse away from the widgets */

    if (esc_edge)                             { close_menu(); return; }
    if (right_now && !opened_now)             { close_menu(); return; }   /* right click somewhere with no menu */

    const bool pressed = left_edge && !opened_now;

    /* keep the radial fully on screen */
    if (s_cx < clamp_x) s_cx = clamp_x;
    if (s_cx > screen_w - clamp_x) s_cx = screen_w - clamp_x;
    if (s_cy < clamp_y) s_cy = clamp_y;
    if (s_cy > screen_h - clamp_y) s_cy = screen_h - clamp_y;

    const menu_def* def = &k_hier_menu;
    if (s_kind == ctx_files)   def = &k_files_menu;
    if (s_kind == ctx_texshop) def = &k_scene_textureshop;
    if (s_kind == ctx_scene) {
        switch (g_mode_state.context) {
            case mode_context_edit:        def = &k_scene_edit;        break;
            case mode_context_sculpt:      def = &k_scene_sculpt;      break;
            case mode_context_paint:       def = &k_scene_paint;       break;
            case mode_context_textureshop: def = &k_scene_textureshop; break;
            default:                       def = &k_scene_object;      break;
        }
    }
    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    bool inside = false;

    const float ddx = (float)(mx - s_cx), ddy = (float)(my - s_cy);

    /* ---- center circle button: closes the menu ---- */
    {
        const bool hov = (ddx * ddx + ddy * ddy <= (float)(center_radius * center_radius));
        system_color fill = system_theme.context_menu_center;
        if (hov) fill = process_theme_color(fill, system_theme.context_menu_hover_delta);
        /* both are one GPU quad each, the edge is anti aliased in the fragment shader. the fill stops at the middle of the ring so no fringe shows */
        draw_circle_element(cmd, (float)s_cx, (float)s_cy, (float)center_radius - 0.5f * (float)center_ring, fill);
        draw_circle_border (cmd, (float)s_cx, (float)s_cy, (float)center_radius, (float)center_ring, system_theme.context_menu_center_ring);

        /* the whole circle is the close button, so the X is only a label: plain text in the menu text color, no box. the mode name above it changes with the mode */
        const int tw = (int)strlen(def->title) * ui_font_w;
        const int title_y = s_cy - 12;
        const int x_y     = s_cy + 3 + (12 - ui_font_h) / 2;   /* a little below the center so the X sits lower in the circle */

        draw_text_element(cmd, ui_element, def->title, s_cx - tw / 2, title_y, system_theme.context_menu_text);
        draw_text_element(cmd, ui_element, "X", s_cx - ui_font_w / 2, x_y, system_theme.context_menu_text);

        if (hov && pressed) { close_menu(); return; }
    }

    /* ---- slots ---- */
    int drop_x = 0, drop_y = 0, drop_slot_w = slot_w;
    const bool drop_block = s_drop_valid && in_rect(mx, my, s_drop_x, s_drop_y, s_drop_w, s_drop_h);   /* the dropdown is over the slots: they ignore the mouse */
    for (int i = 0; i < menu_slot_count; i++) {
        const menu_slot* s = &def->slots[i];
        if (!s->label) continue;

        const slot_rect rc = get_slot_rect(def, i);
        const int bx = s_cx + rc.cx - rc.w / 2;
        const int by = s_cy + rc.cy - slot_h / 2;
        const bool hov = !drop_block && in_rect(mx, my, bx, by, rc.w, slot_h);
        if (in_rect(mx, my, bx - btn_gap / 2, by - btn_gap / 2, rc.w + btn_gap, slot_h + btn_gap)) inside = true;

        if (hov && pressed) {
            if (s->face_type == 1) {
                run_item(ui_element, &s->items[0]);
                if (action_keeps_menu_open(s->items[0].action)) s_slot_open = -1;   /* mode / transform / component: stay open */
                else { close_menu(); return; }
            }
            else s_slot_open = (s_slot_open == i) ? -1 : i;
        }

        const bool lit = (s_slot_open == i) || (s->face_type == 1 && is_active_action(ui_element, s->items[0].action));
        const bool dead = (s->face_type == 1 && s->items[0].action == action_none);
        system_color fill = lit ? system_theme.context_menu_button_open : system_theme.context_menu_button;
        if (hov && !lit) fill = process_theme_color(fill, system_theme.context_menu_hover_delta);

        push_rounded_quad(cmd, (float)(bx - 1), (float)(by - 1), (float)(rc.w + 2), (float)(slot_h + 2), system_theme.context_menu_border, button_radius + 1, corner_all);
        push_rounded_quad(cmd, (float)bx, (float)by, (float)rc.w, (float)slot_h, fill, button_radius, corner_all);

        const int tw = (int)strlen(s->label) * ui_font_w;
        draw_text_element(cmd, ui_element, s->label, bx + (rc.w - tw) / 2 - (s->face_type > 1 ? 4 : 0), by + (slot_h - ui_font_h) / 2,
                          dead ? get_theme_disabled_color() : system_theme.context_menu_text);
        if (s->face_type > 1)
            draw_text_element(cmd, ui_element, ">", bx + rc.w - 14, by + (slot_h - ui_font_h) / 2, system_theme.context_menu_arrow);

        if (s_slot_open == i) { drop_x = bx; drop_y = by; drop_slot_w = rc.w; }
    }

    /* ---- paint: the quick colors as small buttons on the header row ---- */
    if (def->swatches) {
        const int total = 10 * swatch_size + 9 * swatch_gap;
        const int sy    = s_cy - 3 * row_pitch - swatch_size / 2;
        for (int i = 0; i < 10; i++) {
            const int sx = s_cx - total / 2 + i * (swatch_size + swatch_gap);
            const bool hov = !drop_block && in_rect(mx, my, sx, sy, swatch_size, swatch_size);
            if (in_rect(mx, my, sx - swatch_gap / 2, sy - swatch_gap / 2, swatch_size + swatch_gap, swatch_size + swatch_gap)) inside = true;

            if (hov && pressed) {
                run_action(ui_element, (menu_action)(action_paint_color_0 + i));
                snprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), "Context menu: color %d.", i + 1);
            }

            system_color c = system_theme.context_menu_text;
            c.red = k_quick_colors[i][0]; c.green = k_quick_colors[i][1]; c.blue = k_quick_colors[i][2]; c.alpha = 1.0f;
            push_quad_element(cmd, (float)(sx - 1), (float)(sy - 1), (float)(swatch_size + 2), (float)(swatch_size + 2),
                              hov ? system_theme.context_menu_text : system_theme.context_menu_border);
            push_quad_element(cmd, (float)sx, (float)sy, (float)swatch_size, (float)swatch_size, c);
        }
    }

    /* ---- dropdown of the open slot ---- */
    if (s_slot_open >= 0) {
        const menu_slot* s = &def->slots[s_slot_open];

        int dw = drop_min_w;
        for (int j = 0; j < s->face_type; j++) {
            const int w = (int)strlen(s->items[j].label) * ui_font_w + 24;
            if (w > dw) dw = w;
        }
        const int dh = drop_pad * 2 + s->face_type * menu_item_h;

        int dx, dy;
        if (s_slot_open <= slot_right_5) {                      /* side columns: open outward, inward when outward is off screen */
            const bool go_right = (s_slot_open >= slot_right_0);
            dy = drop_y;
            dx = go_right ? (drop_x + drop_slot_w + 2) : (drop_x - dw - 2);
            if (dx < 0 || dx + dw > screen_w)
                dx = go_right ? (drop_x - dw - 2) : (drop_x + drop_slot_w + 2);
        } else {                                                /* header opens up, bottom row opens down, the other way when there is no room */
            const bool go_up = (s_slot_open <= slot_head_2);
            dx = drop_x + (drop_slot_w - dw) / 2;
            dy = go_up ? (drop_y - dh - 2) : (drop_y + slot_h + 2);
            if (dy < 0 || dy + dh > screen_h)
                dy = go_up ? (drop_y + slot_h + 2) : (drop_y - dh - 2);
        }
        if (dx + dw > screen_w) dx = screen_w - dw - 2;
        if (dx < 0)             dx = 0;
        if (dy + dh > screen_h) dy = screen_h - dh - 2;
        if (dy < 0)             dy = 0;

        if (in_rect(mx, my, dx, dy, dw, dh)) inside = true;
        s_drop_valid = true; s_drop_x = dx; s_drop_y = dy; s_drop_w = dw; s_drop_h = dh;

        push_rounded_quad(cmd, (float)(dx - 1), (float)(dy - 1), (float)(dw + 2), (float)(dh + 2), system_theme.context_menu_border, menu_radius, corner_all);
        push_rounded_quad(cmd, (float)dx, (float)dy, (float)dw, (float)dh, system_theme.context_menu_dropdown, menu_radius, corner_all);

        for (int j = 0; j < s->face_type; j++) {
            const menu_item* it = &s->items[j];
            const int ry = dy + drop_pad + j * menu_item_h;
            const bool hov = in_rect(mx, my, dx + 3, ry, dw - 6, menu_item_h);

            if (hov)
                push_rounded_quad(cmd, (float)(dx + 3), (float)ry, (float)(dw - 6), (float)menu_item_h, system_theme.context_menu_item_hover, button_radius, corner_all);

            const system_color text = (it->action != action_none) ? system_theme.context_menu_item_text : get_theme_disabled_color();
            draw_text_element(cmd, ui_element, it->label, dx + 12, ry + (menu_item_h - ui_font_h) / 2, text);

            if (hov && pressed) {
                run_item(ui_element, it);
                if (action_keeps_menu_open(it->action)) { s_slot_open = -1; break; }
                close_menu();
                return;
            }
        }
    }

    if (s_slot_open < 0) s_drop_valid = false;

    /* ---- a click on nothing closes it ---- */
    if (pressed && !inside) close_menu();
}

void draw_context_menu(VkCommandBuffer cmd, system_element_context* ui_element, int screen_w, int screen_h)
{
    draw_context_menu_impl(cmd, ui_element, screen_w, screen_h);
    if (ui_element && s_begun) { ui_element->input.mouse_down = s_real_left; s_begun = false; }   /* give the real button back */
}