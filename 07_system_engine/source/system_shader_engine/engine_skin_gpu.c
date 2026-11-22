#include "engine_globals.h"

uint32_t get_object_skin_id(uint32_t object_id)
{
    (void)object_id;
    return 0;
}

void texshop_build_skin_gpu(uint32_t skin_id)
{
    (void)skin_id;
}

extern int find_memory_index(unsigned int type_filter, unsigned int property_flags);   /* system_vulkan_backend.c */

/* std140 image of mesh_frag.glsl `Skin`:  uvec4 head; struct { vec4 color; vec4 params; } layers[16]; vec4 surface;  = 16 + 16 * 32 + 16 = 544 bytes
   surface = specular, gloss, height scale, ambient */
#define SKIN_UBO_FLOATS (4 + SKIN_GPU_MAX_LAYERS * 8 + 4)
#define SKIN_UBO_SIZE   ((VkDeviceSize)(SKIN_UBO_FLOATS * sizeof(float)))

#define SKIN_CACHE_MAX   48     /* 32 skins + the empty one + headroom */
#define SKIN_RETIRE_MAX  96
#define SKIN_RETIRE_FRAMES 3    /* same rule as EM_RETIRE_FRAMES in system_edit_mesh.c: >= frames in flight + 1 */
#define SKIN_POOL_SETS   (SKIN_CACHE_MAX + SKIN_RETIRE_MAX)

typedef struct { VkImage image; VkDeviceMemory memory; VkImageView view; } skin_texture;

typedef struct {
    VkBuffer        buffer;
    VkDeviceMemory  memory;
    VkDescriptorSet set;
    skin_texture    texture;          /* the skin's own base color map, when it has one */
    bool            owns_texture;
    skin_texture    aux;              /* packed specular / height / ambient maps, when the skin has any */
    bool            owns_aux;
    skin_texture    normal;           /* normal map, when the skin has one */
    bool            owns_normal;
} skin_gpu_block;

typedef struct { uint32_t id; bool used; skin_gpu_block block; } skin_cache_entry;
typedef struct { skin_gpu_block block; int frames; bool used; } skin_retired_entry;

static VkDevice               s_device = VK_NULL_HANDLE;
static VkDescriptorSetLayout  s_layout = VK_NULL_HANDLE;
static VkDescriptorPool       s_pool   = VK_NULL_HANDLE;
static VkSampler              s_sampler = VK_NULL_HANDLE;       /* one sampler for every skin map */
static skin_texture           s_white;                          /* 1x1 white: bound by every skin without a map */
static skin_gpu_block         s_empty;                          /* shared grey set, skin id 0 */
static skin_cache_entry       s_cache[SKIN_CACHE_MAX];
static skin_retired_entry     s_retired[SKIN_RETIRE_MAX];

/* ------------------------------------------------------------------------- */
/* textures                                                                  */
/* ------------------------------------------------------------------------- */
static void skin_destroy_texture(skin_texture* t)
{
    if (!s_device) return;
    if (t->view   != VK_NULL_HANDLE) vkDestroyImageView(s_device, t->view, NULL);
    if (t->image  != VK_NULL_HANDLE) vkDestroyImage(s_device, t->image, NULL);
    if (t->memory != VK_NULL_HANDLE) vkFreeMemory(s_device, t->memory, NULL);
    memset(t, 0, sizeof(*t));
}

/* RGBA8 pixels -> a sampled image, uploaded right now (staging buffer + one time command buffer, then wait). runs at load time, not per frame. */
static bool skin_create_texture(const uint8_t* rgba, uint32_t width, uint32_t height, skin_texture* out)
{
    memset(out, 0, sizeof(*out));
    if (!s_device || !rgba || width == 0 || height == 0) return false;
    const VkDeviceSize bytes = (VkDeviceSize)width * (VkDeviceSize)height * 4u;

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = bytes;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(s_device, &bi, NULL, &staging) != VK_SUCCESS) return false;
    VkMemoryRequirements breq;
    vkGetBufferMemoryRequirements(s_device, staging, &breq);
    int type_index = find_memory_index(breq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkMemoryAllocateInfo bai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    bai.allocationSize = breq.size;
    bai.memoryTypeIndex = (uint32_t)type_index;
    if (type_index < 0 || vkAllocateMemory(s_device, &bai, NULL, &staging_memory) != VK_SUCCESS) {
        vkDestroyBuffer(s_device, staging, NULL);
        return false;
    }
    vkBindBufferMemory(s_device, staging, staging_memory, 0);
    void* mapped = NULL;
    if (vkMapMemory(s_device, staging_memory, 0, bytes, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(s_device, staging, NULL);
        vkFreeMemory(s_device, staging_memory, NULL);
        return false;
    }
    memcpy(mapped, rgba, (size_t)bytes);
    vkUnmapMemory(s_device, staging_memory);

    bool ok = false;
    VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent.width = width;
    ii.extent.height = height;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(s_device, &ii, NULL, &out->image) == VK_SUCCESS) {
        VkMemoryRequirements ireq;
        vkGetImageMemoryRequirements(s_device, out->image, &ireq);
        int image_type = find_memory_index(ireq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkMemoryAllocateInfo iai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        iai.allocationSize = ireq.size;
        iai.memoryTypeIndex = (uint32_t)image_type;
        if (image_type >= 0 && vkAllocateMemory(s_device, &iai, NULL, &out->memory) == VK_SUCCESS) {
            vkBindImageMemory(s_device, out->image, out->memory, 0);

            VkCommandBufferAllocateInfo ca = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            ca.commandPool = render_context.command_pool;
            ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ca.commandBufferCount = 1;
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            if (vkAllocateCommandBuffers(s_device, &ca, &cmd) == VK_SUCCESS) {
                VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
                begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cmd, &begin);

                VkImageMemoryBarrier to_copy = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                to_copy.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                to_copy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                to_copy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                to_copy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                to_copy.image = out->image;
                to_copy.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                to_copy.subresourceRange.levelCount = 1;
                to_copy.subresourceRange.layerCount = 1;
                to_copy.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &to_copy);

                VkBufferImageCopy region = {0};
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.layerCount = 1;
                region.imageExtent.width = width;
                region.imageExtent.height = height;
                region.imageExtent.depth = 1;
                vkCmdCopyBufferToImage(cmd, staging, out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

                VkImageMemoryBarrier to_read = to_copy;
                to_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                to_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                to_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                to_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &to_read);
                vkEndCommandBuffer(cmd);

                VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
                submit.commandBufferCount = 1;
                submit.pCommandBuffers = &cmd;
                if (vkQueueSubmit(render_context.graphics_queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS) {
                    vkQueueWaitIdle(render_context.graphics_queue);
                    ok = true;
                }
                vkFreeCommandBuffers(s_device, render_context.command_pool, 1, &cmd);
            }
        }
    }

    vkDestroyBuffer(s_device, staging, NULL);
    vkFreeMemory(s_device, staging_memory, NULL);

    if (ok) {
        VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vi.image = out->image;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = VK_FORMAT_R8G8B8A8_UNORM;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        ok = vkCreateImageView(s_device, &vi, NULL, &out->view) == VK_SUCCESS;
    }
    if (!ok) {
        skin_destroy_texture(out);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/* blocks                                                                    */
/* ------------------------------------------------------------------------- */
static void skin_destroy_block(skin_gpu_block* b)
{
    if (!s_device) return;
    if (b->set    != VK_NULL_HANDLE && s_pool != VK_NULL_HANDLE) vkFreeDescriptorSets(s_device, s_pool, 1, &b->set);
    if (b->buffer != VK_NULL_HANDLE) vkDestroyBuffer(s_device, b->buffer, NULL);
    if (b->memory != VK_NULL_HANDLE) vkFreeMemory(s_device, b->memory, NULL);
    if (b->owns_texture) skin_destroy_texture(&b->texture);
    if (b->owns_aux) skin_destroy_texture(&b->aux);
    if (b->owns_normal) skin_destroy_texture(&b->normal);
    memset(b, 0, sizeof(*b));
}

static void skin_pack(const skin_gpu_data* data, bool use_vertex_colour, bool has_map, float out[SKIN_UBO_FLOATS])
{
    memset(out, 0, SKIN_UBO_FLOATS * sizeof(float));
    uint32_t head[4] = { 0, use_vertex_colour ? 1u : 0u, has_map ? 1u : 0u, data->aux_flags };

    uint32_t face_type = data->layer_count;
    if (face_type > SKIN_GPU_MAX_LAYERS) face_type = SKIN_GPU_MAX_LAYERS;
    head[0] = face_type;
    memcpy(&out[0], head, sizeof(head));

    for (uint32_t i = 0; i < face_type; i++) {
        const skin_gpu_layer* l = &data->layers[i];
        float* dst = &out[4 + i * 8];
        dst[0] = l->color[0]; dst[1] = l->color[1]; dst[2] = l->color[2];
        dst[3] = l->color[3] * l->opacity;                       /* colour.a already includes the layer opacity */
        dst[4] = (float)l->blend;
        dst[5] = (float)(l->channel_mask & 31u);                 /* five outputs, exact in a float */
        dst[6] = l->triplanar ? 1.0f : 0.0f;
        dst[7] = 0.0f;
    }

    float* surface = &out[4 + SKIN_GPU_MAX_LAYERS * 8];
    surface[0] = data->specular;
    surface[1] = data->gloss;
    surface[2] = data->height_scale;
    surface[3] = data->ambient;
}

/* own_texture / own_aux: the skin's maps (copied into the block, which then frees them). NULL = the shared white texture. */
static bool skin_build_block(const float* ubo, const skin_texture* own_texture, const skin_texture* own_aux, const skin_texture* own_normal, skin_gpu_block* out)
{
    memset(out, 0, sizeof(*out));
    if (own_normal) {
        out->normal = *own_normal;
        out->owns_normal = true;
    }
    if (own_texture) {
        out->texture = *own_texture;
        out->owns_texture = true;
    }
    if (own_aux) {
        out->aux = *own_aux;
        out->owns_aux = true;
    }

    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = SKIN_UBO_SIZE;
    bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(s_device, &bi, NULL, &out->buffer) != VK_SUCCESS) { out->buffer = VK_NULL_HANDLE; return false; }

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(s_device, out->buffer, &req);
    const int type_index = find_memory_index(req.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (type_index < 0) { skin_destroy_block(out); return false; }

    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = (uint32_t)type_index;
    if (vkAllocateMemory(s_device, &ai, NULL, &out->memory) != VK_SUCCESS) { out->memory = VK_NULL_HANDLE; skin_destroy_block(out); return false; }
    vkBindBufferMemory(s_device, out->buffer, out->memory, 0);

    void* mapped = NULL;
    if (vkMapMemory(s_device, out->memory, 0, SKIN_UBO_SIZE, 0, &mapped) != VK_SUCCESS) { skin_destroy_block(out); return false; }
    memcpy(mapped, ubo, (size_t)SKIN_UBO_SIZE);
    vkUnmapMemory(s_device, out->memory);

    VkDescriptorSetAllocateInfo sa = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    sa.descriptorPool = s_pool;
    sa.descriptorSetCount = 1;
    sa.pSetLayouts = &s_layout;
    VkResult res = vkAllocateDescriptorSets(s_device, &sa, &out->set);
    if (res != VK_SUCCESS) {
        out->set = VK_NULL_HANDLE;
        phoenix_error("SKIN", "vkAllocateDescriptorSets failed (%d).", (int)res);
        skin_destroy_block(out);
        return false;
    }

    VkDescriptorBufferInfo info = { out->buffer, 0, SKIN_UBO_SIZE };
    const skin_texture* bound = &s_white;
    if (out->owns_texture) {
        bound = &out->texture;
    }
    VkDescriptorImageInfo image_info = { s_sampler, bound->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    const skin_texture* aux_bound = &s_white;
    if (out->owns_aux) {
        aux_bound = &out->aux;
    }
    VkDescriptorImageInfo aux_info = { s_sampler, aux_bound->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    const skin_texture* normal_bound = &s_white;
    if (out->owns_normal) {
        normal_bound = &out->normal;
    }
    VkDescriptorImageInfo normal_info = { s_sampler, normal_bound->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet writes[4];
    memset(writes, 0, sizeof(writes));
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = out->set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &info;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = out->set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &image_info;
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = out->set;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &aux_info;
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = out->set;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].pImageInfo = &normal_info;
    vkUpdateDescriptorSets(s_device, 4, writes, 0, NULL);
    return true;
}

/* ------------------------------------------------------------------------- */
/* 3 frame retire (same cadence as em_gpu_retire)                            */
/* ------------------------------------------------------------------------- */
void skin_gpu_flush(void)
{
    if (!s_device) return;
    vkDeviceWaitIdle(s_device);
    for (int i = 0; i < SKIN_RETIRE_MAX; i++)
        if (s_retired[i].used) { skin_destroy_block(&s_retired[i].block); memset(&s_retired[i], 0, sizeof(s_retired[i])); }
}

void skin_gpu_tick(void)
{
    for (int i = 0; i < SKIN_RETIRE_MAX; i++) {
        if (s_retired[i].used && --s_retired[i].frames <= 0) {
            skin_destroy_block(&s_retired[i].block);
            memset(&s_retired[i], 0, sizeof(s_retired[i]));
        }
    }
}

static void skin_retire(const skin_gpu_block* b)
{
    if (!b->buffer && !b->memory && !b->set) return;
    int slot = -1;
    for (int i = 0; i < SKIN_RETIRE_MAX; i++) if (!s_retired[i].used) { slot = i; break; }
    if (slot < 0) { skin_gpu_flush(); slot = 0; }                /* queue full: stall once */
    s_retired[slot].block  = *b;
    s_retired[slot].frames = SKIN_RETIRE_FRAMES;
    s_retired[slot].used   = true;
}

/* ------------------------------------------------------------------------- */
/* layout, pool, empty set                                                   */
/* ------------------------------------------------------------------------- */
bool skin_gpu_create_layout(VkDevice device)
{
    if (s_layout != VK_NULL_HANDLE && s_device == device) return true;
    if (s_layout != VK_NULL_HANDLE) skin_gpu_shutdown(s_device);   /* device changed (should not happen) */
    s_device = device;

    VkDescriptorSetLayoutBinding bindings[4];
    memset(bindings, 0, sizeof(bindings));
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;                                        /* the skin's base color map (mesh_frag.glsl skin_map) */
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].binding = 2;                                        /* packed specular / height / ambient maps (mesh_frag.glsl skin_aux) */
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].binding = 3;                                        /* normal map (mesh_frag.glsl skin_normal) */
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    li.bindingCount = 4;
    li.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &li, NULL, &s_layout) != VK_SUCCESS) {
        s_layout = VK_NULL_HANDLE; s_device = VK_NULL_HANDLE;
        phoenix_error("SKIN", "Failed to create the skin descriptor set layout.");
        return false;
    }

    VkDescriptorPoolSize sizes[2] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SKIN_POOL_SETS },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SKIN_POOL_SETS * 3 }
    };
    VkDescriptorPoolCreateInfo pi = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;   /* sets are freed one by one when a skin is retired */
    pi.maxSets = SKIN_POOL_SETS;
    pi.poolSizeCount = 2;
    pi.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(device, &pi, NULL, &s_pool) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, s_layout, NULL);
        s_layout = VK_NULL_HANDLE; s_pool = VK_NULL_HANDLE; s_device = VK_NULL_HANDLE;
        phoenix_error("SKIN", "Failed to create the skin descriptor pool.");
        return false;
    }

    VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    const uint8_t white_pixel[4] = { 255, 255, 255, 255 };
    if (vkCreateSampler(device, &sci, NULL, &s_sampler) != VK_SUCCESS || !skin_create_texture(white_pixel, 1, 1, &s_white)) {
        phoenix_error("SKIN", "Failed to create the skin sampler or the white fallback texture.");
        if (s_sampler != VK_NULL_HANDLE) vkDestroySampler(device, s_sampler, NULL);
        vkDestroyDescriptorPool(device, s_pool, NULL);
        vkDestroyDescriptorSetLayout(device, s_layout, NULL);
        s_sampler = VK_NULL_HANDLE; s_pool = VK_NULL_HANDLE; s_layout = VK_NULL_HANDLE; s_device = VK_NULL_HANDLE;
        return false;
    }

    /* The shared "no skin" set: one neutral grey layer, and head.y = 1 so mesh_frag.glsl keeps the vertex colour
       (cube and terrain already carry the empty-skin grey there, plus baked light and hand paint). */
    skin_gpu_data grey;
    memset(&grey, 0, sizeof(grey));
    grey.layer_count = 1;
    grey.layers[0].color[0] = grey.layers[0].color[1] = grey.layers[0].color[2] = 0.55f;
    grey.layers[0].color[3] = 1.0f;
    grey.layers[0].opacity = 1.0f;
    grey.layers[0].channel_mask = 1u;
    float ubo[SKIN_UBO_FLOATS];
    skin_pack(&grey, true, false, ubo);
    if (!skin_build_block(ubo, NULL, NULL, NULL, &s_empty)) {
        phoenix_error("SKIN", "Failed to build the shared empty skin set.");
        skin_destroy_texture(&s_white);
        vkDestroySampler(device, s_sampler, NULL);
        vkDestroyDescriptorPool(device, s_pool, NULL);
        vkDestroyDescriptorSetLayout(device, s_layout, NULL);
        s_sampler = VK_NULL_HANDLE; s_pool = VK_NULL_HANDLE; s_layout = VK_NULL_HANDLE; s_device = VK_NULL_HANDLE;
        return false;
    }
    phoenix_info("SKIN", "Skin descriptor layout, pool (%d sets) and shared empty set created.", (int)SKIN_POOL_SETS);
    return true;
}

VkDescriptorSetLayout skin_gpu_get_layout(void) { return s_layout; }
VkDescriptorSet skin_gpu_get_empty_set(void)    { return s_empty.set; }

/* ------------------------------------------------------------------------- */
/* per skin cache                                                            */
/* ------------------------------------------------------------------------- */
static skin_cache_entry* skin_find(uint32_t id)
{
    for (int i = 0; i < SKIN_CACHE_MAX; i++) if (s_cache[i].used && s_cache[i].id == id) return &s_cache[i];
    return NULL;
}

bool skin_gpu_has(uint32_t skin_id) { return skin_id != 0 && skin_find(skin_id) != NULL; }

VkDescriptorSet skin_gpu_get_set(uint32_t skin_id)
{
    const skin_cache_entry* e = skin_id ? skin_find(skin_id) : NULL;
    return e ? e->block.set : s_empty.set;
}

bool skin_gpu_set(uint32_t skin_id, const skin_gpu_data* data)
{
    if (!s_device || s_layout == VK_NULL_HANDLE || skin_id == 0 || !data) return false;

    const bool wants_map = data->map_pixels != NULL && data->map_width > 0 && data->map_height > 0;
    const bool wants_aux = data->aux_pixels != NULL && data->aux_width > 0 && data->aux_height > 0;
    const bool wants_normal = data->normal_pixels != NULL && data->normal_width > 0 && data->normal_height > 0;

    skin_gpu_block fresh;
    bool built = false;
    bool has_map = false;
    for (int attempt = 0; attempt < 2 && !built; attempt++) {
        skin_texture map;
        memset(&map, 0, sizeof(map));
        has_map = wants_map && skin_create_texture(data->map_pixels, data->map_width, data->map_height, &map);

        float ubo[SKIN_UBO_FLOATS];
        skin_pack(data, false, has_map, ubo);

        skin_texture aux;
        memset(&aux, 0, sizeof(aux));
        const bool has_aux = wants_aux && skin_create_texture(data->aux_pixels, data->aux_width, data->aux_height, &aux);

        skin_texture normal;
        memset(&normal, 0, sizeof(normal));
        const bool has_normal = wants_normal && skin_create_texture(data->normal_pixels, data->normal_width, data->normal_height, &normal);

        const skin_texture* own = NULL;
        if (has_map) own = &map;
        const skin_texture* own_aux = NULL;
        if (has_aux) own_aux = &aux;
        const skin_texture* own_normal = NULL;
        if (has_normal) own_normal = &normal;
        built = skin_build_block(ubo, own, own_aux, own_normal, &fresh); /* a failed build frees the textures it was given */
        if (!built) skin_gpu_flush();                         /* pool may be full of retired sets: free them and retry once */
    }
    if (!built) return false;

    skin_cache_entry* e = skin_find(skin_id);
    if (!e) {
        for (int i = 0; i < SKIN_CACHE_MAX; i++) if (!s_cache[i].used) { e = &s_cache[i]; break; }
        if (!e) { skin_destroy_block(&fresh); phoenix_error("SKIN", "Skin GPU cache is full."); return false; }
        e->used = true;
        e->id = skin_id;
    } else {
        skin_retire(&e->block);                                   /* the old buffer may still be in a recorded frame */
    }
    e->block = fresh;
    phoenix_info("SKIN", "Skin %u GPU data built (%u layer%s).", (unsigned)skin_id, (unsigned)data->layer_count, data->layer_count == 1 ? "" : "s");
    return true;
}

void skin_gpu_remove(uint32_t skin_id)
{
    skin_cache_entry* e = skin_id ? skin_find(skin_id) : NULL;
    if (!e) return;
    skin_retire(&e->block);
    memset(e, 0, sizeof(*e));
}

void skin_gpu_shutdown(VkDevice device)
{
    if (!device || s_device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device);
    for (int i = 0; i < SKIN_RETIRE_MAX; i++) if (s_retired[i].used) skin_destroy_block(&s_retired[i].block);
    for (int i = 0; i < SKIN_CACHE_MAX; i++)  if (s_cache[i].used)   skin_destroy_block(&s_cache[i].block);
    skin_destroy_block(&s_empty);
    skin_destroy_texture(&s_white);
    if (s_sampler != VK_NULL_HANDLE) vkDestroySampler(device, s_sampler, NULL);
    if (s_pool   != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, s_pool, NULL);
    if (s_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, s_layout, NULL);
    memset(s_retired, 0, sizeof(s_retired));
    memset(s_cache,   0, sizeof(s_cache));
    memset(&s_empty,  0, sizeof(s_empty));
    s_sampler = VK_NULL_HANDLE; s_pool = VK_NULL_HANDLE; s_layout = VK_NULL_HANDLE; s_device = VK_NULL_HANDLE;
}