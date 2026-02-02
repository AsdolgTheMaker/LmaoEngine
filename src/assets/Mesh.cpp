#include "assets/Mesh.h"
#include "vulkan/CommandPool.h"
#include "core/Log.h"

namespace lmao {

bool Mesh::init(VmaAllocator allocator, VkQueue graphicsQueue, CommandPool& cmdPool,
                const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    m_indexCount = static_cast<uint32_t>(indices.size());

    // Compute AABB
    m_bounds = AABB{};
    for (const auto& v : vertices)
        m_bounds.expand(v.position);

    VkDeviceSize vbSize = vertices.size() * sizeof(Vertex);
    VkDeviceSize ibSize = indices.size() * sizeof(uint32_t);

    // Vertex buffer
    m_vertexBuffer.init(allocator, vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    Buffer staging;
    staging.init(allocator, vbSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    staging.upload(vertices.data(), vbSize);

    cmdPool.submitImmediate(graphicsQueue, [&](VkCommandBuffer cmd) {
        VkBufferCopy copy{};
        copy.size = vbSize;
        vkCmdCopyBuffer(cmd, staging.handle(), m_vertexBuffer.handle(), 1, &copy);
    });
    staging.shutdown();

    // Index buffer
    m_indexBuffer.init(allocator, ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    staging.init(allocator, ibSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    staging.upload(indices.data(), ibSize);

    cmdPool.submitImmediate(graphicsQueue, [&](VkCommandBuffer cmd) {
        VkBufferCopy copy{};
        copy.size = ibSize;
        vkCmdCopyBuffer(cmd, staging.handle(), m_indexBuffer.handle(), 1, &copy);
    });
    staging.shutdown();

    LOG(Assets, Debug, "Mesh created: %zu verts, %u indices, AABB(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)",
        vertices.size(), m_indexCount,
        m_bounds.min.x, m_bounds.min.y, m_bounds.min.z,
        m_bounds.max.x, m_bounds.max.y, m_bounds.max.z);
    return true;
}

void Mesh::shutdown() {
    m_vertexBuffer.shutdown();
    m_indexBuffer.shutdown();
    m_indexCount = 0;
}

} // namespace lmao
