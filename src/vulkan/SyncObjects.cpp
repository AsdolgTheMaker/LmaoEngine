#include "vulkan/SyncObjects.h"
#include "vulkan/VulkanUtils.h"
#include "core/Log.h"

namespace lmao {

FrameSync::~FrameSync() { shutdown(); }

bool FrameSync::init(VkDevice device, uint32_t frameCount) {
    m_device = device;
    m_frameCount = frameCount;
    m_imageAvailable.resize(frameCount, VK_NULL_HANDLE);
    m_renderFinished.resize(frameCount, VK_NULL_HANDLE);
    m_inFlight.resize(frameCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < frameCount; i++) {
        VK_CHECK(vkCreateSemaphore(m_device, &sci, nullptr, &m_imageAvailable[i]));
        VK_CHECK(vkCreateSemaphore(m_device, &sci, nullptr, &m_renderFinished[i]));
        VK_CHECK(vkCreateFence(m_device, &fci, nullptr, &m_inFlight[i]));
    }
    LOG(Vulkan, Debug, "Frame sync created: %u frames", frameCount);
    return true;
}

void FrameSync::shutdown() {
    if (!m_device) return;
    for (uint32_t i = 0; i < m_frameCount; i++) {
        if (m_imageAvailable[i]) vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
        if (m_renderFinished[i]) vkDestroySemaphore(m_device, m_renderFinished[i], nullptr);
        if (m_inFlight[i]) vkDestroyFence(m_device, m_inFlight[i], nullptr);
    }
    m_imageAvailable.clear();
    m_renderFinished.clear();
    m_inFlight.clear();
    m_frameCount = 0;
    m_currentFrame = 0;
}

void FrameSync::waitForFence(VkDevice device) const {
    VK_CHECK(vkWaitForFences(device, 1, &m_inFlight[m_currentFrame], VK_TRUE, UINT64_MAX));
}

void FrameSync::resetFence(VkDevice device) const {
    VK_CHECK(vkResetFences(device, 1, &m_inFlight[m_currentFrame]));
}

} // namespace lmao
