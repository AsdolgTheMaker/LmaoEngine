#pragma once
#include <volk.h>
#include <vector>
#include <cstdint>

namespace lmao {

// Per-frame synchronization primitives.
// Sized to swapchain image count to avoid semaphore reuse hazards.
class FrameSync {
public:
    FrameSync() = default;
    ~FrameSync();

    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;

    bool init(VkDevice device, uint32_t frameCount);
    void shutdown();

    void advance() { m_currentFrame = (m_currentFrame + 1) % m_frameCount; }

    uint32_t currentFrame() const { return m_currentFrame; }
    uint32_t frameCount() const { return m_frameCount; }

    VkSemaphore imageAvailableSemaphore() const { return m_imageAvailable[m_currentFrame]; }
    VkSemaphore renderFinishedSemaphore() const { return m_renderFinished[m_currentFrame]; }
    VkFence inFlightFence() const { return m_inFlight[m_currentFrame]; }

    void waitForFence(VkDevice device) const;
    void resetFence(VkDevice device) const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_currentFrame = 0;
    uint32_t m_frameCount = 0;
    std::vector<VkSemaphore> m_imageAvailable;
    std::vector<VkSemaphore> m_renderFinished;
    std::vector<VkFence> m_inFlight;
};

} // namespace lmao
