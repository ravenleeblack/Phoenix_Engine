#include "engine_globals.h"



void create_vulkan_renderpass(
    vulkan_system_context* context,
    vulkan_renderpass* out_renderpass,
    float x, float y, float w, float h,
    float r, float g, float b, float a,
    float depth,
    int stencil)
{
    out_renderpass->x = x; out_renderpass->y = y;
    out_renderpass->w = w; out_renderpass->h = h;
    out_renderpass->r = r; out_renderpass->g = g;
    out_renderpass->b = b; out_renderpass->a = a;
    out_renderpass->depth = depth;
    out_renderpass->stencil = stencil;
    out_renderpass->state = vulkan_render_pass_state_not_allocated;

    /* The main (UI) pass is plain 1-sample color straight into the swapchain image.
       MSAA + depth live in the offscreen scene pass (system_offscreen.c). */
    VkAttachmentDescription attachments[1] = {{0}};

    VkAttachmentDescription* color_attachment = &attachments[0];
    color_attachment->format         = render_context.swapchain_main.image_format;
    color_attachment->samples        = VK_SAMPLE_COUNT_1_BIT;
    color_attachment->loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment->storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment->stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment->initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment->finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {0};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 2. Map attachments into layout subpass structures cleanly
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &color_attachment_ref;

    // Handle pipeline state synchronization transitions automatically
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments    = attachments;
    render_pass_info.subpassCount    = 1;
    render_pass_info.pSubpasses      = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies   = &dependency;

    VkDevice render_device = context->device.logical_device;
    if (render_device == VK_NULL_HANDLE) {
        phoenix_fatal("VULKAN", "%s", "Cannot create the master render pass with a null logical device!");
        return;
    }

    if (vkCreateRenderPass(
            render_device,
            &render_pass_info, 
            render_context.allocator, 
            &render_context.renderpass_main) != VK_SUCCESS) 
    {
        phoenix_fatal("VULKAN", "%s", "Failed to construct the master window renderpass context handle!");
        return;
    }

    out_renderpass->handle = render_context.renderpass_main;
    out_renderpass->state  = vulkan_render_pass_state_ready;

    phoenix_info("VULKAN", "%s", "Master UI VkRenderPass instantiated (1 sample, scene MSAA is offscreen).");
}



void remove_vulkan_renderpass(vulkan_system_context* context, vulkan_renderpass* renderpass) {
    if (renderpass && renderpass->handle) {
        vkDestroyRenderPass(context->device.logical_device, renderpass->handle, context->allocator);
        renderpass->handle = 0;
    }
}

void begin_vulkan_renderpass(
    vulkan_command_buffer* command_buffer,
    vulkan_renderpass* renderpass,
    VkFramebuffer frame_buffer) {
    VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin_info.renderPass = renderpass->handle;
    begin_info.framebuffer = frame_buffer;
    begin_info.renderArea.offset.x = renderpass->x;
    begin_info.renderArea.offset.y = renderpass->y;
    begin_info.renderArea.extent.width = renderpass->w;
    begin_info.renderArea.extent.height = renderpass->h;

    VkClearValue clear_values[1];
    kzero_memory(clear_values, sizeof(VkClearValue) * 1);
    clear_values[0].color.float32[0] = renderpass->r;
    clear_values[0].color.float32[1] = renderpass->g;
    clear_values[0].color.float32[2] = renderpass->b;
    clear_values[0].color.float32[3] = renderpass->a;
    begin_info.clearValueCount = 1;     /* the ui pass has one color attachment and no depth */
    begin_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    command_buffer->state = vulkan_command_buffer_state_in_render_pass;
}

void end_vulkan_renderpass(vulkan_command_buffer* command_buffer, vulkan_renderpass* renderpass) {
    vkCmdEndRenderPass(command_buffer->handle);
    command_buffer->state = vulkan_command_buffer_state_recording;
}