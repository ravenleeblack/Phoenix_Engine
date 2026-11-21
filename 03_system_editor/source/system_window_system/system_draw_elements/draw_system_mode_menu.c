#include "system_globals.h"


/* mode menu: the 36px strip window. rect is exact: x = node->x, width = node->w, y = content_y, height = content_h.

   G T R S          tools
   Ob Ed            object (default) / edit mode -> Modeler window  (exactly ONE mode is on)
   Sc               sculpt flag mode (system_sculpt_mode.c) -> Brushes tab next to Hierarchy, Sculpt tab next to Lighting
   Pt               paint mode   -> Paint window (Modeler + Sculpt closed)
   Tx               Texture Shop mode -> Texture Shop + UV Editor + Nodes (Modeler / Sculpt / Paint closed), right click = Skin menu
   Wb Bd Cd         World Builder + World Builder Tools / Board Builder / Card Builder
   An               animation (timeline under the Scene view) */

enum { id_spawn_board = 887001u, id_spawn_card = 887002u };

typedef struct { const char* label; uint32_t id; int group; } mode_menu_entry;

static const mode_menu_entry k_entries[] = {
    { "G",  id_tool_grab,       0 }, { "T",  id_tool_translate, 0 }, { "R",  id_tool_rotate, 0 }, { "S",  id_tool_scale, 0 },
    { "Ob", id_mode_object,     1 }, { "Ed", id_mode_mesh,      1 },
    { "Sc", id_work_sculpt,     2 },
    { "Pt", id_work_paint,      3 },
    { "Tx", id_spawn_texshop,   4 },  { "Bd", id_spawn_board,  4 }, { "Cd", id_spawn_card, 4 },
    { "An", id_work_anim,       5 },
};

static bool entry_selected(const system_element_context* ui_element, uint32_t id)
{
    switch (id) {
    case id_tool_grab:       return ui_element->active_tool == tool_select;
    case id_tool_translate:  return ui_element->active_tool == tool_translate;
    case id_tool_rotate:     return ui_element->active_tool == tool_rotate;
    case id_tool_scale:      return ui_element->active_tool == tool_scale;

    case id_mode_object:     return mode_state_is_object();
    case id_mode_mesh:       return mode_state_is_edit();
    case id_work_sculpt:     return mode_state_is_sculpt();
    case id_work_paint:      return mode_state_is_paint();
    case id_work_anim:       return g_mode_state.context == mode_context_animation;

    case id_spawn_texshop:   return mode_state_is_textureshop();
    case id_spawn_board:     return dock_find_leaf_with_view(dock_workspace.root, view_type_board_builder)  != NULL;
    case id_spawn_card:      return dock_find_leaf_with_view(dock_workspace.root, view_type_card_builder)   != NULL;
    default:                 return false;
    }
}

/* ---- window openers ---- */
void mode_menu_open_sculpt_windows(system_element_context* ui_element)
{
    (void)ui_element;
    /* Brushes tab next to the Hierarchy (scene archive), Sculpt tab where the Paint window opens (Inspector, Hierarchy as the fallback anchor) */
    if (!dock_find_leaf_with_view(dock_workspace.root, view_type_brush))
        spawn_dock_view_beside(&dock_workspace, view_type_hierarchy, view_type_inspector, view_type_brush);
    if (!dock_find_leaf_with_view(dock_workspace.root, view_type_sculpt))
        spawn_dock_view_beside(&dock_workspace, view_type_inspector, view_type_hierarchy, view_type_sculpt);
}

void mode_menu_open_board_builder(system_element_context* ui_element)
{
    (void)ui_element;
    spawn_dock_view_beside(&dock_workspace, view_type_worldbld_tools, view_type_inspector, view_type_board_builder);
}

void mode_menu_open_card_builder(system_element_context* ui_element)
{
    (void)ui_element;
    spawn_dock_view_beside(&dock_workspace, view_type_worldbld_tools, view_type_inspector, view_type_card_builder);
}

/* ---- window layout: the Modeler / Sculpt / Paint / Texture Shop area holds ONE mode's windows at a time ----
     Object, Edit : Modeler
     Sculpt       : Sculpt + Brushes   (Modeler and Paint closed)
     Paint        : Paint              (Modeler, Sculpt and Brushes closed)
     Texture Shop : Texture Shop + UV Editor + Nodes (Modeler, Sculpt, Brushes and Paint closed)
   ASSUMES: dock_close_view_type(dock, view) exists (close every leaf of that view). rename to your close call.
   ASSUMES: view_type_paint exists. if Paint still lives in the Modeler, replace it with view_type_modeler below. */
static bool view_is_open(enum_view_type vert_to_edge_array) { return dock_find_leaf_with_view(dock_workspace.root, vert_to_edge_array) != NULL; }

static void close_view(enum_view_type vert_to_edge_array)
{
    if (view_is_open(vert_to_edge_array)) dock_close_view_type(&dock_workspace, vert_to_edge_array);
}

/* the mode's own window: opened if missing, and always brought in front of the other tabs in its dock leaf */
static void show_view_front(enum_view_type beside, enum_view_type fallback, enum_view_type view)
{
    const bool was_background = dock_spawn_background;
    dock_spawn_background = false;
    spawn_dock_view_beside(&dock_workspace, beside, fallback, view);
    dock_spawn_background = was_background;
}

/* prev_ctx = the context whose windows are open right now (-1 = none laid out yet) */
static void mode_windows_apply(system_element_context* ui_element, system_mode_context ctx, int prev_ctx)
{
    /* leaving Texture Shop mode: switch the pair off, start_dock_system removes all three tabs */
    if (prev_ctx == (int)mode_context_textureshop && ctx != mode_context_textureshop) ui_element->spawn_textureshop_window = false;

    dock_spawn_background = true;      /* the mode's windows come up as tabs, the tab you are on (Scene, Game...) stays in front */
    switch (ctx) {
    case mode_context_object:
    case mode_context_edit:
        close_view(view_type_sculpt);
        close_view(view_type_brush);
        close_view(view_type_paint);
        show_view_front(view_type_inspector, view_type_hierarchy, view_type_modeler);
        break;
    case mode_context_sculpt:
        close_view(view_type_modeler);
        close_view(view_type_paint);
        show_view_front(view_type_inspector, view_type_hierarchy, view_type_sculpt);     /* same place the Paint window opens */
        show_view_front(view_type_hierarchy, view_type_inspector, view_type_brush);     /* the Brushes window, in front, like Paint */
        break;
    case mode_context_paint:
        close_view(view_type_modeler);
        close_view(view_type_sculpt);
        show_view_front(view_type_inspector, view_type_hierarchy, view_type_paint);
        show_view_front(view_type_hierarchy, view_type_inspector, view_type_brush);     /* the Brushes window stays open when going Sculpt <-> Paint */
        break;
    case mode_context_textureshop:
        close_view(view_type_modeler);
        close_view(view_type_sculpt);
        close_view(view_type_brush);
        close_view(view_type_paint);
        open_texture_shop_windows(&dock_workspace, ui_element);     /* Texture Shop + UV Editor + Nodes */
        break;
    default:            /* uv / model / animation: leave the windows alone */
        break;
    }
    dock_spawn_background = false;
}

/* flags + surface follow the one active mode. no windows. */
static void mode_flags_apply(system_element_context* ui_element, system_mode_context ctx)
{
    object_mode_set_active(ctx == mode_context_object);
    sculpt_mode_set_active(ctx == mode_context_sculpt);
    paint_mode_set_active (ctx == mode_context_paint);

    if      (ctx == mode_context_sculpt) ui_element->active_surface_mode = surface_sculpt;
    else if (ctx == mode_context_paint)  ui_element->active_surface_mode = surface_paint;
    else if (ctx == mode_context_object || ctx == mode_context_edit || ctx == mode_context_textureshop)
        ui_element->active_surface_mode = surface_none;     /* ASSUMES surface_none = your default. a stale surface_sculpt blocks edit / object input */
}

static int s_applied_ctx = -1;      /* last context whose windows were laid out */

/* ---- the one place a mode gets switched on ---- */
void mode_menu_enter_mode(system_element_context* ui_element, system_mode_context ctx, bool open_windows)
{
    if (!ui_element) return;

    mode_state_set_exclusive(ui_element, ctx);
    ctx = g_mode_state.context;              /* set_exclusive may have folded model -> object */
    mode_flags_apply(ui_element, ctx);

    if (open_windows) {
        const int prev = s_applied_ctx;
        s_applied_ctx = (int)ctx;
        mode_windows_apply(ui_element, ctx, prev);
    }
}

/* call ONCE per frame, right after mode_state_sync_from_ui(ui_element).
   whatever changed the mode (mode menu, right click menu, hotkey, old code) the flags and windows follow g_mode_state. */
void mode_menu_follow_state(system_element_context* ui_element)
{
    if (!ui_element) return;
    const system_mode_context ctx = g_mode_state.context;
    if ((int)ctx == s_applied_ctx) return;
    const int prev = s_applied_ctx;
    s_applied_ctx = (int)ctx;
    mode_flags_apply(ui_element, ctx);
    mode_windows_apply(ui_element, ctx, prev);
}

/* public: the right click radial menu calls this too. */
void mode_menu_activate(system_element_context* ui_element, uint32_t id)
{
    manage_docking* dock = &dock_workspace;

    switch (id) {
    case id_tool_grab:       ui_element->active_tool = tool_select;    break;
    case id_tool_translate:  ui_element->active_tool = tool_translate; break;
    case id_tool_rotate:     ui_element->active_tool = tool_rotate;    break;
    case id_tool_scale:      ui_element->active_tool = tool_scale;     break;

    case id_mode_object:     mode_menu_enter_mode(ui_element, mode_context_object, true); break;
    case id_mode_mesh:       mode_menu_enter_mode(ui_element, mode_context_edit,   true); break;
    case id_work_sculpt:     mode_menu_enter_mode(ui_element, mode_context_sculpt, true); break;
    case id_work_paint:      mode_menu_enter_mode(ui_element, mode_context_paint,  true); break;

    case id_work_anim:
        mode_state_set_exclusive(ui_element, mode_context_animation);
        spawn_dock_view_beside(dock, view_type_scene, view_type_game, view_type_scene);
        break;

    case id_spawn_texshop:   mode_menu_enter_mode(ui_element, mode_context_textureshop, true); break;   /* Texture Shop mode + its three windows */
    case id_spawn_board:     mode_menu_open_board_builder(ui_element);     break;
    case id_spawn_card:      mode_menu_open_card_builder(ui_element);      break;

    /* old ids other code may still send */
    case id_work_uv:         mode_menu_enter_mode(ui_element, mode_context_textureshop, true); break;
    case id_work_model:      mode_menu_enter_mode(ui_element, mode_context_object, false);
                             spawn_dock_view_beside(dock, view_type_inspector, view_type_hierarchy, view_type_modeler); break;
    default: break;
    }
}

void draw_mode_menu_window(system_element_context* ui_element, system_dock_node* node, int content_y, int content_h, VkCommandBuffer cmd)
{
    if (!ui_element || !node) return;

    const int bx    = node->x + (node->w - strip_button_w) / 2;
    const int limit = content_y + content_h;
    int y = content_y + strip_button_gap;
    int prev_group = -1;

    for (int i = 0; i < (int)(sizeof(k_entries) / sizeof(k_entries[0])); i++) {
        const mode_menu_entry* e = &k_entries[i];

        if (prev_group != -1 && e->group != prev_group) {
            push_quad_element(cmd, (float)bx, (float)(y + 1), (float)strip_button_w, 1.0f, system_theme.menu_separator);
            y += strip_group_gap;
        }
        prev_group = e->group;

        if (y + strip_button_h > limit) break;

        const system_color body = entry_selected(ui_element, e->id) ? system_theme.strip_button_selected : system_theme.strip_button;

        const bool is_sculpt_entry = (e->id == id_work_sculpt);
        const bool is_paint_entry  = (e->id == id_work_paint);
        const bool is_texture_entry = (e->id == id_spawn_texshop);
        const bool has_icon        = is_sculpt_entry || is_paint_entry || is_texture_entry;

        if (ui_element->input.mouse_x >= bx && ui_element->input.mouse_x < bx + strip_button_w &&
            ui_element->input.mouse_y >= y && ui_element->input.mouse_y < y + strip_button_h) {
            const char* tip = "";
            switch (e->id) {
            case id_tool_grab:       tip = "Grab: select and drag objects.";                  break;
            case id_tool_translate:  tip = "Move: move the selected object.";                 break;
            case id_tool_rotate:     tip = "Rotate: rotate the selected object.";             break;
            case id_tool_scale:      tip = "Scale: scale the selected object.";               break;
            case id_mode_object:     tip = "Object mode: select and move whole objects.";     break;
            case id_mode_mesh:       tip = "Edit mode: work on vertices, edges and faces.";   break;
            case id_work_sculpt:     tip = "Sculpt mode: shape the mesh with brushes.";       break;
            case id_work_paint:      tip = "Paint mode: paint colors onto the mesh.";         break;
            case id_spawn_texshop:   tip = "Texture Shop: make skins and UV maps.";           break;
            case id_spawn_board:     tip = "Board Builder: build boards from tiles.";         break;
            case id_spawn_card:      tip = "Card Builder: build cards.";                      break;
            case id_work_anim:       tip = "Animation: open the timeline.";                   break;
            default: break;
            }
            set_tooltip_text(tip);
        }

        if (draw_button_element(cmd, ui_element, e->id, has_icon ? "" : e->label, bx, y, strip_button_w, strip_button_h, body))
            mode_menu_activate(ui_element, e->id);

        /* Sculpt, Paint and Texture Shop: a lit sphere like a skin, with the letter S, P or T over it */
        if (has_icon) {
            const float icon_cx = (float)bx + (float)strip_button_w * 0.5f;
            const float icon_cy = (float)y + (float)strip_button_h * 0.5f;
            const float icon_r  = (float)strip_button_h * 0.5f - 2.0f;
            const char* letter = "T";
            float sphere_rgb[3] = { 0.35f, 0.60f, 0.85f };
            if (is_sculpt_entry) {
                letter = "S";
                sphere_rgb[0] = 0.72f; sphere_rgb[1] = 0.62f; sphere_rgb[2] = 0.52f;
            } else if (is_paint_entry) {
                letter = "P";
                sphere_rgb[0] = 0.90f; sphere_rgb[1] = 0.35f; sphere_rgb[2] = 0.30f;
            }
            draw_icon_sphere(cmd, icon_cx, icon_cy, icon_r, sphere_rgb);
            const int letter_x = (int)(icon_cx + 0.5f) - ui_font_w / 2;
            const int letter_y = (int)(icon_cy + 0.5f) - ui_font_h / 2;
            const system_color letter_shadow = { 0.0f, 0.0f, 0.0f, 0.75f };
            const system_color letter_color  = { 1.0f, 1.0f, 1.0f, 1.0f };
            draw_text_element(cmd, ui_element, letter, letter_x + 1, letter_y + 1, letter_shadow);
            draw_text_element(cmd, ui_element, letter, letter_x, letter_y, letter_color);
        }

        y += strip_button_h + strip_button_gap;
    }
}