// VMA requires Vulkan function pointers when VK_NO_PROTOTYPES is defined.
// We supply them via volk's global function table.
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include "vulkan/VulkanContext.h"
#include "vulkan/VulkanUtils.h"
#include "core/Log.h"

#include <GLFW/glfw3.h>
#include <cstring>
#include <set>
#include <algorithm>

namespace lmao {

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOG(Vulkan, Error, "Validation: %s", data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG(Vulkan, Warn, "Validation: %s", data->pMessage);
    }
    return VK_FALSE;
}

VulkanContext::~VulkanContext() {
    shutdown();
}

bool VulkanContext::init(GLFWwindow* window) {
    // Initialize volk (loads vkGetInstanceProcAddr from the Vulkan loader)
    VkResult volkResult = volkInitialize();
    if (volkResult != VK_SUCCESS) {
        LOG(Vulkan, Error, "Failed to initialize volk (Vulkan loader): %d", (int)volkResult);
        return false;
    }

    if (!createInstance()) return false;

    // Load instance-level functions
    volkLoadInstance(m_instance);

    if (s_enableValidation && !setupDebugMessenger()) return false;
    if (!createSurface(window)) return false;
    if (!pickPhysicalDevice()) return false;
    if (!createLogicalDevice()) return false;

    // Load device-level functions for optimal dispatch
    volkLoadDevice(m_device);

    if (!createAllocator()) return false;

    LOG(Vulkan, Info, "Vulkan context initialized");
    LOG(Vulkan, Info, "  Device: %s", m_deviceProps.deviceName);
    LOG(Vulkan, Info, "  API version: %u.%u.%u",
        VK_VERSION_MAJOR(m_deviceProps.apiVersion),
        VK_VERSION_MINOR(m_deviceProps.apiVersion),
        VK_VERSION_PATCH(m_deviceProps.apiVersion));
    LOG(Vulkan, Info, "  Ray tracing: %s", m_features.rayTracing ? "supported" : "not available");
    LOG(Vulkan, Debug, "  Graphics queue family: %u", m_queueFamilies.graphics);
    LOG(Vulkan, Debug, "  Present queue family: %u", m_queueFamilies.present);
    LOG(Vulkan, Debug, "  Compute queue family: %u", m_queueFamilies.compute);
    return true;
}

void VulkanContext::shutdown() {
    if (m_allocator) { vmaDestroyAllocator(m_allocator); m_allocator = VK_NULL_HANDLE; }
    if (m_device) { vkDestroyDevice(m_device, nullptr); m_device = VK_NULL_HANDLE; }
    if (m_surface) { vkDestroySurfaceKHR(m_instance, m_surface, nullptr); m_surface = VK_NULL_HANDLE; }
    if (m_debugMessenger) {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }
    if (m_instance) { vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; }
}

void VulkanContext::waitIdle() const {
    if (m_device) vkDeviceWaitIdle(m_device);
}

bool VulkanContext::createInstance() {
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "LmaoEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "LmaoEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);

    std::vector<const char*> layers;
    if (s_enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    VkResult r = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (r != VK_SUCCESS) {
        LOG(Vulkan, Error, "Failed to create Vulkan instance: %d", (int)r);
        return false;
    }
    return true;
}

bool VulkanContext::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT ci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debugCallback;

    VkResult r = vkCreateDebugUtilsMessengerEXT(m_instance, &ci, nullptr, &m_debugMessenger);
    if (r != VK_SUCCESS) {
        LOG(Vulkan, Warn, "Failed to set up debug messenger");
        return true; // non-fatal
    }
    return true;
}

bool VulkanContext::createSurface(GLFWwindow* window) {
    VkResult r = glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface);
    if (r != VK_SUCCESS) {
        LOG(Vulkan, Error, "Failed to create window surface");
        return false;
    }
    return true;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics = i;

        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            indices.compute = i;

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport) indices.present = i;

        if (indices.isComplete()) break;
    }
    return indices;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& required) const {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    for (auto* req : required) {
        bool found = false;
        for (auto& ext : available) {
            if (std::strcmp(req, ext.extensionName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

int VulkanContext::rateDevice(VkPhysicalDevice device) const {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    auto indices = findQueueFamilies(device);
    if (!indices.isComplete()) return -1;

    std::vector<const char*> required = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (!checkDeviceExtensionSupport(device, required)) return -1;

    int score = 0;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 10000;
    score += static_cast<int>(props.limits.maxImageDimension2D);
    return score;
}

bool VulkanContext::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) {
        LOG(Vulkan, Error, "No Vulkan-capable GPU found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    int bestScore = -1;
    for (auto d : devices) {
        int score = rateDevice(d);
        if (score > bestScore) {
            bestScore = score;
            m_physicalDevice = d;
        }
    }

    if (bestScore < 0) {
        LOG(Vulkan, Error, "No suitable GPU found");
        return false;
    }

    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProps);
    m_queueFamilies = findQueueFamilies(m_physicalDevice);

    // Check for ray tracing support
    m_features.rayTracing = checkDeviceExtensionSupport(m_physicalDevice, {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    });

    return true;
}

bool VulkanContext::createLogicalDevice() {
    std::set<uint32_t> uniqueFamilies = {
        m_queueFamilies.graphics, m_queueFamilies.present
    };
    if (m_queueFamilies.compute != UINT32_MAX)
        uniqueFamilies.insert(m_queueFamilies.compute);

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qi.queueFamilyIndex = family;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    // Extensions
    std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Vulkan 1.3 features (dynamic rendering, synchronization2)
    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.pNext = &features13;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features2.pNext = &features12;
    features2.features.samplerAnisotropy = VK_TRUE;
    features2.features.fillModeNonSolid = VK_TRUE;

    // Ray tracing features (optional)
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};

    if (m_features.rayTracing) {
        extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

        accelFeatures.accelerationStructure = VK_TRUE;
        rtFeatures.rayTracingPipeline = VK_TRUE;

        features13.pNext = &accelFeatures;
        accelFeatures.pNext = &rtFeatures;
    }

    m_features.dynamicRendering = true;
    m_features.synchronization2 = true;

    VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.pNext = &features2;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkResult r = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (r != VK_SUCCESS) {
        LOG(Vulkan, Error, "Failed to create logical device: %d", (int)r);
        return false;
    }

    vkGetDeviceQueue(m_device, m_queueFamilies.graphics, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_queueFamilies.present, 0, &m_presentQueue);
    if (m_queueFamilies.compute != UINT32_MAX)
        vkGetDeviceQueue(m_device, m_queueFamilies.compute, 0, &m_computeQueue);
    else
        m_computeQueue = m_graphicsQueue;

    return true;
}

bool VulkanContext::createAllocator() {
    // With VK_NO_PROTOTYPES, we need to supply function pointers to VMA
    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo ci{};
    ci.instance = m_instance;
    ci.physicalDevice = m_physicalDevice;
    ci.device = m_device;
    ci.vulkanApiVersion = VK_API_VERSION_1_3;
    ci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    ci.pVulkanFunctions = &vulkanFunctions;

    VkResult r = vmaCreateAllocator(&ci, &m_allocator);
    if (r != VK_SUCCESS) {
        LOG(Memory, Error, "Failed to create VMA allocator: %d", (int)r);
        return false;
    }
    return true;
}

} // namespace lmao
