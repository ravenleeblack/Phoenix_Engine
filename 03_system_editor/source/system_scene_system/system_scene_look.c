#include "system_globals.h"

/* Maya-like defaults: lighter blue-grey at the top fading to a dark grey at the bottom. */
scene_background g_scene_bg  = { { 0.43f, 0.49f, 0.57f }, { 0.16f, 0.17f, 0.19f } };
scene_sun        g_scene_sun = { 40.0f, 50.0f, 0.30f };

static VkPipeline       g_bg_pipeline = VK_NULL_HANDLE;
static VkPipelineLayout g_bg_layout   = VK_NULL_HANDLE;

/* ------------------------------------------------------------------------- */
/* presets                                                                    */
/* ------------------------------------------------------------------------- */
static const struct { const char* name; float top[3]; float bottom[3]; } k_presets[SCENE_LOOK_PRESET_COUNT] = {
    { "Maya", { 0.43f, 0.49f, 0.57f }, { 0.16f, 0.17f, 0.19f } },
    { "Dark", { 0.20f, 0.21f, 0.23f }, { 0.08f, 0.08f, 0.09f } },
    { "Sky",  { 0.30f, 0.50f, 0.80f }, { 0.76f, 0.83f, 0.91f } },
    { "Flat", { 0.27f, 0.27f, 0.28f }, { 0.27f, 0.27f, 0.28f } }
};

const char* scene_look_preset_name(int i)
{
    return (i >= 0 && i < SCENE_LOOK_PRESET_COUNT) ? k_presets[i].name : "?";
}

void scene_look_apply_preset(int i)
{
    if (i < 0 || i >= SCENE_LOOK_PRESET_COUNT) return;
    memcpy(g_scene_bg.top,    k_presets[i].top,    sizeof(g_scene_bg.top));
    memcpy(g_scene_bg.bottom, k_presets[i].bottom, sizeof(g_scene_bg.bottom));
}

/* ------------------------------------------------------------------------- */
/* sun                                                                        */
/* ------------------------------------------------------------------------- */
void scene_look_get_light_view(const editor_mat4* view, float out[4])
{
    out[0] = out[1] = out[2] = 0.0f;
    out[3] = g_scene_sun.ambient;
    if (!g_light.enabled || !view) return;

    const float deg = 0.01745329252f;
    float az = g_scene_sun.azimuth_deg   * deg;
    float el = g_scene_sun.elevation_deg * deg;
    float d[3] = { cosf(el) * sinf(az), sinf(el), -cosf(el) * cosf(az) };   /* world space, toward the sun */

    /* rotate into view space: view is column-major, mesh[column][row] (same convention as system_scene_camera.c) */
    for (int r = 0; r < 3; r++) {
        out[r] = view->mesh[0][r] * d[0] + view->mesh[1][r] * d[1] + view->mesh[2][r] * d[2];
    }
}

/* ------------------------------------------------------------------------- */
/* background pipeline                                                        */
/* ------------------------------------------------------------------------- */
bool scene_look_create_pipelines(VkDevice device, VkRenderPass renderpass)
{
    if (g_bg_pipeline != VK_NULL_HANDLE) return true;

    VkShaderModule vs = load_shader_binary_module(device, "bg_vert.spv");
    VkShaderModule fs = load_shader_binary_module(device, "bg_frag.spv");
    if (!vs || !fs) {
        phoenix_error("SCENE3D", "Failed to load bg_vert.spv / bg_frag.spv (compile shaders/bg_*.glsl). Background gradient disabled.");
        if (vs) vkDestroyShaderModule(device, vs, NULL);
        if (fs) vkDestroyShaderModule(device, fs, NULL);
        return false;
    }

    VkPushConstantRange pcr = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, 32 };   /* top + bottom, vec4 each */
    VkPipelineLayoutCreateInfo li = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(device, &li, NULL, &g_bg_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vs, NULL);
        vkDestroyShaderModule(device, fs, NULL);
        g_bg_layout = VK_NULL_HANDLE;
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT,   vs, "main", NULL },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", NULL }
    };

    VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };   /* none: generated in the shader */

    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1; vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.0f;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = get_msaa_sample_count();   /* must equal the offscreen pass */

    /* The offscreen pass has a depth attachment, but the background neither tests nor writes it. */
    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable  = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cba = {0};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1; cb.pAttachments = &cba;

    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dsi = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dsi.dynamicStateCount = 2; dsi.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pi.stageCount = 2;              pi.pStages = stages;
    pi.pVertexInputState = &vi;     pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;        pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;     pi.pDepthStencilState = &ds;
    pi.pColorBlendState = &cb;      pi.pDynamicState = &dsi;
    pi.layout = g_bg_layout;        pi.renderPass = renderpass;  pi.subpass = 0;

    VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, NULL, &g_bg_pipeline);

    vkDestroyShaderModule(device, vs, NULL);
    vkDestroyShaderModule(device, fs, NULL);

    if (res != VK_SUCCESS) {
        phoenix_error("SCENE3D", "vkCreateGraphicsPipelines failed for the background gradient (%d)", (int)res);
        vkDestroyPipelineLayout(device, g_bg_layout, NULL);
        g_bg_layout = VK_NULL_HANDLE;
        g_bg_pipeline = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

void scene_look_draw_background(VkCommandBuffer cmd)
{
    if (g_bg_pipeline == VK_NULL_HANDLE) return;   /* shaders missing: the offscreen clear colour shows instead */

    float pc[8] = {
        g_scene_bg.top[0],    g_scene_bg.top[1],    g_scene_bg.top[2],    1.0f,
        g_scene_bg.bottom[0], g_scene_bg.bottom[1], g_scene_bg.bottom[2], 1.0f
    };
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_bg_pipeline);
    vkCmdPushConstants(cmd, g_bg_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void scene_look_shutdown(VkDevice device)
{
    if (!device) return;
    if (g_bg_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, g_bg_pipeline, NULL);       g_bg_pipeline = VK_NULL_HANDLE; }
    if (g_bg_layout   != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, g_bg_layout, NULL);   g_bg_layout   = VK_NULL_HANDLE; }
}