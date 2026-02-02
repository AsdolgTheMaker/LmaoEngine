#pragma once
#include "assets/Mesh.h"
#include <memory>

namespace lmao {

class CommandPool;

class MeshGenerator {
public:
    static std::shared_ptr<Mesh> createCube(VmaAllocator alloc, VkQueue queue, CommandPool& pool,
                                             float size = 1.0f);
    static std::shared_ptr<Mesh> createSphere(VmaAllocator alloc, VkQueue queue, CommandPool& pool,
                                               float radius = 1.0f,
                                               uint32_t segments = 32, uint32_t rings = 16);
    static std::shared_ptr<Mesh> createPlane(VmaAllocator alloc, VkQueue queue, CommandPool& pool,
                                              float width = 10.0f, float depth = 10.0f,
                                              uint32_t subdivX = 1, uint32_t subdivZ = 1);
    static std::shared_ptr<Mesh> createCylinder(VmaAllocator alloc, VkQueue queue, CommandPool& pool,
                                                 float radius = 1.0f, float height = 2.0f,
                                                 uint32_t segments = 32);
    static std::shared_ptr<Mesh> createCone(VmaAllocator alloc, VkQueue queue, CommandPool& pool,
                                             float radius = 1.0f, float height = 2.0f,
                                             uint32_t segments = 32);
    static std::shared_ptr<Mesh> createTorus(VmaAllocator alloc, VkQueue queue, CommandPool& pool,
                                              float majorRadius = 1.0f, float minorRadius = 0.3f,
                                              uint32_t majorSeg = 48, uint32_t minorSeg = 24);

private:
    static void computeTangents(std::vector<Vertex>& vertices,
                                 const std::vector<uint32_t>& indices);
};

} // namespace lmao
