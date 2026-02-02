#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>

namespace lmao {

class VulkanContext;

class Image {
public:
    Image() = default;
    ~Image();

    Image(Image&& o) noexcept;
    Image& operator=(Image&& o) noexcept;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    struct CreateInfo {
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    };

    bool init(VmaAllocator allocator, VkDevice device, const CreateInfo& info);
    void shutdown();

    VkImage handle() const { return m_image; }
    VkImageView view() const { return m_view; }
    VkFormat format() const { return m_format; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint32_t mipLevels() const { return m_mipLevels; }

    // Transition image layout using a command buffer
    static void transitionLayout(VkCommandBuffer cmd, VkImage image,
                                 VkImageLayout oldLayout, VkImageLayout newLayout,
                                 VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                 uint32_t mipLevels = 1, uint32_t arrayLayers = 1);

private:
    void release();

    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    uint32_t m_width = 0, m_height = 0, m_mipLevels = 1;
};

} // namespace lmao
