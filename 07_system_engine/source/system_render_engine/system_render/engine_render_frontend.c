#include "engine_globals.h"


extern render_backend* backend; 

bool start_render_frontend(const char* application_name) 
{
    backend = kallocate(sizeof(render_backend), MEMORY_TAG_RENDERER);

    if (!create_render_backend(render_backend_type_vulkan, backend)) {
        phoenix_fatal("RENDERER", "Factory mapping failed to extract Vulkan plugin descriptors.");
        return FALSE;
    }
    backend->frame_number = 0;

    // === CRITICAL DIAGNOSTIC FORCE-BIND ===
    // If the factory failed to write the function address, catch it before it crashes:
    if (!backend->start_render) {
        phoenix_warn("RENDERER", "Factory failed to map start_render! Forcing direct link to Vulkan.");
        backend->start_render = start_vulkan_render_backend;
    }

    // This will now execute safely without jumping to 0x0
    if (!backend->start_render(backend, application_name)) {
        phoenix_fatal("RENDERER", "Renderer backend failed to initialize. Shutting down.");
        return FALSE;
    }

    return TRUE;
}

void shutdown_render_frontend() 
{
    if (backend) {
        backend->shutdown_render(backend);
        kfree(backend, sizeof(render_backend), MEMORY_TAG_RENDERER);
        backend = 0;
    }
}

bool begin_frontend_render_frame(float delta_time) 
{
    return backend->begin_frame(backend, delta_time);
}

bool end_frontend_render_frame(float delta_time) 
{
    bool result = backend->end_frame(backend, delta_time);
    backend->frame_number++;
    return result;
}


/* Frontend thread, step 1: offscreen pass + open the main pass + bind. Returns the command buffer the UI must draw into,
   or VK_NULL_HANDLE if there is nothing to record this frame. */
VkCommandBuffer begin_frontend_main_pass(void)
{
    uint32_t image_idx = render_context.swapchain_main.current_frame;
    if (image_idx >= render_context.swapchain_main.image_count) return VK_NULL_HANDLE;

    VkCommandBuffer cmd = render_context.swapchain_main.command_buffers[g_current_inflight_cpu_frame];
    if (cmd == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    g_editor.record_offscreen_frame(cmd);

    VkRenderPassBeginInfo pass_begin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass_begin.renderPass = render_context.renderpass_main;
    pass_begin.framebuffer = render_context.swapchain_main.framebuffers[image_idx];
    pass_begin.renderArea.offset = (VkOffset2D){0, 0};
    pass_begin.renderArea.extent = render_context.swapchain_main.extent;

    VkClearValue clear_values[2];
    clear_values[0].color = (VkClearColorValue){{0.0f, 0.0f, 0.2f, 1.0f}};
    clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
    pass_begin.clearValueCount = 2;
    pass_begin.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd, &pass_begin, VK_SUBPASS_CONTENTS_INLINE);

    if (render_context.graphics_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_context.graphics_pipeline);
        if (render_context.scene_viewport_target.descriptor_set != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_context.pipeline_layout,
                                    0, 1, &render_context.scene_viewport_target.descriptor_set, 0, NULL);
        }
    }
    return cmd;
}

/* Frontend thread, step 2: close the main pass after the UI has been drawn into it. */
void end_frontend_main_pass(VkCommandBuffer cmd)
{
    if (cmd != VK_NULL_HANDLE) vkCmdEndRenderPass(cmd);
}

bool draw_frontend_render_frame(render_packet* packet) 
{
    // Fix: Guard function pointer addresses before execution
    if (!backend || !backend->begin_frame || !backend->end_frame) {
        phoenix_error("RENDERER", "%s", "Backend function pointers are unassigned or null!");
        return FALSE;
    }

    // 1. Let the backend cleanly call vkAcquireNextImageKHR, wait for fences, and open the command buffer
    if (!backend->begin_frame(backend, packet->delta_time)) {
        return FALSE; 
    }

    // 2. Fetch the validated hardware backbuffer texture index that was explicitly written by begin_frame
    uint32_t image_idx = render_context.swapchain_main.current_frame; 
    if (image_idx < render_context.swapchain_main.image_count) {
        VkCommandBuffer cmd = render_context.swapchain_main.command_buffers[g_current_inflight_cpu_frame];
        if (cmd != VK_NULL_HANDLE) {
            /* Scene and Game targets are recorded outside their main UI pass. */
            g_editor.record_offscreen_frame(cmd);

            // 3. Build our explicit clear pass parameters
            VkRenderPassBeginInfo pass_begin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            pass_begin.renderPass = render_context.renderpass_main;
            pass_begin.framebuffer = render_context.swapchain_main.framebuffers[image_idx];
            pass_begin.renderArea.offset = (VkOffset2D){0, 0};
            pass_begin.renderArea.extent = render_context.swapchain_main.extent;
            
            VkClearValue clear_values[2];
            clear_values[0].color = (VkClearColorValue){{0.0f, 0.0f, 0.2f, 1.0f}}; // Clear Color (Dark Blue Canvas)
            clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};
            pass_begin.clearValueCount = 2;
            pass_begin.pClearValues = clear_values;
            
            // 4. Formally enter the rendering pass state to transition the textures out of UNDEFINED
            vkCmdBeginRenderPass(cmd, &pass_begin, VK_SUBPASS_CONTENTS_INLINE);
            
            if (render_context.graphics_pipeline != VK_NULL_HANDLE) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_context.graphics_pipeline);
                
                // =========================================================================
                // 🛠️ FIX: BIND INTERFACE SAMPLER DESCRIPTOR SET FOR SET 0, BINDING 0
                // =========================================================================
                // This ensures the pipeline layout criteria matching object_frag.spv is met
                if (render_context.scene_viewport_target.descriptor_set != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(
                        cmd, 
                        VK_PIPELINE_BIND_POINT_GRAPHICS, 
                        render_context.pipeline_layout, 
                        0, 1, 
                        &render_context.scene_viewport_target.descriptor_set, 
                        0, NULL
                    );
                }
            } else {
                phoenix_warn("RENDERER", "Graphics pipeline handle unassigned during layout collection pass!");
            }
            
            // 5. THE REAL DRAWER: Execute broken-up layout blocks to append commands directly to the pipeline
            start_system_scenepass(cmd);
            shutdown_system_scenepass();
            
            vkCmdEndRenderPass(cmd);
        }
    }

    // 7. Hand the recorded command buffers back to the backend to close, submit, and present
    if (!backend->end_frame(backend, packet->delta_time)) {
        phoenix_error("RENDERER", "%s", "end_frontend_render_frame failed. Pipeline context desynchronized.");
        return FALSE;
    }

    backend->frame_number++;
    return TRUE;
}