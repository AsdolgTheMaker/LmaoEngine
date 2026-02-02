#include "vulkan/Swapchain.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/VulkanUtils.h"
#include "core/Log.h"
#include <algorithm>

namespace lmao {

Swapchain::~Swapchain() {
    // Must be cleaned up explicitly via shutdown()
}

bool Swapchain::init(VulkanContext& ctx, uint32_t width, uint32_t height) {
    VkDevice device = ctx.device();
    VkPhysicalDevice phys = ctx.physicalDevice();
    VkSurfaceKHR surface = ctx.surface();

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps);

    // Choose format: prefer SRGB
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR chosen = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            break;
        }
    }
    m_format = chosen.format;

    // Choose present mode: prefer mailbox (triple buffering), fall back to FIFO
    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = m; break; }
    }

    // Extent
    if (caps.currentExtent.width != UINT32_MAX) {
        m_extent = caps.currentExtent;
    } else {
        m_extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
        m_extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = m_format;
    ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent = m_extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    auto& families = ctx.queueFamilies();
    uint32_t queueFamilyIndices[] = {families.graphics, families.present};
    if (families.graphics != families.present) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device, &ci, nullptr, &m_swapchain));

    // Get images
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, m_images.data());

    // Create image views
    m_imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = m_images[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = m_format;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &vci, nullptr, &m_imageViews[i]));
    }

    LMAO_INFO("Swapchain created: %ux%u, %u images", m_extent.width, m_extent.height, imageCount);
    return true;
}

void Swapchain::shutdown(VkDevice device) {
    cleanup(device);
}

bool Swapchain::recreate(VulkanContext& ctx, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return false;
    ctx.waitIdle();
    cleanup(ctx.device());
    return init(ctx, width, height);
}

void Swapchain::cleanup(VkDevice device) {
    for (auto view : m_imageViews)
        vkDestroyImageView(device, view, nullptr);
    m_imageViews.clear();
    m_images.clear();
    if (m_swapchain) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

uint32_t Swapchain::acquireNextImage(VkDevice device, VkSemaphore signalSemaphore) {
    uint32_t index;
    VkResult r = vkAcquireNextImageKHR(device, m_swapchain, UINT64_MAX, signalSemaphore, VK_NULL_HANDLE, &index);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        return UINT32_MAX;
    VK_CHECK(r);
    return index;
}

VkResult Swapchain::present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &waitSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &m_swapchain;
    info.pImageIndices = &imageIndex;
    return vkQueuePresentKHR(queue, &info);
}

} // namespace lmao
