#include "system_globals.h"


/* ---------------------------------------------------------------------------
   UI batcher.  The queue (g_draw_queue) is uploaded ONCE per frame into a mapped vertex buffer and drawn as
   instanced quads: one vkCmdDraw per run of flat quads instead of one push-constant + one draw per quad.
   Order is preserved (painter's order): a run ends at a viewport quad (alpha > 1.5, textured, needs another
   descriptor + the old pipeline), that quad is drawn alone, then the next run starts.  Typical frame = 3 draws.
   If the batch pipeline cannot be created (ui_batch_*.spv missing) every quad falls back to the old path.
   --------------------------------------------------------------------------- */

extern int find_memory_index(unsigned int type_filter, unsigned int property_flags);

#define RING 3   /* > MAX_FRAMES_IN_FLIGHT, so the GPU can never still be reading the buffer we overwrite */

static VkPipeline     s_pipeline = VK_NULL_HANDLE;
static VkBuffer       s_buf[RING];
static VkDeviceMemory s_mem[RING];
static void*          s_map[RING];
static uint32_t       s_ring = 0;
static bool           s_tried = false;

void ui_batch_shutdown(void)
{
    VkDevice dev = render_context.logical_device;
    for (int i = 0; i < RING; i++) {
        if (s_map[i]) vkUnmapMemory(dev, s_mem[i]);
        if (s_buf[i]) vkDestroyBuffer(dev, s_buf[i], render_context.allocator);
        if (s_mem[i]) vkFreeMemory(dev, s_mem[i], render_context.allocator);
        s_map[i] = NULL; s_buf[i] = VK_NULL_HANDLE; s_mem[i] = VK_NULL_HANDLE;
    }
    if (s_pipeline) vkDestroyPipeline(dev, s_pipeline, render_context.allocator);
    s_pipeline = VK_NULL_HANDLE;
    s_tried = false;
}

static bool batch_create(uint32_t capacity)
{
    VkDevice dev = render_context.logical_device;

    VkShaderModule vs = load_shader_binary_module(dev, "ui_batch_vert.spv");
    VkShaderModule fs = load_shader_binary_module(dev, "ui_batch_frag.spv");
    if (!vs || !fs) {
        if (vs) vkDestroyShaderModule(dev, vs, render_context.allocator);
        if (fs) vkDestroyShaderModule(dev, fs, render_context.allocator);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {{0}};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vs; stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fs; stages[1].pName = "main";

    /* the queue entry itself is the instance record: position(0) size(8) color(16), stride 40 */
    VkVertexInputBindingDescription bind = { 0, (uint32_t)sizeof(ui_push_constant), VK_VERTEX_INPUT_RATE_INSTANCE };
    VkVertexInputAttributeDescription attr[3] = {
        { 0, 0, VK_FORMAT_R32G32_SFLOAT,       (uint32_t)offsetof(ui_push_constant, position) },
        { 1, 0, VK_FORMAT_R32G32_SFLOAT,       (uint32_t)offsetof(ui_push_constant, size)     },
        { 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)offsetof(ui_push_constant, color)    },
    };
    VkPipelineVertexInputStateCreateInfo vin = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vin.vertexBindingDescriptionCount = 1;   vin.pVertexBindingDescriptions   = &bind;
    vin.vertexAttributeDescriptionCount = 3; vin.pVertexAttributeDescriptions = attr;

    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1; vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE; rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState ba = {0};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    /* straight alpha blending. without it a pixel with alpha < 1 replaces what is underneath instead of mixing with it, so no edge
       (circles, anything translucent) can ever be anti aliased. */
    ba.blendEnable         = VK_TRUE;
    ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.colorBlendOp        = VK_BLEND_OP_ADD;
    ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    ba.alphaBlendOp        = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1; cb.pAttachments = &ba;

    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dys = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dys.dynamicStateCount = 2; dys.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.stageCount = 2; pi.pStages = stages; pi.pVertexInputState = &vin; pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp; pi.pRasterizationState = &rs; pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &ds; pi.pColorBlendState = &cb; pi.pDynamicState = &dys;
    pi.layout     = render_context.pipeline_layout;     /* same layout as the old UI pipeline: shares the 40 byte push range */
    pi.renderPass = render_context.renderpass_main;

    VkResult res = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pi, render_context.allocator, &s_pipeline);
    vkDestroyShaderModule(dev, vs, render_context.allocator);
    vkDestroyShaderModule(dev, fs, render_context.allocator);
    if (res != VK_SUCCESS) { s_pipeline = VK_NULL_HANDLE; return false; }

    for (int i = 0; i < RING; i++) {
        VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size  = (VkDeviceSize)capacity * sizeof(ui_push_constant);
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(dev, &bi, render_context.allocator, &s_buf[i]) != VK_SUCCESS) return false;

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(dev, s_buf[i], &req);
        int type = find_memory_index(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (type < 0) return false;

        VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size; ai.memoryTypeIndex = (uint32_t)type;
        if (vkAllocateMemory(dev, &ai, render_context.allocator, &s_mem[i]) != VK_SUCCESS) return false;
        vkBindBufferMemory(dev, s_buf[i], s_mem[i], 0);
        if (vkMapMemory(dev, s_mem[i], 0, VK_WHOLE_SIZE, 0, &s_map[i]) != VK_SUCCESS) return false;
    }
    return true;
}

/* old path: one quad, push constants + draw. Used for viewport quads (textured) and as the fallback. */
static void draw_one(VkCommandBuffer cmd, ui_push_constant* q, VkExtent2D extent)
{
    const VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkPipelineLayout layout = render_context.pipeline_layout;
    VkDescriptorSet  font   = render_context.scene_viewport_target.descriptor_set;   /* 1x1 UI texture */
    VkDescriptorSet  tex    = font;
    const bool viewport     = q->color[3] > 1.5f;

    if (viewport) {
        const offscreen_kind kind = (q->color[3] > 2.5f) ? offscreen_game : offscreen_scene;
        offscreen_request_size(kind, q->size[0], q->size[1]);   /* tells the NEXT frame how big the target must be */
        tex = offscreen_get_descriptor(kind);
        if (tex == VK_NULL_HANDLE) {
            phoenix_warn("ViewportUI", "%s viewport descriptor is not ready (%.0fx%.0f).",
                         kind == offscreen_scene ? "Scene" : "Game", q->size[0], q->size[1]);
            return;
        }
    }

    q->resolution[0] = (float)extent.width;
    q->resolution[1] = (float)extent.height;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_context.graphics_pipeline);
    if (tex != VK_NULL_HANDLE) vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &tex, 0, NULL);
    vkCmdPushConstants(cmd, layout, stages, 0, (uint32_t)sizeof(ui_push_constant), q);
    vkCmdDraw(cmd, 4, 1, 0, 0);
    if (viewport && font != VK_NULL_HANDLE) vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &font, 0, NULL);
}

static void bind_batch(VkCommandBuffer cmd, VkExtent2D extent)
{
    float res[2] = { (float)extent.width, (float)extent.height };
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipeline);
    vkCmdPushConstants(cmd, render_context.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(res), res);
}

void ui_batch_flush(VkCommandBuffer cmd, VkExtent2D extent, ui_push_constant* q, uint32_t face_type, uint32_t capacity)
{
    if (face_type == 0) return;

    if (!s_tried) {
        s_tried = true;
        if (!batch_create(capacity)) {
            phoenix_error("RENDERER", "%s", "UI batch pipeline unavailable (build ui_batch_vert/frag .spv). Falling back to one draw per quad.");
            ui_batch_shutdown();
            s_tried = true;
        }
    }
    if (render_context.graphics_pipeline == VK_NULL_HANDLE) { phoenix_error("VULKAN", "UI pipeline handle is unassigned!"); return; }

    VkViewport viewport = { 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };
    VkRect2D   scissor  = { {0, 0}, extent };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    const bool batched = (s_pipeline != VK_NULL_HANDLE);
    uint32_t run = 0;

    if (batched) {
        s_ring = (s_ring + 1) % RING;
        memcpy(s_map[s_ring], q, (size_t)face_type * sizeof(ui_push_constant));   /* the whole frame, once */
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &s_buf[s_ring], &offset);
        bind_batch(cmd, extent);
    }

    for (uint32_t i = 0; i < face_type; i++) {
        const bool viewport_quad = q[i].color[3] > 1.5f;
        if (batched && !viewport_quad) continue;                 /* part of the current run */

        if (batched && i > run) vkCmdDraw(cmd, 4, i - run, 0, run);   /* flush the run before this quad */
        draw_one(cmd, &q[i], extent);
        run = i + 1;
        if (batched) bind_batch(cmd, extent);                    /* draw_one changed pipeline + push constants */
    }
    if (batched && face_type > run) vkCmdDraw(cmd, 4, face_type - run, 0, run);
}