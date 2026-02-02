#include "vulkan/CommandPool.h"
#include "vulkan/VulkanUtils.h"
#include "core/Log.h"

namespace lmao {

CommandPool::~CommandPool() { shutdown(); }

bool CommandPool::init(VkDevice device, uint32_t queueFamily, VkCommandPoolCreateFlags flags) {
    m_device = device;

    VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    ci.flags = flags;
    ci.queueFamilyIndex = queueFamily;

    VK_CHECK(vkCreateCommandPool(m_device, &ci, nullptr, &m_pool));
    return true;
}

void CommandPool::shutdown() {
    if (m_pool && m_device) {
        vkDestroyCommandPool(m_device, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

VkCommandBuffer CommandPool::allocate() {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = m_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &ai, &cmd));
    return cmd;
}

std::vector<VkCommandBuffer> CommandPool::allocate(uint32_t count) {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = m_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = count;

    std::vector<VkCommandBuffer> cmds(count);
    VK_CHECK(vkAllocateCommandBuffers(m_device, &ai, cmds.data()));
    return cmds;
}

void CommandPool::reset() {
    VK_CHECK(vkResetCommandPool(m_device, m_pool, 0));
}

void CommandPool::submitImmediate(VkQueue queue, const std::function<void(VkCommandBuffer)>& fn) {
    VkCommandBuffer cmd = allocate();

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    fn(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkFence fence;
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK_CHECK(vkCreateFence(m_device, &fci, nullptr, &fence));

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
    VK_CHECK(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_pool, 1, &cmd);
}

} // namespace lmao
