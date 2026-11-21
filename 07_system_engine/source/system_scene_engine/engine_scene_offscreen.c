#include "engine_globals.h"


extern int find_memory_index(unsigned int type_filter, unsigned int property_flags);

/* grid_vert.glsl: half_extent_m = 2000, cell_size_m = 4 -> lines_per_axis = (2000*2)/4 + 1 = 1001, verts_per_axis = 2002, two axes = 4004.
   If this is larger than the shader's real count, the extra vertices are drawn as more Z lines past x = +2000 and the grid stops being square.
   Change half_extent_m / cell_size_m in grid_vert.glsl and this number together (same number lives in system_grid.c). */
#define OFFSCREEN_MAX_DIM 8192u

typedef struct {
    uint32_t width, height;

    VkImage        msaa_image;    VkDeviceMemory msaa_memory;    VkImageView msaa_view;     /* only when samples > 1 */
    VkImage        depth_image;   VkDeviceMemory depth_memory;   VkImageView depth_view;
    VkImage        resolve_image; VkDeviceMemory resolve_memory; VkImageView resolve_view;  /* sampled by the UI pass */

    VkFramebuffer   framebuffer;
    VkDescriptorSet descriptor_set;   /* samples the resolved scene image (also the FXAA input) */

    /* FXAA output: the scene image after the post pass. The UI samples this one when FXAA ran this frame. */
    VkImage         fxaa_image;   VkDeviceMemory fxaa_memory;   VkImageView fxaa_view;
    VkFramebuffer   fxaa_framebuffer;
    VkDescriptorSet fxaa_set;
    bool            fxaa_applied;   /* FXAA was recorded for this target this frame */

    bool     requested;      /* a viewport quad of this kind was flushed last frame */
    uint32_t req_w, req_h;
    bool     drawn;          /* rendered this frame: image is in SHADER_READ_ONLY and may be sampled */
} offscreen_target;

static offscreen_target g_target[offscreen_count];
static VkRenderPass     g_pass            = VK_NULL_HANDLE;
static VkSampler        g_sampler         = VK_NULL_HANDLE;
static VkDescriptorPool g_pool            = VK_NULL_HANDLE;
static VkFormat         g_color_format    = VK_FORMAT_UNDEFINED;
static VkFormat         g_depth_format    = VK_FORMAT_UNDEFINED;
static VkSampleCountFlagBits g_samples    = VK_SAMPLE_COUNT_1_BIT;

/* FXAA post pass: 1 sample color -> color, fullscreen triangle. Switchable at runtime (Preferences > FXAA). */
static VkRenderPass       g_fxaa_pass     = VK_NULL_HANDLE;
static VkPipelineLayout   g_fxaa_layout   = VK_NULL_HANDLE;
static VkPipeline         g_fxaa_pipeline = VK_NULL_HANDLE;

static bool format_has_stencil(VkFormat f)
{
    return f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D24_UNORM_S8_UINT || f == VK_FORMAT_D16_UNORM_S8_UINT;
}

/* ------------------------------------------------------------------ */
/* Render pass                                                         */
/* ------------------------------------------------------------------ */
bool create_offscreen_renderpass(VkDevice device, VkFormat color_format, VkFormat depth_format)
{
    (void)device;
    if (g_pass != VK_NULL_HANDLE) return true;
    if (depth_format == VK_FORMAT_UNDEFINED) {
        phoenix_error("OFFSCREEN", "%s", "No depth format: cannot build the offscreen pass.");
        return false;
    }

    g_color_format = color_format;
    g_depth_format = depth_format;
    g_samples      = get_render_msaa_sample_count();
    const bool msaa = (g_samples != VK_SAMPLE_COUNT_1_BIT);

    /* Attachment order: 0 = color (MSAA, or the resolved image itself when MSAA is off), 1 = depth, 2 = resolve (MSAA only) */
    VkAttachmentDescription att[3];
    memset(att, 0, sizeof(att));

    att[0].format         = color_format;
    att[0].samples        = g_samples;
    att[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att[0].storeOp        = msaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
    att[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att[0].finalLayout    = msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    att[1].format         = depth_format;
    att[1].samples        = g_samples;
    att[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    att[2].format         = color_format;
    att[2].samples        = VK_SAMPLE_COUNT_1_BIT;
    att[2].loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[2].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att[2].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[2].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att[2].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference color_ref   = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depth_ref   = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkAttachmentReference resolve_ref = { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription sub = {0};
    sub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount    = 1;
    sub.pColorAttachments       = &color_ref;
    sub.pDepthStencilAttachment = &depth_ref;
    sub.pResolveAttachments     = msaa ? &resolve_ref : NULL;

    /* in:  the previous frame's UI pass may still be sampling the resolved image / the previous pass used the depth image
       out: the UI pass samples the resolved image in the fragment shader */
    VkSubpassDependency dep[2];
    memset(dep, 0, sizeof(dep));
    dep[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep[0].dstSubpass    = 0;
    dep[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    dep[1].srcSubpass    = 0;
    dep[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    dep[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    info.attachmentCount = msaa ? 3u : 2u;
    info.pAttachments    = att;
    info.subpassCount    = 1;
    info.pSubpasses      = &sub;
    info.dependencyCount = 2;
    info.pDependencies   = dep;

    if (vkCreateRenderPass(render_context.logical_device, &info, render_context.allocator, &g_pass) != VK_SUCCESS) {
        phoenix_error("OFFSCREEN", "%s", "vkCreateRenderPass failed for the offscreen pass.");
        g_pass = VK_NULL_HANDLE;
        return false;
    }

    render_context.offscreen_renderpass = g_pass;
    phoenix_info("OFFSCREEN", "Offscreen pass created (%ux MSAA, depth format %d).", (unsigned)g_samples, (int)depth_format);
    return true;
}

/* ------------------------------------------------------------------ */
/* Images                                                              */
/* ------------------------------------------------------------------ */
static bool make_image(uint32_t w, uint32_t h, VkFormat fmt, VkSampleCountFlagBits samples,
                       VkImageUsageFlags usage, VkImageAspectFlags aspect,
                       VkImage* out_img, VkDeviceMemory* out_mem, VkImageView* out_view)
{
    VkDevice dev = render_context.logical_device;

    VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ii.imageType     = VK_IMAGE_TYPE_2D;
    ii.format        = fmt;
    ii.extent.width  = w;
    ii.extent.height = h;
    ii.extent.depth  = 1;
    ii.mipLevels     = 1;
    ii.arrayLayers   = 1;
    ii.samples       = samples;
    ii.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ii.usage         = usage;
    ii.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(dev, &ii, render_context.allocator, out_img) != VK_SUCCESS) { *out_img = VK_NULL_HANDLE; return false; }

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(dev, *out_img, &req);
    int mem = find_memory_index(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem < 0) return false;

    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = (uint32_t)mem;
    if (vkAllocateMemory(dev, &ai, render_context.allocator, out_mem) != VK_SUCCESS) { *out_mem = VK_NULL_HANDLE; return false; }
    if (vkBindImageMemory(dev, *out_img, *out_mem, 0) != VK_SUCCESS) return false;

    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vi.image    = *out_img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = fmt;
    vi.subresourceRange.aspectMask = aspect;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if (vkCreateImageView(dev, &vi, render_context.allocator, out_view) != VK_SUCCESS) { *out_view = VK_NULL_HANDLE; return false; }
    return true;
}

static void destroy_target_images(offscreen_target* t)
{
    VkDevice dev = render_context.logical_device;
    const VkAllocationCallbacks* a = render_context.allocator;

    if (t->fxaa_framebuffer) vkDestroyFramebuffer(dev, t->fxaa_framebuffer, a);
    if (t->fxaa_view)        vkDestroyImageView(dev, t->fxaa_view, a);
    if (t->fxaa_image)       vkDestroyImage(dev, t->fxaa_image, a);
    if (t->fxaa_memory)      vkFreeMemory(dev, t->fxaa_memory, a);
    t->fxaa_framebuffer = VK_NULL_HANDLE;
    t->fxaa_view = VK_NULL_HANDLE; t->fxaa_image = VK_NULL_HANDLE; t->fxaa_memory = VK_NULL_HANDLE;
    t->fxaa_applied = false;

    if (t->framebuffer)    vkDestroyFramebuffer(dev, t->framebuffer, a);
    if (t->msaa_view)      vkDestroyImageView(dev, t->msaa_view, a);
    if (t->depth_view)     vkDestroyImageView(dev, t->depth_view, a);
    if (t->resolve_view)   vkDestroyImageView(dev, t->resolve_view, a);
    if (t->msaa_image)     vkDestroyImage(dev, t->msaa_image, a);
    if (t->depth_image)    vkDestroyImage(dev, t->depth_image, a);
    if (t->resolve_image)  vkDestroyImage(dev, t->resolve_image, a);
    if (t->msaa_memory)    vkFreeMemory(dev, t->msaa_memory, a);
    if (t->depth_memory)   vkFreeMemory(dev, t->depth_memory, a);
    if (t->resolve_memory) vkFreeMemory(dev, t->resolve_memory, a);

    t->framebuffer = VK_NULL_HANDLE;
    t->msaa_view = t->depth_view = t->resolve_view = VK_NULL_HANDLE;
    t->msaa_image = t->depth_image = t->resolve_image = VK_NULL_HANDLE;
    t->msaa_memory = t->depth_memory = t->resolve_memory = VK_NULL_HANDLE;
    t->width = t->height = 0;
}

static bool create_target_images(offscreen_target* t, uint32_t w, uint32_t h)
{
    const bool msaa = (g_samples != VK_SAMPLE_COUNT_1_BIT);
    VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT | (format_has_stencil(g_depth_format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    bool ok = make_image(w, h, g_color_format, VK_SAMPLE_COUNT_1_BIT,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                         VK_IMAGE_ASPECT_COLOR_BIT, &t->resolve_image, &t->resolve_memory, &t->resolve_view);
    ok = ok && make_image(w, h, g_depth_format, g_samples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                          depth_aspect, &t->depth_image, &t->depth_memory, &t->depth_view);
    if (ok && msaa) {
        ok = make_image(w, h, g_color_format, g_samples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT, &t->msaa_image, &t->msaa_memory, &t->msaa_view);
    }

    if (ok) {
        VkImageView views[3];
        uint32_t n = 0;
        if (msaa) { views[n++] = t->msaa_view; views[n++] = t->depth_view; views[n++] = t->resolve_view; }
        else      { views[n++] = t->resolve_view; views[n++] = t->depth_view; }

        VkFramebufferCreateInfo fi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fi.renderPass      = g_pass;
        fi.attachmentCount = n;
        fi.pAttachments    = views;
        fi.width  = w;
        fi.height = h;
        fi.layers = 1;
        ok = (vkCreateFramebuffer(render_context.logical_device, &fi, render_context.allocator, &t->framebuffer) == VK_SUCCESS);
        if (!ok) t->framebuffer = VK_NULL_HANDLE;
    }

    /* FXAA output image + framebuffer (only when the FXAA pass exists) */
    if (ok && g_fxaa_pass != VK_NULL_HANDLE) {
        ok = make_image(w, h, g_color_format, VK_SAMPLE_COUNT_1_BIT,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT, &t->fxaa_image, &t->fxaa_memory, &t->fxaa_view);
        if (ok) {
            VkFramebufferCreateInfo ffi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            ffi.renderPass      = g_fxaa_pass;
            ffi.attachmentCount = 1;
            ffi.pAttachments    = &t->fxaa_view;
            ffi.width  = w;
            ffi.height = h;
            ffi.layers = 1;
            ok = (vkCreateFramebuffer(render_context.logical_device, &ffi, render_context.allocator, &t->fxaa_framebuffer) == VK_SUCCESS);
            if (!ok) t->fxaa_framebuffer = VK_NULL_HANDLE;
        }
    }

    if (!ok) {
        phoenix_error("OFFSCREEN", "Failed to create offscreen target %ux%u.", w, h);
        destroy_target_images(t);
        return false;
    }

    t->width = w;
    t->height = h;

    VkDescriptorImageInfo di = { g_sampler, t->resolve_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet w_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    w_set.dstSet          = t->descriptor_set;
    w_set.dstBinding      = 0;
    w_set.descriptorCount = 1;
    w_set.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w_set.pImageInfo      = &di;
    vkUpdateDescriptorSets(render_context.logical_device, 1, &w_set, 0, NULL);

    if (t->fxaa_view != VK_NULL_HANDLE && t->fxaa_set != VK_NULL_HANDLE) {
        VkDescriptorImageInfo fdi = { g_sampler, t->fxaa_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet f_set = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        f_set.dstSet          = t->fxaa_set;
        f_set.dstBinding      = 0;
        f_set.descriptorCount = 1;
        f_set.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        f_set.pImageInfo      = &fdi;
        vkUpdateDescriptorSets(render_context.logical_device, 1, &f_set, 0, NULL);
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* FXAA pass + pipeline                                                */
/* Shaders: fxaa_vert.spv / fxaa_frag.spv (next to the exe). If they are missing FXAA is simply unavailable. */
/* ------------------------------------------------------------------ */
static void destroy_fxaa_objects(VkDevice dev)
{
    if (g_fxaa_pipeline) { vkDestroyPipeline(dev, g_fxaa_pipeline, render_context.allocator);       g_fxaa_pipeline = VK_NULL_HANDLE; }
    if (g_fxaa_layout)   { vkDestroyPipelineLayout(dev, g_fxaa_layout, render_context.allocator);   g_fxaa_layout   = VK_NULL_HANDLE; }
    if (g_fxaa_pass)     { vkDestroyRenderPass(dev, g_fxaa_pass, render_context.allocator);         g_fxaa_pass     = VK_NULL_HANDLE; }
}

static bool create_fxaa_objects(void)
{
    VkDevice dev = render_context.logical_device;

    VkShaderModule vs = load_shader_binary_module(dev, "fxaa_vert.spv");
    VkShaderModule fs = load_shader_binary_module(dev, "fxaa_frag.spv");
    if (!vs || !fs) {
        phoenix_warn("OFFSCREEN", "%s", "fxaa_vert.spv / fxaa_frag.spv not found (compile shaders/fxaa_*.glsl). FXAA disabled.");
        if (vs) vkDestroyShaderModule(dev, vs, NULL);
        if (fs) vkDestroyShaderModule(dev, fs, NULL);
        return false;
    }

    /* pass: one color attachment, written fully by the triangle (DONT_CARE load), read by the UI pass afterwards */
    VkAttachmentDescription att;
    memset(&att, 0, sizeof(att));
    att.format         = g_color_format;
    att.samples        = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference cref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub = {0};
    sub.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments    = &cref;

    VkSubpassDependency dep[2];
    memset(dep, 0, sizeof(dep));
    dep[0].srcSubpass    = VK_SUBPASS_EXTERNAL;   /* previous frame's UI pass may still be sampling this image */
    dep[0].dstSubpass    = 0;
    dep[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep[1].srcSubpass    = 0;                     /* the UI pass samples the result */
    dep[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    dep[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rpi = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpi.attachmentCount = 1;  rpi.pAttachments = &att;
    rpi.subpassCount    = 1;  rpi.pSubpasses   = &sub;
    rpi.dependencyCount = 2;  rpi.pDependencies = dep;
    if (vkCreateRenderPass(dev, &rpi, render_context.allocator, &g_fxaa_pass) != VK_SUCCESS) {
        phoenix_error("OFFSCREEN", "%s", "vkCreateRenderPass failed for the FXAA pass.");
        g_fxaa_pass = VK_NULL_HANDLE;
        vkDestroyShaderModule(dev, vs, NULL);
        vkDestroyShaderModule(dev, fs, NULL);
        return false;
    }

    /* layout: set 0 = the same combined-image-sampler layout the UI uses, plus vec2 inverse resolution */
    VkPushConstantRange pcr = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) * 2 };
    VkPipelineLayoutCreateInfo li = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    li.setLayoutCount         = 1;
    li.pSetLayouts            = &render_context.descriptor_layout;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges    = &pcr;
    if (vkCreatePipelineLayout(dev, &li, render_context.allocator, &g_fxaa_layout) != VK_SUCCESS) {
        g_fxaa_layout = VK_NULL_HANDLE;
        vkDestroyShaderModule(dev, vs, NULL);
        vkDestroyShaderModule(dev, fs, NULL);
        destroy_fxaa_objects(dev);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,   vs, "main", NULL },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL }
    };
    VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };   /* none */
    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1; vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth   = 1.0f;
    rs.cullMode    = VK_CULL_MODE_NONE;
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState cba = {0};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1; cb.pAttachments = &cba;
    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dsi = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dsi.dynamicStateCount = 2; dsi.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pi.stageCount = 2;              pi.pStages = stages;
    pi.pVertexInputState = &vi;     pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;        pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;     pi.pDepthStencilState = NULL;
    pi.pColorBlendState = &cb;      pi.pDynamicState = &dsi;
    pi.layout = g_fxaa_layout;      pi.renderPass = g_fxaa_pass;  pi.subpass = 0;

    VkResult res = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pi, render_context.allocator, &g_fxaa_pipeline);
    vkDestroyShaderModule(dev, vs, NULL);
    vkDestroyShaderModule(dev, fs, NULL);
    if (res != VK_SUCCESS) {
        phoenix_error("OFFSCREEN", "vkCreateGraphicsPipelines failed for FXAA (%d). FXAA disabled.", (int)res);
        g_fxaa_pipeline = VK_NULL_HANDLE;
        destroy_fxaa_objects(dev);
        return false;
    }

    phoenix_info("OFFSCREEN", "%s", "FXAA pass ready.");
    return true;
}

/* ------------------------------------------------------------------ */
/* Resources                                                           */
/* ------------------------------------------------------------------ */
bool create_offscreen_resources(void)
{
    VkDevice dev = render_context.logical_device;
    if (g_pass == VK_NULL_HANDLE || render_context.descriptor_layout == VK_NULL_HANDLE) {
        phoenix_error("OFFSCREEN", "%s", "offscreen_create_resources called before the pass / UI descriptor layout exist.");
        return false;
    }

    VkSamplerCreateInfo si = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    si.magFilter    = VK_FILTER_LINEAR;
    si.minFilter    = VK_FILTER_LINEAR;
    si.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod       = 1.0f;
    if (vkCreateSampler(dev, &si, render_context.allocator, &g_sampler) != VK_SUCCESS) {
        phoenix_error("OFFSCREEN", "%s", "Failed to create the offscreen sampler.");
        return false;
    }
    render_context.offscreen_sampler = g_sampler;

    /* the FXAA pass is optional: if its shaders are missing the editor just runs without it */
    create_fxaa_objects();

    VkDescriptorPoolSize ps = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offscreen_count * 2 };   /* scene set + FXAA set per target */
    VkDescriptorPoolCreateInfo pi = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pi.poolSizeCount = 1;
    pi.pPoolSizes    = &ps;
    pi.maxSets       = offscreen_count * 2;
    if (vkCreateDescriptorPool(dev, &pi, render_context.allocator, &g_pool) != VK_SUCCESS) {
        phoenix_error("OFFSCREEN", "%s", "Failed to create the offscreen descriptor pool.");
        return false;
    }

    for (int k = 0; k < offscreen_count; k++) {
        VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        ai.descriptorPool     = g_pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &render_context.descriptor_layout;
        if (vkAllocateDescriptorSets(dev, &ai, &g_target[k].descriptor_set) != VK_SUCCESS) {
            phoenix_error("OFFSCREEN", "%s", "Failed to allocate an offscreen descriptor set.");
            return false;
        }
        if (vkAllocateDescriptorSets(dev, &ai, &g_target[k].fxaa_set) != VK_SUCCESS) {
            phoenix_error("OFFSCREEN", "%s", "Failed to allocate an offscreen FXAA descriptor set.");
            return false;
        }
    }
    return true;
}

void shutdown_offscreen(void)
{
    VkDevice dev = render_context.logical_device;
    if (dev == VK_NULL_HANDLE) return;

    for (int k = 0; k < offscreen_count; k++) {
        destroy_target_images(&g_target[k]);
        g_target[k].descriptor_set = VK_NULL_HANDLE;   /* freed with the pool */
        g_target[k].fxaa_set = VK_NULL_HANDLE;
        g_target[k].requested = g_target[k].drawn = false;
    }
    destroy_fxaa_objects(dev);
    if (g_pool)    { vkDestroyDescriptorPool(dev, g_pool, render_context.allocator); g_pool = VK_NULL_HANDLE; }
    if (g_sampler) { vkDestroySampler(dev, g_sampler, render_context.allocator);     g_sampler = VK_NULL_HANDLE; }
    if (g_pass)    { vkDestroyRenderPass(dev, g_pass, render_context.allocator);     g_pass = VK_NULL_HANDLE; }
    render_context.offscreen_renderpass = VK_NULL_HANDLE;
    render_context.offscreen_sampler    = VK_NULL_HANDLE;
}

/* ------------------------------------------------------------------ */
/* Per-frame                                                           */
/* ------------------------------------------------------------------ */
void request_offscreen_size(offscreen_kind kind, float width, float height)
{
    if (kind < 0 || kind >= offscreen_count) return;
    if (width < 1.0f || height < 1.0f) return;

    offscreen_target* t = &g_target[kind];
    uint32_t w = (uint32_t)width, h = (uint32_t)height;
    if (w > OFFSCREEN_MAX_DIM) w = OFFSCREEN_MAX_DIM;
    if (h > OFFSCREEN_MAX_DIM) h = OFFSCREEN_MAX_DIM;

    /* Several viewports of one kind (e.g. multiple game views): the first one of the frame sets the size. */
    if (!t->requested) { t->req_w = w; t->req_h = h; t->requested = true; }
}

VkDescriptorSet get_offscreen_descriptor(offscreen_kind kind)
{
    if (kind < 0 || kind >= offscreen_count) return VK_NULL_HANDLE;
    if (!g_target[kind].drawn) return VK_NULL_HANDLE;
    return g_target[kind].fxaa_applied ? g_target[kind].fxaa_set : g_target[kind].descriptor_set;
}

static void draw_scene_view(VkCommandBuffer cmd, uint32_t w, uint32_t h)
{
    editor_mat4 view, proj;
    build_scene_view_camera((float)w / (float)h, &view, &proj);   /* SYSTEM camera: the editor fly camera */

    if (graphics_pipeline_grid != VK_NULL_HANDLE && pipeline_layout_grid != VK_NULL_HANDLE) {
        float pc[32];
        memcpy(&pc[0],  &view.mesh[0][0], sizeof(float) * 16);
        memcpy(&pc[16], &proj.mesh[0][0], sizeof(float) * 16);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_grid);
        vkCmdPushConstants(cmd, pipeline_layout_grid, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), pc);
        if (g_scene_view.show_grid) {   /* Grid button (includes the red / blue axis lines) */
            uint32_t grid_half = (uint32_t)g_scene_view.grid_half_cells;
            if (grid_half < 1u) {
                grid_half = 50u;
            }
            vkCmdDraw(cmd, (grid_half * 2u + 1u) * 4u, 1, 0, grid_half);   /* firstInstance carries the grid size to grid_vert.glsl */
        }
    }

    /* Meshes, camera glyphs, origin marker. Binds its own pipeline, so it comes after the grid. */
    draw_scene_objects(cmd, &view.mesh[0][0], &proj.mesh[0][0]);
}

static bool draw_game_view(VkCommandBuffer cmd, uint32_t w, uint32_t h)
{
    editor_mat4 view, proj;
    if (!build_game_view_camera((float)w / (float)h, &view, &proj)) return false;   /* no camera in the hierarchy */
    draw_game_scene_objects(cmd, &view.mesh[0][0], &proj.mesh[0][0]);
    return true;
}

/* Post pass: reads the resolved scene image (descriptor_set), writes the FXAA image. */
static void record_fxaa_pass(VkCommandBuffer cmd, offscreen_target* t)
{
    VkRenderPassBeginInfo bi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    bi.renderPass        = g_fxaa_pass;
    bi.framebuffer       = t->fxaa_framebuffer;
    bi.renderArea.offset = (VkOffset2D){ 0, 0 };
    bi.renderArea.extent = (VkExtent2D){ t->width, t->height };
    bi.clearValueCount   = 0;
    vkCmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp = { 0.0f, 0.0f, (float)t->width, (float)t->height, 0.0f, 1.0f };
    VkRect2D   sc = { { 0, 0 }, { t->width, t->height } };
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);

    float inv_res[2] = { 1.0f / (float)t->width, 1.0f / (float)t->height };
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_fxaa_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_fxaa_layout, 0, 1, &t->descriptor_set, 0, NULL);
    vkCmdPushConstants(cmd, g_fxaa_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(inv_res), inv_res);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
}

void record_offscreen_frame(VkCommandBuffer cmd)
{
    if (g_pass == VK_NULL_HANDLE) return;

    for (int k = 0; k < offscreen_count; k++) {
        offscreen_target* t = &g_target[k];
        t->drawn = false;
        t->fxaa_applied = false;

        if (!t->requested) continue;
        t->requested = false;

        if (t->width != t->req_w || t->height != t->req_h) {
            /* Resize: nothing recorded this frame references the old images yet, the GPU may still use them. */
            vkDeviceWaitIdle(render_context.logical_device);
            destroy_target_images(t);
            if (!create_target_images(t, t->req_w, t->req_h)) continue;
        }
        if (t->framebuffer == VK_NULL_HANDLE) continue;

        /* The Game view with no camera in the hierarchy draws nothing: skip the pass and the composite quad. */
        if (k == offscreen_game) {
            editor_mat4 vert_to_edge_array, p;
            if (!build_game_view_camera((float)t->width / (float)t->height, &vert_to_edge_array, &p)) continue;
        }

        VkClearValue clear[3];
        memset(clear, 0, sizeof(clear));
        /* Scene view: fallback colour under the gradient, follows the Preferences background even if the gradient pipeline is missing.
           Game view: the editor background is NOT used there (fixed dark colour until cameras get their own background setting). */
        if (k == offscreen_scene) clear[0].color = (VkClearColorValue){{ g_scene_bg.top[0], g_scene_bg.top[1], g_scene_bg.top[2], 1.0f }};
        else                      clear[0].color = (VkClearColorValue){{ 0.05f, 0.05f, 0.07f, 1.0f }};
        clear[1].depthStencil = (VkClearDepthStencilValue){ 1.0f, 0 };

        VkRenderPassBeginInfo bi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        bi.renderPass          = g_pass;
        bi.framebuffer         = t->framebuffer;
        bi.renderArea.offset   = (VkOffset2D){ 0, 0 };
        bi.renderArea.extent   = (VkExtent2D){ t->width, t->height };
        bi.clearValueCount     = (g_samples != VK_SAMPLE_COUNT_1_BIT) ? 3u : 2u;
        bi.pClearValues        = clear;
        vkCmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp = { 0.0f, 0.0f, (float)t->width, (float)t->height, 0.0f, 1.0f };
        VkRect2D   sc = { { 0, 0 }, { t->width, t->height } };
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);

        if (k == offscreen_scene) {
            scene_look_draw_background(cmd);   /* editor gradient from Preferences: Scene view only, after viewport + scissor, before the grid */
            draw_scene_view(cmd, t->width, t->height);
        } else {
            draw_game_view(cmd, t->width, t->height);
        }

        vkCmdEndRenderPass(cmd);

        /* FXAA runs on the resolved image; the UI then samples its output instead (see offscreen_get_descriptor) */
        if (g_render_settings.fxaa && g_fxaa_pipeline != VK_NULL_HANDLE && t->fxaa_framebuffer != VK_NULL_HANDLE) {
            record_fxaa_pass(cmd, t);
            t->fxaa_applied = true;
        }
        t->drawn = true;

        /* one-time diagnostic: shows which image the UI will sample for this view */
        {
            static bool s_logged[offscreen_count];
            if (!s_logged[k]) {
                s_logged[k] = true;
                phoenix_info("OFFSCREEN", "%s view first drawn: %ux%u, %ux MSAA, FXAA applied=%d (prefs fxaa=%d, pipeline=%d)",
                             (k == offscreen_scene) ? "Scene" : "Game", t->width, t->height, (unsigned)g_samples,
                             (int)t->fxaa_applied, (int)g_render_settings.fxaa, (int)(g_fxaa_pipeline != VK_NULL_HANDLE));
            }
        }
    }
}
