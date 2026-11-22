#ifndef phoenix_system_terrain_h
#define phoenix_system_terrain_h

#include "engine_terrain.h"
#include "system_element.h"
#include "system_scene_objects.h"
#include "system_edit_manager.h"

edit_mode_mesh* terrain_edit_mesh_acquire(system_scene_object* obj);
edit_mode_mesh* terrain_edit_mesh_current(void);
void terrain_apply_location(void);
bool terrain_commit_edit_mesh(edit_mode_mesh* mesh, system_scene_object* obj);

/* the terrain's paint layer (one RGBA color per cube column). see system_terrain.c */
bool terrain_copy_paint_to_pixels(uint8_t* rgba, int width, int height);
bool terrain_set_paint_from_pixels(const uint8_t* rgba, int width, int height);
void terrain_clear_paint(void);

#endif
