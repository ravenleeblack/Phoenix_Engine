#include "engine_globals.h"


void get_program_framebuffer_size(unsigned int* width, unsigned int* height) 
{
    *width = (unsigned int)program_state.width;
    *height = (unsigned int)program_state.height;
}


void create_vulkan_framebuffer(
    vulkan_system_context* context,
    vulkan_renderpass* renderpass,
    int width,
    int height,
    int attachment_count,
    VkImageView* attachments,
    vulkan_framebuffer* out_framebuffer) {
    // Take a copy of the attachments, renderpass and attachment format
    out_framebuffer->attachments = kallocate(sizeof(VkImageView) * attachment_count, MEMORY_TAG_RENDERER);
    for (int i = 0; i < attachment_count; ++i) {
        out_framebuffer->attachments[i] = attachments[i];
    }
    out_framebuffer->renderpass = renderpass;
    out_framebuffer->attachment_count = attachment_count;

    // Creation info
    VkFramebufferCreateInfo framebuffer_create_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer_create_info.renderPass = renderpass->handle;
    framebuffer_create_info.attachmentCount = attachment_count;
    framebuffer_create_info.pAttachments = out_framebuffer->attachments;
    framebuffer_create_info.width = width;
    framebuffer_create_info.height = height;
    framebuffer_create_info.layers = 1;

    validate_vulkan_result(vkCreateFramebuffer(
        context->device.logical_device,
        &framebuffer_create_info,
        context->allocator,
        &out_framebuffer->handle));
}

void remove_vulkan_framebuffer(vulkan_system_context* context, vulkan_framebuffer* framebuffer) {
    vkDestroyFramebuffer(context->device.logical_device, framebuffer->handle, context->allocator);
    if (framebuffer->attachments) {
        kfree(framebuffer->attachments, sizeof(VkImageView) * framebuffer->attachment_count, MEMORY_TAG_RENDERER);
        framebuffer->attachments = 0;
    }
    framebuffer->handle = 0;
    framebuffer->attachment_count = 0;
    framebuffer->renderpass = 0;
}