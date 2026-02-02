#pragma once
#include <volk.h>
#include <vector>
#include <cstdint>

namespace lmao {

class VulkanContext;

class Swapchain {
public:
    Swapchain() = default;
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    bool init(VulkanContext& ctx, uint32_t width, uint32_t height);
    void shutdown(VkDevice device);
    bool recreate(VulkanContext& ctx, uint32_t width, uint32_t height);

    VkSwapchainKHR handle() const { return m_swapchain; }
    VkFormat imageFormat() const { return m_format; }
    VkExtent2D extent() const { return m_extent; }
    uint32_t imageCount() const { return static_cast<uint32_t>(m_images.size()); }
    VkImageView imageView(uint32_t i) const { return m_imageViews[i]; }
    VkImage image(uint32_t i) const { return m_images[i]; }

    // Returns UINT32_MAX on failure (e.g., need recreate)
    uint32_t acquireNextImage(VkDevice device, VkSemaphore signalSemaphore);
    VkResult present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore);

private:
    void cleanup(VkDevice device);

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
};

} // namespace lmao
