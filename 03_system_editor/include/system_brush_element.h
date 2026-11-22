#pragma once
#include "system_globals.h"
/*
RULES THAT MUST BE FOLLOWED.
uncomment as we start getting things fixed: 
Must use lowercase seperated by underscores. Even structs names, enum names, etc.
Must use verbs before nouns when naming anything
Must not use ternerys. i fucking hate ternery operators.
Must use braced control-flow headers: every if, else if, and else must use braces.
Must call extern in engine_globals.h and init in engine_globals.c
Must not have statics in any file. I fucking hate statics.
No static structs, No static enums, No static anything.
Math functions must go in math file and header.
Drawing stuff must go into drawing stuff and headers.
ALl information gets routed into statusbar.

NO CAPITAL LETTERS UNLESS WINDOWS OR VULKAN DEMANDS IT.
DO NOT REMOVE THIS COMMENT EVER. THESE ARE THE RULES.
IF SOMETHING IS BREAKING THESE RULES CHANGE IT THEN WE CHANGE EVERYTHING CONNECTED AS WE GO.
THIS INCLUDES INSIDE THE SOURCE FILES NOT JUST HEADERS
*/

/* Shared by the Sculpt window, the Paint window, the Brushes window, the Scene view and the brush input files. */

/* ---- once per frame, called from draw_sceneview_window ---- */
void     brush_ui_frame(system_element_context* ui_element);   /* advances the frame id, runs sculpt input + paint input once */
unsigned brush_frame_id(void);

/* ---- brush cursor (ring in the Scene view, OS arrow hidden) ---- */
void brush_cursor_set(const system_element_context* ui_element, editor_vec3 world, float radius_m, float hardness,
                      const float* ring_rgb /* NULL = white */, const float* swatch_rgb /* NULL = no centre swatch */);
bool brush_cursor_is_on(void);
void draw_brush_cursor(VkCommandBuffer cmd, const system_element_context* ui_element);

/* ---- tab sub menubar buttons, same look as the Scene tab ---- */
typedef struct {
    VkCommandBuffer          cmd;
    system_element_context*  ui_element;
    int                      x, y, s, right;     /* cursor, button size, right limit */
    uint32_t                 id_base;
    int                      face_type;
    const char*              hover_name;
    bool                     ok;                 /* false when the bar is too small to draw */
} brush_bar;

void brush_bar_begin(brush_bar* b, VkCommandBuffer cmd, system_element_context* ui_element, const system_dock_node* node, uint32_t id_base);
bool brush_bar_button(brush_bar* b, const char* label, const char* name, bool on);   /* true on click */
void brush_bar_separator(brush_bar* b);
void brush_bar_end(brush_bar* b);                                                    /* hovered button name, right aligned */
int  brush_bar_bottom(const system_dock_node* node, int content_y);                  /* first y under the bar */

/* ---- sliders (drag = set, Shift = fine). Replace the slow -/+ arrows.
        "Label  [=====  0.50 mesh  ]"   label on the left, track with the value written inside it.
        id        any number that is unique inside your window (0..0xFFF). The widget adds its own base.
        log_scale true = the track is logarithmic (needs lo > 0): 0.02 mesh .. 400 mesh is usable at both ends.
        fmt       printf format for the shown value, e.g. "%.2f mesh"
        returns true on every frame the value changed. ---- */
bool brush_slider    (VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id, const char* label,
                      int x, int y, int w, int h, float* value, float lo, float hi, bool log_scale, const char* fmt);
bool brush_slider_int(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id, const char* label,
                      int x, int y, int w, int h, int* value, int lo, int hi);

/* ---- colour picker (Paint window). HSV square + hue bar + preview + Save / Del buttons.
        rgb is sRGB 0..1 (g_paint.color). h_avail = height you can spend. Returns the height it used (0 = no room). ---- */
int  brush_color_picker_height(void);                                          /* the height brush_color_picker uses when it is given enough room */
int  brush_color_palette_height(int w);                                      /* the height brush_color_palette uses for this width */
int  brush_color_picker (VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, int w, int h_avail, float rgb[3]);
/* saved colours (click = use, Del button then click = remove) + a 60 colour preset palette. Returns height used. */
int  brush_color_palette(VkCommandBuffer cmd, system_element_context* ui_element, int x, int y, int w, int h_avail, float rgb[3]);

enum { brush_palette_count = 60, brush_saved_max = 24 };
void brush_palette_color(int index, float out[3]);                 /* preset palette entry 0..59 */
int  brush_saved_color_count(void);
bool brush_saved_color_get(int index, float out[3]);
void brush_saved_color_add(const float rgb[3]);                    /* saved to phoenix_editor.pdb right away */
void brush_saved_color_remove(int index);
bool brush_saved_color_cycle(int dir /* +1 / -1 */, float rgb[3]); /* next / previous saved colour. false when none saved */
void brush_color_shift(float rgb[3], float dv);                    /* lighter (+) / darker (-): brightness step, hue kept */

/* ---- mesh detail: sculpt and paint both need vertices to work with ---- */
int  brush_densify_selected(uint32_t target_faces);                /* subdivides the obj_vert_selected mesh until it has about target_faces (max 6 levels). returns levels added. writes a status message. */

/* ---- windows ---- */
void brush_open_window(void);                                      /* Brushes tab next to the Hierarchy (same place the Sc button puts it) */
void brush_follow_modes(void);                                     /* once per UI frame: entering Paint mode opens the Brushes window, like Sculpt does */

/* ---- paint brush choice (draw_system_brush_window.c). number = order of brush_paint_kind ---- */
int         brush_window_get_paint_brush(void);
void        brush_window_set_paint_brush(int brush);
const char* brush_window_paint_brush_name(int brush);

/* ---- brush inputs ---- */
void brush_ensure_density(void);                                                    /* subdivides a low poly mesh so a brush has vertices to move */
bool brush_mouse_world_ray(const system_element_context* ui_element, editor_vec3* o, editor_vec3* d);   /* world space, d is unit length */
void sculpt_input_update(system_element_context* ui_element);
/* paint_input_update is declared in engine_mode.h. sculpt_input_update stays here until it moves there too. */