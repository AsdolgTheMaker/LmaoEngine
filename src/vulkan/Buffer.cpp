#include "vulkan/Buffer.h"
#include "vulkan/VulkanUtils.h"
#include "core/Log.h"

namespace lmao {

Buffer::~Buffer() { release(); }

Buffer::Buffer(Buffer&& o) noexcept
    : m_allocator(o.m_allocator), m_buffer(o.m_buffer), m_allocation(o.m_allocation),
      m_size(o.m_size), m_mapped(o.m_mapped) {
    o.m_buffer = VK_NULL_HANDLE;
    o.m_allocation = VK_NULL_HANDLE;
    o.m_mapped = nullptr;
}

Buffer& Buffer::operator=(Buffer&& o) noexcept {
    if (this != &o) {
        release();
        m_allocator = o.m_allocator;
        m_buffer = o.m_buffer;
        m_allocation = o.m_allocation;
        m_size = o.m_size;
        m_mapped = o.m_mapped;
        o.m_buffer = VK_NULL_HANDLE;
        o.m_allocation = VK_NULL_HANDLE;
        o.m_mapped = nullptr;
    }
    return *this;
}

bool Buffer::init(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
                  VmaMemoryUsage memUsage, VmaAllocationCreateFlags allocFlags) {
    m_allocator = allocator;
    m_size = size;

    VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memUsage;
    allocInfo.flags = allocFlags;

    VkResult r = vmaCreateBuffer(m_allocator, &bufInfo, &allocInfo, &m_buffer, &m_allocation, nullptr);
    if (r != VK_SUCCESS) {
        LOG(Memory, Error, "Failed to create buffer (size=%llu): %d", (unsigned long long)size, (int)r);
        return false;
    }

    // If host-visible, map persistently
    if (allocFlags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT ||
        allocFlags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT ||
        allocFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        vmaMapMemory(m_allocator, m_allocation, &m_mapped);
    }

    LOG(Memory, Trace, "Buffer created: %llu bytes, usage=0x%x", (unsigned long long)size, usage);
    return true;
}

void Buffer::shutdown() { release(); }

void Buffer::upload(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (!m_mapped) {
        vmaMapMemory(m_allocator, m_allocation, &m_mapped);
        std::memcpy(static_cast<char*>(m_mapped) + offset, data, size);
        vmaUnmapMemory(m_allocator, m_allocation);
        m_mapped = nullptr;
    } else {
        std::memcpy(static_cast<char*>(m_mapped) + offset, data, size);
        vmaFlushAllocation(m_allocator, m_allocation, offset, size);
    }
}

void Buffer::release() {
    if (m_buffer && m_allocator) {
        if (m_mapped) {
            vmaUnmapMemory(m_allocator, m_allocation);
            m_mapped = nullptr;
        }
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

} // namespace lmao
