#include "system_globals.h"


/* ========================================================================= */
/* tree                                                                      */
/* ========================================================================= */
system_dock_node* create_pure_leaf_node(void)
{
    system_dock_node* n = (system_dock_node*)calloc(1, sizeof(system_dock_node));
    if (n) { n->node_type = dock_node_leaf; n->split_ratio = 0.5f; }
    return n;
}

/* leaf (views a,b) or split (children l,r) */
static system_dock_node* new_node(enum_dock_node_type type, float ratio, system_dock_node* l, system_dock_node* r,
                                  enum_view_type a, enum_view_type b)
{
    system_dock_node* n = create_pure_leaf_node();
    if (!n) return NULL;
    n->node_type = type;  n->split_ratio = ratio;
    n->left_child = l;    n->right_child = r;
    if (l) l->parent = n;
    if (r) r->parent = n;
    if (a != view_type_none) n->active_views[n->view_count++] = a;
    if (b != view_type_none) n->active_views[n->view_count++] = b;
    return n;
}

/* true while the mode menu opens a mode's windows: new tabs are added but the tab you are looking at stays in front */
bool dock_spawn_background = false;

static bool leaf_add(system_dock_node* leaf, enum_view_type vert_to_edge_array)
{
    if (!leaf || leaf->node_type != dock_node_leaf || vert_to_edge_array == view_type_none) return false;
    for (int i = 0; i < leaf->view_count; i++) if (leaf->active_views[i] == vert_to_edge_array) { if (!dock_spawn_background) leaf->active_view_index = i; return true; }
    if (leaf->view_count >= 8) return false;
    if (!dock_spawn_background || leaf->view_count == 0) leaf->active_view_index = leaf->view_count;
    leaf->active_views[leaf->view_count++] = vert_to_edge_array;
    return true;
}

void dock_leaf_remove_view(system_dock_node* leaf, int index)
{
    if (!leaf || index < 0 || index >= leaf->view_count) return;
    for (int k = index; k < leaf->view_count - 1; k++) leaf->active_views[k] = leaf->active_views[k + 1];
    leaf->view_count--;
    if (index < leaf->active_view_index) leaf->active_view_index--;
    if (leaf->active_view_index >= leaf->view_count) leaf->active_view_index = leaf->view_count > 0 ? leaf->view_count - 1 : 0;
}

static bool find_view(system_dock_node* n, enum_view_type vert_to_edge_array, system_dock_node** leaf, int* idx)
{
    if (!n) return false;
    if (n->node_type == dock_node_leaf) {
        for (int i = 0; i < n->view_count; i++) if (n->active_views[i] == vert_to_edge_array) { *leaf = n; *idx = i; return true; }
        return false;
    }
    return find_view(n->left_child, vert_to_edge_array, leaf, idx) || find_view(n->right_child, vert_to_edge_array, leaf, idx);
}

/* a split with an empty leaf child becomes its other child. post-order: works at any depth. */
static void collapse(system_dock_node* n)
{
    if (!n || n->node_type == dock_node_leaf) return;
    collapse(n->left_child);
    collapse(n->right_child);

    system_dock_node *l = n->left_child, *r = n->right_child;
    if (!l || !r) return;
    const bool le = (l->node_type == dock_node_leaf && l->view_count == 0);
    const bool re = (r->node_type == dock_node_leaf && r->view_count == 0);
    if (!le && !re) return;

    system_dock_node* keep  = le ? r : l;
    system_dock_node* empty = le ? l : r;
    n->node_type = keep->node_type;   n->split_ratio = keep->split_ratio;
    n->view_count = keep->view_count; n->active_view_index = keep->active_view_index;
    memcpy(n->active_views, keep->active_views, sizeof(n->active_views));
    n->left_child = keep->left_child; n->right_child = keep->right_child;
    if (n->left_child)  n->left_child->parent  = n;
    if (n->right_child) n->right_child->parent = n;
    free(keep);
    free(empty);
}

static void free_tree(system_dock_node* n)
{
    if (!n) return;
    free_tree(n->left_child);
    free_tree(n->right_child);
    free(n);
}

/* ========================================================================= */
/* layout: the ONLY place rectangles are decided. draw code never changes them. */
/* ========================================================================= */
void compute_dock_layout(system_dock_node* n, int x, int y, int w, int h, system_dock_node* parent)
{
    if (!n) return;
    n->x = x; n->y = y; n->w = w; n->h = h; n->parent = parent;
    if (n->node_type == dock_node_leaf) return;

    system_dock_node *l = n->left_child, *r = n->right_child;
    if (!l || !r) { compute_dock_layout(l ? l : r, x, y, w, h, n); return; }

    const bool horiz = (n->node_type == dock_node_split_horizontal);
    const int total = horiz ? w : h;
    const int min_each = horiz ? dock_min_w : dock_min_h;

    int first;
    if      (horiz && dock_is_mode_menu(l)) first = mode_menu_w;            /* fixed strip, other side takes the rest */
    else if (horiz && dock_is_mode_menu(r)) first = total - mode_menu_w;
    else {
        first = (int)((float)total * n->split_ratio + 0.5f);
        if (total < 2 * min_each) first = total / 2;
        else if (first < min_each) first = min_each;
        else if (first > total - min_each) first = total - min_each;
    }

    if (horiz) { compute_dock_layout(l, x, y, first, h, n);  compute_dock_layout(r, x + first, y, w - first, h, n); }
    else       { compute_dock_layout(l, x, y, w, first, n);  compute_dock_layout(r, x, y + first, w, h - first, n); }
}

/* ========================================================================= */
/* push a view into the dock (focus it if it already lives somewhere)         */
/* ========================================================================= */
void push_dock_view(manage_docking* mesh, enum_view_type view)
{
    if (!mesh || !mesh->root || view == view_type_none) return;
    system_dock_node* leaf = NULL;
    int idx = 0;
    if (find_view(mesh->root, view, &leaf, &idx)) { if (!dock_spawn_background) leaf->active_view_index = idx; return; }

    leaf = mesh->root;
    while (leaf->node_type != dock_node_leaf) leaf = leaf->right_child ? leaf->right_child : leaf->left_child;
    leaf_add(leaf, view);
}


/* find the leaf that holds a view */
system_dock_node* dock_find_leaf_with_view(system_dock_node* n, enum_view_type vert_to_edge_array)
{
    system_dock_node* leaf = NULL;
    int idx = 0;
    return find_view(n, vert_to_edge_array, &leaf, &idx) ? leaf : NULL;
}

/* close every tab of this view. an emptied leaf is removed by collapse() in evaluate_dock_interaction next frame. */
void dock_close_view_type(manage_docking* mesh, enum_view_type vert_to_edge_array)
{
    if (!mesh || !mesh->root || vert_to_edge_array == view_type_none) return;
    system_dock_node* leaf = NULL;
    int idx = 0;
    while (find_view(mesh->root, vert_to_edge_array, &leaf, &idx)) dock_leaf_remove_view(leaf, idx);
}

/* tab the view into the leaf that holds anchor_a (else anchor_b). already docked = just bring it to the front. */
void spawn_dock_view_beside(manage_docking* mesh, enum_view_type anchor_a, enum_view_type anchor_b, enum_view_type view)
{
    if (!mesh || !mesh->root || view == view_type_none) return;

    system_dock_node* leaf = NULL;
    int idx = 0;
    if (find_view(mesh->root, view, &leaf, &idx)) { if (!dock_spawn_background) leaf->active_view_index = idx; return; }

    system_dock_node* target = dock_find_leaf_with_view(mesh->root, anchor_a);
    if (!target) target = dock_find_leaf_with_view(mesh->root, anchor_b);
    if (target && dock_can_tab_together(view, dock_active_view(target)) && leaf_add(target, view)) return;

    push_dock_view(mesh, view);                                            /* no anchor on screen: any leaf */
}

/* ========================================================================= */
/* interaction: splitter resize + tab drag and drop                          */
/* ========================================================================= */
void evaluate_dock_interaction(manage_docking* mesh, system_element_context* ui_element)
{
    static bool prev_down = false;
    if (!mesh || !ui_element || !mesh->root) return;

    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    const bool pressed = ui_element->input.mouse_down && !prev_down;   /* rising edge_to_face_array */
    prev_down = ui_element->input.mouse_down;

    if (ui_element->input.camera_manipulating) { mesh->is_resizing_splitter = false; mesh->resizing_splitter_node = NULL; return; }

    /* --- resizing a splitter --- */
    if (mesh->is_resizing_splitter) {
        system_dock_node* n = mesh->resizing_splitter_node;
        if (!n || !ui_element->input.mouse_down) { mesh->is_resizing_splitter = false; mesh->resizing_splitter_node = NULL; return; }

        const bool horiz = (n->node_type == dock_node_split_horizontal);
        const int origin = horiz ? n->x : n->y, extent = horiz ? n->w : n->h, min_each = horiz ? dock_min_w : dock_min_h;
        int obj_vert_pos = horiz ? mx : my;
        if (extent > 2 * min_each) {
            if (obj_vert_pos < origin + min_each) obj_vert_pos = origin + min_each;
            if (obj_vert_pos > origin + extent - min_each) obj_vert_pos = origin + extent - min_each;
            n->split_ratio = (float)(obj_vert_pos - origin) / (float)extent;
        }
        return;
    }


    /* --- dragging a tab --- */
    if (mesh->is_dragging) {
        #define IN(n) ((n) && mx >= (n)->x && mx < (n)->x + (n)->w && my >= (n)->y && my < (n)->y + (n)->h)
        system_dock_node* leaf = IN(mesh->root) ? mesh->root : NULL;
        while (leaf && leaf->node_type != dock_node_leaf) {
            system_dock_node* n = leaf;
            leaf = IN(n->left_child) ? n->left_child : (IN(n->right_child) ? n->right_child : NULL);
        }
        #undef IN
        if (dock_is_mode_menu(leaf)) leaf = NULL;                       /* nothing drops into the tool strip */

        mesh->target_hover_node = leaf;
        mesh->active_drop_zone  = drop_zone_none;
        if (leaf) {
            const float px = (float)(mx - leaf->x) / (float)leaf->w, py = (float)(my - leaf->y) / (float)leaf->h;
            mesh->active_drop_zone = (my < leaf->y + tab_bar_h) ? drop_zone_center :
                                  px < 0.15f ? drop_zone_left  : px > 0.85f ? drop_zone_right :
                                  py < 0.15f ? drop_zone_top   : py > 0.85f ? drop_zone_bottom : drop_zone_center;
        }
        if (ui_element->input.mouse_down) return;

        /* released: drop */
        ui_element->active_id = 0;
        bool placed = false;
        const enum_drop_zone_type zone = mesh->active_drop_zone;
        if (leaf && zone == drop_zone_center) {
            placed = leaf_add(leaf, mesh->dragging_view);
        } else if (leaf && zone != drop_zone_none) {
            system_dock_node* old_part = create_pure_leaf_node();
            system_dock_node* new_part = create_pure_leaf_node();
            if (old_part && new_part) {
                memcpy(old_part->active_views, leaf->active_views, sizeof(leaf->active_views));
                old_part->view_count = leaf->view_count;
                old_part->active_view_index = leaf->active_view_index;
                new_part->active_views[0] = mesh->dragging_view;
                new_part->view_count = 1;
                old_part->parent = new_part->parent = leaf;

                const bool new_first = (zone == drop_zone_left || zone == drop_zone_top);
                leaf->node_type   = (zone == drop_zone_left || zone == drop_zone_right) ? dock_node_split_horizontal : dock_node_split_vertical;
                leaf->split_ratio = 0.5f;
                leaf->left_child  = new_first ? new_part : old_part;
                leaf->right_child = new_first ? old_part : new_part;
                leaf->view_count = 0;  leaf->active_view_index = 0;
                placed = true;
            } else { free(old_part); free(new_part); }
        }
        if (!placed) push_dock_view(mesh, mesh->dragging_view);                /* never lose the tab */

        mesh->dragging_view = view_type_none;  mesh->is_dragging = false;
        mesh->target_hover_node = NULL;        mesh->active_drop_zone = drop_zone_none;
        return;
    }

    collapse(mesh->root);                                                   /* panels emptied last frame vanish, any depth */

    /* --- a fresh click on a splitter starts a resize --- */
    if (pressed) {
        system_dock_node* stack[128];
        int sp = 0;
        stack[sp++] = mesh->root;
        while (sp > 0) {
            system_dock_node* n = stack[--sp];
            if (!n || n->node_type == dock_node_leaf || !n->left_child || !n->right_child) continue;

            system_dock_node *l = n->left_child, *r = n->right_child;
            if (!dock_is_mode_menu(l) && !dock_is_mode_menu(r)) {        /* the mode strip has a fixed width */
                const bool horiz = (n->node_type == dock_node_split_horizontal);
                const int line = horiz ? r->x : r->y;
                const bool hit = horiz ? (mx >= line - dock_grab && mx <= line + dock_grab && my >= n->y && my < n->y + n->h)
                                       : (my >= line - dock_grab && my <= line + dock_grab && mx >= n->x && mx < n->x + n->w);
                if (hit) { mesh->resizing_splitter_node = n; mesh->is_resizing_splitter = true; return; }
            }
            if (sp < 126) { stack[sp++] = r; stack[sp++] = l; }
        }
    }
}

/* ========================================================================= */
/* frame                                                                     */
/* ========================================================================= */
void start_dock_system(VkCommandBuffer cmd, manage_docking* mesh, system_element_context* ui_element)
{
    if (!mesh) return;

    if (!mesh->root) {
        mesh->root = new_node(dock_node_split_horizontal, 0.125f,
                      new_node(dock_node_leaf, 0.5f, NULL, NULL, view_type_hierarchy, view_type_none),
                      new_node(dock_node_split_horizontal, 0.821f,
                          new_node(dock_node_leaf, 0.5f, NULL, NULL, view_type_scene, view_type_game),
                          new_node(dock_node_split_horizontal, 0.120f,
                              new_node(dock_node_leaf, 0.5f, NULL, NULL, view_type_mode_menu, view_type_none),
                              new_node(dock_node_leaf, 0.5f, NULL, NULL, view_type_modeler, view_type_inspector),
                              view_type_none, view_type_none),
                          view_type_none, view_type_none),
                      view_type_none, view_type_none);
        mesh->target_hover_node = NULL;  mesh->is_dragging = false;  mesh->dragging_view = view_type_none;
        mesh->active_drop_zone = drop_zone_none;  mesh->is_resizing_splitter = false;  mesh->resizing_splitter_node = NULL;

        /* Lighting tab sits next to Inspector from the start. the leaf keeps showing whatever tab it showed before. */
        system_dock_node* insp = dock_find_leaf_with_view(mesh->root, view_type_inspector);
        if (insp) { const int keep = insp->active_view_index; leaf_add(insp, view_type_light); insp->active_view_index = keep; }

        /* File Archive tab sits next to Hierarchy from the start. Hierarchy stays the tab that is showing. */
        system_dock_node* hier = dock_find_leaf_with_view(mesh->root, view_type_hierarchy);
        if (hier) { const int keep = hier->active_view_index; leaf_add(hier, view_type_file_archive); hier->active_view_index = keep; }
    }

    evaluate_dock_interaction(mesh, ui_element);

    /* Turning Texture Shop / World Builder off (menu button or the tab's x) removes ALL of its tabs.
       Turning them on is done by update_dock_spawn_requests() in system_dock_tree.c, which docks each tab in its own spot. */
    if (!mesh->is_dragging) {
        const enum_view_type opt_view[5] = { view_type_textureshop, view_type_texshop_editor, view_type_texshop_node, view_type_worldbld, view_type_worldbld_tools };
        const bool           opt_want[5] = { ui_element->spawn_textureshop_window, ui_element->spawn_textureshop_window, ui_element->spawn_textureshop_window,
                                             ui_element->spawn_worldbuilder_window, ui_element->spawn_worldbuilder_window };
        for (int i = 0; i < 5; i++) {
            system_dock_node* leaf = NULL;
            int idx = -1;
            if (!opt_want[i] && find_view(mesh->root, opt_view[i], &leaf, &idx))
                dock_leaf_remove_view(leaf, idx);
        }
    }

    compute_dock_layout(mesh->root, 0, dock_top, (int)program_state.width, (int)program_state.height - dock_top - statusbar_h, NULL);
    traverse_dock_tree(ui_element, mesh->root, cmd);
}

/* the cursor for the dock: 0 = normal arrow, 1 = left/right arrow, 2 = up/down arrow.
   a resize arrow shows over a divider that can really be dragged (the mode strip is fixed, so its edges do not count),
   and stays while one is being dragged. */
int get_dock_cursor_kind(const manage_docking* mesh, int mx, int my)
{
    if (!mesh || !mesh->root) return 0;

    if (mesh->is_resizing_splitter && mesh->resizing_splitter_node) {
        if (mesh->resizing_splitter_node->node_type == dock_node_split_horizontal) return 1;
        return 2;
    }
    if (mesh->is_dragging) return 0;

    const system_dock_node* stack[128];
    int sp = 0;
    stack[sp++] = mesh->root;
    while (sp > 0) {
        const system_dock_node* n = stack[--sp];
        if (!n || n->node_type == dock_node_leaf || !n->left_child || !n->right_child) continue;

        const system_dock_node *l = n->left_child, *r = n->right_child;
        if (!dock_is_mode_menu(l) && !dock_is_mode_menu(r)) {
            if (n->node_type == dock_node_split_horizontal) {
                if (mx >= r->x - dock_grab && mx <= r->x + dock_grab && my >= n->y && my < n->y + n->h) return 1;
            } else {
                if (my >= r->y - dock_grab && my <= r->y + dock_grab && mx >= n->x && mx < n->x + n->w) return 2;
            }
        }
        if (sp < 126) { stack[sp++] = r; stack[sp++] = l; }
    }
    return 0;
}

/* Finds the split whose 1px divider line is under the mouse (within DOCK_GRAB pixels).
   Horizontal split = children side by side, divider at right_child->x.
   Vertical split   = children stacked,      divider at right_child->y.
   Writes the split node to *out_node and returns true, or leaves *out_node alone and returns false. */
bool find_intersecting_splitter_line(system_dock_node* node, int mx, int my, system_dock_node** out_node)
{
    if (!node || node->node_type == dock_node_leaf || !out_node) return false;

    /* children first: a nested splitter wins over the one around it */
    if (find_intersecting_splitter_line(node->left_child,  mx, my, out_node)) return true;
    if (find_intersecting_splitter_line(node->right_child, mx, my, out_node)) return true;

    if (!node->left_child || !node->right_child) return false;

    if (node->node_type == dock_node_split_horizontal) {
        if (abs(mx - node->right_child->x) <= dock_grab && my >= node->y && my < node->y + node->h) {
            *out_node = node;
            return true;
        }
    } else {
        if (abs(my - node->right_child->y) <= dock_grab && mx >= node->x && mx < node->x + node->w) {
            *out_node = node;
            return true;
        }
    }
    return false;
}

void shutdown_dock_system(manage_docking* mesh)
{
    if (!mesh || !mesh->root) return;
    free_tree(mesh->root);
    mesh->root = NULL;
}