#ifndef phoenix_system_installer_h
#define phoenix_system_installer_h

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
/* 09_system_installer -> 01_bin/phoenix_installer.exe                        */
/*                                                                           */
/* installs a game folder made by phoenix_build.exe (it reads the            */
/* phoenix_manifest.txt in that folder).                                      */
/*                                                                           */
/*   phoenix_installer.exe [game_folder] [install_folder]                     */
/*                                                                           */
/* game_folder    defaults to the folder the installer lives in               */
/* install_folder defaults to %LOCALAPPDATA%\phoenix_game                     */
/* it also writes uninstall.bat into the install folder.                      */
/* ========================================================================= */

#define installer_path_size 520

typedef struct {
    char game_folder[installer_path_size];
    char install_folder[installer_path_size];
    int  copied_count;
    int  failed_count;
} installer_job;

/* copies one file, creating the folders in front of it */
bool copy_installer_file(const char* from_path, const char* to_path);

/* writes uninstall.bat into the install folder */
bool write_installer_uninstall_script(const installer_job* job);

/* reads game_folder\phoenix_manifest.txt and copies every listed file */
bool run_installer_job(installer_job* job);

#endif
