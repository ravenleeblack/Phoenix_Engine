#include "engine_globals.h"

/* ------------------------------------------------------------------------- */
/* Shader loading                                                             */
/*                                                                            */
/* Compiled .spv files are looked up in the working directory first, then in  */
/* a few shaders/ folders, so it no longer matters whether you launch from    */
/* Visual Studio, the build folder or the exe folder. When the matching .glsl */
/* is NEWER than the .spv, a warning says the shader was not recompiled.      */
/* Run make after editing any .glsl.                           */
/* ------------------------------------------------------------------------- */
static const char* const k_shader_dirs[] = { "", "shaders/", "../shaders/", "../../shaders/", "../" };
#define SHADER_DIR_COUNT ((int)(sizeof(k_shader_dirs) / sizeof(k_shader_dirs[0])))

static uint32_t* read_spirv_file(const char* path, size_t* out_size)
{
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size <= 0 || (file_size % 4) != 0) {
        phoenix_error("VULKAN", "Shader binary has an invalid size (%ld bytes): %s", file_size, path);
        fclose(file);
        return NULL;
    }
    rewind(file);

    uint32_t* buffer = (uint32_t*)malloc((size_t)file_size);
    if (!buffer) {
        phoenix_error("VULKAN", "%s", "Host side allocation failed while reading a shader binary.");
        fclose(file);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);

    if (read_bytes != (size_t)file_size) {
        phoenix_error("VULKAN", "Truncated read on shader binary: %s", path);
        free(buffer);
        return NULL;
    }
    if (buffer[0] != 0x07230203u) {   /* SPIR-V magic number */
        phoenix_error("VULKAN", "Not a SPIR-V file (bad magic number): %s", path);
        free(buffer);
        return NULL;
    }

    *out_size = (size_t)file_size;
    return buffer;
}

/* Last-write time of a file via Win32. Returns false if the file does not exist. */
static bool get_file_write_time(const char* path, FILETIME* out)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return false;
    *out = data.ftLastWriteTime;
    return true;
}

/* Warn when <name>.glsl is newer than the compiled <name>.spv that was just loaded. */
static void warn_if_shader_stale(const char* spv_path, const char* name)
{
    FILETIME spv_time;
    if (!get_file_write_time(spv_path, &spv_time)) return;

    char base[128];
    strncpy(base, name, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char* dot = strrchr(base, '.');
    if (dot) *dot = '\0';

    for (int i = 0; i < SHADER_DIR_COUNT; i++) {
        char glsl_path[256];
        snprintf(glsl_path, sizeof(glsl_path), "%s%s.glsl", k_shader_dirs[i], base);
        FILETIME glsl_time;
        if (get_file_write_time(glsl_path, &glsl_time)) {
            if (CompareFileTime(&glsl_time, &spv_time) > 0) {
                phoenix_warn("VULKAN", "STALE SHADER: %s is newer than %s. Run make.", glsl_path, spv_path);
            }
            return;
        }
    }
}

VkShaderModule create_shader_module(VkDevice device, const uint32_t* code, size_t code_size, const VkAllocationCallbacks* allocator)
{
    VkShaderModuleCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code_size;
    create_info.pCode = code;

    VkShaderModule shader_module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &create_info, allocator, &shader_module) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Driver failed to instantiate incoming SPIR-V module.");
        return VK_NULL_HANDLE;
    }
    return shader_module;
}

/* The ONE place shaders get loaded. Everything else calls this. */
VkShaderModule load_shader_binary_module(VkDevice device, const char* filename)
{
    for (int i = 0; i < SHADER_DIR_COUNT; i++) {
        char path[256];
        snprintf(path, sizeof(path), "%s%s", k_shader_dirs[i], filename);

        size_t size = 0;
        uint32_t* code = read_spirv_file(path, &size);
        if (!code) continue;

        warn_if_shader_stale(path, filename);

        VkShaderModule module = create_shader_module(device, code, size, render_context.allocator);
        free(code);
        return module;
    }

    phoenix_error("VULKAN", "Shader not found: %s (searched the working dir and shaders/ folders). Run make.", filename);
    return VK_NULL_HANDLE;
}

static void destroy_module_if_valid(VkDevice device, VkShaderModule mesh, const VkAllocationCallbacks* allocator)
{
    if (mesh != VK_NULL_HANDLE) vkDestroyShaderModule(device, mesh, allocator);
}

/* ------------------------------------------------------------------------- */
/* Pipelines                                                                  */
/* ------------------------------------------------------------------------- */
bool create_vulkan_graphics_pipelines(vulkan_system_context* context, VkRenderPass renderpass)
{
    VkDevice device = context->device.logical_device;
    const VkAllocationCallbacks* allocator = context->allocator;

    bool ok = false;
    VkShaderModule ui_vert_mod   = VK_NULL_HANDLE;
    VkShaderModule ui_frag_mod   = VK_NULL_HANDLE;
    VkShaderModule grid_vert_mod = VK_NULL_HANDLE;
    VkShaderModule grid_frag_mod = VK_NULL_HANDLE;
    VkDescriptorSetLayout ui_desc_layouts = VK_NULL_HANDLE;

    /* The offscreen pass must exist first: grid + mesh + background pipelines are built against it. */
    if (render_context.offscreen_renderpass == VK_NULL_HANDLE) {
        phoenix_error("VULKAN", "%s", "Offscreen render pass must be created before the graphics pipelines (offscreen_create_renderpass).");
        return false;
    }

    /* Load every shader up front. Each missing one is logged by name. */
    ui_vert_mod   = load_shader_binary_module(device, "object_vert.spv");
    ui_frag_mod   = load_shader_binary_module(device, "object_frag.spv");
    grid_vert_mod = load_shader_binary_module(device, "scene_3d.spv");
    grid_frag_mod = load_shader_binary_module(device, "grid_frag.spv");

    if (ui_vert_mod == VK_NULL_HANDLE || ui_frag_mod == VK_NULL_HANDLE ||
        grid_vert_mod == VK_NULL_HANDLE || grid_frag_mod == VK_NULL_HANDLE) {
        phoenix_error("VULKAN", "%s", "One or more required shaders are missing. Run make so the .spv files are built into 01_bin.");
        goto cleanup;
    }

    /* ---- PIPELINE 1: THE 2D EDITOR UI ---- */
    VkPipelineShaderStageCreateInfo ui_stages[2];
    memset(ui_stages, 0, sizeof(ui_stages));
    ui_stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ui_stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    ui_stages[0].module = ui_vert_mod;
    ui_stages[0].pName  = "main";
    ui_stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ui_stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    ui_stages[1].module = ui_frag_mod;
    ui_stages[1].pName  = "main";

    VkDescriptorSetLayoutBinding sampler_binding = {0};
    sampler_binding.binding         = 0;
    sampler_binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_binding.descriptorCount = 1;
    sampler_binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo desc_info = {0};
    desc_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    desc_info.bindingCount = 1;
    desc_info.pBindings    = &sampler_binding;

    if (vkCreateDescriptorSetLayout(device, &desc_info, allocator, &ui_desc_layouts) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to create UI descriptor set layout.");
        ui_desc_layouts = VK_NULL_HANDLE;
        goto cleanup;
    }
    render_context.descriptor_layout = ui_desc_layouts;   /* the fallback descriptor set is allocated from it */

    /* push_quad_element pushes with VERTEX|FRAGMENT, so the range must cover both stages */
    VkPushConstantRange ui_push = {0};
    ui_push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ui_push.offset     = 0;
    ui_push.size       = 40;

    VkPipelineLayoutCreateInfo ui_layout_info = {0};
    ui_layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ui_layout_info.setLayoutCount         = 1;
    ui_layout_info.pSetLayouts            = &ui_desc_layouts;
    ui_layout_info.pushConstantRangeCount = 1;
    ui_layout_info.pPushConstantRanges    = &ui_push;

    pipeline_layout_ui = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &ui_layout_info, allocator, &pipeline_layout_ui) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to create UI pipeline layout.");
        pipeline_layout_ui = VK_NULL_HANDLE;
        goto cleanup;
    }

    /* ---- PIPELINE 2: THE 3D VIEWPORT GRID ---- */
    VkPipelineShaderStageCreateInfo grid_stages[2];
    memset(grid_stages, 0, sizeof(grid_stages));
    grid_stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    grid_stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    grid_stages[0].module = grid_vert_mod;
    grid_stages[0].pName  = "main";
    grid_stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    grid_stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    grid_stages[1].module = grid_frag_mod;
    grid_stages[1].pName  = "main";

    VkPushConstantRange grid_push = {0};
    grid_push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    grid_push.offset     = 0;
    grid_push.size       = sizeof(float) * 32;   /* view + projection = 128 bytes, matches scene_3d.glsl */

    VkPipelineLayoutCreateInfo grid_layout_info = {0};
    grid_layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    grid_layout_info.pushConstantRangeCount = 1;
    grid_layout_info.pPushConstantRanges    = &grid_push;

    pipeline_layout_grid = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &grid_layout_info, allocator, &pipeline_layout_grid) != VK_SUCCESS) {
        phoenix_error("VULKAN", "%s", "Failed to create Grid pipeline layout.");
        pipeline_layout_grid = VK_NULL_HANDLE;
        goto cleanup;
    }

    /* ---- shared fixed-function state ---- */
    VkPipelineVertexInputStateCreateInfo vertex_input = {0};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineViewportStateCreateInfo viewport_state = {0};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    VkPipelineInputAssemblyStateCreateInfo assembly_ui = {0};
    assembly_ui.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_ui.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineInputAssemblyStateCreateInfo assembly_grid = {0};
    assembly_grid.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly_grid.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_NONE;
    rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    /* grid + meshes draw in the OFFSCREEN pass: must equal its sample count */
    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = get_msaa_sample_count();

    /* the UI pass draws straight into the 1-sample swapchain image, no depth */
    VkPipelineMultisampleStateCreateInfo multisampling_ui = {0};
    multisampling_ui.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_ui.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo grid_depth = {0};
    grid_depth.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    grid_depth.depthTestEnable  = VK_TRUE;
    grid_depth.depthWriteEnable = VK_TRUE;
    grid_depth.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineDepthStencilStateCreateInfo no_depth = {0};
    no_depth.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    no_depth.depthTestEnable  = VK_FALSE;
    no_depth.depthWriteEnable = VK_FALSE;
    no_depth.depthCompareOp   = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState blend_attach = {0};
    blend_attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attach.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blending = {0};
    blending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1;
    blending.pAttachments    = &blend_attach;

    VkDynamicState dynamics[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {0};
    dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates    = dynamics;

    VkGraphicsPipelineCreateInfo pipe_ui = {0};
    pipe_ui.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_ui.stageCount          = 2;
    pipe_ui.pStages             = ui_stages;
    pipe_ui.pVertexInputState   = &vertex_input;
    pipe_ui.pInputAssemblyState = &assembly_ui;
    pipe_ui.pViewportState      = &viewport_state;
    pipe_ui.pRasterizationState = &rasterizer;
    pipe_ui.pMultisampleState   = &multisampling_ui;
    pipe_ui.pDepthStencilState  = &no_depth;
    pipe_ui.pColorBlendState    = &blending;
    pipe_ui.pDynamicState       = &dynamic_state;
    pipe_ui.layout              = pipeline_layout_ui;
    pipe_ui.renderPass          = renderpass;
    pipe_ui.subpass             = 0;

    VkGraphicsPipelineCreateInfo pipe_grid = {0};
    pipe_grid.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_grid.stageCount          = 2;
    pipe_grid.pStages             = grid_stages;
    pipe_grid.pVertexInputState   = &vertex_input;
    pipe_grid.pInputAssemblyState = &assembly_grid;
    pipe_grid.pViewportState      = &viewport_state;
    pipe_grid.pRasterizationState = &rasterizer;
    pipe_grid.pMultisampleState   = &multisampling;
    pipe_grid.pDepthStencilState  = &grid_depth;
    pipe_grid.pColorBlendState    = &blending;
    pipe_grid.pDynamicState       = &dynamic_state;
    pipe_grid.layout              = pipeline_layout_grid;
    pipe_grid.renderPass          = render_context.offscreen_renderpass;
    pipe_grid.subpass             = 0;

    VkResult ui_res   = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_ui,   allocator, &graphics_pipeline_ui);
    VkResult grid_res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipe_grid, allocator, &graphics_pipeline_grid);

    if (ui_res != VK_SUCCESS || grid_res != VK_SUCCESS) {
        phoenix_error("VULKAN", "Failed to compile pipelines. UI: %d, Grid: %d", (int)ui_res, (int)grid_res);
        goto cleanup;
    }

    /* Shader modules are no longer needed once the pipelines exist. */
    destroy_module_if_valid(device, ui_vert_mod,   allocator); ui_vert_mod   = VK_NULL_HANDLE;
    destroy_module_if_valid(device, ui_frag_mod,   allocator); ui_frag_mod   = VK_NULL_HANDLE;
    destroy_module_if_valid(device, grid_vert_mod, allocator); grid_vert_mod = VK_NULL_HANDLE;
    destroy_module_if_valid(device, grid_frag_mod, allocator); grid_frag_mod = VK_NULL_HANDLE;

    /* ---- PIPELINE 3 (+ line helpers + background gradient): drawn in the offscreen pass (MSAA + depth) ---- */
    ok = create_scene_object_pipeline(device, render_context.offscreen_renderpass, true);
    if (!ok) {
        phoenix_error("VULKAN", "%s", "Mesh pipeline failed (mesh_vert.spv / mesh_frag.spv). Run make.");
    }
    return ok;

cleanup:
    destroy_module_if_valid(device, ui_vert_mod,   allocator);
    destroy_module_if_valid(device, ui_frag_mod,   allocator);
    destroy_module_if_valid(device, grid_vert_mod, allocator);
    destroy_module_if_valid(device, grid_frag_mod, allocator);

    if (graphics_pipeline_grid != VK_NULL_HANDLE) { vkDestroyPipeline(device, graphics_pipeline_grid, allocator); graphics_pipeline_grid = VK_NULL_HANDLE; }
    if (graphics_pipeline_ui   != VK_NULL_HANDLE) { vkDestroyPipeline(device, graphics_pipeline_ui,   allocator); graphics_pipeline_ui   = VK_NULL_HANDLE; }
    if (pipeline_layout_grid != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, pipeline_layout_grid, allocator); pipeline_layout_grid = VK_NULL_HANDLE; }
    if (pipeline_layout_ui   != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, pipeline_layout_ui,   allocator); pipeline_layout_ui   = VK_NULL_HANDLE; }
    if (ui_desc_layouts      != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, ui_desc_layouts, allocator); }
    render_context.descriptor_layout = VK_NULL_HANDLE;
    return false;
}