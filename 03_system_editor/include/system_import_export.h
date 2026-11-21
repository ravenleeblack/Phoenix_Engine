#ifndef phoenix_system_import_export_h
#define phoenix_system_import_export_h

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

/* ========================================================================= */
/* File > Import / Export                                                    */
/*   system_import_export.c     windows file dialogs, loader plugin, import,  */
/*                              export                                        */
/*   draw_system_export.c       the export options window                     */
/*                                                                           */
/*   import: .fbx / .obj -> a scene object, .png -> a skin. the file is copied into  */
/*   the File Archive (assets/models, assets/textures) so it is an asset.     */
/*   the loader is 01_bin/phoenix_loader.dll (06_system_loader).              */
/* ========================================================================= */

typedef enum {
    export_format_fbx_binary = 0,
    export_format_fbx_ascii,
    export_format_obj,
    export_format_png,
    export_format_count
} enum_export_format;

typedef enum {
    export_source_scene_object = 0,
    export_source_archive_file,
    export_source_count
} enum_export_source;

/* ---- windows file dialogs (comdlg32 is loaded at run time) ----
   filter = pairs of "label\0pattern\0" ending with an extra \0. the picked full path goes to out_path. */
bool pick_open_file_path(const char* title, const char* filter, char* out_path, size_t out_size);
bool pick_save_file_path(const char* title, const char* filter, const char* default_extension,
                         const char* initial_name, char* out_path, size_t out_size);

/* ---- import ---- */
void import_asset_from_dialog(void);                 /* File > Import... */
bool import_asset_file(const char* source_path);     /* the whole import for one .fbx / .obj / .png. sizes follow g_unit_prefs.import_profile */

/* ---- export ---- */
void open_export_window(void);                       /* File > Export... */
bool is_export_window_open(void);
bool export_current_selection(void);                 /* runs the save dialog and writes the file for the chosen options */
void export_begin_frame(system_element_context* ui_ctx);
void export_end_frame(system_element_context* ui_ctx);
void draw_export_window(VkCommandBuffer cmd, system_element_context* ui_ctx, int screen_w, int screen_h);

extern bool export_window_open;
extern int  export_format;
extern int  export_source;

bool check_path_extension(const char* path, const char* extension);

#endif
