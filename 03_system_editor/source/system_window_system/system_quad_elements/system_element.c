#include "system_globals.h"

enum { draw_queue_capacity = 262144 };

ui_push_constant g_draw_queue[draw_queue_capacity];
uint32_t g_draw_count = 0;
uint32_t g_ui_frame_index = 0;

static bool draw_queue_overflow_reported = false;
static bool draw_queue_peak_reported = false;

void start_editor_elements(system_element_context* context)
{
    if (!context) return;

    theme_apply_current();

    context->active_id = 0;
    context->hot_id = 0;

    context->scene_view = (struct_viewport_bounds){ 1280, 720, 0, 60 };
    context->game_view  = (struct_viewport_bounds){ 1280, 720, 0, 60 };

    context->input.mouse_x = 0;
    context->input.mouse_y = 0;
    context->input.mouse_down = false;
}

static inline float srgb_to_linear(float c)
{
    return (c <= 0.04045f) ? (c / 12.92f) : powf((c + 0.055f) / 1.055f, 2.4f);
}

static inline float clamp_unit(float vert_to_edge_array)
{
    return vert_to_edge_array < 0.0f ? 0.0f : (vert_to_edge_array > 1.0f ? 1.0f : vert_to_edge_array);
}

/* set this where you pick the surface format:
   g_swapchain_is_srgb = (fmt == VK_FORMAT_B8G8R8A8_SRGB || fmt == VK_FORMAT_R8G8B8A8_SRGB); */
bool g_swapchain_is_srgb = true;

/* the only place that writes the queue. wire_alpha is what the shaders read: 0..1 for flat quads,
   2.0 / 3.0 for scene / game composites (see system_ui_batch.c + the ui_element shaders). */
static void queue_quad(float x, float y, float w, float h, float r, float g, float b, float wire_alpha)
{
    if (g_draw_count >= draw_queue_capacity) {
        if (!draw_queue_overflow_reported) {
            phoenix_error("RENDERER", "UI draw queue overflow (%u quads queued). Quads are being dropped; reduce repeated text/context-menu work or raise the frame budget.", draw_queue_capacity);
            draw_queue_overflow_reported = true;
        }
        return;
    }

    ui_push_constant* q = &g_draw_queue[g_draw_count++];
    q->position[0] = x;  q->position[1] = y;
    q->size[0]     = w;  q->size[1]     = h;
    q->color[0] = r;  q->color[1] = g;  q->color[2] = b;  q->color[3] = wire_alpha;
}

void push_quad_element(VkCommandBuffer cmd, float x, float y, float w, float h, system_color color)
{
    (void)cmd;

    float r = color.red, g = color.green, b = color.blue;

    if (g_swapchain_is_srgb) {
        r = srgb_to_linear(r);
        g = srgb_to_linear(g);
        b = srgb_to_linear(b);
    }
    queue_quad(x, y, w, h, r, g, b, clamp_unit(color.alpha));
}

/* one quad for a whole circle. alpha stays a normal 0..1 (the batch flush gives alpha >= 2 a meaning of its own), so the circle code
   rides in the color instead: blue + 2 marks a circle, green + 2 * k carries the thickness in quarter pixels (k = 0 is a filled disc).
   the fragment shader (ui_batch_frag.glsl) decodes it. the quad is radius + 1px so the soft edge fits. */
void push_circle_element(VkCommandBuffer cmd, float cx, float cy, float radius, float thickness, system_color color)
{
    (void)cmd;
    if (radius <= 0.0f) return;

    float r = color.red, g = color.green, b = color.blue;
    if (g_swapchain_is_srgb) {
        r = srgb_to_linear(r);
        g = srgb_to_linear(g);
        b = srgb_to_linear(b);
    }

    int k = (int)(thickness * 4.0f + 0.5f);
    if (k < 0)    k = 0;
    if (k > 1000) k = 1000;

    const float half = radius + 1.0f;
    queue_quad(cx - half, cy - half, half * 2.0f, half * 2.0f, r, g + 2.0f * (float)k, b + 2.0f, clamp_unit(color.alpha));
}

void push_viewport_quad_element(VkCommandBuffer cmd, float x, float y, float w, float h, enum_viewport_kind kind)
{
    (void)cmd;
    queue_quad(x, y, w, h, 1.0f, 1.0f, 1.0f, (kind == viewport_kind_game) ? 3.0f : 2.0f);
}

/* GPU side lives in system_ui_batch.c: the whole queue is uploaded once and drawn as instanced runs. */
void ui_batch_flush(VkCommandBuffer cmd, VkExtent2D extent, ui_push_constant* queue, uint32_t face_type, uint32_t capacity);

void render_element_commands(VkCommandBuffer cmd, VkExtent2D extent, VkPipelineLayout pipeline_layout)
{
    (void)pipeline_layout;
    g_ui_frame_index++;                 /* one tick per UI frame: lets once-per-frame code (context menu) guard itself */
    if (g_draw_count == 0) return;

    if (!draw_queue_peak_reported) {
        phoenix_info("RENDERER", "UI quads queued in first frame: %u / %u", g_draw_count, (uint32_t)draw_queue_capacity);
        draw_queue_peak_reported = true;
    }

    ui_batch_flush(cmd, extent, g_draw_queue, g_draw_count, draw_queue_capacity);
    g_draw_count = 0;
}