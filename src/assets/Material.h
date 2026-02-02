#pragma once
#include "math/MathUtils.h"
#include "vulkan/Buffer.h"
#include <memory>
#include <volk.h>

namespace lmao {

class Texture;
class VulkanContext;
class DescriptorManager;

struct MaterialParams {
    vec4 albedoColor{1, 1, 1, 1};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float normalScale = 1.0f;
    float _pad{};
};

class Material {
public:
    Material() = default;
    ~Material() = default;

    bool init(VulkanContext& ctx, DescriptorManager& descMgr,
              VkDescriptorSetLayout layout,
              std::shared_ptr<Texture> albedoTex,
              std::shared_ptr<Texture> normalTex,
              std::shared_ptr<Texture> metalRoughTex,
              const MaterialParams& params = {});
    void shutdown();

    VkDescriptorSet descriptorSet() const { return m_descriptorSet; }
    const MaterialParams& params() const { return m_params; }

private:
    std::shared_ptr<Texture> m_albedoTex;
    std::shared_ptr<Texture> m_normalTex;
    std::shared_ptr<Texture> m_metalRoughTex;
    MaterialParams m_params;
    Buffer m_paramsBuffer;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
};

} // namespace lmao
