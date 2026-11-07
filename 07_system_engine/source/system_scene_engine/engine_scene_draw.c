#include "engine_globals.h"


/* Flip to VK_FRONT_FACE_CLOCKWISE if the cube looks inside-out
   (depends on how system_math_perspective_vk / your viewport handle Y). */
#define SCENE_OBJECT_FRONT_FACE VK_FRONT_FACE_COUNTER_CLOCKWISE

static VkPipeline       g_obj_pipeline = VK_NULL_HANDLE;
static VkPipelineLayout g_obj_layout   = VK_NULL_HANDLE;
static VkPipeline       g_line_pipeline = VK_NULL_HANDLE;   /* same shaders/layout/vertex format, LINE_LIST */
static VkPipeline       g_wire_pipeline = VK_NULL_HANDLE;   /* triangles drawn as lines (Wireframe, Wireframe on Shaded). needs fillModeNonSolid */
static VkPipeline       g_wiregrid_pipeline = VK_NULL_HANDLE;   /* FILLED triangles + mesh_frag.glsl cell-grid discard: terrain wire (cell borders only, no diagonals) */
static VkPipeline       g_border_line_pipeline = VK_NULL_HANDLE;   /* region / chunk outlines: LINE_LIST, depth bias toward the camera, no depth write */
static VkPipeline       g_border_tri_pipeline  = VK_NULL_HANDLE;   /* region ribbons: same state, TRIANGLE_LIST */
static VkPipeline       g_xray_pipeline = VK_NULL_HANDLE;   /* alpha blended through blend constants, no depth write (X-Ray) */
extern bool g_device_fill_mode_non_solid;                   /* system_vulkan_device.c */
/* what the pipelines were built against. If the offscreen pass or MSAA face_type changes they are rebuilt. */
static VkRenderPass          g_obj_renderpass = VK_NULL_HANDLE;
static VkSampleCountFlagBits g_obj_samples    = (VkSampleCountFlagBits)0;

/* editor_vertex is 48 bytes: obj_vert_pos(12) colour(16) uv(8) normal(12). The pipeline below, the shaders
   (mesh_vert.glsl) and every uploader share this layout. If this line fails to compile, editor_vertex changed. */
typedef char editor_vertex_size_check[(sizeof(editor_vertex) == 48) ? 1 : -1];

/* modelview (64) + proj (64) + light (16: view-space sun direction xyz, ambient w) + sun colour (16: rgb = tint * brightness).
   light.xyz == 0 means "draw the plain vertex colour". */
#define MESH_PC_SIZE 160

static void terrain_borders_shutdown(VkDevice device);

static void push_mesh_pc(VkCommandBuffer cmd, const editor_mat4* mv, const editor_mat4* proj, const float light[4])
{
    float pc[40];
    float sun[4];
    light_get_sun_color(light[3], sun);
    memcpy(&pc[0],  mv,    sizeof(editor_mat4));
    memcpy(&pc[16], proj,  sizeof(editor_mat4));
    memcpy(&pc[32], light, 4 * sizeof(float));
    memcpy(&pc[36], sun,   4 * sizeof(float));
    vkCmdPushConstants(cmd, g_obj_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), pc);
}

static const float k_unlit[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

/* flat colour (mesh_frag.glsl: light.xyz = 0 and light.w < 0 -> output sun.rgb). used for wire lines. */
static void push_mesh_pc_flat(VkCommandBuffer cmd, const editor_mat4* mv, const editor_mat4* proj, float r, float g, float b)
{
    float pc[40];
    memcpy(&pc[0],  mv,   sizeof(editor_mat4));
    memcpy(&pc[16], proj, sizeof(editor_mat4));
    pc[32] = 0.0f; pc[33] = 0.0f; pc[34] = 0.0f; pc[35] = -1.0f;
    pc[36] = r;    pc[37] = g;    pc[38] = b;    pc[39] = 1.0f;
    vkCmdPushConstants(cmd, g_obj_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), pc);
}

/* terrain cell-grid wire (mesh_frag.glsl: light.xyz = 0 and light.w < -1.5 -> only cell borders are drawn, colour = sun.rgb) */
static void push_mesh_pc_wiregrid(VkCommandBuffer cmd, const editor_mat4* mv, const editor_mat4* proj, float r, float g, float b)
{
    float pc[40];
    memcpy(&pc[0],  mv,   sizeof(editor_mat4));
    memcpy(&pc[16], proj, sizeof(editor_mat4));
    pc[32] = 0.0f; pc[33] = 0.0f; pc[34] = 0.0f; pc[35] = -2.0f;
    pc[36] = r;    pc[37] = g;    pc[38] = b;    pc[39] = 1.0f;
    vkCmdPushConstants(cmd, g_obj_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), pc);
}

#define XRAY_ALPHA 0.35f

/* ------------------------------------------------------------------ */
/* Mesh upload                                                         */
/* ------------------------------------------------------------------ */
static bool find_memory_type(VkPhysicalDevice pd, uint32_t type_bits, VkMemoryPropertyFlags want, uint32_t* out_index)
{
    VkPhysicalDeviceMemoryProperties props;
    vkGetPhysicalDeviceMemoryProperties(pd, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        /* require ALL requested flags, not just any one of them */
        if ((type_bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & want) == want) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static bool create_buffer_with_data(VkDevice device, VkPhysicalDevice pd, VkBufferUsageFlags usage,
                                    const void* data, VkDeviceSize size,
                                    VkBuffer* out_buf, VkDeviceMemory* out_mem)
{
    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bi, NULL, out_buf) != VK_SUCCESS) return false;

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, *out_buf, &req);

    uint32_t type_index = 0;
    if (!find_memory_type(pd, req.memoryTypeBits,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &type_index)) {
        vkDestroyBuffer(device, *out_buf, NULL);
        *out_buf = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = type_index;
    if (vkAllocateMemory(device, &ai, NULL, out_mem) != VK_SUCCESS) {
        vkDestroyBuffer(device, *out_buf, NULL);
        *out_buf = VK_NULL_HANDLE;
        return false;
    }
    vkBindBufferMemory(device, *out_buf, *out_mem, 0);

    void* mapped = NULL;
    if (vkMapMemory(device, *out_mem, 0, size, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(device, *out_buf, NULL);
        vkFreeMemory(device, *out_mem, NULL);
        *out_buf = VK_NULL_HANDLE; *out_mem = VK_NULL_HANDLE;
        return false;
    }
    memcpy(mapped, data, (size_t)size);
    vkUnmapMemory(device, *out_mem);
    return true;
}

bool upload_scene_object_mesh(system_scene_object* obj, VkDevice device, VkPhysicalDevice physical_device,
                              const editor_vertex* vertices, uint32_t vertex_count,
                              const uint32_t* indices, uint32_t index_count)
{
    if (!obj || !device || !physical_device) return false;

    VkBuffer vb = VK_NULL_HANDLE, ib = VK_NULL_HANDLE;
    VkDeviceMemory vm = VK_NULL_HANDLE, im = VK_NULL_HANDLE;

    if (!create_buffer_with_data(device, physical_device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 vertices, sizeof(editor_vertex) * vertex_count, &vb, &vm)) {
        phoenix_error("SCENE3D", "upload '%s': vertex buffer creation failed (%u verts)", obj->name, (unsigned)vertex_count);
        return false;
    }

    if (!create_buffer_with_data(device, physical_device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 indices, sizeof(uint32_t) * index_count, &ib, &im)) {
        phoenix_error("SCENE3D", "upload '%s': index buffer creation failed (%u indices)", obj->name, (unsigned)index_count);
        vkDestroyBuffer(device, vb, NULL);
        vkFreeMemory(device, vm, NULL);
        return false;
    }

    float max_r2 = 0.0f;
    for (uint32_t i = 0; i < vertex_count; i++) {
        float r2 = vertices[i].x * vertices[i].x + vertices[i].y * vertices[i].y + vertices[i].z * vertices[i].z;
        if (r2 > max_r2) max_r2 = r2;
    }

    /* Re-upload (terrain streaming, edit-mode vertex moves): the command buffer being recorded this
       frame may already reference the old buffers, so they must NOT be destroyed here. Queue them;
       em_gpu_tick() destroys them a few frames later. (em_commit_to_gpu no longer retires them itself.) */
    if (obj->vertex_buffer || obj->index_buffer) {
        em_gpu_retire(obj->vertex_buffer, obj->vertex_memory, obj->index_buffer, obj->index_memory);
    }

    obj->vertex_buffer = (uint64_t)(uintptr_t)vb;
    obj->vertex_memory = (uint64_t)(uintptr_t)vm;
    obj->index_buffer  = (uint64_t)(uintptr_t)ib;
    obj->index_memory  = (uint64_t)(uintptr_t)im;
    obj->index_count   = index_count;
    obj->bounds_radius = sqrtf(max_r2);
    phoenix_info("SCENE3D", "uploaded mesh '%s' (id %u): %u verts, %u indices, bounds radius %.1f", obj->name, (unsigned)obj->id, (unsigned)vertex_count, (unsigned)index_count, obj->bounds_radius);
    return true;
}

/* ------------------------------------------------------------------ */
/* World origin marker: a small bright slab at (0, 0, 0) so the grid   */
/* origin is easy to spot. Drawn with the mesh pipeline.               */
/* ------------------------------------------------------------------ */
#define ORIGIN_MARKER_HALF_XZ 0.40f
#define ORIGIN_MARKER_HALF_Y  0.03f

static VkBuffer       g_origin_vertex_buffer = VK_NULL_HANDLE;
static VkDeviceMemory g_origin_vertex_memory = VK_NULL_HANDLE;
static VkBuffer       g_origin_index_buffer  = VK_NULL_HANDLE;
static VkDeviceMemory g_origin_index_memory  = VK_NULL_HANDLE;
static bool           g_origin_marker_failed = false;

static bool create_origin_marker_mesh(void)
{
    if (g_origin_vertex_buffer != VK_NULL_HANDLE) return true;
    if (g_origin_marker_failed) return false;

    VkDevice         device          = render_context.logical_device;
    VkPhysicalDevice physical_device = render_context.physical_device;
    if (!device || !physical_device) return false;   /* renderer not ready yet, try again next frame */

    const float sgn[8][3] = {
        {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
        {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}
    };

    editor_vertex vertices[8];
    memset(vertices, 0, sizeof(vertices));   /* uv + normal must be zero (zero normal = unlit marker, nothing to light) */
    for (int i = 0; i < 8; i++) {
        vertices[i].x = sgn[i][0] * ORIGIN_MARKER_HALF_XZ;
        vertices[i].y = ORIGIN_MARKER_HALF_Y + sgn[i][1] * ORIGIN_MARKER_HALF_Y;   /* sits on y = 0 */
        vertices[i].z = sgn[i][2] * ORIGIN_MARKER_HALF_XZ;
        vertices[i].r = 1.0f;
        vertices[i].g = 0.25f;
        vertices[i].b = 0.65f;
        vertices[i].a = 1.0f;
    }

    const uint32_t indices[36] = {
        4, 5, 6,  6, 7, 4,
        1, 0, 3,  3, 2, 1,
        0, 4, 7,  7, 3, 0,
        5, 1, 2,  2, 6, 5,
        3, 7, 6,  6, 2, 3,
        0, 1, 5,  5, 4, 0
    };

    if (!create_buffer_with_data(device, physical_device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 vertices, sizeof(vertices), &g_origin_vertex_buffer, &g_origin_vertex_memory)) {
        g_origin_marker_failed = true;
        phoenix_error("SCENE3D", "Origin marker vertex buffer creation failed.");
        return false;
    }

    if (!create_buffer_with_data(device, physical_device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 indices, sizeof(indices), &g_origin_index_buffer, &g_origin_index_memory)) {
        vkDestroyBuffer(device, g_origin_vertex_buffer, NULL);
        vkFreeMemory(device, g_origin_vertex_memory, NULL);
        g_origin_vertex_buffer = VK_NULL_HANDLE;
        g_origin_vertex_memory = VK_NULL_HANDLE;
        g_origin_marker_failed = true;
        phoenix_error("SCENE3D", "Origin marker index buffer creation failed.");
        return false;
    }

    return true;
}

static void draw_origin_marker(VkCommandBuffer cmd, const editor_mat4* view, const editor_mat4* proj)
{
    if (!create_origin_marker_mesh()) return;

    push_mesh_pc(cmd, view, proj, k_unlit);   /* model matrix is identity, so modelview = view */

    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &g_origin_vertex_buffer, &off);
    vkCmdBindIndexBuffer(cmd, g_origin_index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
}

/* ------------------------------------------------------------------ */
/* Pipeline                                                            */
/* Called at the end of create_vulkan_graphics_pipelines().            */
/* Shaders: mesh_vert.spv / mesh_frag.spv (next to the exe).           */
/* ------------------------------------------------------------------ */
bool create_scene_object_pipeline(VkDevice device, VkRenderPass renderpass, bool has_depth)
{
    const VkSampleCountFlagBits samples = get_render_msaa_sample_count();

    if (g_obj_pipeline != VK_NULL_HANDLE) {
        if (g_obj_renderpass == renderpass && g_obj_samples == samples) return true;
        /* render pass or sample face_type changed: pipelines built for the old one are invalid, rebuild them */
        vkDeviceWaitIdle(device);
        scene_look_shutdown(device);
        if (g_line_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_line_pipeline, NULL); g_line_pipeline = VK_NULL_HANDLE; }
        if (g_wire_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_wire_pipeline, NULL); g_wire_pipeline = VK_NULL_HANDLE; }
        if (g_wiregrid_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_wiregrid_pipeline, NULL); g_wiregrid_pipeline = VK_NULL_HANDLE; }
        if (g_xray_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_xray_pipeline, NULL); g_xray_pipeline = VK_NULL_HANDLE; }
        if (g_border_line_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_border_line_pipeline, NULL); g_border_line_pipeline = VK_NULL_HANDLE; }
        if (g_border_tri_pipeline  != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_border_tri_pipeline,  NULL); g_border_tri_pipeline  = VK_NULL_HANDLE; }
        vkDestroyPipeline(device, g_obj_pipeline, NULL);  g_obj_pipeline = VK_NULL_HANDLE;
        if (g_obj_layout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, g_obj_layout, NULL); g_obj_layout = VK_NULL_HANDLE; }
    }

    VkShaderModule vs = load_shader_binary_module(device, "mesh_vert.spv");
    VkShaderModule fs = load_shader_binary_module(device, "mesh_frag.spv");
    if (!vs || !fs) {
        phoenix_error("SCENE3D", "Failed to load mesh_vert.spv / mesh_frag.spv");
        if (vs) vkDestroyShaderModule(device, vs, NULL);
        if (fs) vkDestroyShaderModule(device, fs, NULL);
        return false;
    }

    VkPhysicalDeviceProperties dev_props;
    vkGetPhysicalDeviceProperties(render_context.physical_device, &dev_props);
    if (dev_props.limits.maxPushConstantsSize < MESH_PC_SIZE) {
        phoenix_error("SCENE3D", "GPU push constant limit is %u bytes, mesh shader needs %u.",
                      dev_props.limits.maxPushConstantsSize, (uint32_t)MESH_PC_SIZE);
        vkDestroyShaderModule(device, vs, NULL);
        vkDestroyShaderModule(device, fs, NULL);
        return false;
    }

    /* Set 0 = the skin (one uniform buffer, read by mesh_frag.glsl). The layout and the pool outlive pipeline rebuilds. */
    if (!skin_gpu_create_layout(device)) {
        vkDestroyShaderModule(device, vs, NULL);
        vkDestroyShaderModule(device, fs, NULL);
        return false;
    }
    VkDescriptorSetLayout skin_set_layout = skin_gpu_get_layout();

    VkPushConstantRange pcr = { VK_SHADER_STAGE_VERTEX_BIT, 0, MESH_PC_SIZE };   /* modelview + proj + light (matrices stay push constants) */
    VkPipelineLayoutCreateInfo li = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    li.setLayoutCount = 1;
    li.pSetLayouts = &skin_set_layout;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(device, &li, NULL, &g_obj_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vs, NULL);
        vkDestroyShaderModule(device, fs, NULL);
        g_obj_layout = VK_NULL_HANDLE;
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,   vs, "main", NULL },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL }
    };

    VkVertexInputBindingDescription bind = { 0, (uint32_t)sizeof(editor_vertex), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attrs[4] = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    (uint32_t)offsetof(editor_vertex, x)  },   /* position */
        { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)offsetof(editor_vertex, r)  },   /* colour   */
        { 2, 0, VK_FORMAT_R32G32_SFLOAT,       (uint32_t)offsetof(editor_vertex, u)  },   /* uv       */
        { 3, 0, VK_FORMAT_R32G32B32_SFLOAT,    (uint32_t)offsetof(editor_vertex, nx) }    /* normal   */
    };
    VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vi.vertexBindingDescriptionCount = 1;   vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 4; vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1; vp.scissorCount = 1;   /* both dynamic */

    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.0f;
    rs.frontFace = SCENE_OBJECT_FRONT_FACE;
    /* Always draw both sides: terrain, edit meshes and flipped normals all stay visible regardless of winding. */
    (void)has_depth;
    rs.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = samples;   /* must equal the offscreen pass sample face_type */

    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable  = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba = {0};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1; cb.pAttachments = &cba;

    VkDynamicState dyn[3] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS };   /* 3rd only used by the x-ray pipeline */
    VkPipelineDynamicStateCreateInfo dsi = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dsi.dynamicStateCount = 2; dsi.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pi.stageCount = 2;              pi.pStages = stages;
    pi.pVertexInputState = &vi;     pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;        pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;     pi.pDepthStencilState = &ds;   /* the offscreen pass always has a depth attachment; ignored by Vulkan if a pass has none */
    pi.pColorBlendState = &cb;      pi.pDynamicState = &dsi;
    pi.layout = g_obj_layout;       pi.renderPass = renderpass;  pi.subpass = 0;

    VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &g_obj_pipeline);
    if (res == VK_SUCCESS) { g_obj_renderpass = renderpass; g_obj_samples = samples; }

    /* Second pipeline for wireframe helpers (camera outline + frustum): same shaders, layout and
       vertex format, but LINE_LIST and no culling. */
    if (res == VK_SUCCESS) {
        ia.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        rs.cullMode = VK_CULL_MODE_NONE;
        VkResult line_res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &g_line_pipeline);
        if (line_res != VK_SUCCESS) {
            phoenix_error("SCENE3D", "vkCreateGraphicsPipelines failed for line helpers (%d)", (int)line_res);
            g_line_pipeline = VK_NULL_HANDLE;
        }

        /* Wireframe pipeline: the normal triangle mesh drawn as lines. LESS_OR_EQUAL + a small negative depth bias
           pulls the lines toward the camera so they sit on top of the shaded surface (Wireframe on Shaded). */
        if (g_device_fill_mode_non_solid) {
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            rs.polygonMode = VK_POLYGON_MODE_LINE;
            rs.depthBiasEnable = VK_TRUE;
            rs.depthBiasConstantFactor = -2.0f;
            rs.depthBiasSlopeFactor = -1.0f;
            ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            VkResult wire_res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &g_wire_pipeline);
            if (wire_res != VK_SUCCESS) {
                phoenix_error("SCENE3D", "vkCreateGraphicsPipelines failed for wireframe (%d)", (int)wire_res);
                g_wire_pipeline = VK_NULL_HANDLE;
            }
            /* Terrain wire: same state (bias, LESS_OR_EQUAL, no cull) but FILLED. mesh_frag.glsl discards everything except the cell borders. */
            rs.polygonMode = VK_POLYGON_MODE_FILL;
            VkResult grid_res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &g_wiregrid_pipeline);
            if (grid_res != VK_SUCCESS) {
                phoenix_error("SCENE3D", "vkCreateGraphicsPipelines failed for terrain wire grid (%d)", (int)grid_res);
                g_wiregrid_pipeline = VK_NULL_HANDLE;
            }
            rs.depthBiasEnable = VK_FALSE;
            rs.depthBiasConstantFactor = 0.0f;
            rs.depthBiasSlopeFactor = 0.0f;
            ds.depthCompareOp = VK_COMPARE_OP_LESS;
        } else {
            phoenix_warn("SCENE3D", "GPU has no fillModeNonSolid: Wireframe buttons fall back to shaded.");
        }

        /* Border pipelines (region + chunk outlines). Depth tested with LESS_OR_EQUAL and a strong bias toward the camera, so the
           lines sit ON the ground at any distance without z-fighting, and they never write depth. */
        {
            rs.polygonMode = VK_POLYGON_MODE_FILL;
            rs.depthBiasEnable = VK_TRUE;
            rs.depthBiasConstantFactor = -16.0f;
            rs.depthBiasSlopeFactor = -4.0f;
            ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            ds.depthWriteEnable = VK_FALSE;
            ia.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &g_border_line_pipeline) != VK_SUCCESS) g_border_line_pipeline = VK_NULL_HANDLE;
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &g_border_tri_pipeline) != VK_SUCCESS) g_border_tri_pipeline = VK_NULL_HANDLE;
            rs.depthBiasEnable = VK_FALSE;
            rs.depthBiasConstantFactor = 0.0f;
            rs.depthBiasSlopeFactor = 0.0f;
            ds.depthCompareOp = VK_COMPARE_OP_LESS;
            ds.depthWriteEnable = VK_TRUE;
        }

        /* X-Ray pipeline: filled triangles, blended with the dynamic blend constant alpha, depth test on, depth write off. */
        {
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            ds.depthWriteEnable = VK_FALSE;
            cba.blendEnable         = VK_TRUE;
            cba.srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
            cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            cba.colorBlendOp        = VK_BLEND_OP_ADD;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            cba.alphaBlendOp        = VK_BLEND_OP_ADD;
            dsi.dynamicStateCount = 3;
            VkResult xray_res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &g_xray_pipeline);
            if (xray_res != VK_SUCCESS) {
                phoenix_error("SCENE3D", "vkCreateGraphicsPipelines failed for x-ray (%d)", (int)xray_res);
                g_xray_pipeline = VK_NULL_HANDLE;
            }
            dsi.dynamicStateCount = 2;
            cba.blendEnable = VK_FALSE;
            ds.depthWriteEnable = VK_TRUE;
        }
    }

    vkDestroyShaderModule(device, vs, NULL);
    vkDestroyShaderModule(device, fs, NULL);

    /* Gradient background pipeline: built against the same offscreen pass. */
    if (res == VK_SUCCESS) scene_look_create_pipelines(device, renderpass);

    if (res != VK_SUCCESS) {
        phoenix_error("SCENE3D", "vkCreateGraphicsPipelines failed for scene objects (%d)", (int)res);
        vkDestroyPipelineLayout(device, g_obj_layout, NULL);
        g_obj_layout = VK_NULL_HANDLE;
        g_obj_pipeline = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Draw: called from draw_scene_3d_in_rect() right after the grid draw */
/* view/proj are the same 16-float arrays pushed to the grid shader.   */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* Camera helpers: an outline (border) around the camera glyph plus    */
/* the frustum lines. One shared LINE_LIST mesh in camera-local space,  */
/* moved with each camera object's model matrix.                       */
/* ------------------------------------------------------------------ */
#define CAMERA_WIRE_MAX_VERTS    64
#define CAMERA_FRUSTUM_DEPTH     16.0f      /* meters (4 grid cells) */
#define CAMERA_FRUSTUM_HALF_TAN  0.57735f   /* tan(30 deg): half of the 60 deg game camera fov */
#define CAMERA_FRUSTUM_ASPECT    1.7778f    /* 16:9 */

static VkBuffer       g_camera_wire_buffer       = VK_NULL_HANDLE;
static VkDeviceMemory g_camera_wire_memory       = VK_NULL_HANDLE;
static uint32_t       g_camera_wire_vertex_count = 0;
static bool           g_camera_wire_failed       = false;

static void push_wire_line(editor_vertex* verts, uint32_t* face_type, const float* a, const float* b, const float* color)
{
    editor_vertex* vert_to_edge_array = &verts[*face_type];
    memset(vert_to_edge_array, 0, 2 * sizeof(editor_vertex));   /* uv + normal = 0 for line helpers */
    vert_to_edge_array[0].x = a[0]; vert_to_edge_array[0].y = a[1]; vert_to_edge_array[0].z = a[2];
    vert_to_edge_array[0].r = color[0]; vert_to_edge_array[0].g = color[1]; vert_to_edge_array[0].b = color[2]; vert_to_edge_array[0].a = color[3];
    vert_to_edge_array[1].x = b[0]; vert_to_edge_array[1].y = b[1]; vert_to_edge_array[1].z = b[2];
    vert_to_edge_array[1].r = color[0]; vert_to_edge_array[1].g = color[1]; vert_to_edge_array[1].b = color[2]; vert_to_edge_array[1].a = color[3];
    *face_type += 2;
}

static bool create_camera_wire_mesh(void)
{
    if (g_camera_wire_buffer != VK_NULL_HANDLE) return true;
    if (g_camera_wire_failed) return false;

    VkDevice         device          = render_context.logical_device;
    VkPhysicalDevice physical_device = render_context.physical_device;
    if (!device || !physical_device) return false;   /* renderer not ready yet, try again next frame */

    /* Same dimensions as the glyph in create_system_camera(size = 1.0): body box + lens end. */
    const float bx = 0.30f, by = 0.20f, bz = 0.40f;
    const float lx = 0.45f, ly = 0.30f, lz = 1.00f;
    const float p[12][3] = {
        {-bx,-by,-bz}, { bx,-by,-bz}, { bx, by,-bz}, {-bx, by,-bz},
        {-bx,-by, bz}, { bx,-by, bz}, { bx, by, bz}, {-bx, by, bz},
        {-lx,-ly,-lz}, { lx,-ly,-lz}, { lx, ly,-lz}, {-lx, ly,-lz}
    };
    const int edges[20][2] = {
        {0,1},{1,2},{2,3},{3,0},      /* body front */
        {4,5},{5,6},{6,7},{7,4},      /* body back  */
        {0,4},{1,5},{2,6},{3,7},      /* body sides */
        {8,9},{9,10},{10,11},{11,8},  /* lens end   */
        {0,8},{1,9},{2,10},{3,11}     /* lens sides */
    };

    const float outline_color[4]  = { 0.95f, 0.95f, 0.95f, 1.0f };   /* border around the camera object */
    const float frustum_color[4]  = { 0.95f, 0.80f, 0.25f, 1.0f };   /* frustum lines */

    editor_vertex verts[CAMERA_WIRE_MAX_VERTS];
    uint32_t face_type = 0;

    for (int e = 0; e < 20; e++) {
        push_wire_line(verts, &face_type, p[edges[e][0]], p[edges[e][1]], outline_color);
    }

    /* Frustum: apex at the camera origin, far rectangle CAMERA_FRUSTUM_DEPTH down local -Z */
    const float apex[3] = { 0.0f, 0.0f, 0.0f };
    const float fh = CAMERA_FRUSTUM_DEPTH * CAMERA_FRUSTUM_HALF_TAN;
    const float fw = fh * CAMERA_FRUSTUM_ASPECT;
    const float fz = -CAMERA_FRUSTUM_DEPTH;
    const float far_corner[4][3] = {
        {-fw,-fh, fz}, { fw,-fh, fz}, { fw, fh, fz}, {-fw, fh, fz}
    };
    for (int i = 0; i < 4; i++) {
        push_wire_line(verts, &face_type, apex, far_corner[i], frustum_color);
        push_wire_line(verts, &face_type, far_corner[i], far_corner[(i + 1) % 4], frustum_color);
    }

    if (!create_buffer_with_data(device, physical_device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 verts, sizeof(editor_vertex) * face_type,
                                 &g_camera_wire_buffer, &g_camera_wire_memory)) {
        g_camera_wire_failed = true;
        phoenix_error("SCENE3D", "Camera wire mesh creation failed.");
        return false;
    }

    g_camera_wire_vertex_count = face_type;
    return true;
}

static void draw_camera_wire_outlines(VkCommandBuffer cmd, const editor_mat4* view, const editor_mat4* proj)
{
    if (g_line_pipeline == VK_NULL_HANDLE) return;

    bool bound = false;
    for (uint32_t i = 1; i < g_scene_registry.object_count; i++) {   /* 0 = root */
        const system_scene_object* o = &g_scene_registry.objects[i];
        if (o->type != scene_object_camera) continue;

        if (!bound) {
            if (!create_camera_wire_mesh()) return;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_line_pipeline);
            VkDeviceSize off = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &g_camera_wire_buffer, &off);
            bound = true;
        }

        editor_mat4 model = system_math_model_matrix(o->position, o->rotation, o->scale);
        editor_mat4 mv    = system_math_mat4_multiply(*view, model);

        push_mesh_pc(cmd, &mv, proj, k_unlit);
        vkCmdDraw(cmd, g_camera_wire_vertex_count, 1, 0, 0);
    }
}

/* ------------------------------------------------------------------ */
/* Blue culling plane: just the END of the game camera's frustum, at   */
/* game_camera_far, so you can see how big the visible area is at the  */
/* point where things get culled. Drawn for the active game camera     */
/* only, and ignores the object's scale (the Game view ignores it too).*/
/* ------------------------------------------------------------------ */
static VkBuffer       g_cull_buffer = VK_NULL_HANDLE;
static VkDeviceMemory g_cull_memory = VK_NULL_HANDLE;
static bool           g_cull_failed = false;
#define CULL_VERT_COUNT 8

static bool create_camera_cull_mesh(void)
{
    if (g_cull_buffer != VK_NULL_HANDLE) return true;
    if (g_cull_failed) return false;

    VkDevice         device          = render_context.logical_device;
    VkPhysicalDevice physical_device = render_context.physical_device;
    if (!device || !physical_device) return false;

    const float fh = game_camera_far * tanf(game_camera_fov * 0.5f);
    const float fw = fh * CAMERA_FRUSTUM_ASPECT;
    const float fz = -game_camera_far;
    const float c[4][3] = { {-fw,-fh,fz}, {fw,-fh,fz}, {fw,fh,fz}, {-fw,fh,fz} };
    const float blue[4] = { 0.20f, 0.50f, 1.00f, 1.0f };

    editor_vertex verts[CULL_VERT_COUNT];
    uint32_t face_type = 0;
    for (int i = 0; i < 4; i++) push_wire_line(verts, &face_type, c[i], c[(i + 1) % 4], blue);

    if (!create_buffer_with_data(device, physical_device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 verts, sizeof(editor_vertex) * face_type, &g_cull_buffer, &g_cull_memory)) {
        g_cull_failed = true;
        phoenix_error("SCENE3D", "Camera culling plane mesh creation failed.");
        return false;
    }
    return true;
}

static void draw_camera_cull_plane(VkCommandBuffer cmd, const editor_mat4* view, const editor_mat4* proj)
{
    if (g_line_pipeline == VK_NULL_HANDLE) return;
    const system_scene_object* cam = find_scene_game_camera();
    if (!cam) return;
    if (!create_camera_cull_mesh()) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_line_pipeline);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &g_cull_buffer, &off);

    editor_vec3 unit = { 1.0f, 1.0f, 1.0f };
    editor_mat4 model = system_math_model_matrix(cam->position, cam->rotation, unit);
    editor_mat4 mv    = system_math_mat4_multiply(*view, model);

    push_mesh_pc(cmd, &mv, proj, k_unlit);
    vkCmdDraw(cmd, CULL_VERT_COUNT, 1, 0, 0);
}

/* Bind a skin set at set 0 of the mesh layout. Only rebinds when the set changes. */
static void bind_skin_set(VkCommandBuffer cmd, VkDescriptorSet set, VkDescriptorSet* bound)
{
    if (set == VK_NULL_HANDLE || set == *bound) return;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_obj_layout, 0, 1, &set, 0, NULL);
    *bound = set;
}

/* Mesh loop shared by the Scene view and the Game view.
   skip_cameras = true for the Game view (camera glyphs are editor helpers), and in the Scene view when Cameras is off.
   flat_rgb != NULL draws every mesh in that one unlit colour (wire passes).
   use_skins = true: each object binds its skin's descriptor set (Scene view Textured, and the Game view).
   false: every object binds the shared no-skin set, so the picture is exactly what Smooth Shade always drew. */
static void draw_scene_mesh_list(VkCommandBuffer cmd, const editor_mat4* view, const editor_mat4* proj, bool skip_cameras,
                                 const float* flat_rgb, bool use_skins)
{
    static bool logged_drawn = false;

    float light[4];
    scene_look_get_light_view(view, light);   /* zero xyz while the Lit button is off */

    const bool skins_active = use_skins && flat_rgb == NULL;
    VkDescriptorSet bound = VK_NULL_HANDLE;
    bind_skin_set(cmd, skin_gpu_get_empty_set(), &bound);   /* a valid set must be bound for every draw of this layout */

    VkPipeline cur_pipe = g_wire_pipeline;   /* what the caller bound for the wire passes (ignored when flat_rgb == NULL) */

    uint32_t drawn = 0, skipped_terrain = 0;
    for (uint32_t i = 1; i < g_scene_registry.object_count; i++) {   /* 0 = root */
        const system_scene_object* o = &g_scene_registry.objects[i];
        if (o->vertex_buffer == 0 || o->index_buffer == 0 || o->index_count == 0) {
            if (o->type == scene_object_mesh && is_terrain_object(o)) skipped_terrain++;   /* a terrain region with no mesh = hole */
            continue;
        }
        if (skip_cameras && o->type == scene_object_camera) continue;

        if (skins_active) {
            VkDescriptorSet want = skin_gpu_get_empty_set();           /* no skin: shared grey set */
            if (o->type != scene_object_camera) {
                const uint32_t skin_id = get_object_skin_id(o->id);    /* 0 = no skin, or the skin was deleted */
                if (skin_id != 0) {
                    if (!skin_gpu_has(skin_id)) texshop_build_skin_gpu(skin_id);   /* first use: build once, then cached per skin */
                    want = skin_gpu_get_set(skin_id);
                }
            }
            bind_skin_set(cmd, want, &bound);
        }

        editor_mat4 model = system_math_model_matrix(o->position, o->rotation, o->scale);
        editor_mat4 mv    = system_math_mat4_multiply(*view, model);

        if (flat_rgb) {
            /* Terrain (and its regions) use the filled cell-grid wire; every other mesh keeps the polygon-mode line wire. */
            const bool grid = g_wiregrid_pipeline != VK_NULL_HANDLE && is_terrain_object(o);
            const VkPipeline want = grid ? g_wiregrid_pipeline : g_wire_pipeline;
            if (want != cur_pipe) { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, want); cur_pipe = want; }
            if (grid) push_mesh_pc_wiregrid(cmd, &mv, proj, flat_rgb[0], flat_rgb[1], flat_rgb[2]);
            else      push_mesh_pc_flat(cmd, &mv, proj, flat_rgb[0], flat_rgb[1], flat_rgb[2]);
        } else {
            push_mesh_pc(cmd, &mv, proj, (o->type == scene_object_camera) ? k_unlit : light);   /* camera glyph stays flat */
        }

        VkBuffer vb = (VkBuffer)(uintptr_t)o->vertex_buffer;
        VkBuffer ib = (VkBuffer)(uintptr_t)o->index_buffer;
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
        vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, o->index_count, 1, 0, 0, 0);
        drawn++;
    }

    if (flat_rgb == NULL && !skip_cameras) {
        static uint32_t last_drawn = 0xFFFFFFFFu, last_skipped = 0xFFFFFFFFu;
        if (drawn != last_drawn || skipped_terrain != last_skipped) {
            last_drawn = drawn; last_skipped = skipped_terrain;
            phoenix_info("SCENE3D", "scene draw: %u mesh object(s) drawn, %u terrain region(s) have NO mesh", (unsigned)drawn, (unsigned)skipped_terrain);
        }
    }

    if (!skip_cameras && drawn > 0 && !logged_drawn) {
        phoenix_info("SCENE3D", "draw_scene_objects: drawing %u mesh object(s)", drawn);
        logged_drawn = true;
    }
}

/* ------------------------------------------------------------------ */
/* Terrain borders, the Maya / ZBrush way: the mesh never changes, only */
/* lines are laid over it, in levels of importance.                     */
/*   region borders  bright orange, a wide ribbon plus a line, with a    */
/*                   tall post at every region corner (4 x 4 lines)      */
/*   chunk borders   thin cyan lines on every 500 cube boundary          */
/*   cell grid       the terrain wire shader (every 4 mesh cell)            */
/* Lines follow the terrain height and are rebuilt when it is edited.   */
/* ------------------------------------------------------------------ */
#define BORDER_STEP_CELLS   20        /* one line segment every 20 cubes (80 mesh) */
#define BORDER_RIBBON_HALF  14.0f     /* region ribbon is 28 mesh wide */
#define BORDER_POST_HEIGHT  600.0f

static bool           g_borders_on = true;
static VkBuffer       g_bd_line_vb = VK_NULL_HANDLE, g_bd_tri_vb = VK_NULL_HANDLE, g_bd_tri_ib = VK_NULL_HANDLE;
static VkDeviceMemory g_bd_line_vm = VK_NULL_HANDLE, g_bd_tri_vm = VK_NULL_HANDLE, g_bd_tri_im = VK_NULL_HANDLE;
static uint32_t       g_bd_line_count = 0, g_bd_tri_index_count = 0;
static uint32_t       g_bd_revision = 0xFFFFFFFFu;
static int            g_bd_chunks = -1, g_bd_region_chunks = -1;
static bool           g_bd_failed = false;

void scene_set_terrain_borders(bool on) { g_borders_on = on; }
bool scene_get_terrain_borders(void)    { return g_borders_on; }

static void bd_vertex(editor_vertex* o, float x, float y, float z, const float c[3])
{
    memset(o, 0, sizeof(*o));       /* zero normal = unlit: the shader draws the vertex colour */
    o->x = x; o->y = y; o->z = z;
    o->r = c[0]; o->g = c[1]; o->b = c[2]; o->a = 1.0f;
}

static void terrain_borders_release(void)
{
    if (g_bd_line_vb) em_gpu_retire((uint64_t)(uintptr_t)g_bd_line_vb, (uint64_t)(uintptr_t)g_bd_line_vm, 0, 0);
    if (g_bd_tri_vb)  em_gpu_retire((uint64_t)(uintptr_t)g_bd_tri_vb,  (uint64_t)(uintptr_t)g_bd_tri_vm, (uint64_t)(uintptr_t)g_bd_tri_ib, (uint64_t)(uintptr_t)g_bd_tri_im);
    g_bd_line_vb = g_bd_tri_vb = g_bd_tri_ib = VK_NULL_HANDLE;
    g_bd_line_vm = g_bd_tri_vm = g_bd_tri_im = VK_NULL_HANDLE;
    g_bd_line_count = g_bd_tri_index_count = 0;
}

static void terrain_borders_shutdown(VkDevice device)
{
    if (g_bd_line_vb) vkDestroyBuffer(device, g_bd_line_vb, NULL);
    if (g_bd_line_vm) vkFreeMemory(device, g_bd_line_vm, NULL);
    if (g_bd_tri_vb)  vkDestroyBuffer(device, g_bd_tri_vb, NULL);
    if (g_bd_tri_vm)  vkFreeMemory(device, g_bd_tri_vm, NULL);
    if (g_bd_tri_ib)  vkDestroyBuffer(device, g_bd_tri_ib, NULL);
    if (g_bd_tri_im)  vkFreeMemory(device, g_bd_tri_im, NULL);
    g_bd_line_vb = g_bd_tri_vb = g_bd_tri_ib = VK_NULL_HANDLE;
    g_bd_line_vm = g_bd_tri_vm = g_bd_tri_im = VK_NULL_HANDLE;
    g_bd_line_count = g_bd_tri_index_count = 0;
    g_bd_revision = 0xFFFFFFFFu; g_bd_chunks = g_bd_region_chunks = -1; g_bd_failed = false;
}

static void build_terrain_borders(void)
{
    const system_terrain* t = &g_terrain;
    VkDevice device = render_context.logical_device;
    VkPhysicalDevice pd = render_context.physical_device;
    if (!device || !pd) return;

    terrain_borders_release();
    g_bd_revision = t->edit_revision; g_bd_chunks = t->dims.chunks_x; g_bd_region_chunks = t->dims.region_chunks;

    const int   cc = t->dims.chunk_cells, nch = t->dims.chunks_x, rcn = t->dims.region_chunks, total = t->dims.cells_x;
    const float cs = t->settings.cell_size_m, ox = (float)t->dims.origin_x, oz = (float)t->dims.origin_z;
    const int   segs = (total + BORDER_STEP_CELLS - 1) / BORDER_STEP_CELLS;
    const float lift = 0.5f;

    static const float c_region[3] = { 1.00f, 0.55f, 0.10f };
    static const float c_chunk[3]  = { 0.30f, 0.78f, 0.95f };

    const size_t max_lines = (size_t)(nch + 1) * 2 * (size_t)segs * 2 + 16 * 2;
    const size_t max_rib_v = (size_t)((nch / rcn) + 1) * 2 * (size_t)segs * 4;
    editor_vertex* lv = (editor_vertex*)malloc(max_lines * sizeof(editor_vertex));
    editor_vertex* tv = (editor_vertex*)malloc(max_rib_v * sizeof(editor_vertex));
    uint32_t*      ti = (uint32_t*)malloc(max_rib_v / 4 * 6 * sizeof(uint32_t));
    if (!lv || !tv || !ti) { free(lv); free(tv); free(ti); g_bd_failed = true; return; }

    uint32_t nl = 0, nv = 0, ni = 0;
    for (int axis = 0; axis < 2; axis++) {                     /* 0: lines along Z at fixed X, 1: lines along X at fixed Z */
        for (int k = 0; k <= nch; k++) {
            const bool region = (k % rcn) == 0;
            const float fixed = axis == 0 ? ((float)(k * cc) - ox) * cs : ((float)(k * cc) - oz) * cs;
            const float* col = region ? c_region : c_chunk;
            for (int s = 0; s < segs; s++) {
                const int g0 = s * BORDER_STEP_CELLS, g1 = (g0 + BORDER_STEP_CELLS < total) ? g0 + BORDER_STEP_CELLS : total;
                const float a0 = ((float)g0 - (axis == 0 ? oz : ox)) * cs, a1 = ((float)g1 - (axis == 0 ? oz : ox)) * cs;
                const float x0 = axis == 0 ? fixed : a0, z0 = axis == 0 ? a0 : fixed;
                const float x1 = axis == 0 ? fixed : a1, z1 = axis == 0 ? a1 : fixed;
                const float y0 = terrain_sample_height_m(x0, z0) + lift, y1 = terrain_sample_height_m(x1, z1) + lift;
                bd_vertex(&lv[nl++], x0, y0, z0, col);
                bd_vertex(&lv[nl++], x1, y1, z1, col);
                if (region) {                                   /* ribbon: a flat strip lying on the ground */
                    const float px = axis == 0 ? BORDER_RIBBON_HALF : 0.0f, pz = axis == 0 ? 0.0f : BORDER_RIBBON_HALF;
                    const uint32_t b = nv;
                    bd_vertex(&tv[nv++], x0 - px, y0, z0 - pz, col);
                    bd_vertex(&tv[nv++], x0 + px, y0, z0 + pz, col);
                    bd_vertex(&tv[nv++], x1 + px, y1, z1 + pz, col);
                    bd_vertex(&tv[nv++], x1 - px, y1, z1 - pz, col);
                    ti[ni++] = b; ti[ni++] = b + 1; ti[ni++] = b + 2;
                    ti[ni++] = b + 2; ti[ni++] = b + 3; ti[ni++] = b;
                }
            }
        }
    }
    for (int kz = 0; kz <= nch; kz += rcn)                      /* tall post at every region corner */
        for (int kx = 0; kx <= nch; kx += rcn) {
            const float x = ((float)(kx * cc) - ox) * cs, z = ((float)(kz * cc) - oz) * cs;
            const float y = terrain_sample_height_m(x, z);
            bd_vertex(&lv[nl++], x, y, z, c_region);
            bd_vertex(&lv[nl++], x, y + BORDER_POST_HEIGHT, z, c_region);
        }

    bool ok = create_buffer_with_data(device, pd, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, lv, sizeof(editor_vertex) * nl, &g_bd_line_vb, &g_bd_line_vm);
    if (ok) g_bd_line_count = nl;
    if (ok && nv > 0) {
        ok = create_buffer_with_data(device, pd, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, tv, sizeof(editor_vertex) * nv, &g_bd_tri_vb, &g_bd_tri_vm)
          && create_buffer_with_data(device, pd, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, ti, sizeof(uint32_t) * ni, &g_bd_tri_ib, &g_bd_tri_im);
        if (ok) g_bd_tri_index_count = ni;
    }
    if (!ok) { phoenix_error("SCENE3D", "Terrain border mesh creation failed."); g_bd_failed = true; }
    free(lv); free(tv); free(ti);
}

static void draw_terrain_borders(VkCommandBuffer cmd, const editor_mat4* view, const editor_mat4* proj)
{
    if (!g_borders_on || !g_terrain.exists || g_bd_failed) return;
    if (g_border_line_pipeline == VK_NULL_HANDLE || g_border_tri_pipeline == VK_NULL_HANDLE) return;
    if (g_bd_revision != g_terrain.edit_revision || g_bd_chunks != g_terrain.dims.chunks_x || g_bd_region_chunks != g_terrain.dims.region_chunks)
        build_terrain_borders();
    if (g_bd_line_vb == VK_NULL_HANDLE) return;

    editor_vec3 obj_vert_pos = {0, 0, 0};
    terrain_root_position(&obj_vert_pos);
    const editor_vec3 zero = {0, 0, 0}, unit = {1, 1, 1};
    editor_mat4 model = system_math_model_matrix(obj_vert_pos, zero, unit);
    editor_mat4 mv    = system_math_mat4_multiply(*view, model);
    push_mesh_pc(cmd, &mv, proj, k_unlit);

    VkDeviceSize off = 0;
    if (g_bd_tri_vb != VK_NULL_HANDLE && g_bd_tri_index_count > 0) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_border_tri_pipeline);
        vkCmdBindVertexBuffers(cmd, 0, 1, &g_bd_tri_vb, &off);
        vkCmdBindIndexBuffer(cmd, g_bd_tri_ib, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, g_bd_tri_index_count, 1, 0, 0, 0);
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_border_line_pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, &g_bd_line_vb, &off);
    vkCmdDraw(cmd, g_bd_line_count, 1, 0, 0);
}

/* Scene view: origin marker, every mesh object, then camera outlines + frustum lines. */
void draw_scene_objects(VkCommandBuffer cmd, const float* view16, const float* proj16)
{
    static bool logged_missing_pipeline = false;

    if (g_obj_pipeline == VK_NULL_HANDLE) {
        if (!logged_missing_pipeline) {
            phoenix_error("SCENE3D", "draw_scene_objects: mesh pipeline is null, nothing will draw.");
            logged_missing_pipeline = true;
        }
        return;
    }

    editor_mat4 view, proj;
    memcpy(&view, view16, sizeof(view));
    memcpy(&proj, proj16, sizeof(proj));

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_obj_pipeline);

    /* The origin marker, camera outlines and wire passes draw with this layout too, so a set must be bound for them. */
    {
        VkDescriptorSet empty = skin_gpu_get_empty_set();
        if (empty != VK_NULL_HANDLE)
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_obj_layout, 0, 1, &empty, 0, NULL);
    }

    /* Origin marker first (the depth buffer sorts it against the real objects). */
    if (g_scene_view.show_origin) draw_origin_marker(cmd, &view, &proj);

    const bool skip_cams = !g_scene_view.show_cameras;
    const float* wire_light = g_scene_view.wire_color;             /* Wireframe mode lines (g_scene_view, saved as view.wire_color) */
    const float* wire_dark  = g_scene_view.wire_on_shaded_color;   /* Wireframe on Shaded lines (view.wire_on_shaded_color) */

    if (g_scene_view.shading == scene_shading_wire && g_wire_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_wire_pipeline);
        draw_scene_mesh_list(cmd, &view, &proj, skip_cams, wire_light, false);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_obj_pipeline);
    } else {
        /* Smooth Shade All and Textured. Textured is the switch that turns skins on: each object then binds its skin's set.
           Smooth Shade binds the shared no-skin set everywhere, which keeps the vertex colour. */
        if (g_scene_view.xray && g_xray_pipeline != VK_NULL_HANDLE) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_xray_pipeline);
            const float blend[4] = { 0.0f, 0.0f, 0.0f, XRAY_ALPHA };
            vkCmdSetBlendConstants(cmd, blend);
        }
        draw_scene_mesh_list(cmd, &view, &proj, skip_cams, NULL, g_scene_view.shading == scene_shading_textured);

        if (g_scene_view.wire_on_shaded && g_wire_pipeline != VK_NULL_HANDLE) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_wire_pipeline);
            draw_scene_mesh_list(cmd, &view, &proj, skip_cams, wire_dark, false);
        }
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_obj_pipeline);
    }

    draw_terrain_borders(cmd, &view, &proj);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_obj_pipeline);

    /* Lines last so the outline sits on top of the camera glyph. */
    if (g_scene_view.show_cameras) {
        draw_camera_wire_outlines(cmd, &view, &proj);
        draw_camera_cull_plane(cmd, &view, &proj);
    }
}

/* Game view: meshes only. No origin marker, no camera glyphs, no outlines, no frustum. */
void draw_game_scene_objects(VkCommandBuffer cmd, const float* view16, const float* proj16)
{
    if (g_obj_pipeline == VK_NULL_HANDLE) return;

    editor_mat4 view, proj;
    memcpy(&view, view16, sizeof(view));
    memcpy(&proj, proj16, sizeof(proj));

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_obj_pipeline);
    draw_scene_mesh_list(cmd, &view, &proj, true, NULL, true);   /* the Game view always shows skins */
}

/* ------------------------------------------------------------------ */
/* Cleanup: call from cleanup_modeler_objects_vram() / shutdown        */
/* ------------------------------------------------------------------ */
void shutdown_scene_object_rendering(VkDevice device)
{
    if (!device) return;
    vkDeviceWaitIdle(device);
    scene_look_shutdown(device);
    skin_gpu_shutdown(device);   /* skin buffers, sets, pool and set layout (after the pipelines that use them are idle) */

    for (uint32_t i = 0; i < g_scene_registry.object_count; i++) {
        system_scene_object* o = &g_scene_registry.objects[i];
        if (o->vertex_buffer) vkDestroyBuffer(device, (VkBuffer)(uintptr_t)o->vertex_buffer, NULL);
        if (o->vertex_memory) vkFreeMemory(device, (VkDeviceMemory)(uintptr_t)o->vertex_memory, NULL);
        if (o->index_buffer)  vkDestroyBuffer(device, (VkBuffer)(uintptr_t)o->index_buffer, NULL);
        if (o->index_memory)  vkFreeMemory(device, (VkDeviceMemory)(uintptr_t)o->index_memory, NULL);
        o->vertex_buffer = o->vertex_memory = o->index_buffer = o->index_memory = 0;
        o->index_count = 0;
    }

    if (g_origin_vertex_buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, g_origin_vertex_buffer, NULL); g_origin_vertex_buffer = VK_NULL_HANDLE; }
    if (g_origin_vertex_memory != VK_NULL_HANDLE) { vkFreeMemory(device, g_origin_vertex_memory, NULL);    g_origin_vertex_memory = VK_NULL_HANDLE; }
    if (g_origin_index_buffer  != VK_NULL_HANDLE) { vkDestroyBuffer(device, g_origin_index_buffer, NULL);  g_origin_index_buffer  = VK_NULL_HANDLE; }
    if (g_origin_index_memory  != VK_NULL_HANDLE) { vkFreeMemory(device, g_origin_index_memory, NULL);     g_origin_index_memory  = VK_NULL_HANDLE; }
    g_origin_marker_failed = false;

    if (g_camera_wire_buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, g_camera_wire_buffer, NULL); g_camera_wire_buffer = VK_NULL_HANDLE; }
    if (g_camera_wire_memory != VK_NULL_HANDLE) { vkFreeMemory(device, g_camera_wire_memory, NULL);    g_camera_wire_memory = VK_NULL_HANDLE; }
    g_camera_wire_vertex_count = 0;
    g_camera_wire_failed = false;

    if (g_cull_buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, g_cull_buffer, NULL); g_cull_buffer = VK_NULL_HANDLE; }
    if (g_cull_memory != VK_NULL_HANDLE) { vkFreeMemory(device, g_cull_memory, NULL);    g_cull_memory = VK_NULL_HANDLE; }
    g_cull_failed = false;

    if (g_line_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_line_pipeline, NULL); g_line_pipeline = VK_NULL_HANDLE; }
    if (g_wire_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_wire_pipeline, NULL); g_wire_pipeline = VK_NULL_HANDLE; }
    if (g_wiregrid_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_wiregrid_pipeline, NULL); g_wiregrid_pipeline = VK_NULL_HANDLE; }
    if (g_xray_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_xray_pipeline, NULL); g_xray_pipeline = VK_NULL_HANDLE; }
    if (g_border_line_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_border_line_pipeline, NULL); g_border_line_pipeline = VK_NULL_HANDLE; }
    if (g_border_tri_pipeline  != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_border_tri_pipeline,  NULL); g_border_tri_pipeline  = VK_NULL_HANDLE; }
    terrain_borders_shutdown(device);
    if (g_obj_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_obj_pipeline, NULL); g_obj_pipeline = VK_NULL_HANDLE; }
    if (g_obj_layout   != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, g_obj_layout, NULL); g_obj_layout = VK_NULL_HANDLE; }
    g_obj_renderpass = VK_NULL_HANDLE;
    g_obj_samples = (VkSampleCountFlagBits)0;
}