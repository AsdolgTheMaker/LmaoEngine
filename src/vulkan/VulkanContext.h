#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <cstdint>

struct GLFWwindow;

namespace lmao {

struct QueueFamilyIndices {
    uint32_t graphics = UINT32_MAX;
    uint32_t present = UINT32_MAX;
    uint32_t compute = UINT32_MAX;
    bool isComplete() const {
        return graphics != UINT32_MAX && present != UINT32_MAX;
    }
};

struct DeviceFeatures {
    bool rayTracing = false;
    bool dynamicRendering = false;
    bool synchronization2 = false;
};

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    bool init(GLFWwindow* window);
    void shutdown();

    VkInstance instance() const { return m_instance; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice device() const { return m_device; }
    VkSurfaceKHR surface() const { return m_surface; }
    VmaAllocator allocator() const { return m_allocator; }

    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }
    VkQueue computeQueue() const { return m_computeQueue; }
    const QueueFamilyIndices& queueFamilies() const { return m_queueFamilies; }
    const DeviceFeatures& features() const { return m_features; }

    VkPhysicalDeviceProperties physicalDeviceProperties() const { return m_deviceProps; }

    void waitIdle() const;

private:
    bool createInstance();
    bool setupDebugMessenger();
    bool createSurface(GLFWwindow* window);
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createAllocator();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions) const;
    int rateDevice(VkPhysicalDevice device) const;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkQueue m_computeQueue = VK_NULL_HANDLE;

    QueueFamilyIndices m_queueFamilies;
    DeviceFeatures m_features;
    VkPhysicalDeviceProperties m_deviceProps{};

#ifdef NDEBUG
    static constexpr bool s_enableValidation = false;
#else
    static constexpr bool s_enableValidation = true;
#endif
};

} // namespace lmao
