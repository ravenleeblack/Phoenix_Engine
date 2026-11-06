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

/* std140 image of mesh_frag.glsl `Skin`:  uvec4 head; struct { vec4 color; vec4 params; } layers[16];  = 16 + 16 * 32 = 528 bytes */
#define SKIN_UBO_FLOATS (4 + SKIN_GPU_MAX_LAYERS * 8)
#define SKIN_UBO_SIZE   ((VkDeviceSize)(SKIN_UBO_FLOATS * sizeof(float)))

#define SKIN_CACHE_MAX   48     /* 32 skins + the empty one + headroom */
#define SKIN_RETIRE_MAX  96
#define SKIN_RETIRE_FRAMES 3    /* same rule as EM_RETIRE_FRAMES in system_edit_mesh.c: >= frames in flight + 1 */
#define SKIN_POOL_SETS   (SKIN_CACHE_MAX + SKIN_RETIRE_MAX)

typedef struct {
    VkBuffer        buffer;
    VkDeviceMemory  memory;
    VkDescriptorSet set;
} skin_gpu_block;

typedef struct { uint32_t id; bool used; skin_gpu_block block; } skin_cache_entry;
typedef struct { skin_gpu_block block; int frames; bool used; } skin_retired_entry;

static VkDevice               s_device = VK_NULL_HANDLE;
static VkDescriptorSetLayout  s_layout = VK_NULL_HANDLE;
static VkDescriptorPool       s_pool   = VK_NULL_HANDLE;
static skin_gpu_block         s_empty;                          /* shared grey set, skin id 0 */
static skin_cache_entry       s_cache[SKIN_CACHE_MAX];
static skin_retired_entry     s_retired[SKIN_RETIRE_MAX];

/* ------------------------------------------------------------------------- */
/* blocks                                                                    */
/* ------------------------------------------------------------------------- */
static void skin_destroy_block(skin_gpu_block* b)
{
    if (!s_device) return;
    if (b->set    != VK_NULL_HANDLE && s_pool != VK_NULL_HANDLE) vkFreeDescriptorSets(s_device, s_pool, 1, &b->set);
    if (b->buffer != VK_NULL_HANDLE) vkDestroyBuffer(s_device, b->buffer, NULL);
    if (b->memory != VK_NULL_HANDLE) vkFreeMemory(s_device, b->memory, NULL);
    memset(b, 0, sizeof(*b));
}

static void skin_pack(const skin_gpu_data* data, bool use_vertex_colour, float out[SKIN_UBO_FLOATS])
{
    memset(out, 0, SKIN_UBO_FLOATS * sizeof(float));
    uint32_t head[4] = { 0, use_vertex_colour ? 1u : 0u, 0, 0 };

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
}

static bool skin_build_block(const float* ubo, skin_gpu_block* out)
{
    memset(out, 0, sizeof(*out));

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
    VkWriteDescriptorSet w = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    w.dstSet = out->set;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo = &info;
    vkUpdateDescriptorSets(s_device, 1, &w, 0, NULL);
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

    VkDescriptorSetLayoutBinding binding = {0};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    li.bindingCount = 1;
    li.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device, &li, NULL, &s_layout) != VK_SUCCESS) {
        s_layout = VK_NULL_HANDLE; s_device = VK_NULL_HANDLE;
        phoenix_error("SKIN", "Failed to create the skin descriptor set layout.");
        return false;
    }

    VkDescriptorPoolSize size = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SKIN_POOL_SETS };
    VkDescriptorPoolCreateInfo pi = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;   /* sets are freed one by one when a skin is retired */
    pi.maxSets = SKIN_POOL_SETS;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &size;
    if (vkCreateDescriptorPool(device, &pi, NULL, &s_pool) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, s_layout, NULL);
        s_layout = VK_NULL_HANDLE; s_pool = VK_NULL_HANDLE; s_device = VK_NULL_HANDLE;
        phoenix_error("SKIN", "Failed to create the skin descriptor pool.");
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
    skin_pack(&grey, true, ubo);
    if (!skin_build_block(ubo, &s_empty)) {
        phoenix_error("SKIN", "Failed to build the shared empty skin set.");
        vkDestroyDescriptorPool(device, s_pool, NULL);
        vkDestroyDescriptorSetLayout(device, s_layout, NULL);
        s_pool = VK_NULL_HANDLE; s_layout = VK_NULL_HANDLE; s_device = VK_NULL_HANDLE;
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

    float ubo[SKIN_UBO_FLOATS];
    skin_pack(data, false, ubo);

    skin_gpu_block fresh;
    if (!skin_build_block(ubo, &fresh)) {
        skin_gpu_flush();                                         /* pool may be full of retired sets: free them and retry once */
        if (!skin_build_block(ubo, &fresh)) return false;
    }

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
    if (s_pool   != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, s_pool, NULL);
    if (s_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, s_layout, NULL);
    memset(s_retired, 0, sizeof(s_retired));
    memset(s_cache,   0, sizeof(s_cache));
    memset(&s_empty,  0, sizeof(s_empty));
    s_pool = VK_NULL_HANDLE; s_layout = VK_NULL_HANDLE; s_device = VK_NULL_HANDLE;
}