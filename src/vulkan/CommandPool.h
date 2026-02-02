#pragma once
#include <volk.h>
#include <vector>
#include <cstdint>
#include <functional>

namespace lmao {

class VulkanContext;

class CommandPool {
public:
    CommandPool() = default;
    ~CommandPool();

    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    bool init(VkDevice device, uint32_t queueFamily, VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    void shutdown();

    VkCommandBuffer allocate();
    std::vector<VkCommandBuffer> allocate(uint32_t count);
    void reset();

    // Single-use command buffer helper: allocates, begins, calls fn, ends, submits, and waits
    void submitImmediate(VkQueue queue, const std::function<void(VkCommandBuffer)>& fn);

    VkCommandPool handle() const { return m_pool; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkCommandPool m_pool = VK_NULL_HANDLE;
};

} // namespace lmao
