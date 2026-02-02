#include "assets/MeshGenerator.h"
#include "vulkan/CommandPool.h"
#include "core/Log.h"
#include <cmath>

namespace lmao {

void MeshGenerator::computeTangents(std::vector<Vertex>& vertices,
                                     const std::vector<uint32_t>& indices) {
    // Accumulate tangents per triangle
    std::vector<vec3> tan(vertices.size(), vec3(0));

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i], i1 = indices[i+1], i2 = indices[i+2];
        const vec3& p0 = vertices[i0].position;
        const vec3& p1 = vertices[i1].position;
        const vec3& p2 = vertices[i2].position;
        const vec2& uv0 = vertices[i0].uv;
        const vec2& uv1 = vertices[i1].uv;
        const vec2& uv2 = vertices[i2].uv;

        vec3 e1 = p1 - p0, e2 = p2 - p0;
        vec2 duv1 = uv1 - uv0, duv2 = uv2 - uv0;

        float denom = duv1.x * duv2.y - duv2.x * duv1.y;
        if (std::abs(denom) < 1e-8f) continue;
        float r = 1.0f / denom;

        vec3 t = (e1 * duv2.y - e2 * duv1.y) * r;
        tan[i0] += t;
        tan[i1] += t;
        tan[i2] += t;
    }

    // Orthogonalize and store
    for (size_t i = 0; i < vertices.size(); i++) {
        const vec3& n = vertices[i].normal;
        vec3 t = tan[i];
        // Gram-Schmidt: project out normal component
        t = t - n * glm::dot(n, t);
        float len = glm::length(t);
        if (len > 1e-6f) {
            t /= len;
        } else {
            // Fallback: pick an arbitrary tangent perpendicular to normal
            if (std::abs(n.x) < 0.9f)
                t = glm::normalize(glm::cross(n, vec3(1, 0, 0)));
            else
                t = glm::normalize(glm::cross(n, vec3(0, 1, 0)));
        }
        vertices[i].tangent = vec4(t, 1.0f);
    }
}

std::shared_ptr<Mesh> MeshGenerator::createCube(VmaAllocator alloc, VkQueue queue,
                                                  CommandPool& pool, float size) {
    float h = size * 0.5f;
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;

    // 6 faces, 4 vertices each
    struct Face { vec3 normal; vec3 up; vec3 right; };
    Face faces[] = {
        {{ 0, 0, 1}, { 0, 1, 0}, { 1, 0, 0}},  // front  +Z
        {{ 0, 0,-1}, { 0, 1, 0}, {-1, 0, 0}},  // back   -Z
        {{ 0, 1, 0}, { 0, 0,-1}, { 1, 0, 0}},  // top    +Y
        {{ 0,-1, 0}, { 0, 0, 1}, { 1, 0, 0}},  // bottom -Y
        {{ 1, 0, 0}, { 0, 1, 0}, { 0, 0,-1}},  // right  +X
        {{-1, 0, 0}, { 0, 1, 0}, { 0, 0, 1}},  // left   -X
    };

    for (const auto& f : faces) {
        uint32_t base = static_cast<uint32_t>(verts.size());
        vec3 center = f.normal * h;

        for (int j = 0; j < 4; j++) {
            float u = (j == 0 || j == 3) ? 0.0f : 1.0f;
            float v = (j < 2) ? 0.0f : 1.0f;
            vec3 pos = center + f.right * h * (u * 2.0f - 1.0f) + f.up * h * (v * 2.0f - 1.0f);

            Vertex vert{};
            vert.position = pos;
            vert.normal = f.normal;
            vert.uv = {u, v};
            verts.push_back(vert);
        }

        idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base + 2); idx.push_back(base + 3); idx.push_back(base + 0);
    }

    computeTangents(verts, idx);

    auto mesh = std::make_shared<Mesh>();
    mesh->init(alloc, queue, pool, verts, idx);
    LOG(Assets, Debug, "Generated cube: size=%.2f, %zu verts, %zu indices", size, verts.size(), idx.size());
    return mesh;
}

std::shared_ptr<Mesh> MeshGenerator::createSphere(VmaAllocator alloc, VkQueue queue,
                                                    CommandPool& pool,
                                                    float radius, uint32_t segments, uint32_t rings) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;

    for (uint32_t y = 0; y <= rings; y++) {
        float theta = static_cast<float>(y) / rings * PI;
        float sinT = std::sin(theta), cosT = std::cos(theta);

        for (uint32_t x = 0; x <= segments; x++) {
            float phi = static_cast<float>(x) / segments * TWO_PI;
            float sinP = std::sin(phi), cosP = std::cos(phi);

            vec3 n(cosP * sinT, cosT, sinP * sinT);
            Vertex v{};
            v.position = n * radius;
            v.normal = n;
            v.uv = {static_cast<float>(x) / segments, static_cast<float>(y) / rings};
            verts.push_back(v);
        }
    }

    for (uint32_t y = 0; y < rings; y++) {
        for (uint32_t x = 0; x < segments; x++) {
            uint32_t a = y * (segments + 1) + x;
            uint32_t b = a + segments + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(a + 1); idx.push_back(b); idx.push_back(b + 1);
        }
    }

    computeTangents(verts, idx);

    auto mesh = std::make_shared<Mesh>();
    mesh->init(alloc, queue, pool, verts, idx);
    LOG(Assets, Debug, "Generated sphere: r=%.2f, %ux%u, %zu verts", radius, segments, rings, verts.size());
    return mesh;
}

std::shared_ptr<Mesh> MeshGenerator::createPlane(VmaAllocator alloc, VkQueue queue,
                                                   CommandPool& pool,
                                                   float width, float depth,
                                                   uint32_t subdivX, uint32_t subdivZ) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;

    for (uint32_t z = 0; z <= subdivZ; z++) {
        for (uint32_t x = 0; x <= subdivX; x++) {
            float u = static_cast<float>(x) / subdivX;
            float v = static_cast<float>(z) / subdivZ;

            Vertex vert{};
            vert.position = {(u - 0.5f) * width, 0.0f, (v - 0.5f) * depth};
            vert.normal = {0, 1, 0};
            vert.uv = {u, v};
            verts.push_back(vert);
        }
    }

    for (uint32_t z = 0; z < subdivZ; z++) {
        for (uint32_t x = 0; x < subdivX; x++) {
            uint32_t a = z * (subdivX + 1) + x;
            uint32_t b = a + subdivX + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(a + 1); idx.push_back(b); idx.push_back(b + 1);
        }
    }

    computeTangents(verts, idx);

    auto mesh = std::make_shared<Mesh>();
    mesh->init(alloc, queue, pool, verts, idx);
    return mesh;
}

std::shared_ptr<Mesh> MeshGenerator::createCylinder(VmaAllocator alloc, VkQueue queue,
                                                      CommandPool& pool,
                                                      float radius, float height, uint32_t segments) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    float halfH = height * 0.5f;

    // Side vertices
    for (uint32_t i = 0; i <= segments; i++) {
        float angle = static_cast<float>(i) / segments * TWO_PI;
        float c = std::cos(angle), s = std::sin(angle);
        float u = static_cast<float>(i) / segments;

        Vertex top{}, bot{};
        top.position = {c * radius, halfH, s * radius};
        top.normal = {c, 0, s};
        top.uv = {u, 0};

        bot.position = {c * radius, -halfH, s * radius};
        bot.normal = {c, 0, s};
        bot.uv = {u, 1};

        verts.push_back(top);
        verts.push_back(bot);
    }

    for (uint32_t i = 0; i < segments; i++) {
        uint32_t a = i * 2, b = a + 1, c = a + 2, d = a + 3;
        idx.push_back(a); idx.push_back(b); idx.push_back(c);
        idx.push_back(c); idx.push_back(b); idx.push_back(d);
    }

    // Top cap
    uint32_t topCenter = static_cast<uint32_t>(verts.size());
    Vertex tc{}; tc.position = {0, halfH, 0}; tc.normal = {0, 1, 0}; tc.uv = {0.5f, 0.5f};
    verts.push_back(tc);
    for (uint32_t i = 0; i <= segments; i++) {
        float angle = static_cast<float>(i) / segments * TWO_PI;
        float c = std::cos(angle), s = std::sin(angle);
        Vertex v{}; v.position = {c * radius, halfH, s * radius};
        v.normal = {0, 1, 0}; v.uv = {c * 0.5f + 0.5f, s * 0.5f + 0.5f};
        verts.push_back(v);
    }
    for (uint32_t i = 0; i < segments; i++) {
        idx.push_back(topCenter);
        idx.push_back(topCenter + 1 + i);
        idx.push_back(topCenter + 2 + i);
    }

    // Bottom cap
    uint32_t botCenter = static_cast<uint32_t>(verts.size());
    Vertex bc{}; bc.position = {0, -halfH, 0}; bc.normal = {0, -1, 0}; bc.uv = {0.5f, 0.5f};
    verts.push_back(bc);
    for (uint32_t i = 0; i <= segments; i++) {
        float angle = static_cast<float>(i) / segments * TWO_PI;
        float c = std::cos(angle), s = std::sin(angle);
        Vertex v{}; v.position = {c * radius, -halfH, s * radius};
        v.normal = {0, -1, 0}; v.uv = {c * 0.5f + 0.5f, s * 0.5f + 0.5f};
        verts.push_back(v);
    }
    for (uint32_t i = 0; i < segments; i++) {
        idx.push_back(botCenter);
        idx.push_back(botCenter + 2 + i);
        idx.push_back(botCenter + 1 + i);
    }

    computeTangents(verts, idx);

    auto mesh = std::make_shared<Mesh>();
    mesh->init(alloc, queue, pool, verts, idx);
    return mesh;
}

std::shared_ptr<Mesh> MeshGenerator::createCone(VmaAllocator alloc, VkQueue queue,
                                                  CommandPool& pool,
                                                  float radius, float height, uint32_t segments) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    float halfH = height * 0.5f;
    float slope = radius / height;

    // Side vertices
    for (uint32_t i = 0; i <= segments; i++) {
        float angle = static_cast<float>(i) / segments * TWO_PI;
        float c = std::cos(angle), s = std::sin(angle);
        float u = static_cast<float>(i) / segments;

        // Normal for cone side: tilted outward
        vec3 n = glm::normalize(vec3(c, slope, s));

        Vertex tip{}, base{};
        tip.position = {0, halfH, 0};
        tip.normal = n;
        tip.uv = {u, 0};

        base.position = {c * radius, -halfH, s * radius};
        base.normal = n;
        base.uv = {u, 1};

        verts.push_back(tip);
        verts.push_back(base);
    }

    for (uint32_t i = 0; i < segments; i++) {
        uint32_t a = i * 2, b = a + 1, c = a + 2, d = a + 3;
        idx.push_back(a); idx.push_back(b); idx.push_back(d);
        idx.push_back(a); idx.push_back(d); idx.push_back(c);
    }

    // Base cap
    uint32_t center = static_cast<uint32_t>(verts.size());
    Vertex cv{}; cv.position = {0, -halfH, 0}; cv.normal = {0, -1, 0}; cv.uv = {0.5f, 0.5f};
    verts.push_back(cv);
    for (uint32_t i = 0; i <= segments; i++) {
        float angle = static_cast<float>(i) / segments * TWO_PI;
        float cc = std::cos(angle), ss = std::sin(angle);
        Vertex v{}; v.position = {cc * radius, -halfH, ss * radius};
        v.normal = {0, -1, 0}; v.uv = {cc * 0.5f + 0.5f, ss * 0.5f + 0.5f};
        verts.push_back(v);
    }
    for (uint32_t i = 0; i < segments; i++) {
        idx.push_back(center);
        idx.push_back(center + 2 + i);
        idx.push_back(center + 1 + i);
    }

    computeTangents(verts, idx);

    auto mesh = std::make_shared<Mesh>();
    mesh->init(alloc, queue, pool, verts, idx);
    return mesh;
}

std::shared_ptr<Mesh> MeshGenerator::createTorus(VmaAllocator alloc, VkQueue queue,
                                                   CommandPool& pool,
                                                   float majorR, float minorR,
                                                   uint32_t majorSeg, uint32_t minorSeg) {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;

    for (uint32_t i = 0; i <= majorSeg; i++) {
        float u = static_cast<float>(i) / majorSeg * TWO_PI;
        float cu = std::cos(u), su = std::sin(u);

        for (uint32_t j = 0; j <= minorSeg; j++) {
            float v = static_cast<float>(j) / minorSeg * TWO_PI;
            float cv = std::cos(v), sv = std::sin(v);

            vec3 pos{(majorR + minorR * cv) * cu, minorR * sv, (majorR + minorR * cv) * su};
            vec3 n{cv * cu, sv, cv * su};

            Vertex vert{};
            vert.position = pos;
            vert.normal = n;
            vert.uv = {static_cast<float>(i) / majorSeg, static_cast<float>(j) / minorSeg};
            verts.push_back(vert);
        }
    }

    for (uint32_t i = 0; i < majorSeg; i++) {
        for (uint32_t j = 0; j < minorSeg; j++) {
            uint32_t a = i * (minorSeg + 1) + j;
            uint32_t b = a + minorSeg + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(a + 1); idx.push_back(b); idx.push_back(b + 1);
        }
    }

    computeTangents(verts, idx);

    auto mesh = std::make_shared<Mesh>();
    mesh->init(alloc, queue, pool, verts, idx);
    LOG(Assets, Debug, "Generated torus: R=%.2f r=%.2f, %zu verts", majorR, minorR, verts.size());
    return mesh;
}

} // namespace lmao
