#ifndef PHOENIX_SYSTEM_SCENEVIEW_H
#define PHOENIX_SYSTEM_SCENEVIEW_H

typedef enum {
    scene_icon_lock, scene_icon_home,
    scene_icon_grid, scene_icon_origin, scene_icon_camera,
    scene_icon_wire, scene_icon_shaded, scene_icon_textured, scene_icon_wire_on_shaded, scene_icon_xray,
    scene_icon_lit, scene_icon_aa, scene_icon_selection, scene_icon_sky, scene_icon_fog
} enum_scene_icon;

typedef struct {
    enum_scene_icon icon;
    const char* name;
    bool separator_before;
} scene_button;

#define scene_button_count 15

#endif
