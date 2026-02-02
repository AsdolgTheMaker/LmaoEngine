#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>
#include <cstdint>
#include <cstring>

namespace lmao {

class VulkanContext;

class Buffer {
public:
    Buffer() = default;
    ~Buffer();

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    bool init(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
              VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO,
              VmaAllocationCreateFlags allocFlags = 0);
    void shutdown();

    void upload(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    VkBuffer handle() const { return m_buffer; }
    VkDeviceSize size() const { return m_size; }
    VmaAllocation allocation() const { return m_allocation; }

    // For persistently mapped buffers
    void* mapped() const { return m_mapped; }

private:
    void release();

    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    void* m_mapped = nullptr;
};

} // namespace lmao
