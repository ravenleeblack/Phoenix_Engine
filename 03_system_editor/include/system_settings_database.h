#ifndef SYSTEM_SETTINGS_DATABASE_H
#define SYSTEM_SETTINGS_DATABASE_H

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

/* Program settings database.
   One small text file that lives NEXT TO THE EXE (the folder is taken from GetModuleFileNameA, never from the
   working directory, so it is the same file no matter how the editor is launched).
   Every system that wants something remembered between runs reads it at boot and writes it when it changes. */

#define SETTINGS_DB_FILE_NAME "phoenix_editor.pdb"

/* Finds the exe folder and loads the file if it exists. Safe to call more than once; every other call below
   starts the database by itself if it has not been started yet. */
void settings_db_start(void);

/* Writes every key to disk (temp file + replace, so a crash never leaves half a file). */
bool settings_db_save(void);

/* Re-reads the file from disk and overlays it onto the in-memory table (keys in the file overwrite memory). */
void settings_db_reload(void);

bool        settings_db_file_existed(void);                 /* true when a saved file was found at start */
const char* settings_db_directory(void);                    /* exe folder, trailing backslash */
const char* settings_db_file_path(void);                    /* full path of the settings file */
bool        settings_db_make_path(const char* file_name, char* out, size_t out_size);   /* exe folder + file_name */

/* get: false when the key is missing or does not parse (out is left untouched). */
bool settings_db_get_int(const char* key, int* out);
bool settings_db_get_float(const char* key, float* out);
bool settings_db_get_floats(const char* key, float* out, int face_type);
bool settings_db_get_string(const char* key, char* out, size_t out_size);

/* set: changes memory only. Call settings_db_save() to write. */
bool settings_db_set_int(const char* key, int value);
bool settings_db_set_float(const char* key, float value);
bool settings_db_set_floats(const char* key, const float* values, int face_type);
bool settings_db_set_string(const char* key, const char* value);

#endif