#include "engine_globals.h"

/* msaa color target. only exists when the active sample count is > 1. the swapchain image becomes the resolve target.
   the depth image, msaa image, memory and view live in engine_globals.c (g_swapchain_depth_image, g_msaa_*). */

VkImageView get_swapchain_msaa_view(void)
{
    return g_msaa_view;
}

void destroy_swapchain_msaa_image(void)
{
    VkDevice dev = render_context.logical_device;
    if (dev == VK_NULL_HANDLE) {
        return;
    }
    if (g_msaa_view) {
        vkDestroyImageView(dev, g_msaa_view, render_context.allocator);
        g_msaa_view = VK_NULL_HANDLE;
    }
    if (g_msaa_image) {
        vkDestroyImage(dev, g_msaa_image, render_context.allocator);
        g_msaa_image = VK_NULL_HANDLE;
    }
    if (g_msaa_memory) {
        vkFreeMemory(dev, g_msaa_memory, render_context.allocator);
        g_msaa_memory = VK_NULL_HANDLE;
    }
}

void create_swapchain_msaa_image(VkExtent2D extent, VkFormat format)
{
    VkSampleCountFlagBits samples = get_render_msaa_sample_count();
    if (samples == VK_SAMPLE_COUNT_1_BIT) {
        return;
    }
    VkDevice dev = render_context.logical_device;

    VkImageCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType     = VK_IMAGE_TYPE_2D;
    info.format        = format;
    info.extent.width  = extent.width;
    info.extent.height = extent.height;
    info.extent.depth  = 1;
    info.mipLevels     = 1;          /* multisampled images must have exactly 1 mip */
    info.arrayLayers   = 1;
    info.samples       = samples;
    info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    info.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    validate_vulkan_result(vkCreateImage(dev, &info, render_context.allocator, &g_msaa_image));

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(dev, g_msaa_image, &req);
    int mem = find_memory_index(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem < 0) {
        phoenix_error("vulkan", "%s", "no device local memory type for the msaa color target.");
        return;
    }

    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize  = req.size;
    alloc.memoryTypeIndex = (uint32_t)mem;
    validate_vulkan_result(vkAllocateMemory(dev, &alloc, render_context.allocator, &g_msaa_memory));
    validate_vulkan_result(vkBindImageMemory(dev, g_msaa_image, g_msaa_memory, 0));

    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image    = g_msaa_image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format   = format;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    validate_vulkan_result(vkCreateImageView(dev, &view, render_context.allocator, &g_msaa_view));

    phoenix_info("vulkan", "msaa color target created: %ux (%ux%u).", (unsigned)samples, extent.width, extent.height);
}


void create_vulkan_swapchain(
    vulkan_system_context* context,
    int width,
    int height,
    vulkan_swapchain* out_swapchain)
{
    create_vulkan_swapchain_objects(context, width, height, out_swapchain);
}

void recreate_vulkan_swapchain(
    vulkan_system_context* context,
    int width,
    int height,
    vulkan_swapchain* swapchain)
{
    destroy_vulkan_swapchain_objects(context, swapchain);
    create_vulkan_swapchain_objects(context, width, height, swapchain);
}

void remove_vulkan_swapchain(
    vulkan_system_context* context,
    vulkan_swapchain* swapchain)
{
    destroy_vulkan_swapchain_objects(context, swapchain);
}

bool acquire_vulkan_swapchain_next_image_index(
    vulkan_system_context* context,
    vulkan_swapchain* swapchain,
    unsigned long long timeout_ns,
    VkSemaphore image_available_semaphore,
    VkFence fence,
    int* out_image_index)
{
    uint32_t local_idx = 0;
    VkResult result = vkAcquireNextImageKHR(
        context->device.logical_device,
        swapchain->handle,
        timeout_ns,
        image_available_semaphore,
        fence,
        &local_idx);

    *out_image_index = (int)local_idx;

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_vulkan_swapchain(context, context->framebuffer_width, context->framebuffer_height, swapchain);
        return FALSE;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        phoenix_fatal("vulkan", "failed to acquire swapchain image!");
        return FALSE;
    }

    return TRUE;
}

void present_vulkan_swapchain(
    vulkan_system_context* context,
    vulkan_swapchain* swapchain,
    VkQueue graphics_queue,
    VkQueue present_queue,
    VkSemaphore render_complete_semaphore,
    int present_image_index)
{
    (void)graphics_queue;
    uint32_t image_idx = (uint32_t)present_image_index;

    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain->handle;
    present_info.pImageIndices = &image_idx;
    present_info.pResults = 0;

    VkResult result = vkQueuePresentKHR(present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate_vulkan_swapchain(context, context->framebuffer_width, context->framebuffer_height, swapchain);
    } else if (result != VK_SUCCESS) {
        phoenix_fatal("vulkan", "failed to present swap chain image!");
    }

    context->current_frame = (context->current_frame + 1) % swapchain->max_frames_in_flight;
}


void create_vulkan_swapchain_objects(vulkan_system_context* context, int width, int height, vulkan_swapchain* swapchain)
{
    (void)swapchain;

    /* validation gate fallback */
    if (!context || context->device.physical_device == VK_NULL_HANDLE) {
        if (context) {
            context->device.physical_device = render_context.physical_device;
        }
    }

    VkExtent2D swapchain_extent = {(uint32_t)width, (uint32_t)height};
    render_context.swapchain_main.current_frame = 0;

    query_vulkan_device_swapchain_support(
        render_context.physical_device,
        render_context.surface_main,
        &context->device.swapchain_support
    );

    bool found = FALSE;
    for (int i = 0; i < context->device.swapchain_support.format_count; ++i) {
        VkSurfaceFormatKHR format = context->device.swapchain_support.formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            render_context.swapchain_main.image_format = format.format;
            found = TRUE;
            break;
        }
    }

    if (!found && context->device.swapchain_support.format_count > 0) {
        render_context.swapchain_main.image_format = context->device.swapchain_support.formats[0].format;
    } else if (!found) {
        render_context.swapchain_main.image_format = VK_FORMAT_B8G8R8A8_UNORM;
    }

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (int i = 0; i < context->device.swapchain_support.present_mode_count; ++i) {
        VkPresentModeKHR mode = context->device.swapchain_support.present_modes[i];
        if (!g_render_settings.vsync && mode == VK_PRESENT_MODE_MAILBOX_KHR) {   /* vsync on = fifo */
            present_mode = mode;
            break;
        }
    }

    if (context->device.swapchain_support.capabilities.currentExtent.width != 0xFFFFFFFFu) {
        swapchain_extent = context->device.swapchain_support.capabilities.currentExtent;
    }

    VkExtent2D min = context->device.swapchain_support.capabilities.minImageExtent;
    VkExtent2D max = context->device.swapchain_support.capabilities.maxImageExtent;
    if (swapchain_extent.width < min.width) {
        swapchain_extent.width = min.width;
    }
    if (swapchain_extent.width > max.width) {
        swapchain_extent.width = max.width;
    }
    if (swapchain_extent.height < min.height) {
        swapchain_extent.height = min.height;
    }
    if (swapchain_extent.height > max.height) {
        swapchain_extent.height = max.height;
    }

    uint32_t image_count = context->device.swapchain_support.capabilities.minImageCount + 1;
    if (context->device.swapchain_support.capabilities.maxImageCount > 0 &&
        image_count > context->device.swapchain_support.capabilities.maxImageCount) {
        image_count = context->device.swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {0};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = render_context.surface_main;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = render_context.swapchain_main.image_format;
    swapchain_create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchain_create_info.imageExtent = swapchain_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    swapchain_create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices   = NULL;

    swapchain_create_info.preTransform = context->device.swapchain_support.capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    validate_vulkan_result(vkCreateSwapchainKHR(
        render_context.logical_device,
        &swapchain_create_info,
        render_context.allocator,
        &render_context.swapchain_main.handle
    ));

    render_context.swapchain_main.extent = swapchain_extent;

    uint32_t returned_image_count = 0;
    validate_vulkan_result(vkGetSwapchainImagesKHR(
        render_context.logical_device,
        render_context.swapchain_main.handle,
        &returned_image_count,
        NULL
    ));

    if (returned_image_count > 8) {
        returned_image_count = 8;
    }
    render_context.swapchain_main.image_count = returned_image_count;

    validate_vulkan_result(vkGetSwapchainImagesKHR(
        render_context.logical_device,
        render_context.swapchain_main.handle,
        &returned_image_count,
        render_context.swapchain_main.images
    ));

    for (uint32_t i = 0; i < render_context.swapchain_main.image_count; ++i) {
        VkImageViewCreateInfo view_info = {0};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = render_context.swapchain_main.images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = render_context.swapchain_main.image_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        validate_vulkan_result(vkCreateImageView(
            render_context.logical_device,
            &view_info,
            render_context.allocator,
            &render_context.swapchain_main.image_views[i]
        ));
    }

    if (!detect_vulkan_device_depth_format(&context->device)) {
        context->device.depth_format = VK_FORMAT_UNDEFINED;
        phoenix_fatal("vulkan", "%s", "failed to find a supported depth format!");
    }

    /* free any previous depth image (recreate path), then create into the global depth image */
    destroy_swapchain_depth_image();

    create_vulkan_image(
        context,
        VK_IMAGE_TYPE_2D,
        (int)swapchain_extent.width,
        (int)swapchain_extent.height,
        context->device.depth_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        TRUE,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        &g_swapchain_depth_image
    );

    /* msaa moved to the offscreen scene pass; the swapchain pass is 1 sample. */
    (void)create_swapchain_msaa_image;

    phoenix_info("vulkan", "%s", "swapchain created successfully.");
}



void destroy_vulkan_swapchain_objects(vulkan_system_context* context, vulkan_swapchain* swapchain)
{
    (void)context;
    (void)swapchain;   /* the live swapchain is render_context.swapchain_main, not the argument */
    VkDevice dev = render_context.logical_device;
    if (dev == VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(dev);
    destroy_swapchain_depth_image();

    for (uint32_t i = 0; i < render_context.swapchain_main.image_count; ++i) {
        if (render_context.swapchain_main.image_views[i]) {
            vkDestroyImageView(dev, render_context.swapchain_main.image_views[i], render_context.allocator);
            render_context.swapchain_main.image_views[i] = VK_NULL_HANDLE;
        }
    }
    if (render_context.swapchain_main.handle) {
        vkDestroySwapchainKHR(dev, render_context.swapchain_main.handle, render_context.allocator);
        render_context.swapchain_main.handle = VK_NULL_HANDLE;
    }
}


void destroy_swapchain_depth_image(void)
{
    if (render_context.logical_device == VK_NULL_HANDLE) {
        return;
    }

    destroy_swapchain_msaa_image();   /* runs on every swapchain (re)create and at shutdown, so the msaa target shares the depth image's lifetime */

    if (g_swapchain_depth_image.view) {
        vkDestroyImageView(render_context.logical_device, g_swapchain_depth_image.view, render_context.allocator);
        g_swapchain_depth_image.view = 0;
    }
    if (g_swapchain_depth_image.handle) {
        vkDestroyImage(render_context.logical_device, g_swapchain_depth_image.handle, render_context.allocator);
        g_swapchain_depth_image.handle = 0;
    }
    if (g_swapchain_depth_image.memory) {
        vkFreeMemory(render_context.logical_device, g_swapchain_depth_image.memory, render_context.allocator);
        g_swapchain_depth_image.memory = 0;
    }
}