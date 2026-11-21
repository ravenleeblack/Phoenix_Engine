#ifndef PHOENIX_SYSTEM_TOOLTIP_H
#define PHOENIX_SYSTEM_TOOLTIP_H

#include <stdbool.h>
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

/* Tooltips: a small box with a description that comes up next to the mouse when it rests on a button.
   A button calls set_tooltip_text("...") on every frame the mouse is over it. draw_tooltip_overlay runs once at the end of
   the frame: the box shows after the mouse has rested on the same text for a short moment, and hides while a mouse button is down. */

enum {
    tooltip_text_size = 160,
    tooltip_delay_frames = 18
};

typedef struct {
    char pending[tooltip_text_size];     /* asked for this frame */
    char shown[tooltip_text_size];       /* the text the mouse has been resting on */
    int  hover_frames;
} tooltip_state;

extern tooltip_state g_tooltip;

void set_tooltip_text(const char* text);
void draw_tooltip_overlay(VkCommandBuffer cmd, system_element_context* ui_element, int screen_w, int screen_h);

#endif
