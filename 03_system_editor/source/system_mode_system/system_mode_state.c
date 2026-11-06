#include "system_globals.h"



system_mode_state g_mode_state = {0};

static bool s_skip_detect = false;

static system_mode_context context_from_workflow(enum_workflow_mode w)
{
    switch (w) {
        case workflow_mode_uv:        return mode_context_uv;
        case workflow_mode_sculpt:    return mode_context_sculpt;
        case workflow_mode_paint:     return mode_context_paint;
        case workflow_mode_animation: return mode_context_animation;
        default:                      return mode_context_model;
    }
}

const char* mode_context_name(system_mode_context c)
{
    switch (c) {
        case mode_context_object:      return "Object Mode";
        case mode_context_edit:        return "Edit Mode";
        case mode_context_uv:          return "UV Mode";
        case mode_context_model:       return "Model Mode";
        case mode_context_sculpt:      return "Sculpt Mode";
        case mode_context_paint:       return "Paint Mode";
        case mode_context_animation:   return "Animation Mode";
        case mode_context_textureshop: return "Texture Shop Mode";
    }
    return "Mode";
}

void start_mode_state(void)
{
    if (g_mode_state.initialized) return;
    memset(&g_mode_state, 0, sizeof(g_mode_state));

    g_mode_state.context  = mode_context_object;
    g_mode_state.mode     = mode_object;
    g_mode_state.workflow = workflow_mode_model;

    g_mode_state.object.tool             = tool_select;
    g_mode_state.object.snap_enabled     = false;
    g_mode_state.object.snap_translate_m = 0.25f;
    g_mode_state.object.snap_rotate_deg  = 15.0f;
    g_mode_state.object.snap_scale       = 0.1f;

    g_mode_state.edit.select_type        = select_vert;
    g_mode_state.edit.soft_select_radius = 1.0f;

    g_mode_state.uv.texture_size         = 1024;

    g_mode_state.sculpt.brush_radius     = 1.0f;
    g_mode_state.sculpt.brush_strength   = 0.5f;
    g_mode_state.sculpt.brush_falloff    = 0.5f;

    g_mode_state.model.extrude_distance  = 1.0f;
    g_mode_state.model.bevel_width       = 0.1f;
    g_mode_state.model.bevel_segments    = 2;
    g_mode_state.model.merge_threshold   = 0.001f;

    g_mode_state.paint.color[0] = g_mode_state.paint.color[1] = g_mode_state.paint.color[2] = g_mode_state.paint.color[3] = 1.0f;
    g_mode_state.paint.brush_size = 16.0f;
    g_mode_state.paint.opacity    = 1.0f;
    g_mode_state.paint.hardness   = 0.5f;

    g_mode_state.initialized = true;
}

void mode_state_sync_from_ui(system_element_context* ui_element)
{
    start_mode_state();
    if (!ui_element) return;

    /* Whichever menu group changed last decides what the Modeler window shows. */
    static bool have_prev = false;
    static enum_dcc_mode prev_mode;
    static enum_workflow_mode prev_wf;
    if (s_skip_detect) {
        s_skip_detect = false;          /* mode_state_set_exclusive just chose the context: do not let change detection override it */
    } else if (have_prev) {
        if (ui_element->active_mode != prev_mode)
            g_mode_state.context = (ui_element->active_mode == mode_edit_mesh) ? mode_context_edit : mode_context_object;
        if (ui_element->current_workflow != prev_wf)
            g_mode_state.context = context_from_workflow(ui_element->current_workflow);
    }
    prev_mode = ui_element->active_mode; prev_wf = ui_element->current_workflow; have_prev = true;

    g_mode_state.object.tool       = ui_element->active_tool;
    g_mode_state.mode              = ui_element->active_mode;
    g_mode_state.workflow          = ui_element->current_workflow;
    g_mode_state.edit.select_type  = ui_element->active_select_type;

    {
        static int last_ctx = -1;
        if (last_ctx != (int)g_mode_state.context) {
            phoenix_info("Mode", "context %s -> %s (active_mode=%d workflow=%d)", last_ctx < 0 ? "start" : mode_context_name((system_mode_context)last_ctx),
                         mode_context_name(g_mode_state.context), (int)ui_element->active_mode, (int)ui_element->current_workflow);
            last_ctx = (int)g_mode_state.context;
        }
    }
}

void mode_state_set_tool(system_element_context* ui_element, enum_dcc_tool tool)
{
    start_mode_state();
    g_mode_state.object.tool = tool;
    if (ui_element) ui_element->active_tool = tool;
}

void mode_state_set_mode(system_element_context* ui_element, enum_dcc_mode mode)
{
    start_mode_state();
    g_mode_state.mode = mode;
    g_mode_state.context = (mode == mode_edit_mesh) ? mode_context_edit : mode_context_object;
    if (ui_element) ui_element->active_mode = mode;
}

void mode_state_set_workflow(system_element_context* ui_element, enum_workflow_mode workflow)
{
    start_mode_state();
    g_mode_state.workflow = workflow;
    g_mode_state.context = context_from_workflow(workflow);
    if (ui_element) ui_element->current_workflow = workflow;
}

/* ---- one mode at a time ---- */
void mode_state_set_exclusive(system_element_context* ui_element, system_mode_context ctx)
{
    start_mode_state();

    /* uv / model / animation are workflows, not exclusive modes: they fall back to the matching base mode.
       texture shop is its own context: it runs on top of the object / model base mode. */
    enum_dcc_mode      mesh  = mode_object;
    enum_workflow_mode wf = workflow_mode_model;

    switch (ctx) {
        case mode_context_edit:        mesh = mode_edit_mesh;                          break;
        case mode_context_sculpt:      wf = workflow_mode_sculpt;                   break;
        case mode_context_paint:       wf = workflow_mode_paint;                    break;
        case mode_context_uv:          wf = workflow_mode_uv;                       break;
        case mode_context_animation:   wf = workflow_mode_animation;                break;
        case mode_context_textureshop: break;
        case mode_context_model:
        case mode_context_object:
        default:                       ctx = mode_context_object;                   break;
    }

    g_mode_state.context  = ctx;
    g_mode_state.mode     = mesh;
    g_mode_state.workflow = wf;

    if (ui_element) {
        ui_element->active_mode      = mesh;
        ui_element->current_workflow = wf;
    }
    s_skip_detect = true;
    phoenix_info("Mode", "set_exclusive: %s (mode=%d workflow=%d)", mode_context_name(g_mode_state.context), (int)mesh, (int)wf);
}

bool mode_state_is_sculpt(void)      { start_mode_state(); return g_mode_state.context == mode_context_sculpt;      }
bool mode_state_is_paint(void)       { start_mode_state(); return g_mode_state.context == mode_context_paint;       }
bool mode_state_is_edit(void)        { start_mode_state(); return g_mode_state.context == mode_context_edit;        }
bool mode_state_is_object(void)      { start_mode_state(); return g_mode_state.context == mode_context_object;      }
bool mode_state_is_textureshop(void) { start_mode_state(); return g_mode_state.context == mode_context_textureshop; }