#include "engine_globals.h"

/* all state that used to be file static lives in engine_globals.c (g_cached_framebuffer_*, g_current_inflight_cpu_frame,
   g_debug_messenger, g_ui_fallback_*, g_vulkan_runtime_context). prototypes live in engine_render.h. */

VKAPI_ATTR VkBool32 VKAPI_CALL handle_vulkan_debug_message(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    (void)message_types;
    (void)user_data;
    switch (message_severity) {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            phoenix_error("vulkan_validation", "%s", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            phoenix_warn("vulkan_validation", "%s", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            phoenix_info("vulkan_validation", "%s", callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            phoenix_trace("vulkan_validation", "%s", callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}

bool create_ui_fallback_descriptor(void)
{
    VkDevice device = render_context.logical_device;

    if (render_context.descriptor_layout == VK_NULL_HANDLE) {
        phoenix_error("vulkan", "%s", "ui descriptor layout missing; cannot create fallback descriptor.");
        return false;
    }

    /* 1. 1x1 image */
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

    if (vkCreateImage(device, &image_info, render_context.allocator, &g_ui_fallback_image) != VK_SUCCESS) {
        phoenix_error("vulkan", "%s", "failed to create fallback ui image.");
        return false;
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device, g_ui_fallback_image, &mem_req);
    int mem_index = find_memory_index(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_index < 0) {
        phoenix_error("vulkan", "%s", "no memory type for fallback ui image.");
        return false;
    }

    VkMemoryAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc_info.allocationSize  = mem_req.size;
    alloc_info.memoryTypeIndex = (uint32_t)mem_index;
    if (vkAllocateMemory(device, &alloc_info, render_context.allocator, &g_ui_fallback_memory) != VK_SUCCESS ||
        vkBindImageMemory(device, g_ui_fallback_image, g_ui_fallback_memory, 0) != VK_SUCCESS) {
        phoenix_error("vulkan", "%s", "failed to allocate or bind fallback ui image memory.");
        return false;
    }

    /* 2. view */
    VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image    = g_ui_fallback_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format   = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &view_info, render_context.allocator, &g_ui_fallback_view) != VK_SUCCESS) {
        phoenix_error("vulkan", "%s", "failed to create fallback ui image view.");
        return false;
    }

    /* 3. sampler */
    VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.magFilter    = VK_FILTER_LINEAR;
    sampler_info.minFilter    = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxLod       = 1.0f;
    if (vkCreateSampler(device, &sampler_info, render_context.allocator, &g_ui_fallback_sampler) != VK_SUCCESS) {
        phoenix_error("vulkan", "%s", "failed to create fallback ui sampler.");
        return false;
    }

    /* 4. descriptor pool + set */
    VkDescriptorPoolSize pool_size = {0};
    pool_size.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes    = &pool_size;
    pool_info.maxSets       = 1;
    if (vkCreateDescriptorPool(device, &pool_info, render_context.allocator, &g_ui_fallback_pool) != VK_SUCCESS) {
        phoenix_error("vulkan", "%s", "failed to create fallback ui descriptor pool.");
        return false;
    }

    VkDescriptorSetAllocateInfo set_alloc = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    set_alloc.descriptorPool     = g_ui_fallback_pool;
    set_alloc.descriptorSetCount = 1;
    set_alloc.pSetLayouts        = &render_context.descriptor_layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &set_alloc, &set) != VK_SUCCESS) {
        phoenix_error("vulkan", "%s", "failed to allocate fallback ui descriptor set.");
        return false;
    }

    VkDescriptorImageInfo descriptor_image = {0};
    descriptor_image.sampler     = g_ui_fallback_sampler;
    descriptor_image.imageView   = g_ui_fallback_view;
    descriptor_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet          = set;
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &descriptor_image;
    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

    /* 5. one time submit: undefined -> transfer_dst, clear, -> shader_read_only */
    VkCommandBufferAllocateInfo cmd_alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmd_alloc.commandPool        = render_context.command_pool;
    cmd_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &cmd_alloc, &cmd) != VK_SUCCESS) {
        phoenix_error("vulkan", "%s", "failed to allocate fallback ui upload command buffer.");
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
    to_transfer.image               = g_ui_fallback_image;
    to_transfer.subresourceRange    = range;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &to_transfer);

    VkClearColorValue clear_color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    vkCmdClearColorImage(cmd, g_ui_fallback_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &range);

    VkImageMemoryBarrier to_shader = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    to_shader.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    to_shader.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.image               = g_ui_fallback_image;
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

    /* 6. publish the set so draw_frontend_render_frame always has something valid to bind.
          only where offscreen_create_resources() has not already put the real scene / game texture:
          this runs after it, and overwriting would make the scene / game views sample this 1x1 grey texture. */
    g_ui_fallback_set = set;
    if (render_context.scene_viewport_target.descriptor_set == VK_NULL_HANDLE) {
        render_context.scene_viewport_target.descriptor_set = set;
    }
    if (render_context.game_viewport_target.descriptor_set == VK_NULL_HANDLE) {
        render_context.game_viewport_target.descriptor_set = set;
    }

    phoenix_info("vulkan", "%s", "fallback ui descriptor set created.");
    return true;
}

void destroy_ui_fallback_descriptor(void)
{
    VkDevice device = render_context.logical_device;
    if (device == VK_NULL_HANDLE) {
        return;
    }

    if (g_ui_fallback_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, g_ui_fallback_pool, render_context.allocator);
        g_ui_fallback_pool = VK_NULL_HANDLE;
    }
    if (g_ui_fallback_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, g_ui_fallback_sampler, render_context.allocator);
        g_ui_fallback_sampler = VK_NULL_HANDLE;
    }
    if (g_ui_fallback_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, g_ui_fallback_view, render_context.allocator);
        g_ui_fallback_view = VK_NULL_HANDLE;
    }
    if (g_ui_fallback_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, g_ui_fallback_image, render_context.allocator);
        g_ui_fallback_image = VK_NULL_HANDLE;
    }
    if (g_ui_fallback_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, g_ui_fallback_memory, render_context.allocator);
        g_ui_fallback_memory = VK_NULL_HANDLE;
    }
    /* only clear the fields that still point at the fallback; the offscreen module owns the others */
    if (render_context.scene_viewport_target.descriptor_set == g_ui_fallback_set) {
        render_context.scene_viewport_target.descriptor_set = VK_NULL_HANDLE;
    }
    if (render_context.game_viewport_target.descriptor_set == g_ui_fallback_set) {
        render_context.game_viewport_target.descriptor_set = VK_NULL_HANDLE;
    }
    g_ui_fallback_set = VK_NULL_HANDLE;
}




bool start_vulkan_render_backend(render_backend* backend, const char* application_name)
{
    phoenix_info("vulkan", "configuring backend graphics structures for: %s", application_name);

    render_context.allocator = NULL;

    get_program_framebuffer_size(&g_cached_framebuffer_width, &g_cached_framebuffer_height);
    render_context.swapchain_main.extent.width = 800;
    if (g_cached_framebuffer_width != 0) {
        render_context.swapchain_main.extent.width = g_cached_framebuffer_width;
    }
    render_context.swapchain_main.extent.height = 600;
    if (g_cached_framebuffer_height != 0) {
        render_context.swapchain_main.extent.height = g_cached_framebuffer_height;
    }
    g_cached_framebuffer_width = 0;
    g_cached_framebuffer_height = 0;

    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_2;
    app_info.pApplicationName = application_name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "phoenix engine";
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
        phoenix_fatal("vulkan", "%s", "vulkan instance creation failed.");
        parry_destroy(required_extensions);
        if (required_validation_layer_names) {
            parry_destroy(required_validation_layer_names);
        }
        return FALSE;
    }
    phoenix_info("vulkan", "%s", "vulkan instance created.");

    parry_destroy(required_extensions);
    if (required_validation_layer_names) {
        parry_destroy(required_validation_layer_names);
    }

#if defined(PHOENIX_VULKAN_VALIDATION)
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = handle_vulkan_debug_message;

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
        phoenix_fatal("vulkan", "%s", "native platform window wsi binding mapping failure.");
        return FALSE;
    }

    vulkan_system_context* vk_context = &g_vulkan_runtime_context;
    memset(vk_context, 0, sizeof(vulkan_system_context));

    vk_context->instance = render_context.instance;
    vk_context->allocator = render_context.allocator;
    vk_context->surface = render_context.surface_main;
    vk_context->find_memory_index = find_memory_index;

    if (!create_vulkan_device(vk_context)) {
        phoenix_error("vulkan", "%s", "failed to compile hardware logical device drivers!");
        return FALSE;
    }

    render_context.physical_device = vk_context->device.physical_device;
    render_context.logical_device  = vk_context->device.logical_device;
    render_context.graphics_queue  = vk_context->device.graphics_queue;
    render_context.command_pool    = vk_context->device.graphics_command_pool;

    int target_width = (int)render_context.swapchain_main.extent.width;
    int target_height = (int)render_context.swapchain_main.extent.height;

    create_vulkan_swapchain(vk_context, target_width, target_height, &vk_context->swapchain);

    create_vulkan_renderpass(
        vk_context,
        &vk_context->main_renderpass,
        0.0f, 0.0f, (float)target_width, (float)target_height,
        0.0f, 0.0f, 0.2f, 1.0f,
        1.0f, 0
    );

    render_context.renderpass_main = vk_context->main_renderpass.handle;

    /* offscreen scene pass (msaa color + depth -> resolve). grid and mesh pipelines are built against it. */
    phoenix_trace("vulkan", "%s", "step a: calling editor create_offscreen_renderpass");
    if (!g_editor.create_offscreen_renderpass(vk_context->device.logical_device,
                                               render_context.swapchain_main.image_format,
                                               vk_context->device.depth_format)) {
        phoenix_fatal("vulkan", "%s", "failed to create the offscreen scene render pass!");
        return FALSE;
    }

    phoenix_trace("vulkan", "%s", "step b: editor offscreen renderpass returned");
    VkRenderPass offscreen_pass = g_editor.get_offscreen_renderpass();
    phoenix_trace("vulkan", "offscreen_get_renderpass returned=%p low=%llx", (void*)offscreen_pass,
                  (unsigned long long)(uintptr_t)offscreen_pass);
    if (offscreen_pass == VK_NULL_HANDLE) {
        phoenix_fatal("vulkan", "%s", "editor dll returned a null offscreen render pass!");
        return FALSE;
    }
    render_context.offscreen_renderpass = offscreen_pass;

    phoenix_trace("vulkan", "%s", "step d: building graphics pipelines");
    if (!create_vulkan_graphics_pipelines(vk_context, render_context.renderpass_main)) {
        phoenix_fatal("vulkan", "%s", "failed to compile the decoupled ui and 3d graphics pipeline suites!");
        return FALSE;
    }

    render_context.graphics_pipeline      = graphics_pipeline_ui;
    render_context.graphics_pipeline_grid = graphics_pipeline_grid;
    render_context.pipeline_layout        = pipeline_layout_ui;

    phoenix_trace("vulkan", "%s", "step e: graphics pipelines done, calling editor create_offscreen_resources");
    if (!g_editor.create_offscreen_resources()) {
        phoenix_fatal("vulkan", "%s", "failed to create offscreen sampler / descriptor sets!");
        return FALSE;
    }

    phoenix_trace("vulkan", "%s", "step f: offscreen resources done, creating ui fallback descriptor");
    if (!create_ui_fallback_descriptor()) {
        phoenix_fatal("vulkan", "%s", "failed to create fallback ui descriptor set!");
        return FALSE;
    }

    phoenix_trace("vulkan", "%s", "step g: creating command buffers and framebuffers");
    create_command_buffers(backend);
    regenerate_framebuffers(backend, &render_context.swapchain_main, &render_context.renderpass_main);
    phoenix_trace("vulkan", "%s", "step h: framebuffers done, creating sync objects");

    /* size every loop from the real array length (no out of bounds writes) */
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

    phoenix_info("vulkan", "%s", "vulkan renderer initialized successfully.");
    return TRUE;
}



void shutdown_vulkan_render_backend(render_backend* backend)
{
    (void)backend;
    phoenix_info("vulkan", "destroying vulkan hardware allocation contexts cleanly.");

    if (render_context.logical_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(render_context.logical_device);

        destroy_ui_fallback_descriptor();

        /* the offscreen pass belongs to the editor: go through the plugin table, the engine never links editor symbols */
        if (g_editor.shutdown_offscreen) {
            g_editor.shutdown_offscreen();
        }
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

        /* destroy sync objects using the real array lengths */
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
    g_cached_framebuffer_width = width;
    g_cached_framebuffer_height = height;
    g_framebuffer_size_generation++;
    phoenix_info("vulkan", "vulkan renderer backend->resized: w/h/gen: %i/%i/%llu", width, height, g_framebuffer_size_generation);
}



bool begin_vulkan_render_backend_frame(render_backend* backend, float delta_time)
{
    (void)delta_time;

    /* 1. cpu pacing: stalls the host thread if the gpu is still working on this cpu frame slot */
    VkFence cpu_fence = render_context.swapchain_main.in_flight_fences[g_current_inflight_cpu_frame];
    if (cpu_fence != VK_NULL_HANDLE) {
        vkWaitForFences(render_context.logical_device, 1, &cpu_fence, VK_TRUE, max_unsigned_aisle);
    }

    /* 2. acquire the next swapchain image index */
    uint32_t acquired_image_index = 0;
    VkResult acquire_result = vkAcquireNextImageKHR(
        render_context.logical_device,
        render_context.swapchain_main.handle,
        max_unsigned_aisle,
        render_context.swapchain_main.image_available_semaphores[g_current_inflight_cpu_frame],
        VK_NULL_HANDLE,
        &acquired_image_index
    );

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        vulkan_render_backend_on_resized(backend, 0, 0);
        return false;
    } else if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
        phoenix_error("vulkan", "failed to acquire next swapchain image target handle.");
        return false;
    }

    /* cache the verified image slot index */
    render_context.swapchain_main.current_frame = acquired_image_index;

    /* 3. reset this cpu frame slot's command buffer and start recording. It is the slot's fence that was just waited on, so the buffer is
       guaranteed finished. (Indexing by image used to reset a buffer another slot was still executing: device lost on a slow gpu frame.) */
    VkCommandBuffer cmd = render_context.swapchain_main.command_buffers[g_current_inflight_cpu_frame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
        phoenix_error("vulkan", "failed to transition target command buffer to an active recording state.");
        return false;
    }

    return true;
}

bool end_vulkan_render_backend_frame(render_backend* backend, float delta_time)
{
    (void)delta_time;

    uint32_t active_image_index = render_context.swapchain_main.current_frame;
    VkCommandBuffer cmd = render_context.swapchain_main.command_buffers[g_current_inflight_cpu_frame];

    /* 1. close the recording */
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        phoenix_error("vulkan", "failed to compile command buffer executable structures.");
        return false;
    }

    /* 2. reset the tracking fence before queue submission */
    VkFence cpu_fence = render_context.swapchain_main.in_flight_fences[g_current_inflight_cpu_frame];
    if (cpu_fence != VK_NULL_HANDLE) {
        vkResetFences(render_context.logical_device, 1, &cpu_fence);
    }

    /* acquire tracking stays bound to the cpu frame slot. render_finished is indexed by the image index so a
       semaphore can only be reused once its image has been re-acquired. */
    VkSemaphore wait_primitive   = render_context.swapchain_main.image_available_semaphores[g_current_inflight_cpu_frame];
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

    if (vkQueueSubmit(render_context.graphics_queue, 1, &submit_info, cpu_fence) != VK_SUCCESS) {
        phoenix_error("vulkan", "graphics queue execution submission pipeline dropped or rejected command arrays.");
        return false;
    }

    /* 4. present */
    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &signal_primitive;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &render_context.swapchain_main.handle;
    present_info.pImageIndices = &active_image_index;
    present_info.pResults = NULL;

    VkResult present_result = vkQueuePresentKHR(render_context.graphics_queue, &present_info);

    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
        vulkan_render_backend_on_resized(backend, 0, 0);
    } else if (present_result != VK_SUCCESS) {
        phoenix_error("vulkan", "hardware backbuffer presentation queue submission failed completely.");
        return false;
    }

    /* 5. turn over the cpu in flight ring index for the next frame */
    g_current_inflight_cpu_frame = (g_current_inflight_cpu_frame + 1) % vulkan_frames_in_flight_count;

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
    phoenix_warn("vulkan", "unable to find suitable hardware memory index parameters!");
    return -1;
}


void create_command_buffers(render_backend* backend)
{
    (void)backend;
    uint32_t count = render_context.swapchain_main.image_count;

    if (count == 0 || count > 8) {
        count = 3;   /* enforce safe hardware fallback bounds */
    }

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
        phoenix_info("vulkan", "allocated %u command buffers safely.", count);
    } else {
        phoenix_fatal("vulkan", "%s", "failed to allocate frame recording command buffers!");
    }
}


void regenerate_framebuffers(render_backend* backend, editor_window_swapchain* swapchain, VkRenderPass* renderpass)
{
    (void)backend;

    for (uint32_t i = 0; i < swapchain->image_count; ++i)
    {
        if (swapchain->framebuffers[i]) {
            vkDestroyFramebuffer(render_context.logical_device, swapchain->framebuffers[i], render_context.allocator);
            swapchain->framebuffers[i] = VK_NULL_HANDLE;
        }

        /* main (ui) pass: one 1 sample color attachment = the swapchain image. */
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
    phoenix_info("vulkan", "%s", "framebuffers regenerated successfully for main swapchain.");
}




bool recreate_swapchain(render_backend* backend)
{
    if (g_recreating_swapchain) {
        return FALSE;
    }

    /* if the platform minimizes, width/height drop to 0. stop allocation. */
    if (g_cached_framebuffer_width == 0 || g_cached_framebuffer_height == 0) {
        return FALSE;
    }

    g_recreating_swapchain = TRUE;
    vkDeviceWaitIdle(render_context.logical_device);

    /* 1. wipe outstanding sync states */
    for (unsigned int i = 0; i < 8; ++i) {
        render_context.swapchain_main.images_in_flight[i] = VK_NULL_HANDLE;
    }

    /* the real vulkan context (was a bad cast of render_context, which is a different struct) */
    vulkan_system_context* vk_context = &g_vulkan_runtime_context;

    /* 2. clean up obsolete image views before compiling fresh ones */
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

    /* 3. build the new swapchain */
    create_vulkan_swapchain(
        vk_context,
        (int)g_cached_framebuffer_width,
        (int)g_cached_framebuffer_height,
        &vk_context->swapchain
    );

    /* sync structural extents from driver clamps */
    render_context.swapchain_main.extent.width = g_cached_framebuffer_width;
    render_context.swapchain_main.extent.height = g_cached_framebuffer_height;

    g_cached_framebuffer_width = 0;
    g_cached_framebuffer_height = 0;
    g_framebuffer_size_last_generation = g_framebuffer_size_generation;

    /* 4. regenerate dependent framebuffers */
    regenerate_framebuffers(backend, &render_context.swapchain_main, &render_context.renderpass_main);

    vkFreeCommandBuffers(render_context.logical_device, render_context.command_pool,
                     render_context.swapchain_main.image_count,
                     render_context.swapchain_main.command_buffers);

    /* allocate fresh primary command buffers into the fixed arrays */
    create_command_buffers(backend);

    g_recreating_swapchain = false;
    phoenix_info("vulkan", "swapchain runtime recreation process finalized safely.");
    return TRUE;
}