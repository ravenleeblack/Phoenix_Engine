#include "system_globals.h"

extern system_render_context render_context;

/* the scene / game tag is an enum_viewport_kind argument now, not a magic alpha. see push_viewport_quad_element. */
static void draw_viewport_quad(system_element_context* context, VkCommandBuffer cmd, const char* who,
                               int x, int y, int w, int h, VkDescriptorSet texture_handle, enum_viewport_kind kind)
{
    if (!context || w <= 0 || h <= 0) return;

    if (cmd == VK_NULL_HANDLE) {
        phoenix_error("Viewport_UI", "Fatal: VK_NULL_HANDLE command buffer passed to %s.", who);
        return;
    }

    /* quads are queued and drawn later in render_element_commands(), which binds the descriptor set itself */
    (void)texture_handle;

    push_viewport_quad_element(cmd, (float)x, (float)y, (float)w, (float)h, kind);
}

void draw_sceneview_viewport(system_element_context* context, VkCommandBuffer cmd, int x, int y, int w, int h, VkDescriptorSet scene_texture_handle)
{
    draw_viewport_quad(context, cmd, "draw_sceneview_viewport", x, y, w, h, scene_texture_handle, viewport_kind_scene);
}

void draw_gameview_viewport_00(system_element_context* context, VkCommandBuffer cmd, int x, int y, int w, int h, VkDescriptorSet game_texture_00_handle)
{
    draw_viewport_quad(context, cmd, "draw_gameview_viewport_00", x, y, w, h, game_texture_00_handle, viewport_kind_game);
}

void draw_gameview_viewport_01(system_element_context* context, VkCommandBuffer cmd, int x, int y, int w, int h, VkDescriptorSet game_texture_01_handle)
{
    draw_viewport_quad(context, cmd, "draw_gameview_viewport_01", x, y, w, h, game_texture_01_handle, viewport_kind_game);
}

void draw_gameview_viewport_02(system_element_context* context, VkCommandBuffer cmd, int x, int y, int w, int h, VkDescriptorSet game_texture_02_handle)
{
    draw_viewport_quad(context, cmd, "draw_gameview_viewport_02", x, y, w, h, game_texture_02_handle, viewport_kind_game);
}