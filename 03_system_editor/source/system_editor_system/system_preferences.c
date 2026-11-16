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
    preferences_pending         = g_prefs;
    preferences_window_open     = true;
    phoenix_info("Prefs", "open_preferences_window: window_open=%d", (int)preferences_window_open);
    preferences_prev_down       = true;
    preferences_saved_note      = false;
    preferences_pressed         = false;
    preferences_drag_id         = -1;
    preferences_window_dragging = false;
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

void draw_preferences_window(VkCommandBuffer cmd, system_element_context* ui_element, int screen_w, int screen_h)
{
    if (!preferences_window_open || !ui_element) return;

    const int W = 500, H = 620;
    if (preferences_window_x < 0) {
        preferences_window_x = (screen_w - W) / 2;
    }
    if (preferences_window_y < 0) {
        preferences_window_y = (screen_h - H) / 2;
    }
    const int X = preferences_window_x;
    int Y = preferences_window_y;
    if (Y < 0) Y = 0;
    const int pad = 18;
    const int row_h = 22;
    system_color sep_color = system_theme.menu_hort_separator;
    sep_color.alpha = 1.0f;

    /* Esc closes */
    bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
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

    /* ---- Anti-aliasing ---- */
    draw_text_element(cmd, ui_element, "ANTI-ALIASING", label_x, y, system_theme.menu_active_text);
    push_quad_element(cmd, (float)label_x, (float)(y + ui_font_h + 4), (float)(W - pad * 2), 1.0f,
                      sep_color);
    y += ui_font_h + 14;

    /* MSAA: Off / 2x / 4x / 8x. Applies to the 3D grid, scene objects (cube, camera, terrain) and the UI pass. */
    const int cap = max_supported_msaa();
    draw_text_element(cmd, ui_element, "MSAA", label_x, y + 3, system_theme.system_text);
    int bx = ctl_x;
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

    /* ---- Camera: Scene view clip planes and pan speed (until there is a Camera window) ---- */
    y += 10;
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

    if (preferences_look_dirty && !preferences_real_down) { prefs_save(); preferences_look_dirty = false; }

    /* ---- status line ---- */
    int active_msaa_shown = (preferences_active_msaa > 0) ? preferences_active_msaa : g_prefs.msaa_samples;
    int saved_msaa_effective = (g_prefs.msaa_samples > cap) ? cap : g_prefs.msaa_samples;
    bool needs_restart = (saved_msaa_effective != active_msaa_shown) || (g_prefs.vsync != preferences_active_vsync);
    if (preferences_pending.msaa_samples > cap) {
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