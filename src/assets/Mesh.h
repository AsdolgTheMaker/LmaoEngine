#pragma once
#include "vulkan/Buffer.h"
#include "math/MathUtils.h"
#include <vector>

namespace lmao {

class CommandPool;

class Mesh {
public:
    Mesh() = default;
    ~Mesh() = default;

    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    bool init(VmaAllocator allocator, VkQueue graphicsQueue, CommandPool& cmdPool,
              const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void shutdown();

    VkBuffer vertexBuffer() const { return m_vertexBuffer.handle(); }
    VkBuffer indexBuffer() const { return m_indexBuffer.handle(); }
    uint32_t indexCount() const { return m_indexCount; }
    const AABB& bounds() const { return m_bounds; }

private:
    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;
    uint32_t m_indexCount = 0;
    AABB m_bounds;
};

} // namespace lmao
