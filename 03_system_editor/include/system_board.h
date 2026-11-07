#ifndef PHOENIX_SYSTEM_BOARD_H
#define PHOENIX_SYSTEM_BOARD_H

#include <stdbool.h>
#include <stdint.h>
#include "system_math.h"
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
/* Board pieces (truncated-square tiling: octagons + diamonds)               */
/*                                                                           */
/*   a = BOARD_EDGE_M (4 mesh, same as the cube). Every outline edge_to_face_array is 4 mesh.    */
/*   Pieces lie flat in the X/Z plane, thickness along Y.                    */
/*                                                                           */
/*   octagon : flat sides face +/-Z and +/-X. Across the flats = a(1+sqrt2). */
/*   diamond : 4 sides of a. Fills the gap where 4 octagons meet.            */
/*   triangle half    : half diamond (cut along the long diagonal). Legs a.  */
/*                      Long edge_to_face_array (a*sqrt2) lies on the board edge_to_face_array.          */
/*   triangle quarter : quarter diamond. Legs a/sqrt2, long edge_to_face_array a. Corners. */
/*                                                                           */
/*   Origin: octagon = its center. diamond / half / quarter = the CENTER OF  */
/*   THE FULL DIAMOND, so the three always share the same pivot.             */
/* ------------------------------------------------------------------------- */
#define BOARD_EDGE_M       4.0f
#define BOARD_THICKNESS_M  0.25f

typedef enum {
    BOARD_TILE_OCTAGON = 0,
    BOARD_TILE_DIAMOND,
    BOARD_TILE_TRIANGLE_HALF,
    BOARD_TILE_TRIANGLE_QUARTER,
    BOARD_TILE_KIND_COUNT
} enum_board_tile_kind;

/* Stored in the object database as object_data_board_tile. */
typedef struct {
    uint32_t kind;          /* enum_board_tile_kind */
    float    edge_m;
    float    thickness_m;
    uint32_t reserved;
} board_tile_data;

/* Distance between two neighbouring octagon centers (= across the flats). */
float board_cell_pitch(float edge_m);

/* Outline in X/Z (object space, y ignored). Returns the point face_type (max 8). Order does not matter. */
int board_outline_octagon(float edge_m, float out_xz[8][2]);          /* system_board_octogon.c  */
int board_outline_diamond(float edge_m, float out_xz[8][2]);          /* system_board_diamond.c  */
int board_outline_triangle_half(float edge_m, float out_xz[8][2]);    /* system_board_triangle.c */
int board_outline_triangle_quarter(float edge_m, float out_xz[8][2]); /* system_board_triangle.c */

const char* board_tile_name(enum_board_tile_kind kind);
editor_vec3 board_default_position(enum_board_tile_kind kind);

/* Shared spawner. position may be NULL (default spot). Registers the scene object, uploads it to the GPU,
   stores the mesh + board_tile_data in the object database and adopts the mesh into edit mode. */
bool board_spawn_tile(enum_board_tile_kind kind, const editor_vec3* position);

/* position may be NULL. */
bool create_system_board_octagon(const editor_vec3* position);
bool create_system_board_diamond(const editor_vec3* position);
bool create_system_board_triangle_half(const editor_vec3* position);
bool create_system_board_triangle_quarter(const editor_vec3* position);

#endif