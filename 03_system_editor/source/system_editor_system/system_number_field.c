#include "system_globals.h"


extern uint32_t g_ui_frame_index;      /* system_element.c */

/* ui_element->active_id holds this while a press that started on a number box is held, so nothing underneath reacts */
static const uint32_t k_nf_lock_id = 0x7C7C7C04u;

enum { nf_idle = 0, nf_pending, nf_scrub, nf_edit };
enum { nf_char_w = 8, nf_pad = 4, nf_scrub_px = 3, nf_buf = 32 };

static int      s_mode = nf_idle;
static uint32_t s_id = 0;
static int      s_press_x = 0, s_last_x = 0;
static float    s_raw = 0.0f;                  /* unrounded display value while scrubbing, so small drags are not eaten */
static char     s_text[nf_buf];
static int      s_len = 0;
static bool     s_fresh = false;               /* first key replaces the whole text */
static uint32_t s_last_drawn = 0;
static uint32_t s_edit_frame = 0xFFFFFFFFu;

/* ------------------------------------------------------------------------- */
/* once per frame: mouse edge_to_face_array + key edges, however many panels draw          */
/* ------------------------------------------------------------------------- */
static uint32_t s_frame = 0xFFFFFFFFu;
static bool     s_prev_down = false, s_edge = false;
static bool     s_key_prev[256];
static char     s_typed[16];
static int      s_typed_n = 0;
static bool     s_k_back = false, s_k_enter = false, s_k_esc = false;

static bool key_edge(int vk)
{
    const bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool e = now && !s_key_prev[vk & 255];
    s_key_prev[vk & 255] = now;
    return e;
}

static void queue_char(char c) { if (s_typed_n < (int)sizeof(s_typed)) s_typed[s_typed_n++] = c; }

static void cancel_all(system_element_context* ui_element)
{
    if (ui_element && ui_element->active_id == k_nf_lock_id) ui_element->active_id = 0;
    s_mode = nf_idle; s_id = 0;
}

static void begin_frame_once(system_element_context* ui_element)
{
    if (s_frame == g_ui_frame_index) return;
    s_frame = g_ui_frame_index;

    s_edge = ui_element->input.mouse_down && !s_prev_down;
    s_prev_down = ui_element->input.mouse_down;

    /* the box that was active is no longer drawn (object deselected, window closed ...): never leave the UI locked */
    if (s_mode != nf_idle && g_ui_frame_index - s_last_drawn > 1) cancel_all(ui_element);

    s_typed_n = 0; s_k_back = s_k_enter = s_k_esc = false;
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    for (int d = 0; d <= 9; ++d) {
        const bool a = key_edge('0' + d), b = key_edge(VK_NUMPAD0 + d);
        if ((a && !shift) || b) queue_char((char)('0' + d));
    }
    { const bool a = key_edge(VK_OEM_PERIOD), b = key_edge(VK_DECIMAL);  if ((a && !shift) || b) queue_char('.'); }
    { const bool a = key_edge(VK_OEM_MINUS),  b = key_edge(VK_SUBTRACT); if ((a && !shift) || b) queue_char('-'); }
    { const bool a = key_edge(VK_OEM_2),      b = key_edge(VK_DIVIDE);   if ((a && !shift) || b) queue_char('/'); }
    s_k_back  = key_edge(VK_BACK);
    s_k_enter = key_edge(VK_RETURN);
    s_k_esc   = key_edge(VK_ESCAPE);
}

bool is_number_field_active(void)  { return s_mode != nf_idle; }
bool is_number_field_editing(void) { return s_mode == nf_edit; }

/* ------------------------------------------------------------------------- */
/* text helpers                                                              */
/* ------------------------------------------------------------------------- */
/* the most decimals that still fit in max_chars. never prints "-0.0000" */
static void format_fit(char* out, size_t cap, float vert_to_edge_array, int decimals, int max_chars)
{
    for (int d = decimals; d >= 0; --d) {
        const float half_ulp = 0.5f * powf(10.0f, (float)-d);
        const float shown = (fabsf(vert_to_edge_array) < half_ulp) ? 0.0f : vert_to_edge_array;
        snprintf(out, cap, "%.*f", d, (double)shown);
        if ((int)strlen(out) <= max_chars) return;
    }
}

/* "0.062500" -> "0.0625", "1.000000" -> "1" */
static void format_edit_text(char* out, size_t cap, float vert_to_edge_array)
{
    snprintf(out, cap, "%.6f", (double)vert_to_edge_array);
    char* dot = strchr(out, '.');
    if (!dot) return;
    char* end = out + strlen(out) - 1;
    while (end > dot && *end == '0') *end-- = 0;
    if (end == dot) *end = 0;
    if (strcmp(out, "-0") == 0) strcpy(out, "0");
}

static bool parse_text(const char* s, double* out)
{
    if (!s[0]) return false;
    const char* slash = strchr(s, '/');
    char* e1 = NULL;
    if (!slash) {
        const double vert_to_edge_array = strtod(s, &e1);
        if (e1 == s || *e1 != 0) return false;
        *out = vert_to_edge_array; return true;
    }
    const double a = strtod(s, &e1);
    if (e1 == s || e1 != slash) return false;
    char* e2 = NULL;
    const double b = strtod(slash + 1, &e2);
    if (e2 == slash + 1 || *e2 != 0 || b == 0.0) return false;
    *out = a / b; return true;
}

static void insert_char(char c)
{
    if (s_fresh) { s_len = 0; s_text[0] = 0; s_fresh = false; }
    if (s_len >= nf_buf - 2) return;
    if (c == '-' && s_len != 0) return;                         /* only as the first character */
    if (c == '.') {                                             /* one point per number (numerator / denominator) */
        const char* seg = strrchr(s_text, '/');
        if (strchr(seg ? seg + 1 : s_text, '.')) return;
    }
    if (c == '/' && (s_len == 0 || strchr(s_text, '/'))) return;
    s_text[s_len++] = c; s_text[s_len] = 0;
}

static float clamp_opts(float vert_to_edge_array, const number_field_opts* o)
{
    if (o->min != 0.0f || o->max != 0.0f) { if (vert_to_edge_array < o->min) vert_to_edge_array = o->min; if (vert_to_edge_array > o->max) vert_to_edge_array = o->max; }
    return vert_to_edge_array;
}

/* ------------------------------------------------------------------------- */
/* the widget                                                                */
/* ------------------------------------------------------------------------- */
number_field_result draw_number_field(VkCommandBuffer cmd, system_element_context* ui_element, uint32_t id,
                                    int x, int y, int w, int h, float* value, const number_field_opts* opts)
{
    number_field_result result = number_field_none;
    if (!ui_element || !value) return result;
    begin_frame_once(ui_element);

    static const number_field_opts k_default = { 0 };
    const number_field_opts* o = opts ? opts : &k_default;
    const float scale = (o->display_scale != 0.0f) ? o->display_scale : 1.0f;
    const float step  = (o->step != 0.0f) ? o->step : 0.0625f;
    const int   decimals = (o->decimals > 0) ? o->decimals : 4;

    const int mx = ui_element->input.mouse_x, my = ui_element->input.mouse_y;
    const bool hov = mx >= x && mx < x + w && my >= y && my < y + h;

    /* press on this box starts a scrub (or a click-to-type) */
    if (s_mode == nf_idle && s_edge && hov && ui_element->active_id == 0 && !is_context_menu_open()) {
        s_mode = nf_pending; s_id = id;
        s_press_x = s_last_x = mx;
        s_raw = *value * scale;
        ui_element->active_id = k_nf_lock_id;
    }

    const bool mine = (s_id == id) && s_mode != nf_idle;
    if (mine) s_last_drawn = g_ui_frame_index;

    if (mine && (s_mode == nf_pending || s_mode == nf_scrub)) {
        if (ui_element->input.mouse_down) {
            int dx = mx - s_last_x;
            s_last_x = mx;
            if (s_mode == nf_pending && abs(mx - s_press_x) >= nf_scrub_px) { s_mode = nf_scrub; dx = mx - s_press_x; }
            if (s_mode == nf_scrub && dx != 0) {
                const bool fine = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                s_raw += (float)dx * step * (fine ? (1.0f / 16.0f) : 1.0f);
                float shown = s_raw;
                if (o->quantum > 0.0f && !fine) shown = roundf(s_raw / o->quantum) * o->quantum;
                shown = clamp_opts(shown, o);
                *value = shown / scale;
                result = number_field_scrubbed;
            }
        } else if (s_mode == nf_pending) {                       /* released without moving: type a number */
            s_mode = nf_edit;
            format_edit_text(s_text, sizeof(s_text), *value * scale);
            s_len = (int)strlen(s_text);
            s_fresh = true;
            if (ui_element->active_id == k_nf_lock_id) ui_element->active_id = 0;
        } else {                                                 /* released after a scrub */
            cancel_all(ui_element);
        }
    }
    else if (mine && s_mode == nf_edit) {
        bool apply = false, cancel = false;
        if (s_edge && !hov) apply = true;                        /* click away = apply */

        if (s_edit_frame != g_ui_frame_index) {                  /* keys are consumed once per frame, not once per Scene view */
            s_edit_frame = g_ui_frame_index;
            for (int i = 0; i < s_typed_n; ++i) insert_char(s_typed[i]);
            if (s_k_back) { if (s_fresh) { s_len = 0; s_text[0] = 0; s_fresh = false; } else if (s_len > 0) s_text[--s_len] = 0; }
            if (s_k_enter) apply = true;
            if (s_k_esc)   cancel = true;
        }

        if (apply || cancel) {
            double parsed = 0.0;
            if (apply && parse_text(s_text, &parsed)) {
                *value = clamp_opts((float)parsed, o) / scale;
                result = number_field_committed;
            }
            cancel_all(ui_element);
        }
    }

    /* ---- draw ---- */
    const bool editing = (s_mode == nf_edit) && (s_id == id);
    const bool active  = (s_mode != nf_idle) && (s_id == id);
    push_quad_element(cmd, (float)x, (float)y, (float)w, (float)h,
                      active ? system_theme.strip_button_selected : (hov ? system_theme.tab_item : system_theme.strip_button));

    const int max_chars = (w - nf_pad * 2) / nf_char_w;
    char text[nf_buf + 4];
    if (editing) {
        const bool caret = ((GetTickCount64() / 500) & 1) == 0;
        snprintf(text, sizeof(text), "%s%s", s_text, caret ? "_" : " ");
        const int n = (int)strlen(text);
        const char* shown = (n > max_chars) ? text + (n - max_chars) : text;   /* keep the end (the caret side) visible */
        draw_text_element(cmd, ui_element, shown, x + w - nf_pad - (int)strlen(shown) * nf_char_w, y + (h - 8) / 2, system_theme.system_text);

        const system_color amber = { .red = 1.0f, .green = 0.7f, .blue = 0.1f, .alpha = 1.0f };
        push_quad_element(cmd, (float)x, (float)y, (float)w, 1.0f, amber);
        push_quad_element(cmd, (float)x, (float)(y + h - 1), (float)w, 1.0f, amber);
        push_quad_element(cmd, (float)x, (float)y, 1.0f, (float)h, amber);
        push_quad_element(cmd, (float)(x + w - 1), (float)y, 1.0f, (float)h, amber);
    } else {
        format_fit(text, sizeof(text), *value * scale, decimals, max_chars);
        draw_text_element(cmd, ui_element, text, x + w - nf_pad - (int)strlen(text) * nf_char_w, y + (h - 8) / 2, system_theme.button_text);
    }
    return result;
}