#pragma once

#include <array>
#include <volk.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>

namespace lmao {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using quat = glm::quat;
using ivec2 = glm::ivec2;
using uvec2 = glm::uvec2;

// Standard vertex format used throughout the engine.
// Interleaved for GPU cache friendliness: 48 bytes per vertex, aligned to 16-byte boundaries.
// Position and tangent pack direction + handedness into the w component.
struct Vertex {
    vec3 position;
    float pad0;        // align normal to 16 bytes
    vec3 normal;
    float pad1;
    vec2 uv;
    vec2 pad2;
    vec4 tangent;      // xyz = tangent direction, w = bitangent sign (+1 or -1)

    static VkVertexInputBindingDescription bindingDesc() {
        return {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 4> attributeDescs() {
        return {{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
            {2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, uv)},
            {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)},
        }};
    }
};

struct AABB {
    vec3 min{std::numeric_limits<float>::max()};
    vec3 max{std::numeric_limits<float>::lowest()};

    void expand(const vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    vec3 center() const { return (min + max) * 0.5f; }
    vec3 extents() const { return (max - min) * 0.5f; }
};

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = PI * 2.0f;
constexpr float HALF_PI = PI * 0.5f;

} // namespace lmao
