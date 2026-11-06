#ifndef system_context_menu_h
#define system_context_menu_h

#include <stdbool.h>
#include <vulkan/vulkan.h>
#include "system_element.h"   /* system_element_context */
#include "system_theme.h"     /* system_color */
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

/* ------------------------------------------------------------------------- */
/* circles, built from 1px strips like push_rounded_quad. pixel-stepped edges.  */
/* ------------------------------------------------------------------------- */
void draw_circle_element(VkCommandBuffer cmd, float cx, float cy, float radius, system_color color);
void draw_circle_border (VkCommandBuffer cmd, float cx, float cy, float radius, float thickness, system_color color);

/* ------------------------------------------------------------------------- */
/* right click radial menu. every window decides what its own menu is.         */
/*                                                                             */
/*   inside a window's draw function, once per frame:                          */
/*       context_menu_scene_window(ui_element, x, y, w, h);        scene view menu     */
/*       context_menu_hierarchy_window(ui_element, x, y, w, h);    scene archive menu  */
/*       context_menu_files_window(ui_element, x, y, w, h);        file archive menu   */
/*       context_menu_textureshop_window(ui_element, x, y, w, h);  texture shop menu   */
/*   they open the menu when the right button is pressed inside that rect.     */
/*                                                                             */
/*   draw_context_menu() draws it. call it ONCE per frame, as late as you can  */
/*   (end of the UI pass, before the preferences window) so nothing covers it. */
/* ------------------------------------------------------------------------- */
void context_menu_scene_window       (system_element_context* ui_element, int x, int y, int w, int h);
void context_menu_hierarchy_window   (system_element_context* ui_element, int x, int y, int w, int h);
void context_menu_files_window       (system_element_context* ui_element, int x, int y, int w, int h);
void context_menu_textureshop_window (system_element_context* ui_element, int x, int y, int w, int h);
void draw_context_menu(VkCommandBuffer cmd, system_element_context* ui_element, int screen_w, int screen_h);

bool is_context_menu_open(void);

/* ---- skins (draw_system_textureshop.c): the Texture Shop menu calls these ---- */
void create_texshop_skin(void);
void duplicate_texshop_skin(void);
void delete_texshop_skin(void);
void step_texshop_skin(int direction);

/* ---- fed by the window proc: the right button only exists as a message, so system_proc forwards it.
        in system_proc():   case WM_RBUTTONDOWN: context_menu_notify_right_button(LOWORD(lp), HIWORD(lp)); return 0;  ---- */
void context_menu_notify_right_button(int x, int y);

/* true on the frame a right click landed inside this rect. lets other windows react to it (hierarchy selects the row). */
bool context_menu_right_click_in(int x, int y, int w, int h);

/* old hook, no longer needed. kept so a call you already added still links. */
void context_menu_begin_frame(system_element_context* ui_element);

#endif