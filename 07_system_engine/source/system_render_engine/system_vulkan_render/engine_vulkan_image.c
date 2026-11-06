#include "engine_globals.h"


void create_vulkan_image(
    vulkan_system_context* context,
    VkImageType image_type,
    int width,
    int height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    int create_view,
    VkImageAspectFlags view_aspect_flags,
    vulkan_image* out_image) {

    (void)image_type; // Prevent unused parameter alerts
    // Copy params
    out_image->width = width;
    out_image->height = height;

    // Creation info.
    VkImageCreateInfo image_create_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.extent.depth = 1;  
    image_create_info.mipLevels = 4;     
    image_create_info.arrayLayers = 1;   
    image_create_info.format = format;
    image_create_info.tiling = tiling;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = usage;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;          
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  

    // FIXED: Uses your updated type-safe checker token
    validate_vulkan_result(vkCreateImage(context->device.logical_device, &image_create_info, context->allocator, &out_image->handle));

    // Query memory requirements.
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(context->device.logical_device, out_image->handle, &memory_requirements);

    int memory_type = context->find_memory_index(memory_requirements.memoryTypeBits, memory_flags);
    if (memory_type == -1) {
        // FIXED: Added missing unified module string parameter context cleanly
        phoenix_error("VULKAN", "Required memory type not found. Image not valid.");
    }

    // Allocate memory
    VkMemoryAllocateInfo memory_allocate_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = memory_type;
    validate_vulkan_result(vkAllocateMemory(context->device.logical_device, &memory_allocate_info, context->allocator, &out_image->memory));

    // Bind the memory
    validate_vulkan_result(vkBindImageMemory(context->device.logical_device, out_image->handle, out_image->memory, 0));  

    // Create view
    if (create_view) {
        out_image->view = 0;
        create_vulkan_image_view(context, format, out_image, view_aspect_flags);
    }
}

void create_vulkan_image_view(
    vulkan_system_context* context,
    VkFormat format,
    vulkan_image* image,
    VkImageAspectFlags aspect_flags) {
    VkImageViewCreateInfo view_create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_create_info.image = image->handle;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;  
    view_create_info.format = format;
    view_create_info.subresourceRange.aspectMask = aspect_flags;

    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    validate_vulkan_result(vkCreateImageView(context->device.logical_device, &view_create_info, context->allocator, &image->view));
}

void remove_vulkan_image(vulkan_system_context* context, vulkan_image* image) {
    if (image->view) {
        vkDestroyImageView(context->device.logical_device, image->view, context->allocator);
        image->view = 0;
    }
    if (image->memory) {
        vkFreeMemory(context->device.logical_device, image->memory, context->allocator);
        image->memory = 0;
    }
    if (image->handle) {
        vkDestroyImage(context->device.logical_device, image->handle, context->allocator);
        image->handle = 0;
    }
}
