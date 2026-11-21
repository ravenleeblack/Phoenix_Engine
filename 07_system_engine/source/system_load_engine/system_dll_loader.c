#include "engine_globals.h"

FARPROC resolve_system_symbol(HMODULE module, const char* module_name, const char* symbol_name)
{
    if (!module) {
        phoenix_engine_error("linker", "%s is not loaded, cannot resolve '%s'.", module_name, symbol_name);
        return NULL;
    }

    FARPROC address = GetProcAddress(module, symbol_name);
    if (!address) {
        phoenix_engine_error("linker", "%s is missing '%s'.", module_name, symbol_name);
    }
    return address;
}

bool load_editor_library(void)
{
    /* the editor dll imports the engine exe by name, so the exe must be called phoenix_engine.exe */
    system_editor_dll = LoadLibraryA("phoenix_editor.dll");
    if (!system_editor_dll) {
        phoenix_engine_error("linker", "failed to load phoenix_editor.dll from bin directory (error %lu).", GetLastError());
        return false;
    }

    const char* name = "phoenix_editor.dll";
    g_editor.load_settings     = (load_editor_settings_ptr)   resolve_system_symbol(system_editor_dll, name, "editor_load_settings");
    g_editor.save_settings     = (save_editor_settings_ptr)   resolve_system_symbol(system_editor_dll, name, "editor_save_settings");
    g_editor.apply_settings    = (apply_editor_settings_ptr)  resolve_system_symbol(system_editor_dll, name, "editor_apply_settings");
    g_editor.start             = (start_editor_ptr)           resolve_system_symbol(system_editor_dll, name, "editor_start");
    g_editor.update            = (update_editor_ptr)          resolve_system_symbol(system_editor_dll, name, "editor_update");
    g_editor.begin_frame_pass  = (begin_editor_frame_pass_ptr)resolve_system_symbol(system_editor_dll, name, "editor_begin_frame_pass");
    g_editor.draw_ui           = (draw_editor_ui_ptr)         resolve_system_symbol(system_editor_dll, name, "editor_draw_ui");
    g_editor.draw_menubar      = (draw_editor_menubar_ptr)    resolve_system_symbol(system_editor_dll, name, "editor_draw_menubar");
    g_editor.draw_toolbar      = (draw_editor_toolbar_ptr)    resolve_system_symbol(system_editor_dll, name, "editor_draw_toolbar");
    g_editor.draw_dock         = (draw_editor_dock_ptr)       resolve_system_symbol(system_editor_dll, name, "editor_draw_dock");
    g_editor.draw_statusbar    = (draw_editor_statusbar_ptr)  resolve_system_symbol(system_editor_dll, name, "editor_draw_statusbar");
    g_editor.flush_ui          = (flush_editor_ui_ptr)        resolve_system_symbol(system_editor_dll, name, "editor_flush_ui");
    g_editor.end_frame_pass    = (end_editor_frame_pass_ptr)  resolve_system_symbol(system_editor_dll, name, "editor_end_frame_pass");
    g_editor.handle_message    = (handle_editor_message_ptr)  resolve_system_symbol(system_editor_dll, name, "editor_handle_message");
    g_editor.shutdown          = (shutdown_editor_ptr)        resolve_system_symbol(system_editor_dll, name, "editor_shutdown");

    /* the editor's offscreen scene pass: the vulkan backend calls these while it builds the device */
    g_editor.create_offscreen_renderpass  = (create_offscreen_renderpass_ptr) resolve_system_symbol(system_editor_dll, name, "offscreen_create_renderpass");
    g_editor.get_offscreen_renderpass     = (get_offscreen_renderpass_ptr)    resolve_system_symbol(system_editor_dll, name, "offscreen_get_renderpass");
    g_editor.create_offscreen_resources   = (create_offscreen_resources_ptr)  resolve_system_symbol(system_editor_dll, name, "offscreen_create_resources");
    g_editor.shutdown_offscreen           = (shutdown_offscreen_ptr)          resolve_system_symbol(system_editor_dll, name, "offscreen_shutdown");
    g_editor.create_scene_object_pipeline = (create_scene_object_pipeline_ptr)resolve_system_symbol(system_editor_dll, name, "create_scene_object_pipeline");
    g_editor.record_offscreen_frame       = (record_offscreen_frame_ptr)      resolve_system_symbol(system_editor_dll, name, "offscreen_record_frame");

    if (!g_editor.load_settings || !g_editor.save_settings || !g_editor.apply_settings || !g_editor.start || !g_editor.update ||
        !g_editor.begin_frame_pass || !g_editor.draw_ui || !g_editor.draw_menubar ||
        !g_editor.draw_toolbar || !g_editor.draw_dock || !g_editor.draw_statusbar ||
        !g_editor.flush_ui || !g_editor.end_frame_pass ||
        !g_editor.handle_message || !g_editor.shutdown ||
        !g_editor.create_offscreen_renderpass || !g_editor.create_offscreen_resources ||
        !g_editor.get_offscreen_renderpass ||
        !g_editor.shutdown_offscreen || !g_editor.create_scene_object_pipeline ||
        !g_editor.record_offscreen_frame) {
        phoenix_engine_error("linker", "failed to resolve editor symbols in phoenix_editor.dll.");
        return false;
    }
    return true;
}

bool load_loader_library(void)
{
    system_loader_dll = LoadLibraryA("phoenix_loader.dll");
    if (!system_loader_dll) {
        phoenix_engine_error("linker", "failed to load phoenix_loader.dll from bin directory (error %lu).", GetLastError());
        return false;
    }

    const char* name = "phoenix_loader.dll";
    g_loader.load_image_asset = (load_image_asset_ptr)
        resolve_system_symbol(system_loader_dll, name, "load_image_asset");
    g_loader.remove_image_asset = (remove_image_asset_ptr)
        resolve_system_symbol(system_loader_dll, name, "remove_image_asset");
    g_loader.load_3d_mesh_asset = (load_3d_mesh_asset_ptr)
        resolve_system_symbol(system_loader_dll, name, "load_3d_mesh_asset");
    g_loader.remove_3d_mesh_asset = (remove_3d_mesh_asset_ptr)
        resolve_system_symbol(system_loader_dll, name, "remove_3d_mesh_asset");

    if (!g_loader.load_image_asset || !g_loader.remove_image_asset ||
        !g_loader.load_3d_mesh_asset || !g_loader.remove_3d_mesh_asset) {
        phoenix_engine_error("linker", "failed to resolve loader symbols in phoenix_loader.dll.");
        return false;
    }
    return true;
}

bool load_game_library(void)
{
    system_game_dll = LoadLibraryA("system_game.dll");
    if (!system_game_dll) {
        phoenix_engine_error("linker", "failed to load system_game.dll from bin directory (error %lu).", GetLastError());
        return false;
    }

    create_game_handle = (create_game_ptr)resolve_system_symbol(system_game_dll, "system_game.dll", "create_game");
    if (!create_game_handle) {
        phoenix_engine_error("linker", "failed to resolve factory symbol 'create_game' inside system_game.dll.");
        return false;
    }
    return true;
}

bool load_subsystem_libraries(void)
{
    if (!load_editor_library())
    {
        return false;
    }
    if (!load_loader_library())
    {
        return false;
    }
    if (!load_game_library())
    {
        return false;
    }

    phoenix_engine_info("linker", "editor, loader and game plugins linked and mapped.");
    return true;
}

void unload_subsystem_libraries(void)
{
    /* reverse of load order */
    if (system_game_dll) {
        FreeLibrary(system_game_dll);
        system_game_dll = NULL;
    }
    if (system_loader_dll) {
        FreeLibrary(system_loader_dll);
        system_loader_dll = NULL;
    }
    if (system_editor_dll) {
        FreeLibrary(system_editor_dll);
        system_editor_dll = NULL;
    }
    g_editor = (editor_plugin){0};
    g_loader = (loader_plugin){0};
    create_game_handle = NULL;
}