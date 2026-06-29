// SECTION: INCLUDES

#define GLFW_INCLUDE_VULKAN
#define VK_USE_PLATFORM_WAYLAND_KHR
#define GLFW_EXPOSE_NATIVE_WAYLAND

#include "../core/defines.h"

#include <math.h>
#include <string.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "../core/logger.h"
#include "../core/memory.h"

// SECTION: TYPE DEFINITIONS

struct QueueFamilyIndices
{
    u32 graphicsFamily;
    u32 presentFamily;
};

struct SwapchainInfo
{
    VkFormat format;
    VkExtent2D extent;
    u32 imageCount;

    VkImage *images;
    VkImageView *imageViews;
    VkFramebuffer *framebuffers;
};

struct SyncObjects
{
    VkSemaphore *drawComplete;
    VkSemaphore presentationComplete;
    VkFence frameRendered;
};

struct VertexInfo
{
    f32 position[2];
    f32 color[3];
};

// SECTION: GLOBAL STATE
struct VertexInfo vertices[3] = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}}, {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

#define REQUESTED_EXTENSION_COUNT 1
const char *requested_extensions[REQUESTED_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

#define REQUESTED_LAYER_COUNT 1
const char *requested_layers[REQUESTED_LAYER_COUNT] = {"VK_LAYER_KHRONOS_validation"};

#define REQUESTED_DEVICE_EXTENSION_COUNT 1
const char *requested_device_extensions[REQUESTED_DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

static VkInstance instance;
static VkDevice device;
static VkRenderPass renderpass;
static VkPipeline graphics_pipeline;
static VkSurfaceKHR surface;
static VkPhysicalDevice physical_device;
static VkPipelineLayout pipeline_layout;

static VkCommandPool command_pool;
static VkCommandBuffer command_buffer;

static VkSwapchainKHR swapchain;

static VkQueue graphics_queue;
static VkQueue present_queue;

static struct SwapchainInfo swapchain_info;

static struct SyncObjects sync_objects;

static struct QueueFamilyIndices queue_family_indices;

static VkDebugUtilsMessengerEXT debug_messenger;

static GLFWwindow *glfw_window; // TODO: Move this out

static VkBuffer vertex_buffer;
static VkDeviceMemory vertex_buffer_memory;

// SECTION: FUNCTION DEFINITIONS

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                            VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                            const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data)
{
    if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        PERROR("VULKAN VALIDATION LAYER: %s", callback_data->pMessage);
    }
    if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        PWARN("VULKAN VALIDATION LAYER: %s", callback_data->pMessage);
    }

    return VK_FALSE;
}

VkResult vulkan_create_debug_messenger(VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info)
{
    PFN_vkCreateDebugUtilsMessengerEXT function_pointer =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (function_pointer == NULL)
    {
        return VK_ERROR_UNKNOWN;
    }
    else
    {
        return function_pointer(instance, &debug_messenger_create_info, NULL, &debug_messenger);
    }
}

b32 vulkan_check_extension_availability(const char **extension_list, u32 extension_count)
{
    b32 all_extensions_available = 1;

    u32 available_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &available_extension_count, NULL);
    VkExtensionProperties *available_extensions =
        (VkExtensionProperties *)platform_alloc(available_extension_count * sizeof(VkExtensionProperties), MEM_TAG_INIT);
    vkEnumerateInstanceExtensionProperties(NULL, &available_extension_count, available_extensions);

    for (i32 i = 0; i < extension_count; i++)
    {
        b32 extension_available = 0;
        for (i32 j = 0; j < available_extension_count; j++)
        {
            if (strcmp(extension_list[i], available_extensions[j].extensionName) == 0)
            {
                extension_available = 1;
            }
        }
        if (extension_available)
        {
            PDEBUG("\033[32m\t%s: AVAILABLE\033[0m", extension_list[i]);
        }
        else
        {
            PDEBUG("\031[31m\t%s: AVAILABLE\033[0m", extension_list[i]);
            all_extensions_available = 0;
        }
    }
    platform_free(available_extensions, available_extension_count * sizeof(VkExtensionProperties), MEM_TAG_INIT);
    return (all_extensions_available);
}

b32 vulkan_check_layer_availability(const char **layer_list, u32 layer_count)
{
    b32 all_layers_available = 1;

    u32 available_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);
    VkLayerProperties *available_layers = (VkLayerProperties *)platform_alloc(available_layer_count * sizeof(VkLayerProperties), MEM_TAG_INIT);
    vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers);

    for (i32 i = 0; i < layer_count; i++)
    {
        b32 layer_available = 0;
        for (i32 j = 0; j < available_layer_count; j++)
        {
            if (strcmp(layer_list[i], available_layers[j].layerName) == 0)
            {
                layer_available = 1;
            }
        }
        if (layer_available)
        {
            PDEBUG("\033[32m\t%s: AVAILABLE\033[0m", layer_list[i]);
        }
        else
        {
            PDEBUG("\033[31m\t%s: UNAVAILABLE\033[0m", layer_list[i]);
            all_layers_available = 0;
        }
    }
    platform_free(available_layers, available_layer_count * sizeof(VkLayerProperties), MEM_TAG_INIT);
    return (all_layers_available);
}

b32 vulkan_check_physical_device_suitability(VkPhysicalDevice candidate_device)
{
    b32 is_discrete_GPU = 0;
    b32 has_geometry_shader = 0;
    b32 has_graphics_family = 0;
    b32 has_presentation_family = 0;
    b32 all_device_extensions_available = 1;
    b32 swapchain_is_compatible = 0;

    // SUBFUNC: Check for discrete GPU
    VkPhysicalDeviceProperties candidate_device_properties;
    vkGetPhysicalDeviceProperties(candidate_device, &candidate_device_properties);
    if (candidate_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        PDEBUG("\tDiscrete GPU: TRUE");
        is_discrete_GPU = 1;
    }
    else
    {
        PDEBUG("\tDiscrete GPU: FALSE");
    }

    // SUBFUNC: Check for geometry shader
    VkPhysicalDeviceFeatures candidate_device_features;
    vkGetPhysicalDeviceFeatures(candidate_device, &candidate_device_features);
    if (candidate_device_features.geometryShader)
    {
        PDEBUG("\tGeometry shader present: TRUE");
        has_geometry_shader = 1;
    }
    else
    {
        PDEBUG("\tGeometry shader present: FALSE");
    }

    // SUBFUNC: Find graphics queue family
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate_device, &queue_family_count, NULL);

    PDEBUG("\tDevice has %d queue families", queue_family_count);

    VkQueueFamilyProperties *queue_families =
        (VkQueueFamilyProperties *)platform_alloc(queue_family_count * sizeof(VkQueueFamilyProperties), MEM_TAG_INIT);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate_device, &queue_family_count, queue_families);

    for (i32 i = 0; i < queue_family_count; i++)
    {
        VkBool32 presentation_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(candidate_device, i, surface, &presentation_support);
        if (presentation_support == VK_TRUE)
        {
            PDEBUG("\t\tQueue family %d supports presentation", i);
            queue_family_indices.presentFamily = i;
            has_presentation_family = 1;
        }
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            PDEBUG("\t\tQueue family %d supports graphics", i);
            queue_family_indices.graphicsFamily = i;
            has_graphics_family = 1;
        }
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentation_support)
        {
            queue_family_indices.graphicsFamily = i;
            queue_family_indices.presentFamily = i;
            break;
        }
    }

    if (!has_graphics_family)
    {
        PDEBUG("\t\tNo queue family supports graphics");
    }
    if (!has_presentation_family)
    {
        PDEBUG("\t\tNo queue supports presentation");
    }

    // SUBFUNC: Check for device extensions
    u32 device_extension_count;
    vkEnumerateDeviceExtensionProperties(candidate_device, NULL, &device_extension_count, NULL);

    VkExtensionProperties *available_device_extensions =
        (VkExtensionProperties *)platform_alloc(device_extension_count * sizeof(VkExtensionProperties), MEM_TAG_INIT);
    vkEnumerateDeviceExtensionProperties(candidate_device, NULL, &device_extension_count, available_device_extensions);

    PDEBUG("\tChecking for device extensions:");
    for (i32 i = 0; i < REQUESTED_DEVICE_EXTENSION_COUNT; i++)
    {
        b32 requested_extension_available = 0;
        for (i32 j; j < device_extension_count; j++)
        {
            if (strcmp(requested_device_extensions[i], available_device_extensions[j].extensionName) == 0)
            {
                requested_extension_available = 1;
            }
        }
        if (requested_extension_available)
        {
            PDEBUG("\033[32m\t\t%s: AVAILABLE\033[0m", requested_device_extensions[i]);
        }
        else
        {
            PDEBUG("\033[31m\t\t%s: UNAVAILABLE\033[0m", requested_device_extensions[i]);
            all_device_extensions_available = 0;
        }
    }

    // SUBFUNC: Check swapchain properties
    u32 surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(candidate_device, surface, &surface_format_count, NULL);

    u32 present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(candidate_device, surface, &present_mode_count, NULL);

    if (surface_format_count != 0 && present_mode_count != 0)
    {
        PDEBUG("\tSwapchain is suitable");
        swapchain_is_compatible = 1;
    }
    else
    {
        PDEBUG("\tSwapchain is not suitable");
    }

    platform_free(available_device_extensions, device_extension_count * sizeof(VkExtensionProperties), MEM_TAG_INIT);
    platform_free(queue_families, queue_family_count * sizeof(VkQueueFamilyProperties), MEM_TAG_INIT);
    return (is_discrete_GPU && has_geometry_shader && has_graphics_family && has_presentation_family && all_device_extensions_available &&
            swapchain_is_compatible);
}

int vulkan_initialisation()
{
    VkResult result; // This variable is reused to hold the result for various API calls in this code block

    // SUBFUNC: Instance creation
    VkApplicationInfo application_info = {};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "Vulkan Test";
    application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName = "No Engine";
    application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &application_info;

    u32 glfw_required_extension_count = 0;
    const char **glfw_required_extensions;

    glfw_required_extensions = glfwGetRequiredInstanceExtensions(&glfw_required_extension_count);

    u32 instance_extension_count = glfw_required_extension_count + REQUESTED_EXTENSION_COUNT;
    const char **instance_extensions = platform_alloc(instance_extension_count * sizeof(const char **), MEM_TAG_INIT);
    for (i32 i = 0; i < REQUESTED_EXTENSION_COUNT; i++)
    {
        instance_extensions[i] = requested_extensions[i];
    }
    for (i32 i = 0; i < glfw_required_extension_count; i++)
    {
        instance_extensions[i + REQUESTED_EXTENSION_COUNT] = glfw_required_extensions[i];
    }

    PDEBUG("Checking for instance extension availablility");
    vulkan_check_extension_availability(instance_extensions, instance_extension_count);

    PDEBUG("Checking for layer availablility");
    vulkan_check_layer_availability(requested_layers, REQUESTED_LAYER_COUNT);

    instance_create_info.enabledExtensionCount = instance_extension_count;
    instance_create_info.ppEnabledExtensionNames = instance_extensions;
    instance_create_info.enabledLayerCount = REQUESTED_LAYER_COUNT;
    instance_create_info.ppEnabledLayerNames = requested_layers;

    result = vkCreateInstance(&instance_create_info, NULL, &instance);

    if (result == VK_SUCCESS)
    {
        PDEBUG("Vulkan instance created successfully");
    }
    else
    {
        PERROR("Failed to create vulkan instance");
    }

    // SUBFUNC: Create debug messenger
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};
    debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_create_info.pfnUserCallback = vulkan_debug_callback;

    result = vulkan_create_debug_messenger(debug_messenger_create_info);

    if (result == VK_SUCCESS)
    {
        PDEBUG("Debug messenger created successfully");
    }
    else
    {
        PERROR("Failed to create debug messenger");
    }

    // SUBFUNC: Create KHR surface
    //  TODO: This should be a part of any future platform layer
    VkWaylandSurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    surface_create_info.surface = glfwGetWaylandWindow(glfw_window);
    surface_create_info.display = glfwGetWaylandDisplay();

    result = vkCreateWaylandSurfaceKHR(instance, &surface_create_info, NULL, &surface);

    if (result == VK_SUCCESS)
    {
        PDEBUG("Window surface created");
    }
    else
    {
        PFATAL("Failed to create window surface");
    }

    // SUBFUNC: Physical device selection
    physical_device = VK_NULL_HANDLE;

    u32 candidate_physical_device_count = 0;
    vkEnumeratePhysicalDevices(instance, &candidate_physical_device_count, NULL);
    if (candidate_physical_device_count == 0)
    {
        PFATAL("No GPU's with vulkan support");
    }
    VkPhysicalDevice *candidate_physical_devices =
        (VkPhysicalDevice *)platform_alloc(candidate_physical_device_count * sizeof(VkPhysicalDevice), MEM_TAG_INIT);
    vkEnumeratePhysicalDevices(instance, &candidate_physical_device_count, candidate_physical_devices);

    for (i32 i = 0; i < candidate_physical_device_count; i++)
    {
        PDEBUG("Assessing suitability of physical device %d:", i);
        if (vulkan_check_physical_device_suitability(candidate_physical_devices[i]))
        {
            physical_device = candidate_physical_devices[i];
            break;
        }
    }
    if (physical_device == VK_NULL_HANDLE)
    {
        PFATAL("Unable to find a valid GPU");
    }

    // SUBFUNC: Create logical device
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    VkDeviceQueueCreateInfo graphics_queue_create_info = {};
    graphics_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_create_info.queueFamilyIndex = queue_family_indices.graphicsFamily;
    graphics_queue_create_info.queueCount = 1;
    f32 graphics_queue_priority = 0.5f;
    graphics_queue_create_info.pQueuePriorities = &graphics_queue_priority;

    if (queue_family_indices.graphicsFamily == queue_family_indices.presentFamily)
    {
        device_create_info.queueCreateInfoCount = 1;
        device_create_info.pQueueCreateInfos = &graphics_queue_create_info;
    }
    else
    {
        VkDeviceQueueCreateInfo present_queue_create_info = {};
        present_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        present_queue_create_info.queueFamilyIndex = queue_family_indices.presentFamily;
        present_queue_create_info.queueCount = 1;
        f32 present_queue_priority = 0.5f;
        present_queue_create_info.pQueuePriorities = &present_queue_priority;

        VkDeviceQueueCreateInfo queue_create_infos[2] = {graphics_queue_create_info, present_queue_create_info};

        device_create_info.queueCreateInfoCount = 2;
        device_create_info.pQueueCreateInfos = queue_create_infos;
    }

    VkPhysicalDeviceFeatures physical_device_features = {};
    VkPhysicalDeviceVulkan11Features vulkan_11_features = {};
    vulkan_11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan_11_features.shaderDrawParameters = VK_TRUE;

    device_create_info.pEnabledFeatures = &physical_device_features;
    device_create_info.pNext = &vulkan_11_features;

    device_create_info.enabledExtensionCount = 1;
    device_create_info.ppEnabledExtensionNames = requested_device_extensions;

    result = vkCreateDevice(physical_device, &device_create_info, NULL, &device);
    if (result == VK_SUCCESS)
    {
        PDEBUG("Logical device created successfully");
    }
    else
    {
        PFATAL("Failed to create logical device");
    }

    // SUBFUNC: Retrieve handle to graphics queue
    vkGetDeviceQueue(device, queue_family_indices.graphicsFamily, 0, &graphics_queue);
    vkGetDeviceQueue(device, queue_family_indices.presentFamily, 0, &present_queue);

    // SUBFUNC: Create swapchain
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkExtent2D swap_extent;

    u32 surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, NULL);
    VkSurfaceFormatKHR *available_surface_formats =
        (VkSurfaceFormatKHR *)platform_alloc(surface_format_count * sizeof(VkSurfaceFormatKHR), MEM_TAG_INIT);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, available_surface_formats);

    surface_format = available_surface_formats[0];

    for (i32 i = 0; i < surface_format_count; i++)
    {
        if (available_surface_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surface_format = available_surface_formats[i];
        }
    }

    u32 present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL);
    VkPresentModeKHR *available_present_modes = (VkPresentModeKHR *)platform_alloc(present_mode_count * sizeof(VkPresentModeKHR), MEM_TAG_INIT);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, available_present_modes);

    present_mode = VK_PRESENT_MODE_FIFO_KHR;

    for (i32 i = 0; i < present_mode_count; i++)
    {
        if (available_present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

    if (surface_capabilities.currentExtent.width == UINT32_MAX)
    {
        i32 width, height;
        glfwGetFramebufferSize(glfw_window, &width, &height);
        if (width < surface_capabilities.minImageExtent.width)
        {
            width = surface_capabilities.minImageExtent.width;
        }
        if (width > surface_capabilities.maxImageExtent.width)
        {
            width = surface_capabilities.maxImageExtent.width;
        }
        if (height < surface_capabilities.minImageExtent.height)
        {
            height = surface_capabilities.minImageExtent.height;
        }
        if (height > surface_capabilities.maxImageExtent.height)
        {
            height = surface_capabilities.maxImageExtent.height;
        }
        swap_extent.width = (u32)width;
        swap_extent.height = (u32)height;
    }
    else
    {
        swap_extent = surface_capabilities.currentExtent;
    }

    u32 image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount)
    {
        image_count = surface_capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = swap_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (queue_family_indices.graphicsFamily == queue_family_indices.presentFamily)
    {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else
    {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = (u32 *)&queue_family_indices;
    }

    swapchain_create_info.preTransform = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain);

    if (result == VK_SUCCESS)
    {
        PDEBUG("Swapchain created successfully");
    }
    else
    {
        PFATAL("Unable to create swapchain");
    }

    // SUBFUNC: Get swapchain images

    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_info.imageCount, NULL);
    swapchain_info.images = (VkImage *)platform_alloc(swapchain_info.imageCount * sizeof(VkImage), MEM_TAG_RENDERER);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_info.imageCount, swapchain_info.images);

    swapchain_info.format = surface_format.format;
    swapchain_info.extent = swap_extent;

    // SUBFUNC: Create Image Views
    swapchain_info.imageViews = (VkImageView *)platform_alloc(swapchain_info.imageCount * sizeof(VkImageView), MEM_TAG_RENDERER);
    for (i32 i = 0; i < swapchain_info.imageCount; i++)
    {
        VkImageViewCreateInfo view_create_info = {};
        view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create_info.image = swapchain_info.images[i];
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_create_info.format = swapchain_info.format;
        view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_create_info.subresourceRange.baseMipLevel = 0;
        view_create_info.subresourceRange.levelCount = 1;
        view_create_info.subresourceRange.baseArrayLayer = 0;
        view_create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(device, &view_create_info, NULL, &swapchain_info.imageViews[i]);

        if (result == VK_SUCCESS)
        {
            PDEBUG("Image view %d created", i);
        }
        else
        {
            PFATAL("Failed to create image view %d", i);
        }
    }

    platform_free(available_present_modes, present_mode_count * sizeof(VkPresentModeKHR), MEM_TAG_INIT);
    platform_free(available_surface_formats, surface_format_count * sizeof(VkSurfaceFormatKHR), MEM_TAG_INIT);
    platform_free(candidate_physical_devices, candidate_physical_device_count * sizeof(VkPhysicalDevice), MEM_TAG_INIT);
    platform_free(instance_extensions, instance_extension_count * sizeof(const char **), MEM_TAG_INIT);
    return 0;
}

VkShaderModule create_shader_module(char *code, u32 code_size)
{
    VkShaderModuleCreateInfo SM_create_info = {};
    SM_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    SM_create_info.codeSize = code_size;
    SM_create_info.pCode = (u32 *)code;

    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(device, &SM_create_info, NULL, &shader_module);
    if (result == VK_SUCCESS)
    {
        PDEBUG("Shader module created");
    }
    else
    {
        PDEBUG("Failed to create shader module");
    }
    return shader_module;
}

int vulkan_pipeline_construction()
{
    VkResult result = 0;

    // SUBFUNC: set up vertex buffer
    VkVertexInputBindingDescription binding_description = {};
    binding_description.binding = 0;
    binding_description.stride = sizeof(struct VertexInfo);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute_descriptions[2] = {};
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(struct VertexInfo, position);

    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(struct VertexInfo, color);

    // SUBFUNC: Compile shaders
    u32 shader_code_size = 0;
    char *shader_code = open_file("data/shaders/shader.spv", &shader_code_size);

    VkShaderModule shader_module = create_shader_module(shader_code, shader_code_size);

    VkPipelineShaderStageCreateInfo vert_create_info = {};
    vert_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_create_info.module = shader_module;
    vert_create_info.pName = "vertex_main";

    VkPipelineShaderStageCreateInfo frag_create_info = {};
    frag_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_create_info.module = shader_module;
    frag_create_info.pName = "fragment_main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_create_info, frag_create_info};

    // SUBFUNC: Fixed function stages

    // TODO: Sort out dynamic states
    // VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo DSC_info = {};
    DSC_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    // DSC_info.dynamicStateCount = 2;
    // DSC_info.pDynamicStates = dynamic_states;

    VkPipelineVertexInputStateCreateInfo VISC_info = {};
    VISC_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VISC_info.vertexBindingDescriptionCount = 1;
    VISC_info.pVertexBindingDescriptions = &binding_description;
    VISC_info.vertexAttributeDescriptionCount = 2;
    VISC_info.pVertexAttributeDescriptions = attribute_descriptions;

    VkPipelineInputAssemblyStateCreateInfo IASC_info = {};
    IASC_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    IASC_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    IASC_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = swapchain_info.extent.width;
    viewport.height = swapchain_info.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = swapchain_info.extent;

    VkPipelineViewportStateCreateInfo VSC_info = {};
    VSC_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    VSC_info.viewportCount = 1;
    VSC_info.scissorCount = 1;
    VSC_info.pViewports = &viewport;
    VSC_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo RSC_info = {};
    RSC_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RSC_info.depthClampEnable = VK_FALSE;
    RSC_info.rasterizerDiscardEnable = VK_FALSE;
    RSC_info.polygonMode = VK_POLYGON_MODE_FILL;
    RSC_info.lineWidth = 1.0f;
    RSC_info.cullMode = VK_CULL_MODE_BACK_BIT;
    RSC_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    RSC_info.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo MSC_info = {};
    MSC_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MSC_info.sampleShadingEnable = VK_FALSE;
    MSC_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState CBA_state = {};
    CBA_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    CBA_state.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo CBSC_info = {};
    CBSC_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    CBSC_info.logicOpEnable = VK_FALSE;
    CBSC_info.attachmentCount = 1;
    CBSC_info.pAttachments = &CBA_state;

    VkPipelineLayoutCreateInfo PLC_info = {};
    PLC_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    result = vkCreatePipelineLayout(device, &PLC_info, NULL, &pipeline_layout);

    if (result == VK_SUCCESS)
    {
        PDEBUG("Pipeline layout created");
    }
    else
    {
        PFATAL("Failed to create pipeline layout");
    }

    // SUBFUNC: Create render pass
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = swapchain_info.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_reference = {};
    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_attachment_reference;

    VkSubpassDependency subpass_dependency = {};
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.dstSubpass = 0;
    subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.srcAccessMask = 0;
    subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo RPC_info = {};
    RPC_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RPC_info.attachmentCount = 1;
    RPC_info.pAttachments = &color_attachment;
    RPC_info.subpassCount = 1;
    RPC_info.pSubpasses = &subpass_description;
    RPC_info.dependencyCount = 1;
    RPC_info.pDependencies = &subpass_dependency;

    result = vkCreateRenderPass(device, &RPC_info, NULL, &renderpass);

    if (result == VK_SUCCESS)
    {
        PDEBUG("Render pass created");
    }
    else
    {
        PFATAL("Failed to create render pass");
    }

    // SUBFUNC: Pipeline creation
    VkGraphicsPipelineCreateInfo GPC_info = {};
    GPC_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    GPC_info.stageCount = 2;
    GPC_info.pStages = shader_stages;

    GPC_info.pVertexInputState = &VISC_info;
    GPC_info.pInputAssemblyState = &IASC_info;
    GPC_info.pViewportState = &VSC_info;
    GPC_info.pRasterizationState = &RSC_info;
    GPC_info.pMultisampleState = &MSC_info;
    GPC_info.pDepthStencilState = NULL;
    GPC_info.pColorBlendState = &CBSC_info;
    GPC_info.pDynamicState = &DSC_info;

    GPC_info.layout = pipeline_layout;
    GPC_info.renderPass = renderpass;
    GPC_info.subpass = 0;

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &GPC_info, NULL, &graphics_pipeline);
    if (result == VK_SUCCESS)
    {
        PDEBUG("Graphics pipeline created");
    }
    else
    {
        PFATAL("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device, shader_module, NULL);
    close_file(shader_code, shader_code_size);
    return 0;
}

void vulkan_setup_renderer()
{
    VkResult result = 0;

    // SUBFUNC: create vertex buffer
    VkBufferCreateInfo BC_info = {};
    BC_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    BC_info.size = sizeof(struct VertexInfo) * 3;
    BC_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    BC_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device, &BC_info, NULL, &vertex_buffer);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, vertex_buffer, &memory_requirements);

    VkMemoryAllocateInfo MA_info = {};
    MA_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MA_info.allocationSize = memory_requirements.size;

    VkPhysicalDeviceMemoryProperties PDM_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &PDM_properties);

    u32 property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    for (i32 i = 0; i < PDM_properties.memoryTypeCount; i++)
    {
        if ((memory_requirements.memoryTypeBits & 1 << i) && ((PDM_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags))
        {
            MA_info.memoryTypeIndex = i;
            break;
        }
    }

    vkAllocateMemory(device, &MA_info, NULL, &vertex_buffer_memory);

    vkBindBufferMemory(device, vertex_buffer, vertex_buffer_memory, 0);

    // SUBFUNC: create framebuffers
    swapchain_info.framebuffers = (VkFramebuffer *)platform_alloc(swapchain_info.imageCount * sizeof(VkFramebuffer), MEM_TAG_RENDERER);
    for (i32 i = 0; i < swapchain_info.imageCount; i++)
    {
        VkImageView attachment = swapchain_info.imageViews[i];

        VkFramebufferCreateInfo FBC_info = {};
        FBC_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FBC_info.renderPass = renderpass;
        FBC_info.attachmentCount = 1;
        FBC_info.pAttachments = &swapchain_info.imageViews[i];
        FBC_info.width = swapchain_info.extent.width;
        FBC_info.height = swapchain_info.extent.height;
        FBC_info.layers = 1;

        result = vkCreateFramebuffer(device, &FBC_info, NULL, &swapchain_info.framebuffers[i]);
        if (result == VK_SUCCESS)
        {
            PDEBUG("Framebuffer %d created", i);
        }
        else
        {
            PFATAL("Failed to create framebuffer %d", i);
        }
    }

    // SUBFUNC: Command Pool creation
    VkCommandPoolCreateInfo CPC_info = {};
    CPC_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CPC_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CPC_info.queueFamilyIndex = queue_family_indices.graphicsFamily;

    result = vkCreateCommandPool(device, &CPC_info, NULL, &command_pool);
    if (result == VK_SUCCESS)
    {
        PDEBUG("Command pool created");
    }
    else
    {
        PFATAL("Failed to create command pool");
    }

    // SUBFUNC: Command buffer creation
    VkCommandBufferAllocateInfo CBA_info = {};
    CBA_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CBA_info.commandPool = command_pool;
    CBA_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CBA_info.commandBufferCount = 1;

    result = vkAllocateCommandBuffers(device, &CBA_info, &command_buffer);
    if (result == VK_SUCCESS)
    {
        PDEBUG("Command buffer allocated");
    }
    else
    {
        PFATAL("Failed to allocate command buffer");
    }

    return;
}

void record_command_buffer(u32 image_index)
{
    VkResult result = 0;
    VkCommandBufferBeginInfo CBB_info = {};
    CBB_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    result = vkBeginCommandBuffer(command_buffer, &CBB_info);

    VkRenderPassBeginInfo RPB_info = {};
    RPB_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    RPB_info.renderPass = renderpass;
    RPB_info.framebuffer = swapchain_info.framebuffers[image_index];
    RPB_info.renderArea.offset.x = 0;
    RPB_info.renderArea.offset.y = 0;
    RPB_info.renderArea.extent = swapchain_info.extent;

    VkClearValue clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    RPB_info.clearValueCount = 1;
    RPB_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &RPB_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

    VkBuffer vertex_buffers[] = {vertex_buffer};
    VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    result = vkEndCommandBuffer(command_buffer);

    if (result != VK_SUCCESS)
    {
        PFATAL("Failed to record command buffer");
    }

    return;
}

void create_sync_objects()
{
    VkResult result = 0;

    VkSemaphoreCreateInfo SC_info = {};
    SC_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    result = vkCreateSemaphore(device, &SC_info, NULL, &sync_objects.presentationComplete);

    sync_objects.drawComplete = (VkSemaphore *)platform_alloc(swapchain_info.imageCount * sizeof(VkSemaphore), MEM_TAG_RENDERER);

    for (i32 i = 0; i < swapchain_info.imageCount; i++)
    {
        result = vkCreateSemaphore(device, &SC_info, NULL, &sync_objects.drawComplete[i]);
    }

    VkFenceCreateInfo FC_info = {};
    FC_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    FC_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    result = vkCreateFence(device, &FC_info, NULL, &sync_objects.frameRendered);

    return;
}

void init_vulkan(GLFWwindow *window)
{
    glfw_window = window;
    vulkan_initialisation();
    vulkan_pipeline_construction();
    vulkan_setup_renderer();
    create_sync_objects();
}

void vulkan_test(f64 angle)
{

    glfwPollEvents();

    for (i32 i = 0; i < 3; i++)
    {
        vertices[i].position[0] = 0.5f * (f32)sin(angle - ((6.28 * i) / 3));
        vertices[i].position[1] = 0.5f * (f32)cos(angle - ((6.28 * i) / 3));
    }

    void *data;
    vkMapMemory(device, vertex_buffer_memory, 0, sizeof(struct VertexInfo) * 3, 0, &data);
    memcpy(data, vertices, sizeof(struct VertexInfo) * 3);
    vkUnmapMemory(device, vertex_buffer_memory);

    vkWaitForFences(device, 1, &sync_objects.frameRendered, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &sync_objects.frameRendered);

    u32 current_image_index;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, sync_objects.presentationComplete, VK_NULL_HANDLE, &current_image_index);

    vkResetCommandBuffer(command_buffer, 0);

    record_command_buffer(current_image_index);

    VkSubmitInfo CBS_info = {};
    CBS_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags waitstage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    CBS_info.waitSemaphoreCount = 1;
    CBS_info.pWaitSemaphores = &sync_objects.presentationComplete;
    CBS_info.pWaitDstStageMask = &waitstage;
    CBS_info.commandBufferCount = 1;
    CBS_info.pCommandBuffers = &command_buffer;
    CBS_info.signalSemaphoreCount = 1;
    CBS_info.pSignalSemaphores = &sync_objects.drawComplete[current_image_index];

    VkResult result = vkQueueSubmit(graphics_queue, 1, &CBS_info, sync_objects.frameRendered);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &sync_objects.drawComplete[current_image_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &current_image_index;

    vkQueuePresentKHR(present_queue, &present_info);

    // glfwSwapBuffers (window);

    vkDeviceWaitIdle(device);
}
