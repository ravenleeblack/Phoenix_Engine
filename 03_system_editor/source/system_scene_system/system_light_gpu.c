#include "system_globals.h"

light_gpu_state g_light_gpu = {0};

bool create_light_gpu_layout(VkDevice device, VkPhysicalDevice physical_device)
{
    if (g_light_gpu.layout != VK_NULL_HANDLE && g_light_gpu.device == device) {
        return true;
    }

    VkDescriptorSetLayoutBinding binding = {0};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    li.bindingCount = 1;
    li.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device, &li, NULL, &g_light_gpu.layout) != VK_SUCCESS) {
        g_light_gpu.layout = VK_NULL_HANDLE;
        phoenix_error("LIGHT", "%s", "Failed to create the light descriptor set layout.");
        return false;
    }

    VkDescriptorPoolSize size = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 };
    VkDescriptorPoolCreateInfo pi = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pi.maxSets = 1;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &size;
    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = (VkDeviceSize)light_gpu_slot_count * light_gpu_slot_size;
    bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    bool ok = vkCreateDescriptorPool(device, &pi, NULL, &g_light_gpu.pool) == VK_SUCCESS;
    ok = ok && vkCreateBuffer(device, &bi, NULL, &g_light_gpu.buffer) == VK_SUCCESS;
    if (ok) {
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, g_light_gpu.buffer, &req);
        VkPhysicalDeviceMemoryProperties props;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &props);
        const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        int type_index = -1;
        for (uint32_t i = 0; i < props.memoryTypeCount && type_index < 0; i++) {
            if ((req.memoryTypeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & want) == want) {
                type_index = (int)i;
            }
        }
        VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = (uint32_t)type_index;
        ok = type_index >= 0 && vkAllocateMemory(device, &ai, NULL, &g_light_gpu.memory) == VK_SUCCESS;
        ok = ok && vkBindBufferMemory(device, g_light_gpu.buffer, g_light_gpu.memory, 0) == VK_SUCCESS;
        void* mapped = NULL;
        ok = ok && vkMapMemory(device, g_light_gpu.memory, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS;
        g_light_gpu.mapped = (uint8_t*)mapped;
    }
    if (ok) {
        VkDescriptorSetAllocateInfo sa = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        sa.descriptorPool = g_light_gpu.pool;
        sa.descriptorSetCount = 1;
        sa.pSetLayouts = &g_light_gpu.layout;
        ok = vkAllocateDescriptorSets(device, &sa, &g_light_gpu.set) == VK_SUCCESS;
    }
    if (!ok) {
        g_light_gpu.device = device;
        destroy_light_gpu(device);
        phoenix_error("LIGHT", "%s", "Failed to create the light uniform buffer.");
        return false;
    }

    VkDescriptorBufferInfo info = { g_light_gpu.buffer, 0, light_gpu_slot_size };
    VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet = g_light_gpu.set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    write.pBufferInfo = &info;
    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

    g_light_gpu.device = device;
    g_light_gpu.next_slot = 0;
    return true;
}

VkDescriptorSetLayout get_light_gpu_layout(void)
{
    return g_light_gpu.layout;
}

/* world space -> view space (rotation only, or with translation when is_point) */
void convert_to_view_space(const editor_mat4* view, const float v[3], bool is_point, float out[3])
{
    for (int r = 0; r < 3; r++) {
        out[r] = view->mesh[0][r] * v[0] + view->mesh[1][r] * v[1] + view->mesh[2][r] * v[2];
        if (is_point) {
            out[r] += view->mesh[3][r];
        }
    }
}

void normalize_vector3(float v[3])
{
    const float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 1e-6f) {
        v[0] /= len; v[1] /= len; v[2] /= len;
    }
}

void bind_light_gpu_set(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, const editor_mat4* view)
{
    if (g_light_gpu.set == VK_NULL_HANDLE || g_light_gpu.mapped == NULL || view == NULL) {
        return;
    }

    float block[light_gpu_block_floats];
    memset(block, 0, sizeof(block));

    const float world_up[3] = { 0.0f, 1.0f, 0.0f };
    float up_view[3];
    convert_to_view_space(view, world_up, false, up_view);
    block[1] = up_view[0]; block[2] = up_view[1]; block[3] = up_view[2];

    int count = 0;
    if (g_light.enabled) {
        const editor_vec3 unit = { 1.0f, 1.0f, 1.0f };
        for (int i = 0; i < editor_light_count; i++) {
            const light_object* lo = &editor_lights[i];
            if (!lo->enabled) {
                continue;
            }
            const system_scene_object* obj = find_scene_object_by_id(lo->object_id);
            if (!obj) {
                continue;
            }
            if (lo->kind == light_kind_hemisphere) {
                for (int k = 0; k < 3; k++) {
                    block[4 + k] = lo->color[k] * lo->intensity;
                    block[8 + k] = lo->ground_color[k] * lo->intensity;
                }
                block[7] = 1.0f;
                continue;
            }
            if (lo->kind == light_kind_directional && i == editor_light_driver) {
                continue;                                       /* this one is already the sun */
            }
            if (count >= light_gpu_max_lights) {
                continue;
            }

            const editor_mat4 model = system_math_model_matrix(obj->position, obj->rotation, unit);
            float travel[3] = { -model.mesh[2][0], -model.mesh[2][1], -model.mesh[2][2] };   /* local -Z: where the light points */
            normalize_vector3(travel);
            float travel_view[3];
            convert_to_view_space(view, travel, false, travel_view);

            float* dst = &block[12 + count * 16];
            const float world_pos[3] = { obj->position.x, obj->position.y, obj->position.z };
            float pos_view[3];
            convert_to_view_space(view, world_pos, true, pos_view);

            dst[0] = pos_view[0]; dst[1] = pos_view[1]; dst[2] = pos_view[2];
            dst[3] = lo->range;
            float tinted[3] = { lo->color[0] * lo->intensity, lo->color[1] * lo->intensity, lo->color[2] * lo->intensity };
            apply_light_warmth(tinted, lo->warmth);
            dst[4] = tinted[0];
            dst[5] = tinted[1];
            dst[6] = tinted[2];
            dst[7] = (float)lo->kind;                           /* the shader knows 0 directional, 1 point, 2 spot, 3 area */
            if (lo->kind == light_kind_point) {
                dst[7] = (float)get_point_light_shader_kind();
            } else if (lo->kind == light_kind_spot) {
                dst[7] = (float)get_spot_light_shader_kind();
            } else if (lo->kind == light_kind_area) {
                dst[7] = (float)get_area_light_shader_kind();
            }
            if (lo->kind == light_kind_directional || lo->kind == light_kind_area) {
                dst[8] = -travel_view[0]; dst[9] = -travel_view[1]; dst[10] = -travel_view[2];
            } else {
                dst[8] = travel_view[0]; dst[9] = travel_view[1]; dst[10] = travel_view[2];
                const float outer = lo->cone_deg * 0.01745329252f;
                float soft = lo->softness;
                if (soft < 0.02f) {
                    soft = 0.02f;
                }
                if (soft > 1.0f) {
                    soft = 1.0f;
                }
                dst[11] = cosf(outer);
                dst[12] = cosf(outer * (1.0f - soft));
            }
            count++;
        }
    }
    block[0] = (float)count;

    const uint32_t slot = g_light_gpu.next_slot % light_gpu_slot_count;
    g_light_gpu.next_slot++;
    const uint32_t offset = slot * light_gpu_slot_size;
    memcpy(g_light_gpu.mapped + offset, block, sizeof(block));
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 1, 1, &g_light_gpu.set, 1, &offset);
}

void destroy_light_gpu(VkDevice device)
{
    if (!device || g_light_gpu.device == VK_NULL_HANDLE) {
        return;
    }
    if (g_light_gpu.mapped) {
        vkUnmapMemory(device, g_light_gpu.memory);
    }
    if (g_light_gpu.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, g_light_gpu.buffer, NULL);
    }
    if (g_light_gpu.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, g_light_gpu.memory, NULL);
    }
    if (g_light_gpu.pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, g_light_gpu.pool, NULL);
    }
    if (g_light_gpu.layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, g_light_gpu.layout, NULL);
    }
    memset(&g_light_gpu, 0, sizeof(g_light_gpu));
}
