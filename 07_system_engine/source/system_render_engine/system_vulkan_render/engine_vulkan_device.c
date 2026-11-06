#include "engine_globals.h"



typedef struct vulkan_physical_device_requirements {
    bool graphics;
    bool present;
    bool compute;
    bool transfer;
    const char** device_extension_names;
    bool sampler_anisotropy;
    bool discrete_gpu;
} vulkan_physical_device_requirements;

typedef struct vulkan_physical_device_queue_family_info {
    int graphics_family_index;
    int present_family_index;
    int compute_family_index;
    int transfer_family_index;
} vulkan_physical_device_queue_family_info;

bool select_physical_device(vulkan_system_context* context);

bool physical_device_meets_requirements(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const VkPhysicalDeviceProperties* properties,
    const VkPhysicalDeviceFeatures* features,
    const vulkan_physical_device_requirements* requirements,
    vulkan_physical_device_queue_family_info* out_queue_family_info,
    vulkan_swapchain_support_info* out_swapchain_support);


/* read by system_scene_draw.c: true when the logical device was created with fillModeNonSolid */
bool g_device_fill_mode_non_solid = false;

bool create_vulkan_device(vulkan_system_context* context) 
{
    // FORCE SYNC: Bind the active global Vulkan instance handle directly onto the local wrapper tracking pointer
    context->instance = render_context.instance;
    context->surface  = render_context.surface_main;

    if (!select_physical_device(context)) {
        return FALSE;
    }

    phoenix_info("VULKAN", "Creating logical device...");
    bool present_shares_graphics_queue = context->device.graphics_queue_index == context->device.present_queue_index;
    bool transfer_shares_graphics_queue = context->device.graphics_queue_index == context->device.transfer_queue_index;
    
    int index_count = 1;
    if (!present_shares_graphics_queue) {
        index_count++;
    }
    if (!transfer_shares_graphics_queue) {
        index_count++;
    }
    
    int indices[3] = {0};
    char index = 0;
    indices[index++] = context->device.graphics_queue_index;
    if (!present_shares_graphics_queue) {
        indices[index++] = context->device.present_queue_index;
    }
    if (!transfer_shares_graphics_queue) {
        indices[index++] = context->device.transfer_queue_index;
    }

    VkDeviceQueueCreateInfo queue_create_infos[3];
    memset(queue_create_infos, 0, sizeof(queue_create_infos));
    
    float queue_priority = 1.0f;
    for (int i = 0; i < index_count; ++i) {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex = indices[i];
        queue_create_infos[i].queueCount = 1;
        queue_create_infos[i].flags = 0;
        queue_create_infos[i].pNext = 0;
        queue_create_infos[i].pQueuePriorities = &queue_priority;
    }

    VkPhysicalDeviceFeatures supported_features;
    vkGetPhysicalDeviceFeatures(context->device.physical_device, &supported_features);

    VkPhysicalDeviceFeatures device_features = {0};
    device_features.samplerAnisotropy = VK_TRUE; 
    /* wireframe view (polygonMode LINE) needs this. if the GPU lacks it the Wireframe button falls back to shaded. */
    device_features.fillModeNonSolid = supported_features.fillModeNonSolid;
    g_device_fill_mode_non_solid     = (supported_features.fillModeNonSolid == VK_TRUE);

    VkDeviceCreateInfo device_create_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_create_info.queueCreateInfoCount = index_count;
    device_create_info.pQueueCreateInfos = queue_create_infos;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = 1;
    const char* extension_names = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    device_create_info.ppEnabledExtensionNames = &extension_names;

    device_create_info.enabledLayerCount = 0;
    device_create_info.ppEnabledLayerNames = 0;

    validate_vulkan_result(vkCreateDevice(context->device.physical_device, &device_create_info, context->allocator, &context->device.logical_device));
    phoenix_info("VULKAN", "Logical device created.");

    // =========================================================================
    // 🛠️ CRASH FIX: SYNCHRONIZE LOGICAL DEVICE & QUEUES TO GLOBAL CONTEXT TRAY
    // =========================================================================
    render_context.logical_device = context->device.logical_device;

    vkGetDeviceQueue(context->device.logical_device, context->device.graphics_queue_index, 0, &context->device.graphics_queue);
    vkGetDeviceQueue(context->device.logical_device, context->device.present_queue_index, 0, &context->device.present_queue);
    vkGetDeviceQueue(context->device.logical_device, context->device.transfer_queue_index, 0, &context->device.transfer_queue);
    
    // Bind hardware handles back to top level execution pointers
    render_context.graphics_queue = context->device.graphics_queue;
    phoenix_info("VULKAN", "Queues obtained.");

    VkCommandPoolCreateInfo pool_create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_create_info.queueFamilyIndex = context->device.graphics_queue_index;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    validate_vulkan_result(vkCreateCommandPool(context->device.logical_device, &pool_create_info, context->allocator, &context->device.graphics_command_pool));
    
    render_context.command_pool = context->device.graphics_command_pool;
    phoenix_info("VULKAN", "Graphics command pool created.");

    return TRUE;
}

void remove_vulkan_device(vulkan_system_context* context) 
{
    context->device.graphics_queue = 0;
    context->device.present_queue = 0;
    context->device.transfer_queue = 0;

    phoenix_info("VULKAN", "Destroying command pools...");
    vkDestroyCommandPool(context->device.logical_device, context->device.graphics_command_pool, context->allocator);

    phoenix_info("VULKAN", "Destroying logical device...");
    if (context->device.logical_device) {
        vkDestroyDevice(context->device.logical_device, context->allocator);
        context->device.logical_device = 0;
    }

    phoenix_info("VULKAN", "Releasing physical device resources...");
    context->device.physical_device = 0;

    if (context->device.swapchain_support.formats) {
        kfree(
            context->device.swapchain_support.formats,
            sizeof(VkSurfaceFormatKHR) * context->device.swapchain_support.format_count,
            MEMORY_TAG_RENDERER);
        context->device.swapchain_support.formats = 0;
        context->device.swapchain_support.format_count = 0;
    }

    if (context->device.swapchain_support.present_modes) {
        kfree(
            context->device.swapchain_support.present_modes,
            sizeof(VkPresentModeKHR) * context->device.swapchain_support.present_mode_count,
            MEMORY_TAG_RENDERER);
        context->device.swapchain_support.present_modes = 0;
        context->device.swapchain_support.present_mode_count = 0;
    }

    kzero_memory(
        &context->device.swapchain_support.capabilities,
        sizeof(context->device.swapchain_support.capabilities));

    context->device.graphics_queue_index = -1;
    context->device.present_queue_index = -1;
    context->device.transfer_queue_index = -1;
}

void query_vulkan_device_swapchain_support(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    vulkan_swapchain_support_info* out_support_info) {
    validate_vulkan_result(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device,
        surface,
        &out_support_info->capabilities));

    uint32_t format_count = 0;
    validate_vulkan_result(vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device,
        surface,
        &format_count,
        0));
    out_support_info->format_count = (int)format_count;

    if (format_count != 0) {
        if (!out_support_info->formats) {
            out_support_info->formats = kallocate(sizeof(VkSurfaceFormatKHR) * format_count, MEMORY_TAG_RENDERER);
        }
        validate_vulkan_result(vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device,
            surface,
            &format_count,
            out_support_info->formats));
    }

    uint32_t present_mode_count = 0;
    validate_vulkan_result(vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device,
        surface,
        &present_mode_count,
        0));
    out_support_info->present_mode_count = (int)present_mode_count;

    if (present_mode_count != 0) {
        if (!out_support_info->present_modes) {
            out_support_info->present_modes = kallocate(sizeof(VkPresentModeKHR) * present_mode_count, MEMORY_TAG_RENDERER);
        }
        validate_vulkan_result(vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device,
            surface,
            &present_mode_count,
            out_support_info->present_modes));
    }
}

bool detect_vulkan_device_depth_format(vulkan_device* device) {
    const long long candidate_count = 3;
    VkFormat candidates[3] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT};

    int flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    for (long long i = 0; i < candidate_count; ++i) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device->physical_device, candidates[i], &properties);

        if ((properties.linearTilingFeatures & flags) == flags) {
            device->depth_format = candidates[i];
            return TRUE;
        } else if ((properties.optimalTilingFeatures & flags) == flags) {
            device->depth_format = candidates[i];
            return TRUE;
        }
    }

    return FALSE;
}

bool select_physical_device(vulkan_system_context* context) 
{
    uint32_t physical_device_count = 0;
    validate_vulkan_result(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, 0));
    if (physical_device_count == 0) {
        phoenix_fatal("VULKAN", "No devices which support Vulkan were found.");
        return FALSE;
    }

    VkPhysicalDevice physical_devices[physical_device_count];
    validate_vulkan_result(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices));

    for (uint32_t i = 0; i < physical_device_count; ++i) 
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_devices[i], &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physical_devices[i], &features);

        VkPhysicalDeviceMemoryProperties memory;
        vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &memory);

        vulkan_physical_device_requirements requirements;
        memset(&requirements, 0, sizeof(requirements));
        requirements.graphics = TRUE;
        requirements.present = TRUE;
        requirements.transfer = TRUE;
        requirements.sampler_anisotropy = TRUE;
        requirements.discrete_gpu = TRUE;
        
        // ALLOCATE REQ PACKET USING STRONG POINTER DEFINITIONS
        requirements.device_extension_names = (const char**)create_parry(PARRY_DEFAULT_CAPACITY, sizeof(char*));

        // FIX: Changed from 'const char' primitive to 'const char*' string literal pointer path
        const char* ext_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        parry_push(requirements.device_extension_names, ext_name);
vulkan_physical_device_queue_family_info queue_info;
memset(&queue_info, 0, sizeof(queue_info));
bool result = physical_device_meets_requirements(
physical_devices[i],
context->surface,
&properties,
&features,
&requirements,
&queue_info,
&context->device.swapchain_support);
if (result) {
phoenix_info("VULKAN", "Selected device: '%s'.", properties.deviceName);
switch (properties.deviceType) {
default:
case VK_PHYSICAL_DEVICE_TYPE_OTHER:
phoenix_info("VULKAN", "GPU type is Unknown.");
break;
case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
phoenix_info("VULKAN", "GPU type is Integrated.");
break;
case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
phoenix_info("VULKAN", "GPU type is Discrete.");
break;
case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
phoenix_info("VULKAN", "GPU type is Virtual.");
break;
case VK_PHYSICAL_DEVICE_TYPE_CPU:
phoenix_info("VULKAN", "GPU type is CPU.");
break;
}
phoenix_info("VULKAN",
"GPU Driver version: %d.%d.%d",
VK_VERSION_MAJOR(properties.driverVersion),
VK_VERSION_MINOR(properties.driverVersion),
VK_VERSION_PATCH(properties.driverVersion));
phoenix_info("VULKAN",
"Vulkan API version: %d.%d.%d",
VK_VERSION_MAJOR(properties.apiVersion),
VK_VERSION_MINOR(properties.apiVersion),
VK_VERSION_PATCH(properties.apiVersion));
for (uint32_t j = 0; j < memory.memoryHeapCount; ++j) {
float memory_size_gib = (((float)memory.memoryHeaps[j].size) / 1024.0f / 1024.0f / 1024.0f);
if (memory.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
phoenix_info("VULKAN", "Local GPU memory: %.2f GiB", memory_size_gib);
} else {
phoenix_info("VULKAN", "Shared System memory: %.2f GiB", memory_size_gib);
}
}
context->device.physical_device = physical_devices[i];
context->device.graphics_queue_index = queue_info.graphics_family_index;
context->device.present_queue_index = queue_info.present_family_index;
context->device.transfer_queue_index = queue_info.transfer_family_index;
context->device.properties = properties;
context->device.features = features;
context->device.memory = memory;
break;
}
}
if (!context->device.physical_device) {
phoenix_error("VULKAN", "No physical devices were found which meet the requirements.");
return FALSE;
}
phoenix_info("VULKAN", "Physical device selected.");
return TRUE;
}

bool physical_device_meets_requirements(
    VkPhysicalDevice device,
    VkSurfaceKHR surface,
    const VkPhysicalDeviceProperties* properties,
    const VkPhysicalDeviceFeatures* features,
    const vulkan_physical_device_requirements* requirements,
    vulkan_physical_device_queue_family_info* out_queue_info,
    vulkan_swapchain_support_info* out_swapchain_support) 
{
    out_queue_info->graphics_family_index = -1;
    out_queue_info->present_family_index = -1;
    out_queue_info->compute_family_index = -1;
    out_queue_info->transfer_family_index = -1;

    if (requirements->discrete_gpu) {
        if (properties->deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            phoenix_info("VULKAN", "Device is not a discrete GPU, and one is required. Skipping.");
            return FALSE;
        }
    }

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, 0);
    VkQueueFamilyProperties queue_families[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    phoenix_info("VULKAN", "Graphics | Present | Compute | Transfer | Name");
    unsigned char min_transfer_score = 255;

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        unsigned char current_transfer_score = 0;
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            out_queue_info->graphics_family_index = (int)i;
            ++current_transfer_score;
        }
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            out_queue_info->compute_family_index = (int)i;
            ++current_transfer_score;
        }
        if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (current_transfer_score <= min_transfer_score) {
                min_transfer_score = current_transfer_score;
                out_queue_info->transfer_family_index = (int)i;
            }
        }
        VkBool32 supports_present = VK_FALSE;
        validate_vulkan_result(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supports_present));
        if (supports_present) {
            out_queue_info->present_family_index = (int)i;
        }
    }

    phoenix_info("VULKAN", "       %d |       %d |       %d |        %d | %s",
        out_queue_info->graphics_family_index != -1,
        out_queue_info->present_family_index != -1,
        out_queue_info->compute_family_index != -1,
        out_queue_info->transfer_family_index != -1,
        properties->deviceName);

    if ((!requirements->graphics || (requirements->graphics && out_queue_info->graphics_family_index != -1)) &&
        (!requirements->present  || (requirements->present  && out_queue_info->present_family_index  != -1)) &&
        (!requirements->compute  || (requirements->compute  && out_queue_info->compute_family_index  != -1)) &&
        (!requirements->transfer || (requirements->transfer && out_queue_info->transfer_family_index != -1))) 
    {
        phoenix_info("VULKAN", "Device meets queue requirements.");
        phoenix_trace("VULKAN", "Graphics Family Index: %i", out_queue_info->graphics_family_index);
        phoenix_trace("VULKAN", "Present Family Index:  %i", out_queue_info->present_family_index);
        phoenix_trace("VULKAN", "Transfer Family Index: %i", out_queue_info->transfer_family_index);
        phoenix_trace("VULKAN", "Compute Family Index:  %i", out_queue_info->compute_family_index);

        query_vulkan_device_swapchain_support(device, surface, out_swapchain_support);

        if (out_swapchain_support->format_count < 1 || out_swapchain_support->present_mode_count < 1) {
            if (out_swapchain_support->formats) {
                kfree(out_swapchain_support->formats, sizeof(VkSurfaceFormatKHR) * out_swapchain_support->format_count, MEMORY_TAG_RENDERER);
                out_swapchain_support->formats = NULL;
            }
            if (out_swapchain_support->present_modes) {
                kfree(out_swapchain_support->present_modes, sizeof(VkPresentModeKHR) * out_swapchain_support->present_mode_count, MEMORY_TAG_RENDERER);
                out_swapchain_support->present_modes = NULL;
            }
            phoenix_info("VULKAN", "Required swapchain support not present, skipping device.");
            return FALSE;
        }

        // =====================================================================
        // 🛠️ FIX: SAFE EXTENSION MAPPING AND STRING TEST EVALUATIONS
        // =====================================================================
        if (requirements->device_extension_names) {
            uint32_t available_extension_count = 0;
            VkExtensionProperties* available_extensions = NULL;
            validate_vulkan_result(vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, NULL));

            if (available_extension_count != 0) {
                available_extensions = kallocate(sizeof(VkExtensionProperties) * available_extension_count, MEMORY_TAG_RENDERER);
                validate_vulkan_result(vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, available_extensions));

                int required_extension_count = (int)parry_length(requirements->device_extension_names);
                for (int i = 0; i < required_extension_count; ++i) {
                    bool found = FALSE;
                    
                    // Defensive validation gate to prevent uninitialized pointer reads
                    if (!requirements->device_extension_names[i]) {
                        continue;
                    }

                    for (uint32_t j = 0; j < available_extension_count; ++j) {
                        if (strcmp(requirements->device_extension_names[i], available_extensions[j].extensionName) == 0) {
                            found = TRUE;
                            break;
                        }
                    }

                    if (!found) {
                        phoenix_info("VULKAN", "Required extension not found: '%s', skipping device.", requirements->device_extension_names[i]);
                        kfree(available_extensions, sizeof(VkExtensionProperties) * available_extension_count, MEMORY_TAG_RENDERER);
                        return FALSE;
                    }
                }
            }
            kfree(available_extensions, sizeof(VkExtensionProperties) * available_extension_count, MEMORY_TAG_RENDERER);
        }

        if (requirements->sampler_anisotropy && !features->samplerAnisotropy) {
            phoenix_info("VULKAN", "Device does not support samplerAnisotropy, skipping.");
            return FALSE;
        }

        return TRUE;
    }

    return FALSE;
}