#ifndef PHOENIX_SYSTEM_SANDBOX_MC_H
#define PHOENIX_SYSTEM_SANDBOX_MC_H

#include <stdbool.h>
#include <stdint.h>
#include "system_math.h"      /* editor_vertex */
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

/*Need to add cat smooth and subdive along with liner*/


/*Marching Smoothing ALgoritmn*/
/* ------------------------------------------------------------------------- */
/* Sandbox terrain: marching cubes over a 3D density grid.                    */
/*                                                                            */
/*   every cube has 8 corners. every corner stores ONE density, 0..15:        */
/*       0  = air        15 = solid        solid when density >= 8             */
/*   the surface crosses an edge_to_face_array where the density passes 7.5, so a vertex     */
/*   lands on one of ~16 positions along that edge_to_face_array ("divided by 16").          */
/*   open world keeps using the height grid. sandbox uses this.               */
/*                                                                            */
/*   grid space: corner (gx,gy,gz) sits at (gx,gy,gz) * cell_size metres.      */
/*   data lives in 32 x 32 x 32 cell chunks that are created on demand.        */
/* ------------------------------------------------------------------------- */
#define SBX_LEVELS       16
#define SBX_MAX_DENSITY  15
#define SBX_ISO          7.5f
#define SBX_CHUNK_CELLS  32

typedef struct { editor_vertex* vert_to_edge_array; uint32_t* i; uint32_t vc, ic; } sbx_mesh;
typedef enum { SBX_DIG, SBX_ADD } sbx_brush_mode;
typedef float (*sbx_height_fn)(int gx, int gz, void* user);

void sbx_start(float cell_size_m);       /* builds the case table. safe to call again (also clears the world) */
void sbx_shutdown(void);
void sbx_clear(void);
float sbx_cell_size(void);

/* corner density. unset corners read as air (0) */
uint8_t sbx_get(int gx, int gy, int gz);
void    sbx_set(int gx, int gy, int gz, uint8_t density);   /* marks every chunk whose mesh touches this corner dirty */

/* Open world -> sandbox: solid below the height, air above, gy_min is a solid floor. */
void sbx_fill_from_heights(sbx_height_fn height_m, void* user, int gx0, int gz0, int gx1, int gz1, int gy_min, int gy_max);

/* digger brush: a sphere in grid-space metres. DIG carves it out, ADD fills it in. one call = one stamp. */
void sbx_sphere(float x_m, float y_m, float z_m, float radius_m, sbx_brush_mode mode);

/* meshing. one chunk at a time. offset_* moves the mesh (terrain origin). counter-clockwise from outside.
   returns false if the chunk has no surface. free the result with sbx_mesh_free. */
bool sbx_mesh_chunk(int cx, int cy, int cz, float offset_x, float offset_y, float offset_z, sbx_mesh* out);
void sbx_mesh_free(sbx_mesh* mesh);

/* dirty chunks: call until it returns false, rebuild each one it hands you. */
bool sbx_next_dirty_chunk(int* cx, int* cy, int* cz);
int  sbx_chunk_count(void);

#endif