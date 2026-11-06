#include "engine_globals.h"


// --- INTERNAL STATIC STATE VARIABLES ---
static unsigned int cached_framebuffer_width = 0;
static unsigned int cached_framebuffer_height = 0;
static unsigned long long framebuffer_size_generation = 0;
static unsigned long long framebuffer_size_last_generation = 0;
static bool recreating_swapchain = false;
static unsigned int image_index = 0;

// Track the CPU pacing ring frame index independently of the GPU swapchain image index
static uint32_t current_inflight_cpu_frame = 0;
#define MAX_FRAMES_IN_FLIGHT 2

static VkDebugUtilsMessengerEXT g_debug_messenger = VK_NULL_HANDLE;
extern void destroy_swapchain_depth_image(void);


// =========================================================================
// 🛠️ LINKAGE FIX: EXPLICIT SUB-SYSTEM FORWARD PROTOTYPE DECLARATIONS
// =========================================================================
// This forces the backend translation unit to resolve the exact addresses
// compiled inside your swapchain and renderpass C files.
extern void create_vulkan_swapchain(vulkan_system_context* context, int width, int height, vulkan_swapchain* out_swapchain);
extern void create_vulkan_renderpass(vulkan_system_context* context, vulkan_renderpass* out_renderpass, float x, float y, float w, float h, float r, float g, float b, float a, float depth, int stencil);
extern void remove_vulkan_swapchain(vulkan_system_context* context, vulkan_swapchain* swapchain);
extern void remove_vulkan_renderpass(vulkan_system_context* context, vulkan_renderpass* renderpass);

int find_memory_index(unsigned int type_filter, unsigned int property_flags);
void create_command_buffers(render_backend* backend);
void regenerate_framebuffers(render_backend* backend, editor_window_swapchain* swapchain, VkRenderPass* renderpass);
bool recreate_swapchain(render_backend* backend);

// Fallback checking helper to replace strings_equal
static bool validate_string_match(const char* str1, const char* str2) {
    return strcmp(str1, str2) == 0;
}

// Fallback helper to replace vulkan_result_is_success
static bool check_vulkan_success(VkResult result) {
    return result == VK_SUCCESS;
}

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) 
{
    (void)message_types; (void)user_data;
    switch (message_severity) {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            phoenix_error("VULKAN_VALIDATION", "%s", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            phoenix_warn("VULKAN_VALIDATION", "%s", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            phoenix_info("VULKAN_VALIDATION", "%s", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            phoenix_trace("VULKAN_VALIDATION", "%s", callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}

// --- FALLBACK UI TEXTURE STATE ---
static VkImage          ui_fallback_image   = VK_NULL_HANDLE;
static VkDeviceMemory   ui_fallback_memory  = VK_NULL_HANDLE;
static VkImageView      ui_fallback_view    = VK_NULL_HANDLE;
static VkSampler        ui_fallback_sampler = VK_NULL_HANDLE;
static VkDescriptorPool ui_fallback_pool    = VK_NULL_HANDLE;
static VkDescriptorSet  ui_fallback_set     = VK_NULL_HANDLE;

bool create_ui_fallback_descriptor(void)
{
    VkDevice device = render_context.logical_device;

    if (render_context.descriptor_layout == VK_NULL_HANDLE) {
        phoenix_error("VULKAN", "%s", "UI descriptor layout missing; cannot create fallback descriptor.");
        return false;
    }

    // 1. 1x1 image
    VkImageCreateInfo image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType     = VK_IMAGE_TYPE_2D;
    image_info.format        = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width  = 1;
    image_info.extent.height = 1;
    image_info.extent.depth  = 1;
    image_info.mipLevels     = 1;
    image_info.arrayLayers   = 1;
    image_info.samples       = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &image_info, render_context.allocator, &ui_fallback_image) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to create fallback UI image.");
        return false;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device, ui_fallback_image, &mem_req);
    int mem_index = find_memory_index(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_index < 0) {
        phoenix_error("VULKAN", "%s", "No memory type for fallback UI image.");
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc_info.allocationSize  = mem_req.size;
    alloc_info.memoryTypeIndex = (uint32_t)mem_index;
    if (vkAllocateMemory(device, &alloc_info, render_context.allocator, &ui_fallback_memory) != VK_SUCCESS ||
        vkBindImageMemory(device, ui_fallback_image, ui_fallback_memory, 0) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to allocate or bind fallback UI image memory.");
        return false;
    }

    // 2. View
    VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image    = ui_fallback_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format   = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &view_info, render_context.allocator, &ui_fallback_view) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to create fallback UI image view.");
        return false;
    }

    // 3. Sampler
    VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter    = VK_FILTER_LINEAR;
    sampler_info.minFilter    = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxLod       = 1.0f;
    if (vkCreateSampler(device, &sampler_info, render_context.allocator, &ui_fallback_sampler) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to create fallback UI sampler.");
        return false;
    }

    // 4. Descriptor pool + set
    VkDescriptorPoolSize pool_size = {0};
    pool_size.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes    = &pool_size;
    pool_info.maxSets       = 1;
    if (vkCreateDescriptorPool(device, &pool_info, render_context.allocator, &ui_fallback_pool) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to create fallback UI descriptor pool.");
        return false;
    }

    VkDescriptorSetAllocateInfo set_alloc = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    set_alloc.descriptorPool     = ui_fallback_pool;
    set_alloc.descriptorSetCount = 1;
    set_alloc.pSetLayouts        = &render_context.descriptor_layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &set_alloc, &set) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to allocate fallback UI descriptor set.");
        return false;
    }

    VkDescriptorImageInfo descriptor_image = {0};
    descriptor_image.sampler     = ui_fallback_sampler;
    descriptor_image.imageView   = ui_fallback_view;
    descriptor_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet          = set;
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &descriptor_image;
    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

    // 5. One-time submit: UNDEFINED -> TRANSFER_DST, clear, -> SHADER_READ_ONLY
    VkCommandBufferAllocateInfo cmd_alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmd_alloc.commandPool        = render_context.command_pool;
    cmd_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &cmd_alloc, &cmd) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to allocate fallback UI upload command buffer.");
        return false;
    }

    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkImageSubresourceRange range = {0};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;

    VkImageMemoryBarrier to_transfer = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    to_transfer.srcAccessMask       = 0;
    to_transfer.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_transfer.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    to_transfer.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image               = ui_fallback_image;
    to_transfer.subresourceRange    = range;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_transfer);

    VkClearColorValue clear_color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    vkCmdClearColorImage(cmd, ui_fallback_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &range);

    VkImageMemoryBarrier to_shader = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    to_shader.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    to_shader.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.image               = ui_fallback_image;
    to_shader.subresourceRange    = range;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_shader);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;
    vkQueueSubmit(render_context.graphics_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(render_context.graphics_queue);
    vkFreeCommandBuffers(device, render_context.command_pool, 1, &cmd);

    // 6. Publish the set so draw_frontend_render_frame always has something valid to bind.
    //    ONLY where offscreen_create_resources() has not already put the real scene / game texture:
    //    this runs after it, and overwriting would make the Scene / Game views sample this 1x1 grey texture.
    ui_fallback_set = set;
    if (render_context.scene_viewport_target.descriptor_set == VK_NULL_HANDLE)
        render_context.scene_viewport_target.descriptor_set = set;
    if (render_context.game_viewport_target.descriptor_set == VK_NULL_HANDLE)
        render_context.game_viewport_target.descriptor_set = set;

    phoenix_info("VULKAN", "%s", "Fallback UI descriptor set created.");
    return true;
}

void destroy_ui_fallback_descriptor(void)
{
    VkDevice device = render_context.logical_device;
    if (device == VK_NULL_HANDLE) return;

    if (ui_fallback_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, ui_fallback_pool, render_context.allocator);
        ui_fallback_pool = VK_NULL_HANDLE;
    }
    if (ui_fallback_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, ui_fallback_sampler, render_context.allocator);
        ui_fallback_sampler = VK_NULL_HANDLE;
    }
    if (ui_fallback_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, ui_fallback_view, render_context.allocator);
        ui_fallback_view = VK_NULL_HANDLE;
    }
    if (ui_fallback_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, ui_fallback_image, render_context.allocator);
        ui_fallback_image = VK_NULL_HANDLE;
    }
    if (ui_fallback_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, ui_fallback_memory, render_context.allocator);
        ui_fallback_memory = VK_NULL_HANDLE;
    }
    /* only clear the fields that still point at the fallback; the offscreen module owns the others */
    if (render_context.scene_viewport_target.descriptor_set == ui_fallback_set)
        render_context.scene_viewport_target.descriptor_set = VK_NULL_HANDLE;
    if (render_context.game_viewport_target.descriptor_set == ui_fallback_set)
        render_context.game_viewport_target.descriptor_set  = VK_NULL_HANDLE;
    ui_fallback_set = VK_NULL_HANDLE;
}




bool start_vulkan_render_backend(render_backend* backend, const char* application_name) 
{
    phoenix_info("VULKAN", "Configuring backend graphics structures for: %s", application_name);

    render_context.allocator = NULL;

    get_program_framebuffer_size(&cached_framebuffer_width, &cached_framebuffer_height);
    render_context.swapchain_main.extent.width = (cached_framebuffer_width != 0) ? cached_framebuffer_width : 800;
    render_context.swapchain_main.extent.height = (cached_framebuffer_height != 0) ? cached_framebuffer_height : 600;
    cached_framebuffer_width = 0;
    cached_framebuffer_height = 0;

    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_2;
    app_info.pApplicationName = application_name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Phoenix Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    const char** required_extensions = (const char**)create_parry(PARRY_DEFAULT_CAPACITY, sizeof(char*));
    const char* ext_surf = VK_KHR_SURFACE_EXTENSION_NAME;
    parry_push(required_extensions, ext_surf); 

    #if defined(PHOENIX_PLATFORM_WINDOWS)
    const char* ext_win32 = "VK_KHR_win32_surface";
    parry_push(required_extensions, ext_win32);
    #endif

#if defined(PHOENIX_VULKAN_VALIDATION)
    const char* ext_debug = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    parry_push(required_extensions, ext_debug);  
#endif

    create_info.enabledExtensionCount = (unsigned int)parry_length(required_extensions);
    create_info.ppEnabledExtensionNames = required_extensions;

    const char** required_validation_layer_names = NULL;
    int required_validation_layer_count = 0;

#if defined(PHOENIX_VULKAN_VALIDATION)
    required_validation_layer_names = (const char**)create_parry(PARRY_DEFAULT_CAPACITY, sizeof(char*));
    const char* layer_khronos = "VK_LAYER_KHRONOS_validation";
    parry_push(required_validation_layer_names, layer_khronos);
    required_validation_layer_count = (int)parry_length(required_validation_layer_names);
#endif

    create_info.enabledLayerCount = required_validation_layer_count;
    create_info.ppEnabledLayerNames = required_validation_layer_names;

    if (vkCreateInstance(&create_info, render_context.allocator, &render_context.instance) != VK_SUCCESS) {
        phoenix_fatal("VULKAN", "%s", "Vulkan instance creation failed.");
        parry_destroy(required_extensions);
        if (required_validation_layer_names) parry_destroy(required_validation_layer_names);
        return FALSE;
    }
    phoenix_info("VULKAN", "%s", "Vulkan Instance created.");

    parry_destroy(required_extensions);
    if (required_validation_layer_names) parry_destroy(required_validation_layer_names);

#if defined(PHOENIX_VULKAN_VALIDATION)
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = vk_debug_callback;

    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(render_context.instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        func(render_context.instance, &debug_create_info, render_context.allocator, &g_debug_messenger);
    }
#endif

    VkWin32SurfaceCreateInfoKHR surface_create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surface_create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = GetModuleHandleA(NULL);
    surface_create_info.hwnd      = hwnd_window_00;

    if (vkCreateWin32SurfaceKHR(render_context.instance, &surface_create_info, render_context.allocator, &render_context.surface_main) != VK_SUCCESS) {
        phoenix_fatal("VULKAN", "%s", "Native platform window WSI binding mapping failure.");
        return FALSE;
    }

    static vulkan_system_context vk_runtime_context;
    memset(&vk_runtime_context, 0, sizeof(vulkan_system_context));

    vk_runtime_context.instance = render_context.instance;
    vk_runtime_context.allocator = render_context.allocator;
    vk_runtime_context.surface = render_context.surface_main;
    vk_runtime_context.find_memory_index = find_memory_index;

    if (!create_vulkan_device(&vk_runtime_context)) {
        phoenix_error("VULKAN", "%s", "Failed to compile hardware logical device drivers!");
        return FALSE;
    }

    render_context.physical_device = vk_runtime_context.device.physical_device;
    render_context.logical_device  = vk_runtime_context.device.logical_device;
    render_context.graphics_queue  = vk_runtime_context.device.graphics_queue;
    render_context.command_pool    = vk_runtime_context.device.graphics_command_pool;

    int target_width = (int)render_context.swapchain_main.extent.width;
    int target_height = (int)render_context.swapchain_main.extent.height;

    create_vulkan_swapchain(&vk_runtime_context, target_width, target_height, &vk_runtime_context.swapchain);

    create_vulkan_renderpass(
        &vk_runtime_context, 
        &vk_runtime_context.main_renderpass,
        0.0f, 0.0f, (float)target_width, (float)target_height,
        0.0f, 0.0f, 0.2f, 1.0f, 
        1.0f, 0
    );

    render_context.renderpass_main = vk_runtime_context.main_renderpass.handle;

    /* Offscreen scene pass (MSAA color + depth -> resolve). Grid and mesh pipelines are built against it. */
    if (!offscreen_create_renderpass(render_context.swapchain_main.image_format, vk_runtime_context.device.depth_format)) {
        phoenix_fatal("VULKAN", "%s", "Failed to create the offscreen scene render pass!");
        return FALSE;
    }

    if (!create_vulkan_graphics_pipelines(&vk_runtime_context, render_context.renderpass_main)) {
        phoenix_fatal("VULKAN", "%s", "Failed to compile the decoupled UI and 3D graphics pipeline suites!");
        return FALSE;
    }

    render_context.graphics_pipeline      = graphics_pipeline_ui; 
    render_context.graphics_pipeline_grid = graphics_pipeline_grid;
    render_context.pipeline_layout        = pipeline_layout_ui;

    if (!offscreen_create_resources()) {
        phoenix_fatal("VULKAN", "%s", "Failed to create offscreen sampler / descriptor sets!");
        return FALSE;
    }

    if (!create_ui_fallback_descriptor()) {
        phoenix_fatal("VULKAN", "%s", "Failed to create fallback UI descriptor set!");
        return FALSE;
    }

    create_command_buffers(backend);
    regenerate_framebuffers(backend, &render_context.swapchain_main, &render_context.renderpass_main);

    // FIX: size every loop from the real array length (no out-of-bounds writes)
    {
        VkDevice dev = render_context.logical_device;
        editor_window_swapchain* sc = &render_context.swapchain_main;

        const uint32_t acquire_sem_count  = (uint32_t)(sizeof(sc->image_available_semaphores) / sizeof(sc->image_available_semaphores[0]));
        const uint32_t finished_sem_count = (uint32_t)(sizeof(sc->render_finished_semaphores)  / sizeof(sc->render_finished_semaphores[0]));
        const uint32_t fence_count        = (uint32_t)(sizeof(sc->in_flight_fences)            / sizeof(sc->in_flight_fences[0]));
        const uint32_t flight_count       = (uint32_t)(sizeof(sc->images_in_flight)            / sizeof(sc->images_in_flight[0]));

        VkSemaphoreCreateInfo sem_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (uint32_t i = 0; i < acquire_sem_count; ++i) {
            vkCreateSemaphore(dev, &sem_info, render_context.allocator, &sc->image_available_semaphores[i]);
        }
        for (uint32_t i = 0; i < finished_sem_count; ++i) {
            vkCreateSemaphore(dev, &sem_info, render_context.allocator, &sc->render_finished_semaphores[i]);
        }

        VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (uint32_t i = 0; i < fence_count; ++i) {
            vkCreateFence(dev, &fence_info, render_context.allocator, &sc->in_flight_fences[i]);
        }

        for (uint32_t i = 0; i < flight_count; ++i) {
            sc->images_in_flight[i] = VK_NULL_HANDLE;
        }
    }

    phoenix_info("VULKAN", "%s", "Vulkan renderer initialized successfully.");
    return TRUE;
}



void shutdown_vulkan_render_backend(render_backend* backend)
{
    (void)backend;
    phoenix_info("VULKAN", "Destroying Vulkan hardware allocation contexts cleanly.");

    if (render_context.logical_device != VK_NULL_HANDLE) 
    {
        vkDeviceWaitIdle(render_context.logical_device);

        destroy_ui_fallback_descriptor();
        offscreen_shutdown();
        destroy_swapchain_depth_image();

        if (render_context.graphics_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(render_context.logical_device, render_context.graphics_pipeline, render_context.allocator);
            render_context.graphics_pipeline = VK_NULL_HANDLE;
        }
        if (render_context.graphics_pipeline_3d != VK_NULL_HANDLE) {
            vkDestroyPipeline(render_context.logical_device, render_context.graphics_pipeline_3d, render_context.allocator);
            render_context.graphics_pipeline_3d = VK_NULL_HANDLE;
        }
        if (render_context.graphics_pipeline_grid != VK_NULL_HANDLE) {
            vkDestroyPipeline(render_context.logical_device, render_context.graphics_pipeline_grid, render_context.allocator);
            render_context.graphics_pipeline_grid = VK_NULL_HANDLE;
        }
        if (render_context.pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(render_context.logical_device, render_context.pipeline_layout, render_context.allocator);
            render_context.pipeline_layout = VK_NULL_HANDLE;
        }
        if (pipeline_layout_grid != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(render_context.logical_device, pipeline_layout_grid, render_context.allocator);
            pipeline_layout_grid = VK_NULL_HANDLE;
        }
        if (render_context.descriptor_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(render_context.logical_device, render_context.descriptor_layout, render_context.allocator);
            render_context.descriptor_layout = VK_NULL_HANDLE;
        }
        if (render_context.pipeline_layout_3d != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(render_context.logical_device, render_context.pipeline_layout_3d, render_context.allocator);
            render_context.pipeline_layout_3d = VK_NULL_HANDLE;
        }

        for (uint32_t i = 0; i < render_context.swapchain_main.image_count; ++i) {
            if (render_context.swapchain_main.framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(render_context.logical_device, render_context.swapchain_main.framebuffers[i], render_context.allocator);
                render_context.swapchain_main.framebuffers[i] = VK_NULL_HANDLE;
            }
            if (render_context.swapchain_main.image_views[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(render_context.logical_device, render_context.swapchain_main.image_views[i], render_context.allocator);
                render_context.swapchain_main.image_views[i] = VK_NULL_HANDLE;
            }
        }

        // FIX: destroy sync objects using the real array lengths
        {
            VkDevice dev = render_context.logical_device;
            editor_window_swapchain* sc = &render_context.swapchain_main;

            const uint32_t acquire_sem_count  = (uint32_t)(sizeof(sc->image_available_semaphores) / sizeof(sc->image_available_semaphores[0]));
            const uint32_t finished_sem_count = (uint32_t)(sizeof(sc->render_finished_semaphores)  / sizeof(sc->render_finished_semaphores[0]));
            const uint32_t fence_count        = (uint32_t)(sizeof(sc->in_flight_fences)            / sizeof(sc->in_flight_fences[0]));

            for (uint32_t i = 0; i < acquire_sem_count; ++i) {
                if (sc->image_available_semaphores[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(dev, sc->image_available_semaphores[i], render_context.allocator);
                    sc->image_available_semaphores[i] = VK_NULL_HANDLE;
                }
            }
            for (uint32_t i = 0; i < finished_sem_count; ++i) {
                if (sc->render_finished_semaphores[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(dev, sc->render_finished_semaphores[i], render_context.allocator);
                    sc->render_finished_semaphores[i] = VK_NULL_HANDLE;
                }
            }
            for (uint32_t i = 0; i < fence_count; ++i) {
                if (sc->in_flight_fences[i] != VK_NULL_HANDLE) {
                    vkDestroyFence(dev, sc->in_flight_fences[i], render_context.allocator);
                    sc->in_flight_fences[i] = VK_NULL_HANDLE;
                }
            }
        }

        if (render_context.renderpass_main != VK_NULL_HANDLE) {
            vkDestroyRenderPass(render_context.logical_device, render_context.renderpass_main, render_context.allocator);
            render_context.renderpass_main = VK_NULL_HANDLE;
        }

        if (render_context.swapchain_main.handle != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(render_context.logical_device, render_context.swapchain_main.handle, render_context.allocator);
            render_context.swapchain_main.handle = VK_NULL_HANDLE;
        }

        if (render_context.command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(render_context.logical_device, render_context.command_pool, render_context.allocator);
            render_context.command_pool = VK_NULL_HANDLE;
        }

        vkDestroyDevice(render_context.logical_device, render_context.allocator);
        render_context.logical_device = VK_NULL_HANDLE;
    }

    if (render_context.surface_main != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(render_context.instance, render_context.surface_main, render_context.allocator);
        render_context.surface_main = VK_NULL_HANDLE;
    }

#if defined(PHOENIX_VULKAN_VALIDATION)
    if (g_debug_messenger != VK_NULL_HANDLE && render_context.instance != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_fn =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(render_context.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_fn) {
            destroy_fn(render_context.instance, g_debug_messenger, render_context.allocator);
        }
        g_debug_messenger = VK_NULL_HANDLE;
    }
#endif

    if (render_context.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(render_context.instance, render_context.allocator);
        render_context.instance = VK_NULL_HANDLE;
    }
}



void vulkan_render_backend_on_resized(render_backend* backend, int width, int height)
{
    (void)backend;
    cached_framebuffer_width = width;
    cached_framebuffer_height = height;
    framebuffer_size_generation++;
    phoenix_info("VULKAN", "Vulkan renderer backend->resized: w/h/gen: %i/%i/%llu", width, height, framebuffer_size_generation);
}



bool begin_vulkan_render_backend_frame(render_backend* backend, float delta_time) 
{
    (void)backend; (void)delta_time;

    // 1. CPU Pace Pacing: Stalls the host thread if the GPU is still working on this specific CPU frame slot
    VkFence cpu_fence = render_context.swapchain_main.in_flight_fences[current_inflight_cpu_frame];
    if (cpu_fence != VK_NULL_HANDLE) {
        vkWaitForFences(render_context.logical_device, 1, &cpu_fence, VK_TRUE, max_unsigned_aisle);
    }

    // 2. Acquire the next texture image index target slot out of the hardware device tray
    uint32_t acquired_image_index = 0;
    VkResult acquire_result = vkAcquireNextImageKHR(
        render_context.logical_device,
        render_context.swapchain_main.handle,
        max_unsigned_aisle,
        render_context.swapchain_main.image_available_semaphores[current_inflight_cpu_frame], // Linked to CPU loop slot
        VK_NULL_HANDLE,
        &acquired_image_index
    );

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        vulkan_render_backend_on_resized(backend, 0, 0);
        return false;
    } else if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
        phoenix_error("VULKAN", "Failed to acquire next swapchain image target handle.");
        return false;
    }

    // Cache the verified image slot destination index safely into global context tracking variables
    render_context.swapchain_main.current_frame = acquired_image_index;

    // 3. Clear and reset the recorded command buffer context linked to this specific texture buffer
    VkCommandBuffer cmd = render_context.swapchain_main.command_buffers[acquired_image_index];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
        phoenix_error("VULKAN", "Failed to transition target command buffer to an active recording state.");
        return false;
    }

    return true;
}

bool end_vulkan_render_backend_frame(render_backend* backend, float delta_time) 
{
    (void)backend; (void)delta_time;
    
    uint32_t active_image_index = render_context.swapchain_main.current_frame;
    VkCommandBuffer cmd = render_context.swapchain_main.command_buffers[active_image_index];

    // 1. Formally close the hardware recording line to compile executable binary command pools
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        phoenix_error("VULKAN", "Failed to compile command buffer executable structures.");
        return false;
    }

    // 2. Fence Reset: Safe host reset of the target tracking fence BEFORE queue submission operations run
    VkFence cpu_fence = render_context.swapchain_main.in_flight_fences[current_inflight_cpu_frame];
    if (cpu_fence != VK_NULL_HANDLE) {
        vkResetFences(render_context.logical_device, 1, &cpu_fence);
    }

    // === SWAPCHAIN SEMAPHORE REUSE MATRIX FIX ===
    // Acquire tracking remains bound to the CPU frame slot loop pipeline.
    VkSemaphore wait_primitive   = render_context.swapchain_main.image_available_semaphores[current_inflight_cpu_frame];
    
    // FIX: Index render_finished_semaphores by active_image_index instead of current_inflight_cpu_frame.
    // This guarantees the semaphore can only be reused once the image has been re-acquired.
    VkSemaphore signal_primitive = render_context.swapchain_main.render_finished_semaphores[active_image_index];

    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    VkPipelineStageFlags wait_destination_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &wait_primitive;
    submit_info.pWaitDstStageMask = &wait_destination_stage_mask;

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal_primitive;

    // Submit the command block to the hardware pipeline and bind its tracking fence update handle
    if (vkQueueSubmit(render_context.graphics_queue, 1, &submit_info, cpu_fence) != VK_SUCCESS) {
        phoenix_error("VULKAN", "Graphics queue execution submission pipeline dropped or rejected command arrays.");
        return false;
    }

    // 4. Issue the presentation descriptors to route the rendered backbuffer to the monitor display
    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &signal_primitive; // Present engine waits for rendering on this specific texture slot to finish
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &render_context.swapchain_main.handle;
    present_info.pImageIndices = &active_image_index; // Explicitly targets the hardware destination slot texture
    present_info.pResults = NULL;

    VkResult present_result = vkQueuePresentKHR(render_context.graphics_queue, &present_info);

    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
        vulkan_render_backend_on_resized(backend, 0, 0);
    } else if (present_result != VK_SUCCESS) {
        phoenix_error("VULKAN", "Hardware backbuffer presentation queue submission failed completely.");
        return false;
    }

    // 5. Turn over the CPU inflight ring buffer frame index slot for the subsequent loop cycle step
    current_inflight_cpu_frame = (current_inflight_cpu_frame + 1) % MAX_FRAMES_IN_FLIGHT;

    return true;
}


int find_memory_index(unsigned int type_filter, unsigned int property_flags) 
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(render_context.physical_device, &memory_properties);
    for (unsigned int i = 0; i < memory_properties.memoryTypeCount; ++i)
    {
        if (type_filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags) 
        {
            return (int)i;
        }
    }
    phoenix_warn("VULKAN", "Unable to find suitable hardware memory index parameters!");
    return -1;
}


void create_command_buffers(render_backend* backend) {
    (void)backend;
    vulkan_system_context* vk_context = (vulkan_system_context*)&render_context;
    uint32_t count = render_context.swapchain_main.image_count;

    if (count == 0 || count > 8) count = 3; // Enforce safe hardware fallback tracking bounds

    VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = render_context.command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = count;

    VkCommandBuffer temp_buffers[8] = {0};
    if (vkAllocateCommandBuffers(render_context.logical_device, &allocate_info, temp_buffers) == VK_SUCCESS) {
        for (uint32_t i = 0; i < count; ++i) {
            render_context.swapchain_main.command_buffers[i] = temp_buffers[i];
        }
        render_context.swapchain_main.image_count = count;
        phoenix_info("VULKAN", "Allocated %u command buffers safely.", count);
    } else {
        phoenix_fatal("VULKAN", "%s", "Failed to allocate frame recording command buffers!");
    }
}


// =========================================================================
// 🛠️ CRASH FIX: LIVE NATIVE SWAPCHAIN FRAMEBUFFER ATTACHMENT INJECTIONS
// =========================================================================
void regenerate_framebuffers(render_backend* backend, editor_window_swapchain* swapchain, VkRenderPass* renderpass) {
    (void)backend;
    
    for (uint32_t i = 0; i < swapchain->image_count; ++i) 
    {
        if (swapchain->framebuffers[i]) {
            vkDestroyFramebuffer(render_context.logical_device, swapchain->framebuffers[i], render_context.allocator);
            swapchain->framebuffers[i] = VK_NULL_HANDLE;
        }

        /* Main (UI) pass: one 1-sample color attachment = the swapchain image. */
        VkImageView attachments[1] = { swapchain->image_views[i] };
        const uint32_t attachment_total = 1;

        VkFramebufferCreateInfo framebuffer_create_info = {0};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = *renderpass;
        framebuffer_create_info.attachmentCount = attachment_total;
        framebuffer_create_info.pAttachments = attachments;
        framebuffer_create_info.width = swapchain->extent.width;
        framebuffer_create_info.height = swapchain->extent.height;
        framebuffer_create_info.layers = 1;

        validate_vulkan_result(vkCreateFramebuffer(
            render_context.logical_device, 
            &framebuffer_create_info, 
            render_context.allocator, 
            &swapchain->framebuffers[i]
        ));
    }
    phoenix_info("VULKAN", "%s", "Framebuffers regenerated successfully for main swapchain.");
}




bool recreate_swapchain(render_backend* backend) {
    if (recreating_swapchain) return FALSE;
    
    // Fallback protection: If the platform minimizes, width/height drop to 0. Stop allocation.
    if (cached_framebuffer_width == 0 || cached_framebuffer_height == 0) {
        return FALSE;
    }
    
    recreating_swapchain = TRUE;
    vkDeviceWaitIdle(render_context.logical_device);

    // 1. Wipe outstanding sync states safely
    for (unsigned int i = 0; i < 8; ++i) {
        render_context.swapchain_main.images_in_flight[i] = VK_NULL_HANDLE;
    }

    vulkan_system_context* vk_context = (vulkan_system_context*)&render_context;

    // 2. Clean up obsolete image view structures before compiling fresh paths
    // This maps matching unmapping hooks directly over your global arrays
    for (uint32_t i = 0; i < render_context.swapchain_main.image_count; ++i) {
        if (render_context.swapchain_main.image_views[i]) {
            vkDestroyImageView(render_context.logical_device, render_context.swapchain_main.image_views[i], render_context.allocator);
            render_context.swapchain_main.image_views[i] = VK_NULL_HANDLE;
        }
    }
    
    if (render_context.swapchain_main.handle) {
        vkDestroySwapchainKHR(render_context.logical_device, render_context.swapchain_main.handle, render_context.allocator);
        render_context.swapchain_main.handle = VK_NULL_HANDLE;
    }

    // 3. Compile your modern modular layout parameters safely
    create_vulkan_swapchain(
        vk_context,
        (int)cached_framebuffer_width,
        (int)cached_framebuffer_height,
        (vulkan_swapchain*)&render_context.swapchain_main
    );

    // Sync structural container extents cleanly from driver clamps
    render_context.swapchain_main.extent.width = cached_framebuffer_width;
    render_context.swapchain_main.extent.height = cached_framebuffer_height;
    
    cached_framebuffer_width = 0;
    cached_framebuffer_height = 0;
    framebuffer_size_last_generation = framebuffer_size_generation;

    // 4. Re-generate dependent graphics pipelines 
    regenerate_framebuffers(backend, &render_context.swapchain_main, &render_context.renderpass_main);
    
    vkFreeCommandBuffers(render_context.logical_device, render_context.command_pool,
                     render_context.swapchain_main.image_count,
                     render_context.swapchain_main.command_buffers);

    // Allocate fresh primary command streams into fixed arrays 
    create_command_buffers(backend);

    recreating_swapchain = false;
    phoenix_info("VULKAN", "Swapchain runtime recreation process finalized safely.");
    return TRUE;
}