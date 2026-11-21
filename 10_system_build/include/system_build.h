#ifndef phoenix_system_build_h
#define phoenix_system_build_h

#include <stdbool.h>
#include <stddef.h>

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
/* 10_system_build -> 01_bin/phoenix_build.exe                                */
/*                                                                           */
/* turns the editor's 01_bin folder into a game folder: the engine exe, the   */
/* game dll, the loader dll, the shaders and the project assets. the editor   */
/* dll and the settings file stay behind.                                    */
/*                                                                           */
/*   phoenix_build.exe <output_folder> [source_folder]                        */
/*                                                                           */
/* source_folder defaults to the folder phoenix_build.exe lives in.           */
/* the folder gets phoenix_manifest.txt: the list the installer reads.        */
/* the editor starts it from Preferences > Build.                             */
/* ========================================================================= */

#define build_path_size 520

typedef struct {
    char source_folder[build_path_size];
    char output_folder[build_path_size];
    int  copied_count;
    int  failed_count;
    int  skipped_count;
} build_job;

/* files that never go into a game build (the editor and the editor's own settings) */
bool is_build_file_excluded(const char* file_name);

/* copies one file, creating the folders in front of it */
bool copy_build_file(const char* from_path, const char* to_path);

/* copies a folder and everything in it, relative paths of copied files go to the manifest file */
bool copy_build_folder(build_job* job, const char* from_folder, const char* to_folder, const char* relative_folder, void* manifest_file);

/* runs the whole build for job->source_folder -> job->output_folder */
bool run_build_job(build_job* job);

#endif
