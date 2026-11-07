#ifndef phoenix_system_render_h
#define phoenix_system_render_h

#include "engine_defines.h"
#include "engine_element.h"


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

/* the engine's vulkan renderer. only vulkan's own names (Vk*, VK_*, vk*) are capitalised, everything else is ours.
   the editor ui scene pass lives in the editor, not here. */

struct static_mesh_data;

#define vulkan_frames_in_flight_count 2   /* cpu pacing ring size for the main swapchain */
#define shader_directory_count        5   /* entries in g_shader_dirs (engine_globals.h) */

typedef enum render_backend_type
{
    render_backend_type_vulkan,
    render_backend_type_opengl,
    render_backend_type_directx
} render_backend_type;

typedef struct render_backend
{
    double frame_number;

    bool (*start_render)(struct render_backend* backend, const char* application_name);

    void (*shutdown_render)(struct render_backend* backend);

    void (*resized)(struct render_backend* backend, int width, int height);

    bool (*begin_frame)(struct render_backend* backend, float delta_time);
    bool (*end_frame)(struct render_backend* backend, float delta_time);
} render_backend;

typedef struct render_packet
{
    float delta_time;
} render_packet;

typedef struct vulkan_swapchain_support_info
{
    VkSurfaceCapabilitiesKHR capabilities;
    int format_count;
    VkSurfaceFormatKHR* formats;
    int present_mode_count;
    VkPresentModeKHR* present_modes;
} vulkan_swapchain_support_info;

typedef struct vulkan_device
{
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    vulkan_swapchain_support_info swapchain_support;
    int graphics_queue_index;
    int present_queue_index;
    int transfer_queue_index;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkCommandPool graphics_command_pool;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;

    VkFormat depth_format;
} vulkan_device;

typedef enum vulkan_render_pass_state
{
    vulkan_render_pass_state_ready,
    vulkan_render_pass_state_recording,
    vulkan_render_pass_state_in_render_pass,
    vulkan_render_pass_state_recording_ended,
    vulkan_render_pass_state_submitted,
    vulkan_render_pass_state_not_allocated
} vulkan_render_pass_state;

typedef struct vulkan_renderpass
{
    VkRenderPass handle;
    float x, y, w, h;
    float r, g, b, a;

    float depth;
    int stencil;

    vulkan_render_pass_state state;
} vulkan_renderpass;

typedef struct vulkan_framebuffer
{
    VkFramebuffer handle;
    int attachment_count;
    VkImageView* attachments;
    vulkan_renderpass* renderpass;
} vulkan_framebuffer;

typedef struct vulkan_image
{
    VkImage handle;
    VkDeviceMemory memory;
    VkImageView view;
    uint32_t width;
    uint32_t height;
} vulkan_image;

typedef struct vulkan_swapchain
{
    VkSurfaceFormatKHR image_format;
    char max_frames_in_flight;
    VkSwapchainKHR handle;
    int image_count;
    VkImage* images;
    VkImageView* views;

    vulkan_image depth_attachment;

    /* framebuffers used for on screen rendering */
    vulkan_framebuffer* framebuffers;
} vulkan_swapchain;

typedef enum vulkan_command_buffer_state
{
    vulkan_command_buffer_state_ready,
    vulkan_command_buffer_state_recording,
    vulkan_command_buffer_state_in_render_pass,
    vulkan_command_buffer_state_recording_ended,
    vulkan_command_buffer_state_submitted,
    vulkan_command_buffer_state_not_allocated
} vulkan_command_buffer_state;

typedef struct vulkan_command_buffer
{
    VkCommandBuffer handle;
    vulkan_command_buffer_state state;
} vulkan_command_buffer;

typedef struct vulkan_fence
{
    VkFence handle;
    bool is_signaled;
} vulkan_fence;

typedef struct vulkan_system_context
{
    /* the framebuffer's current size */
    int framebuffer_width;
    int framebuffer_height;

    /* current generation of the framebuffer size. if it does not match framebuffer_size_last_generation,
       a new one should be generated. */
    long long framebuffer_size_generation;

    /* the generation of the framebuffer when it was last created. set to framebuffer_size_generation when updated. */
    long long framebuffer_size_last_generation;

    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;

#if defined(_DEBUG)
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    vulkan_device device;

    vulkan_swapchain swapchain;
    vulkan_renderpass main_renderpass;

    /* darray */
    vulkan_command_buffer* graphics_command_buffers;

    /* darray */
    VkSemaphore* image_available_semaphores;

    /* darray */
    VkSemaphore* queue_complete_semaphores;

    int in_flight_fence_count;
    vulkan_fence* in_flight_fences;

    /* pointers to fences which exist and are owned elsewhere */
    vulkan_fence** images_in_flight;

    int image_index;
    int current_frame;

    bool recreating_swapchain;

    int (*find_memory_index)(unsigned int type_filter, unsigned int property_flags);
} vulkan_system_context;

typedef struct editor_window_swapchain
{
    VkSwapchainKHR handle;
    uint32_t image_count;
    VkImage images[8];
    VkImageView image_views[8];
    VkFormat image_format;
    VkExtent2D extent;
    VkFramebuffer framebuffers[8];
    VkCommandBuffer command_buffers[8];

    VkSemaphore image_available_semaphores[8];
    VkSemaphore render_finished_semaphores[8];

    VkFence in_flight_fences[2];
    VkFence images_in_flight[8];
    uint32_t current_frame;
} editor_window_swapchain;

typedef struct struct_offscreen_texture
{
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkDescriptorSet descriptor_set;
} struct_offscreen_texture;

typedef struct system_render_context
{
    VkInstance instance;
    VkAllocationCallbacks* allocator;

    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties device_properties;
    bool validation_enabled;

    VkSurfaceKHR surface_main;
    editor_window_swapchain swapchain_main;
    VkRenderPass renderpass_main;

    VkCommandPool command_pool;
    VkDescriptorSetLayout descriptor_layout;

    VkPipeline graphics_pipeline;
    VkPipeline graphics_pipeline_3d;
    VkPipeline graphics_pipeline_grid;
    VkPipelineLayout pipeline_layout;
    VkPipelineLayout pipeline_layout_3d;

    VkFramebuffer offscreen_framebuffer_scene;
    VkSampler offscreen_sampler;

    VkDevice logical_device;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family_index;

    struct_offscreen_texture scene_viewport_target;
    struct_offscreen_texture game_viewport_target;
    VkRenderPass offscreen_renderpass;
} system_render_context;


void validate_vulkan_result(VkResult result);
void get_program_framebuffer_size(unsigned int* width, unsigned int* height);

/* ---- factory ---- */
bool create_render_backend(render_backend_type type, render_backend* out_renderer_backend);
void destroy_render_backend(render_backend* renderer_backend);

/* ---- vulkan backend ---- */
bool start_vulkan_render_backend(render_backend* backend, const char* application_name);
void shutdown_vulkan_render_backend(render_backend* backend);
void vulkan_render_backend_on_resized(render_backend* backend, int width, int height);
bool begin_vulkan_render_backend_frame(render_backend* backend, float delta_time);
bool end_vulkan_render_backend_frame(render_backend* backend, float delta_time);
int  find_memory_index(unsigned int type_filter, unsigned int property_flags);
void create_command_buffers(render_backend* backend);
void regenerate_framebuffers(render_backend* backend, editor_window_swapchain* swapchain, VkRenderPass* renderpass);
bool recreate_swapchain(render_backend* backend);
bool create_ui_fallback_descriptor(void);
void destroy_ui_fallback_descriptor(void);
VKAPI_ATTR VkBool32 VKAPI_CALL handle_vulkan_debug_message(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data);

/* ---- frontend ---- */
bool start_render_frontend(const char* application_name);
void shutdown_render_frontend(void);
bool begin_frontend_render_frame(float delta_time);
bool end_frontend_render_frame(float delta_time);
bool draw_frontend_render_frame(render_packet* packet);
VkCommandBuffer begin_frontend_main_pass(void);
void end_frontend_main_pass(VkCommandBuffer cmd);

/* ---- command buffers ---- */
void allocate_vulkan_command_buffer(
    vulkan_system_context* context,
    VkCommandPool pool,
    bool is_primary,
    vulkan_command_buffer* out_command_buffer);

void free_vulkan_command_buffer(
    vulkan_system_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* command_buffer);

void begin_vulkan_command_buffer(
    vulkan_command_buffer* command_buffer,
    bool is_single_use,
    bool is_renderpass_continue,
    bool is_simultaneous_use);

void end_vulkan_command_buffer(vulkan_command_buffer* command_buffer);

void update_vulkan_command_buffer(vulkan_command_buffer* command_buffer);

void reset_vulkan_command_buffer(vulkan_command_buffer* command_buffer);

void begin_vulkan_command_buffer_allocate_and_single_use(
    vulkan_system_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* out_command_buffer);

void end_vulkan_command_buffer_single_use(
    vulkan_system_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* command_buffer,
    VkQueue queue);

/* ---- device ---- */
bool create_vulkan_device(vulkan_system_context* context);

void remove_vulkan_device(vulkan_system_context* context);

void query_vulkan_device_swapchain_support(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    vulkan_swapchain_support_info* out_support_info);

bool detect_vulkan_device_depth_format(vulkan_device* device);

/* ---- fences ---- */
void create_vulkan_fence(
    vulkan_system_context* context,
    bool create_signaled,
    vulkan_fence* out_fence);

void remove_vulkan_fence(vulkan_system_context* context, vulkan_fence* fence);

bool vulkan_fence_wait(vulkan_system_context* context, vulkan_fence* fence, uint64_t timeout_ns);

void reset_vulkan_fence(vulkan_system_context* context, vulkan_fence* fence);

/* ---- framebuffers ---- */
void create_vulkan_framebuffer(
    vulkan_system_context* context,
    vulkan_renderpass* renderpass,
    int width,
    int height,
    int attachment_count,
    VkImageView* attachments,
    vulkan_framebuffer* out_framebuffer);

void remove_vulkan_framebuffer(vulkan_system_context* context, vulkan_framebuffer* framebuffer);

/* ---- images ---- */
void create_vulkan_image(
    vulkan_system_context* context,
    VkImageType image_type,
    int width,
    int height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    int32_t create_view,
    VkImageAspectFlags view_aspect_flags,
    vulkan_image* out_image);

void create_vulkan_image_view(
    vulkan_system_context* context,
    VkFormat format,
    vulkan_image* image,
    VkImageAspectFlags aspect_flags);

void remove_vulkan_image(vulkan_system_context* context, vulkan_image* image);

/* ---- renderpass ---- */
void create_vulkan_renderpass(
    vulkan_system_context* context,
    vulkan_renderpass* out_renderpass,
    float x, float y, float w, float h,
    float r, float g, float b, float a,
    float depth,
    int stencil);

void remove_vulkan_renderpass(vulkan_system_context* context, vulkan_renderpass* renderpass);

void begin_vulkan_renderpass(
    vulkan_command_buffer* command_buffer,
    vulkan_renderpass* renderpass,
    VkFramebuffer frame_buffer);

void end_vulkan_renderpass(vulkan_command_buffer* command_buffer, vulkan_renderpass* renderpass);

/* ---- swapchain ---- */
void create_vulkan_swapchain(
    vulkan_system_context* context,
    int width,
    int height,
    vulkan_swapchain* out_swapchain);

void recreate_vulkan_swapchain(
    vulkan_system_context* context,
    int width,
    int height,
    vulkan_swapchain* swapchain);

void remove_vulkan_swapchain(
    vulkan_system_context* context,
    vulkan_swapchain* swapchain);

bool acquire_vulkan_swapchain_next_image_index(
    vulkan_system_context* context,
    vulkan_swapchain* swapchain,
    unsigned long long timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    int* out_image_index);

void present_vulkan_swapchain(
    vulkan_system_context* context,
    vulkan_swapchain* swapchain,
    VkQueue graphics_queue,
    VkQueue present_queue,
    VkSemaphore render_complete_semaphore,
    int present_image_index);

void create_vulkan_swapchain_objects(vulkan_system_context* context, int width, int height, vulkan_swapchain* swapchain);
void destroy_vulkan_swapchain_objects(vulkan_system_context* context, vulkan_swapchain* swapchain);

void destroy_swapchain_depth_image(void);
void destroy_swapchain_msaa_image(void);
void create_swapchain_msaa_image(VkExtent2D extent, VkFormat format);
VkImageView get_swapchain_msaa_view(void);

/* ---- scene pass (engine side) ---- */
void start_system_scenepass(VkCommandBuffer cmd);
void shutdown_system_scenepass(void);
void end_system_scenepass_element_handling(void);

/* ---- shaders and pipelines ---- */
uint32_t* read_spirv_file(const char* path, size_t* out_size);
bool get_file_write_time(const char* path, FILETIME* out);
void warn_if_shader_stale(const char* spv_path, const char* name);
VkShaderModule create_shader_module(VkDevice device, const uint32_t* code, size_t code_size, const VkAllocationCallbacks* allocator);
void destroy_module_if_valid(VkDevice device, VkShaderModule mesh, const VkAllocationCallbacks* allocator);
VkShaderModule load_shader_binary_module(VkDevice device, const char* filename);
bool create_vulkan_graphics_pipelines(vulkan_system_context* context, VkRenderPass renderpass);
void skin_gpu_tick(void);
void skin_gpu_flush(void);
VkSampleCountFlagBits get_render_msaa_sample_count(void);
VkSampleCountFlagBits get_valid_grid_sample_count(vulkan_system_context* context);


#endif
