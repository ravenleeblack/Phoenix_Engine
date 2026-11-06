#ifndef phoenix_engine_defines_h
#define phoenix_engine_defines_h

/* windows 10 target before windows.h: SetProcessDpiAwarenessContext needs it */
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#define PHOENIX_PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>
#include <stddef.h>
#include <math.h>

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

#if !defined(_WIN64)
#error "64-bit is required on windows"
#endif

/* the engine exe exports its functions, the editor, loader and game dlls import them.
   phoenix_engine_build is set only when compiling the engine exe.
   phoenix_static_link is for a single binary build with no dlls. */
#if defined(phoenix_static_link) || defined(PHOENIX_STATIC_LINK)   /* the makefile passes the caps spelling. drop the second test once it passes -Dphoenix_static_link */
    #define phoenix_engine_api
#elif defined(phoenix_engine_build)
    #define phoenix_engine_api __declspec(dllexport)
#else
    #define phoenix_engine_api __declspec(dllimport)
#endif

#define max_unsigned_aisle 0xFFFFFFFFFFFFFFFFULL

#endif