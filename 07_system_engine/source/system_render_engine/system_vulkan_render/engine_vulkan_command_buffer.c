#include "engine_globals.h"



void allocate_vulkan_command_buffer(
    vulkan_system_context* context,
    VkCommandPool pool,
    bool is_primary,
    vulkan_command_buffer* out_command_buffer) {

    kzero_memory(out_command_buffer, sizeof(*out_command_buffer));

    VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate_info.commandPool = pool;
    allocate_info.level = is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocate_info.commandBufferCount = 1;
    allocate_info.pNext = 0;

    out_command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;
    validate_vulkan_result(vkAllocateCommandBuffers(
        context->device.logical_device,
        &allocate_info,
        &out_command_buffer->handle));
    out_command_buffer->state = COMMAND_BUFFER_STATE_READY;
}

void free_vulkan_command_buffer(
    vulkan_system_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* command_buffer) {
    vkFreeCommandBuffers(
        context->device.logical_device,
        pool,
        1,
        &command_buffer->handle);

    command_buffer->handle = 0;
    command_buffer->state = COMMAND_BUFFER_STATE_NOT_ALLOCATED;
}

void begin_vulkan_command_buffer(
    vulkan_command_buffer* command_buffer,
    bool is_single_use,
    bool is_renderpass_continue,
    bool is_simultaneous_use) {
    
    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = 0;
    if (is_single_use) {
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }
    if (is_renderpass_continue) {
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }
    if (is_simultaneous_use) {
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    }

    validate_vulkan_result(vkBeginCommandBuffer(command_buffer->handle, &begin_info));
    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING;
}

void end_vulkan_command_buffer(vulkan_command_buffer* command_buffer) {
    validate_vulkan_result(vkEndCommandBuffer(command_buffer->handle));
    command_buffer->state = COMMAND_BUFFER_STATE_RECORDING_ENDED;
}

void update_vulkan_command_buffer(vulkan_command_buffer* command_buffer) {
    command_buffer->state = COMMAND_BUFFER_STATE_SUBMITTED;
}

void reset_vulkan_command_buffer(vulkan_command_buffer* command_buffer) {
    command_buffer->state = COMMAND_BUFFER_STATE_READY;
}

void begin_vulkan_command_buffer_allocate_and_single_use(
    vulkan_system_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* out_command_buffer) {
    allocate_vulkan_command_buffer(context, pool, TRUE, out_command_buffer);
    begin_vulkan_command_buffer(out_command_buffer, TRUE, FALSE, FALSE);
}

void end_vulkan_command_buffer_single_use(
    vulkan_system_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* command_buffer,
    VkQueue queue) {

    // End the command buffer.
    end_vulkan_command_buffer(command_buffer);

    // Submit the queue
    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer->handle;
    validate_vulkan_result(vkQueueSubmit(queue, 1, &submit_info, 0));

    // Wait for it to finish
    validate_vulkan_result(vkQueueWaitIdle(queue));

    // Free the command buffer.
    free_vulkan_command_buffer(context, pool, command_buffer);
 }