#ifndef SYSTEM_PREFERENCES_H
#define SYSTEM_PREFERENCES_H

#include <stdbool.h>
#include <vulkan/vulkan.h>
#include "system_element.h"   /* system_element_context */
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

/* Settings that are saved to editor_prefs.ini (next to the exe) and loaded at boot. */
typedef struct {
    int  msaa_samples;   /* 1 = off, 2, 4 or 8 */
    bool vsync;          /* true = FIFO present mode */
    bool fxaa;           /* post-process FXAA on the scene / game view. Live: no restart needed */
} editor_prefs;

extern editor_prefs g_prefs;

void prefs_load(void);   /* call once, before the renderer starts */
void update_system_preference(void);   /* call AFTER every system is initialized: re-reads the save file and re-applies all options */
void prefs_save(void);

/* editor state kept in the same phoenix_editor.pdb: Scene tab buttons, wire colors, editor camera, tool, select type, snap.
   system_editor_state.c. prefs.c calls these from its load and save. */
void editor_state_apply_from_db(void);
void editor_state_write_to_db(void);

/* The sample face_type the renderer is really running with. Decided once (settings clamped to what the GPU supports),
   so renderpass, MSAA image, framebuffers and EVERY pipeline always agree. Use it for any pipeline that draws
   into the main pass:  multisample.rasterizationSamples = get_msaa_sample_count();  */
VkSampleCountFlagBits get_msaa_sample_count(void);

/* Preferences window (modal). */
void open_preferences_window(void);
bool is_preferences_window_open(void);
void preferences_begin_frame(system_element_context* ui_ctx);   /* start of the UI pass, BEFORE the dock: swallows the mouse button while the window is open */
void draw_preferences_window(VkCommandBuffer cmd, system_element_context* ui_ctx, int screen_w, int screen_h);   /* end of the UI pass, draws on top */

#endif