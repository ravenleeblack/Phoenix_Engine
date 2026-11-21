#include "system_globals.h"
#include "system_scene_look.h"
#include <stdarg.h>
#include <stdio.h>


/* defaults: MSAA off until the mesh pipeline also reads get_msaa_sample_count() (see create_scene_object_pipeline). */
editor_prefs g_prefs = { 1, true, false };

/* scene camera: near / far clip in meters, pan speed multiplier */
camera_prefs g_camera_prefs = { 0.5f, 5000.0f, 5.0f };   /* near 0.5 mesh: a near clip below 0.1 mesh wrecks depth precision (see camera_prefs_clamp) */

void camera_prefs_clamp(void)
{
    /* The depth buffer is standard D32_SFLOAT: the smallest depth step it can tell apart grows with distance squared and shrinks
       as near grows. Near 0.005 mesh gives ~12 mesh of error at 1 km (terrain shimmers / looks warped). Any saved value below 0.1 mesh
       (old default 0.03, old minimum 0.005) is moved to 0.5 mesh on load. */
    if (g_camera_prefs.near_clip < 0.01f)   g_camera_prefs.near_clip = 0.01f;   /* 1 cm. The scene camera also pulls near down by itself when you are close to the ground (system_scene_camera.c) */
    if (g_camera_prefs.near_clip > 2.0f)    g_camera_prefs.near_clip = 2.0f;
    if (g_camera_prefs.far_clip  < 100.0f)  g_camera_prefs.far_clip  = 100.0f;
    if (g_camera_prefs.far_clip  > 50000.0f) g_camera_prefs.far_clip = 50000.0f;
    if (g_camera_prefs.far_clip  < g_camera_prefs.near_clip * 100.0f) g_camera_prefs.far_clip = g_camera_prefs.near_clip * 100.0f;
    if (g_camera_prefs.pan_speed < 0.5f)    g_camera_prefs.pan_speed = 0.5f;
    if (g_camera_prefs.pan_speed > 30.0f)   g_camera_prefs.pan_speed = 30.0f;
}

/* what the renderer actually started with (needs a restart to change) */
int  preferences_active_msaa  = -1;
bool preferences_active_vsync = true;

/* prefs_save() is ignored until update_system_preference() has run at the end of boot.
   Init code that saves early would write default theme / FXAA over the real file. */
bool preferences_boot_done = false;
void prefs_write(void);

/* ------------------------------------------------------------------------- */
/* load / save                                                                */
/* ------------------------------------------------------------------------- */

/* Keys in the settings database (phoenix_editor.pdb, next to the exe). */
/* One-time migration: older builds wrote editor_prefs.ini in the working directory. */
bool load_legacy_ini(void)
{
    FILE* f = fopen("editor_prefs.ini", "r");
    if (!f) return false;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        int vert_to_edge_array = 0;
        float a = 0, b = 0, c = 0;
        if (sscanf(line, "bg_top=%f,%f,%f", &a, &b, &c) == 3) {
            g_scene_bg.top[0] = a; g_scene_bg.top[1] = b; g_scene_bg.top[2] = c;
        } else if (sscanf(line, "bg_bottom=%f,%f,%f", &a, &b, &c) == 3) {
            g_scene_bg.bottom[0] = a; g_scene_bg.bottom[1] = b; g_scene_bg.bottom[2] = c;
        } else if (sscanf(line, "sun=%f,%f,%f", &a, &b, &c) == 3) {
            g_scene_sun.azimuth_deg = a; g_scene_sun.elevation_deg = b; g_scene_sun.ambient = c;
        } else if (sscanf(line, "msaa=%d", &vert_to_edge_array) == 1) {
            g_prefs.msaa_samples = (vert_to_edge_array == 2 || vert_to_edge_array == 4 || vert_to_edge_array == 8) ? vert_to_edge_array : 1;
        } else if (sscanf(line, "vsync=%d", &vert_to_edge_array) == 1) {
            g_prefs.vsync = (vert_to_edge_array != 0);
        }
    }
    fclose(f);
    return true;
}

/* Reads every key from the settings database and pushes it into the live globals. */
void apply_prefs_from_db(bool allow_legacy_migration)
{
    int   vert_to_edge_array = 0;
    float f[3];
    bool  found = false;

    /* a key that is missing or does not parse leaves the default untouched */
    if (settings_db_get_int("prefs.msaa", &vert_to_edge_array))  { g_prefs.msaa_samples = (vert_to_edge_array == 2 || vert_to_edge_array == 4 || vert_to_edge_array == 8) ? vert_to_edge_array : 1; found = true; }
    if (settings_db_get_int("prefs.vsync", &vert_to_edge_array)) { g_prefs.vsync = (vert_to_edge_array != 0); found = true; }
    if (settings_db_get_int("prefs.fxaa", &vert_to_edge_array))  { g_prefs.fxaa  = (vert_to_edge_array != 0); found = true; }
    if (settings_db_get_floats("prefs.bg.top", f, 3))    { g_scene_bg.top[0] = f[0];    g_scene_bg.top[1] = f[1];    g_scene_bg.top[2] = f[2];    found = true; }
    if (settings_db_get_floats("prefs.bg.bottom", f, 3)) { g_scene_bg.bottom[0] = f[0]; g_scene_bg.bottom[1] = f[1]; g_scene_bg.bottom[2] = f[2]; found = true; }
    if (settings_db_get_int("prefs.theme", &vert_to_edge_array) && vert_to_edge_array >= 0 && vert_to_edge_array < theme_count()) { theme_set(vert_to_edge_array); found = true; }   /* re-applied by update_system_preference after start_editor_elements */
    if (settings_db_get_floats("prefs.sun", f, 3))       { g_scene_sun.azimuth_deg = f[0]; g_scene_sun.elevation_deg = f[1]; g_scene_sun.ambient = f[2]; found = true; }

    if (settings_db_get_floats("prefs.camera", f, 3)) { g_camera_prefs.near_clip = f[0]; g_camera_prefs.far_clip = f[1]; g_camera_prefs.pan_speed = f[2]; found = true; }
    camera_prefs_clamp();

    if (settings_db_get_floats("prefs.light", f, 3)) {
        g_light.min_light = f[0] < 0.0f ? 0.0f : (f[0] > 1.0f ? 1.0f : f[0]);
        g_light.enabled   = (f[1] != 0.0f);
        const int layer   = (int)(f[2] + 0.5f);
        if (layer >= 1 && layer < terrain_max_layers) g_light.layer = layer;
        found = true;
    }

    {
        float sl[2];
        if (settings_db_get_floats("prefs.sunlight", sl, 2)) {
            g_light.sun_intensity = sl[0] < 0.0f ? 0.0f : (sl[0] > 3.0f ? 3.0f : sl[0]);
            g_light.sun_warmth    = sl[1] < -1.0f ? -1.0f : (sl[1] > 1.0f ? 1.0f : sl[1]);
            found = true;
        }
    }

    {
        int grid_cells = 0;
        if (settings_db_get_int("prefs.grid_cells", &grid_cells) && grid_cells >= 1 && grid_cells <= 500) { g_scene_view.grid_half_cells = grid_cells; found = true; }
    }
    {
        float units[2];
        if (settings_db_get_floats("prefs.units", units, 2)) {
            const int import_profile = (int)(units[0] + 0.5f);
            const int export_profile = (int)(units[1] + 0.5f);
            if (import_profile >= 0 && import_profile < unit_profile_count) { g_unit_prefs.import_profile = import_profile; }
            if (export_profile >= 0 && export_profile < unit_profile_zbrush) { g_unit_prefs.export_profile = export_profile; }
            found = true;
        }
    }

    {
        float sky_floats[8];
        if (settings_db_get_floats("prefs.skyatm", sky_floats, 3)) {
            g_sky.sky_brightness = sky_floats[0];
            g_sky.sky_haze = sky_floats[1];
            g_sky.fog_sky_color = (sky_floats[2] != 0.0f);
            found = true;
        }
        if (settings_db_get_floats("prefs.fog", sky_floats, 7)) {
            for (int k = 0; k < 3; k++) {
                g_sky.fog_color[k] = sky_floats[k];
            }
            g_sky.fog_density = sky_floats[3];
            g_sky.fog_height_falloff = sky_floats[4];
            g_sky.fog_base_height = sky_floats[5];
            g_sky.fog_start = sky_floats[6];
            found = true;
        }
        if (settings_db_get_floats("prefs.fog2", sky_floats, 3)) {
            g_sky.fog_max_opacity = sky_floats[0];
            g_sky.fog_sun_scatter = sky_floats[1];
            g_sky.fog_sun_exponent = sky_floats[2];
            found = true;
        }
        if (settings_db_get_floats("prefs.skyview", sky_floats, 4)) {
            g_sky.scene_sky_on = (sky_floats[0] != 0.0f);
            g_sky.scene_fog_on = (sky_floats[1] != 0.0f);
            g_sky.game_sky_on  = (sky_floats[2] != 0.0f);
            g_sky.game_fog_on  = (sky_floats[3] != 0.0f);
            found = true;
        }
    }

    editor_state_apply_from_db();

    if (allow_legacy_migration && !found && load_legacy_ini()) {
        phoenix_info("Prefs", "Migrated %s into the settings database.", "editor_prefs.ini");
        prefs_write();
    }
}

void prefs_load(void)
{
    settings_db_start();     /* finds the exe folder and reads phoenix_editor.pdb if it exists */
    apply_prefs_from_db(true);

    preferences_active_vsync = g_prefs.vsync;
    phoenix_info("Prefs", "Loaded from %s: msaa=%d vsync=%d fxaa=%d theme=%d", settings_db_file_path(), g_prefs.msaa_samples, (int)g_prefs.vsync, (int)g_prefs.fxaa, theme_get_index());
}

void prefs_write(void)
{
    float f[3];
    settings_db_set_int("prefs.msaa",  g_prefs.msaa_samples);
    settings_db_set_int("prefs.vsync", g_prefs.vsync ? 1 : 0);
    settings_db_set_int("prefs.fxaa",  g_prefs.fxaa ? 1 : 0);
    settings_db_set_int("prefs.theme", theme_get_index());
    f[0] = g_scene_bg.top[0];    f[1] = g_scene_bg.top[1];    f[2] = g_scene_bg.top[2];    settings_db_set_floats("prefs.bg.top", f, 3);
    f[0] = g_scene_bg.bottom[0]; f[1] = g_scene_bg.bottom[1]; f[2] = g_scene_bg.bottom[2]; settings_db_set_floats("prefs.bg.bottom", f, 3);
    f[0] = g_scene_sun.azimuth_deg; f[1] = g_scene_sun.elevation_deg; f[2] = g_scene_sun.ambient;     settings_db_set_floats("prefs.sun", f, 3);

    camera_prefs_clamp();
    f[0] = g_camera_prefs.near_clip; f[1] = g_camera_prefs.far_clip; f[2] = g_camera_prefs.pan_speed;     settings_db_set_floats("prefs.camera", f, 3);

    f[0] = g_light.min_light; f[1] = g_light.enabled ? 1.0f : 0.0f; f[2] = (float)g_light.layer;     settings_db_set_floats("prefs.light", f, 3);

    f[0] = g_light.sun_intensity; f[1] = g_light.sun_warmth;     settings_db_set_floats("prefs.sunlight", f, 2);

    settings_db_set_int("prefs.grid_cells", g_scene_view.grid_half_cells);
    f[0] = (float)g_unit_prefs.import_profile; f[1] = (float)g_unit_prefs.export_profile;     settings_db_set_floats("prefs.units", f, 2);

    {
        float sky_floats[8];
        sky_floats[0] = g_sky.sky_brightness;
        sky_floats[1] = g_sky.sky_haze;
        sky_floats[2] = g_sky.fog_sky_color ? 1.0f : 0.0f;
        settings_db_set_floats("prefs.skyatm", sky_floats, 3);
        for (int k = 0; k < 3; k++) {
            sky_floats[k] = g_sky.fog_color[k];
        }
        sky_floats[3] = g_sky.fog_density;
        sky_floats[4] = g_sky.fog_height_falloff;
        sky_floats[5] = g_sky.fog_base_height;
        sky_floats[6] = g_sky.fog_start;
        settings_db_set_floats("prefs.fog", sky_floats, 7);
        sky_floats[0] = g_sky.fog_max_opacity;
        sky_floats[1] = g_sky.fog_sun_scatter;
        sky_floats[2] = g_sky.fog_sun_exponent;
        settings_db_set_floats("prefs.fog2", sky_floats, 3);
        sky_floats[0] = g_sky.scene_sky_on ? 1.0f : 0.0f;
        sky_floats[1] = g_sky.scene_fog_on ? 1.0f : 0.0f;
        sky_floats[2] = g_sky.game_sky_on ? 1.0f : 0.0f;
        sky_floats[3] = g_sky.game_fog_on ? 1.0f : 0.0f;
        settings_db_set_floats("prefs.skyview", sky_floats, 4);
    }

    editor_state_write_to_db();

    if (!settings_db_save()) {
        phoenix_error("Prefs", "Could not write %s (is the exe folder writable?)", settings_db_file_path());
        return;
    }
    phoenix_info("Prefs", "Saved to %s: msaa=%d vsync=%d fxaa=%d theme=%d", settings_db_file_path(), g_prefs.msaa_samples, (int)g_prefs.vsync, (int)g_prefs.fxaa, theme_get_index());
}

void prefs_save(void)
{
    if (!preferences_boot_done) return;
    prefs_write();
}

/* ------------------------------------------------------------------------- */
/* sample face_type                                                               */
/* ------------------------------------------------------------------------- */

int max_supported_msaa(void)
{
    if (render_context.physical_device == VK_NULL_HANDLE) return 1;
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(render_context.physical_device, &p);
    /* the offscreen pass uses this face_type for BOTH the color and the depth attachment */
    VkSampleCountFlags ok = p.limits.framebufferColorSampleCounts & p.limits.framebufferDepthSampleCounts;
    if (ok & VK_SAMPLE_COUNT_8_BIT) return 8;
    if (ok & VK_SAMPLE_COUNT_4_BIT) return 4;
    if (ok & VK_SAMPLE_COUNT_2_BIT) return 2;
    return 1;
}

VkSampleCountFlagBits get_msaa_sample_count(void)
{
    if (preferences_active_msaa < 0) {
        if (render_context.physical_device == VK_NULL_HANDLE) return VK_SAMPLE_COUNT_1_BIT;   /* too early, do not cache */
        int want = g_prefs.msaa_samples;
        int cap  = max_supported_msaa();
        preferences_active_msaa = (want > cap) ? cap : want;
        if (preferences_active_msaa < 2) preferences_active_msaa = 1;
        set_engine_scene_msaa(preferences_active_msaa);   /* the engine's grid pipeline must use the same count as the scene pass */
        phoenix_info("Prefs", "MSAA active: %dx (requested %dx, GPU max %dx)", preferences_active_msaa, want, cap);
    }
    switch (preferences_active_msaa) {
        case 8:  return VK_SAMPLE_COUNT_8_BIT;
        case 4:  return VK_SAMPLE_COUNT_4_BIT;
        case 2:  return VK_SAMPLE_COUNT_2_BIT;
        default: return VK_SAMPLE_COUNT_1_BIT;
    }
}

/* ------------------------------------------------------------------------- */
/* window                                                                     */
/* ------------------------------------------------------------------------- */

bool         preferences_window_open       = false;
editor_prefs preferences_pending;
bool         preferences_real_down  = false;
bool         preferences_input_hidden = false;   /* begin_frame hid the mouse button this frame, end_frame must give it back */
bool         preferences_prev_down  = false;
bool         preferences_pressed    = false;
bool         preferences_prev_esc   = false;
bool         preferences_saved_note = false;
int          preferences_drag_id    = -1;
bool         preferences_look_dirty = false;
bool         preferences_window_dragging = false;
int          preferences_window_x = -1;
int          preferences_window_y = -1;
int          preferences_window_drag_offset_x = 0;
int          preferences_window_drag_offset_y = 0;
int          preferences_tab = 0;           /* 0 general, 1 grid, 2 camera, 3 import / export, 4 build */

/* Late re-apply. prefs_load() runs before the renderer / editor elements exist, so later init code can overwrite
   what it loaded. Call this once everything is up: it re-reads the file and sets every option again.
   preferences_active_vsync / preferences_active_msaa are NOT touched: they record what the renderer really started with. */
void update_system_preference(void)
{
    settings_db_reload();
    apply_prefs_from_db(false);
    preferences_boot_done = true;

    if (preferences_window_open) preferences_pending = g_prefs;

    phoenix_info("Prefs", "Re-applied from %s (file existed=%d): theme=%d camera=%.3f/%.0f/%.1f msaa=%d vsync=%d fxaa=%d",
                 settings_db_file_path(), (int)settings_db_file_existed(), theme_get_index(),
                 g_camera_prefs.near_clip, g_camera_prefs.far_clip, g_camera_prefs.pan_speed,
                 g_prefs.msaa_samples, (int)g_prefs.vsync, (int)g_prefs.fxaa);
}

void open_preferences_window(void)
{
    export_window_open          = false;                    /* two modal windows at once would fight over the mouse */
    preferences_pending         = g_prefs;
    preferences_window_open     = true;
    phoenix_info("Prefs", "open_preferences_window: window_open=%d", (int)preferences_window_open);
    preferences_prev_down       = true;
    preferences_saved_note      = false;
    preferences_pressed         = false;
    preferences_drag_id         = -1;
    preferences_window_dragging = false;
    preferences_tab             = 0;
    preferences_window_x        = -1;
    preferences_window_y        = -1;
}

bool is_preferences_window_open(void) { return preferences_window_open; }

void preferences_begin_frame(system_element_context* ui_ctx)
{
    if (!preferences_window_open || !ui_ctx) { return; }
    preferences_real_down = ui_ctx->input.mouse_down;
    preferences_pressed   = preferences_real_down && !preferences_prev_down;
    preferences_prev_down = preferences_real_down;
    ui_ctx->input.mouse_down = false;      /* modal: dock, toolbar and menubar see no button while we are open */
    preferences_input_hidden = true;
}

void preferences_end_frame(system_element_context* ui_ctx)
{
    if (ui_ctx && preferences_input_hidden) {
        ui_ctx->input.mouse_down = preferences_real_down;
        preferences_input_hidden = false;
    }
}

bool in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < (x + w) && py >= y && py < (y + h);
}

int text_w(const char* s) { return (int)strlen(s) * ui_font_w; }

/* status text goes to the statusbar (draw_system_statusbar.c reads status_bar_text_buffer). */
void set_preferences_status(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(status_bar_text_buffer, sizeof(status_bar_text_buffer), format, args);
    va_end(args);
}

/* every color comes from system_theme: face = tab_sub_menubar_button, selected = tab_sub_menubar_button_active (accent),
   hover = the shared hover step, text = button_text. the face is always dark enough (or accent) for that text. */
bool pref_button(VkCommandBuffer cmd, system_element_context* ui_element, const char* label,
                        int x, int y, int w, int h, bool obj_vert_selected, bool enabled)
{
    bool hover = enabled && in_rect(ui_element->input.mouse_x, ui_element->input.mouse_y, x, y, w, h);

    system_color bg;
    system_color fg;
    if (!enabled) {
        bg = system_theme.tab_item;
        fg = get_theme_disabled_color();
    } else {
        bg = obj_vert_selected ? system_theme.tab_sub_menubar_button_active : system_theme.tab_sub_menubar_button;
        if (hover) bg = process_theme_color(bg, system_theme.system_hover_delta);
        fg = system_theme.button_text;
    }

    push_rounded_quad(cmd, (float)x, (float)y, (float)w, (float)h, bg, 3, corner_all);
    draw_text_element(cmd, ui_element, label, x + (w - text_w(label)) / 2, y + (h - ui_font_h) / 2, fg);
    return hover && preferences_pressed;
}

/* Horizontal slider. Edits *val live. disp_scale only changes the number shown (255 for colours, 100 for %). */
void pref_slider(VkCommandBuffer cmd, system_element_context* ui_element, int id, const char* label, int label_w,
                        int x, int y, int w, float* val, float lo, float hi, float disp_scale)
{
    const int h  = 18;
    const int tx = x + label_w;
    const int tw = w - label_w - 40;

    draw_text_element(cmd, ui_element, label, x, y + (h - ui_font_h) / 2, system_theme.system_text);

    bool over = in_rect(ui_element->input.mouse_x, ui_element->input.mouse_y, tx - 4, y, tw + 8, h);
    if (over && preferences_pressed) preferences_drag_id = id;
    if (preferences_drag_id == id) {
        if (!preferences_real_down) {
            preferences_drag_id = -1;
            preferences_look_dirty = true;
        } else {
            float t = (float)(ui_element->input.mouse_x - tx) / (float)tw;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            *val = lo + t * (hi - lo);
        }
    }

    float t = (*val - lo) / (hi - lo);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    push_rounded_quad(cmd, (float)tx, (float)(y + h / 2 - 2), (float)tw, 4.0f, system_theme.tab_element, 2, corner_all);
    if (tw * t >= 4.0f) {
        push_rounded_quad(cmd, (float)tx, (float)(y + h / 2 - 2), (float)tw * t, 4.0f, system_theme.system_accent, 2, corner_all);
    }
    bool active = over || preferences_drag_id == id;
    push_rounded_quad(cmd, (float)tx + (float)tw * t - 4.0f, (float)(y + h / 2 - 6), 8.0f, 12.0f,
                      active ? system_theme.system_active_text : system_theme.system_text, 3, corner_all);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", (int)(*val * disp_scale + 0.5f));
    draw_text_element(cmd, ui_element, buf, tx + tw + 10, y + (h - ui_font_h) / 2, system_theme.system_text);
}

/* runs one of the tools next to the exe (10_system_build, 09_system_installer) and waits for it. returns its exit code, or -1 when it could not start */
int run_build_tool(const char* tool_file, const char* arguments)
{
    char tool_path[MAX_PATH];
    if (!settings_db_make_path(tool_file, tool_path, sizeof(tool_path))) {
        return -1;
    }
    char command[MAX_PATH * 2 + 8];
    snprintf(command, sizeof(command), "\"%s\" %s", tool_path, arguments);

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
        return -1;
    }
    WaitForSingleObject(process.hProcess, 60000);
    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return (int)exit_code;
}

/* Preferences > Build: 01_bin -> game_build next to the exe, without the editor. the installer is copied in beside it. */
void build_game_folder(void)
{
    char output_folder[MAX_PATH];
    char installer_from[MAX_PATH];
    char installer_to[MAX_PATH + 32];
    char arguments[MAX_PATH + 8];
    if (!settings_db_make_path("game_build", output_folder, sizeof(output_folder))) {
        set_preferences_status("Build: could not find the exe folder.");
        return;
    }
    snprintf(arguments, sizeof(arguments), "\"%s\"", output_folder);
    const int result = run_build_tool("phoenix_build.exe", arguments);
    if (result < 0) {
        set_preferences_status("Build: phoenix_build.exe was not found next to the editor. Run make all.");
        return;
    }
    if (result != 0) {
        set_preferences_status("Build finished with errors (exit code %d).", result);
        return;
    }
    if (settings_db_make_path("phoenix_installer.exe", installer_from, sizeof(installer_from))) {
        snprintf(installer_to, sizeof(installer_to), "%s\\phoenix_installer.exe", output_folder);
        CopyFileA(installer_from, installer_to, FALSE);
    }
    set_preferences_status("Build done: %s", output_folder);
}

void draw_preferences_window(VkCommandBuffer cmd, system_element_context* ui_element, int screen_w, int screen_h)
{
    if (!preferences_window_open || !ui_element) return;

    const int W = 500;
    int H = 620;
    if (preferences_tab == 5) {
        H = 800;                                     /* the Sky / Fog tab has the most controls */
    }
    if (H > screen_h - 8) {
        H = screen_h - 8;
    }
    if (preferences_window_x < 0) {
        preferences_window_x = (screen_w - W) / 2;
    }
    if (preferences_window_y < 0) {
        preferences_window_y = (screen_h - H) / 2;
    }
    if (preferences_window_y + H > screen_h) {
        preferences_window_y = screen_h - H;
    }
    const int X = preferences_window_x;
    int Y = preferences_window_y;
    if (Y < 0) Y = 0;
    const int pad = 18;
    const int row_h = 22;
    system_color sep_color = system_theme.menu_hort_separator;
    sep_color.alpha = 1.0f;

    /* Esc closes */
    bool esc = check_window_focus() && ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0);
    if (esc && !preferences_prev_esc) preferences_window_open = false;
    preferences_prev_esc = esc;

    const bool title_hover = in_rect(ui_element->input.mouse_x, ui_element->input.mouse_y,
                                     X, Y, W, 28);
    if (title_hover && preferences_pressed) {
        preferences_window_dragging = true;
        preferences_window_drag_offset_x = ui_element->input.mouse_x - X;
        preferences_window_drag_offset_y = ui_element->input.mouse_y - Y;
    }
    if (!preferences_real_down) {
        preferences_window_dragging = false;
    }
    if (preferences_window_dragging) {
        preferences_window_x = ui_element->input.mouse_x - preferences_window_drag_offset_x;
        preferences_window_y = ui_element->input.mouse_y - preferences_window_drag_offset_y;
        if (preferences_window_x < 0) {
            preferences_window_x = 0;
        }
        if (preferences_window_y < 0) {
            preferences_window_y = 0;
        }
        if (preferences_window_x + W > screen_w) {
            preferences_window_x = screen_w - W;
        }
        if (preferences_window_y + H > screen_h) {
            preferences_window_y = screen_h - H;
        }
    }

    /* frame: border, body, title bar */
    push_rounded_quad(cmd, (float)(X - 1), (float)(Y - 1), (float)(W + 2), (float)(H + 2), system_theme.system_border, 6, corner_all);
    push_rounded_quad(cmd, (float)X, (float)Y, (float)W, (float)H, system_theme.tab_content_window, 6, corner_all);
    push_rounded_quad(cmd, (float)X, (float)Y, (float)W, 28.0f, system_theme.tab_sub_menubar, 6, corner_top);
    draw_text_element(cmd, ui_element, "Preferences", X + pad, Y + (28 - ui_font_h) / 2, system_theme.system_active_text);

    /* close [x] */
    if (pref_button(cmd, ui_element, "x", X + W - 28, Y + 4, 20, 20, false, true)) preferences_window_open = false;

    int y = Y + 28 + pad;
    const int label_x = X + pad;
    const int ctl_x   = X + 170;
    const int cap     = max_supported_msaa();
    int bx            = ctl_x;

    /* ---- tabs ---- */
    const char* tab_names[6] = { "General", "Grid", "Camera", "Import / Export", "Build", "Sky / Fog" };
    const int tab_widths[6]  = { 68, 48, 64, 112, 52, 78 };
    int tab_x = label_x;
    for (int i = 0; i < 6; i++) {
        if (pref_button(cmd, ui_element, tab_names[i], tab_x, y - pad + 6, tab_widths[i], row_h, preferences_tab == i, true)) {
            preferences_tab = i;
        }
        tab_x += tab_widths[i] + 6;
    }
    y += row_h - pad + 14;
    push_quad_element(cmd, (float)label_x, (float)(y - 6), (float)(W - pad * 2), 1.0f, sep_color);
    y += 6;

    if (preferences_tab == 0) {
    /* ---- Anti-aliasing ---- */
    draw_text_element(cmd, ui_element, "ANTI-ALIASING", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f,
                      sep_color);
    y += ui_font_h + 14;

    /* MSAA: Off / 2x / 4x / 8x. Applies to the 3D grid, scene objects (cube, camera, terrain) and the UI pass. */
    draw_text_element(cmd, ui_element, "MSAA", label_x, y + 3, system_theme.system_text);
    bx = ctl_x;
    if (pref_button(cmd, ui_element, "Off", bx, y, 52, row_h, preferences_pending.msaa_samples == 1, true)) { preferences_pending.msaa_samples = 1; g_prefs = preferences_pending; prefs_save(); preferences_saved_note = true; set_preferences_status("MSAA: Off (restart the editor to apply)."); }
    bx += 58;
    if (pref_button(cmd, ui_element, "2x", bx, y, 52, row_h, preferences_pending.msaa_samples == 2, cap >= 2)) { preferences_pending.msaa_samples = 2; g_prefs = preferences_pending; prefs_save(); preferences_saved_note = true; set_preferences_status("MSAA: 2x (restart the editor to apply)."); }
    bx += 58;
    if (pref_button(cmd, ui_element, "4x", bx, y, 52, row_h, preferences_pending.msaa_samples == 4, cap >= 4)) { preferences_pending.msaa_samples = 4; g_prefs = preferences_pending; prefs_save(); preferences_saved_note = true; set_preferences_status("MSAA: 4x (restart the editor to apply)."); }
    bx += 58;
    if (pref_button(cmd, ui_element, "8x", bx, y, 52, row_h, preferences_pending.msaa_samples == 8, cap >= 8)) { preferences_pending.msaa_samples = 8; g_prefs = preferences_pending; prefs_save(); preferences_saved_note = true; set_preferences_status("MSAA: 8x (restart the editor to apply)."); }
    y += row_h + 10;

    /* FXAA / TAA: need an offscreen scene target to read from. Shown, but not selectable yet. */
    /* FXAA: post pass over the resolved scene image. Applies live, no restart. */
    draw_text_element(cmd, ui_element, "FXAA", label_x, y + 3, system_theme.system_text);
    if (pref_button(cmd, ui_element, "On",  ctl_x,      y, 52, row_h,  g_prefs.fxaa, true)) { g_prefs.fxaa = true;  preferences_pending.fxaa = true;  prefs_save(); set_preferences_status("FXAA: on (live)."); }
    if (pref_button(cmd, ui_element, "Off", ctl_x + 58, y, 52, row_h, !g_prefs.fxaa, true)) { g_prefs.fxaa = false; preferences_pending.fxaa = false; prefs_save(); set_preferences_status("FXAA: off (live)."); }
    draw_text_element(cmd, ui_element, "live, no restart", ctl_x + 122, y + 3, get_theme_hint_color());
    y += row_h + 10;

    draw_text_element(cmd, ui_element, "TAA", label_x, y + 3, get_theme_hint_color());
    pref_button(cmd, ui_element, "Off", ctl_x, y, 52, row_h, true, false);
    draw_text_element(cmd, ui_element, "not implemented yet", ctl_x + 64, y + 3, get_theme_hint_color());
    y += row_h + 22;

    /* ---- Display ---- */
    draw_text_element(cmd, ui_element, "DISPLAY", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f,
                      sep_color);
    y += ui_font_h + 14;

    draw_text_element(cmd, ui_element, "V-Sync", label_x, y + 3, system_theme.system_text);
    if (pref_button(cmd, ui_element, "On",  ctl_x,       y, 52, row_h,  preferences_pending.vsync, true)) { preferences_pending.vsync = true;  g_prefs = preferences_pending; prefs_save(); preferences_saved_note = true; set_preferences_status("V-Sync: on (restart the editor to apply)."); }
    if (pref_button(cmd, ui_element, "Off", ctl_x + 58, y, 52, row_h, !preferences_pending.vsync, true)) { preferences_pending.vsync = false; g_prefs = preferences_pending; prefs_save(); preferences_saved_note = true; set_preferences_status("V-Sync: off (restart the editor to apply)."); }
    y += row_h + 22;

    /* ---- Theme: applies live, saved when the mouse is released ---- */
    draw_text_element(cmd, ui_element, "THEME", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f,
                      sep_color);
    y += ui_font_h + 14;

    draw_text_element(cmd, ui_element, "Editor", label_x, y + 3, system_theme.system_text);
    bx = ctl_x;
    for (int i = 0; i < theme_count(); i++) {
        if (pref_button(cmd, ui_element, theme_name(i), bx, y, 70, row_h, theme_get_index() == i, true)) {
            theme_set(i);
            prefs_save();   /* immediately, like FXAA: do not rely on a later mouse release */
            set_preferences_status("Theme: %s.", theme_name(i));
        }
        bx += 76;
    }
    y += row_h + 22;

    /* ---- Viewport look: live, nothing here needs a restart ---- */
    draw_text_element(cmd, ui_element, "VIEWPORT BACKGROUND", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f,
                      sep_color);
    y += ui_font_h + 14;

    draw_text_element(cmd, ui_element, "Preset", label_x, y + 3, system_theme.system_text);
    bx = ctl_x;
    for (int i = 0; i < scene_look_preset_count; i++) {
        if (pref_button(cmd, ui_element, scene_look_preset_name(i), bx, y, 70, row_h, false, true)) {
            scene_look_apply_preset(i);
            preferences_look_dirty = true;
            set_preferences_status("Viewport background preset: %s.", scene_look_preset_name(i));
        }
        bx += 76;
    }
    y += row_h + 10;

    const int col_w = (W - pad * 2 - 20) / 2;
    const int cx0 = label_x;
    const int cx1 = label_x + col_w + 20;
    draw_text_element(cmd, ui_element, "Top color",    cx0, y, system_theme.system_text);
    draw_text_element(cmd, ui_element, "Bottom color", cx1, y, system_theme.system_text);
    y += ui_font_h + 6;

    for (int i = 0; i < 3; i++) {
        const char* channel_name = "R";
        if (i == 1) channel_name = "G";
        if (i == 2) channel_name = "B";
        pref_slider(cmd, ui_element, i,     channel_name, 16, cx0, y, col_w, &g_scene_bg.top[i],    0.0f, 1.0f, 255.0f);
        pref_slider(cmd, ui_element, 3 + i, channel_name, 16, cx1, y, col_w, &g_scene_bg.bottom[i], 0.0f, 1.0f, 255.0f);
        y += 20;
    }
    y += 10;

    draw_text_element(cmd, ui_element, "Sun, ambient and terrain light: Lighting tab (next to Inspector).", cx0, y, get_theme_hint_color());
    y += ui_font_h + 6;
    }

    if (preferences_tab == 1) {
    /* ---- Grid: one cell is 4 m, the size of a cube / plane ---- */
    draw_text_element(cmd, ui_element, "GRID", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, sep_color);
    y += ui_font_h + 14;

    draw_text_element(cmd, ui_element, "Show grid", label_x, y + 3, system_theme.system_text);
    if (pref_button(cmd, ui_element, "On",  ctl_x,      y, 52, row_h,  g_scene_view.show_grid, true)) { g_scene_view.show_grid = true;  prefs_save(); set_preferences_status("Grid: on."); }
    if (pref_button(cmd, ui_element, "Off", ctl_x + 58, y, 52, row_h, !g_scene_view.show_grid, true)) { g_scene_view.show_grid = false; prefs_save(); set_preferences_status("Grid: off."); }
    y += row_h + 10;

    draw_text_element(cmd, ui_element, "Grid size", label_x, y + 3, system_theme.system_text);
    const int grid_half_choices[5] = { 16, 32, 50, 125, 250 };
    const char* grid_size_names[5] = { "128 m", "256 m", "400 m", "1 km", "2 km" };
    bx = ctl_x;
    for (int i = 0; i < 5; i++) {
        if (pref_button(cmd, ui_element, grid_size_names[i], bx, y, 56, row_h, g_scene_view.grid_half_cells == grid_half_choices[i], true)) {
            g_scene_view.grid_half_cells = grid_half_choices[i];
            prefs_save();
            set_preferences_status("Grid size: %s across.", grid_size_names[i]);
        }
        bx += 62;
    }
    y += row_h + 14;

    draw_text_element(cmd, ui_element, "One cell is 4 m x 4 m: the size of a cube and a plane.", label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "Brighter lines every 16 m (4 cells).", label_x, y, get_theme_hint_color());
    }

    if (preferences_tab == 2) {
    /* ---- Camera: Scene view clip planes and pan speed ---- */
    draw_text_element(cmd, ui_element, "CAMERA", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, sep_color);
    y += ui_font_h + 14;

    pref_slider(cmd, ui_element, 9,  "Near clip (cm)", 120, label_x, y, W - pad * 2, &g_camera_prefs.near_clip,  0.01f,  2.0f,     100.0f);
    y += 22;
    pref_slider(cmd, ui_element, 10, "Far clip (mesh)",   120, label_x, y, W - pad * 2, &g_camera_prefs.far_clip,   200.0f, 100000.0f, 1.0f);
    y += 22;
    pref_slider(cmd, ui_element, 11, "Pan speed",      120, label_x, y, W - pad * 2, &g_camera_prefs.pan_speed,  1.0f,   30.0f,    1.0f);
    y += 22;
    camera_prefs_clamp();
    }

    if (preferences_tab == 5) {
    /* ---- Sky / Fog: the procedural sky dome, the height fog and the fog volumes. The Sky and Fog buttons in the Scene and Game tabs switch them on and off. ---- */
    draw_text_element(cmd, ui_element, "SKY (procedural: follows the directional light)", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, sep_color);
    y += ui_font_h + 14;

    if (pref_button(cmd, ui_element, "Create Sky Dome", label_x, y, 150, row_h, false, true)) {
        create_system_sky_dome(render_context.logical_device, render_context.physical_device);
    }
    draw_text_element(cmd, ui_element, "Turn the sun with the directional light.", label_x + 160, y + 3, get_theme_hint_color());
    y += row_h + 8;
    pref_slider(cmd, ui_element, 20, "Brightness", 90, label_x, y, W - pad * 2, &g_sky.sky_brightness, 0.0f, 2.0f, 100.0f);
    y += 22;
    pref_slider(cmd, ui_element, 21, "Haze",       90, label_x, y, W - pad * 2, &g_sky.sky_haze,       0.0f, 1.0f, 100.0f);
    y += 28;

    draw_text_element(cmd, ui_element, "FOG", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, sep_color);
    y += ui_font_h + 14;

    draw_text_element(cmd, ui_element, "Fog color", label_x, y + 3, system_theme.system_text);
    if (pref_button(cmd, ui_element, "From sky", ctl_x, y, 80, row_h, g_sky.fog_sky_color, true)) { g_sky.fog_sky_color = true;  preferences_look_dirty = true; }
    if (pref_button(cmd, ui_element, "My color", ctl_x + 86, y, 80, row_h, !g_sky.fog_sky_color, true)) { g_sky.fog_sky_color = false; preferences_look_dirty = true; }
    y += row_h + 6;
    if (!g_sky.fog_sky_color) {
        pref_slider(cmd, ui_element, 26, "Red",   90, label_x, y, W - pad * 2, &g_sky.fog_color[0], 0.0f, 1.0f, 255.0f);
        y += 20;
        pref_slider(cmd, ui_element, 27, "Green", 90, label_x, y, W - pad * 2, &g_sky.fog_color[1], 0.0f, 1.0f, 255.0f);
        y += 20;
        pref_slider(cmd, ui_element, 28, "Blue",  90, label_x, y, W - pad * 2, &g_sky.fog_color[2], 0.0f, 1.0f, 255.0f);
        y += 20;
    }
    pref_slider(cmd, ui_element, 29, "Density",  90, label_x, y, W - pad * 2, &g_sky.fog_density,        0.0f, 0.1f,   1000.0f);
    y += 20;
    pref_slider(cmd, ui_element, 30, "Height",   90, label_x, y, W - pad * 2, &g_sky.fog_height_falloff, 0.0f, 0.2f,   1000.0f);
    y += 20;
    pref_slider(cmd, ui_element, 31, "Start",    90, label_x, y, W - pad * 2, &g_sky.fog_start,          0.0f, 500.0f, 1.0f);
    y += 20;
    pref_slider(cmd, ui_element, 36, "Opacity",  90, label_x, y, W - pad * 2, &g_sky.fog_max_opacity,    0.0f, 1.0f,   100.0f);
    y += 20;
    pref_slider(cmd, ui_element, 37, "Sun glow", 90, label_x, y, W - pad * 2, &g_sky.fog_sun_scatter,    0.0f, 2.0f,   100.0f);
    y += 20;
    pref_slider(cmd, ui_element, 38, "Sun size", 90, label_x, y, W - pad * 2, &g_sky.fog_sun_exponent,   1.0f, 64.0f,  1.0f);
    y += 28;

    draw_text_element(cmd, ui_element, "FOG VOLUMES (a box filled with fog)", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, sep_color);
    y += ui_font_h + 14;

    if (pref_button(cmd, ui_element, "Create Fog Volume", label_x, y, 170, row_h, false, true)) {
        create_system_fog_volume((editor_vec3){ 0.0f, 8.0f, 0.0f });
    }
    draw_text_element(cmd, ui_element, "Resize the box with the Scale tool.", label_x + 180, y + 3, get_theme_hint_color());
    y += row_h + 8;

    prune_sky_volumes();
    if (g_sky.volume_count > 0) {
        int shown = 0;
        for (int i = 0; i < g_sky.volume_count; i++) {
            if (g_sky.volume_object_ids[i] == g_scene_registry.selected_object_id) {
                shown = i;
            }
        }
        char volume_label[40];
        snprintf(volume_label, sizeof(volume_label), "Volume %d type", shown + 1);
        draw_text_element(cmd, ui_element, volume_label, label_x, y + 3, system_theme.system_text);
        if (pref_button(cmd, ui_element, "Cheap", ctl_x, y, 80, row_h, g_sky.volume_type[shown] == fog_volume_cheap, true)) { g_sky.volume_type[shown] = fog_volume_cheap; }
        if (pref_button(cmd, ui_element, "Good",  ctl_x + 86, y, 80, row_h, g_sky.volume_type[shown] == fog_volume_good, true)) { g_sky.volume_type[shown] = fog_volume_good; }
        y += row_h + 6;
        pref_slider(cmd, ui_element, 32, "Density",  90, label_x, y, W - pad * 2, &g_sky.volume_density[shown],     0.0f, 0.3f, 1000.0f);
        y += 20;
        pref_slider(cmd, ui_element, 33, "Edge soft", 90, label_x, y, W - pad * 2, &g_sky.volume_softness[shown],   0.02f, 1.0f, 100.0f);
        y += 20;
        pref_slider(cmd, ui_element, 34, "Noise",    90, label_x, y, W - pad * 2, &g_sky.volume_noise_scale[shown], 0.5f, 12.0f, 10.0f);
        y += 20;
        pref_slider(cmd, ui_element, 35, "Red",      90, label_x, y, W - pad * 2, &g_sky.volume_color[shown][0],    0.0f, 1.0f, 255.0f);
        y += 20;
        pref_slider(cmd, ui_element, 39, "Green",    90, label_x, y, W - pad * 2, &g_sky.volume_color[shown][1],    0.0f, 1.0f, 255.0f);
        y += 20;
        pref_slider(cmd, ui_element, 40, "Blue",     90, label_x, y, W - pad * 2, &g_sky.volume_color[shown][2],    0.0f, 1.0f, 255.0f);
        y += 20;
        draw_text_element(cmd, ui_element, "Cheap = one soft pass. Good = moving cloud noise, thicker low, lit by the sun.", label_x, y, get_theme_hint_color());
    } else {
        draw_text_element(cmd, ui_element, "No fog volume yet.", label_x, y, get_theme_hint_color());
    }
    }
    if (preferences_tab == 4) {
    /* ---- Build: a game folder without the editor, and its installer ---- */
    draw_text_element(cmd, ui_element, "GAME BUILD", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, sep_color);
    y += ui_font_h + 14;

    if (pref_button(cmd, ui_element, "Build game folder", label_x, y, 170, row_h, false, true)) {
        build_game_folder();
    }
    char build_folder[MAX_PATH];
    if (settings_db_make_path("game_build", build_folder, sizeof(build_folder))) {
        if (pref_button(cmd, ui_element, "Open folder", label_x + 180, y, 120, row_h, false, true)) {
            ShellExecuteA(NULL, "open", build_folder, NULL, NULL, SW_SHOWNORMAL);
        }
    }
    y += row_h + 10;
    draw_text_element(cmd, ui_element, "Copies the engine, game, loader, shaders and assets into game_build", label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "next to the editor. The editor and its settings file stay behind.", label_x, y, get_theme_hint_color());
    y += ui_font_h + 22;

    draw_text_element(cmd, ui_element, "INSTALLER", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, sep_color);
    y += ui_font_h + 14;
    draw_text_element(cmd, ui_element, "game_build\\phoenix_installer.exe installs that folder to", label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "%LOCALAPPDATA%\\phoenix_game and writes an uninstall.bat.", label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "The game still needs the editor dll to start: no-editor boot is next.", label_x, y, get_theme_hint_color());
    }

    if (preferences_tab == 3) {
    /* ---- Import / Export: models are sized in meters inside the engine (a cube is 4 m) ---- */
    draw_text_element(cmd, ui_element, "IMPORT (.fbx, .obj)", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, sep_color);
    y += ui_font_h + 14;

    draw_text_element(cmd, ui_element, "Incoming sizes", label_x, y + 3, system_theme.system_text);
    bx = ctl_x;
    for (int i = 0; i < unit_profile_count; i++) {
        if (pref_button(cmd, ui_element, get_unit_profile_name(i), bx, y, 72, row_h, g_unit_prefs.import_profile == i, true)) {
            g_unit_prefs.import_profile = i;
            prefs_save();
            set_preferences_status("Import sizes: %s.", get_unit_profile_name(i));
        }
        bx += 78;
    }
    y += row_h + 10;
    draw_text_element(cmd, ui_element, "Phoenix / Unity: .obj numbers are meters. Maya / ZBrush: centimeters.", label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "Phoenix, Maya and Unity trust the units stored in an .fbx file.", label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "ZBrush ignores them and reads every .fbx number as centimeters.", label_x, y, get_theme_hint_color());
    y += ui_font_h + 22;

    draw_text_element(cmd, ui_element, "EXPORT (.fbx, .obj)", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f, sep_color);
    y += ui_font_h + 14;

    draw_text_element(cmd, ui_element, "Outgoing sizes", label_x, y + 3, system_theme.system_text);
    bx = ctl_x;
    for (int i = 0; i < unit_profile_zbrush; i++) {
        if (pref_button(cmd, ui_element, get_unit_profile_name(i), bx, y, 72, row_h, g_unit_prefs.export_profile == i, true)) {
            g_unit_prefs.export_profile = i;
            prefs_save();
            set_preferences_status("Export sizes: %s.", get_unit_profile_name(i));
        }
        bx += 78;
    }
    y += row_h + 10;
    draw_text_element(cmd, ui_element, "Phoenix and Unity: meters (a 4 m cube exports 4 units wide).", label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "Maya: centimeters (the same cube exports 400 units wide).", label_x, y, get_theme_hint_color());
    y += ui_font_h + 4;
    draw_text_element(cmd, ui_element, "The unit scale is also written into the .fbx header.", label_x, y, get_theme_hint_color());
    }

    if (preferences_look_dirty && !preferences_real_down) { prefs_save(); preferences_look_dirty = false; }

    /* ---- status line ---- */
    int active_msaa_shown = (preferences_active_msaa > 0) ? preferences_active_msaa : g_prefs.msaa_samples;
    int saved_msaa_effective = (g_prefs.msaa_samples > cap) ? cap : g_prefs.msaa_samples;
    bool needs_restart = (saved_msaa_effective != active_msaa_shown) || (g_prefs.vsync != preferences_active_vsync);
    if (preferences_tab != 0) {
        /* nothing to restart on the other tabs */
    } else if (preferences_pending.msaa_samples > cap) {
        draw_text_element(cmd, ui_element, "This GPU does not support that sample face_type.", label_x, Y + H - 62, system_theme.menu_active_text);
    } else if (needs_restart || preferences_saved_note) {
        draw_text_element(cmd, ui_element, "Saved. Restart the editor to apply.", label_x, Y + H - 62, system_theme.menu_active_text);
    } else {
        draw_text_element(cmd, ui_element, "MSAA and V-Sync take effect after a restart.", label_x, Y + H - 62, get_theme_hint_color());
    }

    /* ---- footer ---- */
    const int fy = Y + H - pad - 26;
    if (pref_button(cmd, ui_element, "Apply", X + W - pad - 190, fy, 90, 26, false, true)) {
        g_prefs = preferences_pending;
        prefs_save();
        preferences_saved_note = true;
        set_preferences_status("Preferences saved.");
    }
    if (pref_button(cmd, ui_element, "Close", X + W - pad - 90, fy, 90, 26, false, true)) {
        preferences_window_open = false;
    }
}