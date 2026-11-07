#include "system_globals.h"


/* ========================================================================= */
/* dock tree drawing + window spawning.                                       */
/* rectangles come from compute_dock_layout (system_dock.c). nothing here     */
/* changes them. every window gets an EXACT rect:                             */
/*   x = node->x, width = node->w, y = content_y, height = content_h          */
/* ========================================================================= */

static const char* view_title(enum_view_type vert_to_edge_array)
{
    switch (vert_to_edge_array) {
    case view_type_mode_menu:         return "Modes";
    case view_type_hierarchy:         return "Hierarchy";
    case view_type_scene:             return "Scene";
    case view_type_game:              return "Game";
    case view_type_inspector:         return "Inspector";
    case view_type_modeler:           return "Modeler";
    case view_type_textureshop:       return "Texture Shop";
    case view_type_timeline:          return "Timeline";
    case view_type_texshop_editor:    return "Tex Editor";
    case view_type_worldbld_tools:    return "World Tools";
    case view_type_board_builder:     return "Board Builder";
    case view_type_card_builder:      return "Card Builder";
    case view_type_sculpt:            return "Sculpt";
    case view_type_brush:             return "Brushes";
    case view_type_paint:             return "Paint";
    case view_type_light:             return "Lighting";
    case view_type_texshop_node:      return "Tex Nodes";
    case view_type_texshop_skinmaker: return "Skin Maker";
    case view_type_file_archive:      return "Files";
    default:                          return "?";
    }
}

/* the main three windows have no x */
static bool view_closable(enum_view_type vert_to_edge_array)
{
    return vert_to_edge_array != view_type_hierarchy && vert_to_edge_array != view_type_scene && vert_to_edge_array != view_type_game;
}

static bool in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

/* ---- per frame mouse edge_to_face_array, set once by the root call ---- */
static struct {
    bool            prev_down;
    bool            edge_to_face_array;
    enum_view_type  pending;        /* tab the button went down on, waiting to become a drag */
    int             press_x, press_y;
} s_tab = { 0 };

static void draw_window_content(system_element_context* ui_element, system_dock_node* node, enum_view_type view,
                                int content_y, int content_h, VkCommandBuffer cmd)
{
    switch (view) {
    case view_type_mode_menu:         draw_mode_menu_window(ui_element, node, content_y, content_h, cmd);            break;
    case view_type_hierarchy:         draw_scene_archive_window(ui_element, node, content_y, content_h, cmd);        break;
    case view_type_scene:             draw_sceneview_window(ui_element, node, content_y, content_h, cmd);            break;
    case view_type_game:              draw_gameview_window(ui_element, node, content_y, content_h, cmd);             break;
    case view_type_inspector:         draw_inspector_window(ui_element, node, content_y, content_h, cmd);            break;
    case view_type_modeler:           draw_modeler_window(ui_element, node, content_y, content_h, cmd);              break;
    case view_type_textureshop:       draw_textureshop_window(ui_element, node, content_y, content_h, cmd);          break;
    case view_type_texshop_editor:    draw_textureshop_editor_window(ui_element, node, content_y, content_h, cmd);   break;
    case view_type_texshop_skinmaker: draw_textureshop_skinmaker_window(ui_element, node, content_y, content_h, cmd); break;
    case view_type_timeline:          draw_timeline_window(ui_element, node, content_y, content_h, cmd);             break;
    case view_type_board_builder:     draw_board_builder_window(ui_element, node, content_y, content_h, cmd);       break;
    case view_type_card_builder:      draw_card_builder_window(ui_element, node, content_y, content_h, cmd);        break;
    case view_type_sculpt:            draw_sculpt_window(ui_element, node, content_y, content_h, cmd);               break;
    case view_type_brush:             draw_system_brush_window(ui_element, node, content_y, content_h, cmd);         break;
    case view_type_paint:             draw_system_paint_window(ui_element, node, content_y, content_h, cmd);         break;
    case view_type_light:             draw_system_light_window(ui_element, node, content_y, content_h, cmd);         break;
    case view_type_texshop_node:      draw_textureshop_node_window(ui_element, node, content_y, content_h, cmd);    break;
    case view_type_file_archive:      draw_file_archive_window(ui_element, node, content_y, content_h, cmd);         break;
    default: break;
    }
}

/* closing a tab of a paired window switches the pair off: system_dock.c then removes the other tabs too */
static void on_view_closed(system_element_context* ui_element, enum_view_type vert_to_edge_array)
{
    if (vert_to_edge_array == view_type_textureshop || vert_to_edge_array == view_type_texshop_editor || vert_to_edge_array == view_type_texshop_node || vert_to_edge_array == view_type_texshop_skinmaker)
        ui_element->spawn_textureshop_window = false;
    if (vert_to_edge_array == view_type_worldbld    || vert_to_edge_array == view_type_worldbld_tools) ui_element->spawn_worldbuilder_window = false;
}

/* ---- one leaf ---- */
static void draw_leaf(system_element_context* ui_element, system_dock_node* node, VkCommandBuffer cmd)
{
    manage_docking* mesh = &dock_workspace;
    if (node->w <= 0 || node->h <= 0) return;

    /* strip window (mode menu): grip header + content, no tabs */
    if (dock_is_mode_menu(node)) {
        draw_strip_window_body(cmd, ui_element, node->x, node->y, node->w, node->h);
        (void)draw_strip_grip(cmd, ui_element, dock_tab_id + 900u, node->x, node->y, node->w, strip_header_h);
        const int cy = node->y + strip_header_h, ch = node->h - strip_header_h;
        if (ch > 0) draw_mode_menu_window(ui_element, node, cy, ch, cmd);
        return;
    }

    /* tabbed window */
    draw_tab_element(cmd, ui_element, node->x, node->y, node->w, tab_bar_h);

    int  close_index = -1;
    int  tx = node->x;
    bool started_drag = false;

    for (int i = 0; i < node->view_count; i++) {
        const enum_view_type vert_to_edge_array = node->active_views[i];
        const char* label = view_title(vert_to_edge_array);
        const bool closable = view_closable(vert_to_edge_array);
        const int tw = (int)strlen(label) * ui_font_w + tab_text_pad * 2 + (closable ? tab_close_size + tab_close_margin : 0);
        if (tx + tw > node->x + node->w) break;

        /* press on the tab body (not its x) can become a drag */
        const int close_zone = closable ? tab_close_size + tab_close_margin + 2 : 0;
        if (s_tab.edge_to_face_array && in_rect(ui_element->input.mouse_x, ui_element->input.mouse_y, tx, node->y, tw - close_zone, tab_bar_h)) {
            s_tab.pending = vert_to_edge_array;
            s_tab.press_x = ui_element->input.mouse_x;
            s_tab.press_y = ui_element->input.mouse_y;
        }

        const enum_tab_result r = draw_tab_item(cmd, ui_element, dock_tab_id + (uint32_t)vert_to_edge_array, label, tx, node->y, tw, tab_bar_h,
                                                i == node->active_view_index, closable);
        if (r == tab_result_clicked) node->active_view_index = i;
        if (r == tab_result_closed && close_index < 0) close_index = i;

        /* pulled away from where it was pressed: the tab leaves this leaf and follows the mouse */
        if (!mesh->is_dragging && s_tab.pending == vert_to_edge_array && ui_element->input.mouse_down) {
            const int dx = ui_element->input.mouse_x - s_tab.press_x, dy = ui_element->input.mouse_y - s_tab.press_y;
            if (dx * dx + dy * dy >= 36) {
                mesh->dragging_view = vert_to_edge_array;
                mesh->is_dragging   = true;
                s_tab.pending    = view_type_none;
                dock_leaf_remove_view(node, i);
                ui_element->active_id = 0;
                started_drag = true;
                break;
            }
        }
        tx += tw + 2;
    }

    if (started_drag) return;

    if (close_index >= 0) {
        const enum_view_type closed = node->active_views[close_index];
        dock_leaf_remove_view(node, close_index);
        on_view_closed(ui_element, closed);
        return;
    }

    /* the tab sub menubar band (node->y + tab_bar_h, tab_menu_h tall): painted here for EVERY tabbed window, so a window that
       draws no bar of its own can never leave a hole (whatever is behind, like the clear color, would show through).
       windows with buttons still draw their own bar on top of this at the start of their draw_*_window. */
    {
        const int sub_h = (node->h - tab_bar_h < tab_menu_h) ? (node->h - tab_bar_h) : tab_menu_h;
        if (sub_h > 0) draw_tab_sub_menubar(cmd, ui_element, node->x, node->y + tab_bar_h, node->w, sub_h);
    }

    const int cy = node->y + dock_content_offset, ch = node->h - dock_content_offset;
    if (ch <= 0) return;
    draw_tab_content_window(cmd, ui_element, node->x, cy, node->w, ch);

    const enum_view_type active = dock_active_view(node);
    if (active != view_type_none) draw_window_content(ui_element, node, active, cy, ch, cmd);

    /* the transform gizmo (move / rotate / scale) goes on top of the Scene view image. update + draw in one pass. */
    if (active == view_type_scene) system_transform_gizmo_update_and_draw(cmd, ui_element);
}

static void draw_splitters(system_dock_node* n, VkCommandBuffer cmd)
{
    if (!n || n->node_type == dock_node_leaf) return;
    draw_splitters(n->left_child, cmd);
    draw_splitters(n->right_child, cmd);
    if (!n->left_child || !n->right_child) return;

    if (n->node_type == dock_node_split_horizontal) {
        push_quad_element(cmd, (float)n->right_child->x, (float)n->y, 1.0f, (float)n->h, system_theme.dock_splitter);
    } else {
        push_quad_element(cmd, (float)n->x, (float)n->right_child->y, (float)n->w, 1.0f, system_theme.dock_splitter);
    }
}

/* the zone a dragged tab would drop into, and the tab itself following the mouse */
static void draw_drag_overlay(system_element_context* ui_element, VkCommandBuffer cmd)
{
    manage_docking* mesh = &dock_workspace;
    if (!mesh->is_dragging) return;

    const system_dock_node* t = mesh->target_hover_node;
    if (t && mesh->active_drop_zone != drop_zone_none) {
        float x = (float)t->x, y = (float)t->y, w = (float)t->w, h = (float)t->h;
        switch (mesh->active_drop_zone) {
        case drop_zone_left:   w *= 0.5f;                  break;
        case drop_zone_right:  x += w * 0.5f; w *= 0.5f;   break;
        case drop_zone_top:    h *= 0.5f;                  break;
        case drop_zone_bottom: y += h * 0.5f; h *= 0.5f;   break;
        default: break;                                    /* center: the whole leaf */
        }
        push_quad_element(cmd, x, y, w, h, color_with_alpha(system_theme.system_accent, 0.35f));
    }

    const char* label = view_title(mesh->dragging_view);
    const int tw = (int)strlen(label) * ui_font_w + tab_text_pad * 2;
    const int gx = ui_element->input.mouse_x + 8, gy = ui_element->input.mouse_y + 8;
    push_quad_element(cmd, (float)gx, (float)gy, (float)tw, (float)tab_bar_h, color_with_alpha(system_theme.tab_item_active, 0.85f));
    draw_text_element(cmd, ui_element, label, gx + tab_text_pad, gy + (tab_bar_h - ui_font_h) / 2, system_theme.tab_item_text_active);
}

/* ========================================================================= */
/* public: draw the whole tree. called with the root once per frame.          */
/* ========================================================================= */
void traverse_dock_tree(system_element_context* ui_element, system_dock_node* node, VkCommandBuffer cmd)
{
    if (!ui_element || !node) return;
    const bool is_root = (node == dock_workspace.root);

    if (is_root) {
        s_tab.edge_to_face_array      = ui_element->input.mouse_down && !s_tab.prev_down;
        s_tab.prev_down = ui_element->input.mouse_down;
        if (!ui_element->input.mouse_down) s_tab.pending = view_type_none;
        update_dock_spawn_requests(&dock_workspace, ui_element);
        update_light_from_scene();      /* once per UI frame: a directional light object drives the sun */
        mode_state_sync_from_ui(ui_element);    /* once per UI frame, here and not in the Modeler window: that window is closed in Sculpt / Paint */
        mode_menu_follow_state(ui_element);     /* whatever changed the mode, the flags and the Modeler / Sculpt / Paint windows follow it */
        sculpt_input_update(ui_element);        /* once per UI frame: sculpt strokes + mode_state mirror (system_sculpt_input.c) */
        paint_input_update(ui_element);         /* once per UI frame: paint strokes (was never called) */
    }

    if (node->node_type == dock_node_leaf) {
        draw_leaf(ui_element, node, cmd);
    } else {
        traverse_dock_tree(ui_element, node->left_child,  cmd);
        traverse_dock_tree(ui_element, node->right_child, cmd);
    }

    if (is_root) {
        draw_splitters(node, cmd);
        draw_drag_overlay(ui_element, cmd);
        /* the radial menu goes last so nothing covers it. it only runs once per ui_element frame, extra calls are ignored. */
        draw_context_menu(cmd, ui_element, (int)program_state.width, (int)program_state.height);
    }
}

/* Lighting window: tabbed next to Inspector (Modeler as the fallback anchor). Safe to call every click. */
void open_light_window(manage_docking* mesh)
{
    if (!mesh) return;
    spawn_dock_view_beside(mesh, view_type_inspector, view_type_modeler, view_type_light);
}

/* File Archive window: tabbed next to Hierarchy (Inspector as the fallback anchor). Safe to call every click. */
void open_file_archive_window(manage_docking* mesh)
{
    if (!mesh) return;
    spawn_dock_view_beside(mesh, view_type_hierarchy, view_type_inspector, view_type_file_archive);
}

/* ========================================================================= */
/* window pairs                                                               */
/* ========================================================================= */
/* Texture Shop mode = four tabs:
     Texture Shop   next to Inspector     the shader profile window (profiles, assign, export)
     Skin Maker     next to Texture Shop  layer stack, colour, blend, masks of the obj_vert_selected skin
     Tex Editor     next to Scene / Game  the UV editor
     Tex Nodes      next to Tex Editor    the node canvas */
void open_texture_shop_windows(manage_docking* mesh, system_element_context* ui_element)
{
    if (!mesh || !ui_element) return;
    ui_element->spawn_textureshop_window = true;
    spawn_dock_view_beside(mesh, view_type_inspector,      view_type_modeler, view_type_textureshop);
    spawn_dock_view_beside(mesh, view_type_textureshop,    view_type_inspector, view_type_texshop_skinmaker);
    spawn_dock_view_beside(mesh, view_type_scene,          view_type_game,    view_type_texshop_editor);   /* the editor is the UV editor: next to Scene / Game */
    spawn_dock_view_beside(mesh, view_type_texshop_editor, view_type_scene,   view_type_texshop_node);     /* the node window tabs with the UV editor */
}

void open_world_builder_windows(manage_docking* mesh, system_element_context* ui_element)
{
    if (!mesh || !ui_element) return;
    ui_element->spawn_worldbuilder_window = true;
    spawn_dock_view_beside(mesh, view_type_inspector, view_type_modeler,   view_type_worldbld);
    spawn_dock_view_beside(mesh, view_type_hierarchy, view_type_inspector, view_type_worldbld_tools); /* tools next to Hierarchy */
}

/* flags set from anywhere (old toolbar button, saved layout) turn into windows here. only docks what is missing, so tabs
   the player picked are never pulled forward every frame. turning them off is done in start_dock_system. */
void update_dock_spawn_requests(manage_docking* mesh, system_element_context* ui_element)
{
    if (!mesh || !ui_element || !mesh->root || mesh->is_dragging) return;

    if (ui_element->spawn_textureshop_window) {
        if (!dock_find_leaf_with_view(mesh->root, view_type_textureshop))
            spawn_dock_view_beside(mesh, view_type_inspector, view_type_modeler, view_type_textureshop);
        if (!dock_find_leaf_with_view(mesh->root, view_type_texshop_skinmaker))
            spawn_dock_view_beside(mesh, view_type_textureshop, view_type_inspector, view_type_texshop_skinmaker);
        if (!dock_find_leaf_with_view(mesh->root, view_type_texshop_editor))
            spawn_dock_view_beside(mesh, view_type_scene, view_type_game, view_type_texshop_editor);
        if (!dock_find_leaf_with_view(mesh->root, view_type_texshop_node))
            spawn_dock_view_beside(mesh, view_type_texshop_editor, view_type_scene, view_type_texshop_node);
    }
    if (ui_element->spawn_worldbuilder_window) {
        if (!dock_find_leaf_with_view(mesh->root, view_type_worldbld))
            spawn_dock_view_beside(mesh, view_type_inspector, view_type_modeler, view_type_worldbld);
        if (!dock_find_leaf_with_view(mesh->root, view_type_worldbld_tools))
            spawn_dock_view_beside(mesh, view_type_hierarchy, view_type_inspector, view_type_worldbld_tools);
    }
}